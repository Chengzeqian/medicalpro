// Include cuda_runtime.h FIRST to get CUDA types before types.h defines fallbacks
#include <cuda_runtime.h>
#include "rotation_search.h"
#include <iostream>
#include <algorithm>
#include <cmath>

RotationSearch::RotationSearch(MeshGPU& mesh_gpu, int grid_level)
    : mesh_gpu_(mesh_gpu)
    , grid_level_(grid_level)
    , d_rotation_matrices_(nullptr)
    , d_scores_(nullptr)
    , max_rotations_(0)
{
    // Pre-allocate for 3D search: 360/1 * 13*13 = 360*169 = 60840 rotations (worst case)
    // For typical use: 360/2 * 7*7 = 180*49 = 8820
    // Allocate 65000 for safety with fine search
    max_rotations_ = 65000;

    cudaMalloc(&d_rotation_matrices_, max_rotations_ * 16 * sizeof(float));
    cudaMalloc(&d_scores_, max_rotations_ * sizeof(int));

    std::cout << "[RotationSearch] Initialized with max " << max_rotations_ << " rotations" << std::endl;
}

RotationSearch::~RotationSearch() {
    if (d_rotation_matrices_) cudaFree(d_rotation_matrices_);
    if (d_scores_) cudaFree(d_scores_);
}

Matrix4x4 RotationSearch::createRotationAroundPoint(
    const float3_t& center,
    float angle_x_deg,
    float angle_y_deg,
    float angle_z_deg
) {
    // Convert to radians
    float ax = angle_x_deg * (float)M_PI / 180.0f;
    float ay = angle_y_deg * (float)M_PI / 180.0f;
    float az = angle_z_deg * (float)M_PI / 180.0f;

    // Rotation matrices
    float cx = cosf(ax), sx = sinf(ax);
    float cy = cosf(ay), sy = sinf(ay);
    float cz = cosf(az), sz = sinf(az);

    // Combined rotation: Rz * Ry * Rx
    // Row-major storage
    float r00 = cy * cz;
    float r01 = -cy * sz;
    float r02 = sy;
    float r10 = sx * sy * cz + cx * sz;
    float r11 = -sx * sy * sz + cx * cz;
    float r12 = -sx * cy;
    float r20 = -cx * sy * cz + sx * sz;
    float r21 = cx * sy * sz + sx * cz;
    float r22 = cx * cy;

    // To rotate around a point P:
    // T = T(P) * R * T(-P)
    // Result: p' = R * (p - P) + P = R*p + (P - R*P)

    float tx = center.x - (r00 * center.x + r01 * center.y + r02 * center.z);
    float ty = center.y - (r10 * center.x + r11 * center.y + r12 * center.z);
    float tz = center.z - (r20 * center.x + r21 * center.y + r22 * center.z);

    Matrix4x4 M;
    // Row 0
    M.m[0] = r00; M.m[1] = r01; M.m[2] = r02; M.m[3] = tx;
    // Row 1
    M.m[4] = r10; M.m[5] = r11; M.m[6] = r12; M.m[7] = ty;
    // Row 2
    M.m[8] = r20; M.m[9] = r21; M.m[10] = r22; M.m[11] = tz;
    // Row 3
    M.m[12] = 0;  M.m[13] = 0;  M.m[14] = 0;  M.m[15] = 1;

    return M;
}

std::vector<Matrix4x4> RotationSearch::generateCandidateRotations(
    const float3_t& anchor_point,
    const SearchParams& params
) {
    std::vector<Matrix4x4> rotations;

    // Generate Z-axis rotations (primary search)
    std::vector<float> z_angles;
    for (float az = params.z_angle_min; az < params.z_angle_max; az += params.z_angle_step) {
        z_angles.push_back(az);
    }

    if (params.enable_xy_search) {
        // Generate X/Y fine-tuning angles
        std::vector<float> xy_angles;
        for (float a = -params.xy_angle_range; a <= params.xy_angle_range; a += params.xy_angle_step) {
            xy_angles.push_back(a);
        }

        // Combine Z with X/Y
        for (float az : z_angles) {
            for (float ax : xy_angles) {
                for (float ay : xy_angles) {
                    rotations.push_back(createRotationAroundPoint(anchor_point, ax, ay, az));
                }
            }
        }
    } else {
        // Only Z-axis rotation
        for (float az : z_angles) {
            rotations.push_back(createRotationAroundPoint(anchor_point, 0, 0, az));
        }
    }

    return rotations;
}

