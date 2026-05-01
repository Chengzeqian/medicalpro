#include "ascend_backend_plugin.h"
#include "ascend_runtime_algo_hook.h"
#include "ascend_runtime_loader.h"

#include "ply_reader_cpu.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

using DynamicLibHandle = mesh_gpu::ascend_runtime::DynamicLibHandle;
using AscendRuntimeApi = mesh_gpu::ascend_runtime::RuntimeApi;
using AclError = mesh_gpu::ascend_runtime::AclError;
using AclrtContext = void*;
using AclrtStream = mesh_gpu::ascend_runtime::AclrtStream;

constexpr AclError ACL_SUCCESS = 0;
constexpr int ACL_MEMCPY_HOST_TO_DEVICE = 1;
constexpr int ACL_MEMCPY_DEVICE_TO_HOST = 2;
constexpr int ACL_MEM_MALLOC_HUGE_FIRST = 0;

struct CpuMeshHolder {
    MeshSoA mesh{};
    bool loaded = false;
};

void freeCpuMesh(CpuMeshHolder& holder) {
    MeshSoA& mesh = holder.mesh;
    if (!holder.loaded) {
        return;
    }

    delete[] mesh.vertices_x;
    delete[] mesh.vertices_y;
    delete[] mesh.vertices_z;
    delete[] mesh.normals_x;
    delete[] mesh.normals_y;
    delete[] mesh.normals_z;
    delete[] mesh.curvature;
    delete[] mesh.gaussian_curv;
    delete[] mesh.faces_v0;
    delete[] mesh.faces_v1;
    delete[] mesh.faces_v2;
    delete[] mesh.face_normals_x;
    delete[] mesh.face_normals_y;
    delete[] mesh.face_normals_z;
    delete[] mesh.face_areas;
    delete[] mesh.vertex_face_offset;
    delete[] mesh.vertex_face_indices;
    delete[] mesh.colors_r;
    delete[] mesh.colors_g;
    delete[] mesh.colors_b;
    delete[] mesh.colors_a;

    mesh = MeshSoA{};
    holder.loaded = false;
}

struct AscendStubContext {
    int device_id = 0;
    bool strict = false;

    AscendRuntimeApi runtime;
    bool runtime_ready = false;
    bool acl_initialized = false;
    AclrtContext acl_context = nullptr;
    AclrtStream acl_stream = nullptr;

    DynamicLibHandle algo_plugin_handle = nullptr;
    mesh_gpu::AscendAlgoRunRegistrationV1Fn algo_run_registration_v1 = nullptr;
    mesh_gpu::AscendAlgoRunRegistrationV2Fn algo_run_registration_v2 = nullptr;
    std::string algo_plugin_path;

    CpuMeshHolder mesh_holder;
    mesh_gpu::MeshStats stats{};
    mesh_gpu::Point3D target_centroid{};
    bool has_target_mesh = false;

    std::vector<mesh_gpu::Point3D> source_points;
    std::vector<float> rmse_history;

    std::vector<float> target_xyz_host;
    std::vector<float> source_xyz_host;
    void* target_xyz_device = nullptr;
    void* source_xyz_device = nullptr;
};

thread_local std::string g_last_message;

const char* setMessage(const std::string& message) {
    g_last_message = message;
    return g_last_message.c_str();
}

void destroyDeviceBuffer(AscendStubContext& ctx, void*& ptr) {
    if (ptr && ctx.runtime_ready && ctx.runtime.aclrtFree) {
        ctx.runtime.aclrtFree(ptr);
    }
    ptr = nullptr;
}

