// ============================================================================
// MeshGPU Interface Implementation (CUDA)
// ============================================================================

#include "mesh_gpu_interface.h"
#include "mesh_gpu_runtime_api.h"
#include "mesh_gpu.h"
#include "gicp_registration.h"
#include "probe_simulator.h"
#include "rotation_search.h"
#include "ply_reader.h"
#include "types.h"
#include "ascend_backend_plugin.h"

#include <cuda_runtime.h>
#include <thrust/device_ptr.h>
#include <thrust/scan.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

namespace mesh_gpu {

namespace {

const char* backendKindToCString(ComputeBackendKind kind) {
    switch (kind) {
    case ComputeBackendKind::AUTO:
        return "AUTO";
    case ComputeBackendKind::CUDA:
        return "CUDA";
    case ComputeBackendKind::ASCEND:
        return "ASCEND";
    case ComputeBackendKind::CPU:
        return "CPU";
    default:
        return "UNKNOWN";
    }
}

#if defined(_WIN32)
using DynamicLibHandle = HMODULE;

DynamicLibHandle openDynamicLibrary(const std::string& path) {
    return LoadLibraryA(path.c_str());
}

void closeDynamicLibrary(DynamicLibHandle handle) {
    if (handle) {
        FreeLibrary(handle);
    }
}

void* getDynamicSymbol(DynamicLibHandle handle, const char* symbol_name) {
    if (!handle || !symbol_name) {
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(handle, symbol_name));
}
#else
using DynamicLibHandle = void*;

DynamicLibHandle openDynamicLibrary(const std::string& path) {
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
}

void closeDynamicLibrary(DynamicLibHandle handle) {
    if (handle) {
        dlclose(handle);
    }
}

void* getDynamicSymbol(DynamicLibHandle handle, const char* symbol_name) {
    if (!handle || !symbol_name) {
        return nullptr;
    }
    return dlsym(handle, symbol_name);
}
#endif

struct LoadedAscendPlugin {
    DynamicLibHandle handle = nullptr;
    const AscendBackendApiV1* api = nullptr;
    std::string path;
    std::string detail;
};

std::vector<std::string> getAscendPluginCandidates() {
    std::vector<std::string> candidates;

    const char* env_path = std::getenv("MESHGPU_ASCEND_PLUGIN");
    if (env_path && env_path[0] != '\0') {
        candidates.emplace_back(env_path);
    }

#if defined(_WIN32)
    candidates.emplace_back("mesgpu_ascend_backend.dll");
    candidates.emplace_back("MeshGPUAscendBackend.dll");
#else
    candidates.emplace_back("libmesgpu_ascend_backend.so");
    candidates.emplace_back("libMeshGPUAscendBackend.so");
#endif

    return candidates;
}

bool validateAscendPluginApi(const AscendBackendApiV1* api, std::string& detail) {
    if (!api) {
        detail = "Plugin symbol returned null API pointer.";
        return false;
    }

    if (api->abi_version != kAscendBackendApiV1) {
        detail = "Plugin ABI mismatch. Expected " + std::to_string(kAscendBackendApiV1) +
                 ", got " + std::to_string(api->abi_version);
        return false;
    }

    const bool required_ok =
        (api->create_context != nullptr) &&
        (api->destroy_context != nullptr) &&
        (api->load_target_mesh != nullptr) &&
        (api->has_target_mesh != nullptr) &&
        (api->get_mesh_stats != nullptr) &&
        (api->set_source_point_cloud != nullptr) &&
        (api->clear_source_point_cloud != nullptr) &&
        (api->run_registration != nullptr);

    if (!required_ok) {
        detail = "Plugin API missing one or more required function pointers.";
        return false;
    }

    detail = "API validated";
    return true;
}

bool tryLoadAscendPlugin(LoadedAscendPlugin& plugin, std::string& detail) {
    const auto candidates = getAscendPluginCandidates();
    if (candidates.empty()) {
        detail = "No Ascend plugin candidates found.";
        return false;
    }

    for (const auto& candidate : candidates) {
        DynamicLibHandle handle = openDynamicLibrary(candidate);
        if (!handle) {
            continue;
        }

        auto symbol = reinterpret_cast<GetAscendBackendApiV1Fn>(
            getDynamicSymbol(handle, "MeshGPU_GetAscendBackendApiV1"));
        if (!symbol) {
            closeDynamicLibrary(handle);
            continue;
        }

        const AscendBackendApiV1* api = symbol();
        std::string api_detail;
        if (!validateAscendPluginApi(api, api_detail)) {
            closeDynamicLibrary(handle);
            continue;
        }

        plugin.handle = handle;
        plugin.api = api;
        plugin.path = candidate;
        plugin.detail = api_detail;

        detail = "Loaded Ascend plugin: " + candidate +
                 ", backend=" + std::string(api->backend_name ? api->backend_name : "unknown");
        return true;
    }

    detail = "Ascend plugin not found. Set MESHGPU_ASCEND_PLUGIN to plugin library path.";
    return false;
}

bool probeAscendPlugin(std::string& detail) {
    LoadedAscendPlugin plugin;
    if (!tryLoadAscendPlugin(plugin, detail)) {
        return false;
    }
    closeDynamicLibrary(plugin.handle);
    return true;
}

bool probeCudaRuntime(int device_id, std::string& detail) {
    int device_count = 0;
    cudaError_t error = cudaGetDeviceCount(&device_count);
    if (error != cudaSuccess) {
        detail = std::string("cudaGetDeviceCount failed: ") + cudaGetErrorString(error);
        return false;
    }

    if (device_count <= 0) {
        detail = "No CUDA device found";
        return false;
    }

    if (device_id < 0 || device_id >= device_count) {
        detail = "Requested CUDA device id " + std::to_string(device_id) +
                 " out of range [0, " + std::to_string(device_count - 1) + "]";
        return false;
    }

    error = cudaSetDevice(device_id);
    if (error != cudaSuccess) {
        detail = std::string("cudaSetDevice failed: ") + cudaGetErrorString(error);
        return false;
    }

    cudaDeviceProp prop;
    error = cudaGetDeviceProperties(&prop, device_id);
    if (error == cudaSuccess) {
        detail = "CUDA device " + std::to_string(device_id) + ": " + prop.name;
    } else {
        detail = "CUDA device " + std::to_string(device_id) + " selected";
    }
    return true;
}

bool probeAscendRuntime(std::string& detail) {
#ifdef _WIN32
    const char* candidates[] = {"ascendcl.dll", "acl.dll"};
    for (const char* lib : candidates) {
        HMODULE handle = LoadLibraryA(lib);
        if (handle) {
            FreeLibrary(handle);
            detail = std::string("Detected runtime library: ") + lib;
            return true;
        }
    }
    detail = "Ascend runtime library not found (tried ascendcl.dll, acl.dll)";
    return false;
#else
    const char* candidates[] = {"libascendcl.so", "libascendcl.so.1", "libacl.so", "libacl_rt.so"};
    for (const char* lib : candidates) {
        void* handle = dlopen(lib, RTLD_LAZY | RTLD_LOCAL);
        if (handle) {
            dlclose(handle);
            detail = std::string("Detected runtime library: ") + lib;
            return true;
        }
    }
    detail = "Ascend runtime library not found (tried libascendcl.so/libacl.so variants)";
    return false;
#endif
}

BackendStatus resolveBackendStatus(const BackendConfig& config) {
    BackendStatus status;
    status.requested = config.requested;
    status.selected = ComputeBackendKind::CPU;
    status.initialized = false;
    status.cpu_available = true;

    std::string cuda_detail;
    std::string ascend_runtime_detail;
    std::string ascend_plugin_detail;
    status.cuda_available = probeCudaRuntime(config.device_id, cuda_detail);
    status.ascend_runtime_available = probeAscendRuntime(ascend_runtime_detail);
    status.ascend_plugin_available = probeAscendPlugin(ascend_plugin_detail);

    auto set_status = [&](ComputeBackendKind selected, bool initialized, const std::string& message) {
        status.selected = selected;
        status.initialized = initialized;
        status.message = message;
    };

    switch (config.requested) {
    case ComputeBackendKind::AUTO:
        if (status.cuda_available) {
            set_status(ComputeBackendKind::CUDA, true, "AUTO selected CUDA. " + cuda_detail);
        } else if (status.ascend_plugin_available) {
            set_status(ComputeBackendKind::ASCEND, true,
                       "AUTO selected ASCEND plugin. " + ascend_plugin_detail +
                           " Runtime probe: " + ascend_runtime_detail);
        } else {
            set_status(ComputeBackendKind::CPU, false,
                       "AUTO fallback to CPU placeholder. CUDA: " + cuda_detail +
                           "; ASCEND runtime: " + ascend_runtime_detail +
                           "; ASCEND plugin: " + ascend_plugin_detail);
        }
        break;

    case ComputeBackendKind::CUDA:
        if (status.cuda_available) {
            set_status(ComputeBackendKind::CUDA, true, "CUDA selected. " + cuda_detail);
        } else if (config.strict) {
            set_status(ComputeBackendKind::CUDA, false, "Strict CUDA mode failed. " + cuda_detail);
        } else if (status.ascend_plugin_available) {
            set_status(ComputeBackendKind::ASCEND, true,
                       "CUDA unavailable, fallback to ASCEND plugin. " + ascend_plugin_detail);
        } else {
            set_status(ComputeBackendKind::CPU, false, "CUDA unavailable, fallback to CPU placeholder. " + cuda_detail);
        }
        break;

    case ComputeBackendKind::ASCEND:
        if (status.ascend_plugin_available) {
            set_status(ComputeBackendKind::ASCEND, true,
                       "ASCEND selected. " + ascend_plugin_detail +
                           " Runtime probe: " + ascend_runtime_detail);
        } else if (config.strict) {
            set_status(ComputeBackendKind::ASCEND, false,
                       "Strict ASCEND mode failed. Plugin: " + ascend_plugin_detail +
                           "; Runtime: " + ascend_runtime_detail);
        } else if (status.cuda_available) {
            set_status(ComputeBackendKind::CUDA, true,
                       "ASCEND plugin unavailable, fallback to CUDA. " + cuda_detail);
        } else {
            set_status(ComputeBackendKind::CPU, false,
                       "ASCEND plugin unavailable, fallback to CPU placeholder.");
        }
        break;

    case ComputeBackendKind::CPU:
        set_status(ComputeBackendKind::CPU, false, "CPU backend is a placeholder (compute kernels not implemented).");
        break;

    default:
        set_status(ComputeBackendKind::CPU, false, "Unknown backend request; fallback to CPU placeholder.");
        break;
    }

    return status;
}

std::string buildBackendInfoString(const BackendStatus& status) {
    std::stringstream ss;
    ss << "Backend status: requested=" << backendKindToCString(status.requested)
       << ", selected=" << backendKindToCString(status.selected)
       << ", initialized=" << (status.initialized ? "yes" : "no")
       << ", cuda_available=" << (status.cuda_available ? "yes" : "no")
       << ", ascend_runtime_available=" << (status.ascend_runtime_available ? "yes" : "no")
       << ", ascend_plugin_available=" << (status.ascend_plugin_available ? "yes" : "no")
       << ", cpu_available=" << (status.cpu_available ? "yes" : "no")
       << ", message=" << status.message;
    return ss.str();
}

} // namespace

// ============================================================================
// PIMPL Implementation
// ============================================================================

class MeshGPUInterface::Impl {
public:
    Impl()
        : mesh_(nullptr)
        , source_cloud_(nullptr)
        , source_cloud_owned_(false)
        , gicp_(nullptr)
        , probe_sim_(nullptr)
        , backend_config_()
        , backend_status_()
        , ascend_plugin_handle_(nullptr)
        , ascend_api_(nullptr)
        , ascend_context_(nullptr) {
        std::memset(&host_mesh_, 0, sizeof(MeshSoA));
        std::memset(&stats_, 0, sizeof(MeshStats));
    }

    ~Impl() {
        cleanup();
        shutdownAscendBackend();
    }

    void cleanup() {
        freeSourceCloud();
        if (probe_sim_) {
            delete probe_sim_;
            probe_sim_ = nullptr;
        }
        if (gicp_) {
            delete gicp_;
            gicp_ = nullptr;
        }
        if (mesh_) {
            delete mesh_;
            mesh_ = nullptr;
        }
        source_points_.clear();
        freeMeshHost();
    }

    void freeMeshHost() {
        if (host_mesh_.vertices_x) {
            delete[] host_mesh_.vertices_x;
            delete[] host_mesh_.vertices_y;
            delete[] host_mesh_.vertices_z;
            delete[] host_mesh_.normals_x;
            delete[] host_mesh_.normals_y;
            delete[] host_mesh_.normals_z;
            delete[] host_mesh_.curvature;
            delete[] host_mesh_.gaussian_curv;
            delete[] host_mesh_.faces_v0;
            delete[] host_mesh_.faces_v1;
            delete[] host_mesh_.faces_v2;
            delete[] host_mesh_.face_normals_x;
            delete[] host_mesh_.face_normals_y;
            delete[] host_mesh_.face_normals_z;
            delete[] host_mesh_.face_areas;
            host_mesh_ = MeshSoA();
        }
    }

    // Target mesh
    MeshGPU* mesh_;
    MeshSoA host_mesh_;
    MeshStats stats_;

    // Source point cloud
    SourcePointCloud* source_cloud_;
    bool source_cloud_owned_;
    std::vector<Point3D> source_points_;

    // GICP registration
    GICPRegistration* gicp_;
    RegistrationResult last_result_;

    // Probe simulator for testing
    ProbeSimulator* probe_sim_;
    Transform4x4 ground_truth_transform_;

    // Explicit anchor override (set by external caller for live mode)
    Point3D source_anchor_override_;
    bool has_source_anchor_override_ = false;

