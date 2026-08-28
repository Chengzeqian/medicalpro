#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cstdint>
#include <cstdio>

/**
 * GPU Kernel: Evaluate rotation matrices in parallel
 *
 * Strategy:
 * - Each thread block handles ONE rotation matrix
 * - Threads in the block cooperatively process all source points
 * - Use shared memory for the rotation matrix
 * - Atomic add to count hits for this rotation
 *
 * Grid/Block configuration:
 * - gridDim.x = num_rotations
 * - blockDim.x = 256 (threads per block)
 */
__global__ void evaluateRotationsKernel(
    const float* __restrict__ source_x,
    const float* __restrict__ source_y,
    const float* __restrict__ source_z,
    uint32_t num_points,
    const float* __restrict__ rotation_matrices,  // num_rotations * 16 floats
    uint32_t num_rotations,
    float3 grid_min,
    float grid_cell_size,
    int3 grid_dims,
    const uint32_t* __restrict__ grid_cell_counts,
    int* __restrict__ scores
) {
    // Each block handles one rotation
    uint32_t rotation_idx = blockIdx.x;
    if (rotation_idx >= num_rotations) return;

    // Load rotation matrix into shared memory
    __shared__ float R[16];
    if (threadIdx.x < 16) {
        R[threadIdx.x] = rotation_matrices[rotation_idx * 16 + threadIdx.x];
    }
    __syncthreads();

    // Each thread processes multiple points
    __shared__ int block_score;
    if (threadIdx.x == 0) {
        block_score = 0;
    }
    __syncthreads();

    int local_hits = 0;

    // Process points in strided fashion
    for (uint32_t i = threadIdx.x; i < num_points; i += blockDim.x) {
        float px = source_x[i];
        float py = source_y[i];
        float pz = source_z[i];

        // Apply rotation matrix: p' = R * p
        // R is stored row-major: R[0-3] = row0, R[4-7] = row1, etc.
        float tx = R[0] * px + R[1] * py + R[2] * pz + R[3];
        float ty = R[4] * px + R[5] * py + R[6] * pz + R[7];
        float tz = R[8] * px + R[9] * py + R[10] * pz + R[11];

        // Compute grid cell index
        int cx = (int)floorf((tx - grid_min.x) / grid_cell_size);
        int cy = (int)floorf((ty - grid_min.y) / grid_cell_size);
        int cz = (int)floorf((tz - grid_min.z) / grid_cell_size);

        // Check bounds
        if (cx >= 0 && cx < grid_dims.x &&
            cy >= 0 && cy < grid_dims.y &&
            cz >= 0 && cz < grid_dims.z) {

            uint32_t cell_idx = cx + cy * grid_dims.x + cz * grid_dims.x * grid_dims.y;

            // Check if this cell is occupied (has target vertices)
            if (grid_cell_counts[cell_idx] > 0) {
                local_hits++;
            }
        }
    }

    // Reduce within block using atomicAdd to shared memory
    atomicAdd(&block_score, local_hits);
    __syncthreads();

    // Thread 0 writes the final score
    if (threadIdx.x == 0) {
        scores[rotation_idx] = block_score;
    }
}

/**
 * Distance energy kernel: score = -Σ min(dist_to_mesh², cutoff²)
 * Truncated distance avoids outlier domination.
 * Higher score (closer to 0) = better alignment.
 * Uses 3x3x3 grid neighborhood for nearest-neighbor search.
 */
