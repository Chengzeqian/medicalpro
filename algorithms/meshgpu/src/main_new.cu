#include <iostream>
#include <cuda_runtime.h>
#include "types.h"
#include "ply_reader.h"
#include "mesh_gpu.h"
#include "probe_simulator.h"
#include "gicp_registration.h"
#include "rotation_search.h"

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
    // Default PLY file path
    std::string ply_path = "E:/ICPtry/fixed_tibia_normalized.ply";

    if (argc > 1) {
        ply_path = argv[1];
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