    // Backend configuration/runtime status
    BackendConfig backend_config_;
    BackendStatus backend_status_;
    DynamicLibHandle ascend_plugin_handle_;
    const AscendBackendApiV1* ascend_api_;
    void* ascend_context_;
    std::string ascend_plugin_path_;

    void shutdownAscendBackend() {
        if (ascend_api_ && ascend_context_ && ascend_api_->destroy_context) {
            ascend_api_->destroy_context(ascend_context_);
        }
        ascend_context_ = nullptr;
        ascend_api_ = nullptr;

        if (ascend_plugin_handle_) {
            closeDynamicLibrary(ascend_plugin_handle_);
        }
        ascend_plugin_handle_ = nullptr;
        ascend_plugin_path_.clear();
    }

    bool initializeAscendBackend() {
        shutdownAscendBackend();

        LoadedAscendPlugin plugin;
        std::string load_detail;
        if (!tryLoadAscendPlugin(plugin, load_detail)) {
            backend_status_.initialized = false;
            backend_status_.message = "Failed to load ASCEND plugin. " + load_detail;
            return false;
        }

        if (!plugin.api || !plugin.api->create_context) {
            closeDynamicLibrary(plugin.handle);
            backend_status_.initialized = false;
            backend_status_.message = "ASCEND plugin create_context entry is missing.";
            return false;
        }

        const char* plugin_message = nullptr;
        void* context = nullptr;
        const bool created = plugin.api->create_context(
            backend_config_.device_id,
            backend_config_.strict,
            &context,
            &plugin_message);

        if (!created || !context) {
            closeDynamicLibrary(plugin.handle);
            backend_status_.initialized = false;
            backend_status_.message =
                "ASCEND plugin create_context failed: " +
                std::string(plugin_message ? plugin_message : "unknown error");
            return false;
        }

        ascend_plugin_handle_ = plugin.handle;
        ascend_api_ = plugin.api;
        ascend_context_ = context;
        ascend_plugin_path_ = plugin.path;

        backend_status_.initialized = true;
        backend_status_.selected = ComputeBackendKind::ASCEND;
        backend_status_.message =
            "ASCEND plugin initialized: " + ascend_plugin_path_ +
            (plugin_message ? std::string(", ") + plugin_message : std::string());
        return true;
    }

    // Helper to free SourcePointCloud
    void freeSourceCloud() {
        if (source_cloud_) {
            if (source_cloud_owned_ && source_cloud_->on_device) {
                cudaFree(source_cloud_->points_x);
                cudaFree(source_cloud_->points_y);
                cudaFree(source_cloud_->points_z);
                if (source_cloud_->normals_x) cudaFree(source_cloud_->normals_x);
                if (source_cloud_->normals_y) cudaFree(source_cloud_->normals_y);
                if (source_cloud_->normals_z) cudaFree(source_cloud_->normals_z);
            }
            delete source_cloud_;
            source_cloud_ = nullptr;
            source_cloud_owned_ = false;
        }
    }

    // Helper to create SourcePointCloud from vector
    bool updateSourceCloud() {
        if (source_points_.empty()) {
            return false;
        }

        // Free old cloud
        freeSourceCloud();

        // Create new cloud
        source_cloud_ = new SourcePointCloud();
        source_cloud_owned_ = true;
        source_cloud_->num_points = static_cast<uint32_t>(source_points_.size());
        source_cloud_->on_device = true;

        // Allocate device memory
        size_t size = source_points_.size() * sizeof(float);
        cudaMalloc(&source_cloud_->points_x, size);
        cudaMalloc(&source_cloud_->points_y, size);
        cudaMalloc(&source_cloud_->points_z, size);
        source_cloud_->normals_x = nullptr;
        source_cloud_->normals_y = nullptr;
        source_cloud_->normals_z = nullptr;

        // Copy data to device
        std::vector<float> x(source_points_.size());
        std::vector<float> y(source_points_.size());
        std::vector<float> z(source_points_.size());

        for (size_t i = 0; i < source_points_.size(); i++) {
            x[i] = source_points_[i].x;
            y[i] = source_points_[i].y;
            z[i] = source_points_[i].z;
        }

        cudaMemcpy(source_cloud_->points_x, x.data(), size, cudaMemcpyHostToDevice);
        cudaMemcpy(source_cloud_->points_y, y.data(), size, cudaMemcpyHostToDevice);
        cudaMemcpy(source_cloud_->points_z, z.data(), size, cudaMemcpyHostToDevice);

        return true;
    }

    // Reference source cloud owned by ProbeSimulator
    void setSourceCloudReference(const SourcePointCloud& source_cloud) {
        freeSourceCloud();
        source_cloud_ = new SourcePointCloud();
        *source_cloud_ = source_cloud;
        source_cloud_owned_ = false;
    }

    bool ensureCudaBackend(const char* operation) const {
        if (backend_status_.selected == ComputeBackendKind::CUDA && backend_status_.initialized) {
            return true;
        }

        std::cerr << "[MeshGPUInterface] " << operation
                  << " requires CUDA backend. Current backend: "
                  << MeshGPUInterface::backendKindToString(backend_status_.selected)
                  << ". " << backend_status_.message << std::endl;
        return false;
    }
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

MeshGPUInterface::MeshGPUInterface()
    : pImpl(std::make_unique<Impl>())
{
    pImpl->backend_status_ = probeBackend(pImpl->backend_config_);
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND &&
        pImpl->backend_status_.initialized) {
        pImpl->initializeAscendBackend();
    }
}

MeshGPUInterface::~MeshGPUInterface() = default;

MeshGPUInterface::MeshGPUInterface(MeshGPUInterface&&) noexcept = default;
MeshGPUInterface& MeshGPUInterface::operator=(MeshGPUInterface&&) noexcept = default;

// ============================================================================
// Static Methods
// ============================================================================

bool MeshGPUInterface::isCudaAvailable() {
    int deviceCount = 0;
    cudaError_t error = cudaGetDeviceCount(&deviceCount);
    return (error == cudaSuccess && deviceCount > 0);
}

std::string MeshGPUInterface::getCudaDeviceInfo() {
    std::stringstream ss;

    int deviceCount = 0;
    cudaError_t error = cudaGetDeviceCount(&deviceCount);

    if (error != cudaSuccess) {
        ss << "CUDA Error: " << cudaGetErrorString(error);
        return ss.str();
    }

    if (deviceCount == 0) {
        ss << "No CUDA devices found";
        return ss.str();
    }

    for (int i = 0; i < deviceCount; i++) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);

        ss << "Device " << i << ": " << prop.name << "\n";
        ss << "  Compute capability: " << prop.major << "." << prop.minor << "\n";
        ss << "  Total memory: " << (prop.totalGlobalMem / (1024 * 1024)) << " MB\n";
        ss << "  Multiprocessors: " << prop.multiProcessorCount << "\n";
    }

    return ss.str();
}

const char* MeshGPUInterface::backendKindToString(ComputeBackendKind kind) {
    return backendKindToCString(kind);
}

BackendStatus MeshGPUInterface::probeBackend(const BackendConfig& config) {
    return resolveBackendStatus(config);
}

void MeshGPUInterface::setBackendConfig(const BackendConfig& config) {
    pImpl->cleanup();
    pImpl->shutdownAscendBackend();
    pImpl->backend_config_ = config;
    pImpl->backend_status_ = probeBackend(config);
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND &&
        pImpl->backend_status_.initialized) {
        pImpl->initializeAscendBackend();
    }
}

BackendConfig MeshGPUInterface::getBackendConfig() const {
    return pImpl->backend_config_;
}

BackendStatus MeshGPUInterface::getBackendStatus() const {
    return pImpl->backend_status_;
}

std::string MeshGPUInterface::getBackendInfo() const {
    return buildBackendInfoString(pImpl->backend_status_);
}

// ============================================================================
// Target Mesh Operations
// ============================================================================

bool MeshGPUInterface::loadTargetMesh(const std::string& ply_file, float cell_size) {
    pImpl->cleanup();

    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        if (!pImpl->backend_status_.initialized || !pImpl->ascend_api_ || !pImpl->ascend_context_) {
            std::cerr << "[MeshGPUInterface] ASCEND backend is not initialized. "
                      << pImpl->backend_status_.message << std::endl;
            return false;
        }

        const char* plugin_message = nullptr;
        if (!pImpl->ascend_api_->load_target_mesh(pImpl->ascend_context_,
                                                  ply_file.c_str(),
                                                  cell_size,
                                                  &plugin_message)) {
            std::cerr << "[MeshGPUInterface] ASCEND loadTargetMesh failed: "
                      << (plugin_message ? plugin_message : "unknown error") << std::endl;
            return false;
        }

        MeshStats plugin_stats{};
        if (pImpl->ascend_api_->get_mesh_stats &&
            pImpl->ascend_api_->get_mesh_stats(pImpl->ascend_context_, &plugin_stats)) {
            pImpl->stats_ = plugin_stats;
        }

        std::cout << "[MeshGPUInterface] ASCEND loadTargetMesh succeeded."
                  << (plugin_message ? std::string(" ") + plugin_message : std::string())
                  << std::endl;
        return true;
    }

    if (!pImpl->ensureCudaBackend("loadTargetMesh")) {
        return false;
    }

    try {
        // Read PLY file into MeshSoA
        if (!PLYReader::readPLY(ply_file, pImpl->host_mesh_)) {
            std::cerr << "Failed to read PLY file: " << ply_file << std::endl;
            return false;
        }

        // Create MeshGPU and initialize
        pImpl->mesh_ = new MeshGPU();

        if (!pImpl->mesh_->initialize(pImpl->host_mesh_)) {
            std::cerr << "Failed to initialize MeshGPU" << std::endl;
            delete pImpl->mesh_;
            pImpl->mesh_ = nullptr;
            return false;
        }

        // Build grid index for point queries
        if (!pImpl->mesh_->buildGridIndex(cell_size)) {
            std::cerr << "Failed to build grid index" << std::endl;
            delete pImpl->mesh_;
            pImpl->mesh_ = nullptr;
            return false;
        }

        // Update stats
        pImpl->stats_.num_vertices = static_cast<int>(pImpl->host_mesh_.num_vertices);
        pImpl->stats_.num_triangles = static_cast<int>(pImpl->host_mesh_.num_faces);
        pImpl->stats_.cell_size = cell_size;

        // Get bounding box from mesh
        BoundingBox bbox = pImpl->mesh_->getBoundingBox();
        pImpl->stats_.bounding_box_min = Point3D(bbox.min_pt.x, bbox.min_pt.y, bbox.min_pt.z);
        pImpl->stats_.bounding_box_max = Point3D(bbox.max_pt.x, bbox.max_pt.y, bbox.max_pt.z);

        std::cout << "[MeshGPUInterface] Loaded mesh: " << pImpl->stats_.num_vertices
                  << " vertices, " << pImpl->stats_.num_triangles << " triangles" << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception loading mesh: " << e.what() << std::endl;
        pImpl->cleanup();
        return false;
    }
}

bool MeshGPUInterface::setTargetMesh(const std::vector<Point3D>& vertices,
                                      const std::vector<Normal3D>& normals,
                                      const std::vector<std::array<int, 3>>& triangles,
                                      float cell_size)
{
    pImpl->cleanup();

    if (vertices.empty() || triangles.empty()) {
        return false;
    }

    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        std::cerr << "[MeshGPUInterface] setTargetMesh is not exposed by current ASCEND plugin API. "
                  << "Use loadTargetMesh(ply_file) for ASCEND workflow." << std::endl;
        return false;
    }

    if (!pImpl->ensureCudaBackend("setTargetMesh")) {
        return false;
    }

    try {
        // Allocate MeshSoA
        uint32_t num_vertices = static_cast<uint32_t>(vertices.size());
        uint32_t num_faces = static_cast<uint32_t>(triangles.size());

        pImpl->host_mesh_.num_vertices = num_vertices;
        pImpl->host_mesh_.num_faces = num_faces;

        pImpl->host_mesh_.vertices_x = new float[num_vertices];
        pImpl->host_mesh_.vertices_y = new float[num_vertices];
        pImpl->host_mesh_.vertices_z = new float[num_vertices];
        pImpl->host_mesh_.normals_x = new float[num_vertices];
        pImpl->host_mesh_.normals_y = new float[num_vertices];
        pImpl->host_mesh_.normals_z = new float[num_vertices];
        pImpl->host_mesh_.curvature = new float[num_vertices]();
        pImpl->host_mesh_.gaussian_curv = new float[num_vertices]();
        pImpl->host_mesh_.faces_v0 = new uint32_t[num_faces];
        pImpl->host_mesh_.faces_v1 = new uint32_t[num_faces];
        pImpl->host_mesh_.faces_v2 = new uint32_t[num_faces];
        pImpl->host_mesh_.face_normals_x = new float[num_faces]();
        pImpl->host_mesh_.face_normals_y = new float[num_faces]();
        pImpl->host_mesh_.face_normals_z = new float[num_faces]();
        pImpl->host_mesh_.face_areas = new float[num_faces]();

        // Copy vertex data
        for (size_t i = 0; i < vertices.size(); i++) {
            pImpl->host_mesh_.vertices_x[i] = vertices[i].x;
            pImpl->host_mesh_.vertices_y[i] = vertices[i].y;
            pImpl->host_mesh_.vertices_z[i] = vertices[i].z;
        }

        // Copy normals (or use defaults)
        if (normals.size() == vertices.size()) {
            for (size_t i = 0; i < normals.size(); i++) {
                pImpl->host_mesh_.normals_x[i] = normals[i].nx;
                pImpl->host_mesh_.normals_y[i] = normals[i].ny;
                pImpl->host_mesh_.normals_z[i] = normals[i].nz;
            }
        } else {
            for (size_t i = 0; i < vertices.size(); i++) {
                pImpl->host_mesh_.normals_x[i] = 0.0f;
                pImpl->host_mesh_.normals_y[i] = 0.0f;
                pImpl->host_mesh_.normals_z[i] = 1.0f;
            }
        }

        // Copy triangles
        for (size_t i = 0; i < triangles.size(); i++) {
            pImpl->host_mesh_.faces_v0[i] = static_cast<uint32_t>(triangles[i][0]);
            pImpl->host_mesh_.faces_v1[i] = static_cast<uint32_t>(triangles[i][1]);
            pImpl->host_mesh_.faces_v2[i] = static_cast<uint32_t>(triangles[i][2]);
        }

        // Create MeshGPU and initialize
        pImpl->mesh_ = new MeshGPU();

        if (!pImpl->mesh_->initialize(pImpl->host_mesh_)) {
            std::cerr << "Failed to initialize MeshGPU" << std::endl;
            delete pImpl->mesh_;
            pImpl->mesh_ = nullptr;
            return false;
        }

        // Build grid index
        if (!pImpl->mesh_->buildGridIndex(cell_size)) {
            std::cerr << "Failed to build grid index" << std::endl;
            delete pImpl->mesh_;
            pImpl->mesh_ = nullptr;
            return false;
        }

        // Update stats
        pImpl->stats_.num_vertices = static_cast<int>(num_vertices);
        pImpl->stats_.num_triangles = static_cast<int>(num_faces);
        pImpl->stats_.cell_size = cell_size;

        BoundingBox bbox = pImpl->mesh_->getBoundingBox();
        pImpl->stats_.bounding_box_min = Point3D(bbox.min_pt.x, bbox.min_pt.y, bbox.min_pt.z);
        pImpl->stats_.bounding_box_max = Point3D(bbox.max_pt.x, bbox.max_pt.y, bbox.max_pt.z);

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception setting mesh: " << e.what() << std::endl;
        pImpl->cleanup();
        return false;
    }
}

