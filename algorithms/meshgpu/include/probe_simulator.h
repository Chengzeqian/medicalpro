#pragma once

#include "types.h"
#include <cuda_runtime.h>
#include <random>
#include <iostream>
#include <cmath>

// ============================================================================
// Probe Simulator
// Generates simulated probe point cloud from target mesh for registration testing
// ============================================================================

class ProbeSimulator {
public:
    ProbeSimulator() : source_initialized_(false), anchor_set_(false), rng_(std::random_device{}()) {}
    ~ProbeSimulator() { freeSourceMemory(); }

    // Disable copy
    ProbeSimulator(const ProbeSimulator&) = delete;
    ProbeSimulator& operator=(const ProbeSimulator&) = delete;

    // Generate simulated probe data from target mesh
    // 1. Copy target vertices to source
    // 2. Apply known transformation (rotation + translation)
    // 3. Add Gaussian noise
    // 4. Upload to GPU
    bool generateSimulatedProbe(
        const MeshSoA& target_mesh,          // Target mesh (on host)
        float rotation_z_deg,                 // Rotation around Z axis (degrees)
        float translation_x,                  // X translation (mm)
        float translation_y,                  // Y translation (mm)
        float translation_z,                  // Z translation (mm)
        float noise_stddev,                   // Gaussian noise std dev (mm)
        bool include_normals = true           // Whether to transform normals too
    );

    // Generate with default test parameters (10° Z rotation, 5mm X translation)
    bool generateDefaultTestProbe(const MeshSoA& target_mesh, float noise_stddev = 0.5f);

    // Generate simulated probe with FULL 3D rotation (X, Y, Z axes)
    // This simulates "doctor hand shake" in all directions
    bool generateSimulatedProbe3D(
        const MeshSoA& target_mesh,          // Target mesh (on host)
        float rotation_x_deg,                 // Rotation around X axis (degrees) - tilt forward/back
        float rotation_y_deg,                 // Rotation around Y axis (degrees) - tilt left/right
        float rotation_z_deg,                 // Rotation around Z axis (degrees) - horizontal rotation
        float translation_x,                  // X translation (mm)
        float translation_y,                  // Y translation (mm)
        float translation_z,                  // Z translation (mm)
        float noise_stddev,                   // Gaussian noise std dev (mm)
        bool include_normals = true           // Whether to transform normals too
    );

    // Generate PARTIAL probe data (simulates partial overlap / microinvasive surgery)
    // Only keeps points within a specified Z range (relative to mesh bbox)
    // z_min_ratio/z_max_ratio: 0.0 = bottom of mesh, 1.0 = top of mesh
    bool generatePartialProbe(
        const MeshSoA& target_mesh,          // Target mesh (on host)
        float rotation_z_deg,                 // Rotation around Z axis (degrees)
        float translation_x,                  // X translation (mm)
        float translation_y,                  // Y translation (mm)
        float translation_z,                  // Z translation (mm)
        float noise_stddev,                   // Gaussian noise std dev (mm)
        float z_min_ratio,                    // Keep points where z >= bbox.min + (max-min)*z_min_ratio
        float z_max_ratio,                    // Keep points where z <= bbox.min + (max-min)*z_max_ratio
        bool include_normals = true           // Whether to transform normals too
    );

    // Generate CLINICAL ACCESSIBLE probe data (simulates real surgical access)
    // Filters by:
    // 1. Z range: distal end only (bottom z_height_mm)
    // 2. Normal direction: excludes bottom-facing (joint interior) surfaces
    // 3. Keeps only anterior (Ny > 0) and medial (Nx > 0) accessible surfaces
    bool generateClinicalProbe(
        const MeshSoA& target_mesh,          // Target mesh (on host)
        float rotation_z_deg,                 // Rotation around Z axis (degrees)
        float translation_x,                  // X translation (mm)
        float translation_y,                  // Y translation (mm)
        float translation_z,                  // Z translation (mm)
        float noise_stddev,                   // Gaussian noise std dev (mm)
        float z_height_mm,                    // Keep points within this height from bottom (mm)
        float nz_threshold,                   // Reject points with Nz < threshold (e.g. -0.8)
        bool require_anterior_or_medial,      // If true, only keep Ny>0 OR Nx>0
        bool include_normals = true           // Whether to transform normals too
    );

    // Get source point cloud (on GPU)
    const SourcePointCloud& getSourceCloud() const { return source_; }

    // Get ground truth transformation matrix
    const Matrix4x4& getGroundTruthTransform() const { return source_.ground_truth_transform; }

