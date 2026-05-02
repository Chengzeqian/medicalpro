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
using AclrtStream = mesh_gpu::ascend_runtime::AclrtStream;
using AscendRuntimeApi = mesh_gpu::ascend_runtime::RuntimeApi;
constexpr AclError ACL_SUCCESS = 0;
constexpr int ACL_MEMCPY_HOST_TO_DEVICE = 1;
constexpr int ACL_MEMCPY_DEVICE_TO_HOST = 2;
constexpr int ACL_MEM_MALLOC_HUGE_FIRST = 0;
constexpr int ACL_FLOAT = 0;
constexpr int ACL_INT64 = 9;
constexpr int ACL_FORMAT_ND = 2;

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

bool getEnvBool(const char* key, bool default_value) {
    if (!key) {
        return default_value;
    }
    const char* raw = std::getenv(key);
    if (!raw || raw[0] == '\0') {
        return default_value;
    }

    const std::string value = toLowerCopy(std::string(raw));
    if (value == "1" || value == "true" || value == "on" || value == "yes") {
        return true;
    }
    if (value == "0" || value == "false" || value == "off" || value == "no") {
        return false;
    }
    return default_value;
}

struct CentroidMaskCache {
    int device_id = -1;
    int point_count = 0;
    std::size_t elem_count = 0u;
    void* dev_mask_x = nullptr;
    void* dev_mask_y = nullptr;
    void* dev_mask_z = nullptr;
};

struct CentroidMaskBuffers {
    void* dev_mask_x = nullptr;
    void* dev_mask_y = nullptr;
    void* dev_mask_z = nullptr;
    bool from_cache = false;
};

thread_local CentroidMaskCache g_centroid_mask_cache;

void resetCentroidMaskCache(CentroidMaskCache& cache) {
    cache.device_id = -1;
    cache.point_count = 0;
    cache.elem_count = 0u;
    cache.dev_mask_x = nullptr;
    cache.dev_mask_y = nullptr;
    cache.dev_mask_z = nullptr;
}

void releaseCentroidMaskCache(AscendRuntimeApi& runtime) {
    if (g_centroid_mask_cache.dev_mask_z && runtime.aclrtFree) {
        runtime.aclrtFree(g_centroid_mask_cache.dev_mask_z);
    }
    if (g_centroid_mask_cache.dev_mask_y && runtime.aclrtFree) {
        runtime.aclrtFree(g_centroid_mask_cache.dev_mask_y);
    }
    if (g_centroid_mask_cache.dev_mask_x && runtime.aclrtFree) {
        runtime.aclrtFree(g_centroid_mask_cache.dev_mask_x);
    }
    resetCentroidMaskCache(g_centroid_mask_cache);
}

void releaseTemporaryCentroidMasks(AscendRuntimeApi& runtime, CentroidMaskBuffers& buffers) {
    if (buffers.from_cache) {
        return;
    }
    if (buffers.dev_mask_z && runtime.aclrtFree) {
        runtime.aclrtFree(buffers.dev_mask_z);
    }
    if (buffers.dev_mask_y && runtime.aclrtFree) {
        runtime.aclrtFree(buffers.dev_mask_y);
    }
    if (buffers.dev_mask_x && runtime.aclrtFree) {
        runtime.aclrtFree(buffers.dev_mask_x);
    }
    buffers.dev_mask_x = nullptr;
    buffers.dev_mask_y = nullptr;
    buffers.dev_mask_z = nullptr;
}