__global__ void evaluateRotationsDistanceEnergyKernel(
    const float* __restrict__ source_x,
    const float* __restrict__ source_y,
    const float* __restrict__ source_z,
    uint32_t num_points,
    const float* __restrict__ rotation_matrices,
    uint32_t num_rotations,
    float3 grid_min,
    float grid_cell_size,
    int3 grid_dims,
    const uint32_t* __restrict__ grid_cell_counts,
    const uint32_t* __restrict__ grid_cell_starts,
    const uint32_t* __restrict__ grid_vertex_indices,
    const float* __restrict__ target_x,
    const float* __restrict__ target_y,
    const float* __restrict__ target_z,
    float cutoff_sq,
    int* __restrict__ scores
) {
    uint32_t rotation_idx = blockIdx.x;
    if (rotation_idx >= num_rotations) return;

    __shared__ float R[16];
    if (threadIdx.x < 16) {
        R[threadIdx.x] = rotation_matrices[rotation_idx * 16 + threadIdx.x];
    }
    __syncthreads();

    __shared__ int block_score;
    if (threadIdx.x == 0) {
        block_score = 0;
    }
    __syncthreads();

    float local_energy = 0.0f;

    for (uint32_t i = threadIdx.x; i < num_points; i += blockDim.x) {
        float px = source_x[i];
        float py = source_y[i];
        float pz = source_z[i];

        float tx = R[0] * px + R[1] * py + R[2] * pz + R[3];
        float ty = R[4] * px + R[5] * py + R[6] * pz + R[7];
        float tz = R[8] * px + R[9] * py + R[10] * pz + R[11];

        int cx = (int)floorf((tx - grid_min.x) / grid_cell_size);
        int cy = (int)floorf((ty - grid_min.y) / grid_cell_size);
        int cz = (int)floorf((tz - grid_min.z) / grid_cell_size);

        float min_dist_sq = cutoff_sq;

        // Search 3x3x3 neighborhood
        for (int dz = -1; dz <= 1; dz++) {
            int ncz = cz + dz;
            if (ncz < 0 || ncz >= grid_dims.z) continue;
            for (int dy = -1; dy <= 1; dy++) {
                int ncy = cy + dy;
                if (ncy < 0 || ncy >= grid_dims.y) continue;
                for (int dx = -1; dx <= 1; dx++) {
                    int ncx = cx + dx;
                    if (ncx < 0 || ncx >= grid_dims.x) continue;

                    uint32_t cell_idx = ncx + ncy * grid_dims.x
                                      + ncz * grid_dims.x * grid_dims.y;
                    uint32_t count = grid_cell_counts[cell_idx];
                    if (count == 0) continue;

                    uint32_t start = grid_cell_starts[cell_idx];
                    for (uint32_t j = 0; j < count; j++) {
                        uint32_t vid = grid_vertex_indices[start + j];
                        float ex = tx - target_x[vid];
                        float ey = ty - target_y[vid];
                        float ez = tz - target_z[vid];
                        float d2 = ex * ex + ey * ey + ez * ez;
                        if (d2 < min_dist_sq) min_dist_sq = d2;
                    }
                }
            }
        }

        // Accumulate negative truncated distance energy
        local_energy -= min_dist_sq;  // min_dist_sq <= cutoff_sq
    }

    // Convert to integer (×1000 for precision) and reduce
    atomicAdd(&block_score, (int)(local_energy * 1000.0f));
    __syncthreads();

    if (threadIdx.x == 0) {
        scores[rotation_idx] = block_score;
    }
}