bool MeshGPUInterface::hasTargetMesh() const {
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        if (!pImpl->backend_status_.initialized || !pImpl->ascend_api_ || !pImpl->ascend_context_) {
            return false;
        }
        if (pImpl->ascend_api_->has_target_mesh) {
            return pImpl->ascend_api_->has_target_mesh(pImpl->ascend_context_);
        }
        return false;
    }
    return pImpl->mesh_ != nullptr;
}

MeshStats MeshGPUInterface::getMeshStats() const {
    return pImpl->stats_;
}

// ============================================================================
// Source Point Cloud Operations
// ============================================================================

bool MeshGPUInterface::setSourcePointCloud(const std::vector<Point3D>& points) {
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        if (!pImpl->backend_status_.initialized || !pImpl->ascend_api_ || !pImpl->ascend_context_) {
            std::cerr << "[MeshGPUInterface] ASCEND backend is not initialized." << std::endl;
            return false;
        }

        pImpl->source_points_ = points;
        const char* plugin_message = nullptr;
        const bool ok = pImpl->ascend_api_->set_source_point_cloud(
            pImpl->ascend_context_,
            points.empty() ? nullptr : points.data(),
            static_cast<int>(points.size()),
            &plugin_message);

        if (!ok) {
            std::cerr << "[MeshGPUInterface] ASCEND setSourcePointCloud failed: "
                      << (plugin_message ? plugin_message : "unknown error") << std::endl;
        }
        return ok;
    }

    if (!pImpl->ensureCudaBackend("setSourcePointCloud")) {
        return false;
    }
    pImpl->source_points_ = points;
    return pImpl->updateSourceCloud();
}

bool MeshGPUInterface::addSourcePoints(const std::vector<Point3D>& points) {
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        if (!pImpl->backend_status_.initialized || !pImpl->ascend_api_ || !pImpl->ascend_context_) {
            std::cerr << "[MeshGPUInterface] ASCEND backend is not initialized." << std::endl;
            return false;
        }

        pImpl->source_points_.insert(pImpl->source_points_.end(), points.begin(), points.end());

        const char* plugin_message = nullptr;
        const bool ok = pImpl->ascend_api_->set_source_point_cloud(
            pImpl->ascend_context_,
            pImpl->source_points_.empty() ? nullptr : pImpl->source_points_.data(),
            static_cast<int>(pImpl->source_points_.size()),
            &plugin_message);

        if (!ok) {
            std::cerr << "[MeshGPUInterface] ASCEND addSourcePoints failed: "
                      << (plugin_message ? plugin_message : "unknown error") << std::endl;
        }
        return ok;
    }

    if (!pImpl->ensureCudaBackend("addSourcePoints")) {
        return false;
    }
    pImpl->source_points_.insert(pImpl->source_points_.end(), points.begin(), points.end());
    return pImpl->updateSourceCloud();
}

void MeshGPUInterface::clearSourcePoints() {
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND &&
        pImpl->backend_status_.initialized &&
        pImpl->ascend_api_ &&
        pImpl->ascend_api_->clear_source_point_cloud &&
        pImpl->ascend_context_) {
        const char* plugin_message = nullptr;
        pImpl->ascend_api_->clear_source_point_cloud(pImpl->ascend_context_, &plugin_message);
    }

    pImpl->source_points_.clear();
    pImpl->freeSourceCloud();
}

int MeshGPUInterface::getSourcePointCount() const {
    return static_cast<int>(pImpl->source_points_.size());
}

// ============================================================================
// Anchor Point Override
// ============================================================================

void MeshGPUInterface::setSourceAnchorOverride(const Point3D& anchor) {
    pImpl->source_anchor_override_ = anchor;
    pImpl->has_source_anchor_override_ = true;
    std::cout << "[MeshGPUInterface] Source anchor override set: ("
              << anchor.x << ", " << anchor.y << ", " << anchor.z << ")" << std::endl;
}

void MeshGPUInterface::clearSourceAnchorOverride() {
    pImpl->has_source_anchor_override_ = false;
}

bool MeshGPUInterface::hasSourceAnchorOverride() const {
    return pImpl->has_source_anchor_override_;
}

// ============================================================================
// Registration
// ============================================================================

RegistrationResult MeshGPUInterface::runRegistration(const RegistrationParams& params) {
    Transform4x4 identity;
    return runRegistration(identity, params);
}

RegistrationResult MeshGPUInterface::runRegistration(const Transform4x4& initial_transform,
                                                      const RegistrationParams& params)
{
    RegistrationResult result;

    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        if (!pImpl->backend_status_.initialized || !pImpl->ascend_api_ || !pImpl->ascend_context_) {
            std::cerr << "[MeshGPUInterface] ASCEND backend is not initialized." << std::endl;
            return result;
        }
        if (!pImpl->ascend_api_->run_registration) {
            std::cerr << "[MeshGPUInterface] ASCEND run_registration entry is missing." << std::endl;
            return result;
        }

        const char* plugin_message = nullptr;
        AscendRegistrationResultView view{};
        const bool ok = pImpl->ascend_api_->run_registration(
            pImpl->ascend_context_,
            &initial_transform,
            &params,
            &view,
            &plugin_message);

        if (!ok) {
            std::cerr << "[MeshGPUInterface] ASCEND runRegistration failed: "
                      << (plugin_message ? plugin_message : "unknown error") << std::endl;
            return result;
        }

        result.transform = view.transform;
        result.rmse = view.rmse;
        result.iterations = view.iterations;
        result.converged = (view.converged != 0);
        if (view.rmse_history && view.rmse_history_count > 0) {
            result.rmse_history.assign(view.rmse_history, view.rmse_history + view.rmse_history_count);
        }

        pImpl->last_result_ = result;
        std::cout << "[MeshGPUInterface] ASCEND registration completed: "
                  << result.iterations << " iterations, RMSE=" << result.rmse
                  << ", converged=" << (result.converged ? "yes" : "no")
                  << (plugin_message ? std::string(", ") + plugin_message : std::string())
                  << std::endl;
        return result;
    }

    if (!pImpl->ensureCudaBackend("runRegistration")) {
        return result;
    }

    if (!pImpl->mesh_) {
        std::cerr << "No target mesh loaded" << std::endl;
        return result;
    }

    // Check if source cloud is available (either from setSourcePoints or generateSimulatedProbe*)
    // source_cloud_ can be set directly by generateSimulatedProbe3D() without filling source_points_
    if (!pImpl->source_cloud_ || (pImpl->source_cloud_->num_points == 0 && pImpl->source_points_.empty())) {
        std::cerr << "No source points set" << std::endl;
        return result;
    }

    try {
        // Create GICP registration
        if (pImpl->gicp_) {
            delete pImpl->gicp_;
        }
        pImpl->gicp_ = new GICPRegistration();

        if (!pImpl->gicp_->initialize(pImpl->mesh_, pImpl->source_cloud_)) {
            std::cerr << "Failed to initialize GICP" << std::endl;
            return result;
        }

        // Setup GICP parameters
        GICPParams gicp_params;
        gicp_params.max_iterations = params.max_iterations;
        gicp_params.convergence_threshold = params.convergence_threshold;
        gicp_params.distance_threshold = params.distance_threshold;
        gicp_params.search_radius = params.search_radius;
        gicp_params.use_point_to_plane = params.use_point_to_plane;
        gicp_params.verbose = params.verbose;
        // ENV bypass for W1-5b A/B comparison: MEDICALPRO_USE_TENSOR_ICP=1 routes
        // the inner ICP loop to Open3D Tensor ICP. Once the comparison is done
        // this should be replaced by a typed parameter on RegistrationParams.
        if (const char* env = std::getenv("MEDICALPRO_USE_TENSOR_ICP")) {
            if (env[0] == '1') gicp_params.use_tensor_backend = true;
        }

        // Convert initial transform
        Matrix4x4 init_mat;
        for (int i = 0; i < 16; i++) {
            init_mat.m[i] = initial_transform.data[i];
        }

        // Run GICP
        GICPResult gicp_result = pImpl->gicp_->align(init_mat, gicp_params);

        // Convert result
        for (int i = 0; i < 16; i++) {
            result.transform.data[i] = gicp_result.final_transform.m[i];
        }
        result.rmse = gicp_result.final_rmse;
        result.iterations = gicp_result.iterations;
        result.converged = gicp_result.converged;
        result.rmse_history = gicp_result.rmse_history;

        pImpl->last_result_ = result;

        std::cout << "[MeshGPUInterface] Registration completed: "
                  << result.iterations << " iterations, RMSE=" << result.rmse
                  << ", converged=" << (result.converged ? "yes" : "no") << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception during registration: " << e.what() << std::endl;
    }

    return result;
}

std::vector<RegistrationResult> MeshGPUInterface::refineTransformCandidates(
    const std::vector<Transform4x4>& initial_transforms,
    const RegistrationParams& params)
{
    std::vector<RegistrationResult> results;
    results.reserve(initial_transforms.size());
    if (initial_transforms.empty()) {
        return results;
    }

    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        for (const Transform4x4& initial_transform : initial_transforms) {
            results.push_back(runRegistration(initial_transform, params));
        }
        return results;
    }

    if (!pImpl->ensureCudaBackend("refineTransformCandidates")) {
        return results;
    }

    if (!pImpl->mesh_) {
        std::cerr << "No target mesh loaded" << std::endl;
        return results;
    }

    if (!pImpl->source_cloud_ || (pImpl->source_cloud_->num_points == 0 && pImpl->source_points_.empty())) {
        std::cerr << "No source points set" << std::endl;
        return results;
    }

    try {
        if (pImpl->gicp_) {
            delete pImpl->gicp_;
        }
        pImpl->gicp_ = new GICPRegistration();

        if (!pImpl->gicp_->initialize(pImpl->mesh_, pImpl->source_cloud_)) {
            std::cerr << "Failed to initialize GICP" << std::endl;
            return results;
        }

        GICPParams gicp_params;
        gicp_params.max_iterations = params.max_iterations;
        gicp_params.convergence_threshold = params.convergence_threshold;
        gicp_params.distance_threshold = params.distance_threshold;
        gicp_params.search_radius = params.search_radius;
        gicp_params.use_point_to_plane = params.use_point_to_plane;
        gicp_params.verbose = params.verbose;
        gicp_params.curvature_weight_mode =
            static_cast<::CurvatureWeightMode>(static_cast<int>(params.curvature_weight_mode));
        gicp_params.curvature_weight_scale = params.curvature_weight_scale;
        gicp_params.min_weight = params.min_weight;
        gicp_params.max_weight = params.max_weight;
        // ENV bypass: MEDICALPRO_USE_TENSOR_ICP=1 routes to Open3D Tensor ICP.
        if (const char* env = std::getenv("MEDICALPRO_USE_TENSOR_ICP")) {
            if (env[0] == '1') gicp_params.use_tensor_backend = true;
        }

        for (const Transform4x4& initial_transform : initial_transforms) {
            Matrix4x4 init_mat;
            for (int i = 0; i < 16; i++) {
                init_mat.m[i] = initial_transform.data[i];
            }

            const GICPResult gicp_result = pImpl->gicp_->align(init_mat, gicp_params);

            RegistrationResult result;
            for (int i = 0; i < 16; i++) {
                result.transform.data[i] = gicp_result.final_transform.m[i];
            }
            result.rmse = gicp_result.final_rmse;
            result.iterations = gicp_result.iterations;
            result.converged = gicp_result.converged;
            result.rmse_history = gicp_result.rmse_history;
            results.push_back(result);
            pImpl->last_result_ = result;
        }

        std::cout << "[MeshGPUInterface] Batch refine completed: "
                  << results.size() << " transforms" << std::endl;
    } catch (const std::exception& e) {
        results.clear();
        std::cerr << "Exception during batch refine: " << e.what() << std::endl;
    }

    return results;
}

