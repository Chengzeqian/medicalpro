#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <cmath>

// ============================================================================
// GICP Registration CUDA Kernels
// ============================================================================

#define BLOCK_SIZE 256
#define EMPTY_CELL_VALUE 0xFFFFFFFF

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            printf("CUDA error at %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(err)); \
            return; \
        } \
    } while(0)

// ============================================================================
// Kernel: Transform Source Points and Find Correspondences
// ============================================================================

__global__ void findCorrespondencesKernel(
    // Source points
    const float* __restrict__ source_x,
    const float* __restrict__ source_y,
    const float* __restrict__ source_z,
    // Transform matrix (row-major, 4x4)
    const float* __restrict__ transform,
    // Target grid
    const uint32_t* __restrict__ cell_vertex_id,
    const float* __restrict__ target_x,
    const float* __restrict__ target_y,
    const float* __restrict__ target_z,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    // Output
    float* __restrict__ transformed_x,
    float* __restrict__ transformed_y,
    float* __restrict__ transformed_z,
    uint32_t* __restrict__ corr_target_idx,
    float* __restrict__ corr_distance,
    // Params
    uint32_t num_points,
    int search_radius
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_points) return;

    // Load source point
    float sx = source_x[idx];
    float sy = source_y[idx];
    float sz = source_z[idx];

    // Apply transform: p' = T * p
    // T is row-major: T[row*4 + col]
    float tx = transform[0] * sx + transform[1] * sy + transform[2] * sz + transform[3];
    float ty = transform[4] * sx + transform[5] * sy + transform[6] * sz + transform[7];
    float tz = transform[8] * sx + transform[9] * sy + transform[10] * sz + transform[11];

    // Store transformed point
    transformed_x[idx] = tx;
    transformed_y[idx] = ty;
    transformed_z[idx] = tz;

    // Compute cell index for transformed point
    int cx = (int)((tx - origin_x) / cell_size);
    int cy = (int)((ty - origin_y) / cell_size);
    int cz = (int)((tz - origin_z) / cell_size);

    // Check if inside grid bounds
    bool inside_grid = (cx >= -search_radius && cx < dim_x + search_radius &&
                        cy >= -search_radius && cy < dim_y + search_radius &&
                        cz >= -search_radius && cz < dim_z + search_radius);

    uint32_t best_vid = EMPTY_CELL_VALUE;
    float best_dist_sq = 1e30f;

    if (inside_grid) {
        // Clamp center cell
        int cx_clamped = max(0, min(cx, dim_x - 1));
        int cy_clamped = max(0, min(cy, dim_y - 1));
        int cz_clamped = max(0, min(cz, dim_z - 1));

        // Search in neighborhood
        for (int dz = -search_radius; dz <= search_radius; ++dz) {
            for (int dy = -search_radius; dy <= search_radius; ++dy) {
                for (int dx = -search_radius; dx <= search_radius; ++dx) {
                    int nx = cx_clamped + dx;
                    int ny = cy_clamped + dy;
                    int nz = cz_clamped + dz;

                    if (nx >= 0 && nx < dim_x && ny >= 0 && ny < dim_y && nz >= 0 && nz < dim_z) {
                        uint32_t cell_idx = nz * dim_y * dim_x + ny * dim_x + nx;
                        uint32_t vid = cell_vertex_id[cell_idx];

                        if (vid != EMPTY_CELL_VALUE) {
                            float vx = target_x[vid];
                            float vy = target_y[vid];
                            float vz = target_z[vid];

                            float dist_sq = (vx - tx) * (vx - tx) +
                                           (vy - ty) * (vy - ty) +
                                           (vz - tz) * (vz - tz);

                            if (dist_sq < best_dist_sq) {
                                best_dist_sq = dist_sq;
                                best_vid = vid;
                            }
                        }
                    }
                }
            }
        }
    }

    corr_target_idx[idx] = best_vid;
    corr_distance[idx] = (best_vid != EMPTY_CELL_VALUE) ? sqrtf(best_dist_sq) : -1.0f;
}

