#pragma once

#include "ascend_runtime_algo_hook.h"

namespace mesh_gpu {

// Optional secondary hook for real CANN/NPU kernel implementation.
// Symbol expected by loader:
//   MeshGPU_AscendCannKernel_RunRegistrationV1
//
// The hook receives device buffers and optional host fallback buffers.
// Implementers can run true NPU kernels and return registration deltas.
struct AscendCannKernelRegistrationInputV1 {
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

using AscendCannKernelRunRegistrationV1Fn = bool (*)(
    const AscendCannKernelRegistrationInputV1* input,
    AscendAlgoRegistrationOutputV1* output,
    const char** out_message);

} // namespace mesh_gpu