    // Download source to host arrays (for debugging/visualization)
    bool downloadToHost(float* h_x, float* h_y, float* h_z, uint32_t max_points);

    // Get statistics
    uint32_t getNumPoints() const { return source_.num_points; }
    float getNoiseStdDev() const { return source_.noise_stddev; }
    bool isInitialized() const { return source_initialized_; }

    // Print transformation info
    void printTransformInfo() const;

    // ========================================================================
    // Anchor Point Support (for coarse registration)
    // ========================================================================

    // Get the transformed anchor point (e.g., medial malleolus tip after transformation)
    // This simulates "doctor clicking on the landmark in the probe data"
    float3_t getTransformedAnchorPoint() const { return transformed_anchor_; }

    // Get the original anchor point from target mesh
    float3_t getOriginalAnchorPoint() const { return original_anchor_; }

    // Check if anchor point is set
    bool hasAnchorPoint() const { return anchor_set_; }

private:
    void freeSourceMemory();

    // Find the point with minimum Z in target mesh (medial malleolus tip)
    float3_t findMinZPoint(const MeshSoA& target_mesh);

    SourcePointCloud source_;
    bool source_initialized_;
    std::mt19937 rng_;

    // Anchor point support
    float3_t original_anchor_;      // Anchor point in target mesh coordinates
    float3_t transformed_anchor_;   // Anchor point after transformation (simulates doctor's click)
    bool anchor_set_;
};

// ============================================================================
// Implementation
// ============================================================================

inline void ProbeSimulator::freeSourceMemory() {
    if (source_initialized_ && source_.on_device) {
        if (source_.points_x) cudaFree(source_.points_x);
        if (source_.points_y) cudaFree(source_.points_y);
        if (source_.points_z) cudaFree(source_.points_z);
        if (source_.normals_x) cudaFree(source_.normals_x);
        if (source_.normals_y) cudaFree(source_.normals_y);
        if (source_.normals_z) cudaFree(source_.normals_z);
    }
    source_.points_x = source_.points_y = source_.points_z = nullptr;
    source_.normals_x = source_.normals_y = source_.normals_z = nullptr;
    source_.num_points = 0;
    source_.on_device = false;
    source_initialized_ = false;
    anchor_set_ = false;
}

// Find the point with minimum Z in target mesh (medial malleolus tip for tibia)
inline float3_t ProbeSimulator::findMinZPoint(const MeshSoA& target_mesh) {
    if (target_mesh.num_vertices == 0) {
        return float3_t(0, 0, 0);
    }

    uint32_t min_z_idx = 0;
    float min_z = target_mesh.vertices_z[0];

    for (uint32_t i = 1; i < target_mesh.num_vertices; ++i) {
        if (target_mesh.vertices_z[i] < min_z) {
            min_z = target_mesh.vertices_z[i];
            min_z_idx = i;
        }
    }

    return float3_t(
        target_mesh.vertices_x[min_z_idx],
        target_mesh.vertices_y[min_z_idx],
        target_mesh.vertices_z[min_z_idx]
    );
}