void cleanupAscendRuntime(AscendStubContext& ctx) {
    destroyDeviceBuffer(ctx, ctx.source_xyz_device);
    destroyDeviceBuffer(ctx, ctx.target_xyz_device);

    if (ctx.runtime_ready && ctx.runtime.aclrtSynchronizeStream && ctx.acl_stream) {
        ctx.runtime.aclrtSynchronizeStream(ctx.acl_stream);
    }
    if (ctx.runtime_ready && ctx.runtime.aclrtDestroyStream && ctx.acl_stream) {
        ctx.runtime.aclrtDestroyStream(ctx.acl_stream);
    }
    ctx.acl_stream = nullptr;

    if (ctx.runtime_ready && ctx.runtime.aclrtDestroyContext && ctx.acl_context) {
        ctx.runtime.aclrtDestroyContext(ctx.acl_context);
    }
    ctx.acl_context = nullptr;

    if (ctx.runtime_ready && ctx.runtime.aclrtResetDevice) {
        ctx.runtime.aclrtResetDevice(ctx.device_id);
    }

    if (ctx.acl_initialized && ctx.runtime.aclFinalize) {
        ctx.runtime.aclFinalize();
    }
    ctx.acl_initialized = false;
    ctx.runtime_ready = false;
    ctx.runtime.unload();
}

std::vector<std::string> getAlgoPluginCandidates() {
    std::vector<std::string> candidates;

    const char* env_path = std::getenv("MESHGPU_ASCEND_ALGO_PLUGIN");
    if (env_path && env_path[0] != '\0') {
        candidates.emplace_back(env_path);
    }

#if defined(_WIN32)
    candidates.emplace_back("mesgpu_ascend_algo.dll");
    candidates.emplace_back("MeshGPUAscendAlgo.dll");
#else
    candidates.emplace_back("libmesgpu_ascend_algo.so");
    candidates.emplace_back("libMeshGPUAscendAlgo.so");
#endif

    return candidates;
}

bool loadAlgoPlugin(AscendStubContext& ctx, std::string& detail) {
    if (ctx.algo_plugin_handle) {
        mesh_gpu::ascend_runtime::closeDynamicLibrary(ctx.algo_plugin_handle);
        ctx.algo_plugin_handle = nullptr;
    }
    ctx.algo_run_registration_v1 = nullptr;
    ctx.algo_run_registration_v2 = nullptr;
    ctx.algo_plugin_path.clear();

    for (const auto& candidate : getAlgoPluginCandidates()) {
        DynamicLibHandle handle = mesh_gpu::ascend_runtime::openDynamicLibrary(candidate);
        if (!handle) {
            continue;
        }

        auto fn_v2 = reinterpret_cast<mesh_gpu::AscendAlgoRunRegistrationV2Fn>(
            mesh_gpu::ascend_runtime::getDynamicSymbol(handle, "MeshGPU_AscendAlgo_RunRegistrationV2"));
        auto fn_v1 = reinterpret_cast<mesh_gpu::AscendAlgoRunRegistrationV1Fn>(
            mesh_gpu::ascend_runtime::getDynamicSymbol(handle, "MeshGPU_AscendAlgo_RunRegistrationV1"));
        if (!fn_v2 && !fn_v1) {
            mesh_gpu::ascend_runtime::closeDynamicLibrary(handle);
            continue;
        }

        ctx.algo_plugin_handle = handle;
        ctx.algo_run_registration_v1 = fn_v1;
        ctx.algo_run_registration_v2 = fn_v2;
        ctx.algo_plugin_path = candidate;
        if (fn_v2 && fn_v1) {
            detail = "Loaded algorithm plugin: " + candidate + " (V2+V1)";
        } else if (fn_v2) {
            detail = "Loaded algorithm plugin: " + candidate + " (V2)";
        } else {
            detail = "Loaded algorithm plugin: " + candidate + " (V1)";
        }
        return true;
    }

    detail = "No Ascend algorithm plugin found. Falling back to built-in CPU placeholder.";
    return false;
}

