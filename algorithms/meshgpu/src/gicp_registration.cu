#include "gicp_registration.h"
#include "tensor_icp_adapter.h"
#include <cstring>
#include <algorithm>
#include <cmath>

// Eigen for linear system solving
#include <Eigen/Dense>

// ============================================================================
// GICP Registration Implementation
// ============================================================================

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            std::cerr << "[GICPRegistration] CUDA error: " << cudaGetErrorString(err) << std::endl; \
            return false; \
        } \
    } while(0)

GICPRegistration::GICPRegistration()
    : target_mesh_(nullptr)
    , source_cloud_(nullptr)
    , d_corr_target_idx_(nullptr)
    , d_corr_distance_(nullptr)
    , d_transformed_x_(nullptr)
    , d_transformed_y_(nullptr)
    , d_transformed_z_(nullptr)
    , d_JtJ_(nullptr)
    , d_Jtb_(nullptr)
    , d_source_cell_counts_(nullptr)
    , grid_total_cells_(0)
    , num_source_points_(0)
    , num_valid_correspondences_(0)
    , initialized_(false)
{
}

GICPRegistration::~GICPRegistration() {
    freeDeviceMemory();
}

void GICPRegistration::freeDeviceMemory() {
    if (d_corr_target_idx_) cudaFree(d_corr_target_idx_);
    if (d_corr_distance_) cudaFree(d_corr_distance_);
    if (d_transformed_x_) cudaFree(d_transformed_x_);
    if (d_transformed_y_) cudaFree(d_transformed_y_);
    if (d_transformed_z_) cudaFree(d_transformed_z_);
    if (d_JtJ_) cudaFree(d_JtJ_);
    if (d_Jtb_) cudaFree(d_Jtb_);
    if (d_source_cell_counts_) cudaFree(d_source_cell_counts_);

    d_corr_target_idx_ = nullptr;
    d_corr_distance_ = nullptr;
    d_transformed_x_ = nullptr;
    d_transformed_y_ = nullptr;
    d_transformed_z_ = nullptr;
    d_JtJ_ = nullptr;
    d_Jtb_ = nullptr;
    d_source_cell_counts_ = nullptr;
    grid_total_cells_ = 0;
    initialized_ = false;
}

bool GICPRegistration::initialize(MeshGPU* target_mesh, const SourcePointCloud* source_cloud) {
    if (!target_mesh || !source_cloud) {
        std::cerr << "[GICPRegistration] Invalid input pointers" << std::endl;
        return false;
    }

    if (!target_mesh->hasGridIndex()) {
        std::cerr << "[GICPRegistration] Target mesh must have grid index built" << std::endl;
        return false;
    }

    if (!source_cloud->on_device) {
        std::cerr << "[GICPRegistration] Source cloud must be on device" << std::endl;
        return false;
    }

    // Free previous allocations
    freeDeviceMemory();

    target_mesh_ = target_mesh;
    source_cloud_ = source_cloud;
    num_source_points_ = source_cloud->num_points;

    std::cout << "[GICPRegistration] Initializing..." << std::endl;
    std::cout << "  Source points: " << num_source_points_ << std::endl;
    std::cout << "  Target vertices: " << target_mesh_->getNumVertices() << std::endl;

    // Allocate device memory
    size_t bytes = num_source_points_ * sizeof(float);
    size_t bytes_idx = num_source_points_ * sizeof(uint32_t);

    cudaError_t err;
    err = cudaMalloc(&d_corr_target_idx_, bytes_idx);
    if (err != cudaSuccess) {
        std::cerr << "[GICPRegistration] Failed to allocate corr_target_idx" << std::endl;
        return false;
    }

    err = cudaMalloc(&d_corr_distance_, bytes);
    if (err != cudaSuccess) {
        freeDeviceMemory();
        return false;
    }

    err = cudaMalloc(&d_transformed_x_, bytes);
    if (err != cudaSuccess) {
        freeDeviceMemory();
        return false;
    }

    err = cudaMalloc(&d_transformed_y_, bytes);
    if (err != cudaSuccess) {
        freeDeviceMemory();
        return false;
    }

    err = cudaMalloc(&d_transformed_z_, bytes);
    if (err != cudaSuccess) {
        freeDeviceMemory();
        return false;
    }

    // Allocate JtJ and Jtb
    err = cudaMalloc(&d_JtJ_, 21 * sizeof(float));
    if (err != cudaSuccess) {
        freeDeviceMemory();
        return false;
    }

    err = cudaMalloc(&d_Jtb_, 6 * sizeof(float));
    if (err != cudaSuccess) {
        freeDeviceMemory();
        return false;
    }

    // Allocate source density buffer for coverage-aware weighting
    const GridIndex& grid = target_mesh->getGridIndex();
    grid_total_cells_ = grid.total_cells;
    err = cudaMalloc(&d_source_cell_counts_, grid_total_cells_ * sizeof(uint32_t));
    if (err != cudaSuccess) {
        freeDeviceMemory();
        return false;
    }

    initialized_ = true;
    std::cout << "[GICPRegistration] Initialized successfully" << std::endl;

    return true;
}