extern "C" void launchFindCorrespondences(
    const float* source_x, const float* source_y, const float* source_z,
    const float* transform_matrix,
    const uint32_t* cell_vertex_id,
    const float* target_x, const float* target_y, const float* target_z,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    float* transformed_x, float* transformed_y, float* transformed_z,
    uint32_t* corr_target_idx,
    float* corr_distance,
    uint32_t num_points,
    int search_radius
) {
    // Copy transform to device
    float* d_transform;
    cudaMalloc(&d_transform, 16 * sizeof(float));
    cudaMemcpy(d_transform, transform_matrix, 16 * sizeof(float), cudaMemcpyHostToDevice);

    int num_blocks = (num_points + BLOCK_SIZE - 1) / BLOCK_SIZE;
    findCorrespondencesKernel<<<num_blocks, BLOCK_SIZE>>>(
        source_x, source_y, source_z,
        d_transform,
        cell_vertex_id,
        target_x, target_y, target_z,
        origin_x, origin_y, origin_z,
        cell_size, dim_x, dim_y, dim_z,
        transformed_x, transformed_y, transformed_z,
        corr_target_idx, corr_distance,
        num_points, search_radius
    );
    cudaDeviceSynchronize();

    cudaFree(d_transform);
}

// ============================================================================
// Kernel: Count Valid Correspondences (Simple CPU reduction for now)
// ============================================================================

