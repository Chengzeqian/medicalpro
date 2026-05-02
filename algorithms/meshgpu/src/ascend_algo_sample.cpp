#include "ascend_runtime_algo_hook.h"
#include "ascend_cann_kernel_hook.h"
#include "ascend_runtime_loader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace {

thread_local std::string g_last_message;

const char* setMessage(const std::string& msg) {
    g_last_message = msg;
    return g_last_message.c_str();
}

using AclError = mesh_gpu::ascend_runtime::AclError;
using DynamicLibHandle = mesh_gpu::ascend_runtime::DynamicLibHandle;
using AscendRuntimeApi = mesh_gpu::ascend_runtime::RuntimeApi;
constexpr AclError ACL_SUCCESS = 0;
constexpr int ACL_MEMCPY_DEVICE_TO_HOST = 2;

enum class AlgoExecMode {
    kAuto,
    kDeviceOnly,
    kHostOnly
};

std::string toLowerCopy(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

bool isTruthyEnv(const char* key) {
    if (!key) {
        return false;
    }
    const char* raw = std::getenv(key);
    if (!raw || raw[0] == '\0') {
        return false;
    }
    const std::string value = toLowerCopy(std::string(raw));
    return value == "1" || value == "true" || value == "on" || value == "yes";
}

AlgoExecMode resolveExecMode(std::string& mode_text) {
    const char* env = std::getenv("MESHGPU_ASCEND_ALGO_EXEC_MODE");
    if (!env || env[0] == '\0') {
        mode_text = "auto";
        return AlgoExecMode::kAuto;
    }

    mode_text = toLowerCopy(std::string(env));
    if (mode_text == "device_only") {
        return AlgoExecMode::kDeviceOnly;
    }
    if (mode_text == "host_only") {
        return AlgoExecMode::kHostOnly;
    }

    mode_text = "auto";
    return AlgoExecMode::kAuto;
}

struct ExternalCannKernelApi {
    DynamicLibHandle handle = nullptr;
    mesh_gpu::AscendCannKernelRunRegistrationV1Fn run_registration = nullptr;
    std::string plugin_path;

    void close() {
        run_registration = nullptr;
        plugin_path.clear();
        if (handle) {
            mesh_gpu::ascend_runtime::closeDynamicLibrary(handle);
            handle = nullptr;
        }
    }
};

std::vector<std::string> getCannKernelPluginCandidates() {
    std::vector<std::string> candidates;

    const char* env_path = std::getenv("MESHGPU_ASCEND_CANN_KERNEL_PLUGIN");
    if (env_path && env_path[0] != '\0') {
        candidates.emplace_back(env_path);
    }

#if defined(_WIN32)
    candidates.emplace_back("mesgpu_ascend_cann_kernel.dll");
    candidates.emplace_back("MeshGPUAscendCannKernel.dll");
#else
    candidates.emplace_back("libmesgpu_ascend_cann_kernel.so");
    candidates.emplace_back("libMeshGPUAscendCannKernel.so");
#endif

    return candidates;
}

bool loadExternalCannKernelApi(ExternalCannKernelApi& api, std::string& detail) {
    api.close();

    if (isTruthyEnv("MESHGPU_ASCEND_DISABLE_CANN_KERNEL_PLUGIN")) {
        detail = "External CANN kernel plugin loading is disabled by environment.";
        return false;
    }

    for (const auto& candidate : getCannKernelPluginCandidates()) {
        DynamicLibHandle handle = mesh_gpu::ascend_runtime::openDynamicLibrary(candidate);
        if (!handle) {
            continue;
        }

        auto fn = reinterpret_cast<mesh_gpu::AscendCannKernelRunRegistrationV1Fn>(
            mesh_gpu::ascend_runtime::getDynamicSymbol(handle, "MeshGPU_AscendCannKernel_RunRegistrationV1"));
        if (!fn) {
            mesh_gpu::ascend_runtime::closeDynamicLibrary(handle);
            continue;
        }

        api.handle = handle;
        api.run_registration = fn;
        api.plugin_path = candidate;
        detail = "Loaded external CANN kernel plugin: " + candidate;
        return true;
    }

    detail = "No external CANN kernel plugin found.";
    return false;
}

bool computeCentroid(const float* xyz, int count, float& cx, float& cy, float& cz) {
    if (!xyz || count <= 0) {
        return false;
    }
    double sx = 0.0;
    double sy = 0.0;
    double sz = 0.0;
    for (int i = 0; i < count; ++i) {
        const std::size_t base = static_cast<std::size_t>(i) * 3u;
        sx += xyz[base + 0u];
        sy += xyz[base + 1u];
        sz += xyz[base + 2u];
    }
    const double inv = 1.0 / static_cast<double>(count);
    cx = static_cast<float>(sx * inv);
    cy = static_cast<float>(sy * inv);
    cz = static_cast<float>(sz * inv);
    return true;
}

float computeRmseToPoint(const float* source_xyz,
                         int source_count,
                         float tx,
                         float ty,
                         float tz,
                         float target_cx,
                         float target_cy,
                         float target_cz) {
    if (!source_xyz || source_count <= 0) {
        return 0.0f;
    }

    double mse = 0.0;
    for (int i = 0; i < source_count; ++i) {
        const std::size_t base = static_cast<std::size_t>(i) * 3u;
        const float px = source_xyz[base + 0u] + tx;
        const float py = source_xyz[base + 1u] + ty;
        const float pz = source_xyz[base + 2u] + tz;
        const float dx = px - target_cx;
        const float dy = py - target_cy;
        const float dz = pz - target_cz;
        mse += static_cast<double>(dx * dx + dy * dy + dz * dz);
    }
    mse /= static_cast<double>(source_count);
    return static_cast<float>(std::sqrt(std::max(0.0, mse)));
}

bool copyDeviceXYZToHost(const mesh_gpu::AscendAlgoRegistrationInputV2* input,
                         std::vector<float>& target_xyz,
                         std::vector<float>& source_xyz,
                         std::string& detail) {
    if (!input || input->device_buffers_valid == 0 ||
        !input->target_xyz_device || !input->source_xyz_device) {
        detail = "Device buffers are not valid.";
        return false;
    }

    if (input->target_point_count <= 0 || input->source_point_count <= 0) {
        detail = "Invalid point counts for device copy.";
        return false;
    }

    AscendRuntimeApi runtime;
    std::string reason;
    if (!runtime.load(reason, "sample algo plugin")) {
        detail = reason;
        return false;
    }

    const std::size_t target_size =
        static_cast<std::size_t>(input->target_point_count) * 3u * sizeof(float);
    const std::size_t source_size =
        static_cast<std::size_t>(input->source_point_count) * 3u * sizeof(float);
    target_xyz.resize(static_cast<std::size_t>(input->target_point_count) * 3u);
    source_xyz.resize(static_cast<std::size_t>(input->source_point_count) * 3u);

    if (input->ascend_stream) {
        const AclError sync_err = runtime.aclrtSynchronizeStream(input->ascend_stream);
        if (sync_err != ACL_SUCCESS) {
            detail = "aclrtSynchronizeStream failed, error=" + std::to_string(sync_err);
            runtime.unload();
            return false;
        }
    }

    const AclError copy_t_err = runtime.aclrtMemcpy(
        target_xyz.data(), target_size, input->target_xyz_device, target_size, ACL_MEMCPY_DEVICE_TO_HOST);
    if (copy_t_err != ACL_SUCCESS) {
        detail = "aclrtMemcpy target D2H failed, error=" + std::to_string(copy_t_err);
        runtime.unload();
        return false;
    }

    const AclError copy_s_err = runtime.aclrtMemcpy(
        source_xyz.data(), source_size, input->source_xyz_device, source_size, ACL_MEMCPY_DEVICE_TO_HOST);
    if (copy_s_err != ACL_SUCCESS) {
        detail = "aclrtMemcpy source D2H failed, error=" + std::to_string(copy_s_err);
        runtime.unload();
        return false;
    }

    runtime.unload();
    detail = reason;
    return true;
}

} // namespace

