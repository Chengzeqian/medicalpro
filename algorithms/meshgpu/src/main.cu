#include <iostream>
#include <iomanip>
#include <chrono>
#include <cuda_runtime.h>
#include "types.h"
#include "ply_reader.h"
#include "mesh_gpu.h"
#include "probe_simulator.h"
#include "gicp_registration.h"
#include "rotation_search.h"
#include "backend_runtime_cli.h"

// ============================================================================
// MeshGPU Test Program
// Loads PLY file, computes normals and curvature on GPU, generates feature texture
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
    std::cout << "CUDA Device Information" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Device: " << prop.name << std::endl;
    std::cout << "Compute Capability: " << prop.major << "." << prop.minor << std::endl;
    std::cout << "Total Global Memory: " << prop.totalGlobalMem / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Shared Memory per Block: " << prop.sharedMemPerBlock / 1024 << " KB" << std::endl;
    std::cout << "Max Threads per Block: " << prop.maxThreadsPerBlock << std::endl;
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

void printStatistics(const MeshSoA& mesh) {
    // Compute curvature statistics
    float curv_min = FLT_MAX, curv_max = -FLT_MAX, curv_sum = 0;
    float gauss_min = FLT_MAX, gauss_max = -FLT_MAX, gauss_sum = 0;

    for (uint32_t i = 0; i < mesh.num_vertices; ++i) {
        curv_min = std::min(curv_min, mesh.curvature[i]);
        curv_max = std::max(curv_max, mesh.curvature[i]);
        curv_sum += mesh.curvature[i];

        gauss_min = std::min(gauss_min, mesh.gaussian_curv[i]);
        gauss_max = std::max(gauss_max, mesh.gaussian_curv[i]);
        gauss_sum += mesh.gaussian_curv[i];
    }

    float curv_avg = curv_sum / mesh.num_vertices;
    float gauss_avg = gauss_sum / mesh.num_vertices;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Curvature Statistics" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Mean Curvature:" << std::endl;
    std::cout << "  Min: " << curv_min << std::endl;
    std::cout << "  Max: " << curv_max << std::endl;
    std::cout << "  Avg: " << curv_avg << std::endl;
    std::cout << "\nGaussian Curvature:" << std::endl;
    std::cout << "  Min: " << gauss_min << std::endl;
    std::cout << "  Max: " << gauss_max << std::endl;
    std::cout << "  Avg: " << gauss_avg << std::endl;
    std::cout << "========================================\n" << std::endl;
}