__global__ void evaluateTransformCandidateGeometryScoreKernel(
    const float* __restrict__ source_x,
    const float* __restrict__ source_y,
    const float* __restrict__ source_z,
    uint32_t num_points,
    const float* __restrict__ transform_matrices,
    uint32_t num_transforms,
    float3 grid_min,
    float grid_cell_size,
    int3 grid_dims,
    const uint32_t* __restrict__ grid_cell_counts,
    const uint32_t* __restrict__ grid_cell_starts,
    const uint32_t* __restrict__ grid_vertex_indices,
    const float* __restrict__ target_x,
    const float* __restrict__ target_y,
    const float* __restrict__ target_z,
    const float* __restrict__ target_normals_x,
    const float* __restrict__ target_normals_y,
    const float* __restrict__ target_normals_z,
    const float* __restrict__ target_curvature,
    float cutoff_sq,
    float cutoff_mm,
    int* __restrict__ scores,
    float* __restrict__ normal_consistency_scores,
    float* __restrict__ curvature_scores
) {
    uint32_t transform_idx = blockIdx.x;
    if (transform_idx >= num_transforms) return;

    __shared__ float T[16];
    if (threadIdx.x < 16) {
        T[threadIdx.x] = transform_matrices[transform_idx * 16 + threadIdx.x];
    }
    __syncthreads();

    __shared__ int block_score;
    __shared__ float block_normal_score;
    __shared__ float block_curvature_score;
    if (threadIdx.x == 0) {
        block_score = 0;
        block_normal_score = 0.0f;
        block_curvature_score = 0.0f;
    }
    __syncthreads();

    float local_energy = 0.0f;
    float local_normal_score = 0.0f;
    float local_curvature_score = 0.0f;

    for (uint32_t i = threadIdx.x; i < num_points; i += blockDim.x) {
        float px = source_x[i];
        float py = source_y[i];
        float pz = source_z[i];

        float tx = T[0] * px + T[1] * py + T[2] * pz + T[3];
        float ty = T[4] * px + T[5] * py + T[6] * pz + T[7];
        float tz = T[8] * px + T[9] * py + T[10] * pz + T[11];

        int cx = (int)floorf((tx - grid_min.x) / grid_cell_size);
        int cy = (int)floorf((ty - grid_min.y) / grid_cell_size);
        int cz = (int)floorf((tz - grid_min.z) / grid_cell_size);

        float min_dist_sq = cutoff_sq;
        float best_dx = 0.0f;
        float best_dy = 0.0f;
        float best_dz = 0.0f;
        uint32_t best_vid = 0;
        bool has_match = false;

        for (int dz = -1; dz <= 1; dz++) {
            int ncz = cz + dz;
            if (ncz < 0 || ncz >= grid_dims.z) continue;
            for (int dy = -1; dy <= 1; dy++) {
                int ncy = cy + dy;
                if (ncy < 0 || ncy >= grid_dims.y) continue;
                for (int dx = -1; dx <= 1; dx++) {
                    int ncx = cx + dx;
                    if (ncx < 0 || ncx >= grid_dims.x) continue;

                    uint32_t cell_idx = ncx + ncy * grid_dims.x
                                      + ncz * grid_dims.x * grid_dims.y;
                    uint32_t count = grid_cell_counts[cell_idx];
                    if (count == 0) continue;

                    uint32_t start = grid_cell_starts[cell_idx];
                    for (uint32_t j = 0; j < count; j++) {
                        uint32_t vid = grid_vertex_indices[start + j];
                        float ex = tx - target_x[vid];
                        float ey = ty - target_y[vid];
                        float ez = tz - target_z[vid];
                        float d2 = ex * ex + ey * ey + ez * ez;
                        if (d2 < min_dist_sq) {
                            min_dist_sq = d2;
                            best_dx = ex;
                            best_dy = ey;
                            best_dz = ez;
                            best_vid = vid;
                            has_match = true;
                        }
                    }
                }
            }
        }

        local_energy -= min_dist_sq;

        if (has_match && target_normals_x && target_normals_y && target_normals_z) {
            float nx = target_normals_x[best_vid];
            float ny = target_normals_y[best_vid];
            float nz = target_normals_z[best_vid];
            float normal_len = sqrtf(nx * nx + ny * ny + nz * nz);
            if (normal_len > 1e-6f && cutoff_mm > 1e-6f) {
                nx /= normal_len;
                ny /= normal_len;
                nz /= normal_len;
                float point_to_plane_residual = fabsf(best_dx * nx + best_dy * ny + best_dz * nz);
                float normalized_residual = fminf(point_to_plane_residual / cutoff_mm, 1.0f);
                local_normal_score += 1.0f - normalized_residual;
            }
        }

        if (has_match && target_curvature) {
            float curvature = fabsf(target_curvature[best_vid]);
            local_curvature_score += fminf(1.0f - expf(-curvature * 0.05f), 1.0f);
        }
    }

    atomicAdd(&block_score, (int)(local_energy * 1000.0f));
    atomicAdd(&block_normal_score, local_normal_score);
    atomicAdd(&block_curvature_score, local_curvature_score);
    __syncthreads();

    if (threadIdx.x == 0) {
        const float denominator = num_points > 0 ? static_cast<float>(num_points) : 1.0f;
        scores[transform_idx] = block_score;
        normal_consistency_scores[transform_idx] = fminf(fmaxf(block_normal_score / denominator, 0.0f), 1.0f);
        curvature_scores[transform_idx] = fminf(fmaxf(block_curvature_score / denominator, 0.0f), 1.0f);
    }
}