RegistrationResult MeshGPUInterface::getLastResult() const {
    return pImpl->last_result_;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::vector<Point3D> MeshGPUInterface::transformPoints(const std::vector<Point3D>& points,
                                                        const Transform4x4& transform)
{
    std::vector<Point3D> result;
    result.reserve(points.size());

    for (const auto& p : points) {
        Point3D tp;
        tp.x = transform(0, 0) * p.x + transform(0, 1) * p.y + transform(0, 2) * p.z + transform(0, 3);
        tp.y = transform(1, 0) * p.x + transform(1, 1) * p.y + transform(1, 2) * p.z + transform(1, 3);
        tp.z = transform(2, 0) * p.x + transform(2, 1) * p.y + transform(2, 2) * p.z + transform(2, 3);
        result.push_back(tp);
    }

    return result;
}

float MeshGPUInterface::computeRMSE(const std::vector<Point3D>& points1,
                                     const std::vector<Point3D>& points2)
{
    if (points1.size() != points2.size() || points1.empty()) {
        return -1.0f;
    }

    float sum_sq = 0.0f;
    for (size_t i = 0; i < points1.size(); i++) {
        float dx = points1[i].x - points2[i].x;
        float dy = points1[i].y - points2[i].y;
        float dz = points1[i].z - points2[i].z;
        sum_sq += dx * dx + dy * dy + dz * dz;
    }

    return std::sqrt(sum_sq / static_cast<float>(points1.size()));
}

// ============================================================================
// Simulation Test Functions
// ============================================================================

int MeshGPUInterface::generateSimulatedProbe(float rotation_z_deg,
                                              float translation_x, float translation_y, float translation_z,
                                              float noise_stddev)
{
    if (!pImpl->ensureCudaBackend("generateSimulatedProbe")) {
        return 0;
    }
    if (!pImpl->mesh_ || pImpl->host_mesh_.num_vertices == 0) {
        std::cerr << "[MeshGPUInterface] No mesh loaded for probe generation" << std::endl;
        return 0;
    }

    // Free old probe simulator
    if (pImpl->probe_sim_) {
        delete pImpl->probe_sim_;
    }
    pImpl->probe_sim_ = new ProbeSimulator();

    bool success = pImpl->probe_sim_->generateSimulatedProbe(
        pImpl->host_mesh_,
        rotation_z_deg,
        translation_x, translation_y, translation_z,
        noise_stddev,
        true  // include normals
    );

    if (!success) {
        delete pImpl->probe_sim_;
        pImpl->probe_sim_ = nullptr;
        return 0;
    }

    // Store ground truth transform
    const Matrix4x4& gt = pImpl->probe_sim_->getGroundTruthTransform();
    for (int i = 0; i < 16; i++) {
        pImpl->ground_truth_transform_.data[i] = gt.m[i];
    }

    // Copy probe to source cloud
    const SourcePointCloud& probe = pImpl->probe_sim_->getSourceCloud();
    pImpl->setSourceCloudReference(probe);
    pImpl->source_points_.clear();

    // Mark that we don't own this memory (it's owned by probe_sim_)
    // Actually we need to copy it properly - let's just reference it
    // The probe_sim_ owns the GPU memory, we just reference it

    return static_cast<int>(probe.num_points);
}

int MeshGPUInterface::generateSimulatedProbe3D(float rotation_x_deg, float rotation_y_deg, float rotation_z_deg,
                                                float translation_x, float translation_y, float translation_z,
                                                float noise_stddev)
{
    if (!pImpl->ensureCudaBackend("generateSimulatedProbe3D")) {
        return 0;
    }
    if (!pImpl->mesh_ || pImpl->host_mesh_.num_vertices == 0) {
        std::cerr << "[MeshGPUInterface] No mesh loaded for probe generation" << std::endl;
        return 0;
    }

    if (pImpl->probe_sim_) {
        delete pImpl->probe_sim_;
    }
    pImpl->probe_sim_ = new ProbeSimulator();

    bool success = pImpl->probe_sim_->generateSimulatedProbe3D(
        pImpl->host_mesh_,
        rotation_x_deg, rotation_y_deg, rotation_z_deg,
        translation_x, translation_y, translation_z,
        noise_stddev,
        true
    );

    if (!success) {
        delete pImpl->probe_sim_;
        pImpl->probe_sim_ = nullptr;
        return 0;
    }

    const Matrix4x4& gt = pImpl->probe_sim_->getGroundTruthTransform();
    for (int i = 0; i < 16; i++) {
        pImpl->ground_truth_transform_.data[i] = gt.m[i];
    }

    const SourcePointCloud& probe = pImpl->probe_sim_->getSourceCloud();
    pImpl->setSourceCloudReference(probe);
    pImpl->source_points_.clear();

    return static_cast<int>(probe.num_points);
}

int MeshGPUInterface::generatePartialProbe(float rotation_z_deg,
                                            float translation_x, float translation_y, float translation_z,
                                            float noise_stddev,
                                            float z_min_ratio, float z_max_ratio)
{
    if (!pImpl->ensureCudaBackend("generatePartialProbe")) {
        return 0;
    }
    if (!pImpl->mesh_ || pImpl->host_mesh_.num_vertices == 0) {
        std::cerr << "[MeshGPUInterface] No mesh loaded for probe generation" << std::endl;
        return 0;
    }

    if (pImpl->probe_sim_) {
        delete pImpl->probe_sim_;
    }
    pImpl->probe_sim_ = new ProbeSimulator();

    bool success = pImpl->probe_sim_->generatePartialProbe(
        pImpl->host_mesh_,
        rotation_z_deg,
        translation_x, translation_y, translation_z,
        noise_stddev,
        z_min_ratio, z_max_ratio,
        true
    );

    if (!success) {
        delete pImpl->probe_sim_;
        pImpl->probe_sim_ = nullptr;
        return 0;
    }

    const Matrix4x4& gt = pImpl->probe_sim_->getGroundTruthTransform();
    for (int i = 0; i < 16; i++) {
        pImpl->ground_truth_transform_.data[i] = gt.m[i];
    }

    const SourcePointCloud& probe = pImpl->probe_sim_->getSourceCloud();
    pImpl->setSourceCloudReference(probe);
    pImpl->source_points_.clear();

    return static_cast<int>(probe.num_points);
}

int MeshGPUInterface::generateClinicalProbe(float rotation_z_deg,
                                             float translation_x, float translation_y, float translation_z,
                                             float noise_stddev,
                                             float z_height_mm, float nz_threshold)
{
    if (!pImpl->ensureCudaBackend("generateClinicalProbe")) {
        return 0;
    }
    if (!pImpl->mesh_ || pImpl->host_mesh_.num_vertices == 0) {
        std::cerr << "[MeshGPUInterface] No mesh loaded for probe generation" << std::endl;
        return 0;
    }

    if (pImpl->probe_sim_) {
        delete pImpl->probe_sim_;
    }
    pImpl->probe_sim_ = new ProbeSimulator();

    bool success = pImpl->probe_sim_->generateClinicalProbe(
        pImpl->host_mesh_,
        rotation_z_deg,
        translation_x, translation_y, translation_z,
        noise_stddev,
        z_height_mm, nz_threshold,
        true,  // require_anterior_or_medial
        true   // include_normals
    );

    if (!success) {
        delete pImpl->probe_sim_;
        pImpl->probe_sim_ = nullptr;
        return 0;
    }

    const Matrix4x4& gt = pImpl->probe_sim_->getGroundTruthTransform();
    for (int i = 0; i < 16; i++) {
        pImpl->ground_truth_transform_.data[i] = gt.m[i];
    }

    const SourcePointCloud& probe = pImpl->probe_sim_->getSourceCloud();
    pImpl->setSourceCloudReference(probe);
    pImpl->source_points_.clear();

    return static_cast<int>(probe.num_points);
}

Transform4x4 MeshGPUInterface::getGroundTruthTransform() const {
    return pImpl->ground_truth_transform_;
}

std::pair<float, float> MeshGPUInterface::computeRegistrationError(const Transform4x4& estimated_transform) const {
    // Compute T_error = T_estimated * T_gt^(-1)
    // If perfect, T_error = I

    const Transform4x4& gt = pImpl->ground_truth_transform_;

    // For translation error: ||t_estimated - t_gt||
    // Note: The estimated transform aligns source->target,
    // and gt transform is target->source
    // So the combined transform should be close to identity

    // Simplified: compute translation difference from the result
    float tx_est = estimated_transform.data[3];
    float ty_est = estimated_transform.data[7];
    float tz_est = estimated_transform.data[11];

    float tx_gt = gt.data[3];
    float ty_gt = gt.data[7];
    float tz_gt = gt.data[11];

    // The estimated transform T_est should satisfy: T_est * T_gt ≈ I
    // So T_est ≈ T_gt^(-1)
    // Translation error = ||t_est + R_est * t_gt||

    // For simplicity, compute the residual translation in the error matrix
    // T_error = T_est * T_gt
    float error_tx = estimated_transform.data[0] * tx_gt + estimated_transform.data[1] * ty_gt +
                     estimated_transform.data[2] * tz_gt + tx_est;
    float error_ty = estimated_transform.data[4] * tx_gt + estimated_transform.data[5] * ty_gt +
                     estimated_transform.data[6] * tz_gt + ty_est;
    float error_tz = estimated_transform.data[8] * tx_gt + estimated_transform.data[9] * ty_gt +
                     estimated_transform.data[10] * tz_gt + tz_est;

    float trans_error = std::sqrt(error_tx * error_tx + error_ty * error_ty + error_tz * error_tz);

    // Rotation error: compute angle from R_error = R_est * R_gt
    // angle = arccos((trace(R_error) - 1) / 2)
    float r00 = estimated_transform.data[0] * gt.data[0] + estimated_transform.data[1] * gt.data[4] + estimated_transform.data[2] * gt.data[8];
    float r11 = estimated_transform.data[4] * gt.data[1] + estimated_transform.data[5] * gt.data[5] + estimated_transform.data[6] * gt.data[9];
    float r22 = estimated_transform.data[8] * gt.data[2] + estimated_transform.data[9] * gt.data[6] + estimated_transform.data[10] * gt.data[10];

    float trace = r00 + r11 + r22;
    float cos_angle = (trace - 1.0f) / 2.0f;
    cos_angle = std::max(-1.0f, std::min(1.0f, cos_angle));  // Clamp to [-1, 1]
    float rot_error_rad = std::acos(cos_angle);
    float rot_error_deg = rot_error_rad * 180.0f / 3.14159265358979f;

    return {trans_error, rot_error_deg};
}

MeshGPUInterface::SimTestResult MeshGPUInterface::runSimulationTest(
    float rotation_z_deg,
    float translation_x, float translation_y, float translation_z,
    float noise_stddev,
    const RegistrationParams& params)
{
    SimTestResult result = {};

    // Generate probe
    auto start_time = std::chrono::high_resolution_clock::now();

    int num_points = generateSimulatedProbe(rotation_z_deg, translation_x, translation_y, translation_z, noise_stddev);
    if (num_points == 0) {
        std::cerr << "[SimTest] Failed to generate probe" << std::endl;
        return result;
    }
    result.source_points = num_points;

    std::cout << "[SimTest] Generated " << num_points << " probe points" << std::endl;
    std::cout << "[SimTest] Running GICP registration..." << std::endl;

    // Run registration
    RegistrationResult reg_result = runRegistration(params);

    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();

    // Compute errors
    auto errors = computeRegistrationError(reg_result.transform);

    result.rmse = reg_result.rmse;
    result.iterations = reg_result.iterations;
    result.converged = reg_result.converged;
    result.translation_error = errors.first;
    result.rotation_error = errors.second;
    result.success = (result.rmse < 2.0f && result.converged);

    std::cout << "[SimTest] Result: RMSE=" << result.rmse << "mm, "
              << "TransErr=" << result.translation_error << "mm, "
              << "RotErr=" << result.rotation_error << "deg, "
              << "Time=" << result.time_ms << "ms" << std::endl;

    return result;
}

MeshGPUInterface::SimTestResult MeshGPUInterface::runPartialOverlapTest(
    float rotation_z_deg,
    float translation_x, float translation_y, float translation_z,
    float noise_stddev,
    float overlap_ratio,
    const RegistrationParams& params)
{
    SimTestResult result = {};

    auto start_time = std::chrono::high_resolution_clock::now();

    // Generate partial probe (bottom portion only)
    int num_points = generatePartialProbe(rotation_z_deg, translation_x, translation_y, translation_z,
                                          noise_stddev, 0.0f, overlap_ratio);
    if (num_points == 0) {
        std::cerr << "[SimTest] Failed to generate partial probe" << std::endl;
        return result;
    }
    result.source_points = num_points;

    std::cout << "[SimTest] Generated " << num_points << " partial probe points ("
              << (overlap_ratio * 100) << "% overlap)" << std::endl;

    // Run registration
    RegistrationResult reg_result = runRegistration(params);

    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();

    auto errors = computeRegistrationError(reg_result.transform);

    result.rmse = reg_result.rmse;
    result.iterations = reg_result.iterations;
    result.converged = reg_result.converged;
    result.translation_error = errors.first;
    result.rotation_error = errors.second;
    result.success = (result.rmse < 3.0f && result.converged);  // More lenient for partial overlap

    return result;
}

bool MeshGPUInterface::getHostMesh(std::vector<Point3D>& vertices, std::vector<Normal3D>& normals) const {
    if (pImpl->host_mesh_.num_vertices == 0) {
        return false;
    }

    vertices.resize(pImpl->host_mesh_.num_vertices);
    normals.resize(pImpl->host_mesh_.num_vertices);

    for (uint32_t i = 0; i < pImpl->host_mesh_.num_vertices; i++) {
        vertices[i].x = pImpl->host_mesh_.vertices_x[i];
        vertices[i].y = pImpl->host_mesh_.vertices_y[i];
        vertices[i].z = pImpl->host_mesh_.vertices_z[i];

        if (pImpl->host_mesh_.normals_x) {
            normals[i].nx = pImpl->host_mesh_.normals_x[i];
            normals[i].ny = pImpl->host_mesh_.normals_y[i];
            normals[i].nz = pImpl->host_mesh_.normals_z[i];
        }
    }

    return true;
}

ConstrainedMeshResult MeshGPUInterface::buildConstrainedTargetMesh(
    const Point3D& target_region_center,
    float target_region_radius_mm,
    float membership_radius_mm,
    const std::vector<Point3D>& constraint_points,
    int minimum_point_count) const
{
    ConstrainedMeshResult result;
    if (!pImpl->ensureCudaBackend("buildConstrainedTargetMesh")) {
        return result;
    }
    if (!pImpl->mesh_ || pImpl->host_mesh_.num_vertices == 0 || pImpl->host_mesh_.num_faces == 0) {
        return result;
    }

    MeshSoA* device_mesh = pImpl->mesh_->getDeviceMesh();
    if (!device_mesh) {
        return result;
    }

    unsigned char* d_vertex_mask = nullptr;
    unsigned char* d_face_mask = nullptr;
    uint32_t* d_vertex_counts = nullptr;
    uint32_t* d_vertex_offsets = nullptr;
    uint32_t* d_face_counts = nullptr;
    uint32_t* d_face_offsets = nullptr;
    int* d_vertex_mapping = nullptr;
    int* d_selected_vertex_indices = nullptr;
    int* d_selected_face_v0 = nullptr;
    int* d_selected_face_v1 = nullptr;
    int* d_selected_face_v2 = nullptr;
    float* d_constraint_x = nullptr;
    float* d_constraint_y = nullptr;
    float* d_constraint_z = nullptr;

    const auto cleanup = [&]() {
        if (d_vertex_mask) {
            cudaFree(d_vertex_mask);
        }
        if (d_face_mask) {
            cudaFree(d_face_mask);
        }
        if (d_vertex_counts) {
            cudaFree(d_vertex_counts);
        }
        if (d_vertex_offsets) {
            cudaFree(d_vertex_offsets);
        }
        if (d_face_counts) {
            cudaFree(d_face_counts);
        }
        if (d_face_offsets) {
            cudaFree(d_face_offsets);
        }
        if (d_vertex_mapping) {
            cudaFree(d_vertex_mapping);
        }
        if (d_selected_vertex_indices) {
            cudaFree(d_selected_vertex_indices);
        }
        if (d_selected_face_v0) {
            cudaFree(d_selected_face_v0);
        }
        if (d_selected_face_v1) {
            cudaFree(d_selected_face_v1);
        }
        if (d_selected_face_v2) {
            cudaFree(d_selected_face_v2);
        }
        if (d_constraint_x) {
            cudaFree(d_constraint_x);
        }
        if (d_constraint_y) {
            cudaFree(d_constraint_y);
        }
        if (d_constraint_z) {
            cudaFree(d_constraint_z);
        }
    };
    const auto failIfCudaError = [&](cudaError_t error) {
        if (error == cudaSuccess) {
            return false;
        }
        cleanup();
        return true;
    };

    const uint32_t num_vertices = pImpl->host_mesh_.num_vertices;
    const uint32_t num_faces = pImpl->host_mesh_.num_faces;
    std::vector<unsigned char> h_vertex_mask(num_vertices, 0);

    if (failIfCudaError(cudaMalloc(&d_vertex_mask, num_vertices * sizeof(unsigned char)))
        || failIfCudaError(cudaMalloc(&d_face_mask, num_faces * sizeof(unsigned char)))
        || failIfCudaError(cudaMalloc(&d_vertex_counts, num_vertices * sizeof(uint32_t)))
        || failIfCudaError(cudaMalloc(&d_vertex_offsets, num_vertices * sizeof(uint32_t)))
        || failIfCudaError(cudaMalloc(&d_face_counts, num_faces * sizeof(uint32_t)))
        || failIfCudaError(cudaMalloc(&d_face_offsets, num_faces * sizeof(uint32_t)))
        || failIfCudaError(cudaMalloc(&d_vertex_mapping, num_vertices * sizeof(int)))
        || failIfCudaError(cudaMemset(d_vertex_mask, 0, num_vertices * sizeof(unsigned char)))
        || failIfCudaError(cudaMemset(d_face_mask, 0, num_faces * sizeof(unsigned char)))
        || failIfCudaError(cudaMemset(d_vertex_mapping, 0xFF, num_vertices * sizeof(int)))) {
        return result;
    }

    std::vector<float> constraint_x(constraint_points.size(), 0.0f);
    std::vector<float> constraint_y(constraint_points.size(), 0.0f);
    std::vector<float> constraint_z(constraint_points.size(), 0.0f);
    for (size_t index = 0; index < constraint_points.size(); ++index) {
        constraint_x[index] = constraint_points[index].x;
        constraint_y[index] = constraint_points[index].y;
        constraint_z[index] = constraint_points[index].z;
    }

    if (!constraint_points.empty()) {
        if (failIfCudaError(cudaMalloc(&d_constraint_x, constraint_points.size() * sizeof(float)))
            || failIfCudaError(cudaMalloc(&d_constraint_y, constraint_points.size() * sizeof(float)))
            || failIfCudaError(cudaMalloc(&d_constraint_z, constraint_points.size() * sizeof(float)))
            || failIfCudaError(cudaMemcpy(
                d_constraint_x,
                constraint_x.data(),
                constraint_points.size() * sizeof(float),
                cudaMemcpyHostToDevice))
            || failIfCudaError(cudaMemcpy(
                d_constraint_y,
                constraint_y.data(),
                constraint_points.size() * sizeof(float),
                cudaMemcpyHostToDevice))
            || failIfCudaError(cudaMemcpy(
                d_constraint_z,
                constraint_z.data(),
                constraint_points.size() * sizeof(float),
                cudaMemcpyHostToDevice))) {
            return result;
        }
    }

    launchSelectVerticesByConstraints(
        device_mesh->vertices_x,
        device_mesh->vertices_y,
        device_mesh->vertices_z,
        d_vertex_mask,
        target_region_center.x,
        target_region_center.y,
        target_region_center.z,
        target_region_radius_mm > 0.0f ? target_region_radius_mm * target_region_radius_mm : -1.0f,
        d_constraint_x,
        d_constraint_y,
        d_constraint_z,
        static_cast<uint32_t>(constraint_points.size()),
        membership_radius_mm > 0.0f ? membership_radius_mm * membership_radius_mm : -1.0f,
        num_vertices);

    if (failIfCudaError(cudaMemcpy(
            h_vertex_mask.data(),
            d_vertex_mask,
            num_vertices * sizeof(unsigned char),
            cudaMemcpyDeviceToHost))) {
        return result;
    }

    std::vector<int> selected_indices;
    selected_indices.reserve(num_vertices);
    std::vector<std::pair<float, int>> ranked_indices;
    ranked_indices.reserve(num_vertices);
    for (uint32_t index = 0; index < num_vertices; ++index) {
        const float dx = pImpl->host_mesh_.vertices_x[index] - target_region_center.x;
        const float dy = pImpl->host_mesh_.vertices_y[index] - target_region_center.y;
        const float dz = pImpl->host_mesh_.vertices_z[index] - target_region_center.z;
        ranked_indices.emplace_back(dx * dx + dy * dy + dz * dz, static_cast<int>(index));
        if (h_vertex_mask[index]) {
            selected_indices.push_back(static_cast<int>(index));
        }
    }

    if (static_cast<int>(selected_indices.size()) < minimum_point_count) {
        std::sort(ranked_indices.begin(), ranked_indices.end(), [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
        for (const auto& ranked_entry : ranked_indices) {
            const int candidate_index = ranked_entry.second;
            if (!h_vertex_mask[static_cast<size_t>(candidate_index)]) {
                h_vertex_mask[static_cast<size_t>(candidate_index)] = 1;
                selected_indices.push_back(candidate_index);
            }
            if (static_cast<int>(selected_indices.size()) >= minimum_point_count) {
                break;
            }
        }
    }

    if (static_cast<int>(selected_indices.size()) < minimum_point_count) {
        cleanup();
        return result;
    }

    if (failIfCudaError(cudaMemcpy(
            d_vertex_mask,
            h_vertex_mask.data(),
            num_vertices * sizeof(unsigned char),
            cudaMemcpyHostToDevice))) {
        return result;
    }

    launchMaskToCounts(d_vertex_mask, d_vertex_counts, num_vertices);
    thrust::exclusive_scan(
        thrust::device_pointer_cast(d_vertex_counts),
        thrust::device_pointer_cast(d_vertex_counts + num_vertices),
        thrust::device_pointer_cast(d_vertex_offsets));

    uint32_t last_vertex_offset = 0;
    uint32_t last_vertex_count = 0;
    if (failIfCudaError(cudaMemcpy(
            &last_vertex_offset,
            d_vertex_offsets + (num_vertices - 1),
            sizeof(uint32_t),
            cudaMemcpyDeviceToHost))
        || failIfCudaError(cudaMemcpy(
            &last_vertex_count,
            d_vertex_counts + (num_vertices - 1),
            sizeof(uint32_t),
            cudaMemcpyDeviceToHost))) {
        return result;
    }

    const uint32_t selected_vertex_count = last_vertex_offset + last_vertex_count;
    if (selected_vertex_count == 0) {
        cleanup();
        return result;
    }

    if (failIfCudaError(cudaMalloc(&d_selected_vertex_indices, selected_vertex_count * sizeof(int)))) {
        return result;
    }

    launchScatterSelectedVertexIndices(
        d_vertex_mask,
        d_vertex_offsets,
        d_vertex_mapping,
        d_selected_vertex_indices,
        num_vertices);

    launchSelectFacesByVertexMask(
        device_mesh->faces_v0,
        device_mesh->faces_v1,
        device_mesh->faces_v2,
        d_vertex_mask,
        d_face_mask,
        num_faces);

    launchMaskToCounts(d_face_mask, d_face_counts, num_faces);
    thrust::exclusive_scan(
        thrust::device_pointer_cast(d_face_counts),
        thrust::device_pointer_cast(d_face_counts + num_faces),
        thrust::device_pointer_cast(d_face_offsets));

    uint32_t last_face_offset = 0;
    uint32_t last_face_count = 0;
    if (failIfCudaError(cudaMemcpy(
            &last_face_offset,
            d_face_offsets + (num_faces - 1),
            sizeof(uint32_t),
            cudaMemcpyDeviceToHost))
        || failIfCudaError(cudaMemcpy(
            &last_face_count,
            d_face_counts + (num_faces - 1),
            sizeof(uint32_t),
            cudaMemcpyDeviceToHost))) {
        return result;
    }

    const uint32_t selected_face_count = last_face_offset + last_face_count;
    if (selected_face_count == 0) {
        cleanup();
        return result;
    }

    if (failIfCudaError(cudaMalloc(&d_selected_face_v0, selected_face_count * sizeof(int)))
        || failIfCudaError(cudaMalloc(&d_selected_face_v1, selected_face_count * sizeof(int)))
        || failIfCudaError(cudaMalloc(&d_selected_face_v2, selected_face_count * sizeof(int)))) {
        return result;
    }

    launchScatterSelectedFaces(
        d_face_mask,
        d_face_offsets,
        device_mesh->faces_v0,
        device_mesh->faces_v1,
        device_mesh->faces_v2,
        d_vertex_mapping,
        d_selected_face_v0,
        d_selected_face_v1,
        d_selected_face_v2,
        num_faces);

    std::vector<int> h_selected_vertex_indices(selected_vertex_count, -1);
    std::vector<int> h_selected_face_v0(selected_face_count, -1);
    std::vector<int> h_selected_face_v1(selected_face_count, -1);
    std::vector<int> h_selected_face_v2(selected_face_count, -1);
    if (failIfCudaError(cudaMemcpy(
            h_selected_vertex_indices.data(),
            d_selected_vertex_indices,
            selected_vertex_count * sizeof(int),
            cudaMemcpyDeviceToHost))
        || failIfCudaError(cudaMemcpy(
            h_selected_face_v0.data(),
            d_selected_face_v0,
            selected_face_count * sizeof(int),
            cudaMemcpyDeviceToHost))
        || failIfCudaError(cudaMemcpy(
            h_selected_face_v1.data(),
            d_selected_face_v1,
            selected_face_count * sizeof(int),
            cudaMemcpyDeviceToHost))
        || failIfCudaError(cudaMemcpy(
            h_selected_face_v2.data(),
            d_selected_face_v2,
            selected_face_count * sizeof(int),
            cudaMemcpyDeviceToHost))) {
        return result;
    }

    result.vertices.reserve(selected_vertex_count);
    result.normals.reserve(selected_vertex_count);
    result.original_vertex_indices.reserve(selected_vertex_count);
    for (uint32_t compact_index = 0; compact_index < selected_vertex_count; ++compact_index) {
        const int original_index = h_selected_vertex_indices[compact_index];
        if (original_index < 0 || original_index >= static_cast<int>(num_vertices)) {
            cleanup();
            return mesh_gpu::ConstrainedMeshResult {};
        }

        result.vertices.emplace_back(
            pImpl->host_mesh_.vertices_x[static_cast<uint32_t>(original_index)],
            pImpl->host_mesh_.vertices_y[static_cast<uint32_t>(original_index)],
            pImpl->host_mesh_.vertices_z[static_cast<uint32_t>(original_index)]);
        result.normals.emplace_back(
            pImpl->host_mesh_.normals_x ? pImpl->host_mesh_.normals_x[static_cast<uint32_t>(original_index)] : 0.0f,
            pImpl->host_mesh_.normals_y ? pImpl->host_mesh_.normals_y[static_cast<uint32_t>(original_index)] : 0.0f,
            pImpl->host_mesh_.normals_z ? pImpl->host_mesh_.normals_z[static_cast<uint32_t>(original_index)] : 1.0f);
        result.original_vertex_indices.push_back(original_index);
    }

    result.triangles.reserve(selected_face_count);
    for (uint32_t compact_index = 0; compact_index < selected_face_count; ++compact_index) {
        const int mapped_v0 = h_selected_face_v0[compact_index];
        const int mapped_v1 = h_selected_face_v1[compact_index];
        const int mapped_v2 = h_selected_face_v2[compact_index];
        if (mapped_v0 < 0
            || mapped_v1 < 0
            || mapped_v2 < 0
            || mapped_v0 >= static_cast<int>(selected_vertex_count)
            || mapped_v1 >= static_cast<int>(selected_vertex_count)
            || mapped_v2 >= static_cast<int>(selected_vertex_count)) {
            cleanup();
            return mesh_gpu::ConstrainedMeshResult {};
        }

        result.triangles.push_back({ mapped_v0, mapped_v1, mapped_v2 });
    }

    result.success =
        static_cast<int>(result.vertices.size()) >= minimum_point_count && !result.triangles.empty();
    cleanup();
    return result;
}

bool MeshGPUInterface::getSourcePointCloud(std::vector<Point3D>& points) const {
    // Check if probe simulator has generated points
    if (!pImpl->probe_sim_ || !pImpl->probe_sim_->isInitialized()) {
        // Fall back to source_points_ if available
        if (!pImpl->source_points_.empty()) {
            points = pImpl->source_points_;
            return true;
        }
        return false;
    }

    uint32_t num_points = pImpl->probe_sim_->getNumPoints();
    if (num_points == 0) {
        return false;
    }
    if (!pImpl->ensureCudaBackend("getSourcePointCloud")) {
        return false;
    }

    // Allocate host arrays
    std::vector<float> h_x(num_points);
    std::vector<float> h_y(num_points);
    std::vector<float> h_z(num_points);

    // Download from GPU using probe simulator's downloadToHost
    if (!pImpl->probe_sim_->downloadToHost(h_x.data(), h_y.data(), h_z.data(), num_points)) {
        return false;
    }

    // Convert to Point3D vector
    points.resize(num_points);
    for (uint32_t i = 0; i < num_points; i++) {
        points[i].x = h_x[i];
        points[i].y = h_y[i];
        points[i].z = h_z[i];
    }

    return true;
}

// ============================================================================
// Rotation Search Methods
// ============================================================================

// Helper to convert internal SearchResult to public RotationSearchResult
static RotationSearchResult convertSearchResult(const RotationSearch::SearchResult& internal) {
    RotationSearchResult result;
    for (int i = 0; i < 16; i++) {
        result.best_rotation.data[i] = internal.best_rotation.m[i];
    }
    result.best_angle_z = internal.best_angle_z;
    result.best_angle_x = internal.best_angle_x;
    result.best_angle_y = internal.best_angle_y;
    result.best_score = internal.best_score;
    result.total_points = internal.total_points;
    result.mean_dist_mm = internal.mean_dist_mm;
    result.success = (internal.mean_dist_mm < 10.0f);  // Success if mean dist < 10mm
    return result;
}

// Helper to convert public params to internal params
static RotationSearch::SearchParams convertSearchParams(const RotationSearchParams& params) {
    RotationSearch::SearchParams internal;
    internal.z_angle_min = params.z_angle_min;
    internal.z_angle_max = params.z_angle_max;
    internal.z_angle_step = params.z_angle_step;
    internal.xy_angle_range = params.xy_angle_range;
    internal.xy_angle_step = params.xy_angle_step;
    internal.enable_xy_search = params.enable_xy_search;
    return internal;
}

// Helper to get TARGET anchor point (original Zmin on target mesh)
static float3_t getTargetAnchorPoint(const ProbeSimulator* probe_sim, const MeshSoA& host_mesh) {
    // If ProbeSimulator has anchor point, use original_anchor
    if (probe_sim && probe_sim->hasAnchorPoint()) {
        return probe_sim->getOriginalAnchorPoint();
    }

    // Fallback: find Zmin point in target mesh
    float3_t anchor = {0, 0, 0};
    if (host_mesh.num_vertices == 0) return anchor;

    uint32_t min_z_idx = 0;
    float min_z = host_mesh.vertices_z[0];
    for (uint32_t i = 1; i < host_mesh.num_vertices; i++) {
        if (host_mesh.vertices_z[i] < min_z) {
            min_z = host_mesh.vertices_z[i];
            min_z_idx = i;
        }
    }

    anchor.x = host_mesh.vertices_x[min_z_idx];
    anchor.y = host_mesh.vertices_y[min_z_idx];
    anchor.z = host_mesh.vertices_z[min_z_idx];

    std::cout << "[MeshGPUInterface] Target anchor (Zmin): ("
              << anchor.x << ", " << anchor.y << ", " << anchor.z << ")" << std::endl;

    return anchor;
}

// Helper to get SOURCE anchor point (Zmin in source cloud, after transformation)
static float3_t getSourceAnchorPoint(const ProbeSimulator* probe_sim, const SourcePointCloud* cloud,
                                     bool has_override = false, const Point3D* override_anchor = nullptr) {
    // Priority 1: Explicit override from external caller (live mode anchor collection)
    if (has_override && override_anchor) {
        float3_t anchor;
        anchor.x = override_anchor->x;
        anchor.y = override_anchor->y;
        anchor.z = override_anchor->z;
        std::cout << "[MeshGPUInterface] Source anchor (explicit override): ("
                  << anchor.x << ", " << anchor.y << ", " << anchor.z << ")" << std::endl;
        return anchor;
    }

    // Priority 2: ProbeSimulator anchor (simulation mode)
    if (probe_sim && probe_sim->hasAnchorPoint()) {
        return probe_sim->getTransformedAnchorPoint();
    }

    // Priority 3: Fallback - find Zmin point in source cloud
    float3_t anchor = {0, 0, 0};
    if (!cloud || cloud->num_points == 0) return anchor;

    // Download points to find Zmin
    std::vector<float> h_x(cloud->num_points);
    std::vector<float> h_y(cloud->num_points);
    std::vector<float> h_z(cloud->num_points);

    cudaMemcpy(h_x.data(), cloud->points_x, cloud->num_points * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_y.data(), cloud->points_y, cloud->num_points * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_z.data(), cloud->points_z, cloud->num_points * sizeof(float), cudaMemcpyDeviceToHost);

    uint32_t min_z_idx = 0;
    float min_z = h_z[0];
    for (uint32_t i = 1; i < cloud->num_points; i++) {
        if (h_z[i] < min_z) {
            min_z = h_z[i];
            min_z_idx = i;
        }
    }

    anchor.x = h_x[min_z_idx];
    anchor.y = h_y[min_z_idx];
    anchor.z = h_z[min_z_idx];

    std::cout << "[MeshGPUInterface] Source anchor (Zmin fallback): ("
              << anchor.x << ", " << anchor.y << ", " << anchor.z << ")" << std::endl;

    return anchor;
}

// Helper to compute coarse translation (anchor alignment)
static Matrix4x4 computeAnchorTranslation(const float3_t& anchor_target, const float3_t& anchor_source) {
    float tx = anchor_target.x - anchor_source.x;
    float ty = anchor_target.y - anchor_source.y;
    float tz = anchor_target.z - anchor_source.z;

    std::cout << "[MeshGPUInterface] Coarse translation (anchor alignment): ("
              << tx << ", " << ty << ", " << tz << ")" << std::endl;

    return Matrix4x4::translation(tx, ty, tz);
}

// Helper to apply translation to source points on GPU
static bool applyTranslationToSource(
    const SourcePointCloud* source,
    const Matrix4x4& T,
    float*& d_trans_x, float*& d_trans_y, float*& d_trans_z)
{
    if (!source || source->num_points == 0) return false;

    uint32_t n = source->num_points;

    // Download original points
    std::vector<float> h_x(n), h_y(n), h_z(n);
    cudaMemcpy(h_x.data(), source->points_x, n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_y.data(), source->points_y, n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_z.data(), source->points_z, n * sizeof(float), cudaMemcpyDeviceToHost);

    // Apply translation
    for (uint32_t i = 0; i < n; i++) {
        float3_t p(h_x[i], h_y[i], h_z[i]);
        float3_t tp = T.transformPoint(p);
        h_x[i] = tp.x;
        h_y[i] = tp.y;
        h_z[i] = tp.z;
    }

    // Upload to GPU
    cudaMalloc(&d_trans_x, n * sizeof(float));
    cudaMalloc(&d_trans_y, n * sizeof(float));
    cudaMalloc(&d_trans_z, n * sizeof(float));
    cudaMemcpy(d_trans_x, h_x.data(), n * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_trans_y, h_y.data(), n * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_trans_z, h_z.data(), n * sizeof(float), cudaMemcpyHostToDevice);

    return true;
}

RotationSearchResult MeshGPUInterface::runRotationSearch(const RotationSearchParams& params) {
    RotationSearchResult result;
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        std::cerr << "[MeshGPUInterface] runRotationSearch is CUDA-only in current release. "
                  << "ASCEND plugin path currently supports registration entry points only." << std::endl;
        return result;
    }
    if (!pImpl->ensureCudaBackend("runRotationSearch")) {
        return result;
    }

    if (!pImpl->mesh_) {
        std::cerr << "[MeshGPUInterface] No target mesh loaded for rotation search" << std::endl;
        return result;
    }

    if (!pImpl->source_cloud_ || pImpl->source_cloud_->num_points == 0) {
        std::cerr << "[MeshGPUInterface] No source points for rotation search" << std::endl;
        return result;
    }

    try {
        RotationSearch rotSearch(*pImpl->mesh_);

        // Use Zmin point as anchor (from ProbeSimulator if available)
        float3_t anchor = getSourceAnchorPoint(pImpl->probe_sim_, pImpl->source_cloud_);

        auto internal_params = convertSearchParams(params);
        auto internal_result = rotSearch.search(
            pImpl->source_cloud_->points_x,
            pImpl->source_cloud_->points_y,
            pImpl->source_cloud_->points_z,
            pImpl->source_cloud_->num_points,
            anchor,
            internal_params
        );

        result = convertSearchResult(internal_result);

        std::cout << "[MeshGPUInterface] Rotation search: best_angle_z=" << result.best_angle_z
                  << "deg, mean_dist=" << result.mean_dist_mm << "mm" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[MeshGPUInterface] Rotation search error: " << e.what() << std::endl;
    }

    return result;
}

RotationSearchResult MeshGPUInterface::runHierarchicalRotationSearch(const RotationSearchParams& params) {
    RotationSearchResult result;
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        std::cerr << "[MeshGPUInterface] runHierarchicalRotationSearch is CUDA-only in current release. "
                  << "ASCEND plugin path currently supports registration entry points only." << std::endl;
        return result;
    }
    if (!pImpl->ensureCudaBackend("runHierarchicalRotationSearch")) {
        return result;
    }

    if (!pImpl->mesh_) {
        std::cerr << "[MeshGPUInterface] No target mesh loaded for rotation search" << std::endl;
        return result;
    }

    if (!pImpl->source_cloud_ || pImpl->source_cloud_->num_points == 0) {
        std::cerr << "[MeshGPUInterface] No source points for rotation search" << std::endl;
        return result;
    }

    try {
        RotationSearch rotSearch(*pImpl->mesh_);

        // Use Zmin point as anchor (from ProbeSimulator if available)
        float3_t anchor = getSourceAnchorPoint(pImpl->probe_sim_, pImpl->source_cloud_);

        auto internal_params = convertSearchParams(params);
        auto internal_result = rotSearch.searchHierarchical(
            pImpl->source_cloud_->points_x,
            pImpl->source_cloud_->points_y,
            pImpl->source_cloud_->points_z,
            pImpl->source_cloud_->num_points,
            anchor,
            internal_params
        );

        result = convertSearchResult(internal_result);

        std::cout << "[MeshGPUInterface] Hierarchical rotation search: best_angle_z=" << result.best_angle_z
                  << "deg, mean_dist=" << result.mean_dist_mm << "mm" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[MeshGPUInterface] Hierarchical rotation search error: " << e.what() << std::endl;
    }

    return result;
}

RotationSearchResult MeshGPUInterface::runFullSphereSearch(float angle_step_deg) {
    RotationSearchResult result;
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        std::cerr << "[MeshGPUInterface] runFullSphereSearch is CUDA-only in current release. "
                  << "ASCEND plugin path currently supports registration entry points only." << std::endl;
        return result;
    }
    if (!pImpl->ensureCudaBackend("runFullSphereSearch")) {
        return result;
    }

    if (!pImpl->mesh_) {
        std::cerr << "[MeshGPUInterface] No target mesh loaded for full sphere search" << std::endl;
        return result;
    }

    if (!pImpl->source_cloud_ || pImpl->source_cloud_->num_points == 0) {
        std::cerr << "[MeshGPUInterface] No source points for full sphere search" << std::endl;
        return result;
    }

    try {
        RotationSearch rotSearch(*pImpl->mesh_);

        // Use Zmin point as anchor (from ProbeSimulator if available)
        float3_t anchor = getSourceAnchorPoint(pImpl->probe_sim_, pImpl->source_cloud_);

        auto internal_result = rotSearch.searchFullSphere(
            pImpl->source_cloud_->points_x,
            pImpl->source_cloud_->points_y,
            pImpl->source_cloud_->points_z,
            pImpl->source_cloud_->num_points,
            anchor,
            angle_step_deg
        );

        result = convertSearchResult(internal_result);

        std::cout << "[MeshGPUInterface] Full sphere search: best_angles=["
                  << result.best_angle_x << ", " << result.best_angle_y << ", " << result.best_angle_z
                  << "]deg, mean_dist=" << result.mean_dist_mm << "mm" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[MeshGPUInterface] Full sphere search error: " << e.what() << std::endl;
    }

    return result;
}