bool GICPRegistration::findCorrespondences(const Matrix4x4& transform, int search_radius) {
    if (!initialized_) {
        std::cerr << "[GICPRegistration] Not initialized" << std::endl;
        return false;
    }

    const GridIndex& grid = target_mesh_->getGridIndex();

    launchFindCorrespondences(
        source_cloud_->points_x, source_cloud_->points_y, source_cloud_->points_z,
        transform.m,
        grid.cell_vertex_id,
        target_mesh_->getDeviceMesh()->vertices_x,
        target_mesh_->getDeviceMesh()->vertices_y,
        target_mesh_->getDeviceMesh()->vertices_z,
        grid.origin.x, grid.origin.y, grid.origin.z,
        grid.cell_size,
        grid.dim_x, grid.dim_y, grid.dim_z,
        d_transformed_x_, d_transformed_y_, d_transformed_z_,
        d_corr_target_idx_, d_corr_distance_,
        num_source_points_,
        search_radius
    );

    num_valid_correspondences_ = launchCountValidCorrespondences(d_corr_target_idx_, num_source_points_);

    return true;
}

bool GICPRegistration::findCorrespondences(const Matrix4x4& transform, const GridIndex& grid, int search_radius) {
    if (!initialized_) {
        std::cerr << "[GICPRegistration] Not initialized" << std::endl;
        return false;
    }

    launchFindCorrespondences(
        source_cloud_->points_x, source_cloud_->points_y, source_cloud_->points_z,
        transform.m,
        grid.cell_vertex_id,
        target_mesh_->getDeviceMesh()->vertices_x,
        target_mesh_->getDeviceMesh()->vertices_y,
        target_mesh_->getDeviceMesh()->vertices_z,
        grid.origin.x, grid.origin.y, grid.origin.z,
        grid.cell_size,
        grid.dim_x, grid.dim_y, grid.dim_z,
        d_transformed_x_, d_transformed_y_, d_transformed_z_,
        d_corr_target_idx_, d_corr_distance_,
        num_source_points_,
        search_radius
    );

    num_valid_correspondences_ = launchCountValidCorrespondences(d_corr_target_idx_, num_source_points_);

    return true;
}

