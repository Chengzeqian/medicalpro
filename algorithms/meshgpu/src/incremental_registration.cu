#include "incremental_registration.h"
#include <cuda_runtime.h>
#include <iostream>
#include <cstring>

// ============================================================================
// Incremental Registration Implementation
// ============================================================================

IncrementalRegistration::IncrementalRegistration()
    : target_mesh_(nullptr)
    , d_all_points_x_(nullptr)
    , d_all_points_y_(nullptr)
    , d_all_points_z_(nullptr)
    , total_points_(0)
    , allocated_capacity_(0)
    , current_rmse_(0.0f)
    , update_count_(0)
    , total_gicp_iterations_(0)
    , initialized_(false)
{
}

IncrementalRegistration::~IncrementalRegistration() {
    freeDeviceMemory();
}

void IncrementalRegistration::freeDeviceMemory() {
    if (d_all_points_x_) cudaFree(d_all_points_x_);
    if (d_all_points_y_) cudaFree(d_all_points_y_);
    if (d_all_points_z_) cudaFree(d_all_points_z_);
    d_all_points_x_ = nullptr;
    d_all_points_y_ = nullptr;
    d_all_points_z_ = nullptr;
    allocated_capacity_ = 0;
}

bool IncrementalRegistration::ensureCapacity(uint32_t required) {
    if (required <= allocated_capacity_) return true;

    uint32_t new_capacity = std::max(required * 2, (uint32_t)10000);

    float* new_x = nullptr;
    float* new_y = nullptr;
    float* new_z = nullptr;

    cudaError_t err;
    err = cudaMalloc(&new_x, new_capacity * sizeof(float));
    if (err != cudaSuccess) return false;
    err = cudaMalloc(&new_y, new_capacity * sizeof(float));
    if (err != cudaSuccess) { cudaFree(new_x); return false; }
    err = cudaMalloc(&new_z, new_capacity * sizeof(float));
    if (err != cudaSuccess) { cudaFree(new_x); cudaFree(new_y); return false; }

    // Copy existing data
    if (total_points_ > 0 && d_all_points_x_) {
        cudaMemcpy(new_x, d_all_points_x_, total_points_ * sizeof(float), cudaMemcpyDeviceToDevice);
        cudaMemcpy(new_y, d_all_points_y_, total_points_ * sizeof(float), cudaMemcpyDeviceToDevice);
        cudaMemcpy(new_z, d_all_points_z_, total_points_ * sizeof(float), cudaMemcpyDeviceToDevice);
    }

    // Free old
    if (d_all_points_x_) cudaFree(d_all_points_x_);
    if (d_all_points_y_) cudaFree(d_all_points_y_);
    if (d_all_points_z_) cudaFree(d_all_points_z_);

    d_all_points_x_ = new_x;
    d_all_points_y_ = new_y;
    d_all_points_z_ = new_z;
    allocated_capacity_ = new_capacity;

    return true;
}

bool IncrementalRegistration::initialize(MeshGPU* target_mesh,
                                          const Matrix4x4& initial_transform,
                                          const IncrementalParams& params) {
    if (!target_mesh || !target_mesh->hasGridIndex()) {
        std::cerr << "[IncrementalRegistration] Invalid target mesh or no grid index" << std::endl;
        return false;
    }

    reset();

    target_mesh_ = target_mesh;
    params_ = params;
    current_transform_ = initial_transform;

    // Pre-allocate initial capacity
    if (!ensureCapacity(10000)) {
        std::cerr << "[IncrementalRegistration] Failed to allocate initial buffer" << std::endl;
        return false;
    }

    initialized_ = true;

    if (params_.verbose) {
        std::cout << "[IncrementalRegistration] Initialized." << std::endl;
        std::cout << "  GICP iters per update: " << params_.gicp_iterations_per_update << std::endl;
        std::cout << "  Full re-reg interval: " << params_.full_reregistration_interval << std::endl;
    }

    return true;
}