RotationSearch::SearchResult RotationSearch::search(
    const float* d_source_x,
    const float* d_source_y,
    const float* d_source_z,
    uint32_t num_points,
    const float3_t& anchor_point,
    const SearchParams& params
) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Rotation Search (Brute Force on GPU)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Anchor point (rotation center): (" << anchor_point.x << ", "
              << anchor_point.y << ", " << anchor_point.z << ")" << std::endl;
    std::cout << "  Z-axis search: [" << params.z_angle_min << ", " << params.z_angle_max
              << ") step=" << params.z_angle_step << " deg" << std::endl;
    if (params.enable_xy_search) {
        std::cout << "  X/Y fine-tuning: +/- " << params.xy_angle_range
                  << " step=" << params.xy_angle_step << " deg" << std::endl;
    }

    // Generate candidate rotation matrices
    std::vector<Matrix4x4> rotations = generateCandidateRotations(anchor_point, params);
    uint32_t num_rotations = (uint32_t)rotations.size();

    std::cout << "  Total rotation candidates: " << num_rotations << std::endl;

    if (num_rotations > max_rotations_) {
        std::cerr << "[RotationSearch] Too many rotations! Increase max_rotations_" << std::endl;
        return SearchResult();
    }

    // Flatten matrices for GPU (row-major, 16 floats each)
    std::vector<float> h_matrices(num_rotations * 16);
    for (uint32_t i = 0; i < num_rotations; i++) {
        const Matrix4x4& M = rotations[i];
        for (int j = 0; j < 16; j++) {
            h_matrices[i * 16 + j] = M.m[j];
        }
    }

    // Upload to GPU
    cudaMemcpy(d_rotation_matrices_, h_matrices.data(),
               num_rotations * 16 * sizeof(float), cudaMemcpyHostToDevice);

    // Clear scores
    cudaMemset(d_scores_, 0, num_rotations * sizeof(int));

    // Get grid info from MeshGPU (supports multi-resolution)
    float3 grid_min;
    float grid_cell_size;
    int3 grid_dims;
    const uint32_t* d_cell_counts;
    const uint32_t* d_cell_starts;
    const uint32_t* d_vertex_indices;

    if (grid_level_ >= 0 && mesh_gpu_.hasMultiResGrid()) {
        const GridIndex& g = mesh_gpu_.getMultiResGrid().getLevel(grid_level_);
        grid_min = make_float3(g.origin.x, g.origin.y, g.origin.z);
        grid_cell_size = g.cell_size;
        grid_dims = make_int3(g.dim_x, g.dim_y, g.dim_z);
        d_cell_counts = g.cell_vertex_count;
        d_cell_starts = g.cell_starts;
        d_vertex_indices = g.vertex_indices;
    } else {
        grid_min = mesh_gpu_.getGridMin();
        grid_cell_size = mesh_gpu_.getGridCellSize();
        grid_dims = mesh_gpu_.getGridDims();
        d_cell_counts = mesh_gpu_.getGridCellCounts();
        d_cell_starts = mesh_gpu_.getGridCellStarts();
        d_vertex_indices = mesh_gpu_.getGridVertexIndices();
    }

    const float* d_target_x = mesh_gpu_.getVerticesX();
    const float* d_target_y = mesh_gpu_.getVerticesY();
    const float* d_target_z = mesh_gpu_.getVerticesZ();
    const float cutoff_mm = grid_cell_size * 3.0f;

    std::cout << "  Grid: " << grid_dims.x << " x " << grid_dims.y << " x " << grid_dims.z
              << " (cell=" << grid_cell_size << "mm, cutoff=" << cutoff_mm << "mm)" << std::endl;

    // Launch distance energy kernel
    launchRotationDistanceEnergyKernel(
        d_source_x, d_source_y, d_source_z, num_points,
        d_rotation_matrices_, num_rotations,
        grid_min, grid_cell_size, grid_dims,
        d_cell_counts, d_cell_starts, d_vertex_indices,
        d_target_x, d_target_y, d_target_z,
        cutoff_mm, d_scores_
    );

    // Download scores
    std::vector<int> h_scores(num_rotations);
    cudaMemcpy(h_scores.data(), d_scores_, num_rotations * sizeof(int), cudaMemcpyDeviceToHost);

    // Find best rotation
    int best_idx = 0;
    int best_score = h_scores[0];
    for (uint32_t i = 1; i < num_rotations; i++) {
        if (h_scores[i] > best_score) {
            best_score = h_scores[i];
            best_idx = i;
        }
    }

    // Compute angles for best rotation
    float best_z = params.z_angle_min + (best_idx % (int)((params.z_angle_max - params.z_angle_min) / params.z_angle_step)) * params.z_angle_step;
    float best_x = 0, best_y = 0;

    if (params.enable_xy_search) {
        int num_z = (int)((params.z_angle_max - params.z_angle_min) / params.z_angle_step);
        int num_xy = (int)(2 * params.xy_angle_range / params.xy_angle_step) + 1;

        int z_idx = best_idx / (num_xy * num_xy);
        int remainder = best_idx % (num_xy * num_xy);
        int x_idx = remainder / num_xy;
        int y_idx = remainder % num_xy;

        best_z = params.z_angle_min + z_idx * params.z_angle_step;
        best_x = -params.xy_angle_range + x_idx * params.xy_angle_step;
        best_y = -params.xy_angle_range + y_idx * params.xy_angle_step;
    }

    // Build result
    SearchResult result;
    result.best_rotation = rotations[best_idx];
    result.best_angle_z = best_z;
    result.best_angle_x = best_x;
    result.best_angle_y = best_y;
    result.best_score = best_score;
    result.total_points = num_points;
    float mean_energy = (float)best_score / (num_points * 1000.0f);
    result.mean_dist_mm = std::sqrt(-mean_energy);

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  Best rotation: Z=" << result.best_angle_z << " deg";
    if (params.enable_xy_search) {
        std::cout << ", X=" << result.best_angle_x << ", Y=" << result.best_angle_y;
    }
    std::cout << std::endl;
    std::cout << "  Score: mean_dist=" << result.mean_dist_mm << "mm" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return result;
}

