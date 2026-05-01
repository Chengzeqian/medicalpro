#pragma once

#include "types.h"
#include "mesh_gpu.h"
#include "probe_simulator.h"
#include <cuda_runtime.h>
#include <iostream>
#include <vector>

// ============================================================================
// GICP Registration Module
// GPU-accelerated Generalized ICP for point cloud registration
// ============================================================================

// Correspondence result for a single source point
struct Correspondence {
    uint32_t source_idx;       // Source point index
    uint32_t target_idx;       // Matched target vertex ID (EMPTY_CELL if no match)
    float distance;            // Distance to matched point
    bool valid;                // Whether correspondence is valid
};

// Registration statistics
struct RegistrationStats {
    uint32_t num_correspondences;   // Number of valid correspondences
    float mean_distance;            // Mean correspondence distance
    float max_distance;             // Max correspondence distance
    float rmse;                     // Root mean square error
};

// Curvature weighting strategy
enum class CurvatureWeightMode {
    NONE = 0,           // No curvature weighting (original behavior)
    HIGH_CURVATURE,     // Prefer high-curvature regions (edges, corners)
    LOW_CURVATURE,      // Prefer low-curvature regions (flat surfaces)
    GAUSSIAN_AWARE,     // Use Gaussian curvature for surface type detection
    ADAPTIVE,           // Hybrid: balance between high and low curvature
    COVERAGE_AWARE      // Inverse source density weighting (sparse regions get higher weight)
};

// GICP solver parameters
struct GICPParams {
    int max_iterations;         // Maximum number of iterations
    float convergence_threshold; // Stop when delta_x norm < threshold
    float distance_threshold;   // Reject correspondences with distance > threshold
    int search_radius;          // Grid search radius
    bool use_point_to_plane;    // Use point-to-plane (true) or point-to-point (false)
    bool verbose;               // Print iteration info

    // Curvature weighting parameters
    CurvatureWeightMode curvature_weight_mode;  // Weighting strategy
    float curvature_weight_scale;  // Scale factor for curvature influence (default 1.0)
    float min_weight;              // Minimum weight to prevent singularities (default 0.1)
    float max_weight;              // Maximum weight cap (default 10.0)

    // Coverage-aware weighting parameters
    bool use_coverage_weighting;       // Enable inverse-density weighting (combinable with any mode)
    float coverage_density_exponent;   // w = 1/density^exp (0.5 = inverse sqrt, default)

    // Multi-resolution grid parameters
    int initial_grid_level;            // -1 = use default grid, 0 = coarsest level
    int switch_to_fine_at_iteration;   // -1 = never switch, N = switch at iteration N
    int fine_grid_level;               // Grid level to switch to

    GICPParams() :
        max_iterations(50),
        convergence_threshold(1e-6f),
        distance_threshold(10.0f),  // 10mm
        search_radius(2),
        use_point_to_plane(true),
        verbose(true),
        curvature_weight_mode(CurvatureWeightMode::NONE),
        curvature_weight_scale(1.0f),
        min_weight(0.1f),
        max_weight(10.0f),
        use_coverage_weighting(false),
        coverage_density_exponent(0.5f),
        initial_grid_level(-1),
        switch_to_fine_at_iteration(-1),
        fine_grid_level(-1) {}
};

// GICP result
struct GICPResult {
    Matrix4x4 final_transform;   // Final estimated transform
    int iterations;              // Number of iterations performed
    float final_rmse;            // Final RMSE
    bool converged;              // Whether converged
    std::vector<float> rmse_history;  // RMSE at each iteration
};

// ============================================================================
// GICP Registration Class
// ============================================================================

class GICPRegistration {
public:
    GICPRegistration();
    ~GICPRegistration();

    // Disable copy
    GICPRegistration(const GICPRegistration&) = delete;
    GICPRegistration& operator=(const GICPRegistration&) = delete;

    // Initialize with target mesh (MeshGPU) and source point cloud
    bool initialize(MeshGPU* target_mesh, const SourcePointCloud* source_cloud);

    // ========================================
    // Main Registration Function
    // ========================================
    // Run GICP optimization starting from initial_transform
    // Returns the optimized transform that aligns source to target
    GICPResult align(const Matrix4x4& initial_transform = Matrix4x4(),
                     const GICPParams& params = GICPParams());

    // ========================================
    // Low-level Functions (for debugging)
    // ========================================

    // Find correspondences using current transform (uses target_mesh_ default grid)
    bool findCorrespondences(const Matrix4x4& transform, int search_radius = 2);

    // Find correspondences using a specific grid index
    bool findCorrespondences(const Matrix4x4& transform, const GridIndex& grid, int search_radius = 2);

    // Compute JtJ and Jtb for one Gauss-Newton step (Point-to-Plane)
    // Returns: JtJ (6x6 symmetric) and Jtb (6x1)
    bool computeLinearSystem(const Matrix4x4& current_transform,
                             float* JtJ,  // [21] upper triangular (symmetric 6x6)
                             float* Jtb,  // [6]
                             float distance_threshold = 10.0f);

    // Compute JtJ and Jtb with curvature weighting
    bool computeLinearSystemWeighted(const Matrix4x4& current_transform,
                                     float* JtJ,  // [21] upper triangular
                                     float* Jtb,  // [6]
                                     const GICPParams& params);

    // Solve for delta_x using Eigen (on CPU)
    // delta_x = [rx, ry, rz, tx, ty, tz]
    bool solveLinearSystem(const float* JtJ, const float* Jtb, float* delta_x);

