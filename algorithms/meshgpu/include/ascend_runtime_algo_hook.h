#pragma once

#include "mesh_gpu_interface.h"
#include <cstdint>

namespace mesh_gpu {

// External Ascend algorithm plugin hook (optional).
// Symbol name expected by host plugin loader:
//   MeshGPU_AscendAlgo_RunRegistrationV1
//   MeshGPU_AscendAlgo_RunRegistrationV2
//
// The external implementation should consume Ascend device buffers and
// return translation + RMSE for the current registration iteration.

struct AscendAlgoRegistrationInputV1 {
    const float* target_xyz_device = nullptr;   // Interleaved XYZ on Ascend device
    int target_point_count = 0;

    const float* source_xyz_device = nullptr;   // Interleaved XYZ on Ascend device
    int source_point_count = 0;

    int device_id = 0;
    void* ascend_stream = nullptr;              // aclrtStream as opaque pointer

    const Transform4x4* initial_transform = nullptr;
    const RegistrationParams* params = nullptr;
};

struct AscendAlgoRegistrationOutputV1 {
    float translation_x = 0.0f;
    float translation_y = 0.0f;
    float translation_z = 0.0f;
    float rmse = 0.0f;
    int iterations = 0;
    int converged = 0;
};

using AscendAlgoRunRegistrationV1Fn = bool (*)(
    const AscendAlgoRegistrationInputV1* input,
    AscendAlgoRegistrationOutputV1* output,
    const char** out_message);

// V2 extends V1 by adding optional host interleaved XYZ buffers so plugin
// bring-up can be validated even when Ascend runtime/device buffers are absent.
// If `device_buffers_valid` is 1, device pointers are ready to use.
struct AscendAlgoRegistrationInputV2 {
    const float* target_xyz_device = nullptr;   // Interleaved XYZ on Ascend device
    int target_point_count = 0;

    const float* source_xyz_device = nullptr;   // Interleaved XYZ on Ascend device
    int source_point_count = 0;

    const float* target_xyz_host = nullptr;     // Optional host fallback buffer
    const float* source_xyz_host = nullptr;     // Optional host fallback buffer
    std::int32_t device_buffers_valid = 0;      // 1: device pointers are usable

    int device_id = 0;
    void* ascend_stream = nullptr;              // aclrtStream as opaque pointer

    const Transform4x4* initial_transform = nullptr;
    const RegistrationParams* params = nullptr;
};

using AscendAlgoRunRegistrationV2Fn = bool (*)(
    const AscendAlgoRegistrationInputV2* input,
    AscendAlgoRegistrationOutputV1* output,
    const char** out_message);

} // namespace mesh_gpu