RotationSearchResult MeshGPUInterface::runFullSphereHierarchicalSearch(float coarse_step_deg,
                                                                        float fine_step_deg,
                                                                        float fine_range_deg) {
    RotationSearchResult result;
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        std::cerr << "[MeshGPUInterface] runFullSphereHierarchicalSearch is CUDA-only in current release. "
                  << "ASCEND plugin path currently supports registration entry points only." << std::endl;
        return result;
    }
    if (!pImpl->ensureCudaBackend("runFullSphereHierarchicalSearch")) {
        return result;
    }

    if (!pImpl->mesh_) {
        std::cerr << "[MeshGPUInterface] No target mesh loaded for full sphere hierarchical search" << std::endl;
        return result;
    }

    if (!pImpl->source_cloud_ || pImpl->source_cloud_->num_points == 0) {
        std::cerr << "[MeshGPUInterface] No source points for full sphere hierarchical search" << std::endl;
        return result;
    }

    try {
        RotationSearch rotSearch(*pImpl->mesh_);

        // Use Zmin point as anchor (from ProbeSimulator if available)
        float3_t anchor = getSourceAnchorPoint(pImpl->probe_sim_, pImpl->source_cloud_);

        // Use the built-in hierarchical full sphere search
        auto internal_result = rotSearch.searchFullSphereHierarchical(
            pImpl->source_cloud_->points_x,
            pImpl->source_cloud_->points_y,
            pImpl->source_cloud_->points_z,
            pImpl->source_cloud_->num_points,
            anchor
        );

        result = convertSearchResult(internal_result);

        std::cout << "[MeshGPUInterface] Full sphere hierarchical search: best_angles=["
                  << result.best_angle_x << ", " << result.best_angle_y << ", " << result.best_angle_z
                  << "]deg, mean_dist=" << result.mean_dist_mm << "mm" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[MeshGPUInterface] Full sphere hierarchical search error: " << e.what() << std::endl;
    }

    return result;
}