extern "C" uint32_t launchCountValidCorrespondences(
    const uint32_t* corr_target_idx,
    uint32_t num_points
) {
    // Download and count on CPU (for simplicity in V0.1)
    std::vector<uint32_t> h_idx(num_points);
    cudaMemcpy(h_idx.data(), corr_target_idx, num_points * sizeof(uint32_t), cudaMemcpyDeviceToHost);

    uint32_t count = 0;
    for (uint32_t i = 0; i < num_points; ++i) {
        if (h_idx[i] != EMPTY_CELL_VALUE) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Kernel: Compute Correspondence Statistics
// ============================================================================

extern "C" void launchComputeCorrespondenceStats(
    const uint32_t* corr_target_idx,
    const float* corr_distance,
    uint32_t num_points,
    uint32_t* num_valid,
    float* mean_distance,
    float* max_distance,
    float* rmse
) {
    // Download and compute on CPU (for simplicity in V0.1)
    std::vector<uint32_t> h_idx(num_points);
    std::vector<float> h_dist(num_points);
    cudaMemcpy(h_idx.data(), corr_target_idx, num_points * sizeof(uint32_t), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_dist.data(), corr_distance, num_points * sizeof(float), cudaMemcpyDeviceToHost);

    uint32_t valid_count = 0;
    float sum_dist = 0.0f;
    float sum_dist_sq = 0.0f;
    float max_dist = 0.0f;

    for (uint32_t i = 0; i < num_points; ++i) {
        if (h_idx[i] != EMPTY_CELL_VALUE && h_dist[i] >= 0.0f) {
            valid_count++;
            sum_dist += h_dist[i];
            sum_dist_sq += h_dist[i] * h_dist[i];
            if (h_dist[i] > max_dist) {
                max_dist = h_dist[i];
            }
        }
    }

    *num_valid = valid_count;
    *mean_distance = (valid_count > 0) ? (sum_dist / valid_count) : 0.0f;
    *max_distance = max_dist;
    *rmse = (valid_count > 0) ? sqrtf(sum_dist_sq / valid_count) : 0.0f;
}

// ============================================================================
// Kernel: Compute JtJ and Jtb for Point-to-Plane ICP
// ============================================================================
// For Point-to-Plane ICP, the error for correspondence i is:
//   e_i = (p_i' - q_i) . n_i
// where p_i' = T * p_i (transformed source), q_i = target point, n_i = target normal
//
// The Jacobian w.r.t. small perturbation [rx, ry, rz, tx, ty, tz] is:
//   J_i = [ (p_i' x n_i), n_i ]  (1x6 vector)
//
// We accumulate:
//   JtJ += J_i^T * J_i   (6x6 symmetric matrix, stored as 21 upper triangular)
//   Jtb += J_i^T * e_i   (6x1 vector)

__global__ void computeJtJAndJtbKernel(
    const float* __restrict__ transformed_x,
    const float* __restrict__ transformed_y,
    const float* __restrict__ transformed_z,
    const float* __restrict__ target_x,
    const float* __restrict__ target_y,
    const float* __restrict__ target_z,
    const float* __restrict__ target_nx,
    const float* __restrict__ target_ny,
    const float* __restrict__ target_nz,
    const uint32_t* __restrict__ corr_target_idx,
    const float* __restrict__ corr_distance,
    uint32_t num_points,
    float distance_threshold,
    float* __restrict__ d_JtJ,   // [21]
    float* __restrict__ d_Jtb    // [6]
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_points) return;

    uint32_t target_idx = corr_target_idx[idx];

    // Skip invalid correspondences
    if (target_idx == EMPTY_CELL_VALUE) return;

    float dist = corr_distance[idx];
    if (dist < 0.0f || dist > distance_threshold) return;

    // Get transformed source point p'
    float px = transformed_x[idx];
    float py = transformed_y[idx];
    float pz = transformed_z[idx];

    // Get target point q and normal n
    float qx = target_x[target_idx];
    float qy = target_y[target_idx];
    float qz = target_z[target_idx];

    float nx = target_nx[target_idx];
    float ny = target_ny[target_idx];
    float nz = target_nz[target_idx];

    // Compute error: e = (p' - q) . n
    float dx = px - qx;
    float dy = py - qy;
    float dz = pz - qz;
    float error = dx * nx + dy * ny + dz * nz;

    // Compute Jacobian J = [ (p' x n), n ]
    // p' x n = (py*nz - pz*ny, pz*nx - px*nz, px*ny - py*nx)
    float J[6];
    J[0] = py * nz - pz * ny;  // rx
    J[1] = pz * nx - px * nz;  // ry
    J[2] = px * ny - py * nx;  // rz
    J[3] = nx;                  // tx
    J[4] = ny;                  // ty
    J[5] = nz;                  // tz

    // Accumulate JtJ (upper triangular, 21 elements)
    // JtJ[i,j] = sum(J[i] * J[j]) for i <= j
    // Layout: [0,0], [0,1], [0,2], [0,3], [0,4], [0,5],
    //               [1,1], [1,2], [1,3], [1,4], [1,5],
    //                     [2,2], [2,3], [2,4], [2,5],
    //                           [3,3], [3,4], [3,5],
    //                                 [4,4], [4,5],
    //                                       [5,5]
    int k = 0;
    for (int i = 0; i < 6; ++i) {
        for (int j = i; j < 6; ++j) {
            atomicAdd(&d_JtJ[k], J[i] * J[j]);
            k++;
        }
    }

    // Accumulate Jtb
    for (int i = 0; i < 6; ++i) {
        atomicAdd(&d_Jtb[i], J[i] * error);
    }
}

extern "C" void launchComputeJtJAndJtb(
    const float* transformed_x, const float* transformed_y, const float* transformed_z,
    const float* target_x, const float* target_y, const float* target_z,
    const float* target_nx, const float* target_ny, const float* target_nz,
    const uint32_t* corr_target_idx,
    const float* corr_distance,
    uint32_t num_points,
    float distance_threshold,
    float* d_JtJ,
    float* d_Jtb
) {
    // Initialize JtJ and Jtb to zero
    cudaMemset(d_JtJ, 0, 21 * sizeof(float));
    cudaMemset(d_Jtb, 0, 6 * sizeof(float));

    int num_blocks = (num_points + BLOCK_SIZE - 1) / BLOCK_SIZE;
    computeJtJAndJtbKernel<<<num_blocks, BLOCK_SIZE>>>(
        transformed_x, transformed_y, transformed_z,
        target_x, target_y, target_z,
        target_nx, target_ny, target_nz,
        corr_target_idx, corr_distance,
        num_points, distance_threshold,
        d_JtJ, d_Jtb
    );
    cudaDeviceSynchronize();
}

// ============================================================================
// Kernel: Compute JtJ and Jtb with Curvature Weighting
// ============================================================================
// Curvature weighting modes:
//   0 = NONE (weight = 1)
//   1 = HIGH_CURVATURE (w = 1 + scale * |curvature|)
//   2 = LOW_CURVATURE  (w = 1 / (1 + scale * |curvature|))
//   3 = GAUSSIAN_AWARE (uses Gaussian curvature for surface type)
//   4 = ADAPTIVE (balanced weighting)

__device__ float computeCurvatureWeight(
    float mean_curv,
    float gaussian_curv,
    int weight_mode,
    float weight_scale,
    float min_weight,
    float max_weight
) {
    float weight = 1.0f;
    float abs_curv = fabsf(mean_curv);

    switch (weight_mode) {
        case 0: // NONE
            weight = 1.0f;
            break;

        case 1: // HIGH_CURVATURE - prefer edges and corners
            // Higher curvature = higher weight
            weight = 1.0f + weight_scale * abs_curv;
            break;

        case 2: // LOW_CURVATURE - prefer flat surfaces
            // Higher curvature = lower weight
            weight = 1.0f / (1.0f + weight_scale * abs_curv);
            break;

        case 3: // GAUSSIAN_AWARE - surface type detection
            // K > 0: elliptic (bowl/dome) - stable, high weight
            // K < 0: hyperbolic (saddle) - feature, high weight
            // K ≈ 0: parabolic/planar - moderate weight
            if (gaussian_curv > 0.01f) {
                // Elliptic surface - stable reference
                weight = 1.0f + weight_scale * 0.5f;
            } else if (gaussian_curv < -0.01f) {
                // Hyperbolic surface - distinctive feature
                weight = 1.0f + weight_scale * fabsf(gaussian_curv);
            } else {
                // Near-planar or parabolic
                weight = 1.0f + weight_scale * abs_curv * 0.3f;
            }
            break;

        case 4: // ADAPTIVE - balanced approach
            // Combine information from both curvatures
            // High mean curvature OR distinctive Gaussian curvature = higher weight
            {
                float mean_factor = abs_curv;
                float gauss_factor = fabsf(gaussian_curv);
                float combined = sqrtf(mean_factor * mean_factor + gauss_factor * gauss_factor);
                weight = 1.0f + weight_scale * combined;
            }
            break;

        default:
            weight = 1.0f;
            break;
    }

    // Clamp weight to valid range
    weight = fmaxf(min_weight, fminf(max_weight, weight));

    return weight;
}

__global__ void computeJtJAndJtbWeightedKernel(
    const float* __restrict__ transformed_x,
    const float* __restrict__ transformed_y,
    const float* __restrict__ transformed_z,
    const float* __restrict__ target_x,
    const float* __restrict__ target_y,
    const float* __restrict__ target_z,
    const float* __restrict__ target_nx,
    const float* __restrict__ target_ny,
    const float* __restrict__ target_nz,
    const float* __restrict__ target_curvature,
    const float* __restrict__ target_gaussian_curv,
    const uint32_t* __restrict__ corr_target_idx,
    const float* __restrict__ corr_distance,
    uint32_t num_points,
    float distance_threshold,
    int weight_mode,
    float weight_scale,
    float min_weight,
    float max_weight,
    float* __restrict__ d_JtJ,
    float* __restrict__ d_Jtb,
    // Coverage-aware weighting (optional)
    const uint32_t* __restrict__ source_cell_counts,
    float grid_origin_x, float grid_origin_y, float grid_origin_z,
    float grid_cell_size,
    int grid_dim_x, int grid_dim_y, int grid_dim_z,
    float coverage_density_exponent
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_points) return;

    uint32_t target_idx = corr_target_idx[idx];

    // Skip invalid correspondences
    if (target_idx == EMPTY_CELL_VALUE) return;

    float dist = corr_distance[idx];
    if (dist < 0.0f || dist > distance_threshold) return;

    // Get transformed source point p'
    float px = transformed_x[idx];
    float py = transformed_y[idx];
    float pz = transformed_z[idx];

    // Get target point q and normal n
    float qx = target_x[target_idx];
    float qy = target_y[target_idx];
    float qz = target_z[target_idx];

    float nx = target_nx[target_idx];
    float ny = target_ny[target_idx];
    float nz = target_nz[target_idx];

    // Get curvature values for weighting
    float mean_curv = target_curvature[target_idx];
    float gauss_curv = target_gaussian_curv[target_idx];

    // Compute curvature-based weight
    float weight = computeCurvatureWeight(mean_curv, gauss_curv, weight_mode,
                                          weight_scale, min_weight, max_weight);

    // Apply coverage-aware weighting (inverse source density)
    if (source_cell_counts != nullptr && grid_cell_size > 0.0f) {
        int scx = (int)((px - grid_origin_x) / grid_cell_size);
        int scy = (int)((py - grid_origin_y) / grid_cell_size);
        int scz = (int)((pz - grid_origin_z) / grid_cell_size);
        scx = max(0, min(scx, grid_dim_x - 1));
        scy = max(0, min(scy, grid_dim_y - 1));
        scz = max(0, min(scz, grid_dim_z - 1));
        uint32_t cell_idx = scz * grid_dim_y * grid_dim_x + scy * grid_dim_x + scx;
        float density = (float)max(1u, source_cell_counts[cell_idx]);
        float coverage_weight = 1.0f / powf(density, coverage_density_exponent);
        weight *= coverage_weight;
        // Re-clamp after coverage modulation
        weight = fmaxf(min_weight, fminf(max_weight, weight));
    }

    // Compute error: e = (p' - q) . n
    float dx = px - qx;
    float dy = py - qy;
    float dz = pz - qz;
    float error = dx * nx + dy * ny + dz * nz;

    // Compute Jacobian J = [ (p' x n), n ]
    float J[6];
    J[0] = py * nz - pz * ny;  // rx
    J[1] = pz * nx - px * nz;  // ry
    J[2] = px * ny - py * nx;  // rz
    J[3] = nx;                  // tx
    J[4] = ny;                  // ty
    J[5] = nz;                  // tz

    // Accumulate weighted JtJ (upper triangular, 21 elements)
    // JtWJ[i,j] = sum(w * J[i] * J[j]) for i <= j
    int k = 0;
    for (int i = 0; i < 6; ++i) {
        for (int j = i; j < 6; ++j) {
            atomicAdd(&d_JtJ[k], weight * J[i] * J[j]);
            k++;
        }
    }

    // Accumulate weighted Jtb
    // JtWb[i] = sum(w * J[i] * error)
    for (int i = 0; i < 6; ++i) {
        atomicAdd(&d_Jtb[i], weight * J[i] * error);
    }
}

