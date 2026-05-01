#pragma once

#include "types.h"
#include "mesh_gpu.h"
#include "gicp_registration.h"
#include <vector>

// ============================================================================
// Incremental Online Registration
// Warm-start GICP with few iterations as new probe points arrive
// ============================================================================

struct IncrementalParams {
    int gicp_iterations_per_update;     // GICP iterations per incremental batch
    int full_reregistration_interval;   // 0 = never, N = every N batches do full re-reg
    float distance_threshold;
    int search_radius;
    CurvatureWeightMode curvature_weight_mode;
    bool use_coverage_weighting;
    float coverage_density_exponent;
    bool verbose;

    IncrementalParams()
        : gicp_iterations_per_update(3)
        , full_reregistration_interval(0)
        , distance_threshold(10.0f)
        , search_radius(2)
        , curvature_weight_mode(CurvatureWeightMode::NONE)
        , use_coverage_weighting(false)
        , coverage_density_exponent(0.5f)
        , verbose(false)
    {}
};

struct IncrementalResult {
    Matrix4x4 current_transform;
    float current_rmse;
    int total_points;
    int total_updates;
    int total_gicp_iterations;
    bool valid;

    IncrementalResult()
        : current_rmse(0.0f), total_points(0), total_updates(0)
        , total_gicp_iterations(0), valid(false)
    {}
};

class IncrementalRegistration {
public:
    IncrementalRegistration();
    ~IncrementalRegistration();

    // Disable copy
    IncrementalRegistration(const IncrementalRegistration&) = delete;
    IncrementalRegistration& operator=(const IncrementalRegistration&) = delete;

    // Initialize with target mesh and optional initial transform
    bool initialize(MeshGPU* target_mesh,
                    const Matrix4x4& initial_transform = Matrix4x4(),
                    const IncrementalParams& params = IncrementalParams());

    // Add a batch of new points and update registration (warm-start GICP)
    IncrementalResult addPoints(const float* points_x, const float* points_y,
                                const float* points_z, uint32_t num_new_points);

    // Force full re-registration with all accumulated points
    IncrementalResult fullReregistration(int max_iterations = 50);

    // Getters
    Matrix4x4 getCurrentTransform() const { return current_transform_; }
    float getCurrentRMSE() const { return current_rmse_; }
    int getTotalPoints() const { return (int)total_points_; }
    bool isInitialized() const { return initialized_; }

    // Reset (clear accumulated points, keep target mesh)
    void reset();

private:
    MeshGPU* target_mesh_;
    IncrementalParams params_;

    // Accumulated source cloud (GPU)
    float* d_all_points_x_;
    float* d_all_points_y_;
    float* d_all_points_z_;
    uint32_t total_points_;
    uint32_t allocated_capacity_;

    // Current registration state
    Matrix4x4 current_transform_;
    float current_rmse_;
    int update_count_;
    int total_gicp_iterations_;
    bool initialized_;

    // Grow GPU buffer if needed
    bool ensureCapacity(uint32_t required);
    void freeDeviceMemory();

    // Run GICP with current accumulated points
    IncrementalResult runGICP(int max_iterations);
};