int main(int argc, char** argv) {
    mesh_gpu::BackendCliOptions cli = mesh_gpu::parseBackendCliOptions(argc, argv);
    if (!cli.valid) {
        std::cerr << "[Args] " << cli.error_message << std::endl;
        std::cerr << mesh_gpu::buildBackendCliUsage(argv[0], "[input_mesh.ply]") << std::endl;
        return -1;
    }
    if (cli.show_help) {
        std::cout << mesh_gpu::buildBackendCliUsage(argv[0], "[input_mesh.ply]") << std::endl;
        return 0;
    }
    if (cli.positional_args.size() > 1) {
        std::cerr << "[Args] Too many positional arguments." << std::endl;
        std::cerr << mesh_gpu::buildBackendCliUsage(argv[0], "[input_mesh.ply]") << std::endl;
        return -1;
    }
    if (!mesh_gpu::configureBackendForCudaOnlyExecutable(cli, "mesh_gpu")) {
        return -2;
    }

    // Default PLY file path
    std::string ply_path = "E:/ICPtry/fixed_tibia_normalized.ply";

    if (!cli.positional_args.empty()) {
        ply_path = cli.positional_args[0];
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "MeshGPU - GPU Mesh Feature Computation" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Input: " << ply_path << std::endl;

    // Print device info
    printDeviceInfo();

    // Read PLY file
    MeshSoA host_mesh;
    if (!PLYReader::readPLY(ply_path, host_mesh)) {
        std::cerr << "Failed to read PLY file: " << ply_path << std::endl;
        return -1;
    }

    // Create GPU manager and initialize
    MeshGPU mesh_gpu;
    if (!mesh_gpu.initialize(host_mesh)) {
        std::cerr << "Failed to initialize MeshGPU" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }

    // Generate feature texture
    FeatureTexture feature_tex;
    if (!mesh_gpu.generateFeatureTexture(feature_tex)) {
        std::cerr << "Failed to generate feature texture" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }

    std::cout << "[Main] Feature texture generated: " << feature_tex.num_vertices
              << " vertices x " << feature_tex.feature_dim << " features" << std::endl;

    // Download computed results back to Host
    if (!mesh_gpu.downloadToHost(host_mesh)) {
        std::cerr << "Failed to download results" << std::endl;
        freeMeshHost(host_mesh);
        cudaFree(feature_tex.features);
        return -1;
    }

    // Print statistics
    printStatistics(host_mesh);

    // Print first 10 vertex features
    std::cout << "\n========================================" << std::endl;
    std::cout << "Sample Vertex Features (first 10)" << std::endl;
    std::cout << "========================================" << std::endl;

    // Download features to Host
    std::vector<float> h_features(feature_tex.num_vertices * feature_tex.feature_dim);
    cudaMemcpy(h_features.data(), feature_tex.features,
               h_features.size() * sizeof(float), cudaMemcpyDeviceToHost);

    for (uint32_t i = 0; i < std::min(10u, feature_tex.num_vertices); ++i) {
        float* feat = h_features.data() + i * FEATURE_DIM;
        std::cout << "V" << i << ": "
                  << "Normal=(" << feat[0] << ", " << feat[1] << ", " << feat[2] << ") "
                  << "MeanCurv=" << feat[3] << " "
                  << "GaussCurv=" << feat[4] << " "
                  << "Pos=(" << feat[5] << ", " << feat[6] << ")" << std::endl;
    }

    // ========================================
    // Test Grid Index
    // ========================================
    std::cout << "\n========================================" << std::endl;
    std::cout << "Building Grid Index for O(1) Query" << std::endl;
    std::cout << "========================================" << std::endl;

    // Build grid with 2mm cell size
    if (!mesh_gpu.buildGridIndex(2.0f)) {
        std::cerr << "Failed to build grid index" << std::endl;
        freeMeshHost(host_mesh);
        cudaFree(feature_tex.features);
        return -1;
    }

    // Test single point query
    std::cout << "\n========================================" << std::endl;
    std::cout << "Testing Point Query" << std::endl;
    std::cout << "========================================" << std::endl;

    // Query a point near the mesh center
    BoundingBox bbox = mesh_gpu.getBoundingBox();
    float3_t center = bbox.center();

    std::cout << "Query point (mesh center): (" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;

    PointQueryResult result = mesh_gpu.queryPoint(center.x, center.y, center.z, 2);

    if (result.valid) {
        std::cout << "  Found vertex ID: " << result.vertex_id << std::endl;
        std::cout << "  Position: (" << result.position.x << ", " << result.position.y << ", " << result.position.z << ")" << std::endl;
        std::cout << "  Normal: (" << result.normal.x << ", " << result.normal.y << ", " << result.normal.z << ")" << std::endl;
        std::cout << "  Mean Curvature: " << result.mean_curvature << std::endl;
        std::cout << "  Gaussian Curvature: " << result.gaussian_curvature << std::endl;
        std::cout << "  Distance: " << result.distance << " mm" << std::endl;
    } else {
        std::cout << "  No vertex found (point may be outside mesh)" << std::endl;
    }

    // Test with a point on the mesh surface (use first vertex position)
    std::cout << "\nQuery point (first vertex): (" << host_mesh.vertices_x[0] << ", "
              << host_mesh.vertices_y[0] << ", " << host_mesh.vertices_z[0] << ")" << std::endl;

    result = mesh_gpu.queryPoint(host_mesh.vertices_x[0], host_mesh.vertices_y[0], host_mesh.vertices_z[0], 1);

    if (result.valid) {
        std::cout << "  Found vertex ID: " << result.vertex_id << std::endl;
        std::cout << "  Distance: " << result.distance << " mm" << std::endl;
    }

    // ========================================
    // Test Simulated Probe Generation
    // ========================================
    std::cout << "\n========================================" << std::endl;
    std::cout << "Generating Simulated Probe Data" << std::endl;
    std::cout << "========================================" << std::endl;

    ProbeSimulator probe_sim;

    // Generate probe with default test parameters:
    // - 10 degree rotation around Z axis
    // - 5mm translation along X axis
    // - 0.5mm Gaussian noise
    if (!probe_sim.generateDefaultTestProbe(host_mesh, 0.5f)) {
        std::cerr << "Failed to generate simulated probe" << std::endl;
        freeMeshHost(host_mesh);
        cudaFree(feature_tex.features);
        return -1;
    }

    // Print ground truth transformation
    probe_sim.printTransformInfo();

    // Verify by comparing first few points
    std::cout << "========================================" << std::endl;
    std::cout << "Comparing Original vs Transformed Points" << std::endl;
    std::cout << "========================================" << std::endl;

    // Download source points for comparison
    std::vector<float> src_x(10), src_y(10), src_z(10);
    probe_sim.downloadToHost(src_x.data(), src_y.data(), src_z.data(), 10);

    const Matrix4x4& T = probe_sim.getGroundTruthTransform();

    for (int i = 0; i < 5; ++i) {
        float3_t orig(host_mesh.vertices_x[i], host_mesh.vertices_y[i], host_mesh.vertices_z[i]);
        float3_t expected = T.transformPoint(orig);

        std::cout << "V" << i << ":" << std::endl;
        std::cout << "  Original:    (" << orig.x << ", " << orig.y << ", " << orig.z << ")" << std::endl;
        std::cout << "  Expected:    (" << expected.x << ", " << expected.y << ", " << expected.z << ")" << std::endl;
        std::cout << "  Actual+Noise:(" << src_x[i] << ", " << src_y[i] << ", " << src_z[i] << ")" << std::endl;

        // Compute noise magnitude
        float noise = sqrtf(
            (src_x[i] - expected.x) * (src_x[i] - expected.x) +
            (src_y[i] - expected.y) * (src_y[i] - expected.y) +
            (src_z[i] - expected.z) * (src_z[i] - expected.z)
        );
        std::cout << "  Noise magnitude: " << noise << " mm" << std::endl;
    }

    // ========================================
    // Test: Query source points against target grid
    // ========================================
    std::cout << "\n========================================" << std::endl;
    std::cout << "Query Source Points Against Target Grid" << std::endl;
    std::cout << "========================================" << std::endl;

    const SourcePointCloud& source = probe_sim.getSourceCloud();

    // Query first 5 source points
    for (int i = 0; i < 5; ++i) {
        result = mesh_gpu.queryPoint(src_x[i], src_y[i], src_z[i], 2);
        if (result.valid) {
            std::cout << "Source[" << i << "] -> Target[" << result.vertex_id
                      << "] distance: " << result.distance << " mm" << std::endl;
        } else {
            std::cout << "Source[" << i << "] -> No match found" << std::endl;
        }
    }

    std::cout << "\n[ProbeSimulator] Ready for GICP registration!" << std::endl;
    std::cout << "  Source points on GPU: " << source.num_points << std::endl;
    std::cout << "  Target points on GPU: " << mesh_gpu.getNumVertices() << std::endl;

    // ========================================
    // Test GICP Grid Lookup Verification
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# GICP Registration - Grid Lookup Test #" << std::endl;
    std::cout << "########################################" << std::endl;

    GICPRegistration gicp;
    if (!gicp.initialize(&mesh_gpu, &source)) {
        std::cerr << "Failed to initialize GICP" << std::endl;
        freeMeshHost(host_mesh);
        cudaFree(feature_tex.features);
        return -1;
    }

    // Test 1: Identity transform (Source points are still "moved", so distances should be LARGE)
    std::cout << "\n>>> TEST 1: Identity Transform <<<" << std::endl;
    std::cout << "Expected: Large distances (Source was moved by T_gt)" << std::endl;
    Matrix4x4 identity;  // Default constructor gives identity
    gicp.verifyGridLookup(identity, "Identity (no correction)");

    // Test 2: Inverse of ground truth transform (Should move Source back, distances should be SMALL)
    std::cout << "\n>>> TEST 2: Inverse Ground Truth Transform <<<" << std::endl;
    std::cout << "Expected: Small distances (~noise level, ~0.5mm)" << std::endl;
    Matrix4x4 T_gt_inv = probe_sim.getGroundTruthTransform().inverse();
    gicp.verifyGridLookup(T_gt_inv, "T_gt^-1 (perfect correction)");

    // Test 3: Partial correction (half rotation, half translation)
    std::cout << "\n>>> TEST 3: Partial Correction <<<" << std::endl;
    std::cout << "Expected: Medium distances (partial alignment)" << std::endl;
    float half_angle = -5.0f * 3.14159265358979f / 180.0f;  // -5 degrees (half of 10)
    Matrix4x4 partial_rot = Matrix4x4::rotationZ(half_angle);
    Matrix4x4 partial_trans = Matrix4x4::translation(-2.5f, 0, 0);  // half of 5mm
    Matrix4x4 partial_correction = partial_trans * partial_rot;
    gicp.verifyGridLookup(partial_correction, "Partial correction (50%)");

    std::cout << "\n########################################" << std::endl;
    std::cout << "# Grid Lookup Verification Complete!   #" << std::endl;
    std::cout << "########################################" << std::endl;

    // ========================================
    // Test GICP Automatic Alignment
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# GICP Automatic Alignment Test        #" << std::endl;
    std::cout << "########################################" << std::endl;

    std::cout << "\nStarting from IDENTITY transform (no initial guess)" << std::endl;
    std::cout << "Goal: Automatically find transform to align source -> target" << std::endl;
    std::cout << "Expected: RMSE should drop from ~4mm to ~1mm (noise level)\n" << std::endl;

    // Set up GICP parameters
    GICPParams params;
    params.max_iterations = 50;
    params.convergence_threshold = 1e-6f;
    params.distance_threshold = 20.0f;  // 20mm max correspondence distance
    params.search_radius = 2;
    params.use_point_to_plane = true;
    params.verbose = true;

    // Run GICP starting from identity
    Matrix4x4 initial_guess;  // Identity
    GICPResult result_gicp = gicp.align(initial_guess, params);

    // Print results
    std::cout << "\n========================================" << std::endl;
    std::cout << "GICP Alignment Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Converged: " << (result_gicp.converged ? "YES" : "NO") << std::endl;
    std::cout << "Iterations: " << result_gicp.iterations << std::endl;
    std::cout << "Final RMSE: " << result_gicp.final_rmse << " mm" << std::endl;

    // Compare estimated transform with ground truth
    std::cout << "\n--- Estimated Transform ---" << std::endl;
    printMatrix4x4(result_gicp.final_transform);

    std::cout << "\n--- Ground Truth Inverse ---" << std::endl;
    printMatrix4x4(T_gt_inv);

    // Compute transform error
    // The estimated transform should be close to T_gt^-1
    Matrix4x4 T_error = result_gicp.final_transform * probe_sim.getGroundTruthTransform();

    std::cout << "\n--- Transform Error (T_est * T_gt, should be ~Identity) ---" << std::endl;
    printMatrix4x4(T_error);

    // Extract rotation and translation errors
    float rot_error = sqrtf(
        T_error.m[1] * T_error.m[1] +  // off-diagonal rotation elements
        T_error.m[2] * T_error.m[2] +
        T_error.m[4] * T_error.m[4] +
        T_error.m[6] * T_error.m[6] +
        T_error.m[8] * T_error.m[8] +
        T_error.m[9] * T_error.m[9]
    );

    float trans_error = sqrtf(
        T_error.m[3] * T_error.m[3] +
        T_error.m[7] * T_error.m[7] +
        T_error.m[11] * T_error.m[11]
    );

    std::cout << "\nTransform Accuracy:" << std::endl;
    std::cout << "  Rotation error (Frobenius of off-diag): " << rot_error << std::endl;
    std::cout << "  Translation error: " << trans_error << " mm" << std::endl;

    // Print RMSE history
    std::cout << "\n--- RMSE History ---" << std::endl;
    for (size_t i = 0; i < result_gicp.rmse_history.size(); ++i) {
        std::cout << "  Iter " << i << ": " << result_gicp.rmse_history[i] << " mm" << std::endl;
    }

    std::cout << "\n########################################" << std::endl;
    std::cout << "# GICP Automatic Alignment Complete!   #" << std::endl;
    std::cout << "########################################" << std::endl;

    // ========================================
    // Test CURVATURE-WEIGHTED GICP
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# Curvature-Weighted GICP Test         #" << std::endl;
    std::cout << "########################################" << std::endl;

    // Test different curvature weighting modes
    const char* mode_names[] = {"NONE", "HIGH_CURVATURE", "LOW_CURVATURE", "GAUSSIAN_AWARE", "ADAPTIVE"};
    CurvatureWeightMode modes[] = {
        CurvatureWeightMode::NONE,
        CurvatureWeightMode::HIGH_CURVATURE,
        CurvatureWeightMode::LOW_CURVATURE,
        CurvatureWeightMode::GAUSSIAN_AWARE,
        CurvatureWeightMode::ADAPTIVE
    };

    std::cout << "\n--- Comparing different curvature weighting strategies ---\n" << std::endl;

    for (int m = 0; m < 5; m++) {
        GICPParams params_curv;
        params_curv.max_iterations = 50;
        params_curv.convergence_threshold = 1e-6f;
        params_curv.distance_threshold = 20.0f;
        params_curv.search_radius = 2;
        params_curv.use_point_to_plane = true;
        params_curv.verbose = false;

        // Set curvature weighting mode
        params_curv.curvature_weight_mode = modes[m];
        params_curv.curvature_weight_scale = 1.0f;
        params_curv.min_weight = 0.1f;
        params_curv.max_weight = 10.0f;

        auto start_curv = std::chrono::high_resolution_clock::now();
        Matrix4x4 init_curv;  // Identity
        GICPResult result_curv = gicp.align(init_curv, params_curv);
        auto end_curv = std::chrono::high_resolution_clock::now();
        float time_curv = std::chrono::duration<float, std::milli>(end_curv - start_curv).count();

        // Compute transform error
        Matrix4x4 T_err = result_curv.final_transform * probe_sim.getGroundTruthTransform();
        float t_err = sqrtf(T_err.m[3]*T_err.m[3] + T_err.m[7]*T_err.m[7] + T_err.m[11]*T_err.m[11]);

        std::cout << "  Mode: " << mode_names[m] << std::endl;
        std::cout << "    Iterations: " << result_curv.iterations
                  << ", RMSE: " << result_curv.final_rmse << " mm"
                  << ", Trans Error: " << t_err << " mm"
                  << ", Time: " << time_curv << " ms" << std::endl;
    }

    std::cout << "\n########################################" << std::endl;
    std::cout << "# Curvature-Weighted GICP Complete!    #" << std::endl;
    std::cout << "########################################" << std::endl;

    // ========================================
    // Test GICP with LARGE Initial Offset
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# GICP Large Offset Test               #" << std::endl;
    std::cout << "########################################" << std::endl;

    // Test with much larger offset: 45° rotation + 20mm translation
    ProbeSimulator probe_sim_large;
    if (probe_sim_large.generateSimulatedProbe(
        host_mesh,
        45.0f,      // 45 degree rotation (much larger!)
        20.0f,      // 20mm X translation
        10.0f,      // 10mm Y translation
        5.0f,       // 5mm Z translation
        0.5f,       // noise
        true        // include normals
    )) {
        std::cout << "\n>>> Large Offset Test: 45° rotation + (20, 10, 5)mm translation <<<" << std::endl;

        probe_sim_large.printTransformInfo();

        const SourcePointCloud& source_large = probe_sim_large.getSourceCloud();

        GICPRegistration gicp_large;
        if (gicp_large.initialize(&mesh_gpu, &source_large)) {

            // Test with identity start
            std::cout << "\n--- Test A: Starting from Identity ---" << std::endl;
            GICPParams params_large;
            params_large.max_iterations = 100;
            params_large.convergence_threshold = 1e-6f;
            params_large.distance_threshold = 50.0f;  // Larger threshold for big offset
            params_large.search_radius = 3;           // Larger search radius
            params_large.verbose = true;

            Matrix4x4 identity_large;
            GICPResult result_large = gicp_large.align(identity_large, params_large);

            std::cout << "\nResult:" << std::endl;
            std::cout << "  Converged: " << (result_large.converged ? "YES" : "NO") << std::endl;
            std::cout << "  Iterations: " << result_large.iterations << std::endl;
            std::cout << "  Final RMSE: " << result_large.final_rmse << " mm" << std::endl;

            // Check transform error
            Matrix4x4 T_error_large = result_large.final_transform * probe_sim_large.getGroundTruthTransform();
            float trans_error_large = sqrtf(
                T_error_large.m[3] * T_error_large.m[3] +
                T_error_large.m[7] * T_error_large.m[7] +
                T_error_large.m[11] * T_error_large.m[11]
            );
            std::cout << "  Translation error: " << trans_error_large << " mm" << std::endl;

            if (result_large.final_rmse > 2.0f) {
                std::cout << "\n*** WARNING: Large offset registration may have failed! ***" << std::endl;
                std::cout << "*** ICP is a LOCAL optimizer - needs good initial guess for large offsets ***" << std::endl;
            }
        }
    }

    std::cout << "\n########################################" << std::endl;
    std::cout << "# Large Offset Test Complete!          #" << std::endl;
    std::cout << "########################################" << std::endl;

    // ========================================
    // Test GICP with PARTIAL OVERLAP (Stress Test)
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# Partial Overlap Stress Test          #" << std::endl;
    std::cout << "########################################" << std::endl;

    // Test 1: 10% overlap (bottom fragment only)
    std::cout << "\n>>> Test: 10% Overlap (Bottom 10% of mesh) <<<" << std::endl;
    {
        ProbeSimulator probe_partial;
        if (probe_partial.generatePartialProbe(
            host_mesh,
            10.0f,      // 10 degree rotation
            5.0f,       // 5mm X translation
            0.0f,       // 0mm Y translation
            0.0f,       // 0mm Z translation
            0.5f,       // noise
            0.0f,       // z_min_ratio: from bottom
            0.10f,      // z_max_ratio: to 10%
            true        // include normals
        )) {
            probe_partial.printTransformInfo();

            const SourcePointCloud& source_partial = probe_partial.getSourceCloud();

            GICPRegistration gicp_partial;
            if (gicp_partial.initialize(&mesh_gpu, &source_partial)) {
                GICPParams params_partial;
                params_partial.max_iterations = 100;
                params_partial.convergence_threshold = 1e-6f;
                params_partial.distance_threshold = 20.0f;
                params_partial.search_radius = 2;
                params_partial.verbose = true;

                Matrix4x4 identity_partial;
                GICPResult result_partial = gicp_partial.align(identity_partial, params_partial);

                std::cout << "\n--- 10% Overlap Result ---" << std::endl;
                std::cout << "  Converged: " << (result_partial.converged ? "YES" : "NO") << std::endl;
                std::cout << "  Iterations: " << result_partial.iterations << std::endl;
                std::cout << "  Final RMSE: " << result_partial.final_rmse << " mm" << std::endl;

                Matrix4x4 T_error_partial = result_partial.final_transform * probe_partial.getGroundTruthTransform();
                float trans_error_partial = sqrtf(
                    T_error_partial.m[3] * T_error_partial.m[3] +
                    T_error_partial.m[7] * T_error_partial.m[7] +
                    T_error_partial.m[11] * T_error_partial.m[11]
                );
                std::cout << "  Translation error: " << trans_error_partial << " mm" << std::endl;

                if (result_partial.final_rmse > 2.0f) {
                    std::cout << "  [FAILED] Partial overlap registration failed!" << std::endl;
                } else {
                    std::cout << "  [SUCCESS] Partial overlap registration succeeded!" << std::endl;
                }
            }
        }
    }

    // Test 2: 20% overlap (bottom fragment)
    std::cout << "\n>>> Test: 20% Overlap (Bottom 20% of mesh) <<<" << std::endl;
    {
        ProbeSimulator probe_partial;
        if (probe_partial.generatePartialProbe(
            host_mesh,
            10.0f, 5.0f, 0.0f, 0.0f, 0.5f,
            0.0f, 0.20f, true
        )) {
            probe_partial.printTransformInfo();

            const SourcePointCloud& source_partial = probe_partial.getSourceCloud();

            GICPRegistration gicp_partial;
            if (gicp_partial.initialize(&mesh_gpu, &source_partial)) {
                GICPParams params_partial;
                params_partial.max_iterations = 100;
                params_partial.convergence_threshold = 1e-6f;
                params_partial.distance_threshold = 20.0f;
                params_partial.search_radius = 2;
                params_partial.verbose = true;

                Matrix4x4 identity_partial;
                GICPResult result_partial = gicp_partial.align(identity_partial, params_partial);

                std::cout << "\n--- 20% Overlap Result ---" << std::endl;
                std::cout << "  Converged: " << (result_partial.converged ? "YES" : "NO") << std::endl;
                std::cout << "  Iterations: " << result_partial.iterations << std::endl;
                std::cout << "  Final RMSE: " << result_partial.final_rmse << " mm" << std::endl;

                Matrix4x4 T_error_partial = result_partial.final_transform * probe_partial.getGroundTruthTransform();
                float trans_error_partial = sqrtf(
                    T_error_partial.m[3] * T_error_partial.m[3] +
                    T_error_partial.m[7] * T_error_partial.m[7] +
                    T_error_partial.m[11] * T_error_partial.m[11]
                );
                std::cout << "  Translation error: " << trans_error_partial << " mm" << std::endl;

                if (result_partial.final_rmse > 2.0f) {
                    std::cout << "  [FAILED] Partial overlap registration failed!" << std::endl;
                } else {
                    std::cout << "  [SUCCESS] Partial overlap registration succeeded!" << std::endl;
                }
            }
        }
    }

    // Test 3: 30% overlap (middle fragment)
    std::cout << "\n>>> Test: 30% Overlap (Middle 30% of mesh) <<<" << std::endl;
    {
        ProbeSimulator probe_partial;
        if (probe_partial.generatePartialProbe(
            host_mesh,
            10.0f, 5.0f, 0.0f, 0.0f, 0.5f,
            0.35f, 0.65f, true  // Middle 30%
        )) {
            probe_partial.printTransformInfo();

            const SourcePointCloud& source_partial = probe_partial.getSourceCloud();

            GICPRegistration gicp_partial;
            if (gicp_partial.initialize(&mesh_gpu, &source_partial)) {
                GICPParams params_partial;
                params_partial.max_iterations = 100;
                params_partial.convergence_threshold = 1e-6f;
                params_partial.distance_threshold = 20.0f;
                params_partial.search_radius = 2;
                params_partial.verbose = true;

                Matrix4x4 identity_partial;
                GICPResult result_partial = gicp_partial.align(identity_partial, params_partial);

                std::cout << "\n--- 30% (Middle) Overlap Result ---" << std::endl;
                std::cout << "  Converged: " << (result_partial.converged ? "YES" : "NO") << std::endl;
                std::cout << "  Iterations: " << result_partial.iterations << std::endl;
                std::cout << "  Final RMSE: " << result_partial.final_rmse << " mm" << std::endl;

                Matrix4x4 T_error_partial = result_partial.final_transform * probe_partial.getGroundTruthTransform();
                float trans_error_partial = sqrtf(
                    T_error_partial.m[3] * T_error_partial.m[3] +
                    T_error_partial.m[7] * T_error_partial.m[7] +
                    T_error_partial.m[11] * T_error_partial.m[11]
                );
                std::cout << "  Translation error: " << trans_error_partial << " mm" << std::endl;

                if (result_partial.final_rmse > 2.0f) {
                    std::cout << "  [FAILED] Partial overlap registration failed!" << std::endl;
                } else {
                    std::cout << "  [SUCCESS] Partial overlap registration succeeded!" << std::endl;
                }
            }
        }
    }

    std::cout << "\n########################################" << std::endl;
    std::cout << "# Partial Overlap Test Complete!       #" << std::endl;
    std::cout << "########################################" << std::endl;

    // ========================================
    // Test GICP with CLINICAL ACCESSIBLE AREA (Real Surgical Simulation)
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# Clinical Accessible Area Test        #" << std::endl;
    std::cout << "# (Tibia Distal End - Surgical Access) #" << std::endl;
    std::cout << "########################################" << std::endl;

    std::cout << "\nSimulating real surgical access constraints:" << std::endl;
    std::cout << "  - Distal region only (bottom 30mm)" << std::endl;
    std::cout << "  - Exclude bottom-facing surfaces (Nz < -0.8)" << std::endl;
    std::cout << "  - Keep only anterior (Ny > 0) OR medial (Nx > 0) surfaces" << std::endl;
    std::cout << "  - These are the 'golden registration zones' for tibia surgery" << std::endl;

    // Test: Clinical Accessible Area with moderate offset
    std::cout << "\n>>> Test: Clinical Accessible Area (10° + 5mm) <<<" << std::endl;
    {
        ProbeSimulator probe_clinical;
        if (probe_clinical.generateClinicalProbe(
            host_mesh,
            10.0f,      // 10 degree rotation
            5.0f,       // 5mm X translation
            0.0f,       // 0mm Y translation
            0.0f,       // 0mm Z translation
            0.5f,       // noise
            30.0f,      // z_height_mm: keep bottom 30mm (distal end)
            -0.8f,      // nz_threshold: reject Nz < -0.8 (bottom-facing)
            true,       // require_anterior_or_medial
            true        // include normals
        )) {
            probe_clinical.printTransformInfo();

            const SourcePointCloud& source_clinical = probe_clinical.getSourceCloud();

            GICPRegistration gicp_clinical;
            if (gicp_clinical.initialize(&mesh_gpu, &source_clinical)) {
                GICPParams params_clinical;
                params_clinical.max_iterations = 100;
                params_clinical.convergence_threshold = 1e-6f;
                params_clinical.distance_threshold = 20.0f;
                params_clinical.search_radius = 2;
                params_clinical.verbose = true;

                Matrix4x4 identity_clinical;
                GICPResult result_clinical = gicp_clinical.align(identity_clinical, params_clinical);

                std::cout << "\n--- Clinical Accessible Area Result ---" << std::endl;
                std::cout << "  Converged: " << (result_clinical.converged ? "YES" : "NO") << std::endl;
                std::cout << "  Iterations: " << result_clinical.iterations << std::endl;
                std::cout << "  Final RMSE: " << result_clinical.final_rmse << " mm" << std::endl;

                Matrix4x4 T_error_clinical = result_clinical.final_transform * probe_clinical.getGroundTruthTransform();
                float trans_error_clinical = sqrtf(
                    T_error_clinical.m[3] * T_error_clinical.m[3] +
                    T_error_clinical.m[7] * T_error_clinical.m[7] +
                    T_error_clinical.m[11] * T_error_clinical.m[11]
                );
                std::cout << "  Translation error: " << trans_error_clinical << " mm" << std::endl;

                if (result_clinical.final_rmse > 2.0f) {
                    std::cout << "  [FAILED] Clinical area registration failed!" << std::endl;
                } else {
                    std::cout << "  [SUCCESS] Clinical area registration succeeded!" << std::endl;
                }
            }
        } else {
            std::cout << "  [SKIPPED] Could not generate clinical probe (check mesh orientation)" << std::endl;
        }
    }

    // Test: Clinical Accessible Area with larger offset
    std::cout << "\n>>> Test: Clinical Accessible Area (20° + 10mm) <<<" << std::endl;
    {
        ProbeSimulator probe_clinical2;
        if (probe_clinical2.generateClinicalProbe(
            host_mesh,
            20.0f,      // 20 degree rotation
            10.0f,      // 10mm X translation
            5.0f,       // 5mm Y translation
            0.0f,       // 0mm Z translation
            0.5f,       // noise
            30.0f,      // z_height_mm
            -0.8f,      // nz_threshold
            true,       // require_anterior_or_medial
            true        // include normals
        )) {
            probe_clinical2.printTransformInfo();

            const SourcePointCloud& source_clinical2 = probe_clinical2.getSourceCloud();

            GICPRegistration gicp_clinical2;
            if (gicp_clinical2.initialize(&mesh_gpu, &source_clinical2)) {
                GICPParams params_clinical2;
                params_clinical2.max_iterations = 100;
                params_clinical2.convergence_threshold = 1e-6f;
                params_clinical2.distance_threshold = 30.0f;  // Larger for bigger offset
                params_clinical2.search_radius = 3;
                params_clinical2.verbose = true;

                Matrix4x4 identity_clinical2;
                GICPResult result_clinical2 = gicp_clinical2.align(identity_clinical2, params_clinical2);

                std::cout << "\n--- Clinical Accessible Area (Large Offset) Result ---" << std::endl;
                std::cout << "  Converged: " << (result_clinical2.converged ? "YES" : "NO") << std::endl;
                std::cout << "  Iterations: " << result_clinical2.iterations << std::endl;
                std::cout << "  Final RMSE: " << result_clinical2.final_rmse << " mm" << std::endl;

                Matrix4x4 T_error_clinical2 = result_clinical2.final_transform * probe_clinical2.getGroundTruthTransform();
                float trans_error_clinical2 = sqrtf(
                    T_error_clinical2.m[3] * T_error_clinical2.m[3] +
                    T_error_clinical2.m[7] * T_error_clinical2.m[7] +
                    T_error_clinical2.m[11] * T_error_clinical2.m[11]
                );
                std::cout << "  Translation error: " << trans_error_clinical2 << " mm" << std::endl;

                if (result_clinical2.final_rmse > 2.0f) {
                    std::cout << "  [FAILED] Clinical area registration failed!" << std::endl;
                    std::cout << "  Note: Large offset with partial overlap is challenging for ICP" << std::endl;
                } else {
                    std::cout << "  [SUCCESS] Clinical area registration succeeded!" << std::endl;
                }
            }
        } else {
            std::cout << "  [SKIPPED] Could not generate clinical probe" << std::endl;
        }
    }

    std::cout << "\n########################################" << std::endl;
    std::cout << "# Clinical Accessible Area Test Done!  #" << std::endl;
    std::cout << "########################################" << std::endl;

    // ========================================
    // Test: ANCHOR POINT COARSE REGISTRATION
    // Uses the medial malleolus tip (min Z point) as anchor for coarse alignment
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# Anchor Point Coarse Registration     #" << std::endl;
    std::cout << "# (1-Point Translation Initialization) #" << std::endl;
    std::cout << "########################################" << std::endl;

    std::cout << "\nStrategy:" << std::endl;
    std::cout << "  1. Target: Use PCA-normalized mesh, find Z_min point as anchor" << std::endl;
    std::cout << "  2. Source: Transform anchor point with ground truth transform" << std::endl;
    std::cout << "  3. Coarse: Compute translation t = P_target - P_source" << std::endl;
    std::cout << "  4. Fine: Run GICP with coarse initialization" << std::endl;

    // Test: Large rotation with anchor-based coarse alignment
    std::cout << "\n>>> Test: Anchor Coarse + GICP (30° + 15mm offset) <<<" << std::endl;
    {
        ProbeSimulator probe_anchor;
        if (probe_anchor.generateSimulatedProbe(
            host_mesh,
            30.0f,      // 30 degree rotation (larger than before!)
            15.0f,      // 15mm X translation
            8.0f,       // 8mm Y translation
            0.0f,       // 0mm Z translation
            0.5f,       // noise
            true        // include normals
        )) {
            probe_anchor.printTransformInfo();

            // Get anchor points
            float3_t anchor_target = probe_anchor.getOriginalAnchorPoint();
            float3_t anchor_source = probe_anchor.getTransformedAnchorPoint();

            std::cout << "\n--- Anchor Point Info ---" << std::endl;
            std::cout << "  Target anchor (medial malleolus tip): ("
                      << anchor_target.x << ", " << anchor_target.y << ", " << anchor_target.z << ")" << std::endl;
            std::cout << "  Source anchor (after transform): ("
                      << anchor_source.x << ", " << anchor_source.y << ", " << anchor_source.z << ")" << std::endl;

            // Compute coarse translation: move source anchor to target anchor
            float3_t coarse_translation;
            coarse_translation.x = anchor_target.x - anchor_source.x;
            coarse_translation.y = anchor_target.y - anchor_source.y;
            coarse_translation.z = anchor_target.z - anchor_source.z;

            std::cout << "  Coarse translation vector: ("
                      << coarse_translation.x << ", " << coarse_translation.y << ", " << coarse_translation.z << ")" << std::endl;

            // Build coarse alignment matrix (translation only)
            Matrix4x4 T_coarse = Matrix4x4::translation(coarse_translation.x, coarse_translation.y, coarse_translation.z);

            std::cout << "\n--- Coarse Alignment Matrix ---" << std::endl;
            printMatrix4x4(T_coarse, "T_coarse");

            // Now run GICP with coarse initialization
            const SourcePointCloud& source_anchor = probe_anchor.getSourceCloud();

            GICPRegistration gicp_anchor;
            if (gicp_anchor.initialize(&mesh_gpu, &source_anchor)) {
                GICPParams params_anchor;
                params_anchor.max_iterations = 100;
                params_anchor.convergence_threshold = 1e-6f;
                params_anchor.distance_threshold = 20.0f;
                params_anchor.search_radius = 2;
                params_anchor.verbose = true;

                // Use coarse alignment as initial transform!
                GICPResult result_anchor = gicp_anchor.align(T_coarse, params_anchor);

                std::cout << "\n--- Anchor + GICP Result ---" << std::endl;
                std::cout << "  Converged: " << (result_anchor.converged ? "YES" : "NO") << std::endl;
                std::cout << "  Iterations: " << result_anchor.iterations << std::endl;
                std::cout << "  Final RMSE: " << result_anchor.final_rmse << " mm" << std::endl;

                // Compute error
                Matrix4x4 T_error = result_anchor.final_transform * probe_anchor.getGroundTruthTransform();
                float trans_error = sqrtf(
                    T_error.m[3] * T_error.m[3] +
                    T_error.m[7] * T_error.m[7] +
                    T_error.m[11] * T_error.m[11]
                );
                std::cout << "  Translation error: " << trans_error << " mm" << std::endl;

                if (result_anchor.final_rmse < 1.5f) {
                    std::cout << "  [SUCCESS] Anchor-based coarse + GICP succeeded!" << std::endl;
                } else {
                    std::cout << "  [FAILED] Anchor-based registration failed!" << std::endl;
                }

                // Compare: What if we didn't use coarse alignment?
                std::cout << "\n--- Comparison: GICP without Coarse Alignment ---" << std::endl;
                GICPRegistration gicp_no_coarse;
                if (gicp_no_coarse.initialize(&mesh_gpu, &source_anchor)) {
                    Matrix4x4 identity;
                    GICPResult result_no_coarse = gicp_no_coarse.align(identity, params_anchor);

                    std::cout << "  Without coarse: RMSE=" << result_no_coarse.final_rmse << " mm, "
                              << "iterations=" << result_no_coarse.iterations << std::endl;

                    if (result_anchor.final_rmse < result_no_coarse.final_rmse) {
                        std::cout << "  [IMPROVEMENT] Coarse alignment improved RMSE by "
                                  << (result_no_coarse.final_rmse - result_anchor.final_rmse) << " mm" << std::endl;
                    }
                }
            }
        }
    }

    // Test: Partial overlap with anchor-based coarse alignment
    std::cout << "\n>>> Test: Anchor Coarse + GICP (30% overlap, 20° + 10mm) <<<" << std::endl;
    {
        ProbeSimulator probe_partial_anchor;
        if (probe_partial_anchor.generatePartialProbe(
            host_mesh,
            20.0f,      // 20 degree rotation
            10.0f,      // 10mm X translation
            5.0f,       // 5mm Y translation
            0.0f,       // 0mm Z translation
            0.5f,       // noise
            0.0f,       // z_min_ratio (bottom)
            0.3f,       // z_max_ratio (keep bottom 30%)
            true        // include normals
        )) {
            probe_partial_anchor.printTransformInfo();

            // Get anchor points
            float3_t anchor_target = probe_partial_anchor.getOriginalAnchorPoint();
            float3_t anchor_source = probe_partial_anchor.getTransformedAnchorPoint();

            // Compute coarse translation
            float3_t coarse_translation;
            coarse_translation.x = anchor_target.x - anchor_source.x;
            coarse_translation.y = anchor_target.y - anchor_source.y;
            coarse_translation.z = anchor_target.z - anchor_source.z;

            Matrix4x4 T_coarse = Matrix4x4::translation(coarse_translation.x, coarse_translation.y, coarse_translation.z);

            std::cout << "\n  Coarse translation: ("
                      << coarse_translation.x << ", " << coarse_translation.y << ", " << coarse_translation.z << ")" << std::endl;

            const SourcePointCloud& source_partial = probe_partial_anchor.getSourceCloud();

            GICPRegistration gicp_partial;
            if (gicp_partial.initialize(&mesh_gpu, &source_partial)) {
                GICPParams params_partial;
                params_partial.max_iterations = 100;
                params_partial.convergence_threshold = 1e-6f;
                params_partial.distance_threshold = 25.0f;
                params_partial.search_radius = 3;
                params_partial.verbose = true;

                // With coarse alignment
                GICPResult result_with_coarse = gicp_partial.align(T_coarse, params_partial);

                std::cout << "\n--- Partial Overlap + Anchor Result ---" << std::endl;
                std::cout << "  With coarse: RMSE=" << result_with_coarse.final_rmse << " mm, "
                          << "converged=" << (result_with_coarse.converged ? "YES" : "NO") << std::endl;

                // Without coarse alignment
                GICPRegistration gicp_partial_no;
                if (gicp_partial_no.initialize(&mesh_gpu, &source_partial)) {
                    Matrix4x4 identity;
                    GICPResult result_no_coarse = gicp_partial_no.align(identity, params_partial);
                    std::cout << "  Without coarse: RMSE=" << result_no_coarse.final_rmse << " mm" << std::endl;

                    float improvement = result_no_coarse.final_rmse - result_with_coarse.final_rmse;
                    if (improvement > 0) {
                        std::cout << "  [IMPROVEMENT] Coarse alignment improved RMSE by " << improvement << " mm" << std::endl;
                    }
                }
            }
        }
    }

    std::cout << "\n########################################" << std::endl;
    std::cout << "# Anchor Point Registration Done!      #" << std::endl;
    std::cout << "########################################" << std::endl;

    // ========================================
    // Test: LARGE ANGLE ROTATION with GPU Brute Force Search
    // This is the key test: Translation + Rotation Search + GICP
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# LARGE ANGLE ROTATION TEST            #" << std::endl;
    std::cout << "# (Translation + GPU Rotation Search + GICP) #" << std::endl;
    std::cout << "########################################" << std::endl;

    std::cout << "\nThis test validates the complete pipeline:" << std::endl;
    std::cout << "  Step 1: Anchor-based translation (1-point coarse alignment)" << std::endl;
    std::cout << "  Step 2: GPU brute-force rotation search around anchor" << std::endl;
    std::cout << "  Step 3: GICP fine registration" << std::endl;

    // Create rotation searcher
    RotationSearch rot_search(mesh_gpu);

    // Test angles: 60°, 90°, 120°, 180°
    std::vector<float> test_angles = {60.0f, 90.0f, 120.0f, 180.0f};

    for (float test_angle : test_angles) {
        std::cout << "\n>>> Test: " << test_angle << " degree rotation + 15mm translation <<<" << std::endl;

        ProbeSimulator probe_large;
        if (!probe_large.generateSimulatedProbe(
            host_mesh,
            test_angle,     // Large rotation!
            15.0f,          // 15mm X translation
            8.0f,           // 8mm Y translation
            0.0f,           // 0mm Z translation
            0.5f,           // noise
            true            // include normals
        )) {
            std::cout << "  [SKIPPED] Failed to generate probe" << std::endl;
            continue;
        }

        probe_large.printTransformInfo();

        // Get anchor points
        float3_t anchor_target = probe_large.getOriginalAnchorPoint();
        float3_t anchor_source = probe_large.getTransformedAnchorPoint();

        std::cout << "\n--- Step 1: Anchor-based Translation ---" << std::endl;
        std::cout << "  Target anchor: (" << anchor_target.x << ", " << anchor_target.y << ", " << anchor_target.z << ")" << std::endl;
        std::cout << "  Source anchor: (" << anchor_source.x << ", " << anchor_source.y << ", " << anchor_source.z << ")" << std::endl;

        // Compute coarse translation
        float3_t coarse_trans;
        coarse_trans.x = anchor_target.x - anchor_source.x;
        coarse_trans.y = anchor_target.y - anchor_source.y;
        coarse_trans.z = anchor_target.z - anchor_source.z;

        Matrix4x4 T_translation = Matrix4x4::translation(coarse_trans.x, coarse_trans.y, coarse_trans.z);
        std::cout << "  Translation: (" << coarse_trans.x << ", " << coarse_trans.y << ", " << coarse_trans.z << ")" << std::endl;

        const SourcePointCloud& source = probe_large.getSourceCloud();

        // --- Step 2: GPU Rotation Search ---
        std::cout << "\n--- Step 2: GPU Brute-Force Rotation Search ---" << std::endl;

        // First apply translation to source points (conceptually - we'll combine matrices)
        // The rotation search needs translated source points
        // We search for rotation around the TARGET anchor point

        RotationSearch::SearchParams rot_params;
        rot_params.z_angle_min = 0.0f;
        rot_params.z_angle_max = 360.0f;
        rot_params.z_angle_step = 2.0f;      // 180 candidates
        rot_params.enable_xy_search = false;  // Z-axis only for now

        // Note: The search function expects source points that have been translated
        // We need to apply T_translation first, then search for rotation
        // For simplicity, we'll create a temporary transformed source

        // Allocate temp transformed source
        float* d_temp_x = nullptr;
        float* d_temp_y = nullptr;
        float* d_temp_z = nullptr;
        cudaMalloc(&d_temp_x, source.num_points * sizeof(float));
        cudaMalloc(&d_temp_y, source.num_points * sizeof(float));
        cudaMalloc(&d_temp_z, source.num_points * sizeof(float));

        // Download, transform, upload (simple approach)
        std::vector<float> h_x(source.num_points), h_y(source.num_points), h_z(source.num_points);
        cudaMemcpy(h_x.data(), source.points_x, source.num_points * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_y.data(), source.points_y, source.num_points * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_z.data(), source.points_z, source.num_points * sizeof(float), cudaMemcpyDeviceToHost);

        for (uint32_t i = 0; i < source.num_points; i++) {
            float3_t p(h_x[i], h_y[i], h_z[i]);
            float3_t tp = T_translation.transformPoint(p);
            h_x[i] = tp.x;
            h_y[i] = tp.y;
            h_z[i] = tp.z;
        }

        cudaMemcpy(d_temp_x, h_x.data(), source.num_points * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d_temp_y, h_y.data(), source.num_points * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d_temp_z, h_z.data(), source.num_points * sizeof(float), cudaMemcpyHostToDevice);

        // Search for best rotation
        RotationSearch::SearchResult rot_result = rot_search.search(
            d_temp_x, d_temp_y, d_temp_z,
            source.num_points,
            anchor_target,    // Rotate around target anchor
            rot_params
        );

        cudaFree(d_temp_x);
        cudaFree(d_temp_y);
        cudaFree(d_temp_z);

        std::cout << "  Best rotation found: Z=" << rot_result.best_angle_z << " deg" << std::endl;
        std::cout << "  Mean dist: " << rot_result.mean_dist_mm << "mm" << std::endl;

        // --- Step 3: Combine transforms and run GICP ---
        std::cout << "\n--- Step 3: GICP Fine Registration ---" << std::endl;

        // Combined initial transform: first translate, then rotate
        // T_combined = T_rotation * T_translation
        Matrix4x4 T_combined = rot_result.best_rotation * T_translation;

        GICPRegistration gicp_full;
        if (gicp_full.initialize(&mesh_gpu, &source)) {
            GICPParams params_full;
            params_full.max_iterations = 100;
            params_full.convergence_threshold = 1e-6f;
            params_full.distance_threshold = 20.0f;
            params_full.search_radius = 2;
            params_full.verbose = false;  // Less verbose for multiple tests

            // Run GICP with combined initial transform
            GICPResult result_full = gicp_full.align(T_combined, params_full);

            // Compute error
            Matrix4x4 T_error = result_full.final_transform * probe_large.getGroundTruthTransform();
            float trans_error = sqrtf(
                T_error.m[3] * T_error.m[3] +
                T_error.m[7] * T_error.m[7] +
                T_error.m[11] * T_error.m[11]
            );

            std::cout << "\n--- Result for " << test_angle << " degree test ---" << std::endl;
            std::cout << "  Pipeline: Translation + RotSearch + GICP" << std::endl;
            std::cout << "  Converged: " << (result_full.converged ? "YES" : "NO") << std::endl;
            std::cout << "  Iterations: " << result_full.iterations << std::endl;
            std::cout << "  Final RMSE: " << result_full.final_rmse << " mm" << std::endl;
            std::cout << "  Translation error: " << trans_error << " mm" << std::endl;

            // Compare with GICP-only (no rotation search)
            GICPRegistration gicp_only;
            if (gicp_only.initialize(&mesh_gpu, &source)) {
                // Only use translation, no rotation search
                GICPResult result_no_rot = gicp_only.align(T_translation, params_full);

                std::cout << "\n  Comparison (Translation + GICP only, no RotSearch):" << std::endl;
                std::cout << "  RMSE: " << result_no_rot.final_rmse << " mm" << std::endl;

                if (result_full.final_rmse < result_no_rot.final_rmse) {
                    float improvement = result_no_rot.final_rmse - result_full.final_rmse;
                    std::cout << "  [IMPROVEMENT] Rotation search improved RMSE by " << improvement << " mm" << std::endl;
                }
            }

            // Final verdict
            if (result_full.final_rmse < 2.0f) {
                std::cout << "  [SUCCESS] " << test_angle << " degree registration succeeded!" << std::endl;
            } else {
                std::cout << "  [FAILED] " << test_angle << " degree registration failed!" << std::endl;
            }
        }
    }

    std::cout << "\n########################################" << std::endl;
    std::cout << "# Large Angle Rotation Test Complete!  #" << std::endl;
    std::cout << "########################################" << std::endl;

    // ========================================
    // TIMING BENCHMARK & CONVERGENCE ANALYSIS
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# TIMING BENCHMARK & CONVERGENCE       #" << std::endl;
    std::cout << "# (Real-time Performance Analysis)     #" << std::endl;
    std::cout << "########################################" << std::endl;

    // Use CUDA events for precise timing
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    float milliseconds = 0;

    // Test with 90 degree rotation (representative case)
    ProbeSimulator probe_benchmark;
    if (probe_benchmark.generateSimulatedProbe(host_mesh, 90.0f, 15.0f, 8.0f, 0.0f, 0.5f, true)) {

        std::cout << "\nBenchmark Configuration:" << std::endl;
        std::cout << "  Source points: " << probe_benchmark.getSourceCloud().num_points << std::endl;
        std::cout << "  Target vertices: " << mesh_gpu.getNumVertices() << std::endl;
        std::cout << "  Test case: 90 degree rotation + 15mm translation" << std::endl;

        float3_t anchor_target_bm = probe_benchmark.getOriginalAnchorPoint();
        float3_t anchor_source_bm = probe_benchmark.getTransformedAnchorPoint();
        const SourcePointCloud& source_bm = probe_benchmark.getSourceCloud();

        // ====== Timing: Grid Build (already done, but measure for reference) ======
        std::cout << "\n--- Timing Results ---" << std::endl;
        cudaEventRecord(start);
        mesh_gpu.buildGridIndex(2.0f);  // Rebuild to measure
        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&milliseconds, start, stop);
        float grid_time = milliseconds;
        std::cout << "  Grid Build:        " << std::fixed << std::setprecision(2) << grid_time << " ms (one-time preprocess)" << std::endl;

        // ====== Timing: Translation Computation (CPU, very fast) ======
        auto t_trans_start = std::chrono::high_resolution_clock::now();
        float3_t coarse_trans_bm;
        coarse_trans_bm.x = anchor_target_bm.x - anchor_source_bm.x;
        coarse_trans_bm.y = anchor_target_bm.y - anchor_source_bm.y;
        coarse_trans_bm.z = anchor_target_bm.z - anchor_source_bm.z;
        Matrix4x4 T_trans_bm = Matrix4x4::translation(coarse_trans_bm.x, coarse_trans_bm.y, coarse_trans_bm.z);
        auto t_trans_end = std::chrono::high_resolution_clock::now();
        float trans_time = std::chrono::duration<float, std::milli>(t_trans_end - t_trans_start).count();
        std::cout << "  Translation:       " << std::fixed << std::setprecision(3) << trans_time << " ms" << std::endl;

        // ====== Timing: GPU Rotation Search ======
        // Prepare translated points
        float* d_bm_x = nullptr; float* d_bm_y = nullptr; float* d_bm_z = nullptr;
        cudaMalloc(&d_bm_x, source_bm.num_points * sizeof(float));
        cudaMalloc(&d_bm_y, source_bm.num_points * sizeof(float));
        cudaMalloc(&d_bm_z, source_bm.num_points * sizeof(float));

        std::vector<float> h_bm_x(source_bm.num_points), h_bm_y(source_bm.num_points), h_bm_z(source_bm.num_points);
        cudaMemcpy(h_bm_x.data(), source_bm.points_x, source_bm.num_points * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_bm_y.data(), source_bm.points_y, source_bm.num_points * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_bm_z.data(), source_bm.points_z, source_bm.num_points * sizeof(float), cudaMemcpyDeviceToHost);
        for (uint32_t i = 0; i < source_bm.num_points; i++) {
            float3_t p(h_bm_x[i], h_bm_y[i], h_bm_z[i]);
            float3_t tp = T_trans_bm.transformPoint(p);
            h_bm_x[i] = tp.x; h_bm_y[i] = tp.y; h_bm_z[i] = tp.z;
        }
        cudaMemcpy(d_bm_x, h_bm_x.data(), source_bm.num_points * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d_bm_y, h_bm_y.data(), source_bm.num_points * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d_bm_z, h_bm_z.data(), source_bm.num_points * sizeof(float), cudaMemcpyHostToDevice);

        RotationSearch::SearchParams rot_params_bm;
        rot_params_bm.z_angle_min = 0.0f;
        rot_params_bm.z_angle_max = 360.0f;
        rot_params_bm.z_angle_step = 2.0f;

        cudaEventRecord(start);
        RotationSearch::SearchResult rot_result_bm = rot_search.search(
            d_bm_x, d_bm_y, d_bm_z, source_bm.num_points, anchor_target_bm, rot_params_bm);
        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&milliseconds, start, stop);
        float rot_search_time = milliseconds;
        std::cout << "  Rotation Search:   " << std::fixed << std::setprecision(2) << rot_search_time << " ms (180 candidates)" << std::endl;

        cudaFree(d_bm_x); cudaFree(d_bm_y); cudaFree(d_bm_z);

        // ====== Timing: GICP Fine Registration ======
        Matrix4x4 T_combined_bm = rot_result_bm.best_rotation * T_trans_bm;

        GICPRegistration gicp_bm;
        gicp_bm.initialize(&mesh_gpu, &source_bm);

        GICPParams params_bm;
        params_bm.max_iterations = 100;
        params_bm.convergence_threshold = 1e-6f;
        params_bm.distance_threshold = 20.0f;
        params_bm.search_radius = 2;
        params_bm.verbose = false;

        cudaEventRecord(start);
        GICPResult result_bm = gicp_bm.align(T_combined_bm, params_bm);
        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&milliseconds, start, stop);
        float gicp_time = milliseconds;
        std::cout << "  GICP Fine-tune:    " << std::fixed << std::setprecision(2) << gicp_time << " ms (" << result_bm.iterations << " iterations)" << std::endl;

        float total_time = trans_time + rot_search_time + gicp_time;
        std::cout << "  ----------------------------------------" << std::endl;
        std::cout << "  TOTAL (runtime):   " << std::fixed << std::setprecision(2) << total_time << " ms" << std::endl;
        std::cout << "  Effective FPS:     " << std::fixed << std::setprecision(1) << (1000.0f / total_time) << " FPS" << std::endl;

        if (total_time < 50.0f) {
            std::cout << "  [REAL-TIME] Pipeline achieves > 20 FPS!" << std::endl;
        }

        // ====== Convergence Trajectory Data ======
        std::cout << "\n--- Convergence Trajectory (for plotting) ---" << std::endl;
        std::cout << "# Step, Method, RMSE(mm)" << std::endl;

        // Method 1: Our pipeline (Translation + RotSearch + GICP)
        // Compute RMSE at each step
        GICPRegistration gicp_traj;
        gicp_traj.initialize(&mesh_gpu, &source_bm);

        // Step 0: Initial (Identity - no correction)
        Matrix4x4 identity_traj;
        gicp_traj.findCorrespondences(identity_traj, 2);
        RegistrationStats stats_init = gicp_traj.getStats();
        std::cout << "0, Ours, " << std::fixed << std::setprecision(3) << stats_init.rmse << std::endl;

        // Step 1: After Translation only
        gicp_traj.findCorrespondences(T_trans_bm, 2);
        RegistrationStats stats_trans = gicp_traj.getStats();
        std::cout << "1, Ours, " << std::fixed << std::setprecision(3) << stats_trans.rmse << std::endl;

        // Step 2: After Translation + RotSearch
        gicp_traj.findCorrespondences(T_combined_bm, 2);
        RegistrationStats stats_rot = gicp_traj.getStats();
        std::cout << "2, Ours, " << std::fixed << std::setprecision(3) << stats_rot.rmse << std::endl;

        // Step 3+: GICP iterations
        for (size_t i = 0; i < result_bm.rmse_history.size(); i++) {
            std::cout << (3 + i) << ", Ours, " << std::fixed << std::setprecision(3) << result_bm.rmse_history[i] << std::endl;
        }

        // Method 2: Traditional (Translation + GICP only, no rotation search)
        std::cout << "\n# Traditional GICP (Translation only, no RotSearch)" << std::endl;
        GICPRegistration gicp_trad;
        gicp_trad.initialize(&mesh_gpu, &source_bm);

        params_bm.verbose = false;
        params_bm.max_iterations = 50;
        GICPResult result_trad = gicp_trad.align(T_trans_bm, params_bm);

        std::cout << "0, Traditional, " << std::fixed << std::setprecision(3) << stats_init.rmse << std::endl;
        std::cout << "1, Traditional, " << std::fixed << std::setprecision(3) << stats_trans.rmse << std::endl;
        for (size_t i = 0; i < result_trad.rmse_history.size(); i++) {
            std::cout << (2 + i) << ", Traditional, " << std::fixed << std::setprecision(3) << result_trad.rmse_history[i] << std::endl;
        }

        // Summary comparison
        std::cout << "\n--- Final Comparison ---" << std::endl;
        std::cout << "  Ours (Trans+Rot+GICP):    RMSE = " << result_bm.final_rmse << " mm, " << result_bm.iterations << " iters" << std::endl;
        std::cout << "  Traditional (Trans+GICP): RMSE = " << result_trad.final_rmse << " mm, " << result_trad.iterations << " iters" << std::endl;
    }

    // ========================================
    // 3D ROTATION TEST (Mixed Offset: Z + X/Y tilt)
    // The ultimate stress test: horizontal rotation + tilt
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# 3D ROTATION TEST (Mixed Offset)      #" << std::endl;
    std::cout << "# Z=90 deg + X=15 deg tilt + 15mm      #" << std::endl;
    std::cout << "########################################" << std::endl;

    std::cout << "\nThis is the ULTIMATE test:" << std::endl;
    std::cout << "  - Large horizontal rotation (Z=90 deg)" << std::endl;
    std::cout << "  - Plus tilt/wobble (X=15 deg)" << std::endl;
    std::cout << "  - Plus translation (15mm, 8mm, 0mm)" << std::endl;
    std::cout << "  - 3D rotation search: Z(0-360, 5 deg) + X/Y(+/-20, 10 deg)" << std::endl;

    // Test cases: various 3D rotation combinations
    struct Test3D {
        float rx, ry, rz;
        const char* name;
    };

    std::vector<Test3D> tests_3d = {
        {15.0f, 0.0f, 90.0f, "Z=90 + X=15 (forward tilt)"},
        {0.0f, 15.0f, 90.0f, "Z=90 + Y=15 (side tilt)"},
        {10.0f, 10.0f, 120.0f, "Z=120 + X=10 + Y=10 (diagonal tilt)"},
        {15.0f, -10.0f, 180.0f, "Z=180 + X=15 + Y=-10 (complex)"},
    };

    for (const auto& test : tests_3d) {
        std::cout << "\n>>> Test: " << test.name << " <<<" << std::endl;

        ProbeSimulator probe_3d;
        if (!probe_3d.generateSimulatedProbe3D(
            host_mesh,
            test.rx, test.ry, test.rz,  // 3D rotation
            15.0f, 8.0f, 0.0f,          // translation
            0.5f,                        // noise
            true                         // normals
        )) {
            std::cout << "  [SKIPPED] Failed to generate probe" << std::endl;
            continue;
        }

        probe_3d.printTransformInfo();

        float3_t anchor_target_3d = probe_3d.getOriginalAnchorPoint();
        float3_t anchor_source_3d = probe_3d.getTransformedAnchorPoint();
        const SourcePointCloud& source_3d = probe_3d.getSourceCloud();

        // Step 1: Translation
        float3_t trans_3d;
        trans_3d.x = anchor_target_3d.x - anchor_source_3d.x;
        trans_3d.y = anchor_target_3d.y - anchor_source_3d.y;
        trans_3d.z = anchor_target_3d.z - anchor_source_3d.z;
        Matrix4x4 T_trans_3d = Matrix4x4::translation(trans_3d.x, trans_3d.y, trans_3d.z);

        // Step 2: 3D Rotation Search with X/Y enabled
        float* d_3d_x = nullptr; float* d_3d_y = nullptr; float* d_3d_z = nullptr;
        cudaMalloc(&d_3d_x, source_3d.num_points * sizeof(float));
        cudaMalloc(&d_3d_y, source_3d.num_points * sizeof(float));
        cudaMalloc(&d_3d_z, source_3d.num_points * sizeof(float));

        std::vector<float> h_3d_x(source_3d.num_points), h_3d_y(source_3d.num_points), h_3d_z(source_3d.num_points);
        cudaMemcpy(h_3d_x.data(), source_3d.points_x, source_3d.num_points * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_3d_y.data(), source_3d.points_y, source_3d.num_points * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_3d_z.data(), source_3d.points_z, source_3d.num_points * sizeof(float), cudaMemcpyDeviceToHost);

        for (uint32_t i = 0; i < source_3d.num_points; i++) {
            float3_t p(h_3d_x[i], h_3d_y[i], h_3d_z[i]);
            float3_t tp = T_trans_3d.transformPoint(p);
            h_3d_x[i] = tp.x; h_3d_y[i] = tp.y; h_3d_z[i] = tp.z;
        }

        cudaMemcpy(d_3d_x, h_3d_x.data(), source_3d.num_points * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d_3d_y, h_3d_y.data(), source_3d.num_points * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d_3d_z, h_3d_z.data(), source_3d.num_points * sizeof(float), cudaMemcpyHostToDevice);

        // 3D rotation search parameters
        RotationSearch::SearchParams params_3d;
        params_3d.z_angle_min = 0.0f;
        params_3d.z_angle_max = 360.0f;
        params_3d.z_angle_step = 5.0f;       // 72 Z candidates
        params_3d.enable_xy_search = true;   // ENABLE X/Y search!
        params_3d.xy_angle_range = 20.0f;    // +/- 20 degrees
        params_3d.xy_angle_step = 10.0f;     // 5 values: -20, -10, 0, 10, 20

        // Time the 3D rotation search
        cudaEventRecord(start);
        RotationSearch::SearchResult rot_3d = rot_search.search(
            d_3d_x, d_3d_y, d_3d_z, source_3d.num_points, anchor_target_3d, params_3d);
        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&milliseconds, start, stop);

        cudaFree(d_3d_x); cudaFree(d_3d_y); cudaFree(d_3d_z);

        std::cout << "  3D Rotation Search: " << std::fixed << std::setprecision(2) << milliseconds << " ms" << std::endl;
        std::cout << "  Found: Z=" << rot_3d.best_angle_z << ", X=" << rot_3d.best_angle_x
                  << ", Y=" << rot_3d.best_angle_y << " deg" << std::endl;
        std::cout << "  Mean dist: " << rot_3d.mean_dist_mm << "mm" << std::endl;

        // Step 3: GICP
        Matrix4x4 T_combined_3d = rot_3d.best_rotation * T_trans_3d;

        GICPRegistration gicp_3d;
        gicp_3d.initialize(&mesh_gpu, &source_3d);

        GICPParams params_gicp_3d;
        params_gicp_3d.max_iterations = 100;
        params_gicp_3d.convergence_threshold = 1e-6f;
        params_gicp_3d.distance_threshold = 20.0f;
        params_gicp_3d.search_radius = 2;
        params_gicp_3d.verbose = false;

        cudaEventRecord(start);
        GICPResult result_3d = gicp_3d.align(T_combined_3d, params_gicp_3d);
        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&milliseconds, start, stop);

        float gicp_time_3d = milliseconds;

        // Compute error
        Matrix4x4 T_err_3d = result_3d.final_transform * probe_3d.getGroundTruthTransform();
        float trans_err_3d = sqrtf(
            T_err_3d.m[3] * T_err_3d.m[3] +
            T_err_3d.m[7] * T_err_3d.m[7] +
            T_err_3d.m[11] * T_err_3d.m[11]
        );

        std::cout << "\n--- Result: " << test.name << " ---" << std::endl;
        std::cout << "  GICP time: " << std::fixed << std::setprecision(2) << gicp_time_3d << " ms" << std::endl;
        std::cout << "  GICP iterations: " << result_3d.iterations << std::endl;
        std::cout << "  Final RMSE: " << result_3d.final_rmse << " mm" << std::endl;
        std::cout << "  Translation error: " << trans_err_3d << " mm" << std::endl;

        if (result_3d.final_rmse < 1.5f && result_3d.iterations <= 5) {
            std::cout << "  [SUCCESS] 3D rotation registration succeeded!" << std::endl;
        } else if (result_3d.final_rmse < 2.0f) {
            std::cout << "  [OK] Registration converged but needed more iterations" << std::endl;
        } else {
            std::cout << "  [FAILED] 3D rotation registration failed!" << std::endl;
        }
    }

    std::cout << "\n########################################" << std::endl;
    std::cout << "# 3D Rotation Test Complete!           #" << std::endl;
    std::cout << "########################################" << std::endl;

    // ========================================
    // Test: Full Sphere Hierarchical Search (Large Rotation Offsets)
    // ========================================
    std::cout << "\n########################################" << std::endl;
    std::cout << "# Full Sphere Hierarchical Search Test #" << std::endl;
    std::cout << "# (Large Rotation Offsets)             #" << std::endl;
    std::cout << "########################################" << std::endl;

    std::cout << "\nThis test verifies the hierarchical full-sphere search" << std::endl;
    std::cout << "with LARGE random 3D rotations where initial orientation is unknown." << std::endl;
    std::cout << "\nStrategy:" << std::endl;
    std::cout << "  Stage 1: Coarse search (30 deg step) -> 12^3 = 1728 candidates" << std::endl;
    std::cout << "  Stage 2: Fine refinement (+/- 20 deg, 5 deg step) -> 9^3 = 729 candidates" << std::endl;
    std::cout << "  Total: 2457 candidates, expected time: ~3ms" << std::endl;

    // Test cases with increasingly extreme rotations
    struct FullSphereTest {
        const char* name;
        float rot_x, rot_y, rot_z;   // 3D rotation in degrees
        float tx, ty, tz;             // Translation in mm
    };

    std::vector<FullSphereTest> full_sphere_tests = {
        {"Moderate 3D (30, 45, 60)",     30.0f, 45.0f, 60.0f,    10.0f, 5.0f, 3.0f},
        {"Large 3D (60, 90, 120)",       60.0f, 90.0f, 120.0f,   15.0f, 10.0f, 5.0f},
        {"Extreme 3D (90, 135, 180)",    90.0f, 135.0f, 180.0f,  20.0f, 15.0f, 10.0f},
        {"Random-like (73, 147, 251)",   73.0f, 147.0f, 251.0f,  12.0f, 8.0f, 6.0f},
        {"Upside-down (180, 0, 90)",     180.0f, 0.0f, 90.0f,    10.0f, 10.0f, 10.0f},
    };

    for (const auto& test : full_sphere_tests) {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test: " << test.name << std::endl;
        std::cout << "  Rotation: X=" << test.rot_x << ", Y=" << test.rot_y << ", Z=" << test.rot_z << " deg" << std::endl;
        std::cout << "  Translation: (" << test.tx << ", " << test.ty << ", " << test.tz << ") mm" << std::endl;
        std::cout << "========================================" << std::endl;

        // Generate probe with 3D rotation
        ProbeSimulator probe_fs;
        if (!probe_fs.generateSimulatedProbe3D(
            host_mesh,
            test.rot_x, test.rot_y, test.rot_z,
            test.tx, test.ty, test.tz,
            0.5f,   // noise
            true    // include normals
        )) {
            std::cout << "  [SKIP] Failed to generate probe" << std::endl;
            continue;
        }

        const SourcePointCloud& source_fs = probe_fs.getSourceCloud();

        // Anchor point strategy (simulating real scenario):
        // 1. Target anchor: lowest Z point (medial malleolus tip) - known from CT
        // 2. Source anchor: same vertex transformed by T_gt
        //    - In reality: doctor manually picks corresponding point on patient
        //    - In simulation: track the same vertex through transformation

        // Find target anchor (lowest Z = medial malleolus)
        float min_z_tgt = FLT_MAX;
        int anchor_idx_tgt = 0;
        for (uint32_t i = 0; i < host_mesh.num_vertices; ++i) {
            if (host_mesh.vertices_z[i] < min_z_tgt) {
                min_z_tgt = host_mesh.vertices_z[i];
                anchor_idx_tgt = i;
            }
        }
        float3_t anchor_tgt(host_mesh.vertices_x[anchor_idx_tgt],
                           host_mesh.vertices_y[anchor_idx_tgt],
                           host_mesh.vertices_z[anchor_idx_tgt]);

        // Source anchor = same vertex transformed by ground truth
        // This simulates doctor picking the medial malleolus on the patient
        Matrix4x4 T_gt_src = probe_fs.getGroundTruthTransform();
        float3_t anchor_src = T_gt_src.transformPoint(anchor_tgt);

        std::cout << "  Anchor (target): (" << anchor_tgt.x << ", " << anchor_tgt.y << ", " << anchor_tgt.z << ")" << std::endl;
        std::cout << "  Anchor (source): (" << anchor_src.x << ", " << anchor_src.y << ", " << anchor_src.z << ")" << std::endl;

        // Download source for later operations
        std::vector<float> h_fs_x(source_fs.num_points), h_fs_y(source_fs.num_points), h_fs_z(source_fs.num_points);
        cudaMemcpy(h_fs_x.data(), source_fs.points_x, source_fs.num_points * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_fs_y.data(), source_fs.points_y, source_fs.num_points * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_fs_z.data(), source_fs.points_z, source_fs.num_points * sizeof(float), cudaMemcpyDeviceToHost);

        // Step 1: Translation to align anchors
        float3_t delta = anchor_tgt - anchor_src;
        Matrix4x4 T_trans = Matrix4x4::translation(delta.x, delta.y, delta.z);

        // Apply translation to source points
        float* d_fs_x; float* d_fs_y; float* d_fs_z;
        cudaMalloc(&d_fs_x, source_fs.num_points * sizeof(float));
        cudaMalloc(&d_fs_y, source_fs.num_points * sizeof(float));
        cudaMalloc(&d_fs_z, source_fs.num_points * sizeof(float));

        for (uint32_t i = 0; i < source_fs.num_points; ++i) {
            h_fs_x[i] += delta.x;
            h_fs_y[i] += delta.y;
            h_fs_z[i] += delta.z;
        }
        cudaMemcpy(d_fs_x, h_fs_x.data(), source_fs.num_points * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d_fs_y, h_fs_y.data(), source_fs.num_points * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d_fs_z, h_fs_z.data(), source_fs.num_points * sizeof(float), cudaMemcpyHostToDevice);

        // Step 2: Full Sphere Hierarchical Search
        cudaEventRecord(start);
        RotationSearch::SearchResult rot_fs = rot_search.searchFullSphereHierarchical(
            d_fs_x, d_fs_y, d_fs_z,
            source_fs.num_points,
            anchor_tgt
        );
        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        float rot_time_fs;
        cudaEventElapsedTime(&rot_time_fs, start, stop);

        cudaFree(d_fs_x); cudaFree(d_fs_y); cudaFree(d_fs_z);

        std::cout << "\n  Rotation Search Result:" << std::endl;
        std::cout << "    Time: " << std::fixed << std::setprecision(2) << rot_time_fs << " ms" << std::endl;
        std::cout << "    Found: X=" << rot_fs.best_angle_x << ", Y=" << rot_fs.best_angle_y
                  << ", Z=" << rot_fs.best_angle_z << " deg" << std::endl;
        std::cout << "    Mean dist: " << rot_fs.mean_dist_mm << "mm" << std::endl;

        // Step 3: GICP fine registration
        Matrix4x4 T_combined_fs = rot_fs.best_rotation * T_trans;

        GICPRegistration gicp_fs;
        gicp_fs.initialize(&mesh_gpu, &source_fs);

        GICPParams params_fs;
        params_fs.max_iterations = 100;
        params_fs.convergence_threshold = 1e-6f;
        params_fs.distance_threshold = 20.0f;
        params_fs.search_radius = 2;
        params_fs.verbose = false;

        cudaEventRecord(start);
        GICPResult result_fs = gicp_fs.align(T_combined_fs, params_fs);
        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        float gicp_time_fs;
        cudaEventElapsedTime(&gicp_time_fs, start, stop);

        // Compute error
        Matrix4x4 T_err_fs = result_fs.final_transform * probe_fs.getGroundTruthTransform();
        float trans_err_fs = sqrtf(
            T_err_fs.m[3] * T_err_fs.m[3] +
            T_err_fs.m[7] * T_err_fs.m[7] +
            T_err_fs.m[11] * T_err_fs.m[11]
        );

        std::cout << "\n  Final Result:" << std::endl;
        std::cout << "    Total time: " << std::fixed << std::setprecision(2)
                  << (rot_time_fs + gicp_time_fs) << " ms (Rot: " << rot_time_fs
                  << " + GICP: " << gicp_time_fs << ")" << std::endl;
        std::cout << "    GICP iterations: " << result_fs.iterations << std::endl;
        std::cout << "    Final RMSE: " << result_fs.final_rmse << " mm" << std::endl;
        std::cout << "    Translation error: " << trans_err_fs << " mm" << std::endl;

        if (result_fs.final_rmse < 1.5f) {
            std::cout << "    [SUCCESS] Full sphere hierarchical search works!" << std::endl;
        } else if (result_fs.final_rmse < 3.0f) {
            std::cout << "    [PARTIAL] Converged but with higher error" << std::endl;
        } else {
            std::cout << "    [FAILED] Registration failed for this extreme case" << std::endl;
        }
    }

    std::cout << "\n########################################" << std::endl;
    std::cout << "# Full Sphere Test Complete!           #" << std::endl;
    std::cout << "########################################" << std::endl;

    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    std::cout << "\n########################################" << std::endl;
    std::cout << "# ALL TESTS COMPLETE!                  #" << std::endl;
    std::cout << "########################################" << std::endl;

    // Cleanup
    freeMeshHost(host_mesh);
    cudaFree(feature_tex.features);

    std::cout << "\n[Main] Done!" << std::endl;

    // 等待用户按键后关闭
    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();

    return 0;
}