mesh_gpu::Point3D computeTargetCentroid(const MeshSoA& mesh) {
    mesh_gpu::Point3D centroid;
    if (mesh.num_vertices == 0) {
        return centroid;
    }

    double sx = 0.0;
    double sy = 0.0;
    double sz = 0.0;
    for (uint32_t i = 0; i < mesh.num_vertices; ++i) {
        sx += mesh.vertices_x[i];
        sy += mesh.vertices_y[i];
        sz += mesh.vertices_z[i];
    }
    const double inv = 1.0 / static_cast<double>(mesh.num_vertices);
    centroid.x = static_cast<float>(sx * inv);
    centroid.y = static_cast<float>(sy * inv);
    centroid.z = static_cast<float>(sz * inv);
    return centroid;
}

void fillMeshStats(const MeshSoA& mesh, mesh_gpu::MeshStats& stats) {
    stats.num_vertices = static_cast<int>(mesh.num_vertices);
    stats.num_triangles = static_cast<int>(mesh.num_faces);
    stats.cell_size = 2.0f;

    if (mesh.num_vertices == 0) {
        stats.bounding_box_min = mesh_gpu::Point3D{};
        stats.bounding_box_max = mesh_gpu::Point3D{};
        return;
    }

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float min_z = std::numeric_limits<float>::max();
    float max_x = -std::numeric_limits<float>::max();
    float max_y = -std::numeric_limits<float>::max();
    float max_z = -std::numeric_limits<float>::max();

    for (uint32_t i = 0; i < mesh.num_vertices; ++i) {
        min_x = std::min(min_x, mesh.vertices_x[i]);
        min_y = std::min(min_y, mesh.vertices_y[i]);
        min_z = std::min(min_z, mesh.vertices_z[i]);
        max_x = std::max(max_x, mesh.vertices_x[i]);
        max_y = std::max(max_y, mesh.vertices_y[i]);
        max_z = std::max(max_z, mesh.vertices_z[i]);
    }

    stats.bounding_box_min = mesh_gpu::Point3D(min_x, min_y, min_z);
    stats.bounding_box_max = mesh_gpu::Point3D(max_x, max_y, max_z);
}

std::vector<float> makeInterleavedXYZFromMesh(const MeshSoA& mesh) {
    std::vector<float> xyz;
    xyz.resize(static_cast<size_t>(mesh.num_vertices) * 3u);
    for (uint32_t i = 0; i < mesh.num_vertices; ++i) {
        xyz[3u * i + 0u] = mesh.vertices_x[i];
        xyz[3u * i + 1u] = mesh.vertices_y[i];
        xyz[3u * i + 2u] = mesh.vertices_z[i];
    }
    return xyz;
}

std::vector<float> makeInterleavedXYZFromPoints(const std::vector<mesh_gpu::Point3D>& points) {
    std::vector<float> xyz;
    xyz.resize(points.size() * 3u);
    for (size_t i = 0; i < points.size(); ++i) {
        xyz[3u * i + 0u] = points[i].x;
        xyz[3u * i + 1u] = points[i].y;
        xyz[3u * i + 2u] = points[i].z;
    }
    return xyz;
}

bool uploadBufferToDevice(AscendStubContext& ctx,
                          const std::vector<float>& host,
                          void*& device_ptr,
                          std::string& reason) {
    destroyDeviceBuffer(ctx, device_ptr);

    if (!ctx.runtime_ready) {
        reason = "Ascend runtime not ready.";
        return false;
    }
    if (host.empty()) {
        reason = "Host buffer is empty.";
        return false;
    }

    const std::size_t bytes = host.size() * sizeof(float);
    void* ptr = nullptr;
    const AclError malloc_err = ctx.runtime.aclrtMalloc(&ptr, bytes, ACL_MEM_MALLOC_HUGE_FIRST);
    if (malloc_err != ACL_SUCCESS) {
        std::stringstream ss;
        ss << "aclrtMalloc failed, error=" << malloc_err;
        reason = ss.str();
        return false;
    }

    const AclError memcpy_err = ctx.runtime.aclrtMemcpy(
        ptr, bytes, host.data(), bytes, ACL_MEMCPY_HOST_TO_DEVICE);
    if (memcpy_err != ACL_SUCCESS) {
        ctx.runtime.aclrtFree(ptr);
        std::stringstream ss;
        ss << "aclrtMemcpy HostToDevice failed, error=" << memcpy_err;
        reason = ss.str();
        return false;
    }

    ctx.runtime.aclrtSynchronizeStream(ctx.acl_stream);
    device_ptr = ptr;
    reason = "uploaded";
    return true;
}

