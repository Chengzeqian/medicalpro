/**
 * MeshGPU Real-time Visualization Demo
 *
 * This program demonstrates real-time point cloud registration visualization
 * using Open3D C++ API. It shows:
 * 1. Target mesh (tibia) in white
 * 2. Source point cloud (probe) in red
 * 3. Real-time registration updates
 *
 * The visualization runs at 60+ FPS even with 50k points.
 */

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
#include "visualizer.h"
#include "backend_runtime_cli.h"

// ============================================================================
// Helper Functions
// ============================================================================

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

// ============================================================================
// Main Program
// ============================================================================
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
    if (!mesh_gpu::configureBackendForCudaOnlyExecutable(cli, "mesh_gpu_realtime")) {
        return -2;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "MeshGPU Real-time Visualization Demo" << std::endl;
    std::cout << "========================================" << std::endl;

    // Default paths
    std::string ply_path = "E:/ICPtry/fixed_tibia_normalized.ply";
    if (!cli.positional_args.empty()) {
        ply_path = cli.positional_args[0];
    }

    std::cout << "Target mesh: " << ply_path << std::endl;
    printDeviceInfo();

    // ========================================
    // Load Target Mesh
    // ========================================
    std::cout << "[Step 1] Loading target mesh..." << std::endl;

    MeshSoA host_mesh;
    if (!PLYReader::readPLY(ply_path, host_mesh)) {
        std::cerr << "Failed to read PLY file: " << ply_path << std::endl;
        return -1;
    }
    std::cout << "  Loaded: " << host_mesh.num_vertices << " vertices, "
              << host_mesh.num_faces << " faces" << std::endl;

    // ========================================
    // Initialize GPU Mesh
    // ========================================
    std::cout << "\n[Step 2] Initializing GPU mesh..." << std::endl;

    MeshGPU mesh_gpu;
    if (!mesh_gpu.initialize(host_mesh)) {
        std::cerr << "Failed to initialize MeshGPU" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }

    // Build grid index for O(1) query
    if (!mesh_gpu.buildGridIndex(2.0f)) {
        std::cerr << "Failed to build grid index" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }
    std::cout << "  Grid index built (2mm cell size)" << std::endl;

    // ========================================
    // Step 1: Initialize Visualizer
    // ========================================
    std::cout << "\n[Step 3] Initializing Open3D visualizer..." << std::endl;

    RealtimeVisualizer vis;
    if (!vis.initialize(ply_path, "MeshGPU Real-time Registration", 1280, 720)) {
        std::cerr << "Failed to initialize visualizer" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }

    // Set colors
    vis.setTargetColor(0.8, 0.8, 0.9);  // Light gray/blue for bone
    vis.setSourceColor(1.0, 0.2, 0.2);  // Red for probe points
    vis.setPointSize(4.0);
    vis.addCoordinateAxes(30.0);

    std::cout << "  Visualizer ready!" << std::endl;

    // ========================================
    // Generate Simulated Probe Data
    // ========================================
    std::cout << "\n[Step 4] Generating simulated probe data..." << std::endl;

    ProbeSimulator probe_sim;

    // Generate probe with moderate rotation and translation
    // These simulate a real surgical scenario where the probe
    // is roughly positioned but not aligned
    float rotation_deg = 45.0f;  // Z-axis rotation
    float tx = 10.0f, ty = 5.0f, tz = 0.0f;  // Translation (mm)
    float noise = 0.5f;  // Gaussian noise (mm)

    if (!probe_sim.generateSimulatedProbe(host_mesh, rotation_deg, tx, ty, tz, noise, true)) {
        std::cerr << "Failed to generate simulated probe" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }

    const SourcePointCloud& source = probe_sim.getSourceCloud();
    std::cout << "  Generated " << source.num_points << " probe points" << std::endl;
    std::cout << "  Ground truth: rotation=" << rotation_deg << " deg, "
              << "translation=(" << tx << ", " << ty << ", " << tz << ") mm" << std::endl;

    // ========================================
    // Initialize Registration
    // ========================================
    std::cout << "\n[Step 5] Setting up registration..." << std::endl;

    GICPRegistration gicp;
    if (!gicp.initialize(&mesh_gpu, &source)) {
        std::cerr << "Failed to initialize GICP" << std::endl;
        freeMeshHost(host_mesh);
        return -1;
    }

    RotationSearch rot_search(mesh_gpu);

    // Get anchor points for coarse alignment
    float3_t anchor_target = probe_sim.getOriginalAnchorPoint();
    float3_t anchor_source = probe_sim.getTransformedAnchorPoint();

    // ========================================
    // Step 2 & 3: Main Visualization Loop
    // ========================================
    std::cout << "\n========================================" << std::endl;
    std::cout << "Starting Real-time Visualization Loop" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nControls:" << std::endl;
    std::cout << "  - Left mouse: Rotate view" << std::endl;
    std::cout << "  - Right mouse: Pan" << std::endl;
    std::cout << "  - Scroll: Zoom" << std::endl;
    std::cout << "  - Close window to exit" << std::endl;
    std::cout << "\nVisualization shows:" << std::endl;
    std::cout << "  - White/Gray: Target bone mesh" << std::endl;
    std::cout << "  - Red: Source probe points (moving)" << std::endl;
    std::cout << "\n";

    // Animation state
    enum class AnimationPhase {
        INITIAL,        // Show initial misalignment
        TRANSLATION,    // Show after anchor translation
        ROTATION,       // Show after rotation search
        GICP,           // Show GICP iterations
        FINAL,          // Hold at final result
        LOOP            // Loop back
    };

    AnimationPhase phase = AnimationPhase::INITIAL;
    int phase_frames = 0;
    int gicp_iteration = 0;

    // Current transform being displayed
    Matrix4x4 current_transform;

    // Precompute transforms for animation
    float3_t coarse_trans;
    coarse_trans.x = anchor_target.x - anchor_source.x;
    coarse_trans.y = anchor_target.y - anchor_source.y;
    coarse_trans.z = anchor_target.z - anchor_source.z;
    Matrix4x4 T_translation = Matrix4x4::translation(coarse_trans.x, coarse_trans.y, coarse_trans.z);

    // Prepare translated source for rotation search
    std::vector<float> h_trans_x(source.num_points);
    std::vector<float> h_trans_y(source.num_points);
    std::vector<float> h_trans_z(source.num_points);

    // Download original source points
    cudaMemcpy(h_trans_x.data(), source.points_x, source.num_points * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_trans_y.data(), source.points_y, source.num_points * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_trans_z.data(), source.points_z, source.num_points * sizeof(float), cudaMemcpyDeviceToHost);

    // Apply translation
    for (uint32_t i = 0; i < source.num_points; i++) {
        float3_t p(h_trans_x[i], h_trans_y[i], h_trans_z[i]);
        float3_t tp = T_translation.transformPoint(p);
        h_trans_x[i] = tp.x;
        h_trans_y[i] = tp.y;
        h_trans_z[i] = tp.z;
    }

    // Upload to GPU for rotation search
    float* d_trans_x = nullptr;
    float* d_trans_y = nullptr;
    float* d_trans_z = nullptr;
    cudaMalloc(&d_trans_x, source.num_points * sizeof(float));
    cudaMalloc(&d_trans_y, source.num_points * sizeof(float));
    cudaMalloc(&d_trans_z, source.num_points * sizeof(float));
    cudaMemcpy(d_trans_x, h_trans_x.data(), source.num_points * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_trans_y, h_trans_y.data(), source.num_points * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_trans_z, h_trans_z.data(), source.num_points * sizeof(float), cudaMemcpyHostToDevice);

    // Perform rotation search
    RotationSearch::SearchParams rot_params;
    rot_params.z_angle_min = 0.0f;
    rot_params.z_angle_max = 360.0f;
    rot_params.z_angle_step = 2.0f;
    rot_params.enable_xy_search = false;

    RotationSearch::SearchResult rot_result = rot_search.search(
        d_trans_x, d_trans_y, d_trans_z,
        source.num_points,
        anchor_target,
        rot_params
    );

    Matrix4x4 T_rotation = rot_result.best_rotation;
    Matrix4x4 T_combined = T_rotation * T_translation;

    cudaFree(d_trans_x);
    cudaFree(d_trans_y);
    cudaFree(d_trans_z);

    // Run full GICP and store history
    GICPParams gicp_params;
    gicp_params.max_iterations = 50;
    gicp_params.convergence_threshold = 1e-6f;
    gicp_params.distance_threshold = 20.0f;
    gicp_params.search_radius = 2;
    gicp_params.verbose = false;

    GICPResult gicp_result = gicp.align(T_combined, gicp_params);

    std::cout << "Registration complete:" << std::endl;
    std::cout << "  Rotation found: " << rot_result.best_angle_z << " deg" << std::endl;
    std::cout << "  GICP iterations: " << gicp_result.iterations << std::endl;
    std::cout << "  Final RMSE: " << gicp_result.final_rmse << " mm" << std::endl;

    // Frame timing
    auto last_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float fps = 0.0f;

    // Download source points for visualization
    std::vector<float> h_src_x(source.num_points);
    std::vector<float> h_src_y(source.num_points);
    std::vector<float> h_src_z(source.num_points);
    cudaMemcpy(h_src_x.data(), source.points_x, source.num_points * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_src_y.data(), source.points_y, source.num_points * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_src_z.data(), source.points_z, source.num_points * sizeof(float), cudaMemcpyDeviceToHost);

    // ========================================
    // Main Render Loop
    // ========================================
    while (vis.isRunning()) {

        // Update transform based on animation phase
        switch (phase) {
            case AnimationPhase::INITIAL:
                // Show initial misalignment (identity transform)
                current_transform = Matrix4x4();  // Identity
                phase_frames++;
                if (phase_frames > 90) {  // ~1.5 seconds at 60fps
                    phase = AnimationPhase::TRANSLATION;
                    phase_frames = 0;
                    std::cout << "\r[Phase] Applying anchor translation...     " << std::flush;
                }
                break;

            case AnimationPhase::TRANSLATION:
                // Show after anchor-based translation
                current_transform = T_translation;
                phase_frames++;
                if (phase_frames > 60) {  // ~1 second
                    phase = AnimationPhase::ROTATION;
                    phase_frames = 0;
                    std::cout << "\r[Phase] Applying rotation search result... " << std::flush;
                }
                break;

            case AnimationPhase::ROTATION:
                // Show after rotation search
                current_transform = T_combined;
                phase_frames++;
                if (phase_frames > 60) {
                    phase = AnimationPhase::GICP;
                    phase_frames = 0;
                    gicp_iteration = 0;
                    std::cout << "\r[Phase] Running GICP refinement...         " << std::flush;
                }
                break;

            case AnimationPhase::GICP:
                // Animate through GICP iterations (fast)
                // We interpolate between T_combined and final_transform
                if (gicp_iteration < gicp_result.iterations) {
                    float t = (float)gicp_iteration / (float)gicp_result.iterations;
                    // Linear interpolation of transforms (simplified)
                    for (int i = 0; i < 16; i++) {
                        current_transform.m[i] = T_combined.m[i] * (1.0f - t) +
                                                  gicp_result.final_transform.m[i] * t;
                    }
                    phase_frames++;
                    if (phase_frames >= 2) {  // ~30fps for GICP animation
                        phase_frames = 0;
                        gicp_iteration++;
                    }
                } else {
                    phase = AnimationPhase::FINAL;
                    phase_frames = 0;
                    std::cout << "\r[Phase] Final registration result          " << std::flush;
                }
                break;

            case AnimationPhase::FINAL:
                // Hold at final result
                current_transform = gicp_result.final_transform;
                phase_frames++;
                if (phase_frames > 180) {  // ~3 seconds
                    phase = AnimationPhase::LOOP;
                    phase_frames = 0;
                }
                break;

            case AnimationPhase::LOOP:
                // Reset to beginning
                phase = AnimationPhase::INITIAL;
                phase_frames = 0;
                std::cout << "\r[Phase] Showing initial misalignment...    " << std::flush;
                break;
        }

        // ========================================
        // Step 2: Update Source Cloud Data
        // ========================================
        // Update source cloud with current transform applied
        vis.updateSourceCloud(h_src_x.data(), h_src_y.data(), h_src_z.data(), source.num_points);
        vis.applyTransform(current_transform.m);  // Pass raw float[16] array

        // ========================================
        // Step 3: Render Frame
        // ========================================
        if (!vis.render()) {
            break;  // Window closed
        }

        // FPS calculation
        frame_count++;
        auto now = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration<float>(now - last_time).count();
        if (elapsed >= 1.0f) {
            fps = frame_count / elapsed;
            frame_count = 0;
            last_time = now;
        }
    }

    std::cout << "\n\n========================================" << std::endl;
    std::cout << "Visualization Complete!" << std::endl;
    std::cout << "========================================" << std::endl;

    // Cleanup
    freeMeshHost(host_mesh);

    std::cout << "[Main] Done!" << std::endl;
    return 0;
}