// ============================================================================
// Hierarchical Coarse-to-Fine Search (100x faster!)
// ============================================================================
RotationSearch::SearchResult RotationSearch::searchHierarchical(
    const float* d_source_x,
    const float* d_source_y,
    const float* d_source_z,
    uint32_t num_points,
    const float3_t& anchor_point,
    const SearchParams& params
) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Hierarchical Rotation Search (Coarse-to-Fine)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Anchor point: (" << anchor_point.x << ", "
              << anchor_point.y << ", " << anchor_point.z << ")" << std::endl;

    // Get grid info
    float3 grid_min = mesh_gpu_.getGridMin();
    float grid_cell_size = mesh_gpu_.getGridCellSize();
    int3 grid_dims = mesh_gpu_.getGridDims();
    const uint32_t* d_cell_counts = mesh_gpu_.getGridCellCounts();
    const uint32_t* d_cell_starts = mesh_gpu_.getGridCellStarts();
    const uint32_t* d_vertex_indices = mesh_gpu_.getGridVertexIndices();
    const float* d_target_x = mesh_gpu_.getVerticesX();
    const float* d_target_y = mesh_gpu_.getVerticesY();
    const float* d_target_z = mesh_gpu_.getVerticesZ();
    const float cutoff_mm = grid_cell_size * 3.0f;

    // ========================================
    // Stage 1: Coarse Z-axis search only
    // ========================================
    const float coarse_z_step = 5.0f;  // 72 candidates for full 360 degrees
    int num_z_coarse = (int)((params.z_angle_max - params.z_angle_min) / coarse_z_step);

    std::cout << "\n--- Stage 1: Coarse Z Search ---" << std::endl;
    std::cout << "  Z range: [" << params.z_angle_min << ", " << params.z_angle_max
              << ") step=" << coarse_z_step << " deg" << std::endl;
    std::cout << "  Candidates: " << num_z_coarse << std::endl;

    // Generate coarse Z rotations
    std::vector<Matrix4x4> coarse_rotations;
    std::vector<float> coarse_z_angles;
    for (float az = params.z_angle_min; az < params.z_angle_max; az += coarse_z_step) {
        coarse_rotations.push_back(createRotationAroundPoint(anchor_point, 0, 0, az));
        coarse_z_angles.push_back(az);
    }

    // Upload and evaluate
    std::vector<float> h_matrices(coarse_rotations.size() * 16);
    for (size_t i = 0; i < coarse_rotations.size(); i++) {
        for (int j = 0; j < 16; j++) {
            h_matrices[i * 16 + j] = coarse_rotations[i].m[j];
        }
    }

    cudaMemcpy(d_rotation_matrices_, h_matrices.data(),
               coarse_rotations.size() * 16 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemset(d_scores_, 0, coarse_rotations.size() * sizeof(int));

    // Time Stage 1
    cudaEvent_t start1, stop1;
    cudaEventCreate(&start1);
    cudaEventCreate(&stop1);
    cudaEventRecord(start1);

    launchRotationDistanceEnergyKernel(
        d_source_x, d_source_y, d_source_z, num_points,
        d_rotation_matrices_, (uint32_t)coarse_rotations.size(),
        grid_min, grid_cell_size, grid_dims,
        d_cell_counts, d_cell_starts, d_vertex_indices,
        d_target_x, d_target_y, d_target_z,
        cutoff_mm, d_scores_
    );

    cudaEventRecord(stop1);
    cudaEventSynchronize(stop1);
    float ms1 = 0;
    cudaEventElapsedTime(&ms1, start1, stop1);

    // Find best coarse Z
    std::vector<int> h_scores(coarse_rotations.size());
    cudaMemcpy(h_scores.data(), d_scores_, coarse_rotations.size() * sizeof(int), cudaMemcpyDeviceToHost);

    int best_coarse_idx = 0;
    int best_coarse_score = h_scores[0];
    for (size_t i = 1; i < h_scores.size(); i++) {
        if (h_scores[i] > best_coarse_score) {
            best_coarse_score = h_scores[i];
            best_coarse_idx = (int)i;
        }
    }

    float best_coarse_z = coarse_z_angles[best_coarse_idx];
    {
        float me = (float)best_coarse_score / (num_points * 1000.0f);
        std::cout << "  Best coarse Z: " << best_coarse_z << " deg (mean_dist="
                  << std::sqrt(-me) << "mm)" << std::endl;
    }
    std::cout << "  Stage 1 time: " << ms1 << " ms" << std::endl;

    // ========================================
    // Stage 2: Fine local 3D search
    // ========================================
    const float fine_z_range = 10.0f;  // +/- 10 degrees around best Z
    const float fine_z_step = 2.0f;    // 2 degree step

    std::cout << "\n--- Stage 2: Fine 3D Search ---" << std::endl;
    std::cout << "  Z range: " << best_coarse_z << " +/- " << fine_z_range
              << " step=" << fine_z_step << " deg" << std::endl;

    if (params.enable_xy_search) {
        std::cout << "  X/Y range: +/- " << params.xy_angle_range
                  << " step=" << params.xy_angle_step << " deg" << std::endl;
    }

    // Generate fine 3D rotations
    std::vector<Matrix4x4> fine_rotations;
    std::vector<float> fine_z_angles, fine_x_angles, fine_y_angles;

    for (float dz = -fine_z_range; dz <= fine_z_range; dz += fine_z_step) {
        float az = best_coarse_z + dz;
        // Wrap to [0, 360)
        while (az < 0) az += 360.0f;
        while (az >= 360.0f) az -= 360.0f;

        if (params.enable_xy_search) {
            for (float ax = -params.xy_angle_range; ax <= params.xy_angle_range; ax += params.xy_angle_step) {
                for (float ay = -params.xy_angle_range; ay <= params.xy_angle_range; ay += params.xy_angle_step) {
                    fine_rotations.push_back(createRotationAroundPoint(anchor_point, ax, ay, az));
                    fine_z_angles.push_back(az);
                    fine_x_angles.push_back(ax);
                    fine_y_angles.push_back(ay);
                }
            }
        } else {
            fine_rotations.push_back(createRotationAroundPoint(anchor_point, 0, 0, az));
            fine_z_angles.push_back(az);
            fine_x_angles.push_back(0);
            fine_y_angles.push_back(0);
        }
    }

    std::cout << "  Candidates: " << fine_rotations.size() << std::endl;

    // Upload and evaluate
    h_matrices.resize(fine_rotations.size() * 16);
    for (size_t i = 0; i < fine_rotations.size(); i++) {
        for (int j = 0; j < 16; j++) {
            h_matrices[i * 16 + j] = fine_rotations[i].m[j];
        }
    }

    cudaMemcpy(d_rotation_matrices_, h_matrices.data(),
               fine_rotations.size() * 16 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemset(d_scores_, 0, fine_rotations.size() * sizeof(int));

    // Time Stage 2
    cudaEvent_t start2, stop2;
    cudaEventCreate(&start2);
    cudaEventCreate(&stop2);
    cudaEventRecord(start2);

    launchRotationDistanceEnergyKernel(
        d_source_x, d_source_y, d_source_z, num_points,
        d_rotation_matrices_, (uint32_t)fine_rotations.size(),
        grid_min, grid_cell_size, grid_dims,
        d_cell_counts, d_cell_starts, d_vertex_indices,
        d_target_x, d_target_y, d_target_z,
        cutoff_mm, d_scores_
    );

    cudaEventRecord(stop2);
    cudaEventSynchronize(stop2);
    float ms2 = 0;
    cudaEventElapsedTime(&ms2, start2, stop2);

    // Find best fine rotation
    h_scores.resize(fine_rotations.size());
    cudaMemcpy(h_scores.data(), d_scores_, fine_rotations.size() * sizeof(int), cudaMemcpyDeviceToHost);

    int best_fine_idx = 0;
    int best_fine_score = h_scores[0];
    for (size_t i = 1; i < h_scores.size(); i++) {
        if (h_scores[i] > best_fine_score) {
            best_fine_score = h_scores[i];
            best_fine_idx = (int)i;
        }
    }

    std::cout << "  Stage 2 time: " << ms2 << " ms" << std::endl;

    // Clean up events
    cudaEventDestroy(start1);
    cudaEventDestroy(stop1);
    cudaEventDestroy(start2);
    cudaEventDestroy(stop2);

    // Build result
    SearchResult result;
    result.best_rotation = fine_rotations[best_fine_idx];
    result.best_angle_z = fine_z_angles[best_fine_idx];
    result.best_angle_x = fine_x_angles[best_fine_idx];
    result.best_angle_y = fine_y_angles[best_fine_idx];
    result.best_score = best_fine_score;
    result.total_points = num_points;
    float mean_energy = (float)best_fine_score / (num_points * 1000.0f);
    result.mean_dist_mm = std::sqrt(-mean_energy);

    float total_time = ms1 + ms2;
    int total_candidates = (int)(coarse_rotations.size() + fine_rotations.size());

    std::cout << "\n--- Result ---" << std::endl;
    std::cout << "  Best rotation: Z=" << result.best_angle_z << " deg";
    if (params.enable_xy_search) {
        std::cout << ", X=" << result.best_angle_x << ", Y=" << result.best_angle_y;
    }
    std::cout << std::endl;
    std::cout << "  Score: mean_dist=" << result.mean_dist_mm << "mm" << std::endl;
    std::cout << "  Total time: " << total_time << " ms (" << total_candidates << " candidates)" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return result;
}

// ============================================================================
// Full Sphere Coverage Search (Brute Force 3D)
// ============================================================================
RotationSearch::SearchResult RotationSearch::searchFullSphere(
    const float* d_source_x,
    const float* d_source_y,
    const float* d_source_z,
    uint32_t num_points,
    const float3_t& anchor_point,
    float angle_step_deg
) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Full Sphere Rotation Search (Brute Force 3D)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Anchor point: (" << anchor_point.x << ", "
              << anchor_point.y << ", " << anchor_point.z << ")" << std::endl;
    std::cout << "  Step size: " << angle_step_deg << " degrees" << std::endl;

    // Calculate number of steps per axis
    int num_steps = (int)(360.0f / angle_step_deg);
    int total_candidates = num_steps * num_steps * num_steps;

    std::cout << "  Candidates per axis: " << num_steps << std::endl;
    std::cout << "  Total candidates: " << total_candidates
              << " (" << num_steps << " x " << num_steps << " x " << num_steps << ")" << std::endl;

    if ((uint32_t)total_candidates > max_rotations_) {
        std::cerr << "[RotationSearch] Too many rotations! Max=" << max_rotations_ << std::endl;
        return SearchResult();
    }

    // Generate all 3D rotation combinations
    std::vector<Matrix4x4> rotations;
    std::vector<float> angles_x, angles_y, angles_z;
    rotations.reserve(total_candidates);
    angles_x.reserve(total_candidates);
    angles_y.reserve(total_candidates);
    angles_z.reserve(total_candidates);

    for (int ix = 0; ix < num_steps; ix++) {
        float ax = ix * angle_step_deg;
        for (int iy = 0; iy < num_steps; iy++) {
            float ay = iy * angle_step_deg;
            for (int iz = 0; iz < num_steps; iz++) {
                float az = iz * angle_step_deg;
                rotations.push_back(createRotationAroundPoint(anchor_point, ax, ay, az));
                angles_x.push_back(ax);
                angles_y.push_back(ay);
                angles_z.push_back(az);
            }
        }
    }

    // Flatten matrices for GPU
    std::vector<float> h_matrices(rotations.size() * 16);
    for (size_t i = 0; i < rotations.size(); i++) {
        for (int j = 0; j < 16; j++) {
            h_matrices[i * 16 + j] = rotations[i].m[j];
        }
    }

    // Upload to GPU
    cudaMemcpy(d_rotation_matrices_, h_matrices.data(),
               rotations.size() * 16 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemset(d_scores_, 0, rotations.size() * sizeof(int));

    // Get grid info
    float3 grid_min = mesh_gpu_.getGridMin();
    float grid_cell_size = mesh_gpu_.getGridCellSize();
    int3 grid_dims = mesh_gpu_.getGridDims();
    const uint32_t* d_cell_counts = mesh_gpu_.getGridCellCounts();
    const uint32_t* d_cell_starts = mesh_gpu_.getGridCellStarts();
    const uint32_t* d_vertex_indices = mesh_gpu_.getGridVertexIndices();
    const float* d_target_x = mesh_gpu_.getVerticesX();
    const float* d_target_y = mesh_gpu_.getVerticesY();
    const float* d_target_z = mesh_gpu_.getVerticesZ();
    const float cutoff_mm = grid_cell_size * 3.0f;

    // Time the kernel
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);

    // Launch kernel
    launchRotationDistanceEnergyKernel(
        d_source_x, d_source_y, d_source_z, num_points,
        d_rotation_matrices_, (uint32_t)rotations.size(),
        grid_min, grid_cell_size, grid_dims,
        d_cell_counts, d_cell_starts, d_vertex_indices,
        d_target_x, d_target_y, d_target_z,
        cutoff_mm, d_scores_
    );

    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    float elapsed_ms = 0;
    cudaEventElapsedTime(&elapsed_ms, start, stop);

    // Download scores
    std::vector<int> h_scores(rotations.size());
    cudaMemcpy(h_scores.data(), d_scores_, rotations.size() * sizeof(int), cudaMemcpyDeviceToHost);

    // Find best rotation
    int best_idx = 0;
    int best_score = h_scores[0];
    for (size_t i = 1; i < h_scores.size(); i++) {
        if (h_scores[i] > best_score) {
            best_score = h_scores[i];
            best_idx = (int)i;
        }
    }

    // Clean up events
    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    // Build result
    SearchResult result;
    result.best_rotation = rotations[best_idx];
    result.best_angle_x = angles_x[best_idx];
    result.best_angle_y = angles_y[best_idx];
    result.best_angle_z = angles_z[best_idx];
    result.best_score = best_score;
    result.total_points = num_points;
    {
        float me = (float)best_score / (num_points * 1000.0f);
        result.mean_dist_mm = std::sqrt(-me);
    }

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  GPU time: " << elapsed_ms << " ms" << std::endl;
    std::cout << "  Best rotation: X=" << result.best_angle_x
              << ", Y=" << result.best_angle_y
              << ", Z=" << result.best_angle_z << " deg" << std::endl;
    std::cout << "  Score: mean_dist=" << result.mean_dist_mm << "mm" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return result;
}

// ============================================================================
// Full Sphere with Hierarchical Refinement (BEST for unknown orientation)
// ============================================================================
RotationSearch::SearchResult RotationSearch::searchFullSphereHierarchical(
    const float* d_source_x,
    const float* d_source_y,
    const float* d_source_z,
    uint32_t num_points,
    const float3_t& anchor_point
) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Full Sphere Hierarchical Search" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Anchor point: (" << anchor_point.x << ", "
              << anchor_point.y << ", " << anchor_point.z << ")" << std::endl;

    // Get grid info
    float3 grid_min = mesh_gpu_.getGridMin();
    float grid_cell_size = mesh_gpu_.getGridCellSize();
    int3 grid_dims = mesh_gpu_.getGridDims();
    const uint32_t* d_cell_counts = mesh_gpu_.getGridCellCounts();
    const uint32_t* d_cell_starts = mesh_gpu_.getGridCellStarts();
    const uint32_t* d_vertex_indices = mesh_gpu_.getGridVertexIndices();
    const float* d_target_x = mesh_gpu_.getVerticesX();
    const float* d_target_y = mesh_gpu_.getVerticesY();
    const float* d_target_z = mesh_gpu_.getVerticesZ();
    const float cutoff_mm = grid_cell_size * 3.0f;

    // ========================================
    // Stage 1: Coarse full sphere search
    // ========================================
    const float coarse_step = 30.0f;  // 12 steps per axis -> 12^3 = 1728 candidates
    int num_coarse = (int)(360.0f / coarse_step);
    int total_coarse = num_coarse * num_coarse * num_coarse;

    std::cout << "\n--- Stage 1: Coarse Full Sphere ---" << std::endl;
    std::cout << "  Step: " << coarse_step << " deg" << std::endl;
    std::cout << "  Candidates: " << total_coarse << " (" << num_coarse << "^3)" << std::endl;

    // Generate coarse rotations
    std::vector<Matrix4x4> coarse_rotations;
    std::vector<float> coarse_ax, coarse_ay, coarse_az;
    coarse_rotations.reserve(total_coarse);

    for (int ix = 0; ix < num_coarse; ix++) {
        float ax = ix * coarse_step;
        for (int iy = 0; iy < num_coarse; iy++) {
            float ay = iy * coarse_step;
            for (int iz = 0; iz < num_coarse; iz++) {
                float az = iz * coarse_step;
                coarse_rotations.push_back(createRotationAroundPoint(anchor_point, ax, ay, az));
                coarse_ax.push_back(ax);
                coarse_ay.push_back(ay);
                coarse_az.push_back(az);
            }
        }
    }

    // Flatten and upload
    std::vector<float> h_matrices(coarse_rotations.size() * 16);
    for (size_t i = 0; i < coarse_rotations.size(); i++) {
        for (int j = 0; j < 16; j++) {
            h_matrices[i * 16 + j] = coarse_rotations[i].m[j];
        }
    }

    cudaMemcpy(d_rotation_matrices_, h_matrices.data(),
               coarse_rotations.size() * 16 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemset(d_scores_, 0, coarse_rotations.size() * sizeof(int));

    // Time Stage 1
    cudaEvent_t start1, stop1;
    cudaEventCreate(&start1);
    cudaEventCreate(&stop1);
    cudaEventRecord(start1);

    launchRotationDistanceEnergyKernel(
        d_source_x, d_source_y, d_source_z, num_points,
        d_rotation_matrices_, (uint32_t)coarse_rotations.size(),
        grid_min, grid_cell_size, grid_dims,
        d_cell_counts, d_cell_starts, d_vertex_indices,
        d_target_x, d_target_y, d_target_z,
        cutoff_mm, d_scores_
    );

    cudaEventRecord(stop1);
    cudaEventSynchronize(stop1);
    float ms1 = 0;
    cudaEventElapsedTime(&ms1, start1, stop1);

    // Find best coarse rotation
    std::vector<int> h_scores(coarse_rotations.size());
    cudaMemcpy(h_scores.data(), d_scores_, coarse_rotations.size() * sizeof(int), cudaMemcpyDeviceToHost);

    int best_coarse_idx = 0;
    int best_coarse_score = h_scores[0];
    for (size_t i = 1; i < h_scores.size(); i++) {
        if (h_scores[i] > best_coarse_score) {
            best_coarse_score = h_scores[i];
            best_coarse_idx = (int)i;
        }
    }

    float best_ax_coarse = coarse_ax[best_coarse_idx];
    float best_ay_coarse = coarse_ay[best_coarse_idx];
    float best_az_coarse = coarse_az[best_coarse_idx];

    std::cout << "  Best coarse: X=" << best_ax_coarse << ", Y=" << best_ay_coarse
              << ", Z=" << best_az_coarse << " deg" << std::endl;
    {
        float me = (float)best_coarse_score / (num_points * 1000.0f);
        std::cout << "  Score: mean_dist=" << std::sqrt(-me) << "mm" << std::endl;
    }
    std::cout << "  Stage 1 time: " << ms1 << " ms" << std::endl;

    // ========================================
    // Stage 2: Fine local refinement
    // ========================================
    const float fine_range = 20.0f;  // +/- 20 degrees
    const float fine_step = 5.0f;    // 5 degree step
    int num_fine_per_axis = (int)(2 * fine_range / fine_step) + 1;  // 9 per axis
    int total_fine = num_fine_per_axis * num_fine_per_axis * num_fine_per_axis;  // 729

    std::cout << "\n--- Stage 2: Fine Local Refinement ---" << std::endl;
    std::cout << "  Range: +/- " << fine_range << " deg, step=" << fine_step << " deg" << std::endl;
    std::cout << "  Candidates: " << total_fine << " (" << num_fine_per_axis << "^3)" << std::endl;

    // Generate fine rotations around best coarse (using integer indices for precision)
    std::vector<Matrix4x4> fine_rotations;
    std::vector<float> fine_ax, fine_ay, fine_az;
    fine_rotations.reserve(total_fine);

    // Use integer loop to avoid floating point precision issues
    // Range: [-20, -15, -10, -5, 0, 5, 10, 15, 20] = 9 values per axis
    for (int ix = 0; ix < num_fine_per_axis; ix++) {
        float dx = -fine_range + ix * fine_step;
        float ax = best_ax_coarse + dx;
        // Wrap to [0, 360)
        while (ax < 0) ax += 360.0f;
        while (ax >= 360.0f) ax -= 360.0f;

        for (int iy = 0; iy < num_fine_per_axis; iy++) {
            float dy = -fine_range + iy * fine_step;
            float ay = best_ay_coarse + dy;
            while (ay < 0) ay += 360.0f;
            while (ay >= 360.0f) ay -= 360.0f;

            for (int iz = 0; iz < num_fine_per_axis; iz++) {
                float dz = -fine_range + iz * fine_step;
                float az = best_az_coarse + dz;
                while (az < 0) az += 360.0f;
                while (az >= 360.0f) az -= 360.0f;

                fine_rotations.push_back(createRotationAroundPoint(anchor_point, ax, ay, az));
                fine_ax.push_back(ax);
                fine_ay.push_back(ay);
                fine_az.push_back(az);
            }
        }
    }

    // Flatten and upload
    h_matrices.resize(fine_rotations.size() * 16);
    for (size_t i = 0; i < fine_rotations.size(); i++) {
        for (int j = 0; j < 16; j++) {
            h_matrices[i * 16 + j] = fine_rotations[i].m[j];
        }
    }

    cudaMemcpy(d_rotation_matrices_, h_matrices.data(),
               fine_rotations.size() * 16 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemset(d_scores_, 0, fine_rotations.size() * sizeof(int));

    // Time Stage 2
    cudaEvent_t start2, stop2;
    cudaEventCreate(&start2);
    cudaEventCreate(&stop2);
    cudaEventRecord(start2);

    launchRotationDistanceEnergyKernel(
        d_source_x, d_source_y, d_source_z, num_points,
        d_rotation_matrices_, (uint32_t)fine_rotations.size(),
        grid_min, grid_cell_size, grid_dims,
        d_cell_counts, d_cell_starts, d_vertex_indices,
        d_target_x, d_target_y, d_target_z,
        cutoff_mm, d_scores_
    );

    cudaEventRecord(stop2);
    cudaEventSynchronize(stop2);
    float ms2 = 0;
    cudaEventElapsedTime(&ms2, start2, stop2);

    // Find best fine rotation
    h_scores.resize(fine_rotations.size());
    cudaMemcpy(h_scores.data(), d_scores_, fine_rotations.size() * sizeof(int), cudaMemcpyDeviceToHost);

    int best_fine_idx = 0;
    int best_fine_score = h_scores[0];
    for (size_t i = 1; i < h_scores.size(); i++) {
        if (h_scores[i] > best_fine_score) {
            best_fine_score = h_scores[i];
            best_fine_idx = (int)i;
        }
    }

    std::cout << "  Stage 2 time: " << ms2 << " ms" << std::endl;

    // Clean up events
    cudaEventDestroy(start1);
    cudaEventDestroy(stop1);
    cudaEventDestroy(start2);
    cudaEventDestroy(stop2);

    // Build result
    SearchResult result;
    result.best_rotation = fine_rotations[best_fine_idx];
    result.best_angle_x = fine_ax[best_fine_idx];
    result.best_angle_y = fine_ay[best_fine_idx];
    result.best_angle_z = fine_az[best_fine_idx];
    result.best_score = best_fine_score;
    result.total_points = num_points;
    {
        float me = (float)best_fine_score / (num_points * 1000.0f);
        result.mean_dist_mm = std::sqrt(-me);
    }

    float total_time = ms1 + ms2;
    int total_candidates = (int)(coarse_rotations.size() + fine_rotations.size());

    std::cout << "\n--- Result ---" << std::endl;
    std::cout << "  Best rotation: X=" << result.best_angle_x
              << ", Y=" << result.best_angle_y
              << ", Z=" << result.best_angle_z << " deg" << std::endl;
    std::cout << "  Score: mean_dist=" << result.mean_dist_mm << "mm" << std::endl;
    std::cout << "  Total time: " << total_time << " ms (" << total_candidates << " candidates)" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return result;
}