mesh_gpu::Point3D computeSourceCentroid(const std::vector<mesh_gpu::Point3D>& points) {
    mesh_gpu::Point3D centroid;
    if (points.empty()) {
        return centroid;
    }

    double sx = 0.0;
    double sy = 0.0;
    double sz = 0.0;
    for (const auto& p : points) {
        sx += p.x;
        sy += p.y;
        sz += p.z;
    }
    const double inv = 1.0 / static_cast<double>(points.size());
    centroid.x = static_cast<float>(sx * inv);
    centroid.y = static_cast<float>(sy * inv);
    centroid.z = static_cast<float>(sz * inv);
    return centroid;
}

bool createContext(int device_id, bool strict, void** out_context, const char** out_message) {
    if (!out_context) {
        if (out_message) {
            *out_message = setMessage("out_context is null.");
        }
        return false;
    }

    auto* ctx = new AscendStubContext();
    ctx->device_id = device_id;
    ctx->strict = strict;

    std::string runtime_detail;
    if (!ctx->runtime.load(runtime_detail, "Ascend backend stub")) {
        if (strict) {
            if (out_message) {
                *out_message = setMessage("Strict ASCEND mode: " + runtime_detail);
            }
            delete ctx;
            return false;
        }

        ctx->runtime_ready = false;
        std::string algo_detail;
        loadAlgoPlugin(*ctx, algo_detail);
        *out_context = ctx;
        if (out_message) {
            *out_message = setMessage(
                "Ascend runtime unavailable, using CPU placeholder path. " + runtime_detail + ". " + algo_detail);
        }
        return true;
    }

    if (!ctx->runtime.hasLifecycleApi()) {
        const std::string lifecycle_msg =
            "Loaded Ascend runtime but lifecycle symbols are incomplete for backend stub.";
        if (strict) {
            if (out_message) {
                *out_message = setMessage("Strict ASCEND mode: " + lifecycle_msg);
            }
            ctx->runtime.unload();
            delete ctx;
            return false;
        }
        ctx->runtime.unload();
        std::string algo_detail;
        loadAlgoPlugin(*ctx, algo_detail);
        *out_context = ctx;
        if (out_message) {
            *out_message = setMessage(lifecycle_msg + " Fallback to CPU placeholder. " + algo_detail);
        }
        return true;
    }

    const AclError init_err = ctx->runtime.aclInit(nullptr);
    if (init_err != ACL_SUCCESS) {
        if (strict) {
            if (out_message) {
                std::stringstream ss;
                ss << "Strict ASCEND mode: aclInit failed, error=" << init_err;
                *out_message = setMessage(ss.str());
            }
            ctx->runtime.unload();
            delete ctx;
            return false;
        }
        ctx->runtime.unload();
        std::string algo_detail;
        loadAlgoPlugin(*ctx, algo_detail);
        *out_context = ctx;
        if (out_message) {
            *out_message = setMessage("aclInit failed, fallback to CPU placeholder. " + algo_detail);
        }
        return true;
    }
    ctx->acl_initialized = true;

    const AclError set_dev_err = ctx->runtime.aclrtSetDevice(device_id);
    if (set_dev_err != ACL_SUCCESS) {
        if (strict) {
            if (out_message) {
                std::stringstream ss;
                ss << "Strict ASCEND mode: aclrtSetDevice failed, error=" << set_dev_err;
                *out_message = setMessage(ss.str());
            }
            cleanupAscendRuntime(*ctx);
            delete ctx;
            return false;
        }
        cleanupAscendRuntime(*ctx);
        std::string algo_detail;
        loadAlgoPlugin(*ctx, algo_detail);
        *out_context = ctx;
        if (out_message) {
            *out_message = setMessage("aclrtSetDevice failed, fallback to CPU placeholder. " + algo_detail);
        }
        return true;
    }

    const AclError create_ctx_err = ctx->runtime.aclrtCreateContext(&ctx->acl_context, device_id);
    if (create_ctx_err != ACL_SUCCESS) {
        if (strict) {
            if (out_message) {
                std::stringstream ss;
                ss << "Strict ASCEND mode: aclrtCreateContext failed, error=" << create_ctx_err;
                *out_message = setMessage(ss.str());
            }
            cleanupAscendRuntime(*ctx);
            delete ctx;
            return false;
        }
        cleanupAscendRuntime(*ctx);
        std::string algo_detail;
        loadAlgoPlugin(*ctx, algo_detail);
        *out_context = ctx;
        if (out_message) {
            *out_message = setMessage("aclrtCreateContext failed, fallback to CPU placeholder. " + algo_detail);
        }
        return true;
    }

    const AclError create_stream_err = ctx->runtime.aclrtCreateStream(&ctx->acl_stream);
    if (create_stream_err != ACL_SUCCESS) {
        if (strict) {
            if (out_message) {
                std::stringstream ss;
                ss << "Strict ASCEND mode: aclrtCreateStream failed, error=" << create_stream_err;
                *out_message = setMessage(ss.str());
            }
            cleanupAscendRuntime(*ctx);
            delete ctx;
            return false;
        }
        cleanupAscendRuntime(*ctx);
        std::string algo_detail;
        loadAlgoPlugin(*ctx, algo_detail);
        *out_context = ctx;
        if (out_message) {
            *out_message = setMessage("aclrtCreateStream failed, fallback to CPU placeholder. " + algo_detail);
        }
        return true;
    }

    ctx->runtime_ready = true;
    std::string algo_detail;
    loadAlgoPlugin(*ctx, algo_detail);
    *out_context = ctx;

    if (out_message) {
        *out_message = setMessage(
            "Ascend runtime initialized on device " + std::to_string(device_id) +
            ". " + runtime_detail + ". " + algo_detail);
    }
    return true;
}