    // Convert delta_x to incremental transform matrix
    static Matrix4x4 deltaToMatrix(const float* delta_x);

    // Get correspondence statistics
    RegistrationStats getStats() const;

    // Download correspondences to host for debugging
    bool downloadCorrespondences(std::vector<Correspondence>& correspondences, uint32_t max_count = 0);

    // Print correspondence info (for debugging)
    void printCorrespondenceInfo(uint32_t num_samples = 10);

    // Verify grid lookup with known transform
    void verifyGridLookup(const Matrix4x4& transform, const char* description);

    // Get number of valid correspondences
    uint32_t getNumValidCorrespondences() const { return num_valid_correspondences_; }

private:
    void freeDeviceMemory();

    // Target mesh (owned externally)
    MeshGPU* target_mesh_;

    // Source point cloud (owned externally)
    const SourcePointCloud* source_cloud_;

    // Device memory for correspondence results
    uint32_t* d_corr_target_idx_;    // [num_source_points]
    float* d_corr_distance_;          // [num_source_points]

    // Device memory for transformed source points
    float* d_transformed_x_;
    float* d_transformed_y_;
    float* d_transformed_z_;

    // Device memory for JtJ and Jtb reduction
    float* d_JtJ_;   // [21] upper triangular
    float* d_Jtb_;   // [6]

    // Device memory for coverage-aware weighting
    uint32_t* d_source_cell_counts_;  // [total_cells] source point density per grid cell
    uint32_t grid_total_cells_;       // Cached total cells for allocation

    uint32_t num_source_points_;
    uint32_t num_valid_correspondences_;
    bool initialized_;
};

// ============================================================================
// CUDA Kernel Declarations
// ============================================================================

// Transform source points and find correspondences in target grid
extern "C" void launchFindCorrespondences(
    // Source points (device)
    const float* source_x, const float* source_y, const float* source_z,
    // Transform matrix (host, will be copied to constant memory)
    const float* transform_matrix,  // [16] row-major
    // Target grid info
    const uint32_t* cell_vertex_id,
    const float* target_vertices_x, const float* target_vertices_y, const float* target_vertices_z,
    float grid_origin_x, float grid_origin_y, float grid_origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    // Output
    float* transformed_x, float* transformed_y, float* transformed_z,
    uint32_t* corr_target_idx,
    float* corr_distance,
    // Params
    uint32_t num_points,
    int search_radius
);

// Count valid correspondences (reduction)
extern "C" uint32_t launchCountValidCorrespondences(
    const uint32_t* corr_target_idx,
    uint32_t num_points
);

// Compute correspondence statistics
extern "C" void launchComputeCorrespondenceStats(
    const uint32_t* corr_target_idx,
    const float* corr_distance,
    uint32_t num_points,
    // Output (host)
    uint32_t* num_valid,
    float* mean_distance,
    float* max_distance,
    float* rmse
);

// Compute JtJ and Jtb for Point-to-Plane ICP (without curvature weighting)
// Each thread processes one correspondence, uses atomic add for reduction
extern "C" void launchComputeJtJAndJtb(
    // Transformed source points
    const float* transformed_x, const float* transformed_y, const float* transformed_z,
    // Target points
    const float* target_x, const float* target_y, const float* target_z,
    // Target normals (for point-to-plane)
    const float* target_nx, const float* target_ny, const float* target_nz,
    // Correspondence indices
    const uint32_t* corr_target_idx,
    const float* corr_distance,
    // Params
    uint32_t num_points,
    float distance_threshold,
    // Output (device memory, will be atomically accumulated)
    float* d_JtJ,   // [21] upper triangular
    float* d_Jtb    // [6]
);

// Compute JtJ and Jtb with curvature weighting
// Curvature-weighted Point-to-Plane ICP: JᵀWJ·Δx = -JᵀWb
extern "C" void launchComputeJtJAndJtbWeighted(
    // Transformed source points
    const float* transformed_x, const float* transformed_y, const float* transformed_z,
    // Target points
    const float* target_x, const float* target_y, const float* target_z,
    // Target normals (for point-to-plane)
    const float* target_nx, const float* target_ny, const float* target_nz,
    // Target curvature (mean curvature and Gaussian curvature)
    const float* target_curvature,       // Mean curvature
    const float* target_gaussian_curv,   // Gaussian curvature
    // Correspondence indices
    const uint32_t* corr_target_idx,
    const float* corr_distance,
    // Params
    uint32_t num_points,
    float distance_threshold,
    int weight_mode,        // CurvatureWeightMode enum value
    float weight_scale,     // Scaling factor
    float min_weight,       // Minimum weight
    float max_weight,       // Maximum weight
    // Output (device memory, will be atomically accumulated)
    float* d_JtJ,   // [21] upper triangular
    float* d_Jtb,   // [6]
    // Coverage-aware weighting (optional, pass nullptr to disable)
    const uint32_t* source_cell_counts = nullptr,
    float grid_origin_x = 0, float grid_origin_y = 0, float grid_origin_z = 0,
    float grid_cell_size = 0,
    int grid_dim_x = 0, int grid_dim_y = 0, int grid_dim_z = 0,
    float coverage_density_exponent = 0.5f
);

// Compute source point density per grid cell (for coverage-aware weighting)
extern "C" void launchComputeSourceDensity(
    const float* transformed_x, const float* transformed_y, const float* transformed_z,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    uint32_t num_points,
    uint32_t* source_cell_counts  // [total_cells] output
);