RegistrationResult MeshGPUInterface::runRegistrationWithRotationSearch(
    const RotationSearchParams& rot_params,
    const RegistrationParams& reg_params)
{
    RegistrationResult result;
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        std::cout << "[MeshGPUInterface] ASCEND backend selected: rotation-search pipeline "
                  << "falls back to direct runRegistration()." << std::endl;
        return runRegistration(reg_params);
    }
    if (!pImpl->ensureCudaBackend("runRegistrationWithRotationSearch")) {
        return result;
    }

    if (!pImpl->mesh_ || !pImpl->source_cloud_ || pImpl->source_cloud_->num_points == 0) {
        std::cerr << "[MeshGPUInterface] Missing mesh or source points" << std::endl;
        return result;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "[MeshGPUInterface] Starting Coarse-to-Fine Registration" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // ========================================
        // Step 1: Anchor-based Translation (Coarse Alignment)
        // ========================================
        std::cout << "\n[Step 1] Anchor-based translation..." << std::endl;

        float3_t anchor_target = getTargetAnchorPoint(pImpl->probe_sim_, pImpl->host_mesh_);
        float3_t anchor_source = getSourceAnchorPoint(pImpl->probe_sim_, pImpl->source_cloud_,
            pImpl->has_source_anchor_override_, &pImpl->source_anchor_override_);
        Matrix4x4 T_translation = computeAnchorTranslation(anchor_target, anchor_source);

        // Apply translation to source points for rotation search
        float* d_trans_x = nullptr;
        float* d_trans_y = nullptr;
        float* d_trans_z = nullptr;

        if (!applyTranslationToSource(pImpl->source_cloud_, T_translation, d_trans_x, d_trans_y, d_trans_z)) {
            std::cerr << "[MeshGPUInterface] Failed to apply translation" << std::endl;
            return runRegistration(reg_params);  // Fallback
        }

        // ========================================
        // Step 2: Rotation Search (on translated points)
        // ========================================
        std::cout << "\n[Step 2] Rotation search on translated points..." << std::endl;

        RotationSearch rotSearch(*pImpl->mesh_);
        auto internal_params = convertSearchParams(rot_params);

        // Use target anchor as rotation center (points are now aligned at anchor)
        auto rot_internal_result = rotSearch.searchHierarchical(
            d_trans_x, d_trans_y, d_trans_z,
            pImpl->source_cloud_->num_points,
            anchor_target,
            internal_params
        );

        // Clean up temporary GPU memory
        cudaFree(d_trans_x);
        cudaFree(d_trans_y);
        cudaFree(d_trans_z);

        RotationSearchResult rot_result = convertSearchResult(rot_internal_result);
        std::cout << "  Best rotation: Z=" << rot_result.best_angle_z << " deg" << std::endl;
        std::cout << "  Mean dist: " << rot_result.mean_dist_mm << "mm" << std::endl;

        // ========================================
        // Step 3: Combine Transforms
        // ========================================
        std::cout << "\n[Step 3] Combining transforms..." << std::endl;

        // T_combined = T_rotation * T_translation (apply translation first, then rotation)
        Matrix4x4 T_rotation = rot_internal_result.best_rotation;
        Matrix4x4 T_combined = T_rotation * T_translation;

        // Convert to Transform4x4 for GICP
        Transform4x4 initial_transform;
        for (int i = 0; i < 16; i++) {
            initial_transform.data[i] = T_combined.m[i];
        }

        // ========================================
        // Step 4: GICP Fine Registration
        // ========================================
        std::cout << "\n[Step 4] GICP fine registration..." << std::endl;

        result = runRegistration(initial_transform, reg_params);

        std::cout << "\n========================================" << std::endl;
        std::cout << "[Result] RMSE=" << result.rmse << "mm, iterations=" << result.iterations << std::endl;
        std::cout << "========================================\n" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[MeshGPUInterface] Error: " << e.what() << std::endl;
        return runRegistration(reg_params);  // Fallback to direct GICP
    }

    return result;
}