extern "C" void launchComputeJtJAndJtbWeighted(
    const float* transformed_x, const float* transformed_y, const float* transformed_z,
    const float* target_x, const float* target_y, const float* target_z,
    const float* target_nx, const float* target_ny, const float* target_nz,
    const float* target_curvature,
    const float* target_gaussian_curv,
    const uint32_t* corr_target_idx,
    const float* corr_distance,
    uint32_t num_points,
    float distance_threshold,
    int weight_mode,
    float weight_scale,
    float min_weight,
    float max_weight,
    float* d_JtJ,
    float* d_Jtb,
    const uint32_t* source_cell_counts,
    float grid_origin_x, float grid_origin_y, float grid_origin_z,
    float grid_cell_size,
    int grid_dim_x, int grid_dim_y, int grid_dim_z,
    float coverage_density_exponent
) {
    // Initialize JtJ and Jtb to zero
    cudaMemset(d_JtJ, 0, 21 * sizeof(float));
    cudaMemset(d_Jtb, 0, 6 * sizeof(float));

    int num_blocks = (num_points + BLOCK_SIZE - 1) / BLOCK_SIZE;
    computeJtJAndJtbWeightedKernel<<<num_blocks, BLOCK_SIZE>>>(
        transformed_x, transformed_y, transformed_z,
        target_x, target_y, target_z,
        target_nx, target_ny, target_nz,
        target_curvature, target_gaussian_curv,
        corr_target_idx, corr_distance,
        num_points, distance_threshold,
        weight_mode, weight_scale, min_weight, max_weight,
        d_JtJ, d_Jtb,
        source_cell_counts,
        grid_origin_x, grid_origin_y, grid_origin_z,
        grid_cell_size,
        grid_dim_x, grid_dim_y, grid_dim_z,
        coverage_density_exponent
    );
    cudaDeviceSynchronize();
}