void destroyContext(void* context) {
    auto* ctx = reinterpret_cast<AscendStubContext*>(context);
    if (!ctx) {
        return;
    }

    if (ctx->algo_plugin_handle) {
        mesh_gpu::ascend_runtime::closeDynamicLibrary(ctx->algo_plugin_handle);
        ctx->algo_plugin_handle = nullptr;
    }

    cleanupAscendRuntime(*ctx);
    freeCpuMesh(ctx->mesh_holder);
    delete ctx;
}

bool loadTargetMesh(void* context, const char* ply_file, float cell_size, const char** out_message) {
    auto* ctx = reinterpret_cast<AscendStubContext*>(context);
    if (!ctx || !ply_file) {
        if (out_message) {
            *out_message = setMessage("Invalid context or ply_file.");
        }
        return false;
    }

    freeCpuMesh(ctx->mesh_holder);
    ctx->target_xyz_host.clear();
    destroyDeviceBuffer(*ctx, ctx->target_xyz_device);

    if (!PLYReader::readPLY(ply_file, ctx->mesh_holder.mesh)) {
        if (out_message) {
            *out_message = setMessage(std::string("Failed to read mesh: ") + ply_file);
        }
        return false;
    }

    ctx->mesh_holder.loaded = true;
    fillMeshStats(ctx->mesh_holder.mesh, ctx->stats);
    ctx->stats.cell_size = cell_size;
    ctx->target_centroid = computeTargetCentroid(ctx->mesh_holder.mesh);
    ctx->has_target_mesh = true;
    ctx->target_xyz_host = makeInterleavedXYZFromMesh(ctx->mesh_holder.mesh);

    std::string upload_detail;
    if (ctx->runtime_ready) {
        if (!uploadBufferToDevice(*ctx, ctx->target_xyz_host, ctx->target_xyz_device, upload_detail)) {
            if (ctx->strict) {
                if (out_message) {
                    *out_message = setMessage("Strict ASCEND mode: target upload failed. " + upload_detail);
                }
                return false;
            }
            upload_detail = "target upload skipped (" + upload_detail + ")";
        } else {
            upload_detail = "target uploaded to Ascend device";
        }
    } else {
        upload_detail = "runtime not ready; using CPU placeholder";
    }

    if (out_message) {
        std::stringstream ss;
        ss << "Loaded mesh: vertices=" << ctx->stats.num_vertices
           << ", faces=" << ctx->stats.num_triangles << ". " << upload_detail;
        *out_message = setMessage(ss.str());
    }
    return true;
}