RegistrationResult MeshGPUInterface::runRegistrationWithFullSphereSearch(
    float angle_step_deg,
    const RegistrationParams& reg_params)
{
    RegistrationResult result;
    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        std::cout << "[MeshGPUInterface] ASCEND backend selected: full-sphere pipeline "
                  << "falls back to direct runRegistration()." << std::endl;
        return runRegistration(reg_params);
    }
    if (!pImpl->ensureCudaBackend("runRegistrationWithFullSphereSearch")) {
        return result;
    }

    if (!pImpl->mesh_ || !pImpl->source_cloud_ || pImpl->source_cloud_->num_points == 0) {
        std::cerr << "[MeshGPUInterface] Missing mesh or source points" << std::endl;
        return result;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "[MeshGPUInterface] Starting Full Sphere Registration" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // ========================================
        // Step 1: Anchor-based Translation (Coarse Alignment)
        // ========================================
        std::cout << "\n[Step 1] Anchor-based translation..." << std::endl;

        float3_t anchor_target = getTargetAnchorPoint(pImpl->probe_sim_, pImpl->host_mesh_);
        float3_t anchor_source = getSourceAnchorPoint(pImpl->probe_sim_, pImpl->source_cloud_,
            pImpl->has_source_anchor_override_, &pImpl->source_anchor_override_);
        Matrix4x4 T_translation = computeAnchorTranslation(anchor_target, anchor_source);

        // Apply translation to source points
        float* d_trans_x = nullptr;
        float* d_trans_y = nullptr;
        float* d_trans_z = nullptr;

        if (!applyTranslationToSource(pImpl->source_cloud_, T_translation, d_trans_x, d_trans_y, d_trans_z)) {
            std::cerr << "[MeshGPUInterface] Failed to apply translation" << std::endl;
            return runRegistration(reg_params);  // Fallback
        }

        // ========================================
        // Step 2: Full Sphere Rotation Search (on translated points)
        // ========================================
        std::cout << "\n[Step 2] Full sphere rotation search..." << std::endl;

        RotationSearch rotSearch(*pImpl->mesh_);

        // Use target anchor as rotation center
        auto rot_internal_result = rotSearch.searchFullSphereHierarchical(
            d_trans_x, d_trans_y, d_trans_z,
            pImpl->source_cloud_->num_points,
            anchor_target
        );

        // Clean up temporary GPU memory
        cudaFree(d_trans_x);
        cudaFree(d_trans_y);
        cudaFree(d_trans_z);

        RotationSearchResult rot_result = convertSearchResult(rot_internal_result);
        std::cout << "  Best rotation: X=" << rot_result.best_angle_x
                  << ", Y=" << rot_result.best_angle_y
                  << ", Z=" << rot_result.best_angle_z << " deg" << std::endl;
        std::cout << "  Mean dist: " << rot_result.mean_dist_mm << "mm" << std::endl;

        // ========================================
        // Step 3: Combine Transforms
        // ========================================
        std::cout << "\n[Step 3] Combining transforms..." << std::endl;

        Matrix4x4 T_rotation = rot_internal_result.best_rotation;
        Matrix4x4 T_combined = T_rotation * T_translation;

        Transform4x4 initial_transform;
        for (int i = 0; i < 16; i++) {
            initial_transform.data[i] = T_combined.m[i];
        }

        // ========================================
        // Step 4: GICP Fine Registration
        // ========================================
        std::cout << "\n[Step 4] GICP fine registration..." << std::endl;

        result = runRegistration(initial_transform, reg_params);

        std::cout << "\n========================================" << std::endl;
        std::cout << "[Result] RMSE=" << result.rmse << "mm, iterations=" << result.iterations << std::endl;
        std::cout << "========================================\n" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[MeshGPUInterface] Error: " << e.what() << std::endl;
        return runRegistration(reg_params);  // Fallback
    }

    return result;
}