// ============================================================================
// Kernel: Compute Source Point Density Per Grid Cell
// ============================================================================

__global__ void computeSourceDensityKernel(
    const float* __restrict__ transformed_x,
    const float* __restrict__ transformed_y,
    const float* __restrict__ transformed_z,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    uint32_t num_points,
    uint32_t* __restrict__ source_cell_counts
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_points) return;

    float px = transformed_x[idx];
    float py = transformed_y[idx];
    float pz = transformed_z[idx];

    int cx = (int)((px - origin_x) / cell_size);
    int cy = (int)((py - origin_y) / cell_size);
    int cz = (int)((pz - origin_z) / cell_size);

    cx = max(0, min(cx, dim_x - 1));
    cy = max(0, min(cy, dim_y - 1));
    cz = max(0, min(cz, dim_z - 1));

    uint32_t cell_idx = cz * dim_y * dim_x + cy * dim_x + cx;
    atomicAdd(&source_cell_counts[cell_idx], 1u);
}

extern "C" void launchComputeSourceDensity(
    const float* transformed_x, const float* transformed_y, const float* transformed_z,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    uint32_t num_points,
    uint32_t* source_cell_counts
) {
    uint32_t total_cells = (uint32_t)dim_x * dim_y * dim_z;
    cudaMemset(source_cell_counts, 0, total_cells * sizeof(uint32_t));

    int num_blocks = (num_points + BLOCK_SIZE - 1) / BLOCK_SIZE;
    computeSourceDensityKernel<<<num_blocks, BLOCK_SIZE>>>(
        transformed_x, transformed_y, transformed_z,
        origin_x, origin_y, origin_z,
        cell_size, dim_x, dim_y, dim_z,
        num_points, source_cell_counts
    );
    cudaDeviceSynchronize();
}
