#pragma once

/**
 * @file meshgpu_adapter.h
 * @brief Adapter between ProbeCalibration and MeshGPU
 *
 * This module provides the interface to convert ProbeCalibration's
 * Super Points into MeshGPU's SourcePointCloud format for GPU registration.
 *
 * Usage:
 *   1. Collect points using RealtimePointCloudCollector
 *   2. Use MeshGPUAdapter to create SourcePointCloud
 *   3. Pass to GICPRegistration for alignment
 *
 * Note: This header should be included ONLY in files that link with MeshGPU.
 * It depends on MeshGPU types (SourcePointCloud, Matrix4x4, etc.)
 */

#include "realtime_point_cloud_collector.h"
#include <cstring>

// Forward declarations for MeshGPU types
// These are defined in MeshGPU/include/types.h
struct SourcePointCloud;
struct Matrix4x4;

namespace ProbeCalib {

/**
 * @class MeshGPUAdapter
 * @brief Converts ProbeCalibration point cloud to MeshGPU format
 *
 * This is a stateless adapter that handles memory conversion
 * between the two modules.
 */
class MeshGPUAdapter {
public:
    /**
     * Convert ExportedPointCloud to SourcePointCloud (host memory).
     * The caller is responsible for freeing the returned memory.
     *
     * @param input Fused point cloud from RealtimePointCloudCollector
     * @param output SourcePointCloud structure (will allocate host memory)
     * @return true if successful
     */
    static bool convertToSourceCloud(const ExportedPointCloud& input,
                                     SourcePointCloud& output) {
        if (input.num_points == 0) {
            output.num_points = 0;
            output.on_device = false;
            return true;
        }

        // Allocate host memory
        output.points_x = new float[input.num_points];
        output.points_y = new float[input.num_points];
        output.points_z = new float[input.num_points];
        output.normals_x = nullptr;  // No normals from probe
        output.normals_y = nullptr;
        output.normals_z = nullptr;
        output.num_points = input.num_points;
        output.on_device = false;
        output.noise_stddev = 0.0f;

        // Copy data
        std::memcpy(output.points_x, input.points_x.data(), input.num_points * sizeof(float));
        std::memcpy(output.points_y, input.points_y.data(), input.num_points * sizeof(float));
        std::memcpy(output.points_z, input.points_z.data(), input.num_points * sizeof(float));

        return true;
    }

    /**
     * Convert directly from RealtimePointCloudCollector.
     */
    static bool convertToSourceCloud(const RealtimePointCloudCollector& collector,
                                     SourcePointCloud& output) {
        ExportedPointCloud exported;
        collector.exportForGPU(exported);
        return convertToSourceCloud(exported, output);
    }

    /**
     * Upload SourcePointCloud to GPU.
     * This should be called before passing to GICPRegistration.
     *
     * @param cloud SourcePointCloud (host memory)
     * @return true if successful
     */
    static bool uploadToDevice(SourcePointCloud& cloud);

    /**
     * Free SourcePointCloud memory (host or device).
     */
    static void freeSourceCloud(SourcePointCloud& cloud);

    /**
     * Convert Eigen Matrix4f to MeshGPU Matrix4x4.
     * Useful for passing calibration transforms.
     */
    static void eigenToMatrix4x4(const Matrix4f& eigen_mat, Matrix4x4& gpu_mat) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                gpu_mat.m[i * 4 + j] = eigen_mat(i, j);
            }
        }
    }

    /**
     * Convert MeshGPU Matrix4x4 to Eigen Matrix4f.
     */
    static void matrix4x4ToEigen(const Matrix4x4& gpu_mat, Matrix4f& eigen_mat) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                eigen_mat(i, j) = gpu_mat.m[i * 4 + j];
            }
        }
    }
};

} // namespace ProbeCalib

// ============================================================================
// Implementation requiring CUDA (separate file or conditionally included)
// ============================================================================

#ifdef MESHGPU_ADAPTER_IMPLEMENTATION

#include <cuda_runtime.h>

namespace ProbeCalib {

bool MeshGPUAdapter::uploadToDevice(SourcePointCloud& cloud) {
    if (cloud.on_device) {
        return true;  // Already on device
    }

    if (cloud.num_points == 0) {
        return true;
    }

    size_t size = cloud.num_points * sizeof(float);

    float* d_x = nullptr;
    float* d_y = nullptr;
    float* d_z = nullptr;

    cudaError_t err;

    err = cudaMalloc(&d_x, size);
    if (err != cudaSuccess) {
        std::cerr << "cudaMalloc failed for points_x: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMalloc(&d_y, size);
    if (err != cudaSuccess) {
        cudaFree(d_x);
        std::cerr << "cudaMalloc failed for points_y: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMalloc(&d_z, size);
    if (err != cudaSuccess) {
        cudaFree(d_x);
        cudaFree(d_y);
        std::cerr << "cudaMalloc failed for points_z: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    // Copy to device
    cudaMemcpy(d_x, cloud.points_x, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_y, cloud.points_y, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_z, cloud.points_z, size, cudaMemcpyHostToDevice);

    // Free host memory
    delete[] cloud.points_x;
    delete[] cloud.points_y;
    delete[] cloud.points_z;

    // Update pointers
    cloud.points_x = d_x;
    cloud.points_y = d_y;
    cloud.points_z = d_z;
    cloud.on_device = true;

    return true;
}

void MeshGPUAdapter::freeSourceCloud(SourcePointCloud& cloud) {
    if (cloud.on_device) {
        if (cloud.points_x) cudaFree(cloud.points_x);
        if (cloud.points_y) cudaFree(cloud.points_y);
        if (cloud.points_z) cudaFree(cloud.points_z);
        if (cloud.normals_x) cudaFree(cloud.normals_x);
        if (cloud.normals_y) cudaFree(cloud.normals_y);
        if (cloud.normals_z) cudaFree(cloud.normals_z);
    } else {
        if (cloud.points_x) delete[] cloud.points_x;
        if (cloud.points_y) delete[] cloud.points_y;
        if (cloud.points_z) delete[] cloud.points_z;
        if (cloud.normals_x) delete[] cloud.normals_x;
        if (cloud.normals_y) delete[] cloud.normals_y;
        if (cloud.normals_z) delete[] cloud.normals_z;
    }

    cloud.points_x = nullptr;
    cloud.points_y = nullptr;
    cloud.points_z = nullptr;
    cloud.normals_x = nullptr;
    cloud.normals_y = nullptr;
    cloud.normals_z = nullptr;
    cloud.num_points = 0;
    cloud.on_device = false;
}

} // namespace ProbeCalib

#endif // MESHGPU_ADAPTER_IMPLEMENTATION