std::vector<TransformCandidateScore> MeshGPUInterface::scoreTransformCandidates(
    const std::vector<Transform4x4>& candidates,
    float cutoff_mm)
{
    std::vector<TransformCandidateScore> results;
    if (candidates.empty()) {
        return results;
    }

    if (pImpl->backend_status_.selected == ComputeBackendKind::ASCEND) {
        std::cerr << "[MeshGPUInterface] scoreTransformCandidates is CUDA-only in current release. "
                  << "ASCEND backend currently skips GPU candidate scoring." << std::endl;
        return results;
    }
    if (!pImpl->ensureCudaBackend("scoreTransformCandidates")) {
        return results;
    }
    if (!pImpl->mesh_) {
        std::cerr << "[MeshGPUInterface] No target mesh loaded for candidate scoring" << std::endl;
        return results;
    }
    if (!pImpl->source_cloud_ || pImpl->source_cloud_->num_points == 0) {
        std::cerr << "[MeshGPUInterface] No source points for candidate scoring" << std::endl;
        return results;
    }

    float* d_matrices = nullptr;
    int* d_scores = nullptr;
    float* d_normal_scores = nullptr;
    float* d_curvature_scores = nullptr;
    const auto cleanupCandidateScoreBuffers = [&]() {
        if (d_curvature_scores) {
            cudaFree(d_curvature_scores);
            d_curvature_scores = nullptr;
        }
        if (d_normal_scores) {
            cudaFree(d_normal_scores);
            d_normal_scores = nullptr;
        }
        if (d_scores) {
            cudaFree(d_scores);
            d_scores = nullptr;
        }
        if (d_matrices) {
            cudaFree(d_matrices);
            d_matrices = nullptr;
        }
    };

    try {
        const uint32_t num_candidates = static_cast<uint32_t>(candidates.size());
        const uint32_t num_points = pImpl->source_cloud_->num_points;

        std::vector<float> h_matrices(num_candidates * 16u, 0.0f);
        for (uint32_t i = 0; i < num_candidates; ++i) {
            for (int j = 0; j < 16; ++j) {
                h_matrices[i * 16u + static_cast<uint32_t>(j)] = candidates[i].data[j];
            }
        }

        const size_t matrices_bytes = static_cast<size_t>(num_candidates) * 16u * sizeof(float);
        const size_t scores_bytes = static_cast<size_t>(num_candidates) * sizeof(int);
        const size_t geometry_scores_bytes = static_cast<size_t>(num_candidates) * sizeof(float);

        cudaError_t err = cudaMalloc(&d_matrices, matrices_bytes);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMalloc(d_matrices) failed: "
                      << cudaGetErrorString(err) << std::endl;
            return results;
        }

        err = cudaMalloc(&d_scores, scores_bytes);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMalloc(d_scores) failed: "
                      << cudaGetErrorString(err) << std::endl;
            cleanupCandidateScoreBuffers();
            return results;
        }

        err = cudaMalloc(&d_normal_scores, geometry_scores_bytes);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMalloc(d_normal_scores) failed: "
                      << cudaGetErrorString(err) << std::endl;
            cleanupCandidateScoreBuffers();
            return results;
        }

        err = cudaMalloc(&d_curvature_scores, geometry_scores_bytes);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMalloc(d_curvature_scores) failed: "
                      << cudaGetErrorString(err) << std::endl;
            cleanupCandidateScoreBuffers();
            return results;
        }

        err = cudaMemcpy(d_matrices, h_matrices.data(), matrices_bytes, cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMemcpy(candidate matrices) failed: "
                      << cudaGetErrorString(err) << std::endl;
            cleanupCandidateScoreBuffers();
            return results;
        }

        err = cudaMemset(d_scores, 0, scores_bytes);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMemset(candidate scores) failed: "
                      << cudaGetErrorString(err) << std::endl;
            cleanupCandidateScoreBuffers();
            return results;
        }

        err = cudaMemset(d_normal_scores, 0, geometry_scores_bytes);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMemset(candidate normal scores) failed: "
                      << cudaGetErrorString(err) << std::endl;
            cleanupCandidateScoreBuffers();
            return results;
        }

        err = cudaMemset(d_curvature_scores, 0, geometry_scores_bytes);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMemset(candidate curvature scores) failed: "
                      << cudaGetErrorString(err) << std::endl;
            cleanupCandidateScoreBuffers();
            return results;
        }

        float3 grid_min;
        float grid_cell_size;
        int3 grid_dims;
        const uint32_t* d_cell_counts;
        const uint32_t* d_cell_starts;
        const uint32_t* d_vertex_indices;

        if (pImpl->mesh_->hasMultiResGrid()) {
            const GridIndex& g = pImpl->mesh_->getMultiResGrid().getLevel(0);
            grid_min = make_float3(g.origin.x, g.origin.y, g.origin.z);
            grid_cell_size = g.cell_size;
            grid_dims = make_int3(g.dim_x, g.dim_y, g.dim_z);
            d_cell_counts = g.cell_vertex_count;
            d_cell_starts = g.cell_starts;
            d_vertex_indices = g.vertex_indices;
        } else {
            grid_min = pImpl->mesh_->getGridMin();
            grid_cell_size = pImpl->mesh_->getGridCellSize();
            grid_dims = pImpl->mesh_->getGridDims();
            d_cell_counts = pImpl->mesh_->getGridCellCounts();
            d_cell_starts = pImpl->mesh_->getGridCellStarts();
            d_vertex_indices = pImpl->mesh_->getGridVertexIndices();
        }

        MeshSoA* device_mesh = pImpl->mesh_->getDeviceMesh();
        launchTransformCandidateGeometryScoreKernel(
            pImpl->source_cloud_->points_x,
            pImpl->source_cloud_->points_y,
            pImpl->source_cloud_->points_z,
            num_points,
            d_matrices,
            num_candidates,
            grid_min,
            grid_cell_size,
            grid_dims,
            d_cell_counts,
            d_cell_starts,
            d_vertex_indices,
            device_mesh->vertices_x,
            device_mesh->vertices_y,
            device_mesh->vertices_z,
            device_mesh->normals_x,
            device_mesh->normals_y,
            device_mesh->normals_z,
            device_mesh->curvature,
            cutoff_mm,
            d_scores,
            d_normal_scores,
            d_curvature_scores);

        std::vector<int> h_scores(num_candidates, 0);
        err = cudaMemcpy(h_scores.data(), d_scores, scores_bytes, cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMemcpy(candidate scores <- device) failed: "
                      << cudaGetErrorString(err) << std::endl;
            cleanupCandidateScoreBuffers();
            return results;
        }

        std::vector<float> h_normal_scores(num_candidates, 0.0f);
        err = cudaMemcpy(
            h_normal_scores.data(),
            d_normal_scores,
            geometry_scores_bytes,
            cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMemcpy(candidate normal scores <- device) failed: "
                      << cudaGetErrorString(err) << std::endl;
            cleanupCandidateScoreBuffers();
            return results;
        }

        std::vector<float> h_curvature_scores(num_candidates, 0.0f);
        err = cudaMemcpy(
            h_curvature_scores.data(),
            d_curvature_scores,
            geometry_scores_bytes,
            cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) {
            std::cerr << "[MeshGPUInterface] cudaMemcpy(candidate curvature scores <- device) failed: "
                      << cudaGetErrorString(err) << std::endl;
            cleanupCandidateScoreBuffers();
            return results;
        }

        results.reserve(num_candidates);
        for (uint32_t i = 0; i < num_candidates; ++i) {
            TransformCandidateScore score;
            score.candidate_index = static_cast<int>(i);
            score.score = h_scores[i];
            const double mean_dist_sq = std::max(
                0.0,
                -static_cast<double>(score.score) /
                    (static_cast<double>(num_points) * 1000.0));
            score.mean_dist_mm = static_cast<float>(std::sqrt(mean_dist_sq));
            score.normal_consistency_score = std::max(0.0f, std::min(1.0f, h_normal_scores[i]));
            score.curvature_score = std::max(0.0f, std::min(1.0f, h_curvature_scores[i]));
            score.geometry_score_available = true;
            score.success = true;
            results.push_back(score);
        }

        std::sort(results.begin(), results.end(),
                  [](const TransformCandidateScore& a, const TransformCandidateScore& b) {
                      if (a.score != b.score) {
                          return a.score > b.score;
                      }
                      return a.candidate_index < b.candidate_index;
                  });

        cleanupCandidateScoreBuffers();

        std::cout << "[MeshGPUInterface] Candidate GPU scoring completed: "
                  << num_candidates << " transforms, cutoff=" << cutoff_mm << "mm" << std::endl;
    } catch (const std::exception& e) {
        if (d_scores) {
            cudaFree(d_scores);
        }
        if (d_normal_scores) {
            cudaFree(d_normal_scores);
        }
        if (d_curvature_scores) {
            cudaFree(d_curvature_scores);
        }
        if (d_matrices) {
            cudaFree(d_matrices);
        }
        results.clear();
        std::cerr << "[MeshGPUInterface] Candidate scoring error: " << e.what() << std::endl;
    }

    return results;
}

} // namespace mesh_gpu

namespace {

class MeshGPURuntimeApiAdapter final : public mesh_gpu::MeshGPURuntimeApi {
public:
    bool loadTargetMesh(const std::string& meshPath, float cellSize) override {
        return impl_.loadTargetMesh(meshPath, cellSize);
    }

    bool hasTargetMesh() const override {
        return impl_.hasTargetMesh();
    }

    bool setTargetMesh(const std::vector<mesh_gpu::Point3D>& vertices,
                       const std::vector<mesh_gpu::Normal3D>& normals,
                       const std::vector<std::array<int, 3>>& triangles,
                       float cellSize) override {
        return impl_.setTargetMesh(vertices, normals, triangles, cellSize);
    }

    bool setSourcePointCloud(const std::vector<mesh_gpu::Point3D>& points) override {
        return impl_.setSourcePointCloud(points);
    }

    mesh_gpu::RuntimeRegistrationResult runRegistration(
        const mesh_gpu::RegistrationParams& params) override {
        return toRuntimeResult(impl_.runRegistration(params));
    }

    mesh_gpu::RuntimeRegistrationResult runRegistrationWithRotationSearch(
        const mesh_gpu::RotationSearchParams& rotationParams,
        const mesh_gpu::RegistrationParams& params) override {
        return toRuntimeResult(impl_.runRegistrationWithRotationSearch(rotationParams, params));
    }

    std::vector<mesh_gpu::RuntimeTransformCandidateScore> scoreTransformCandidates(
        const std::vector<mesh_gpu::Transform4x4>& candidates,
        float cutoffMm) override {
        const auto scores = impl_.scoreTransformCandidates(candidates, cutoffMm);
        std::vector<mesh_gpu::RuntimeTransformCandidateScore> runtimeScores;
        runtimeScores.reserve(scores.size());

        for (const auto& score : scores) {
            mesh_gpu::RuntimeTransformCandidateScore runtimeScore;
            runtimeScore.candidateIndex = score.candidate_index;
            runtimeScore.score = score.score;
            runtimeScore.meanDistanceMm = score.mean_dist_mm;
            runtimeScore.normalConsistencyScore = score.normal_consistency_score;
            runtimeScore.curvatureScore = score.curvature_score;
            runtimeScore.geometryScoreAvailable = score.geometry_score_available;
            runtimeScore.success = score.success;
            runtimeScores.push_back(runtimeScore);
        }

        return runtimeScores;
    }

    std::vector<mesh_gpu::RuntimeRefineCandidateResult> refineTransformCandidates(
        const std::vector<mesh_gpu::RuntimeRefineCandidateRequest>& candidates,
        const mesh_gpu::RegistrationParams& params) override {
        std::vector<mesh_gpu::RuntimeRefineCandidateResult> runtimeResults;
        runtimeResults.reserve(candidates.size());

        std::vector<mesh_gpu::Transform4x4> initialTransforms;
        initialTransforms.reserve(candidates.size());
        for (const auto& candidate : candidates) {
            initialTransforms.push_back(candidate.initialTransform);
        }

        const auto results = impl_.refineTransformCandidates(initialTransforms, params);
        for (size_t resultIndex = 0; resultIndex < results.size(); ++resultIndex) {
            const auto& result = results[resultIndex];
            const auto& candidate = candidates[resultIndex];

            mesh_gpu::RuntimeRefineCandidateResult runtimeResult;
            runtimeResult.candidateIndex = candidate.candidateIndex;
            runtimeResult.transform = result.transform;
            runtimeResult.rmse = result.rmse;
            runtimeResult.iterations = result.iterations;
            runtimeResult.converged = result.converged;
            runtimeResult.success = result.converged;
            runtimeResults.push_back(runtimeResult);
        }

        return runtimeResults;
    }

    mesh_gpu::RuntimeConstraintFilterResult filterSourcePointsByConstraints(
        const std::vector<mesh_gpu::Point3D>& points,
        const mesh_gpu::Point3D& targetRegionCenter,
        float targetRegionRadiusMm,
        float membershipRadiusMm,
        const std::vector<mesh_gpu::Point3D>& constraintPoints,
        int minimumPointCount) override {
        mesh_gpu::RuntimeConstraintFilterResult result;
        if (points.empty()) {
            return result;
        }

        const float radiusSquared = targetRegionRadiusMm > 0.0f ? targetRegionRadiusMm * targetRegionRadiusMm : -1.0f;
        const float membershipSquared = membershipRadiusMm > 0.0f ? membershipRadiusMm * membershipRadiusMm : -1.0f;

        auto matchesConstraint = [&](const mesh_gpu::Point3D& point) {
            if (radiusSquared > 0.0f) {
                const float dx = point.x - targetRegionCenter.x;
                const float dy = point.y - targetRegionCenter.y;
                const float dz = point.z - targetRegionCenter.z;
                const float distanceSquared = dx * dx + dy * dy + dz * dz;
                if (distanceSquared <= radiusSquared) {
                    return true;
                }
            }

            if (constraintPoints.empty() || membershipSquared <= 0.0f) {
                return false;
            }

            for (const auto& constraintPoint : constraintPoints) {
                const float dx = point.x - constraintPoint.x;
                const float dy = point.y - constraintPoint.y;
                const float dz = point.z - constraintPoint.z;
                const float distanceSquared = dx * dx + dy * dy + dz * dz;
                if (distanceSquared <= membershipSquared) {
                    return true;
                }
            }

            return false;
        };

        std::vector<std::pair<float, int>> rankedIndices;
        rankedIndices.reserve(points.size());

        for (int index = 0; index < static_cast<int>(points.size()); ++index) {
            const auto& point = points[static_cast<size_t>(index)];
            const float dx = point.x - targetRegionCenter.x;
            const float dy = point.y - targetRegionCenter.y;
            const float dz = point.z - targetRegionCenter.z;
            rankedIndices.emplace_back(dx * dx + dy * dy + dz * dz, index);

            if (matchesConstraint(point)) {
                result.selectedIndices.push_back(index);
            }
        }

        if (static_cast<int>(result.selectedIndices.size()) < minimumPointCount) {
            std::sort(rankedIndices.begin(), rankedIndices.end(), [](const auto& left, const auto& right) {
                return left.first < right.first;
            });
            for (const auto& rankedEntry : rankedIndices) {
                if (std::find(result.selectedIndices.begin(), result.selectedIndices.end(), rankedEntry.second)
                    == result.selectedIndices.end()) {
                    result.selectedIndices.push_back(rankedEntry.second);
                }
                if (static_cast<int>(result.selectedIndices.size()) >= minimumPointCount) {
                    break;
                }
            }
        }

        result.success = static_cast<int>(result.selectedIndices.size()) >= minimumPointCount;
        return result;
    }

    mesh_gpu::RuntimeConstraintFilterResult filterTargetPointsByConstraints(
        const mesh_gpu::Point3D& targetRegionCenter,
        float targetRegionRadiusMm,
        float membershipRadiusMm,
        const std::vector<mesh_gpu::Point3D>& constraintPoints,
        int minimumPointCount) override {
        std::vector<mesh_gpu::Point3D> vertices;
        std::vector<mesh_gpu::Normal3D> normals;
        if (!impl_.getHostMesh(vertices, normals)) {
            return mesh_gpu::RuntimeConstraintFilterResult {};
        }

        return filterSourcePointsByConstraints(
            vertices,
            targetRegionCenter,
            targetRegionRadiusMm,
            membershipRadiusMm,
            constraintPoints,
            minimumPointCount);
    }

    mesh_gpu::ConstrainedMeshResult buildConstrainedTargetMesh(
        const mesh_gpu::Point3D& targetRegionCenter,
        float targetRegionRadiusMm,
        float membershipRadiusMm,
        const std::vector<mesh_gpu::Point3D>& constraintPoints,
        int minimumPointCount) override {
        return impl_.buildConstrainedTargetMesh(
            targetRegionCenter,
            targetRegionRadiusMm,
            membershipRadiusMm,
            constraintPoints,
            minimumPointCount);
    }

private:
    static mesh_gpu::RuntimeRegistrationResult toRuntimeResult(
        const mesh_gpu::RegistrationResult& result) {
        mesh_gpu::RuntimeRegistrationResult runtimeResult;
        runtimeResult.transform = result.transform;
        runtimeResult.rmse = result.rmse;
        runtimeResult.iterations = result.iterations;
        runtimeResult.converged = result.converged;
        return runtimeResult;
    }

    mesh_gpu::MeshGPUInterface impl_;
};

} // namespace

extern "C" MESHGPU_API mesh_gpu::MeshGPUInterface* CreateMeshGPUInterface()
{
    return new mesh_gpu::MeshGPUInterface();
}

extern "C" MESHGPU_API void DestroyMeshGPUInterface(mesh_gpu::MeshGPUInterface* instance)
{
    delete instance;
}

extern "C" MESHGPU_API mesh_gpu::MeshGPURuntimeApi* CreateMeshGPURuntimeApi()
{
    return new MeshGPURuntimeApiAdapter();
}

extern "C" MESHGPU_API void DestroyMeshGPURuntimeApi(mesh_gpu::MeshGPURuntimeApi* instance)
{
    delete instance;
}
