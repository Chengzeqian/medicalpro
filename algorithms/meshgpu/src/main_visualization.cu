/**
 * MeshGPU Visualization Export Program
 *
 * This program runs a complete registration pipeline and exports PLY files
 * at each stage for offline visualization in Python.
 *
 * Export timeline:
 *   - target.ply        : Target mesh (bone model)
 *   - 0_initial.ply     : Source point cloud (initial state, after transformation + noise)
 *   - 1_coarse.ply      : Source after coarse registration (translation + rotation search)
 *   - 2_final.ply       : Source after fine registration (GICP converged)
 */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <cuda_runtime.h>

#include "types.h"
#include "ply_reader.h"
#include "ply_exporter.h"
#include "mesh_gpu.h"
#include "probe_simulator.h"
#include "gicp_registration.h"
#include "rotation_search.h"
#include "backend_runtime_cli.h"

// ============================================================================
// Configuration
// ============================================================================

struct VisualizationConfig {
    // Input
    std::string input_mesh_path = "E:/ICPtry/fixed_tibia_normalized.ply";

    // Output directory
    std::string output_dir = "E:/ICPtry/MeshGPU/visualization_output";

    // Simulation parameters - CLINICAL SCENARIO (anterior surface ~2000 pts)
    float rotation_z_deg = 150.0f;  // Z rotation (main horizontal rotation)
    float translation_x = 15.0f;    // X translation (mm)
    float translation_y = 10.0f;    // Y translation (mm)
    float translation_z = 5.0f;     // Z translation (mm)
    float noise_stddev = 0.5f;      // Gaussian noise (mm) - realistic probe noise

    // Clinical probe parameters
    float z_height_mm = 15.0f;      // Keep bottom 15mm (realistic surgical window)
    float nz_threshold = -0.3f;     // Reject strongly downward-facing normals
    bool require_anterior = true;   // Only anterior/medial surfaces

    // GICP parameters
    int max_iterations = 100;
    float convergence_threshold = 1e-6f;
    float distance_threshold = 15.0f;
    int search_radius = 2;

    // Rotation search parameters
    float z_angle_step = 1.0f;      // Z rotation step (degrees)
    bool enable_xy_search = false;  // Z-only (clinical: X/Y tilt is small)
    float xy_angle_range = 10.0f;
    float xy_angle_step = 5.0f;
};

// ============================================================================
// Helper Functions
// ============================================================================