#if defined(_WIN32)
#define MESHGPU_ASCEND_ALGO_EXPORT __declspec(dllexport)
#else
#define MESHGPU_ASCEND_ALGO_EXPORT __attribute__((visibility("default")))
#endif

extern "C" MESHGPU_ASCEND_ALGO_EXPORT
bool MeshGPU_AscendAlgo_RunRegistrationV2(
    const mesh_gpu::AscendAlgoRegistrationInputV2* input,
    mesh_gpu::AscendAlgoRegistrationOutputV1* output,
    const char** out_message) {
    if (!input || !output) {
        if (out_message) {
            *out_message = setMessage("Invalid input/output pointer.");
        }
        return false;
    }

    const float* target_xyz = input->target_xyz_host;
    const float* source_xyz = input->source_xyz_host;
    std::vector<float> target_xyz_local;
    std::vector<float> source_xyz_local;
    bool used_device_buffers = false;
    std::string path_detail;
    std::string mode_text;
    const AlgoExecMode mode = resolveExecMode(mode_text);
    const bool force_device_only = (mode == AlgoExecMode::kDeviceOnly);
    const bool force_host_only = (mode == AlgoExecMode::kHostOnly);
    std::string external_kernel_detail;

    if (force_device_only && input->device_buffers_valid == 0) {
        if (out_message) {
            *out_message = setMessage(
                "device_only mode failed. Ascend device buffers are not valid in algorithm plugin input.");
        }
        return false;
    }

    if (!force_host_only) {
        ExternalCannKernelApi kernel_api;
        if (loadExternalCannKernelApi(kernel_api, external_kernel_detail)) {
            mesh_gpu::AscendCannKernelRegistrationInputV1 kernel_input{};
            kernel_input.target_xyz_device = input->target_xyz_device;
            kernel_input.target_point_count = input->target_point_count;
            kernel_input.source_xyz_device = input->source_xyz_device;
            kernel_input.source_point_count = input->source_point_count;
            kernel_input.target_xyz_host = input->target_xyz_host;
            kernel_input.source_xyz_host = input->source_xyz_host;
            kernel_input.device_buffers_valid = input->device_buffers_valid;
            kernel_input.device_id = input->device_id;
            kernel_input.ascend_stream = input->ascend_stream;
            kernel_input.initial_transform = input->initial_transform;
            kernel_input.params = input->params;

            mesh_gpu::AscendAlgoRegistrationOutputV1 kernel_output{};
            const char* kernel_message = nullptr;
            if (kernel_api.run_registration(&kernel_input, &kernel_output, &kernel_message)) {
                *output = kernel_output;
                if (out_message) {
                    std::stringstream ss;
                    ss << "Sample V2 plugin delegated to external CANN kernel plugin"
                       << "; mode=" << mode_text
                       << "; plugin=" << kernel_api.plugin_path
                       << "; detail=" << (kernel_message ? kernel_message : "ok");
                    *out_message = setMessage(ss.str());
                }
                kernel_api.close();
                return true;
            }

            external_kernel_detail =
                std::string("External CANN kernel plugin failed: ") +
                (kernel_message ? kernel_message : "unknown error");
            kernel_api.close();
            if (force_device_only) {
                if (out_message) {
                    *out_message = setMessage("device_only mode failed. " + external_kernel_detail);
                }
                return false;
            }
        }
    }

    if (!force_host_only) {
        if (copyDeviceXYZToHost(input, target_xyz_local, source_xyz_local, path_detail)) {
            target_xyz = target_xyz_local.data();
            source_xyz = source_xyz_local.data();
            used_device_buffers = true;
        } else if (force_device_only) {
            if (out_message) {
                std::string detail = "device_only mode failed. " + path_detail;
                if (!external_kernel_detail.empty()) {
                    detail += " " + external_kernel_detail;
                }
                *out_message = setMessage(detail);
            }
            return false;
        }
    }

    if (!used_device_buffers) {
        if (target_xyz && source_xyz) {
            if (force_host_only) {
                path_detail = "Forced host_only mode.";
            } else {
                path_detail = "Host fallback path.";
            }
        } else {
            if (out_message) {
                *out_message = setMessage("Sample plugin has no usable data path. " + path_detail);
            }
            return false;
        }
    }

    float target_cx = 0.0f;
    float target_cy = 0.0f;
    float target_cz = 0.0f;
    float source_cx = 0.0f;
    float source_cy = 0.0f;
    float source_cz = 0.0f;

    if (!computeCentroid(target_xyz, input->target_point_count, target_cx, target_cy, target_cz) ||
        !computeCentroid(source_xyz, input->source_point_count, source_cx, source_cy, source_cz)) {
        if (out_message) {
            *out_message = setMessage("Invalid point count or host buffers.");
        }
        return false;
    }

    const float tx = target_cx - source_cx;
    const float ty = target_cy - source_cy;
    const float tz = target_cz - source_cz;

    output->translation_x = tx;
    output->translation_y = ty;
    output->translation_z = tz;
    output->rmse = computeRmseToPoint(
        source_xyz,
        input->source_point_count,
        tx,
        ty,
        tz,
        target_cx,
        target_cy,
        target_cz);
    output->iterations = input->params ? std::max(1, std::min(input->params->max_iterations, 2)) : 2;
    output->converged = 1;

    if (out_message) {
        std::stringstream ss;
        ss << "Sample V2 plugin executed via "
           << (used_device_buffers ? "device-buffer D2H path" : "host-fallback path")
           << "; device_buffers_valid=" << input->device_buffers_valid
           << "; mode=" << mode_text
           << "; detail=" << path_detail;
        if (!external_kernel_detail.empty()) {
            ss << "; external_kernel=" << external_kernel_detail;
        }
        *out_message = setMessage(ss.str());
    }
    return true;
}