inline bool ProbeSimulator::generateSimulatedProbe(
    const MeshSoA& target_mesh,
    float rotation_z_deg,
    float translation_x,
    float translation_y,
    float translation_z,
    float noise_stddev,
    bool include_normals
) {
    // Free previous data
    freeSourceMemory();

    uint32_t num_points = target_mesh.num_vertices;
    if (num_points == 0) {
        std::cerr << "[ProbeSimulator] Error: Target mesh has no vertices" << std::endl;
        return false;
    }

    std::cout << "[ProbeSimulator] Generating simulated probe data..." << std::endl;
    std::cout << "  Source points: " << num_points << std::endl;

    // Build transformation matrix: T = Translation * Rotation
    float angle_rad = rotation_z_deg * 3.14159265358979f / 180.0f;
    Matrix4x4 rotation = Matrix4x4::rotationZ(angle_rad);
    Matrix4x4 translation = Matrix4x4::translation(translation_x, translation_y, translation_z);
    Matrix4x4 transform = translation * rotation;  // Apply rotation first, then translation

    source_.ground_truth_transform = transform;
    source_.noise_stddev = noise_stddev;
    source_.num_points = num_points;

    // Find and transform anchor point (medial malleolus tip = min Z point)
    original_anchor_ = findMinZPoint(target_mesh);
    transformed_anchor_ = transform.transformPoint(original_anchor_);
    anchor_set_ = true;

    std::cout << "  Anchor point (Target): (" << original_anchor_.x << ", "
              << original_anchor_.y << ", " << original_anchor_.z << ")" << std::endl;
    std::cout << "  Anchor point (Source): (" << transformed_anchor_.x << ", "
              << transformed_anchor_.y << ", " << transformed_anchor_.z << ")" << std::endl;

    std::cout << "  Rotation Z: " << rotation_z_deg << " degrees" << std::endl;
    std::cout << "  Translation: (" << translation_x << ", " << translation_y << ", " << translation_z << ") mm" << std::endl;
    std::cout << "  Noise StdDev: " << noise_stddev << " mm" << std::endl;

    // Allocate host memory for transformed points
    std::vector<float> h_points_x(num_points);
    std::vector<float> h_points_y(num_points);
    std::vector<float> h_points_z(num_points);
    std::vector<float> h_normals_x, h_normals_y, h_normals_z;

    if (include_normals && target_mesh.normals_x) {
        h_normals_x.resize(num_points);
        h_normals_y.resize(num_points);
        h_normals_z.resize(num_points);
    }

    // Create Gaussian noise generator
    std::normal_distribution<float> noise_dist(0.0f, noise_stddev);

    // Transform each point and add noise
    for (uint32_t i = 0; i < num_points; ++i) {
        // Get original point
        float3_t p(target_mesh.vertices_x[i],
                   target_mesh.vertices_y[i],
                   target_mesh.vertices_z[i]);

        // Apply transformation
        float3_t p_transformed = transform.transformPoint(p);

        // Add Gaussian noise
        h_points_x[i] = p_transformed.x + noise_dist(rng_);
        h_points_y[i] = p_transformed.y + noise_dist(rng_);
        h_points_z[i] = p_transformed.z + noise_dist(rng_);

        // Transform normals (no translation, no noise)
        if (include_normals && target_mesh.normals_x) {
            float3_t n(target_mesh.normals_x[i],
                       target_mesh.normals_y[i],
                       target_mesh.normals_z[i]);
            float3_t n_transformed = transform.transformVector(n).normalized();
            h_normals_x[i] = n_transformed.x;
            h_normals_y[i] = n_transformed.y;
            h_normals_z[i] = n_transformed.z;
        }
    }

    // Allocate GPU memory
    size_t bytes = num_points * sizeof(float);
    cudaError_t err;

    err = cudaMalloc(&source_.points_x, bytes);
    if (err != cudaSuccess) {
        std::cerr << "[ProbeSimulator] cudaMalloc failed for points_x: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMalloc(&source_.points_y, bytes);
    if (err != cudaSuccess) {
        cudaFree(source_.points_x);
        std::cerr << "[ProbeSimulator] cudaMalloc failed for points_y: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMalloc(&source_.points_z, bytes);
    if (err != cudaSuccess) {
        cudaFree(source_.points_x);
        cudaFree(source_.points_y);
        std::cerr << "[ProbeSimulator] cudaMalloc failed for points_z: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    // Upload points to GPU
    cudaMemcpy(source_.points_x, h_points_x.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(source_.points_y, h_points_y.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(source_.points_z, h_points_z.data(), bytes, cudaMemcpyHostToDevice);

    // Handle normals
    if (include_normals && !h_normals_x.empty()) {
        cudaMalloc(&source_.normals_x, bytes);
        cudaMalloc(&source_.normals_y, bytes);
        cudaMalloc(&source_.normals_z, bytes);
        cudaMemcpy(source_.normals_x, h_normals_x.data(), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(source_.normals_y, h_normals_y.data(), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(source_.normals_z, h_normals_z.data(), bytes, cudaMemcpyHostToDevice);
    } else {
        source_.normals_x = source_.normals_y = source_.normals_z = nullptr;
    }

    source_.on_device = true;
    source_initialized_ = true;

    std::cout << "[ProbeSimulator] Source point cloud uploaded to GPU ("
              << (bytes * 3) / 1024 << " KB for positions)" << std::endl;

    return true;
}

inline bool ProbeSimulator::generateDefaultTestProbe(const MeshSoA& target_mesh, float noise_stddev) {
    // Default: 10° Z rotation, 5mm X translation
    return generateSimulatedProbe(
        target_mesh,
        10.0f,      // rotation_z_deg
        5.0f,       // translation_x
        0.0f,       // translation_y
        0.0f,       // translation_z
        noise_stddev,
        true        // include_normals
    );
}

inline bool ProbeSimulator::downloadToHost(float* h_x, float* h_y, float* h_z, uint32_t max_points) {
    if (!source_initialized_ || !source_.on_device) {
        std::cerr << "[ProbeSimulator] No source data to download" << std::endl;
        return false;
    }

    uint32_t count = std::min(max_points, source_.num_points);
    size_t bytes = count * sizeof(float);

    cudaMemcpy(h_x, source_.points_x, bytes, cudaMemcpyDeviceToHost);
    cudaMemcpy(h_y, source_.points_y, bytes, cudaMemcpyDeviceToHost);
    cudaMemcpy(h_z, source_.points_z, bytes, cudaMemcpyDeviceToHost);

    return true;
}

inline void ProbeSimulator::printTransformInfo() const {
    if (!source_initialized_) {
        std::cout << "[ProbeSimulator] No transform (not initialized)" << std::endl;
        return;
    }

    const Matrix4x4& T = source_.ground_truth_transform;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Ground Truth Transformation Matrix" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "| " << T.m[0] << "\t" << T.m[1] << "\t" << T.m[2] << "\t" << T.m[3] << " |" << std::endl;
    std::cout << "| " << T.m[4] << "\t" << T.m[5] << "\t" << T.m[6] << "\t" << T.m[7] << " |" << std::endl;
    std::cout << "| " << T.m[8] << "\t" << T.m[9] << "\t" << T.m[10] << "\t" << T.m[11] << " |" << std::endl;
    std::cout << "| " << T.m[12] << "\t" << T.m[13] << "\t" << T.m[14] << "\t" << T.m[15] << " |" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Noise StdDev: " << source_.noise_stddev << " mm" << std::endl;
    std::cout << "Num Points: " << source_.num_points << std::endl;
    std::cout << "========================================\n" << std::endl;
}

// ============================================================================
// Partial Probe Generation (for partial overlap testing)
// ============================================================================
inline bool ProbeSimulator::generatePartialProbe(
    const MeshSoA& target_mesh,
    float rotation_z_deg,
    float translation_x,
    float translation_y,
    float translation_z,
    float noise_stddev,
    float z_min_ratio,
    float z_max_ratio,
    bool include_normals
) {
    // Free previous data
    freeSourceMemory();

    if (target_mesh.num_vertices == 0) {
        std::cerr << "[ProbeSimulator] Error: Target mesh has no vertices" << std::endl;
        return false;
    }

    // First, compute bounding box of target mesh
    float z_min = target_mesh.vertices_z[0];
    float z_max = target_mesh.vertices_z[0];
    for (uint32_t i = 1; i < target_mesh.num_vertices; ++i) {
        float z = target_mesh.vertices_z[i];
        if (z < z_min) z_min = z;
        if (z > z_max) z_max = z;
    }

    float z_range = z_max - z_min;
    float z_threshold_min = z_min + z_range * z_min_ratio;
    float z_threshold_max = z_min + z_range * z_max_ratio;

    std::cout << "[ProbeSimulator] Generating PARTIAL probe data..." << std::endl;
    std::cout << "  Target mesh Z range: [" << z_min << ", " << z_max << "] mm" << std::endl;
    std::cout << "  Keeping Z in: [" << z_threshold_min << ", " << z_threshold_max << "] mm" << std::endl;
    std::cout << "  (ratio: [" << z_min_ratio * 100 << "%, " << z_max_ratio * 100 << "%])" << std::endl;

    // Count points in range
    std::vector<uint32_t> valid_indices;
    valid_indices.reserve(target_mesh.num_vertices / 4);  // Estimate

    for (uint32_t i = 0; i < target_mesh.num_vertices; ++i) {
        float z = target_mesh.vertices_z[i];
        if (z >= z_threshold_min && z <= z_threshold_max) {
            valid_indices.push_back(i);
        }
    }

    uint32_t num_points = (uint32_t)valid_indices.size();
    if (num_points == 0) {
        std::cerr << "[ProbeSimulator] Error: No points in specified Z range!" << std::endl;
        return false;
    }

    float overlap_ratio = (float)num_points / target_mesh.num_vertices * 100.0f;
    std::cout << "  Selected points: " << num_points << "/" << target_mesh.num_vertices
              << " (" << overlap_ratio << "% overlap)" << std::endl;

    // Build transformation matrix: T = Translation * Rotation
    float angle_rad = rotation_z_deg * 3.14159265358979f / 180.0f;
    Matrix4x4 rotation = Matrix4x4::rotationZ(angle_rad);
    Matrix4x4 translation = Matrix4x4::translation(translation_x, translation_y, translation_z);
    Matrix4x4 transform = translation * rotation;

    source_.ground_truth_transform = transform;
    source_.noise_stddev = noise_stddev;
    source_.num_points = num_points;

    // Find and transform anchor point (medial malleolus tip = min Z point)
    original_anchor_ = findMinZPoint(target_mesh);
    transformed_anchor_ = transform.transformPoint(original_anchor_);
    anchor_set_ = true;

    std::cout << "  Anchor point (Target): (" << original_anchor_.x << ", "
              << original_anchor_.y << ", " << original_anchor_.z << ")" << std::endl;
    std::cout << "  Anchor point (Source): (" << transformed_anchor_.x << ", "
              << transformed_anchor_.y << ", " << transformed_anchor_.z << ")" << std::endl;

    std::cout << "  Rotation Z: " << rotation_z_deg << " degrees" << std::endl;
    std::cout << "  Translation: (" << translation_x << ", " << translation_y << ", " << translation_z << ") mm" << std::endl;
    std::cout << "  Noise StdDev: " << noise_stddev << " mm" << std::endl;

    // Allocate host memory for transformed points
    std::vector<float> h_points_x(num_points);
    std::vector<float> h_points_y(num_points);
    std::vector<float> h_points_z(num_points);
    std::vector<float> h_normals_x, h_normals_y, h_normals_z;

    if (include_normals && target_mesh.normals_x) {
        h_normals_x.resize(num_points);
        h_normals_y.resize(num_points);
        h_normals_z.resize(num_points);
    }

    // Create Gaussian noise generator
    std::normal_distribution<float> noise_dist(0.0f, noise_stddev);

    // Transform each selected point and add noise
    for (uint32_t i = 0; i < num_points; ++i) {
        uint32_t orig_idx = valid_indices[i];

        // Get original point
        float3_t p(target_mesh.vertices_x[orig_idx],
                   target_mesh.vertices_y[orig_idx],
                   target_mesh.vertices_z[orig_idx]);

        // Apply transformation
        float3_t p_transformed = transform.transformPoint(p);

        // Add Gaussian noise
        h_points_x[i] = p_transformed.x + noise_dist(rng_);
        h_points_y[i] = p_transformed.y + noise_dist(rng_);
        h_points_z[i] = p_transformed.z + noise_dist(rng_);

        // Transform normals (no translation, no noise)
        if (include_normals && target_mesh.normals_x) {
            float3_t n(target_mesh.normals_x[orig_idx],
                       target_mesh.normals_y[orig_idx],
                       target_mesh.normals_z[orig_idx]);
            float3_t n_transformed = transform.transformVector(n).normalized();
            h_normals_x[i] = n_transformed.x;
            h_normals_y[i] = n_transformed.y;
            h_normals_z[i] = n_transformed.z;
        }
    }

    // Allocate GPU memory
    size_t bytes = num_points * sizeof(float);
    cudaError_t err;

    err = cudaMalloc(&source_.points_x, bytes);
    if (err != cudaSuccess) {
        std::cerr << "[ProbeSimulator] cudaMalloc failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMalloc(&source_.points_y, bytes);
    if (err != cudaSuccess) {
        cudaFree(source_.points_x);
        std::cerr << "[ProbeSimulator] cudaMalloc failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMalloc(&source_.points_z, bytes);
    if (err != cudaSuccess) {
        cudaFree(source_.points_x);
        cudaFree(source_.points_y);
        std::cerr << "[ProbeSimulator] cudaMalloc failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    // Upload points to GPU
    cudaMemcpy(source_.points_x, h_points_x.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(source_.points_y, h_points_y.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(source_.points_z, h_points_z.data(), bytes, cudaMemcpyHostToDevice);

    // Handle normals
    if (include_normals && !h_normals_x.empty()) {
        cudaMalloc(&source_.normals_x, bytes);
        cudaMalloc(&source_.normals_y, bytes);
        cudaMalloc(&source_.normals_z, bytes);
        cudaMemcpy(source_.normals_x, h_normals_x.data(), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(source_.normals_y, h_normals_y.data(), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(source_.normals_z, h_normals_z.data(), bytes, cudaMemcpyHostToDevice);
    } else {
        source_.normals_x = source_.normals_y = source_.normals_z = nullptr;
    }

    source_.on_device = true;
    source_initialized_ = true;

    std::cout << "[ProbeSimulator] Partial probe uploaded to GPU ("
              << (bytes * 3) / 1024 << " KB for positions)" << std::endl;

    return true;
}

// ============================================================================
// Clinical Accessible Probe Generation (simulates real surgical access)
// ============================================================================
inline bool ProbeSimulator::generateClinicalProbe(
    const MeshSoA& target_mesh,
    float rotation_z_deg,
    float translation_x,
    float translation_y,
    float translation_z,
    float noise_stddev,
    float z_height_mm,
    float nz_threshold,
    bool require_anterior_or_medial,
    bool include_normals
) {
    // Free previous data
    freeSourceMemory();

    if (target_mesh.num_vertices == 0) {
        std::cerr << "[ProbeSimulator] Error: Target mesh has no vertices" << std::endl;
        return false;
    }

    if (!target_mesh.normals_x) {
        std::cerr << "[ProbeSimulator] Error: Normals required for clinical probe generation" << std::endl;
        return false;
    }

    // First, compute bounding box of target mesh
    float z_min = target_mesh.vertices_z[0];
    float z_max = target_mesh.vertices_z[0];
    for (uint32_t i = 1; i < target_mesh.num_vertices; ++i) {
        float z = target_mesh.vertices_z[i];
        if (z < z_min) z_min = z;
        if (z > z_max) z_max = z;
    }

    float z_threshold = z_min + z_height_mm;

    std::cout << "[ProbeSimulator] Generating CLINICAL ACCESSIBLE probe data..." << std::endl;
    std::cout << "  Target mesh Z range: [" << z_min << ", " << z_max << "] mm" << std::endl;
    std::cout << "  Distal region: Z < " << z_threshold << " mm (bottom " << z_height_mm << " mm)" << std::endl;
    std::cout << "  Normal filter: Nz >= " << nz_threshold << " (reject bottom-facing)" << std::endl;
    std::cout << "  Anterior/Medial only: " << (require_anterior_or_medial ? "YES" : "NO") << std::endl;

    // Filter points based on clinical accessibility
    std::vector<uint32_t> valid_indices;
    valid_indices.reserve(target_mesh.num_vertices / 10);

    uint32_t rejected_z = 0, rejected_nz = 0, rejected_dir = 0;

    for (uint32_t i = 0; i < target_mesh.num_vertices; ++i) {
        float z = target_mesh.vertices_z[i];
        float nx = target_mesh.normals_x[i];
        float ny = target_mesh.normals_y[i];
        float nz = target_mesh.normals_z[i];

        // Filter 1: Z range (distal end only)
        if (z > z_threshold) {
            rejected_z++;
            continue;
        }

        // Filter 2: Reject bottom-facing surfaces (joint interior ceiling)
        if (nz < nz_threshold) {
            rejected_nz++;
            continue;
        }

        // Filter 3: Keep only anterior (Ny > 0) or medial (Nx > 0) surfaces
        if (require_anterior_or_medial) {
            if (ny <= 0.0f && nx <= 0.0f) {
                rejected_dir++;
                continue;
            }
        }

        valid_indices.push_back(i);
    }

    uint32_t num_points = (uint32_t)valid_indices.size();
    if (num_points == 0) {
        std::cerr << "[ProbeSimulator] Error: No points pass clinical accessibility filter!" << std::endl;
        std::cerr << "  Rejected by Z: " << rejected_z << std::endl;
        std::cerr << "  Rejected by Nz: " << rejected_nz << std::endl;
        std::cerr << "  Rejected by direction: " << rejected_dir << std::endl;
        return false;
    }

    float overlap_ratio = (float)num_points / target_mesh.num_vertices * 100.0f;
    std::cout << "  Selected points: " << num_points << "/" << target_mesh.num_vertices
              << " (" << overlap_ratio << "% of total)" << std::endl;
    std::cout << "  Rejected: Z=" << rejected_z << ", Nz=" << rejected_nz << ", Dir=" << rejected_dir << std::endl;

    // Build transformation matrix
    float angle_rad = rotation_z_deg * 3.14159265358979f / 180.0f;
    Matrix4x4 rotation = Matrix4x4::rotationZ(angle_rad);
    Matrix4x4 translation = Matrix4x4::translation(translation_x, translation_y, translation_z);
    Matrix4x4 transform = translation * rotation;

    source_.ground_truth_transform = transform;
    source_.noise_stddev = noise_stddev;
    source_.num_points = num_points;

    // Find and transform anchor point (medial malleolus tip = min Z point)
    original_anchor_ = findMinZPoint(target_mesh);
    transformed_anchor_ = transform.transformPoint(original_anchor_);
    anchor_set_ = true;

    std::cout << "  Anchor point (Target): (" << original_anchor_.x << ", "
              << original_anchor_.y << ", " << original_anchor_.z << ")" << std::endl;
    std::cout << "  Anchor point (Source): (" << transformed_anchor_.x << ", "
              << transformed_anchor_.y << ", " << transformed_anchor_.z << ")" << std::endl;

    std::cout << "  Rotation Z: " << rotation_z_deg << " degrees" << std::endl;
    std::cout << "  Translation: (" << translation_x << ", " << translation_y << ", " << translation_z << ") mm" << std::endl;
    std::cout << "  Noise StdDev: " << noise_stddev << " mm" << std::endl;

    // Allocate host memory
    std::vector<float> h_points_x(num_points);
    std::vector<float> h_points_y(num_points);
    std::vector<float> h_points_z(num_points);
    std::vector<float> h_normals_x, h_normals_y, h_normals_z;

    if (include_normals) {
        h_normals_x.resize(num_points);
        h_normals_y.resize(num_points);
        h_normals_z.resize(num_points);
    }

    // Create Gaussian noise generator
    std::normal_distribution<float> noise_dist(0.0f, noise_stddev);

    // Transform each selected point
    for (uint32_t i = 0; i < num_points; ++i) {
        uint32_t orig_idx = valid_indices[i];

        float3_t p(target_mesh.vertices_x[orig_idx],
                   target_mesh.vertices_y[orig_idx],
                   target_mesh.vertices_z[orig_idx]);

        float3_t p_transformed = transform.transformPoint(p);

        h_points_x[i] = p_transformed.x + noise_dist(rng_);
        h_points_y[i] = p_transformed.y + noise_dist(rng_);
        h_points_z[i] = p_transformed.z + noise_dist(rng_);

        if (include_normals) {
            float3_t n(target_mesh.normals_x[orig_idx],
                       target_mesh.normals_y[orig_idx],
                       target_mesh.normals_z[orig_idx]);
            float3_t n_transformed = transform.transformVector(n).normalized();
            h_normals_x[i] = n_transformed.x;
            h_normals_y[i] = n_transformed.y;
            h_normals_z[i] = n_transformed.z;
        }
    }

    // Allocate GPU memory
    size_t bytes = num_points * sizeof(float);
    cudaError_t err;

    err = cudaMalloc(&source_.points_x, bytes);
    if (err != cudaSuccess) {
        std::cerr << "[ProbeSimulator] cudaMalloc failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMalloc(&source_.points_y, bytes);
    if (err != cudaSuccess) {
        cudaFree(source_.points_x);
        return false;
    }

    err = cudaMalloc(&source_.points_z, bytes);
    if (err != cudaSuccess) {
        cudaFree(source_.points_x);
        cudaFree(source_.points_y);
        return false;
    }

    // Upload
    cudaMemcpy(source_.points_x, h_points_x.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(source_.points_y, h_points_y.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(source_.points_z, h_points_z.data(), bytes, cudaMemcpyHostToDevice);

    if (include_normals) {
        cudaMalloc(&source_.normals_x, bytes);
        cudaMalloc(&source_.normals_y, bytes);
        cudaMalloc(&source_.normals_z, bytes);
        cudaMemcpy(source_.normals_x, h_normals_x.data(), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(source_.normals_y, h_normals_y.data(), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(source_.normals_z, h_normals_z.data(), bytes, cudaMemcpyHostToDevice);
    } else {
        source_.normals_x = source_.normals_y = source_.normals_z = nullptr;
    }

    source_.on_device = true;
    source_initialized_ = true;

    std::cout << "[ProbeSimulator] Clinical probe uploaded to GPU ("
              << (bytes * 3) / 1024 << " KB for positions)" << std::endl;

    return true;
}

// ============================================================================
// 3D Rotation Probe Generation (X + Y + Z axes rotation)
// ============================================================================
inline bool ProbeSimulator::generateSimulatedProbe3D(
    const MeshSoA& target_mesh,
    float rotation_x_deg,
    float rotation_y_deg,
    float rotation_z_deg,
    float translation_x,
    float translation_y,
    float translation_z,
    float noise_stddev,
    bool include_normals
) {
    // Free previous data
    freeSourceMemory();

    uint32_t num_points = target_mesh.num_vertices;
    if (num_points == 0) {
        std::cerr << "[ProbeSimulator] Error: Target mesh has no vertices" << std::endl;
        return false;
    }

    std::cout << "[ProbeSimulator] Generating 3D rotation probe data..." << std::endl;
    std::cout << "  Source points: " << num_points << std::endl;

    // Build transformation matrix: T = Translation * Rz * Ry * Rx
    float ax = rotation_x_deg * 3.14159265358979f / 180.0f;
    float ay = rotation_y_deg * 3.14159265358979f / 180.0f;
    float az = rotation_z_deg * 3.14159265358979f / 180.0f;

    Matrix4x4 rot_x = Matrix4x4::rotationX(ax);
    Matrix4x4 rot_y = Matrix4x4::rotationY(ay);
    Matrix4x4 rot_z = Matrix4x4::rotationZ(az);
    Matrix4x4 translation = Matrix4x4::translation(translation_x, translation_y, translation_z);

    // Combined rotation: Rz * Ry * Rx (apply X first, then Y, then Z)
    Matrix4x4 rotation = rot_z * rot_y * rot_x;
    Matrix4x4 transform = translation * rotation;

    source_.ground_truth_transform = transform;
    source_.noise_stddev = noise_stddev;
    source_.num_points = num_points;

    // Find and transform anchor point (medial malleolus tip = min Z point)
    original_anchor_ = findMinZPoint(target_mesh);
    transformed_anchor_ = transform.transformPoint(original_anchor_);
    anchor_set_ = true;

    std::cout << "  Anchor point (Target): (" << original_anchor_.x << ", "
              << original_anchor_.y << ", " << original_anchor_.z << ")" << std::endl;
    std::cout << "  Anchor point (Source): (" << transformed_anchor_.x << ", "
              << transformed_anchor_.y << ", " << transformed_anchor_.z << ")" << std::endl;

    std::cout << "  Rotation: X=" << rotation_x_deg << ", Y=" << rotation_y_deg
              << ", Z=" << rotation_z_deg << " degrees" << std::endl;
    std::cout << "  Translation: (" << translation_x << ", " << translation_y << ", " << translation_z << ") mm" << std::endl;
    std::cout << "  Noise StdDev: " << noise_stddev << " mm" << std::endl;

    // Allocate host memory for transformed points
    std::vector<float> h_points_x(num_points);
    std::vector<float> h_points_y(num_points);
    std::vector<float> h_points_z(num_points);
    std::vector<float> h_normals_x, h_normals_y, h_normals_z;

    if (include_normals && target_mesh.normals_x) {
        h_normals_x.resize(num_points);
        h_normals_y.resize(num_points);
        h_normals_z.resize(num_points);
    }

    // Create Gaussian noise generator
    std::normal_distribution<float> noise_dist(0.0f, noise_stddev);

    // Transform each point and add noise
    for (uint32_t i = 0; i < num_points; ++i) {
        float3_t p(target_mesh.vertices_x[i],
                   target_mesh.vertices_y[i],
                   target_mesh.vertices_z[i]);

        float3_t p_transformed = transform.transformPoint(p);

        h_points_x[i] = p_transformed.x + noise_dist(rng_);
        h_points_y[i] = p_transformed.y + noise_dist(rng_);
        h_points_z[i] = p_transformed.z + noise_dist(rng_);

        if (include_normals && target_mesh.normals_x) {
            float3_t n(target_mesh.normals_x[i],
                       target_mesh.normals_y[i],
                       target_mesh.normals_z[i]);
            float3_t n_transformed = transform.transformVector(n).normalized();
            h_normals_x[i] = n_transformed.x;
            h_normals_y[i] = n_transformed.y;
            h_normals_z[i] = n_transformed.z;
        }
    }

    // Allocate GPU memory
    size_t bytes = num_points * sizeof(float);
    cudaError_t err;

    err = cudaMalloc(&source_.points_x, bytes);
    if (err != cudaSuccess) {
        std::cerr << "[ProbeSimulator] cudaMalloc failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMalloc(&source_.points_y, bytes);
    if (err != cudaSuccess) {
        cudaFree(source_.points_x);
        return false;
    }

    err = cudaMalloc(&source_.points_z, bytes);
    if (err != cudaSuccess) {
        cudaFree(source_.points_x);
        cudaFree(source_.points_y);
        return false;
    }

    cudaMemcpy(source_.points_x, h_points_x.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(source_.points_y, h_points_y.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(source_.points_z, h_points_z.data(), bytes, cudaMemcpyHostToDevice);

    if (include_normals && !h_normals_x.empty()) {
        cudaMalloc(&source_.normals_x, bytes);
        cudaMalloc(&source_.normals_y, bytes);
        cudaMalloc(&source_.normals_z, bytes);
        cudaMemcpy(source_.normals_x, h_normals_x.data(), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(source_.normals_y, h_normals_y.data(), bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(source_.normals_z, h_normals_z.data(), bytes, cudaMemcpyHostToDevice);
    } else {
        source_.normals_x = source_.normals_y = source_.normals_z = nullptr;
    }

    source_.on_device = true;
    source_initialized_ = true;

    std::cout << "[ProbeSimulator] 3D rotation probe uploaded to GPU ("
              << (bytes * 3) / 1024 << " KB for positions)" << std::endl;

    return true;
}
