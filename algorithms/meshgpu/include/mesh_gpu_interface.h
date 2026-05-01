#pragma once

// ============================================================================
// MeshGPU C++ Interface (CUDA-free header)
// This header can be included in pure C++ code without CUDA compiler
// ============================================================================

#include <vector>
#include <memory>
#include <string>
#include <array>

// DLL export/import macros
#ifdef _WIN32
    #if defined(MESHGPU_STATIC)
        #define MESHGPU_API
    #elif defined(MESHGPU_EXPORTS)
        #define MESHGPU_API __declspec(dllexport)
    #else
        #define MESHGPU_API __declspec(dllimport)
    #endif
#else
    #define MESHGPU_API
#endif

namespace mesh_gpu {

// ============================================================================
// Data Types (Plain C++ structs, no CUDA)
// ============================================================================

struct Point3D {
    float x, y, z;
    Point3D() : x(0), y(0), z(0) {}
    Point3D(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Normal3D {
    float nx, ny, nz;
    Normal3D() : nx(0), ny(0), nz(1) {}
    Normal3D(float nx_, float ny_, float nz_) : nx(nx_), ny(ny_), nz(nz_) {}
};

// 4x4 Transform matrix (row-major)
struct Transform4x4 {
    float data[16];

    Transform4x4() {
        // Initialize to identity
        for (int i = 0; i < 16; i++) {
            data[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        }
    }

    float& operator()(int row, int col) { return data[row * 4 + col]; }
    float operator()(int row, int col) const { return data[row * 4 + col]; }
};

// Registration result
struct RegistrationResult {
    Transform4x4 transform;      // Final transformation
    float rmse;                  // Root mean square error
    int iterations;              // Number of iterations
    bool converged;              // Whether converged
    std::vector<float> rmse_history;  // RMSE at each iteration

    RegistrationResult() : rmse(0), iterations(0), converged(false) {}
};

// Curvature weighting mode for GICP
enum class CurvatureWeightMode {
    NONE = 0,           // No curvature weighting (standard GICP)
    HIGH_CURVATURE,     // Prefer high-curvature regions (edges, corners)
    LOW_CURVATURE,      // Prefer low-curvature regions (flat surfaces)
    GAUSSIAN_AWARE,     // Use Gaussian curvature for surface type detection
    ADAPTIVE            // Hybrid: balance between high and low curvature
};

// Compute backend selector (for heterogeneous accelerator compatibility)
enum class ComputeBackendKind {
    AUTO = 0,   // Auto-select best available backend
    CUDA,       // Force CUDA backend
    ASCEND,     // Prefer Huawei Ascend backend
    CPU         // CPU fallback
};

// Backend selection configuration
struct BackendConfig {
    ComputeBackendKind requested;
    int device_id;   // Device index for backend that supports explicit device IDs
    bool strict;     // If true, do not fallback to other backends

    BackendConfig()
        : requested(ComputeBackendKind::AUTO)
        , device_id(0)
        , strict(false)
    {}
};

// Runtime backend status for observability/debugging
struct BackendStatus {
    ComputeBackendKind requested;
    ComputeBackendKind selected;
    bool cuda_available;
    bool ascend_runtime_available;
    bool ascend_plugin_available;
    bool cpu_available;
    bool initialized;
    std::string message;

    BackendStatus()
        : requested(ComputeBackendKind::AUTO)
        , selected(ComputeBackendKind::CPU)
        , cuda_available(false)
        , ascend_runtime_available(false)
        , ascend_plugin_available(false)
        , cpu_available(true)
        , initialized(false)
    {}
};

// Registration parameters
struct RegistrationParams {
    int max_iterations;           // Maximum iterations (default: 50)
    float convergence_threshold;  // Convergence threshold (default: 1e-6)
    float distance_threshold;     // Distance threshold for correspondences (default: 10mm)
    int search_radius;            // Grid search radius (default: 2)
    bool use_point_to_plane;      // Point-to-plane vs point-to-point (default: true)
    bool verbose;                 // Print progress (default: false)

    // Curvature weighting parameters
    CurvatureWeightMode curvature_weight_mode;  // Weighting strategy (default: NONE)
    float curvature_weight_scale;  // Scale factor for curvature influence (default: 1.0)
    float min_weight;              // Minimum weight to prevent singularities (default: 0.1)
    float max_weight;              // Maximum weight cap (default: 10.0)

    RegistrationParams()
        : max_iterations(50)
        , convergence_threshold(1e-6f)
        , distance_threshold(10.0f)
        , search_radius(2)
        , use_point_to_plane(true)
        , verbose(false)
        , curvature_weight_mode(CurvatureWeightMode::NONE)
        , curvature_weight_scale(1.0f)
        , min_weight(0.1f)
        , max_weight(10.0f)
    {}
};

// Rotation search parameters
struct RotationSearchParams {
    // Z-axis rotation (main search)
    float z_angle_min = 0.0f;       // degrees
    float z_angle_max = 360.0f;     // degrees
    float z_angle_step = 2.0f;      // degrees

    // X/Y axis fine-tuning (for 3D search)
    float xy_angle_range = 10.0f;   // +/- degrees
    float xy_angle_step = 5.0f;     // degrees

    bool enable_xy_search = false;  // Enable X/Y fine-tuning

    RotationSearchParams() = default;
};

// Rotation search result
struct RotationSearchResult {
    Transform4x4 best_rotation;   // Best rotation matrix
    float best_angle_z;           // Best Z rotation angle (degrees)
    float best_angle_x;           // Best X rotation angle (degrees)
    float best_angle_y;           // Best Y rotation angle (degrees)
    int best_score;               // Negative truncated distance energy (×1000)
    int total_points;             // Total source points
    float mean_dist_mm;           // Mean distance to mesh (mm)
    bool success;                 // Whether search succeeded

    RotationSearchResult()
        : best_angle_z(0), best_angle_x(0), best_angle_y(0)
        , best_score(0), total_points(0), mean_dist_mm(0), success(false) {}
};

struct TransformCandidateScore {
    int candidate_index;
    int score;               // Negative truncated distance energy (*1000)
    float mean_dist_mm;      // Derived from energy, lower is better
    bool success;

    TransformCandidateScore()
        : candidate_index(-1), score(0), mean_dist_mm(0), success(false) {}
};

// Mesh statistics
struct MeshStats {
    int num_vertices;
    int num_triangles;
    Point3D bounding_box_min;
    Point3D bounding_box_max;
    float cell_size;
};

// ============================================================================
// MeshGPU Interface Class (PIMPL pattern)
// ============================================================================

class MESHGPU_API MeshGPUInterface {
public:
    MeshGPUInterface();
    ~MeshGPUInterface();

    // Disable copy
    MeshGPUInterface(const MeshGPUInterface&) = delete;
    MeshGPUInterface& operator=(const MeshGPUInterface&) = delete;

    // Move allowed
    MeshGPUInterface(MeshGPUInterface&&) noexcept;
    MeshGPUInterface& operator=(MeshGPUInterface&&) noexcept;

    // ========================================
    // Initialization
    // ========================================

    // Check if CUDA is available
    static bool isCudaAvailable();

    // Get CUDA device info
    static std::string getCudaDeviceInfo();

    // Convert backend enum to readable string
    static const char* backendKindToString(ComputeBackendKind kind);

    // Probe backend availability with fallback policy (does not run registration)
    static BackendStatus probeBackend(const BackendConfig& config = BackendConfig());

    // Configure runtime backend policy for this instance
    void setBackendConfig(const BackendConfig& config);

    // Current backend config/status
    BackendConfig getBackendConfig() const;
    BackendStatus getBackendStatus() const;
    std::string getBackendInfo() const;

    // ========================================
    // Target Mesh Operations
    // ========================================

    // Load target mesh from PLY file
    bool loadTargetMesh(const std::string& ply_file, float cell_size = 1.0f);

    // Set target mesh from point cloud data
    bool setTargetMesh(const std::vector<Point3D>& vertices,
                       const std::vector<Normal3D>& normals,
                       const std::vector<std::array<int, 3>>& triangles,
                       float cell_size = 1.0f);

    // Check if target mesh is loaded
    bool hasTargetMesh() const;

    // Get mesh statistics
    MeshStats getMeshStats() const;

    // ========================================
    // Source Point Cloud Operations
    // ========================================

    // Set source point cloud (collected probe points)
    bool setSourcePointCloud(const std::vector<Point3D>& points);

    // Add points to source point cloud
    bool addSourcePoints(const std::vector<Point3D>& points);

    // Clear source point cloud
    void clearSourcePoints();

    // Get source point count
    int getSourcePointCount() const;

    // ========================================
    // Anchor Point Override (for coarse registration)
    // ========================================

    // Set explicit source anchor point (overrides automatic Zmin fallback).
    // In live mode, this should be the tracker-space position of a known
    // anatomical landmark (e.g. medial malleolus) tapped by the surgeon.
    void setSourceAnchorOverride(const Point3D& anchor);

    // Clear source anchor override (revert to automatic Zmin computation)
    void clearSourceAnchorOverride();

    // Check if an explicit source anchor is set
    bool hasSourceAnchorOverride() const;

    // ========================================
    // Registration (ICP/GICP)
    // ========================================

    // Run GICP registration
    // Returns: transformation that aligns source points to target mesh
    RegistrationResult runRegistration(const RegistrationParams& params = RegistrationParams());

    // Run registration with initial transform
    RegistrationResult runRegistration(const Transform4x4& initial_transform,
                                       const RegistrationParams& params = RegistrationParams());

    // Get last registration result
    RegistrationResult getLastResult() const;

    // ========================================
    // Rotation Search (GPU-accelerated)
    // ========================================

    // Basic Z-axis rotation search
    // Searches through all Z rotations to find best initial alignment
    RotationSearchResult runRotationSearch(const RotationSearchParams& params = RotationSearchParams());

    // Hierarchical Z-axis rotation search (faster, two-stage coarse-to-fine)
    // Stage 1: Coarse search with large step
    // Stage 2: Fine search around best candidate
    RotationSearchResult runHierarchicalRotationSearch(const RotationSearchParams& params = RotationSearchParams());

    // Full sphere rotation search (searches X, Y, Z rotations)
    // angle_step_deg: step size for all axes (default 20 degrees = 5832 candidates)
    RotationSearchResult runFullSphereSearch(float angle_step_deg = 20.0f);

    // Hierarchical full sphere search (faster version)
    // Stage 1: Coarse full sphere search
    // Stage 2: Fine refinement around best candidate
    RotationSearchResult runFullSphereHierarchicalSearch(float coarse_step_deg = 30.0f,
                                                          float fine_step_deg = 5.0f,
                                                          float fine_range_deg = 20.0f);

    // Combined workflow: rotation search + GICP registration
    // First finds best initial rotation, then runs GICP for fine alignment
    RegistrationResult runRegistrationWithRotationSearch(
        const RotationSearchParams& rot_params = RotationSearchParams(),
        const RegistrationParams& reg_params = RegistrationParams());

    // Combined workflow: full sphere search + GICP registration
    RegistrationResult runRegistrationWithFullSphereSearch(
        float angle_step_deg = 20.0f,
        const RegistrationParams& reg_params = RegistrationParams());

    // Score arbitrary rigid transform candidates against current target mesh using
    // the same GPU distance-energy field used by rotation search.
    std::vector<TransformCandidateScore> scoreTransformCandidates(
        const std::vector<Transform4x4>& candidates,
        float cutoff_mm = 12.0f);

    // ========================================
    // Utility Functions
    // ========================================

    // Transform points using a transform matrix
    static std::vector<Point3D> transformPoints(const std::vector<Point3D>& points,
                                                 const Transform4x4& transform);

    // Compute RMSE between two point sets
    static float computeRMSE(const std::vector<Point3D>& points1,
                             const std::vector<Point3D>& points2);

    // ========================================
    // Simulation Test Functions
    // ========================================

    // Generate simulated probe point cloud from target mesh
    // rotation_z_deg: rotation around Z axis
    // translation: X/Y/Z translation
    // noise_stddev: Gaussian noise standard deviation
    // Returns: number of generated points (0 on failure)
    int generateSimulatedProbe(float rotation_z_deg,
                               float translation_x, float translation_y, float translation_z,
                               float noise_stddev);

    // Generate 3D rotation probe (rotation around all axes)
    int generateSimulatedProbe3D(float rotation_x_deg, float rotation_y_deg, float rotation_z_deg,
                                  float translation_x, float translation_y, float translation_z,
                                  float noise_stddev);

    // Generate partial probe (for partial overlap testing)
    // z_min_ratio/z_max_ratio: 0.0 = bottom of mesh, 1.0 = top of mesh
    int generatePartialProbe(float rotation_z_deg,
                             float translation_x, float translation_y, float translation_z,
                             float noise_stddev,
                             float z_min_ratio, float z_max_ratio);

    // Generate clinical accessible probe (simulates real surgical access)
    int generateClinicalProbe(float rotation_z_deg,
                              float translation_x, float translation_y, float translation_z,
                              float noise_stddev,
                              float z_height_mm, float nz_threshold);

    // Get the ground truth transformation used to generate probe
    Transform4x4 getGroundTruthTransform() const;

    // Compute registration error against ground truth
    // Returns: {translation_error_mm, rotation_error_deg}
    std::pair<float, float> computeRegistrationError(const Transform4x4& estimated_transform) const;

    // Run complete simulation test: generate probe -> run registration -> compute error
    struct SimTestResult {
        float rmse;
        int iterations;
        bool converged;
        float translation_error;  // mm
        float rotation_error;     // degrees
        float time_ms;
        int source_points;
        bool success;
    };

    SimTestResult runSimulationTest(float rotation_z_deg,
                                     float translation_x, float translation_y, float translation_z,
                                     float noise_stddev,
                                     const RegistrationParams& params = RegistrationParams());

    // Run partial overlap simulation test
    SimTestResult runPartialOverlapTest(float rotation_z_deg,
                                         float translation_x, float translation_y, float translation_z,
                                         float noise_stddev,
                                         float overlap_ratio,  // 0.0~1.0
                                         const RegistrationParams& params = RegistrationParams());

    // Get host mesh (for direct access to mesh data)
    bool getHostMesh(std::vector<Point3D>& vertices, std::vector<Normal3D>& normals) const;

    // Get generated source point cloud (after generateSimulatedProbe* calls)
    // Returns the transformed source points that were generated
    bool getSourcePointCloud(std::vector<Point3D>& points) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace mesh_gpu

extern "C" {
MESHGPU_API mesh_gpu::MeshGPUInterface* CreateMeshGPUInterface();
MESHGPU_API void DestroyMeshGPUInterface(mesh_gpu::MeshGPUInterface* instance);
}
