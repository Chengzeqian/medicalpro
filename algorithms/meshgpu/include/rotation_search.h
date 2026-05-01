#pragma once

#include "types.h"
#include "mesh_gpu.h"
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * GPU-accelerated brute-force rotation search
 *
 * Strategy:
 * 1. Source has been translated to align anchor points (1-point translation done)
 * 2. Now search for best rotation around the anchor point
 * 3. Use Grid to score each rotation: count how many source points fall into valid grid cells
 * 4. Return the best rotation matrix
 */
class RotationSearch {
public:
    struct SearchParams {
        // Z-axis rotation (main search)
        float z_angle_min = 0.0f;       // degrees
        float z_angle_max = 360.0f;     // degrees
        float z_angle_step = 2.0f;      // degrees (180 candidates)

        // X/Y axis fine-tuning (optional)
        float xy_angle_range = 10.0f;   // +/- degrees
        float xy_angle_step = 5.0f;     // degrees

        bool enable_xy_search = false;  // Enable X/Y fine-tuning
    };

    struct SearchResult {
        Matrix4x4 best_rotation;
        float best_angle_z;      // Best Z rotation angle (degrees)
        float best_angle_x;      // Best X rotation angle (degrees)
        float best_angle_y;      // Best Y rotation angle (degrees)
        int best_score;          // Negative truncated distance energy (×1000)
        int total_points;        // Total source points
        float mean_dist_mm;      // Mean distance to mesh (mm)
    };

    /**
     * Initialize rotation search with target mesh's grid
     * @param mesh_gpu Target mesh with grid index built
     * @param grid_level Multi-res grid level to use (-1 = default grid, 0 = coarsest)
     */
    RotationSearch(MeshGPU& mesh_gpu, int grid_level = -1);

    // Set which multi-res grid level to use
    void setGridLevel(int level) { grid_level_ = level; }

    ~RotationSearch();

    /**
     * Search for the best rotation around anchor point (original brute-force)
     * @param d_source_points Source points on GPU (already translated by coarse alignment)
     * @param num_points Number of source points
     * @param anchor_point The rotation center (target's anchor = medial malleolus tip)
     * @param params Search parameters
     * @return Best rotation result
     */
    SearchResult search(
        const float* d_source_x,
        const float* d_source_y,
        const float* d_source_z,
        uint32_t num_points,
        const float3_t& anchor_point,
        const SearchParams& params = SearchParams()
    );

    /**
     * Hierarchical Coarse-to-Fine Search (RECOMMENDED - much faster!)
     *
     * Strategy:
     * 1. Stage 1 (Coarse): Search Z-axis only with coarse step (e.g., 5 deg)
     *    -> Find best Z angle quickly
     * 2. Stage 2 (Fine): Search around best Z with finer step, plus X/Y axes
     *    -> Refine the result with local 3D search
     *
     * This reduces candidates from 60000+ to ~500, achieving 100x speedup!
     *
     * @param d_source_points Source points on GPU
     * @param num_points Number of source points
     * @param anchor_point Rotation center
     * @param params Search parameters (xy_angle_range/step used in stage 2)
     * @return Best rotation result
     */
    SearchResult searchHierarchical(
        const float* d_source_x,
        const float* d_source_y,
        const float* d_source_z,
        uint32_t num_points,
        const float3_t& anchor_point,
        const SearchParams& params = SearchParams()
    );

    /**
     * Full Sphere Coverage Search (Brute Force - when orientation is unknown)
     *
     * When we don't know the initial orientation at all (no assumption about
     * which axis is "up"), we perform a full 3D rotation search.
     *
     * Strategy:
     * - Search all 3 axes: Roll (X), Pitch (Y), Yaw (Z)
     * - Step size: 20 degrees (GICP convergence domain covers ~20 deg)
     * - Total candidates: 18 x 18 x 18 = 5832
     * - Estimated time: ~5ms on GPU (based on 1800 rotations in 1.46ms)
     *
     * This is useful for:
     * - First-time initialization when patient orientation is unknown
     * - Recovery from tracking loss
     * - Robust global registration
     *
     * @param d_source_points Source points on GPU (translated to anchor)
     * @param num_points Number of source points
     * @param anchor_point Rotation center (e.g., medial malleolus)
     * @param angle_step_deg Step size in degrees (default 20)
     * @return Best rotation result
     */
    SearchResult searchFullSphere(
        const float* d_source_x,
        const float* d_source_y,
        const float* d_source_z,
        uint32_t num_points,
        const float3_t& anchor_point,
        float angle_step_deg = 20.0f
    );

    /**
     * Full Sphere with Hierarchical Refinement (BEST for unknown orientation)
     *
     * Strategy:
     * 1. Stage 1 (Full Sphere Coarse): All 3 axes, 30 deg step -> 1728 candidates
     * 2. Stage 2 (Local Refinement): +/- 20 deg around best, 5 deg step
     *
     * @param d_source_points Source points on GPU
     * @param num_points Number of source points
     * @param anchor_point Rotation center
     * @return Best rotation result
     */
    SearchResult searchFullSphereHierarchical(
        const float* d_source_x,
        const float* d_source_y,
        const float* d_source_z,
        uint32_t num_points,
        const float3_t& anchor_point
    );

private:
    MeshGPU& mesh_gpu_;
    int grid_level_;       // Multi-res grid level (-1 = default)

    // Device memory for rotation matrices and scores
    float* d_rotation_matrices_;    // Flattened 4x4 matrices
    int* d_scores_;                 // Score for each rotation
    uint32_t max_rotations_;

    // Generate candidate rotation matrices on CPU
    std::vector<Matrix4x4> generateCandidateRotations(
        const float3_t& anchor_point,
        const SearchParams& params
    );

    // Create rotation matrix around a point
    static Matrix4x4 createRotationAroundPoint(
        const float3_t& center,
        float angle_x_deg,
        float angle_y_deg,
        float angle_z_deg
    );
};

// CUDA kernel declarations
extern "C" {
    /**
     * Evaluate multiple rotations in parallel
     * Each thread block handles one rotation matrix
     *
     * @param source_x/y/z Source point coordinates
     * @param num_points Number of source points
     * @param rotation_matrices Flattened rotation matrices (16 floats each)
     * @param num_rotations Number of rotation candidates
     * @param grid_min Grid minimum corner
     * @param grid_cell_size Grid cell size
     * @param grid_dims Grid dimensions (nx, ny, nz)
     * @param grid_cell_counts Number of vertices in each cell (0 = empty)
     * @param scores Output: score for each rotation (atomic add)
     */
    void launchRotationEvaluationKernel(
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
    );

    void launchRotationDistanceEnergyKernel(
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
    );
}