bool hasTargetMesh(void* context) {
    auto* ctx = reinterpret_cast<AscendStubContext*>(context);
    return ctx && ctx->has_target_mesh;
}

bool getMeshStats(void* context, mesh_gpu::MeshStats* out_stats) {
    auto* ctx = reinterpret_cast<AscendStubContext*>(context);
    if (!ctx || !out_stats || !ctx->has_target_mesh) {
        return false;
    }
    *out_stats = ctx->stats;
    return true;
}

bool setSourcePointCloud(void* context,
                         const mesh_gpu::Point3D* points,
                         int count,
                         const char** out_message) {
    auto* ctx = reinterpret_cast<AscendStubContext*>(context);
    if (!ctx) {
        if (out_message) {
            *out_message = setMessage("Invalid context.");
        }
        return false;
    }
    if (count < 0) {
        if (out_message) {
            *out_message = setMessage("count must be >= 0.");
        }
        return false;
    }
    if (count > 0 && !points) {
        if (out_message) {
            *out_message = setMessage("points is null while count > 0.");
        }
        return false;
    }

    ctx->source_points.clear();
    ctx->source_xyz_host.clear();
    destroyDeviceBuffer(*ctx, ctx->source_xyz_device);

    if (count > 0) {
        ctx->source_points.assign(points, points + count);
        ctx->source_xyz_host = makeInterleavedXYZFromPoints(ctx->source_points);
    }

    std::string upload_detail;
    if (ctx->runtime_ready && !ctx->source_xyz_host.empty()) {
        if (!uploadBufferToDevice(*ctx, ctx->source_xyz_host, ctx->source_xyz_device, upload_detail)) {
            if (ctx->strict) {
                if (out_message) {
                    *out_message = setMessage("Strict ASCEND mode: source upload failed. " + upload_detail);
                }
                return false;
            }
            upload_detail = "source upload skipped (" + upload_detail + ")";
        } else {
            upload_detail = "source uploaded to Ascend device";
        }
    } else if (ctx->runtime_ready) {
        upload_detail = "source cleared";
    } else {
        upload_detail = "runtime not ready; using CPU placeholder";
    }

    if (out_message) {
        *out_message = setMessage("Accepted source cloud, count=" + std::to_string(count) + ". " + upload_detail);
    }
    return true;
}

bool clearSourcePointCloud(void* context, const char** out_message) {
    auto* ctx = reinterpret_cast<AscendStubContext*>(context);
    if (!ctx) {
        if (out_message) {
            *out_message = setMessage("Invalid context.");
        }
        return false;
    }

    ctx->source_points.clear();
    ctx->source_xyz_host.clear();
    destroyDeviceBuffer(*ctx, ctx->source_xyz_device);

    if (out_message) {
        *out_message = setMessage("Source cloud cleared.");
    }
    return true;
}