void IncrementalRegistration::reset() {
    freeDeviceMemory();
    total_points_ = 0;
    current_rmse_ = 0.0f;
    update_count_ = 0;
    total_gicp_iterations_ = 0;
    // Keep target_mesh_ and params_
}

IncrementalResult IncrementalRegistration::addPoints(
    const float* points_x, const float* points_y, const float* points_z,
    uint32_t num_new_points)
{
    IncrementalResult result;

    if (!initialized_ || num_new_points == 0) {
        return result;
    }

    // Ensure capacity
    if (!ensureCapacity(total_points_ + num_new_points)) {
        std::cerr << "[IncrementalRegistration] Failed to grow buffer" << std::endl;
        return result;
    }

    // Copy new points to GPU at offset total_points_
    cudaMemcpy(d_all_points_x_ + total_points_, points_x,
               num_new_points * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_all_points_y_ + total_points_, points_y,
               num_new_points * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_all_points_z_ + total_points_, points_z,
               num_new_points * sizeof(float), cudaMemcpyHostToDevice);

    total_points_ += num_new_points;
    update_count_++;

    // Decide iteration count
    int max_iter = params_.gicp_iterations_per_update;

    // Periodic full re-registration
    if (params_.full_reregistration_interval > 0 &&
        update_count_ % params_.full_reregistration_interval == 0) {
        max_iter = 50;  // Full convergence
    }

    // Run warm-start GICP
    result = runGICP(max_iter);

    if (params_.verbose) {
        std::cout << "[Incremental] Update #" << update_count_
                  << ": +" << num_new_points << " pts (total=" << total_points_
                  << "), RMSE=" << result.current_rmse << " mm" << std::endl;
    }

    return result;
}

IncrementalResult IncrementalRegistration::fullReregistration(int max_iterations) {
    if (!initialized_ || total_points_ == 0) {
        return IncrementalResult();
    }

    if (params_.verbose) {
        std::cout << "[IncrementalRegistration] Full re-registration with " << total_points_
                  << " points, max_iter=" << max_iterations << std::endl;
    }

    return runGICP(max_iterations);
}

IncrementalResult IncrementalRegistration::runGICP(int max_iterations) {
    IncrementalResult result;

    // Create a SourcePointCloud view
    SourcePointCloud cloud;
    cloud.points_x = d_all_points_x_;
    cloud.points_y = d_all_points_y_;
    cloud.points_z = d_all_points_z_;
    cloud.normals_x = nullptr;
    cloud.normals_y = nullptr;
    cloud.normals_z = nullptr;
    cloud.num_points = total_points_;
    cloud.on_device = true;

    // Create GICP instance
    GICPRegistration gicp;
    if (!gicp.initialize(target_mesh_, &cloud)) {
        std::cerr << "[IncrementalRegistration] Failed to initialize GICP" << std::endl;
        return result;
    }

    // Set params
    GICPParams gicp_params;
    gicp_params.max_iterations = max_iterations;
    gicp_params.convergence_threshold = 1e-6f;
    gicp_params.distance_threshold = params_.distance_threshold;
    gicp_params.search_radius = params_.search_radius;
    gicp_params.use_point_to_plane = true;
    gicp_params.verbose = false;  // Suppress per-iteration output
    gicp_params.curvature_weight_mode = params_.curvature_weight_mode;
    gicp_params.use_coverage_weighting = params_.use_coverage_weighting;
    gicp_params.coverage_density_exponent = params_.coverage_density_exponent;

    // Warm-start from current transform
    GICPResult gicp_result = gicp.align(current_transform_, gicp_params);

    // Update state
    current_transform_ = gicp_result.final_transform;
    current_rmse_ = gicp_result.final_rmse;
    total_gicp_iterations_ += gicp_result.iterations;

    // Build result
    result.current_transform = current_transform_;
    result.current_rmse = current_rmse_;
    result.total_points = (int)total_points_;
    result.total_updates = update_count_;
    result.total_gicp_iterations = total_gicp_iterations_;
    result.valid = true;

    return result;
}