// Host wrapper function
extern "C" void launchRotationEvaluationKernel(
    const float* source_x,
    const float* source_y,
    const float* source_z,
    uint32_t num_points,
    const float* rotation_matrices,
    uint32_t num_rotations,
    float3 grid_min,
    float grid_cell_size,
    int3 grid_dims,
    const uint32_t* grid_cell_counts,
    int* scores
) {
    // One block per rotation, 256 threads per block
    dim3 grid(num_rotations);
    dim3 block(256);

    evaluateRotationsKernel<<<grid, block>>>(
        source_x, source_y, source_z, num_points,
        rotation_matrices, num_rotations,
        grid_min, grid_cell_size, grid_dims,
        grid_cell_counts, scores
    );

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("[RotationSearch] Kernel error: %s\n", cudaGetErrorString(err));
    }
    cudaDeviceSynchronize();
}

extern "C" void launchTransformCandidateGeometryScoreKernel(
    const float* source_x,
    const float* source_y,
    const float* source_z,
    uint32_t num_points,
    const float* transform_matrices,
    uint32_t num_transforms,
    float3 grid_min,
    float grid_cell_size,
    int3 grid_dims,
    const uint32_t* grid_cell_counts,
    const uint32_t* grid_cell_starts,
    const uint32_t* grid_vertex_indices,
    const float* target_x,
    const float* target_y,
    const float* target_z,
    const float* target_normals_x,
    const float* target_normals_y,
    const float* target_normals_z,
    const float* target_curvature,
    float cutoff_mm,
    int* scores,
    float* normal_consistency_scores,
    float* curvature_scores
) {
    dim3 grid(num_transforms);
    dim3 block(256);

    const float effective_cutoff_mm = fmaxf(cutoff_mm, 1e-3f);
    const float cutoff_sq = effective_cutoff_mm * effective_cutoff_mm;

    evaluateTransformCandidateGeometryScoreKernel<<<grid, block>>>(
        source_x,
        source_y,
        source_z,
        num_points,
        transform_matrices,
        num_transforms,
        grid_min,
        grid_cell_size,
        grid_dims,
        grid_cell_counts,
        grid_cell_starts,
        grid_vertex_indices,
        target_x,
        target_y,
        target_z,
        target_normals_x,
        target_normals_y,
        target_normals_z,
        target_curvature,
        cutoff_sq,
        effective_cutoff_mm,
        scores,
        normal_consistency_scores,
        curvature_scores
    );

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("[RotationSearch] Candidate geometry score kernel error: %s\n",
               cudaGetErrorString(err));
    }
    cudaDeviceSynchronize();
}

// Host wrapper for distance energy kernel
extern "C" void launchRotationDistanceEnergyKernel(
    const float* source_x,
    const float* source_y,
    const float* source_z,
    uint32_t num_points,
    const float* rotation_matrices,
    uint32_t num_rotations,
    float3 grid_min,
    float grid_cell_size,
    int3 grid_dims,
    const uint32_t* grid_cell_counts,
    const uint32_t* grid_cell_starts,
    const uint32_t* grid_vertex_indices,
    const float* target_x,
    const float* target_y,
    const float* target_z,
    float cutoff_mm,
    int* scores
) {
    dim3 grid(num_rotations);
    dim3 block(256);

    float cutoff_sq = cutoff_mm * cutoff_mm;

    evaluateRotationsDistanceEnergyKernel<<<grid, block>>>(
        source_x, source_y, source_z, num_points,
        rotation_matrices, num_rotations,
        grid_min, grid_cell_size, grid_dims,
        grid_cell_counts, grid_cell_starts, grid_vertex_indices,
        target_x, target_y, target_z,
        cutoff_sq, scores
    );

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("[RotationSearch] Distance energy kernel error: %s\n",
               cudaGetErrorString(err));
    }
    cudaDeviceSynchronize();
}