void printMatrix4x4(const Matrix4x4& mat, const char* name = nullptr) {
    if (name) {
        std::cout << name << ":" << std::endl;
    }
    for (int i = 0; i < 4; ++i) {
        std::cout << "  [";
        for (int j = 0; j < 4; ++j) {
            printf("%8.4f", mat.m[i * 4 + j]);
            if (j < 3) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }
}

void printDeviceInfo() {
    int device;
    cudaGetDevice(&device);

    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, device);

    std::cout << "\n========================================" << std::endl;
    std::cout << "CUDA Device: " << prop.name << std::endl;
    std::cout << "Compute Capability: " << prop.major << "." << prop.minor << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void freeMeshHost(MeshSoA& mesh) {
    delete[] mesh.vertices_x;
    delete[] mesh.vertices_y;
    delete[] mesh.vertices_z;
    delete[] mesh.normals_x;
    delete[] mesh.normals_y;
    delete[] mesh.normals_z;
    delete[] mesh.curvature;
    delete[] mesh.gaussian_curv;
    delete[] mesh.faces_v0;
    delete[] mesh.faces_v1;
    delete[] mesh.faces_v2;
    delete[] mesh.face_normals_x;
    delete[] mesh.face_normals_y;
    delete[] mesh.face_normals_z;
    delete[] mesh.face_areas;
}

// ============================================================================
// Main Visualization Pipeline
// ============================================================================

int main(int argc, char** argv) {
    mesh_gpu::BackendCliOptions cli = mesh_gpu::parseBackendCliOptions(argc, argv);
    if (!cli.valid) {
        std::cerr << "[Args] " << cli.error_message << std::endl;
        std::cerr << mesh_gpu::buildBackendCliUsage(argv[0], "[input_mesh.ply] [output_dir]") << std::endl;
        return -1;
    }
    if (cli.show_help) {
        std::cout << mesh_gpu::buildBackendCliUsage(argv[0], "[input_mesh.ply] [output_dir]") << std::endl;
        return 0;
    }
    if (cli.positional_args.size() > 2) {
        std::cerr << "[Args] Too many positional arguments." << std::endl;
        std::cerr << mesh_gpu::buildBackendCliUsage(argv[0], "[input_mesh.ply] [output_dir]") << std::endl;
        return -1;
    }
    if (!mesh_gpu::configureBackendForCudaOnlyExecutable(cli, "mesh_gpu_vis")) {
        return -2;
    }

    VisualizationConfig config;

    // Parse command line arguments
    if (!cli.positional_args.empty()) {
        config.input_mesh_path = cli.positional_args[0];
    }
    if (cli.positional_args.size() > 1) {
        config.output_dir = cli.positional_args[1];
    }

    std::cout << "\n########################################" << std::endl;
    std::cout << "# MeshGPU Visualization Export         #" << std::endl;
    std::cout << "########################################" << std::endl;
    std::cout << "\nInput:  " << config.input_mesh_path << std::endl;
    std::cout << "Output: " << config.output_dir << std::endl;

    // Print device info
    printDeviceInfo();

    // Create output directory
    PLYExporter::ensureDirectoryExists(config.output_dir);

    // ========================================
    // Step 1: Load Target Mesh
    // ========================================
    std::cout << "\n>>> Step 1: Loading Target Mesh <<<" << std::endl;

    MeshSoA host_mesh;
    if (!PLYReader::readPLY(config.input_mesh_path, host_mesh)) {
        std::cerr << "Failed to read PLY file: " << config.input_mesh_path << std::endl;
        return -1;
    }

    // Initialize GPU mesh
    MeshGPU mesh_gpu;
    if (!mesh_gpu.initialize(host_mesh)) {
        std::cerr << "Failed to initialize MeshGPU" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }

    // Generate feature texture (computes normals and curvature)
    FeatureTexture feature_tex;
    if (!mesh_gpu.generateFeatureTexture(feature_tex)) {
        std::cerr << "Failed to generate feature texture" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }

    // Download computed results back to Host (for normals)
    if (!mesh_gpu.downloadToHost(host_mesh)) {
        std::cerr << "Failed to download results" << std::endl;
        freeMeshHost(host_mesh);
        cudaFree(feature_tex.features);
        return -1;
    }

    // Build grid index for fast lookup
    if (!mesh_gpu.buildGridIndex(2.0f)) {
        std::cerr << "Failed to build grid index" << std::endl;
        freeMeshHost(host_mesh);
        cudaFree(feature_tex.features);
        return -1;
    }

    // Export target mesh
    std::string target_path = config.output_dir + "/target.ply";
    PLYExporter::exportMesh(target_path, host_mesh, true);

    // ========================================
    // Step 2: Generate Simulated Probe Data
    // ========================================
    std::cout << "\n>>> Step 2: Generating Clinical Probe (anterior surface) <<<" << std::endl;
    std::cout << "  Rotation Z=" << config.rotation_z_deg << " degrees" << std::endl;
    std::cout << "  Translation: (" << config.translation_x << ", " << config.translation_y
              << ", " << config.translation_z << ") mm" << std::endl;
    std::cout << "  Noise: " << config.noise_stddev << " mm" << std::endl;
    std::cout << "  Z height: " << config.z_height_mm << " mm, Nz threshold: " << config.nz_threshold << std::endl;

    ProbeSimulator probe_sim;
    if (!probe_sim.generateClinicalProbe(
        host_mesh,
        config.rotation_z_deg,
        config.translation_x,
        config.translation_y,
        config.translation_z,
        config.noise_stddev,
        config.z_height_mm,
        config.nz_threshold,
        config.require_anterior,
        true  // include normals
    )) {
        std::cerr << "Failed to generate simulated probe" << std::endl;
        freeMeshHost(host_mesh);
        cudaFree(feature_tex.features);
        return -1;
    }

    probe_sim.printTransformInfo();

    const SourcePointCloud& source = probe_sim.getSourceCloud();

    // ========================================
    // Export 0: Initial State (before any registration)
    // ========================================
    std::cout << "\n>>> Exporting: 0_initial.ply (before registration) <<<" << std::endl;

    std::string initial_path = config.output_dir + "/0_initial.ply";
    PLYExporter::exportSourceCloud(initial_path, source, true);

    // ========================================
    // Step 3: Coarse Registration
    // ========================================
    std::cout << "\n>>> Step 3: Coarse Registration <<<" << std::endl;

    // 3a: Anchor-based translation
    float3_t anchor_target = probe_sim.getOriginalAnchorPoint();
    float3_t anchor_source = probe_sim.getTransformedAnchorPoint();

    std::cout << "\n--- 3a: Anchor-based Translation ---" << std::endl;
    std::cout << "  Target anchor (medial malleolus): (" << anchor_target.x << ", "
              << anchor_target.y << ", " << anchor_target.z << ")" << std::endl;
    std::cout << "  Source anchor: (" << anchor_source.x << ", "
              << anchor_source.y << ", " << anchor_source.z << ")" << std::endl;

    float3_t coarse_trans;
    coarse_trans.x = anchor_target.x - anchor_source.x;
    coarse_trans.y = anchor_target.y - anchor_source.y;
    coarse_trans.z = anchor_target.z - anchor_source.z;

    Matrix4x4 T_translation = Matrix4x4::translation(coarse_trans.x, coarse_trans.y, coarse_trans.z);
    std::cout << "  Coarse translation: (" << coarse_trans.x << ", " << coarse_trans.y
              << ", " << coarse_trans.z << ")" << std::endl;

    // 3b: GPU Rotation Search
    std::cout << "\n--- 3b: GPU Rotation Search ---" << std::endl;

    RotationSearch rot_search(mesh_gpu);

    // Apply translation to source points for rotation search
    uint32_t num_points = source.num_points;
    float* d_temp_x = nullptr;
    float* d_temp_y = nullptr;
    float* d_temp_z = nullptr;
    cudaMalloc(&d_temp_x, num_points * sizeof(float));
    cudaMalloc(&d_temp_y, num_points * sizeof(float));
    cudaMalloc(&d_temp_z, num_points * sizeof(float));

    // Download, transform, upload
    std::vector<float> h_x(num_points), h_y(num_points), h_z(num_points);
    cudaMemcpy(h_x.data(), source.points_x, num_points * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_y.data(), source.points_y, num_points * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_z.data(), source.points_z, num_points * sizeof(float), cudaMemcpyDeviceToHost);

    for (uint32_t i = 0; i < num_points; i++) {
        float3_t p(h_x[i], h_y[i], h_z[i]);
        float3_t tp = T_translation.transformPoint(p);
        h_x[i] = tp.x;
        h_y[i] = tp.y;
        h_z[i] = tp.z;
    }

    cudaMemcpy(d_temp_x, h_x.data(), num_points * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_temp_y, h_y.data(), num_points * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_temp_z, h_z.data(), num_points * sizeof(float), cudaMemcpyHostToDevice);

    // Set up rotation search parameters
    RotationSearch::SearchParams rot_params;
    rot_params.z_angle_min = 0.0f;
    rot_params.z_angle_max = 360.0f;
    rot_params.z_angle_step = config.z_angle_step;
    rot_params.enable_xy_search = config.enable_xy_search;
    rot_params.xy_angle_range = config.xy_angle_range;
    rot_params.xy_angle_step = config.xy_angle_step;

    // Time the rotation search
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    float milliseconds = 0;

    cudaEventRecord(start);
    // Use hierarchical coarse-to-fine search for ~100x speedup
    RotationSearch::SearchResult rot_result = rot_search.searchHierarchical(
        d_temp_x, d_temp_y, d_temp_z,
        num_points,
        anchor_target,
        rot_params
    );
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&milliseconds, start, stop);

    cudaFree(d_temp_x);
    cudaFree(d_temp_y);
    cudaFree(d_temp_z);

    std::cout << "  Search time: " << std::fixed << std::setprecision(2) << milliseconds << " ms" << std::endl;
    std::cout << "  Best rotation: Z=" << rot_result.best_angle_z
              << ", X=" << rot_result.best_angle_x
              << ", Y=" << rot_result.best_angle_y << " deg" << std::endl;
    std::cout << "  Mean dist: " << rot_result.mean_dist_mm << "mm" << std::endl;

    // Combine translation and rotation
    Matrix4x4 T_coarse = rot_result.best_rotation * T_translation;

    std::cout << "\n--- Coarse Transform ---" << std::endl;
    printMatrix4x4(T_coarse, "T_coarse");

    // ========================================
    // Export 1: After Coarse Registration
    // ========================================
    std::cout << "\n>>> Exporting: 1_coarse.ply (after coarse registration) <<<" << std::endl;

    std::string coarse_path = config.output_dir + "/1_coarse.ply";
    PLYExporter::exportSourceCloudTransformed(coarse_path, source, T_coarse, true);

    // ========================================
    // Step 4: Fine Registration (GICP)
    // ========================================
    std::cout << "\n>>> Step 4: Fine Registration (GICP) <<<" << std::endl;

    GICPRegistration gicp;
    if (!gicp.initialize(&mesh_gpu, &source)) {
        std::cerr << "Failed to initialize GICP" << std::endl;
        freeMeshHost(host_mesh);
        cudaFree(feature_tex.features);
        return -1;
    }

    GICPParams gicp_params;
    gicp_params.max_iterations = config.max_iterations;
    gicp_params.convergence_threshold = config.convergence_threshold;
    gicp_params.distance_threshold = config.distance_threshold;
    gicp_params.search_radius = config.search_radius;
    gicp_params.use_point_to_plane = true;
    gicp_params.verbose = true;

    cudaEventRecord(start);
    GICPResult result = gicp.align(T_coarse, gicp_params);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&milliseconds, start, stop);

    std::cout << "\n--- GICP Result ---" << std::endl;
    std::cout << "  Time: " << std::fixed << std::setprecision(2) << milliseconds << " ms" << std::endl;
    std::cout << "  Converged: " << (result.converged ? "YES" : "NO") << std::endl;
    std::cout << "  Iterations: " << result.iterations << std::endl;
    std::cout << "  Final RMSE: " << result.final_rmse << " mm" << std::endl;

    printMatrix4x4(result.final_transform, "\nFinal Transform");

    // Compute error vs ground truth
    Matrix4x4 T_gt = probe_sim.getGroundTruthTransform();
    Matrix4x4 T_error = result.final_transform * T_gt;

    float trans_error = sqrtf(
        T_error.m[3] * T_error.m[3] +
        T_error.m[7] * T_error.m[7] +
        T_error.m[11] * T_error.m[11]
    );

    std::cout << "\n--- Registration Accuracy ---" << std::endl;
    std::cout << "  Translation error: " << trans_error << " mm" << std::endl;

    // ========================================
    // Export 2: After Fine Registration
    // ========================================
    std::cout << "\n>>> Exporting: 2_final.ply (after GICP) <<<" << std::endl;

    std::string final_path = config.output_dir + "/2_final.ply";
    PLYExporter::exportSourceCloudTransformed(final_path, source, result.final_transform, true);

    // ========================================
    // Export RMSE History for plotting
    // ========================================
    std::cout << "\n>>> Exporting: rmse_history.csv <<<" << std::endl;

    std::string rmse_path = config.output_dir + "/rmse_history.csv";
    std::ofstream rmse_file(rmse_path);
    if (rmse_file.is_open()) {
        rmse_file << "iteration,rmse_mm" << std::endl;
        for (size_t i = 0; i < result.rmse_history.size(); ++i) {
            rmse_file << i << "," << result.rmse_history[i] << std::endl;
        }
        rmse_file.close();
        std::cout << "  Saved " << result.rmse_history.size() << " iterations" << std::endl;
    }

    // ========================================
    // Summary
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# Export Complete!                     #" << std::endl;
    std::cout << "########################################" << std::endl;
    std::cout << "\nExported files:" << std::endl;
    std::cout << "  " << config.output_dir << "/target.ply" << std::endl;
    std::cout << "  " << config.output_dir << "/0_initial.ply" << std::endl;
    std::cout << "  " << config.output_dir << "/1_coarse.ply" << std::endl;
    std::cout << "  " << config.output_dir << "/2_final.ply" << std::endl;
    std::cout << "  " << config.output_dir << "/rmse_history.csv" << std::endl;
    std::cout << "\nRun the Python visualization script to generate figures:" << std::endl;
    std::cout << "  python visualize_registration.py " << config.output_dir << std::endl;

    // Cleanup
    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    freeMeshHost(host_mesh);
    cudaFree(feature_tex.features);

    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();

    return 0;
}
