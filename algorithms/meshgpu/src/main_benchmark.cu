/**
 * MeshGPU Benchmark Program
 *
 * Automated benchmark for patent experiments:
 * 1. Rotation sweep: different initial rotation offsets
 * 2. Curvature weight ablation: NONE / HIGH / ADAPTIVE
 * 3. Noise sensitivity: different noise levels
 * 4. Voxel size sensitivity: different grid cell sizes
 *
 * Outputs CSV files to benchmark_results/ directory.
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <cuda_runtime.h>

#include "types.h"
#include "ply_reader.h"
#include "mesh_gpu.h"
#include "probe_simulator.h"
#include "gicp_registration.h"
#include "rotation_search.h"
#include "incremental_registration.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#define MKDIR(dir) mkdir(dir, 0755)
#endif

// ============================================================================
// Configuration
// ============================================================================

struct BenchmarkConfig {
    std::string input_mesh_path = "E:/ICPtry/fixed_tibia_normalized.ply";
    std::string output_dir = "E:/ICPtry/MeshGPU/benchmark_results";
    int num_trials = 3;  // Repeat each experiment
};

// ============================================================================
// Helpers
// ============================================================================

static void ensureDir(const std::string& path) {
    MKDIR(path.c_str());
}

static float computeRotationError(const Matrix4x4& estimated, const Matrix4x4& ground_truth) {
    // T_error = T_est * T_gt  (if perfect, T_error = I)
    Matrix4x4 T_error = estimated * ground_truth;
    // Rotation error from trace: cos(theta) = (trace(R) - 1) / 2
    float trace = T_error.m[0] + T_error.m[5] + T_error.m[10];
    float cos_theta = (trace - 1.0f) / 2.0f;
    cos_theta = std::max(-1.0f, std::min(1.0f, cos_theta));
    return acosf(cos_theta) * 180.0f / 3.14159265f;  // degrees
}

static float computeTranslationError(const Matrix4x4& estimated, const Matrix4x4& ground_truth) {
    Matrix4x4 T_error = estimated * ground_truth;
    return sqrtf(T_error.m[3] * T_error.m[3] +
                 T_error.m[7] * T_error.m[7] +
                 T_error.m[11] * T_error.m[11]);
}

static void freeMeshHost(MeshSoA& mesh) {
    delete[] mesh.vertices_x; delete[] mesh.vertices_y; delete[] mesh.vertices_z;
    delete[] mesh.normals_x;  delete[] mesh.normals_y;  delete[] mesh.normals_z;
    delete[] mesh.curvature;  delete[] mesh.gaussian_curv;
    delete[] mesh.faces_v0;   delete[] mesh.faces_v1;   delete[] mesh.faces_v2;
    delete[] mesh.face_normals_x; delete[] mesh.face_normals_y; delete[] mesh.face_normals_z;
    delete[] mesh.face_areas;
}

// ============================================================================
// Single Experiment Runner
// ============================================================================

struct ExperimentResult {
    float coarse_rmse;
    float final_rmse;
    float coarse_time_ms;
    float gicp_time_ms;
    float total_time_ms;
    int iterations;
    bool converged;
    float translation_error;
    float rotation_error;
    int num_source_points;
};

ExperimentResult runSingleExperiment(
    MeshGPU& mesh_gpu,
    const MeshSoA& host_mesh,
    float rotation_z_deg,
    float translation_x, float translation_y, float translation_z,
    float noise_stddev,
    float voxel_size,
    CurvatureWeightMode weight_mode,
    bool use_coarse_registration,
    bool clinical_probe,
    float z_height_mm = 15.0f,
    int max_iterations = 100,
    bool use_coverage_weighting = false,
    bool use_multi_res = false
) {
    ExperimentResult result = {};

    // Build grid index with specified voxel size
    if (!mesh_gpu.buildGridIndex(voxel_size)) {
        std::cerr << "Failed to build grid index" << std::endl;
        return result;
    }

    // Generate probe
    ProbeSimulator probe_sim;
    bool probe_ok = false;

    if (clinical_probe) {
        probe_ok = probe_sim.generateClinicalProbe(
            host_mesh, rotation_z_deg,
            translation_x, translation_y, translation_z,
            noise_stddev, z_height_mm, -0.3f, true, true);
    } else {
        probe_ok = probe_sim.generateSimulatedProbe(
            host_mesh, rotation_z_deg,
            translation_x, translation_y, translation_z,
            noise_stddev, true);
    }

    if (!probe_ok) {
        std::cerr << "Failed to generate probe" << std::endl;
        return result;
    }

    const SourcePointCloud& source = probe_sim.getSourceCloud();
    result.num_source_points = source.num_points;

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    float ms = 0;

    Matrix4x4 T_coarse;

    if (use_coarse_registration) {
        // --- Coarse Registration ---
        cudaEventRecord(start);

        // Anchor translation
        float3_t anchor_target = probe_sim.getOriginalAnchorPoint();
        float3_t anchor_source = probe_sim.getTransformedAnchorPoint();
        float3_t coarse_trans;
        coarse_trans.x = anchor_target.x - anchor_source.x;
        coarse_trans.y = anchor_target.y - anchor_source.y;
        coarse_trans.z = anchor_target.z - anchor_source.z;
        Matrix4x4 T_translation = Matrix4x4::translation(coarse_trans.x, coarse_trans.y, coarse_trans.z);

        // Rotation search (use coarsest grid level if multi-res available)
        int rot_grid_level = (use_multi_res && mesh_gpu.hasMultiResGrid()) ? 0 : -1;
        RotationSearch rot_search(mesh_gpu, rot_grid_level);

        uint32_t num_points = source.num_points;
        float* d_temp_x = nullptr, *d_temp_y = nullptr, *d_temp_z = nullptr;
        cudaMalloc(&d_temp_x, num_points * sizeof(float));
        cudaMalloc(&d_temp_y, num_points * sizeof(float));
        cudaMalloc(&d_temp_z, num_points * sizeof(float));

        std::vector<float> h_x(num_points), h_y(num_points), h_z(num_points);
        cudaMemcpy(h_x.data(), source.points_x, num_points * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_y.data(), source.points_y, num_points * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_z.data(), source.points_z, num_points * sizeof(float), cudaMemcpyDeviceToHost);

        for (uint32_t i = 0; i < num_points; i++) {
            float3_t p(h_x[i], h_y[i], h_z[i]);
            float3_t tp = T_translation.transformPoint(p);
            h_x[i] = tp.x; h_y[i] = tp.y; h_z[i] = tp.z;
        }

        cudaMemcpy(d_temp_x, h_x.data(), num_points * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d_temp_y, h_y.data(), num_points * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d_temp_z, h_z.data(), num_points * sizeof(float), cudaMemcpyHostToDevice);

        RotationSearch::SearchParams rot_params;
        rot_params.z_angle_min = 0.0f;
        rot_params.z_angle_max = 360.0f;
        rot_params.z_angle_step = 1.0f;
        rot_params.enable_xy_search = false;

        RotationSearch::SearchResult rot_result = rot_search.searchHierarchical(
            d_temp_x, d_temp_y, d_temp_z, num_points, anchor_target, rot_params);

        cudaFree(d_temp_x); cudaFree(d_temp_y); cudaFree(d_temp_z);

        T_coarse = rot_result.best_rotation * T_translation;

        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&ms, start, stop);
        result.coarse_time_ms = ms;

        // Compute coarse RMSE (approximate via GICP stats)
        GICPRegistration gicp_check;
        if (gicp_check.initialize(&mesh_gpu, &source)) {
            gicp_check.findCorrespondences(T_coarse, 2);
            RegistrationStats stats = gicp_check.getStats();
            result.coarse_rmse = stats.rmse;
        }
    } else {
        // No coarse registration - use identity as initial pose
        T_coarse = Matrix4x4();  // Identity
        result.coarse_time_ms = 0;
        result.coarse_rmse = 0;
    }

    // --- Fine Registration (GICP) ---
    GICPRegistration gicp;
    if (!gicp.initialize(&mesh_gpu, &source)) {
        std::cerr << "Failed to initialize GICP" << std::endl;
        cudaEventDestroy(start); cudaEventDestroy(stop);
        return result;
    }

    GICPParams gicp_params;
    gicp_params.max_iterations = max_iterations;
    gicp_params.convergence_threshold = 1e-6f;
    gicp_params.distance_threshold = 15.0f;
    gicp_params.search_radius = 2;
    gicp_params.use_point_to_plane = true;
    gicp_params.verbose = false;
    gicp_params.curvature_weight_mode = weight_mode;
    gicp_params.curvature_weight_scale = 1.0f;
    gicp_params.min_weight = 0.1f;
    gicp_params.max_weight = 10.0f;
    gicp_params.use_coverage_weighting = use_coverage_weighting;

    // Multi-resolution grid: use coarse for first half of iterations, then fine
    if (use_multi_res && mesh_gpu.hasMultiResGrid()) {
        gicp_params.initial_grid_level = 0;  // coarsest
        gicp_params.switch_to_fine_at_iteration = max_iterations / 2;
        gicp_params.fine_grid_level = mesh_gpu.getMultiResGrid().num_levels - 1;
    }

    cudaEventRecord(start);
    GICPResult gicp_result = gicp.align(T_coarse, gicp_params);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&ms, start, stop);

    result.gicp_time_ms = ms;
    result.total_time_ms = result.coarse_time_ms + result.gicp_time_ms;
    result.final_rmse = gicp_result.final_rmse;
    result.iterations = gicp_result.iterations;
    result.converged = gicp_result.converged;

    // Compute errors vs ground truth
    Matrix4x4 T_gt = probe_sim.getGroundTruthTransform();
    result.translation_error = computeTranslationError(gicp_result.final_transform, T_gt);
    result.rotation_error = computeRotationError(gicp_result.final_transform, T_gt);

    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    return result;
}

// Average multiple trials
ExperimentResult averageResults(const std::vector<ExperimentResult>& results) {
    ExperimentResult avg = {};
    int n = (int)results.size();
    if (n == 0) return avg;

    for (const auto& r : results) {
        avg.coarse_rmse += r.coarse_rmse;
        avg.final_rmse += r.final_rmse;
        avg.coarse_time_ms += r.coarse_time_ms;
        avg.gicp_time_ms += r.gicp_time_ms;
        avg.total_time_ms += r.total_time_ms;
        avg.iterations += r.iterations;
        avg.translation_error += r.translation_error;
        avg.rotation_error += r.rotation_error;
        avg.num_source_points += r.num_source_points;
        if (r.converged) avg.converged = true;
    }

    avg.coarse_rmse /= n;
    avg.final_rmse /= n;
    avg.coarse_time_ms /= n;
    avg.gicp_time_ms /= n;
    avg.total_time_ms /= n;
    avg.iterations /= n;
    avg.translation_error /= n;
    avg.rotation_error /= n;
    avg.num_source_points /= n;
    return avg;
}

// ============================================================================
// Experiment Suites
// ============================================================================

void runRotationSweep(MeshGPU& mesh_gpu, const MeshSoA& host_mesh,
                       const BenchmarkConfig& config) {
    std::cout << "\n===== Experiment 1: Rotation Sweep =====" << std::endl;

    std::string csv_path = config.output_dir + "/rotation_sweep.csv";
    std::ofstream csv(csv_path);
    csv << "rotation_deg,method,final_rmse_mm,coarse_rmse_mm,coarse_time_ms,"
        << "gicp_time_ms,total_time_ms,iterations,converged,"
        << "trans_error_mm,rot_error_deg,num_points" << std::endl;

    float rotations[] = {30, 60, 90, 120, 150, 180};

    for (float rot : rotations) {
        std::cout << "  Rotation = " << rot << " deg" << std::endl;

        // Method A: Full pipeline (coarse + fine)
        {
            std::vector<ExperimentResult> trials;
            for (int t = 0; t < config.num_trials; t++) {
                ExperimentResult r = runSingleExperiment(
                    mesh_gpu, host_mesh, rot, 15, 10, 5, 0.5f, 2.0f,
                    CurvatureWeightMode::ADAPTIVE, true, true);
                trials.push_back(r);
            }
            ExperimentResult avg = averageResults(trials);
            csv << rot << ",full_pipeline," << avg.final_rmse << "," << avg.coarse_rmse << ","
                << avg.coarse_time_ms << "," << avg.gicp_time_ms << "," << avg.total_time_ms << ","
                << avg.iterations << "," << (avg.converged ? 1 : 0) << ","
                << avg.translation_error << "," << avg.rotation_error << ","
                << avg.num_source_points << std::endl;
            std::cout << "    Full: RMSE=" << avg.final_rmse << "mm, time=" << avg.total_time_ms << "ms" << std::endl;
        }

        // Method B: GICP only (no coarse registration)
        {
            std::vector<ExperimentResult> trials;
            for (int t = 0; t < config.num_trials; t++) {
                ExperimentResult r = runSingleExperiment(
                    mesh_gpu, host_mesh, rot, 15, 10, 5, 0.5f, 2.0f,
                    CurvatureWeightMode::ADAPTIVE, false, true);
                trials.push_back(r);
            }
            ExperimentResult avg = averageResults(trials);
            csv << rot << ",gicp_only," << avg.final_rmse << "," << avg.coarse_rmse << ","
                << avg.coarse_time_ms << "," << avg.gicp_time_ms << "," << avg.total_time_ms << ","
                << avg.iterations << "," << (avg.converged ? 1 : 0) << ","
                << avg.translation_error << "," << avg.rotation_error << ","
                << avg.num_source_points << std::endl;
            std::cout << "    GICP only: RMSE=" << avg.final_rmse << "mm, time=" << avg.total_time_ms << "ms" << std::endl;
        }
    }

    csv.close();
    std::cout << "  Saved: " << csv_path << std::endl;
}

void runCurvatureAblation(MeshGPU& mesh_gpu, const MeshSoA& host_mesh,
                           const BenchmarkConfig& config) {
    std::cout << "\n===== Experiment 2: Curvature Weight Ablation =====" << std::endl;

    std::string csv_path = config.output_dir + "/curvature_ablation.csv";
    std::ofstream csv(csv_path);
    csv << "weight_mode,final_rmse_mm,gicp_time_ms,iterations,converged,"
        << "trans_error_mm,rot_error_deg" << std::endl;

    struct ModeEntry {
        CurvatureWeightMode mode;
        const char* name;
    };

    ModeEntry modes[] = {
        {CurvatureWeightMode::NONE, "none"},
        {CurvatureWeightMode::HIGH_CURVATURE, "high_curvature"},
        {CurvatureWeightMode::LOW_CURVATURE, "low_curvature"},
        {CurvatureWeightMode::GAUSSIAN_AWARE, "gaussian_aware"},
        {CurvatureWeightMode::ADAPTIVE, "adaptive"},
    };

    for (const auto& mode : modes) {
        std::cout << "  Mode: " << mode.name << std::endl;
        std::vector<ExperimentResult> trials;
        for (int t = 0; t < config.num_trials; t++) {
            ExperimentResult r = runSingleExperiment(
                mesh_gpu, host_mesh, 150, 15, 10, 5, 0.5f, 2.0f,
                mode.mode, true, true);
            trials.push_back(r);
        }
        ExperimentResult avg = averageResults(trials);
        csv << mode.name << "," << avg.final_rmse << "," << avg.gicp_time_ms << ","
            << avg.iterations << "," << (avg.converged ? 1 : 0) << ","
            << avg.translation_error << "," << avg.rotation_error << std::endl;
        std::cout << "    RMSE=" << avg.final_rmse << "mm, iters=" << avg.iterations << std::endl;
    }

    csv.close();
    std::cout << "  Saved: " << csv_path << std::endl;
}

void runNoiseSweep(MeshGPU& mesh_gpu, const MeshSoA& host_mesh,
                    const BenchmarkConfig& config) {
    std::cout << "\n===== Experiment 3: Noise Sensitivity =====" << std::endl;

    std::string csv_path = config.output_dir + "/noise_sweep.csv";
    std::ofstream csv(csv_path);
    csv << "noise_mm,final_rmse_mm,coarse_rmse_mm,gicp_time_ms,iterations,converged,"
        << "trans_error_mm,rot_error_deg" << std::endl;

    float noise_levels[] = {0.1f, 0.3f, 0.5f, 1.0f, 2.0f};

    for (float noise : noise_levels) {
        std::cout << "  Noise = " << noise << " mm" << std::endl;
        std::vector<ExperimentResult> trials;
        for (int t = 0; t < config.num_trials; t++) {
            ExperimentResult r = runSingleExperiment(
                mesh_gpu, host_mesh, 150, 15, 10, 5, noise, 2.0f,
                CurvatureWeightMode::ADAPTIVE, true, true);
            trials.push_back(r);
        }
        ExperimentResult avg = averageResults(trials);
        csv << noise << "," << avg.final_rmse << "," << avg.coarse_rmse << ","
            << avg.gicp_time_ms << "," << avg.iterations << ","
            << (avg.converged ? 1 : 0) << ","
            << avg.translation_error << "," << avg.rotation_error << std::endl;
        std::cout << "    RMSE=" << avg.final_rmse << "mm" << std::endl;
    }

    csv.close();
    std::cout << "  Saved: " << csv_path << std::endl;
}

void runVoxelSweep(MeshGPU& mesh_gpu, const MeshSoA& host_mesh,
                    const BenchmarkConfig& config) {
    std::cout << "\n===== Experiment 4: Voxel Size Sensitivity =====" << std::endl;

    std::string csv_path = config.output_dir + "/voxel_sweep.csv";
    std::ofstream csv(csv_path);
    csv << "voxel_mm,final_rmse_mm,coarse_time_ms,gicp_time_ms,total_time_ms,"
        << "iterations,converged,trans_error_mm" << std::endl;

    float voxels[] = {0.5f, 1.0f, 2.0f, 3.0f, 5.0f};

    for (float vox : voxels) {
        std::cout << "  Voxel = " << vox << " mm" << std::endl;
        std::vector<ExperimentResult> trials;
        for (int t = 0; t < config.num_trials; t++) {
            ExperimentResult r = runSingleExperiment(
                mesh_gpu, host_mesh, 150, 15, 10, 5, 0.5f, vox,
                CurvatureWeightMode::ADAPTIVE, true, true);
            trials.push_back(r);
        }
        ExperimentResult avg = averageResults(trials);
        csv << vox << "," << avg.final_rmse << "," << avg.coarse_time_ms << ","
            << avg.gicp_time_ms << "," << avg.total_time_ms << ","
            << avg.iterations << "," << (avg.converged ? 1 : 0) << ","
            << avg.translation_error << std::endl;
        std::cout << "    RMSE=" << avg.final_rmse << "mm, time=" << avg.total_time_ms << "ms" << std::endl;
    }

    csv.close();
    std::cout << "  Saved: " << csv_path << std::endl;
}

void runCoverageSweep(MeshGPU& mesh_gpu, const MeshSoA& host_mesh,
                       const BenchmarkConfig& config) {
    std::cout << "\n===== Experiment 5: Coverage / Point Count =====" << std::endl;

    std::string csv_path = config.output_dir + "/coverage_sweep.csv";
    std::ofstream csv(csv_path);
    csv << "coverage_type,num_points,final_rmse_mm,coarse_time_ms,gicp_time_ms,"
        << "total_time_ms,iterations,converged,trans_error_mm" << std::endl;

    // Full surface probe (all vertices with transform + noise)
    {
        std::cout << "  Full surface" << std::endl;
        std::vector<ExperimentResult> trials;
        for (int t = 0; t < config.num_trials; t++) {
            ExperimentResult r = runSingleExperiment(
                mesh_gpu, host_mesh, 150, 15, 10, 5, 0.5f, 2.0f,
                CurvatureWeightMode::ADAPTIVE, true, false);  // full surface
            trials.push_back(r);
        }
        ExperimentResult avg = averageResults(trials);
        csv << "full_surface," << avg.num_source_points << "," << avg.final_rmse << ","
            << avg.coarse_time_ms << "," << avg.gicp_time_ms << "," << avg.total_time_ms << ","
            << avg.iterations << "," << (avg.converged ? 1 : 0) << ","
            << avg.translation_error << std::endl;
        std::cout << "    pts=" << avg.num_source_points << " RMSE=" << avg.final_rmse << "mm" << std::endl;
    }

    // Clinical probe with different z_height
    float z_heights[] = {30.0f, 15.0f, 8.0f};
    const char* z_names[] = {"clinical_30mm", "clinical_15mm", "clinical_8mm"};

    for (int i = 0; i < 3; i++) {
        std::cout << "  " << z_names[i] << std::endl;
        std::vector<ExperimentResult> trials;
        for (int t = 0; t < config.num_trials; t++) {
            ExperimentResult r = runSingleExperiment(
                mesh_gpu, host_mesh, 150, 15, 10, 5, 0.5f, 2.0f,
                CurvatureWeightMode::ADAPTIVE, true, true, z_heights[i]);
            trials.push_back(r);
        }
        ExperimentResult avg = averageResults(trials);
        csv << z_names[i] << "," << avg.num_source_points << "," << avg.final_rmse << ","
            << avg.coarse_time_ms << "," << avg.gicp_time_ms << "," << avg.total_time_ms << ","
            << avg.iterations << "," << (avg.converged ? 1 : 0) << ","
            << avg.translation_error << std::endl;
        std::cout << "    pts=" << avg.num_source_points << " RMSE=" << avg.final_rmse << "mm" << std::endl;
    }

    csv.close();
    std::cout << "  Saved: " << csv_path << std::endl;
}

// ============================================================================
// Experiment 6: Coverage Weighting Ablation
// ============================================================================

void runCoverageWeightingAblation(MeshGPU& mesh_gpu, const MeshSoA& host_mesh,
                                    const BenchmarkConfig& config) {
    std::cout << "\n===== Experiment 6: Coverage Weighting Ablation =====" << std::endl;

    std::string csv_path = config.output_dir + "/coverage_weighting_ablation.csv";
    std::ofstream csv(csv_path);
    csv << "weight_mode,coverage_type,num_points,final_rmse_mm,trans_error_mm,"
        << "rot_error_deg,gicp_time_ms,iterations" << std::endl;

    struct CoverageCase {
        const char* name;
        bool clinical;
        float z_height;
    };
    CoverageCase cases[] = {
        {"full_surface", false, 15.0f},
        {"clinical_15mm", true, 15.0f},
        {"clinical_8mm", true, 8.0f},
    };

    struct WeightCase {
        const char* name;
        CurvatureWeightMode mode;
        bool coverage;
    };
    WeightCase weights[] = {
        {"none", CurvatureWeightMode::NONE, false},
        {"coverage_only", CurvatureWeightMode::NONE, true},
        {"adaptive+coverage", CurvatureWeightMode::ADAPTIVE, true},
    };

    for (auto& cc : cases) {
        for (auto& wc : weights) {
            std::cout << "  " << cc.name << " / " << wc.name << std::endl;
            std::vector<ExperimentResult> trials;
            for (int t = 0; t < config.num_trials; t++) {
                ExperimentResult r = runSingleExperiment(
                    mesh_gpu, host_mesh, 150, 15, 10, 5, 0.5f, 2.0f,
                    wc.mode, true, cc.clinical, cc.z_height, 100,
                    wc.coverage, false);
                trials.push_back(r);
            }
            ExperimentResult avg = averageResults(trials);
            csv << wc.name << "," << cc.name << "," << avg.num_source_points << ","
                << avg.final_rmse << "," << avg.translation_error << ","
                << avg.rotation_error << "," << avg.gicp_time_ms << ","
                << avg.iterations << std::endl;
            std::cout << "    RMSE=" << avg.final_rmse << " trans_err=" << avg.translation_error << std::endl;
        }
    }

    csv.close();
    std::cout << "  Saved: " << csv_path << std::endl;
}

// ============================================================================
// Experiment 7: Multi-Resolution Grid Ablation
// ============================================================================

void runMultiResAblation(MeshGPU& mesh_gpu, const MeshSoA& host_mesh,
                          const BenchmarkConfig& config) {
    std::cout << "\n===== Experiment 7: Multi-Resolution Grid =====" << std::endl;

    std::string csv_path = config.output_dir + "/multires_ablation.csv";
    std::ofstream csv(csv_path);
    csv << "mode,final_rmse_mm,coarse_time_ms,gicp_time_ms,total_time_ms,"
        << "iterations,trans_error_mm,rot_error_deg" << std::endl;

    float rotations[] = {60, 120, 150};

    // Single resolution (default 2mm)
    for (float rot : rotations) {
        std::cout << "  Single-res 2mm, rot=" << rot << "deg" << std::endl;
        std::vector<ExperimentResult> trials;
        for (int t = 0; t < config.num_trials; t++) {
            ExperimentResult r = runSingleExperiment(
                mesh_gpu, host_mesh, rot, 15, 10, 5, 0.5f, 2.0f,
                CurvatureWeightMode::NONE, true, true, 15.0f, 100, false, false);
            trials.push_back(r);
        }
        ExperimentResult avg = averageResults(trials);
        csv << "single_2mm_rot" << (int)rot << "," << avg.final_rmse << ","
            << avg.coarse_time_ms << "," << avg.gicp_time_ms << "," << avg.total_time_ms << ","
            << avg.iterations << "," << avg.translation_error << "," << avg.rotation_error << std::endl;
        std::cout << "    RMSE=" << avg.final_rmse << " time=" << avg.total_time_ms << "ms" << std::endl;
    }

    // Multi-resolution {8mm, 4mm, 2mm}
    float multi_sizes[] = {8.0f, 4.0f, 2.0f};
    mesh_gpu.buildMultiResGridIndex(multi_sizes, 3);

    for (float rot : rotations) {
        std::cout << "  Multi-res {8,4,2}mm, rot=" << rot << "deg" << std::endl;
        std::vector<ExperimentResult> trials;
        for (int t = 0; t < config.num_trials; t++) {
            ExperimentResult r = runSingleExperiment(
                mesh_gpu, host_mesh, rot, 15, 10, 5, 0.5f, 2.0f,
                CurvatureWeightMode::NONE, true, true, 15.0f, 100, false, true);
            trials.push_back(r);
        }
        ExperimentResult avg = averageResults(trials);
        csv << "multires_842_rot" << (int)rot << "," << avg.final_rmse << ","
            << avg.coarse_time_ms << "," << avg.gicp_time_ms << "," << avg.total_time_ms << ","
            << avg.iterations << "," << avg.translation_error << "," << avg.rotation_error << std::endl;
        std::cout << "    RMSE=" << avg.final_rmse << " time=" << avg.total_time_ms << "ms" << std::endl;
    }

    csv.close();
    std::cout << "  Saved: " << csv_path << std::endl;
}

// ============================================================================
// Experiment 8: Incremental vs Batch Registration
// ============================================================================

void runIncrementalBenchmark(MeshGPU& mesh_gpu, const MeshSoA& host_mesh,
                              const BenchmarkConfig& config) {
    std::cout << "\n===== Experiment 8: Incremental vs Batch =====" << std::endl;

    std::string csv_path = config.output_dir + "/incremental_benchmark.csv";
    std::ofstream csv(csv_path);
    csv << "mode,batch_size,total_points,final_rmse_mm,trans_error_mm,"
        << "rot_error_deg,total_time_ms" << std::endl;

    // First, run batch as baseline
    std::cout << "  Batch registration baseline..." << std::endl;
    {
        std::vector<ExperimentResult> trials;
        for (int t = 0; t < config.num_trials; t++) {
            ExperimentResult r = runSingleExperiment(
                mesh_gpu, host_mesh, 150, 15, 10, 5, 0.5f, 2.0f,
                CurvatureWeightMode::NONE, true, true, 15.0f, 100);
            trials.push_back(r);
        }
        ExperimentResult avg = averageResults(trials);
        csv << "batch,all," << avg.num_source_points << "," << avg.final_rmse << ","
            << avg.translation_error << "," << avg.rotation_error << ","
            << avg.total_time_ms << std::endl;
        std::cout << "    Batch: RMSE=" << avg.final_rmse << " time=" << avg.total_time_ms << "ms" << std::endl;
    }

    // Incremental: generate probe, feed in batches
    int batch_sizes[] = {50, 100, 200};

    for (int bs : batch_sizes) {
        std::cout << "  Incremental, batch_size=" << bs << std::endl;

        float total_rmse = 0, total_trans = 0, total_rot = 0, total_time = 0;
        int total_pts = 0;

        for (int t = 0; t < config.num_trials; t++) {
            // Build grid
            mesh_gpu.buildGridIndex(2.0f);

            // Generate probe
            ProbeSimulator probe_sim;
            probe_sim.generateClinicalProbe(
                host_mesh, 150, 15, 10, 5, 0.5f, 15.0f, -0.3f, true, true);

            const SourcePointCloud& source = probe_sim.getSourceCloud();
            uint32_t total = source.num_points;

            // Download all points to host
            std::vector<float> h_x(total), h_y(total), h_z(total);
            cudaMemcpy(h_x.data(), source.points_x, total * sizeof(float), cudaMemcpyDeviceToHost);
            cudaMemcpy(h_y.data(), source.points_y, total * sizeof(float), cudaMemcpyDeviceToHost);
            cudaMemcpy(h_z.data(), source.points_z, total * sizeof(float), cudaMemcpyDeviceToHost);

            // First, do coarse registration with initial batch
            uint32_t initial_batch = std::min((uint32_t)bs, total);

            // Anchor translation (from probe simulator)
            float3_t anchor_target = probe_sim.getOriginalAnchorPoint();
            float3_t anchor_source = probe_sim.getTransformedAnchorPoint();
            Matrix4x4 T_init = Matrix4x4::translation(
                anchor_target.x - anchor_source.x,
                anchor_target.y - anchor_source.y,
                anchor_target.z - anchor_source.z);

            // Simple rotation search with initial batch
            float* d_init_x, *d_init_y, *d_init_z;
            cudaMalloc(&d_init_x, initial_batch * sizeof(float));
            cudaMalloc(&d_init_y, initial_batch * sizeof(float));
            cudaMalloc(&d_init_z, initial_batch * sizeof(float));

            // Apply translation to initial batch
            std::vector<float> tx(initial_batch), ty(initial_batch), tz(initial_batch);
            for (uint32_t i = 0; i < initial_batch; i++) {
                float3_t p(h_x[i], h_y[i], h_z[i]);
                float3_t tp = T_init.transformPoint(p);
                tx[i] = tp.x; ty[i] = tp.y; tz[i] = tp.z;
            }
            cudaMemcpy(d_init_x, tx.data(), initial_batch * sizeof(float), cudaMemcpyHostToDevice);
            cudaMemcpy(d_init_y, ty.data(), initial_batch * sizeof(float), cudaMemcpyHostToDevice);
            cudaMemcpy(d_init_z, tz.data(), initial_batch * sizeof(float), cudaMemcpyHostToDevice);

            RotationSearch rot_search(mesh_gpu);
            RotationSearch::SearchParams rot_params;
            rot_params.z_angle_step = 1.0f;
            auto rot_result = rot_search.searchHierarchical(
                d_init_x, d_init_y, d_init_z, initial_batch, anchor_target, rot_params);
            cudaFree(d_init_x); cudaFree(d_init_y); cudaFree(d_init_z);

            Matrix4x4 T_coarse = rot_result.best_rotation * T_init;

            // Start incremental registration
            cudaEvent_t ev_start, ev_stop;
            cudaEventCreate(&ev_start);
            cudaEventCreate(&ev_stop);

            cudaEventRecord(ev_start);

            IncrementalParams inc_params;
            inc_params.gicp_iterations_per_update = 3;
            inc_params.distance_threshold = 15.0f;
            inc_params.verbose = false;

            IncrementalRegistration inc_reg;
            inc_reg.initialize(&mesh_gpu, T_coarse, inc_params);

            // Feed points in batches
            IncrementalResult last_result;
            for (uint32_t offset = 0; offset < total; offset += bs) {
                uint32_t count = std::min((uint32_t)bs, total - offset);
                last_result = inc_reg.addPoints(
                    h_x.data() + offset, h_y.data() + offset, h_z.data() + offset, count);
            }

            // Final full re-registration
            last_result = inc_reg.fullReregistration(50);

            cudaEventRecord(ev_stop);
            cudaEventSynchronize(ev_stop);
            float ms = 0;
            cudaEventElapsedTime(&ms, ev_start, ev_stop);

            Matrix4x4 T_gt = probe_sim.getGroundTruthTransform();
            total_rmse += last_result.current_rmse;
            total_trans += computeTranslationError(last_result.current_transform, T_gt);
            total_rot += computeRotationError(last_result.current_transform, T_gt);
            total_time += ms;
            total_pts += last_result.total_points;

            cudaEventDestroy(ev_start);
            cudaEventDestroy(ev_stop);
        }

        int n = config.num_trials;
        csv << "incremental," << bs << "," << total_pts / n << ","
            << total_rmse / n << "," << total_trans / n << ","
            << total_rot / n << "," << total_time / n << std::endl;
        std::cout << "    RMSE=" << total_rmse / n << " time=" << total_time / n << "ms" << std::endl;
    }

    csv.close();
    std::cout << "  Saved: " << csv_path << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "\n########################################" << std::endl;
    std::cout << "# MeshGPU Benchmark Suite              #" << std::endl;
    std::cout << "########################################" << std::endl;

    BenchmarkConfig config;

    // Parse arguments
    if (argc > 1) config.input_mesh_path = argv[1];
    if (argc > 2) config.output_dir = argv[2];

    std::cout << "\nInput: " << config.input_mesh_path << std::endl;
    std::cout << "Output: " << config.output_dir << std::endl;
    std::cout << "Trials per experiment: " << config.num_trials << std::endl;

    // Print GPU info
    int device;
    cudaGetDevice(&device);
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, device);
    std::cout << "\nGPU: " << prop.name << " (SM " << prop.major << "." << prop.minor << ")" << std::endl;

    // Create output directory
    ensureDir(config.output_dir.c_str());

    // Load mesh
    std::cout << "\n>>> Loading mesh <<<" << std::endl;
    MeshSoA host_mesh;
    if (!PLYReader::readPLY(config.input_mesh_path, host_mesh)) {
        std::cerr << "Failed to read PLY: " << config.input_mesh_path << std::endl;
        return -1;
    }
    std::cout << "  Vertices: " << host_mesh.num_vertices << ", Faces: " << host_mesh.num_faces << std::endl;

    // Initialize GPU
    MeshGPU mesh_gpu;
    if (!mesh_gpu.initialize(host_mesh)) {
        std::cerr << "Failed to initialize MeshGPU" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }

    // Compute features (normals, curvature)
    FeatureTexture feature_tex;
    if (!mesh_gpu.generateFeatureTexture(feature_tex)) {
        std::cerr << "Failed to generate features" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }

    // Download computed normals back to host (needed by ProbeSimulator)
    if (!mesh_gpu.downloadToHost(host_mesh)) {
        std::cerr << "Failed to download" << std::endl;
        freeMeshHost(host_mesh);
        cudaFree(feature_tex.features);
        return -1;
    }

    // Run all experiments
    auto t0 = std::chrono::high_resolution_clock::now();

    runRotationSweep(mesh_gpu, host_mesh, config);
    runCurvatureAblation(mesh_gpu, host_mesh, config);
    runNoiseSweep(mesh_gpu, host_mesh, config);
    runVoxelSweep(mesh_gpu, host_mesh, config);
    runCoverageSweep(mesh_gpu, host_mesh, config);
    runCoverageWeightingAblation(mesh_gpu, host_mesh, config);
    runMultiResAblation(mesh_gpu, host_mesh, config);
    runIncrementalBenchmark(mesh_gpu, host_mesh, config);

    auto t1 = std::chrono::high_resolution_clock::now();
    float total_sec = std::chrono::duration<float>(t1 - t0).count();

    std::cout << "\n########################################" << std::endl;
    std::cout << "# Benchmark Complete!                  #" << std::endl;
    std::cout << "# Total time: " << std::fixed << std::setprecision(1) << total_sec << " seconds" << std::endl;
    std::cout << "########################################" << std::endl;
    std::cout << "\nCSV files saved to: " << config.output_dir << std::endl;

    // Cleanup
    freeMeshHost(host_mesh);
    cudaFree(feature_tex.features);

    return 0;
}