bool runRegistration(void* context,
                     const mesh_gpu::Transform4x4* initial_transform,
                     const mesh_gpu::RegistrationParams* params,
                     mesh_gpu::AscendRegistrationResultView* out_result,
                     const char** out_message) {
    auto* ctx = reinterpret_cast<AscendStubContext*>(context);
    if (!ctx || !out_result) {
        if (out_message) {
            *out_message = setMessage("Invalid context or output.");
        }
        return false;
    }
    if (!ctx->has_target_mesh) {
        if (out_message) {
            *out_message = setMessage("Target mesh is not loaded.");
        }
        return false;
    }
    if (ctx->source_points.empty()) {
        if (out_message) {
            *out_message = setMessage("Source cloud is empty.");
        }
        return false;
    }

    mesh_gpu::Transform4x4 transform;
    if (initial_transform) {
        transform = *initial_transform;
    }

    bool used_algo_plugin = false;
    float tx = 0.0f;
    float ty = 0.0f;
    float tz = 0.0f;
    float rmse = 0.0f;
    int iterations = 1;
    bool converged = true;
    std::string run_detail;

    const bool device_buffers_valid =
        ctx->runtime_ready && ctx->target_xyz_device && ctx->source_xyz_device;

    if (ctx->algo_run_registration_v2) {
        mesh_gpu::AscendAlgoRegistrationInputV2 input{};
        input.target_xyz_device = reinterpret_cast<const float*>(ctx->target_xyz_device);
        input.target_point_count = static_cast<int>(ctx->mesh_holder.mesh.num_vertices);
        input.source_xyz_device = reinterpret_cast<const float*>(ctx->source_xyz_device);
        input.source_point_count = static_cast<int>(ctx->source_points.size());
        input.target_xyz_host = ctx->target_xyz_host.empty() ? nullptr : ctx->target_xyz_host.data();
        input.source_xyz_host = ctx->source_xyz_host.empty() ? nullptr : ctx->source_xyz_host.data();
        input.device_buffers_valid = device_buffers_valid ? 1 : 0;
        input.device_id = ctx->device_id;
        input.ascend_stream = ctx->acl_stream;
        input.initial_transform = &transform;
        input.params = params;

        mesh_gpu::AscendAlgoRegistrationOutputV1 output{};
        const char* algo_message = nullptr;
        if (ctx->algo_run_registration_v2(&input, &output, &algo_message)) {
            tx = output.translation_x;
            ty = output.translation_y;
            tz = output.translation_z;
            rmse = output.rmse;
            iterations = output.iterations;
            converged = (output.converged != 0);
            used_algo_plugin = true;
            run_detail = std::string("Ascend algorithm plugin V2 executed") +
                         (algo_message ? std::string(": ") + algo_message : std::string());
        } else if (ctx->strict) {
            if (out_message) {
                *out_message = setMessage(std::string("Strict ASCEND mode: algorithm plugin V2 failed. ") +
                                          (algo_message ? algo_message : "unknown error"));
            }
            return false;
        } else {
            run_detail = std::string("Algorithm plugin V2 failed, fallback CPU placeholder. ") +
                         (algo_message ? algo_message : "unknown error");
        }
    } else if (ctx->algo_run_registration_v1 && device_buffers_valid) {
        mesh_gpu::AscendAlgoRegistrationInputV1 input{};
        input.target_xyz_device = reinterpret_cast<const float*>(ctx->target_xyz_device);
        input.target_point_count = static_cast<int>(ctx->mesh_holder.mesh.num_vertices);
        input.source_xyz_device = reinterpret_cast<const float*>(ctx->source_xyz_device);
        input.source_point_count = static_cast<int>(ctx->source_points.size());
        input.device_id = ctx->device_id;
        input.ascend_stream = ctx->acl_stream;
        input.initial_transform = &transform;
        input.params = params;

        mesh_gpu::AscendAlgoRegistrationOutputV1 output{};
        const char* algo_message = nullptr;
        if (ctx->algo_run_registration_v1(&input, &output, &algo_message)) {
            tx = output.translation_x;
            ty = output.translation_y;
            tz = output.translation_z;
            rmse = output.rmse;
            iterations = output.iterations;
            converged = (output.converged != 0);
            used_algo_plugin = true;
            run_detail = std::string("Ascend algorithm plugin V1 executed") +
                         (algo_message ? std::string(": ") + algo_message : std::string());
        } else if (ctx->strict) {
            if (out_message) {
                *out_message = setMessage(std::string("Strict ASCEND mode: algorithm plugin V1 failed. ") +
                                          (algo_message ? algo_message : "unknown error"));
            }
            return false;
        } else {
            run_detail = std::string("Algorithm plugin V1 failed, fallback CPU placeholder. ") +
                         (algo_message ? algo_message : "unknown error");
        }
    } else if (ctx->algo_run_registration_v1 && !device_buffers_valid) {
        if (ctx->strict) {
            if (out_message) {
                *out_message = setMessage(
                    "Strict ASCEND mode: algorithm plugin V1 requires ready device buffers.");
            }
            return false;
        }
        run_detail = "Algorithm plugin V1 requires device buffers; fallback CPU placeholder.";
    } else {
        run_detail = "No Ascend algorithm plugin/device buffers; fallback CPU placeholder.";
    }

    if (!used_algo_plugin) {
        const mesh_gpu::Point3D src_centroid = computeSourceCentroid(ctx->source_points);
        tx = ctx->target_centroid.x - src_centroid.x;
        ty = ctx->target_centroid.y - src_centroid.y;
        tz = ctx->target_centroid.z - src_centroid.z;

        double mse = 0.0;
        for (const auto& p : ctx->source_points) {
            const float px = p.x + tx;
            const float py = p.y + ty;
            const float pz = p.z + tz;
            const float dx = px - ctx->target_centroid.x;
            const float dy = py - ctx->target_centroid.y;
            const float dz = pz - ctx->target_centroid.z;
            mse += static_cast<double>(dx * dx + dy * dy + dz * dz);
        }
        mse /= static_cast<double>(ctx->source_points.size());
        rmse = static_cast<float>(std::sqrt(std::max(0.0, mse)));
        iterations = params ? std::max(1, std::min(params->max_iterations, 3)) : 1;
        converged = true;
    }

    transform(0, 3) += tx;
    transform(1, 3) += ty;
    transform(2, 3) += tz;

    ctx->rmse_history.clear();
    ctx->rmse_history.push_back(rmse);

    out_result->transform = transform;
    out_result->rmse = rmse;
    out_result->iterations = iterations;
    out_result->converged = converged ? 1 : 0;
    out_result->rmse_history = ctx->rmse_history.data();
    out_result->rmse_history_count = static_cast<int>(ctx->rmse_history.size());

    if (out_message) {
        std::stringstream ss;
        ss << "Registration finished. translation=(" << tx << ", " << ty << ", " << tz << ")"
           << ", rmse=" << rmse << ", iterations=" << iterations
           << ", converged=" << (converged ? "yes" : "no")
           << ". " << run_detail;
        *out_message = setMessage(ss.str());
    }
    return true;
}

} // namespace

#if defined(_WIN32)
#define MESHGPU_ASCEND_PLUGIN_EXPORT __declspec(dllexport)
#else
#define MESHGPU_ASCEND_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" MESHGPU_ASCEND_PLUGIN_EXPORT
const mesh_gpu::AscendBackendApiV1* MeshGPU_GetAscendBackendApiV1() {
    static const mesh_gpu::AscendBackendApiV1 api = {
        mesh_gpu::kAscendBackendApiV1,
        "AscendStubRuntimeHook",
        &createContext,
        &destroyContext,
        &loadTargetMesh,
        &hasTargetMesh,
        &getMeshStats,
        &setSourcePointCloud,
        &clearSourcePointCloud,
        &runRegistration
    };
    return &api;
}