bool prepareCentroidMaskBuffers(AscendRuntimeApi& runtime,
                                int device_id,
                                int point_count,
                                bool reuse_cache,
                                CentroidMaskBuffers& out_buffers,
                                std::string& detail) {
    if (point_count <= 0) {
        detail = "Invalid point count for centroid mask preparation.";
        return false;
    }
    if (!runtime.aclrtMalloc || !runtime.aclrtMemcpy || !runtime.aclrtFree) {
        detail = "Runtime memory APIs are unavailable for centroid mask preparation.";
        return false;
    }

    const std::size_t elem_count = static_cast<std::size_t>(point_count) * 3u;
    const std::size_t bytes = elem_count * sizeof(float);

    if (reuse_cache &&
        g_centroid_mask_cache.device_id == device_id &&
        g_centroid_mask_cache.point_count == point_count &&
        g_centroid_mask_cache.elem_count == elem_count &&
        g_centroid_mask_cache.dev_mask_x &&
        g_centroid_mask_cache.dev_mask_y &&
        g_centroid_mask_cache.dev_mask_z) {
        out_buffers.dev_mask_x = g_centroid_mask_cache.dev_mask_x;
        out_buffers.dev_mask_y = g_centroid_mask_cache.dev_mask_y;
        out_buffers.dev_mask_z = g_centroid_mask_cache.dev_mask_z;
        out_buffers.from_cache = true;
        detail = "reused cached centroid masks";
        return true;
    }

    std::vector<float> host_mask_x(elem_count, 0.0f);
    std::vector<float> host_mask_y(elem_count, 0.0f);
    std::vector<float> host_mask_z(elem_count, 0.0f);
    for (int i = 0; i < point_count; ++i) {
        const std::size_t base = static_cast<std::size_t>(i) * 3u;
        host_mask_x[base + 0u] = 1.0f;
        host_mask_y[base + 1u] = 1.0f;
        host_mask_z[base + 2u] = 1.0f;
    }

    auto alloc_upload = [&](const std::vector<float>& host, void*& out_ptr) -> bool {
        out_ptr = nullptr;
        if (runtime.aclrtMalloc(&out_ptr, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS) {
            return false;
        }
        const AclError copy_err =
            runtime.aclrtMemcpy(out_ptr, bytes, host.data(), bytes, ACL_MEMCPY_HOST_TO_DEVICE);
        if (copy_err != ACL_SUCCESS) {
            runtime.aclrtFree(out_ptr);
            out_ptr = nullptr;
            return false;
        }
        return true;
    };

    if (reuse_cache) {
        releaseCentroidMaskCache(runtime);
        if (!alloc_upload(host_mask_x, g_centroid_mask_cache.dev_mask_x) ||
            !alloc_upload(host_mask_y, g_centroid_mask_cache.dev_mask_y) ||
            !alloc_upload(host_mask_z, g_centroid_mask_cache.dev_mask_z)) {
            releaseCentroidMaskCache(runtime);
            detail = "Failed to allocate/upload cached centroid masks.";
            return false;
        }
        g_centroid_mask_cache.device_id = device_id;
        g_centroid_mask_cache.point_count = point_count;
        g_centroid_mask_cache.elem_count = elem_count;

        out_buffers.dev_mask_x = g_centroid_mask_cache.dev_mask_x;
        out_buffers.dev_mask_y = g_centroid_mask_cache.dev_mask_y;
        out_buffers.dev_mask_z = g_centroid_mask_cache.dev_mask_z;
        out_buffers.from_cache = true;
        detail = "built new cached centroid masks";
        return true;
    }

    if (!alloc_upload(host_mask_x, out_buffers.dev_mask_x) ||
        !alloc_upload(host_mask_y, out_buffers.dev_mask_y) ||
        !alloc_upload(host_mask_z, out_buffers.dev_mask_z)) {
        releaseTemporaryCentroidMasks(runtime, out_buffers);
        detail = "Failed to allocate/upload temporary centroid masks.";
        return false;
    }

    out_buffers.from_cache = false;
    detail = "built temporary centroid masks";
    return true;
}

bool runCannBinaryFloat1D(AscendRuntimeApi& runtime,
                          const char* op_name,
                          const void* lhs_device,
                          const void* rhs_device,
                          void* out_device,
                          std::size_t elem_count,
                          AclrtStream stream,
                          std::string& detail) {
    if (!runtime.hasOpApi()) {
        detail = "CANN op API symbols are not available.";
        return false;
    }
    if (!op_name || op_name[0] == '\0') {
        detail = "Invalid CANN op name.";
        return false;
    }
    if (!lhs_device || !rhs_device || !out_device || elem_count == 0u) {
        detail = "Invalid device tensor pointer or element count in CANN binary op.";
        return false;
    }

    void* desc_a = nullptr;
    void* desc_b = nullptr;
    void* desc_out = nullptr;
    void* buf_a = nullptr;
    void* buf_b = nullptr;
    void* buf_out = nullptr;
    void* attr = nullptr;

    auto cleanup = [&]() {
        if (attr && runtime.aclopDestroyAttr) {
            runtime.aclopDestroyAttr(attr);
            attr = nullptr;
        }
        if (buf_out && runtime.aclDestroyDataBuffer) {
            runtime.aclDestroyDataBuffer(buf_out);
            buf_out = nullptr;
        }
        if (buf_b && runtime.aclDestroyDataBuffer) {
            runtime.aclDestroyDataBuffer(buf_b);
            buf_b = nullptr;
        }
        if (buf_a && runtime.aclDestroyDataBuffer) {
            runtime.aclDestroyDataBuffer(buf_a);
            buf_a = nullptr;
        }
        if (desc_out && runtime.aclDestroyTensorDesc) {
            runtime.aclDestroyTensorDesc(desc_out);
            desc_out = nullptr;
        }
        if (desc_b && runtime.aclDestroyTensorDesc) {
            runtime.aclDestroyTensorDesc(desc_b);
            desc_b = nullptr;
        }
        if (desc_a && runtime.aclDestroyTensorDesc) {
            runtime.aclDestroyTensorDesc(desc_a);
            desc_a = nullptr;
        }
    };

    const std::int64_t dims[1] = {static_cast<std::int64_t>(elem_count)};
    desc_a = runtime.aclCreateTensorDesc(ACL_FLOAT, 1, dims, ACL_FORMAT_ND);
    desc_b = runtime.aclCreateTensorDesc(ACL_FLOAT, 1, dims, ACL_FORMAT_ND);
    desc_out = runtime.aclCreateTensorDesc(ACL_FLOAT, 1, dims, ACL_FORMAT_ND);
    if (!desc_a || !desc_b || !desc_out) {
        detail = "aclCreateTensorDesc failed in CANN binary op.";
        cleanup();
        return false;
    }

    const std::size_t bytes = elem_count * sizeof(float);
    buf_a = runtime.aclCreateDataBuffer(const_cast<void*>(lhs_device), bytes);
    buf_b = runtime.aclCreateDataBuffer(const_cast<void*>(rhs_device), bytes);
    buf_out = runtime.aclCreateDataBuffer(out_device, bytes);
    if (!buf_a || !buf_b || !buf_out) {
        detail = "aclCreateDataBuffer failed in CANN binary op.";
        cleanup();
        return false;
    }

    attr = runtime.aclopCreateAttr ? runtime.aclopCreateAttr() : nullptr;
    void* input_desc[2] = {desc_a, desc_b};
    void* input_bufs[2] = {buf_a, buf_b};
    void* output_desc[1] = {desc_out};
    void* output_bufs[1] = {buf_out};
    const AclError op_err = runtime.aclopCompileAndExecute(
        op_name,
        2,
        input_desc,
        input_bufs,
        1,
        output_desc,
        output_bufs,
        attr,
        0,
        0,
        nullptr,
        stream);
    if (op_err != ACL_SUCCESS) {
        detail = std::string("aclopCompileAndExecute(") + op_name +
                 ") failed, error=" + std::to_string(op_err);
        cleanup();
        return false;
    }

    cleanup();
    detail = std::string("CANN ") + op_name + " op executed.";
    return true;
}

bool runCannAddFloat1D(AscendRuntimeApi& runtime,
                       const void* lhs_device,
                       const void* rhs_device,
                       void* out_device,
                       std::size_t elem_count,
                       AclrtStream stream,
                       std::string& detail) {
    return runCannBinaryFloat1D(
        runtime, "Add", lhs_device, rhs_device, out_device, elem_count, stream, detail);
}

bool runCannSubFloat1D(AscendRuntimeApi& runtime,
                       const void* lhs_device,
                       const void* rhs_device,
                       void* out_device,
                       std::size_t elem_count,
                       AclrtStream stream,
                       std::string& detail) {
    return runCannBinaryFloat1D(
        runtime, "Sub", lhs_device, rhs_device, out_device, elem_count, stream, detail);
}

bool runCannMulFloat1D(AscendRuntimeApi& runtime,
                       const void* lhs_device,
                       const void* rhs_device,
                       void* out_device,
                       std::size_t elem_count,
                       AclrtStream stream,
                       std::string& detail) {
    return runCannBinaryFloat1D(
        runtime, "Mul", lhs_device, rhs_device, out_device, elem_count, stream, detail);
}

bool runCannReduceSumFloat1D(AscendRuntimeApi& runtime,
                             const void* input_device,
                             std::size_t elem_count,
                             AclrtStream stream,
                             float& out_sum,
                             std::string& detail) {
    if (!runtime.hasOpApi()) {
        detail = "CANN op API symbols are not available.";
        return false;
    }
    if (!input_device || elem_count == 0u) {
        detail = "Invalid input device tensor in CANN ReduceSum op.";
        return false;
    }

    void* dev_axis = nullptr;
    void* dev_out = nullptr;
    void* desc_in = nullptr;
    void* desc_axis = nullptr;
    void* desc_out = nullptr;
    void* buf_in = nullptr;
    void* buf_axis = nullptr;
    void* buf_out = nullptr;
    void* attr = nullptr;

    auto cleanup = [&]() {
        if (attr && runtime.aclopDestroyAttr) {
            runtime.aclopDestroyAttr(attr);
            attr = nullptr;
        }
        if (buf_out && runtime.aclDestroyDataBuffer) {
            runtime.aclDestroyDataBuffer(buf_out);
            buf_out = nullptr;
        }
        if (buf_axis && runtime.aclDestroyDataBuffer) {
            runtime.aclDestroyDataBuffer(buf_axis);
            buf_axis = nullptr;
        }
        if (buf_in && runtime.aclDestroyDataBuffer) {
            runtime.aclDestroyDataBuffer(buf_in);
            buf_in = nullptr;
        }
        if (desc_out && runtime.aclDestroyTensorDesc) {
            runtime.aclDestroyTensorDesc(desc_out);
            desc_out = nullptr;
        }
        if (desc_axis && runtime.aclDestroyTensorDesc) {
            runtime.aclDestroyTensorDesc(desc_axis);
            desc_axis = nullptr;
        }
        if (desc_in && runtime.aclDestroyTensorDesc) {
            runtime.aclDestroyTensorDesc(desc_in);
            desc_in = nullptr;
        }
        if (dev_out && runtime.aclrtFree) {
            runtime.aclrtFree(dev_out);
            dev_out = nullptr;
        }
        if (dev_axis && runtime.aclrtFree) {
            runtime.aclrtFree(dev_axis);
            dev_axis = nullptr;
        }
    };

    const std::size_t out_bytes = sizeof(float);
    const std::size_t axis_bytes = sizeof(std::int64_t);
    if (runtime.aclrtMalloc(&dev_axis, axis_bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS ||
        runtime.aclrtMalloc(&dev_out, out_bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS) {
        detail = "aclrtMalloc failed in CANN ReduceSum op.";
        cleanup();
        return false;
    }

    const std::int64_t axis_host[1] = {0};
    const AclError axis_copy_err = runtime.aclrtMemcpy(
        dev_axis, axis_bytes, axis_host, axis_bytes, ACL_MEMCPY_HOST_TO_DEVICE);
    if (axis_copy_err != ACL_SUCCESS) {
        detail = "aclrtMemcpy H2D failed for ReduceSum axis, error=" + std::to_string(axis_copy_err);
        cleanup();
        return false;
    }

    const std::int64_t dims_in[1] = {static_cast<std::int64_t>(elem_count)};
    const std::int64_t dims_axis[1] = {1};
    const std::int64_t dims_out[1] = {1};
    desc_in = runtime.aclCreateTensorDesc(ACL_FLOAT, 1, dims_in, ACL_FORMAT_ND);
    desc_axis = runtime.aclCreateTensorDesc(ACL_INT64, 1, dims_axis, ACL_FORMAT_ND);
    desc_out = runtime.aclCreateTensorDesc(ACL_FLOAT, 1, dims_out, ACL_FORMAT_ND);
    if (!desc_in || !desc_axis || !desc_out) {
        detail = "aclCreateTensorDesc failed in CANN ReduceSum op.";
        cleanup();
        return false;
    }

    const std::size_t in_bytes = elem_count * sizeof(float);
    buf_in = runtime.aclCreateDataBuffer(const_cast<void*>(input_device), in_bytes);
    buf_axis = runtime.aclCreateDataBuffer(dev_axis, axis_bytes);
    buf_out = runtime.aclCreateDataBuffer(dev_out, out_bytes);
    if (!buf_in || !buf_axis || !buf_out) {
        detail = "aclCreateDataBuffer failed in CANN ReduceSum op.";
        cleanup();
        return false;
    }

    void* input_desc[2] = {desc_in, desc_axis};
    void* input_bufs[2] = {buf_in, buf_axis};
    void* output_desc[1] = {desc_out};
    void* output_bufs[1] = {buf_out};
    attr = runtime.aclopCreateAttr ? runtime.aclopCreateAttr() : nullptr;
    const AclError op_err = runtime.aclopCompileAndExecute(
        "ReduceSum",
        2,
        input_desc,
        input_bufs,
        1,
        output_desc,
        output_bufs,
        attr,
        0,
        0,
        nullptr,
        stream);
    if (op_err != ACL_SUCCESS) {
        detail = "aclopCompileAndExecute(ReduceSum) failed, error=" + std::to_string(op_err);
        cleanup();
        return false;
    }

    if (stream) {
        const AclError sync_err = runtime.aclrtSynchronizeStream(stream);
        if (sync_err != ACL_SUCCESS) {
            detail = "aclrtSynchronizeStream failed after ReduceSum, error=" + std::to_string(sync_err);
            cleanup();
            return false;
        }
    }

    float host_sum = 0.0f;
    const AclError copy_back_err = runtime.aclrtMemcpy(
        &host_sum, out_bytes, dev_out, out_bytes, ACL_MEMCPY_DEVICE_TO_HOST);
    if (copy_back_err != ACL_SUCCESS) {
        detail = "aclrtMemcpy D2H failed after ReduceSum, error=" + std::to_string(copy_back_err);
        cleanup();
        return false;
    }

    out_sum = host_sum;
    cleanup();
    detail = "CANN ReduceSum op executed.";
    return true;
}

bool runCannAddOpProbe(AscendRuntimeApi& runtime, AclrtStream stream, std::string& detail) {
    if (!runtime.hasOpApi()) {
        detail = "CANN op API symbols are not available.";
        return false;
    }

    constexpr int kElemCount = 8;
    const std::size_t bytes = static_cast<std::size_t>(kElemCount) * sizeof(float);
    const float host_a[kElemCount] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f};
    const float host_b[kElemCount] = {0.5f, 1.5f, -2.f, 3.f, 0.f, -1.f, 2.f, 0.25f};
    float host_out[kElemCount] = {};

    void* dev_a = nullptr;
    void* dev_b = nullptr;
    void* dev_out = nullptr;
    auto cleanup = [&]() {
        if (dev_out && runtime.aclrtFree) {
            runtime.aclrtFree(dev_out);
            dev_out = nullptr;
        }
        if (dev_b && runtime.aclrtFree) {
            runtime.aclrtFree(dev_b);
            dev_b = nullptr;
        }
        if (dev_a && runtime.aclrtFree) {
            runtime.aclrtFree(dev_a);
            dev_a = nullptr;
        }
    };

    if (runtime.aclrtMalloc(&dev_a, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS ||
        runtime.aclrtMalloc(&dev_b, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS ||
        runtime.aclrtMalloc(&dev_out, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS) {
        detail = "aclrtMalloc failed in CANN Add op probe.";
        cleanup();
        return false;
    }

    if (runtime.aclrtMemcpy(dev_a, bytes, host_a, bytes, ACL_MEMCPY_HOST_TO_DEVICE) != ACL_SUCCESS ||
        runtime.aclrtMemcpy(dev_b, bytes, host_b, bytes, ACL_MEMCPY_HOST_TO_DEVICE) != ACL_SUCCESS) {
        detail = "aclrtMemcpy H2D failed in CANN Add op probe.";
        cleanup();
        return false;
    }

    std::string add_detail;
    if (!runCannAddFloat1D(runtime, dev_a, dev_b, dev_out, static_cast<std::size_t>(kElemCount), stream, add_detail)) {
        detail = "CANN Add op probe failed: " + add_detail;
        cleanup();
        return false;
    }

    if (stream) {
        const AclError sync_err = runtime.aclrtSynchronizeStream(stream);
        if (sync_err != ACL_SUCCESS) {
            detail = "aclrtSynchronizeStream failed after Add op, error=" + std::to_string(sync_err);
            cleanup();
            return false;
        }
    }

    const AclError copy_back_err =
        runtime.aclrtMemcpy(host_out, bytes, dev_out, bytes, ACL_MEMCPY_DEVICE_TO_HOST);
    if (copy_back_err != ACL_SUCCESS) {
        detail = "aclrtMemcpy D2H failed after Add op, error=" + std::to_string(copy_back_err);
        cleanup();
        return false;
    }

    for (int i = 0; i < kElemCount; ++i) {
        const float expected = host_a[i] + host_b[i];
        if (std::fabs(host_out[i] - expected) > 1e-3f) {
            detail = "CANN Add op probe verification failed at index " + std::to_string(i);
            cleanup();
            return false;
        }
    }

    cleanup();
    detail = "CANN Add op probe succeeded.";
    return true;
}

bool runCannWeightedReduceSumFloat1D(AscendRuntimeApi& runtime,
                                     const void* input_device,
                                     const void* weight_device,
                                     std::size_t elem_count,
                                     AclrtStream stream,
                                     float& out_sum,
                                     std::string& detail) {
    if (!input_device || !weight_device || elem_count == 0u) {
        detail = "Invalid input/weight pointer in weighted ReduceSum chain.";
        return false;
    }

    const std::size_t bytes = elem_count * sizeof(float);
    void* dev_weighted = nullptr;
    auto cleanup = [&]() {
        if (dev_weighted && runtime.aclrtFree) {
            runtime.aclrtFree(dev_weighted);
            dev_weighted = nullptr;
        }
    };

    if (runtime.aclrtMalloc(&dev_weighted, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS) {
        detail = "aclrtMalloc failed in weighted ReduceSum chain.";
        cleanup();
        return false;
    }

    std::string mul_detail;
    if (!runCannMulFloat1D(
            runtime, input_device, weight_device, dev_weighted, elem_count, stream, mul_detail)) {
        detail = "Weighted ReduceSum Mul stage failed: " + mul_detail;
        cleanup();
        return false;
    }

    std::string reduce_detail;
    if (!runCannReduceSumFloat1D(runtime, dev_weighted, elem_count, stream, out_sum, reduce_detail)) {
        detail = "Weighted ReduceSum Reduce stage failed: " + reduce_detail;
        cleanup();
        return false;
    }

    cleanup();
    detail = "Weighted ReduceSum chain succeeded.";
    return true;
}

bool runCannCentroidOpChainInterleaved(const float* xyz_device,
                                       int point_count,
                                       int device_id,
                                       AscendRuntimeApi& runtime,
                                       AclrtStream stream,
                                       float& out_cx,
                                       float& out_cy,
                                       float& out_cz,
                                       std::string& detail) {
    if (!xyz_device || point_count <= 0) {
        detail = "Invalid point buffer/count for centroid op chain.";
        return false;
    }
    if (!runtime.hasOpApi()) {
        detail = "CANN op API symbols are not available for centroid op chain.";
        return false;
    }

    const bool reuse_mask_cache = getEnvBool("MESHGPU_ASCEND_CANN_REUSE_CENTROID_MASK", true);
    const std::size_t elem_count = static_cast<std::size_t>(point_count) * 3u;
    std::string fail_detail;

    for (int attempt = 0; attempt < 2; ++attempt) {
        CentroidMaskBuffers masks;
        std::string mask_detail;
        if (!prepareCentroidMaskBuffers(
                runtime, device_id, point_count, reuse_mask_cache, masks, mask_detail)) {
            detail = "Centroid mask preparation failed: " + mask_detail;
            return false;
        }

        float sum_x = 0.0f;
        float sum_y = 0.0f;
        float sum_z = 0.0f;
        std::string wx_detail;
        std::string wy_detail;
        std::string wz_detail;
        if (!runCannWeightedReduceSumFloat1D(
                runtime, xyz_device, masks.dev_mask_x, elem_count, stream, sum_x, wx_detail)) {
            fail_detail = "Centroid X weighted ReduceSum failed: " + wx_detail;
        } else if (!runCannWeightedReduceSumFloat1D(
                       runtime, xyz_device, masks.dev_mask_y, elem_count, stream, sum_y, wy_detail)) {
            fail_detail = "Centroid Y weighted ReduceSum failed: " + wy_detail;
        } else if (!runCannWeightedReduceSumFloat1D(
                       runtime, xyz_device, masks.dev_mask_z, elem_count, stream, sum_z, wz_detail)) {
            fail_detail = "Centroid Z weighted ReduceSum failed: " + wz_detail;
        } else {
            const float inv = 1.0f / static_cast<float>(point_count);
            out_cx = sum_x * inv;
            out_cy = sum_y * inv;
            out_cz = sum_z * inv;
            releaseTemporaryCentroidMasks(runtime, masks);
            detail = "CANN centroid op chain succeeded (Mul+Reduce x3, " + mask_detail + ").";
            return true;
        }

        releaseTemporaryCentroidMasks(runtime, masks);
        if (!masks.from_cache || !reuse_mask_cache || attempt > 0) {
            detail = fail_detail;
            return false;
        }

        // Cache may be stale after context/device reset; rebuild and retry once.
        releaseCentroidMaskCache(runtime);
    }

    detail = fail_detail.empty() ? "Centroid op chain failed after cache rebuild retry." : fail_detail;
    return false;
}

float computePairwiseRmse(const float* lhs_xyz, const float* rhs_xyz, int count) {
    if (!lhs_xyz || !rhs_xyz || count <= 0) {
        return 0.0f;
    }
    double mse = 0.0;
    for (int i = 0; i < count; ++i) {
        const std::size_t base = static_cast<std::size_t>(i) * 3u;
        const float dx = lhs_xyz[base + 0u] - rhs_xyz[base + 0u];
        const float dy = lhs_xyz[base + 1u] - rhs_xyz[base + 1u];
        const float dz = lhs_xyz[base + 2u] - rhs_xyz[base + 2u];
        mse += static_cast<double>(dx * dx + dy * dy + dz * dz);
    }
    mse /= static_cast<double>(count);
    return static_cast<float>(std::sqrt(std::max(0.0, mse)));
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

bool runCannTranslationAddChain(const mesh_gpu::AscendCannKernelRegistrationInputV1* input,
                                AscendRuntimeApi& runtime,
                                float tx,
                                float ty,
                                float tz,
                                std::vector<float>& transformed_source_xyz,
                                std::string& detail) {
    if (!input || input->source_point_count <= 0 || !input->source_xyz_device) {
        detail = "Invalid source device buffer for CANN translation Add chain.";
        return false;
    }
    if (!runtime.hasOpApi()) {
        detail = "CANN op API symbols are not available for translation Add chain.";
        return false;
    }

    const std::size_t elem_count = static_cast<std::size_t>(input->source_point_count) * 3u;
    if (elem_count == 0u) {
        detail = "Empty source point count for CANN translation Add chain.";
        return false;
    }
    const std::size_t bytes = elem_count * sizeof(float);

    std::vector<float> host_bias(elem_count, 0.0f);
    for (int i = 0; i < input->source_point_count; ++i) {
        const std::size_t base = static_cast<std::size_t>(i) * 3u;
        host_bias[base + 0u] = tx;
        host_bias[base + 1u] = ty;
        host_bias[base + 2u] = tz;
    }

    void* dev_bias = nullptr;
    void* dev_out = nullptr;
    auto cleanup = [&]() {
        if (dev_out && runtime.aclrtFree) {
            runtime.aclrtFree(dev_out);
            dev_out = nullptr;
        }
        if (dev_bias && runtime.aclrtFree) {
            runtime.aclrtFree(dev_bias);
            dev_bias = nullptr;
        }
    };

    if (runtime.aclrtMalloc(&dev_bias, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS ||
        runtime.aclrtMalloc(&dev_out, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS) {
        detail = "aclrtMalloc failed in CANN translation Add chain.";
        cleanup();
        return false;
    }

    const AclError copy_bias_err = runtime.aclrtMemcpy(
        dev_bias, bytes, host_bias.data(), bytes, ACL_MEMCPY_HOST_TO_DEVICE);
    if (copy_bias_err != ACL_SUCCESS) {
        detail = "aclrtMemcpy H2D for translation bias failed, error=" + std::to_string(copy_bias_err);
        cleanup();
        return false;
    }

    std::string add_detail;
    if (!runCannAddFloat1D(runtime,
                           input->source_xyz_device,
                           dev_bias,
                           dev_out,
                           elem_count,
                           input->ascend_stream,
                           add_detail)) {
        detail = "CANN translation Add chain execution failed: " + add_detail;
        cleanup();
        return false;
    }

    if (input->ascend_stream) {
        const AclError sync_err = runtime.aclrtSynchronizeStream(input->ascend_stream);
        if (sync_err != ACL_SUCCESS) {
            detail = "aclrtSynchronizeStream failed in CANN translation Add chain, error=" +
                     std::to_string(sync_err);
            cleanup();
            return false;
        }
    }

    transformed_source_xyz.resize(elem_count);
    const AclError copy_back_err = runtime.aclrtMemcpy(
        transformed_source_xyz.data(), bytes, dev_out, bytes, ACL_MEMCPY_DEVICE_TO_HOST);
    if (copy_back_err != ACL_SUCCESS) {
        detail = "aclrtMemcpy D2H for translated source failed, error=" + std::to_string(copy_back_err);
        cleanup();
        return false;
    }

    cleanup();
    detail = "CANN translation Add chain succeeded.";
    return true;
}

bool runCannRmseOpChain(const mesh_gpu::AscendCannKernelRegistrationInputV1* input,
                        AscendRuntimeApi& runtime,
                        float tx,
                        float ty,
                        float tz,
                        float& out_rmse,
                        std::string& detail) {
    if (!input || !input->source_xyz_device || !input->target_xyz_device) {
        detail = "Invalid device buffers for CANN RMSE op chain.";
        return false;
    }
    if (!runtime.hasOpApi()) {
        detail = "CANN op API symbols are not available for RMSE op chain.";
        return false;
    }

    const int overlap_count = std::min(input->source_point_count, input->target_point_count);
    if (overlap_count <= 0) {
        detail = "No overlapping points for CANN RMSE op chain.";
        return false;
    }

    const std::size_t elem_count = static_cast<std::size_t>(overlap_count) * 3u;
    const std::size_t bytes = elem_count * sizeof(float);
    std::vector<float> host_bias(elem_count, 0.0f);
    for (int i = 0; i < overlap_count; ++i) {
        const std::size_t base = static_cast<std::size_t>(i) * 3u;
        host_bias[base + 0u] = tx;
        host_bias[base + 1u] = ty;
        host_bias[base + 2u] = tz;
    }

    void* dev_bias = nullptr;
    void* dev_transformed = nullptr;
    void* dev_diff = nullptr;
    void* dev_sq = nullptr;
    auto cleanup = [&]() {
        if (dev_sq && runtime.aclrtFree) {
            runtime.aclrtFree(dev_sq);
            dev_sq = nullptr;
        }
        if (dev_diff && runtime.aclrtFree) {
            runtime.aclrtFree(dev_diff);
            dev_diff = nullptr;
        }
        if (dev_transformed && runtime.aclrtFree) {
            runtime.aclrtFree(dev_transformed);
            dev_transformed = nullptr;
        }
        if (dev_bias && runtime.aclrtFree) {
            runtime.aclrtFree(dev_bias);
            dev_bias = nullptr;
        }
    };

    if (runtime.aclrtMalloc(&dev_bias, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS ||
        runtime.aclrtMalloc(&dev_transformed, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS ||
        runtime.aclrtMalloc(&dev_diff, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS ||
        runtime.aclrtMalloc(&dev_sq, bytes, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS) {
        detail = "aclrtMalloc failed in CANN RMSE op chain.";
        cleanup();
        return false;
    }

    const AclError bias_copy_err = runtime.aclrtMemcpy(
        dev_bias, bytes, host_bias.data(), bytes, ACL_MEMCPY_HOST_TO_DEVICE);
    if (bias_copy_err != ACL_SUCCESS) {
        detail = "aclrtMemcpy H2D failed for RMSE translation bias, error=" +
                 std::to_string(bias_copy_err);
        cleanup();
        return false;
    }

    std::string add_detail;
    if (!runCannAddFloat1D(runtime,
                           input->source_xyz_device,
                           dev_bias,
                           dev_transformed,
                           elem_count,
                           input->ascend_stream,
                           add_detail)) {
        detail = "CANN RMSE Add stage failed: " + add_detail;
        cleanup();
        return false;
    }

    std::string sub_detail;
    if (!runCannSubFloat1D(runtime,
                           dev_transformed,
                           input->target_xyz_device,
                           dev_diff,
                           elem_count,
                           input->ascend_stream,
                           sub_detail)) {
        detail = "CANN RMSE Sub stage failed: " + sub_detail;
        cleanup();
        return false;
    }

    std::string mul_detail;
    if (!runCannMulFloat1D(runtime,
                           dev_diff,
                           dev_diff,
                           dev_sq,
                           elem_count,
                           input->ascend_stream,
                           mul_detail)) {
        detail = "CANN RMSE Mul stage failed: " + mul_detail;
        cleanup();
        return false;
    }

    float sum_sq = 0.0f;
    std::string reduce_detail;
    if (!runCannReduceSumFloat1D(runtime, dev_sq, elem_count, input->ascend_stream, sum_sq, reduce_detail)) {
        detail = "CANN RMSE Reduce stage failed: " + reduce_detail;
        cleanup();
        return false;
    }

    const float mean_sq = sum_sq / static_cast<float>(elem_count);
    out_rmse = std::sqrt(std::max(0.0f, mean_sq));
    cleanup();
    detail = "CANN RMSE op chain succeeded (Add+Sub+Mul+Reduce).";
    return true;
}

bool copyDeviceXYZToHost(const mesh_gpu::AscendCannKernelRegistrationInputV1* input,
                         std::vector<float>& target_xyz,
                         std::vector<float>& source_xyz,
                         std::string& detail) {
    if (!input || input->device_buffers_valid == 0 ||
        !input->target_xyz_device || !input->source_xyz_device) {
        detail = "Device buffers are not valid for CANN kernel sample.";
        return false;
    }

    if (input->target_point_count <= 0 || input->source_point_count <= 0) {
        detail = "Invalid point counts for CANN kernel sample.";
        return false;
    }

    AscendRuntimeApi runtime;
    std::string reason;
    if (!runtime.load(reason, "CANN kernel sample")) {
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
            detail = "aclrtSynchronizeStream failed in CANN kernel sample, error=" +
                     std::to_string(sync_err);
            runtime.unload();
            return false;
        }
    }

    const AclError copy_t_err = runtime.aclrtMemcpy(
        target_xyz.data(), target_size, input->target_xyz_device, target_size, ACL_MEMCPY_DEVICE_TO_HOST);
    if (copy_t_err != ACL_SUCCESS) {
        detail = "aclrtMemcpy target D2H failed in CANN kernel sample, error=" +
                 std::to_string(copy_t_err);
        runtime.unload();
        return false;
    }

    const AclError copy_s_err = runtime.aclrtMemcpy(
        source_xyz.data(), source_size, input->source_xyz_device, source_size, ACL_MEMCPY_DEVICE_TO_HOST);
    if (copy_s_err != ACL_SUCCESS) {
        detail = "aclrtMemcpy source D2H failed in CANN kernel sample, error=" +
                 std::to_string(copy_s_err);
        runtime.unload();
        return false;
    }

    runtime.unload();
    detail = reason;
    return true;
}

} // namespace

#if defined(_WIN32)
#define MESHGPU_ASCEND_CANN_KERNEL_EXPORT __declspec(dllexport)
#else
#define MESHGPU_ASCEND_CANN_KERNEL_EXPORT __attribute__((visibility("default")))
#endif

extern "C" MESHGPU_ASCEND_CANN_KERNEL_EXPORT
bool MeshGPU_AscendCannKernel_RunRegistrationV1(
    const mesh_gpu::AscendCannKernelRegistrationInputV1* input,
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
    bool used_device_path = false;
    std::string detail;
    std::string probe_detail = "off";
    std::string op_chain_detail = "off";
    std::string rmse_op_chain_detail = "off";
    bool used_cann_op_chain = false;
    bool used_rmse_op_chain = false;
    float rmse_from_device_op_chain = 0.0f;
    std::vector<float> transformed_source_xyz;

    if (copyDeviceXYZToHost(input, target_xyz_local, source_xyz_local, detail)) {
        target_xyz = target_xyz_local.data();
        source_xyz = source_xyz_local.data();
        used_device_path = true;
    } else if (target_xyz && source_xyz) {
        detail = "Host fallback path in CANN kernel sample.";
    } else {
        if (out_message) {
            *out_message = setMessage("CANN kernel sample has no usable data path. " + detail);
        }
        return false;
    }

    if (used_device_path && isTruthyEnv("MESHGPU_ASCEND_CANN_RUN_OP_PROBE")) {
        AscendRuntimeApi runtime;
        std::string runtime_detail;
        if (!runtime.load(runtime_detail, "CANN kernel sample")) {
            probe_detail = "probe skipped: " + runtime_detail;
        } else {
            std::string op_detail;
            const bool probe_ok = runCannAddOpProbe(runtime, input->ascend_stream, op_detail);
            runtime.unload();
            if (!probe_ok) {
                if (isTruthyEnv("MESHGPU_ASCEND_CANN_OP_PROBE_REQUIRED")) {
                    if (out_message) {
                        *out_message = setMessage("CANN op probe required but failed: " + op_detail);
                    }
                    return false;
                }
                probe_detail = "probe failed (non-required): " + op_detail;
            } else {
                probe_detail = op_detail;
            }
        }
    }

    float target_cx = 0.0f;
    float target_cy = 0.0f;
    float target_cz = 0.0f;
    float source_cx = 0.0f;
    float source_cy = 0.0f;
    float source_cz = 0.0f;
    bool used_centroid_op_chain = false;
    std::string centroid_detail = "host centroid path.";
    const bool centroid_required = isTruthyEnv("MESHGPU_ASCEND_CANN_CENTROID_OP_CHAIN_REQUIRED");
    const bool enable_centroid_op_chain =
        used_device_path && getEnvBool("MESHGPU_ASCEND_CANN_ENABLE_CENTROID_OP_CHAIN", true);
    if (enable_centroid_op_chain) {
        AscendRuntimeApi centroid_runtime;
        std::string runtime_detail;
        if (!centroid_runtime.load(runtime_detail, "CANN kernel sample")) {
            centroid_detail = "centroid op chain skipped: " + runtime_detail;
        } else {
            std::string target_centroid_detail;
            std::string source_centroid_detail;
            if (!runCannCentroidOpChainInterleaved(
                    input->target_xyz_device,
                    input->target_point_count,
                    input->device_id,
                    centroid_runtime,
                    input->ascend_stream,
                    target_cx,
                    target_cy,
                    target_cz,
                    target_centroid_detail)) {
                centroid_detail = "target centroid op chain failed: " + target_centroid_detail;
            } else if (!runCannCentroidOpChainInterleaved(
                           input->source_xyz_device,
                           input->source_point_count,
                           input->device_id,
                           centroid_runtime,
                           input->ascend_stream,
                           source_cx,
                           source_cy,
                           source_cz,
                           source_centroid_detail)) {
                centroid_detail = "source centroid op chain failed: " + source_centroid_detail;
            } else {
                used_centroid_op_chain = true;
                centroid_detail = "centroid op chain succeeded (target+source).";
            }
            centroid_runtime.unload();
        }
    } else if (used_device_path) {
        centroid_detail = "centroid op chain disabled by MESHGPU_ASCEND_CANN_ENABLE_CENTROID_OP_CHAIN.";
    }

    if (!used_centroid_op_chain) {
        if (!computeCentroid(target_xyz, input->target_point_count, target_cx, target_cy, target_cz) ||
            !computeCentroid(source_xyz, input->source_point_count, source_cx, source_cy, source_cz)) {
            if (out_message) {
                *out_message = setMessage("Invalid point count or fallback buffers.");
            }
            return false;
        }
        if (used_device_path && centroid_required) {
            if (out_message) {
                *out_message = setMessage("CANN centroid op chain required but failed: " + centroid_detail);
            }
            return false;
        }
    }

    const float tx = target_cx - source_cx;
    const float ty = target_cy - source_cy;
    const float tz = target_cz - source_cz;

    if (used_device_path) {
        const bool enable_op_chain = getEnvBool("MESHGPU_ASCEND_CANN_ENABLE_OP_CHAIN", true);
        if (enable_op_chain) {
            AscendRuntimeApi runtime;
            std::string runtime_detail;
            if (!runtime.load(runtime_detail, "CANN kernel sample")) {
                op_chain_detail = "op chain skipped: " + runtime_detail;
            } else {
                std::string chain_detail;
                if (runCannTranslationAddChain(
                        input,
                        runtime,
                        tx,
                        ty,
                        tz,
                        transformed_source_xyz,
                        chain_detail)) {
                    used_cann_op_chain = true;
                    op_chain_detail = chain_detail;
                } else {
                    op_chain_detail = "op chain failed: " + chain_detail;
                    if (isTruthyEnv("MESHGPU_ASCEND_CANN_OP_CHAIN_REQUIRED")) {
                        runtime.unload();
                        if (out_message) {
                            *out_message = setMessage("CANN op chain required but failed: " + chain_detail);
                        }
                        return false;
                    }
                }

                if (used_cann_op_chain) {
                    const bool enable_rmse_op_chain =
                        getEnvBool("MESHGPU_ASCEND_CANN_ENABLE_RMSE_OP_CHAIN", true);
                    if (enable_rmse_op_chain) {
                        std::string rmse_detail;
                        if (runCannRmseOpChain(
                                input,
                                runtime,
                                tx,
                                ty,
                                tz,
                                rmse_from_device_op_chain,
                                rmse_detail)) {
                            used_rmse_op_chain = true;
                            rmse_op_chain_detail = rmse_detail;
                        } else {
                            rmse_op_chain_detail = "rmse op chain failed: " + rmse_detail;
                            if (isTruthyEnv("MESHGPU_ASCEND_CANN_RMSE_OP_CHAIN_REQUIRED")) {
                                runtime.unload();
                                if (out_message) {
                                    *out_message =
                                        setMessage("CANN RMSE op chain required but failed: " + rmse_detail);
                                }
                                return false;
                            }
                        }
                    } else {
                        rmse_op_chain_detail =
                            "rmse op chain disabled by MESHGPU_ASCEND_CANN_ENABLE_RMSE_OP_CHAIN.";
                    }
                }
                runtime.unload();
            }
        } else {
            op_chain_detail = "op chain disabled by MESHGPU_ASCEND_CANN_ENABLE_OP_CHAIN.";
            rmse_op_chain_detail =
                "rmse op chain disabled because translation op chain is disabled.";
        }
    }

    output->translation_x = tx;
    output->translation_y = ty;
    output->translation_z = tz;
    if (used_rmse_op_chain) {
        output->rmse = rmse_from_device_op_chain;
    } else if (used_cann_op_chain) {
        const int overlap_count = std::min(input->source_point_count, input->target_point_count);
        if (overlap_count > 0) {
            output->rmse = computePairwiseRmse(
                transformed_source_xyz.data(),
                target_xyz,
                overlap_count);
        } else {
            output->rmse = 0.0f;
        }
    } else {
        output->rmse = computeRmseToPoint(
            source_xyz,
            input->source_point_count,
            tx,
            ty,
            tz,
            target_cx,
            target_cy,
            target_cz);
    }
    output->iterations = input->params ? std::max(1, std::min(input->params->max_iterations, 2)) : 2;
    output->converged = 1;

    if (out_message) {
        std::stringstream ss;
        ss << "Sample CANN kernel plugin executed via "
           << (used_device_path ? "device-buffer D2H path" : "host-fallback path")
            << "; detail=" << detail
           << "; centroid=" << centroid_detail
            << "; op_probe=" << probe_detail
            << "; op_chain=" << op_chain_detail
            << "; rmse_op_chain=" << rmse_op_chain_detail
           << "; rmse_metric="
           << (used_rmse_op_chain
                   ? "device_after_add_sub_mul_reduce"
                   : (used_cann_op_chain ? "pairwise_after_cann_add" : "centroid_to_point"));
        *out_message = setMessage(ss.str());
    }
    return true;
}