bool GICPRegistration::computeLinearSystem(const Matrix4x4& current_transform,
                                            float* JtJ, float* Jtb,
                                            float distance_threshold) {
    if (!initialized_) {
        return false;
    }

    // First find correspondences with current transform
    findCorrespondences(current_transform, 2);

    // Compute JtJ and Jtb on GPU (without curvature weighting)
    launchComputeJtJAndJtb(
        d_transformed_x_, d_transformed_y_, d_transformed_z_,
        target_mesh_->getDeviceMesh()->vertices_x,
        target_mesh_->getDeviceMesh()->vertices_y,
        target_mesh_->getDeviceMesh()->vertices_z,
        target_mesh_->getDeviceMesh()->normals_x,
        target_mesh_->getDeviceMesh()->normals_y,
        target_mesh_->getDeviceMesh()->normals_z,
        d_corr_target_idx_, d_corr_distance_,
        num_source_points_,
        distance_threshold,
        d_JtJ_, d_Jtb_
    );

    // Download results to host
    cudaMemcpy(JtJ, d_JtJ_, 21 * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(Jtb, d_Jtb_, 6 * sizeof(float), cudaMemcpyDeviceToHost);

    return true;
}

bool GICPRegistration::computeLinearSystemWeighted(const Matrix4x4& current_transform,
                                                    float* JtJ, float* Jtb,
                                                    const GICPParams& params) {
    if (!initialized_) {
        return false;
    }

    // First find correspondences with current transform
    findCorrespondences(current_transform, params.search_radius);

    // Compute source density if coverage weighting is enabled
    const GridIndex& grid = target_mesh_->getGridIndex();
    const uint32_t* coverage_counts = nullptr;
    if (params.use_coverage_weighting || params.curvature_weight_mode == CurvatureWeightMode::COVERAGE_AWARE) {
        launchComputeSourceDensity(
            d_transformed_x_, d_transformed_y_, d_transformed_z_,
            grid.origin.x, grid.origin.y, grid.origin.z,
            grid.cell_size, grid.dim_x, grid.dim_y, grid.dim_z,
            num_source_points_, d_source_cell_counts_
        );
        coverage_counts = d_source_cell_counts_;
    }

    // Compute JtJ and Jtb on GPU with curvature + coverage weighting
    launchComputeJtJAndJtbWeighted(
        d_transformed_x_, d_transformed_y_, d_transformed_z_,
        target_mesh_->getDeviceMesh()->vertices_x,
        target_mesh_->getDeviceMesh()->vertices_y,
        target_mesh_->getDeviceMesh()->vertices_z,
        target_mesh_->getDeviceMesh()->normals_x,
        target_mesh_->getDeviceMesh()->normals_y,
        target_mesh_->getDeviceMesh()->normals_z,
        target_mesh_->getDeviceMesh()->curvature,        // Mean curvature
        target_mesh_->getDeviceMesh()->gaussian_curv,    // Gaussian curvature
        d_corr_target_idx_, d_corr_distance_,
        num_source_points_,
        params.distance_threshold,
        static_cast<int>(params.curvature_weight_mode),
        params.curvature_weight_scale,
        params.min_weight,
        params.max_weight,
        d_JtJ_, d_Jtb_,
        coverage_counts,
        grid.origin.x, grid.origin.y, grid.origin.z,
        grid.cell_size, grid.dim_x, grid.dim_y, grid.dim_z,
        params.coverage_density_exponent
    );

    // Download results to host
    cudaMemcpy(JtJ, d_JtJ_, 21 * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(Jtb, d_Jtb_, 6 * sizeof(float), cudaMemcpyDeviceToHost);

    return true;
}

bool GICPRegistration::solveLinearSystem(const float* JtJ, const float* Jtb, float* delta_x) {
    // Reconstruct 6x6 symmetric matrix from upper triangular
    Eigen::Matrix<float, 6, 6> H;
    int k = 0;
    for (int i = 0; i < 6; ++i) {
        for (int j = i; j < 6; ++j) {
            H(i, j) = JtJ[k];
            H(j, i) = JtJ[k];  // Symmetric
            k++;
        }
    }

    // Create b vector (negate for solving H * x = -b)
    Eigen::Matrix<float, 6, 1> b;
    for (int i = 0; i < 6; ++i) {
        b(i) = -Jtb[i];
    }

    // Add small regularization for numerical stability
    for (int i = 0; i < 6; ++i) {
        H(i, i) += 1e-6f;
    }

    // Solve using LDLT (Cholesky-like for symmetric matrices)
    Eigen::Matrix<float, 6, 1> x = H.ldlt().solve(b);

    // Copy result
    for (int i = 0; i < 6; ++i) {
        delta_x[i] = x(i);
    }

    return true;
}

Matrix4x4 GICPRegistration::deltaToMatrix(const float* delta_x) {
    // delta_x = [rx, ry, rz, tx, ty, tz]
    // For small angles, use linearized rotation
    float rx = delta_x[0];
    float ry = delta_x[1];
    float rz = delta_x[2];
    float tx = delta_x[3];
    float ty = delta_x[4];
    float tz = delta_x[5];

    // Rodrigues formula approximation for small angles:
    // R ≈ I + [w]_x where [w]_x is skew-symmetric matrix
    Matrix4x4 dT;

    // Rotation part (linearized for small angles)
    dT.m[0] = 1.0f;    dT.m[1] = -rz;     dT.m[2] = ry;      dT.m[3] = tx;
    dT.m[4] = rz;      dT.m[5] = 1.0f;    dT.m[6] = -rx;     dT.m[7] = ty;
    dT.m[8] = -ry;     dT.m[9] = rx;      dT.m[10] = 1.0f;   dT.m[11] = tz;
    dT.m[12] = 0.0f;   dT.m[13] = 0.0f;   dT.m[14] = 0.0f;   dT.m[15] = 1.0f;

    return dT;
}

GICPResult GICPRegistration::align(const Matrix4x4& initial_transform, const GICPParams& params) {
    GICPResult result;
    result.final_transform = initial_transform;
    result.iterations = 0;
    result.converged = false;

    if (!initialized_) {
        std::cerr << "[GICPRegistration] Not initialized" << std::endl;
        return result;
    }

    if (params.use_tensor_backend) {
        if (meshgpu_open3d_backend::isAvailable()) {
            if (params.verbose) {
                std::cout << "[GICPRegistration] Routing align() to Open3D Tensor ICP backend"
                          << std::endl;
            }
            const MeshSoA* dev_mesh = target_mesh_->getDeviceMesh();
            return meshgpu_open3d_backend::align(*dev_mesh, *source_cloud_,
                                                 initial_transform, params);
        }
        std::cerr << "[GICPRegistration] use_tensor_backend=true but Open3D backend "
                     "is not built in; falling back to legacy GPU-GICP path"
                  << std::endl;
    }

    // Check if curvature weighting is enabled
    bool use_curvature_weighting = (params.curvature_weight_mode != CurvatureWeightMode::NONE)
                                   || params.use_coverage_weighting;

    if (params.verbose) {
        std::cout << "\n========================================" << std::endl;
        std::cout << "GICP Alignment Starting" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  Max iterations: " << params.max_iterations << std::endl;
        std::cout << "  Convergence threshold: " << params.convergence_threshold << std::endl;
        std::cout << "  Distance threshold: " << params.distance_threshold << " mm" << std::endl;
        if (use_curvature_weighting) {
            const char* mode_names[] = {"NONE", "HIGH_CURVATURE", "LOW_CURVATURE", "GAUSSIAN_AWARE", "ADAPTIVE", "COVERAGE_AWARE"};
            int mode_idx = static_cast<int>(params.curvature_weight_mode);
            std::cout << "  Curvature weighting: " << mode_names[mode_idx] << std::endl;
            std::cout << "  Weight scale: " << params.curvature_weight_scale << std::endl;
            std::cout << "  Weight range: [" << params.min_weight << ", " << params.max_weight << "]" << std::endl;
        } else {
            std::cout << "  Curvature weighting: DISABLED" << std::endl;
        }
    }

    Matrix4x4 T = initial_transform;
    float JtJ[21], Jtb[6], delta_x[6];
    float prev_rmse = 1e10f;
    int rmse_stall_count = 0;
    const int MAX_STALL_ITERATIONS = 3;  // Stop if RMSE doesn't improve for 3 iterations
    const float RMSE_CHANGE_THRESHOLD = 1e-4f;  // Relaxed: 0.1μm change is negligible

    // Oscillation detection: track recent RMSE values
    float rmse_window[8] = {};
    int rmse_window_idx = 0;
    int rmse_window_filled = 0;

    for (int iter = 0; iter < params.max_iterations; ++iter) {
        // Step 1: Find correspondences with current transform
        // Support multi-resolution grid switching
        if (params.initial_grid_level >= 0 && target_mesh_->hasMultiResGrid()) {
            int level = params.initial_grid_level;
            if (params.switch_to_fine_at_iteration >= 0 && iter >= params.switch_to_fine_at_iteration) {
                level = (params.fine_grid_level >= 0) ? params.fine_grid_level
                        : target_mesh_->getMultiResGrid().num_levels - 1;
            }
            findCorrespondences(T, target_mesh_->getMultiResGrid().getLevel(level), params.search_radius);
        } else {
            findCorrespondences(T, params.search_radius);
        }

        // Get current RMSE
        RegistrationStats stats = getStats();
        result.rmse_history.push_back(stats.rmse);

        if (params.verbose && (iter < 10 || iter % 10 == 0)) {
            std::cout << "  Iter " << iter << ": RMSE=" << stats.rmse << " mm, "
                      << "correspondences=" << stats.num_correspondences << "/" << num_source_points_ << std::endl;
        }

        // Check RMSE-based convergence (practical convergence)
        float rmse_change = fabsf(stats.rmse - prev_rmse);
        if (rmse_change < RMSE_CHANGE_THRESHOLD) {
            rmse_stall_count++;
            if (rmse_stall_count >= MAX_STALL_ITERATIONS) {
                result.converged = true;
                if (params.verbose) {
                    std::cout << "  Converged at iteration " << iter
                              << " (RMSE stalled for " << rmse_stall_count << " iterations)" << std::endl;
                }
                break;
            }
        } else {
            rmse_stall_count = 0;  // Reset if RMSE is still changing
        }
        prev_rmse = stats.rmse;

        // Oscillation detection: if RMSE range over last 8 iterations is tiny, we're oscillating
        rmse_window[rmse_window_idx % 8] = stats.rmse;
        rmse_window_idx++;
        if (rmse_window_idx >= 8) rmse_window_filled = 1;
        if (rmse_window_filled) {
            float wmin = rmse_window[0], wmax = rmse_window[0];
            for (int w = 1; w < 8; w++) {
                if (rmse_window[w] < wmin) wmin = rmse_window[w];
                if (rmse_window[w] > wmax) wmax = rmse_window[w];
            }
            if (wmax - wmin < 0.01f) {  // RMSE oscillating within 0.01mm over 8 iters
                result.converged = true;
                if (params.verbose) {
                    std::cout << "  Converged at iteration " << iter
                              << " (RMSE oscillating in [" << wmin << ", " << wmax << "])" << std::endl;
                }
                break;
            }
        }

        // Step 2: Compute linear system (with or without curvature weighting)
        if (use_curvature_weighting) {
            computeLinearSystemWeighted(T, JtJ, Jtb, params);
        } else {
            computeLinearSystem(T, JtJ, Jtb, params.distance_threshold);
        }

        // Step 3: Solve for delta_x
        solveLinearSystem(JtJ, Jtb, delta_x);

        // Check delta-based convergence (numerical convergence)
        float delta_norm = 0.0f;
        for (int i = 0; i < 6; ++i) {
            delta_norm += delta_x[i] * delta_x[i];
        }
        delta_norm = sqrtf(delta_norm);

        // Relaxed threshold: 1e-4 is sufficient for surgical precision
        if (delta_norm < params.convergence_threshold || delta_norm < 1e-4f) {
            result.converged = true;
            if (params.verbose) {
                std::cout << "  Converged at iteration " << iter << " (delta_norm=" << delta_norm << ")" << std::endl;
            }
            break;
        }

        // Step 4: Update transform
        Matrix4x4 dT = deltaToMatrix(delta_x);
        T = dT * T;

        result.iterations = iter + 1;
    }

    // Final evaluation
    findCorrespondences(T, params.search_radius);
    RegistrationStats final_stats = getStats();
    result.final_rmse = final_stats.rmse;
    result.final_transform = T;

    if (params.verbose) {
        std::cout << "========================================" << std::endl;
        std::cout << "GICP Alignment Complete" << std::endl;
        std::cout << "  Iterations: " << result.iterations << std::endl;
        std::cout << "  Converged: " << (result.converged ? "Yes" : "No") << std::endl;
        std::cout << "  Final RMSE: " << result.final_rmse << " mm" << std::endl;
        std::cout << "  Valid correspondences: " << final_stats.num_correspondences << "/" << num_source_points_ << std::endl;
        std::cout << "========================================\n" << std::endl;
    }

    return result;
}

RegistrationStats GICPRegistration::getStats() const {
    RegistrationStats stats;
    memset(&stats, 0, sizeof(stats));

    if (!initialized_) return stats;

    launchComputeCorrespondenceStats(
        d_corr_target_idx_, d_corr_distance_, num_source_points_,
        &stats.num_correspondences, &stats.mean_distance, &stats.max_distance, &stats.rmse
    );

    return stats;
}

bool GICPRegistration::downloadCorrespondences(std::vector<Correspondence>& correspondences, uint32_t max_count) {
    if (!initialized_) return false;

    uint32_t count = (max_count > 0) ? std::min(max_count, num_source_points_) : num_source_points_;

    std::vector<uint32_t> h_idx(count);
    std::vector<float> h_dist(count);

    cudaMemcpy(h_idx.data(), d_corr_target_idx_, count * sizeof(uint32_t), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_dist.data(), d_corr_distance_, count * sizeof(float), cudaMemcpyDeviceToHost);

    correspondences.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        correspondences[i].source_idx = i;
        correspondences[i].target_idx = h_idx[i];
        correspondences[i].distance = h_dist[i];
        correspondences[i].valid = (h_idx[i] != EMPTY_CELL);
    }

    return true;
}

void GICPRegistration::printCorrespondenceInfo(uint32_t num_samples) {
    if (!initialized_) {
        std::cout << "[GICPRegistration] Not initialized" << std::endl;
        return;
    }

    std::vector<Correspondence> corrs;
    downloadCorrespondences(corrs, num_samples);

    std::cout << "\n========================================" << std::endl;
    std::cout << "Correspondence Info (first " << num_samples << " samples)" << std::endl;
    std::cout << "========================================" << std::endl;

    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < corrs.size(); ++i) {
        if (corrs[i].valid) {
            std::cout << "  Source[" << i << "] -> Target[" << corrs[i].target_idx
                      << "] dist=" << corrs[i].distance << " mm" << std::endl;
            valid_count++;
        } else {
            std::cout << "  Source[" << i << "] -> No match" << std::endl;
        }
    }

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Valid in sample: " << valid_count << "/" << corrs.size() << std::endl;

    RegistrationStats stats = getStats();
    std::cout << "\nOverall Statistics:" << std::endl;
    std::cout << "  Valid correspondences: " << stats.num_correspondences << "/" << num_source_points_
              << " (" << (100.0f * stats.num_correspondences / num_source_points_) << "%)" << std::endl;
    std::cout << "  Mean distance: " << stats.mean_distance << " mm" << std::endl;
    std::cout << "  Max distance: " << stats.max_distance << " mm" << std::endl;
    std::cout << "  RMSE: " << stats.rmse << " mm" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void GICPRegistration::verifyGridLookup(const Matrix4x4& transform, const char* description) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Verify Grid Lookup: " << description << std::endl;
    std::cout << "========================================" << std::endl;

    std::cout << "Transform Matrix:" << std::endl;
    std::cout << "  | " << transform.m[0] << "\t" << transform.m[1] << "\t" << transform.m[2] << "\t" << transform.m[3] << " |" << std::endl;
    std::cout << "  | " << transform.m[4] << "\t" << transform.m[5] << "\t" << transform.m[6] << "\t" << transform.m[7] << " |" << std::endl;
    std::cout << "  | " << transform.m[8] << "\t" << transform.m[9] << "\t" << transform.m[10] << "\t" << transform.m[11] << " |" << std::endl;
    std::cout << "  | " << transform.m[12] << "\t" << transform.m[13] << "\t" << transform.m[14] << "\t" << transform.m[15] << " |" << std::endl;

    findCorrespondences(transform, 2);
    printCorrespondenceInfo(10);
}
