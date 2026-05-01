#pragma once

#include <cstdint>

#include "mesh_gpu_interface.h"

namespace mesh_gpu {

// Keep ABI version monotonic when changing this function table.
static constexpr uint32_t kAscendBackendApiV1 = 1;

// Registration output view used by Ascend plugin implementation.
// rmse_history points to plugin-owned memory and is copied by caller.
struct AscendRegistrationResultView {
    Transform4x4 transform;
    float rmse = 0.0f;
    int iterations = 0;
    int converged = 0;
    const float* rmse_history = nullptr;
    int rmse_history_count = 0;
};

// Function table exported by Ascend backend plugin.
// Symbol name: MeshGPU_GetAscendBackendApiV1
struct AscendBackendApiV1 {
    uint32_t abi_version = 0;
    const char* backend_name = nullptr;

    // Lifecycle
    bool (*create_context)(int device_id, bool strict, void** out_context, const char** out_message) = nullptr;
    void (*destroy_context)(void* context) = nullptr;

    // Mesh and source cloud
    bool (*load_target_mesh)(void* context, const char* ply_file, float cell_size, const char** out_message) = nullptr;
    bool (*has_target_mesh)(void* context) = nullptr;
    bool (*get_mesh_stats)(void* context, MeshStats* out_stats) = nullptr;
    bool (*set_source_point_cloud)(void* context, const Point3D* points, int count, const char** out_message) = nullptr;
    bool (*clear_source_point_cloud)(void* context, const char** out_message) = nullptr;

    // Core registration
    bool (*run_registration)(void* context,
                             const Transform4x4* initial_transform,
                             const RegistrationParams* params,
                             AscendRegistrationResultView* out_result,
                             const char** out_message) = nullptr;
};

using GetAscendBackendApiV1Fn = const AscendBackendApiV1* (*)();

} // namespace mesh_gpu
