#pragma once

#include "types.h"
#include <cuda_runtime.h>

// ============================================================================
// Mesh GPU Manager Class
// Handles Host <-> Device memory transfer and GPU compute calls
// ============================================================================

class MeshGPU {
public:
    MeshGPU();
    ~MeshGPU();

    // Disable copy
    MeshGPU(const MeshGPU&) = delete;
    MeshGPU& operator=(const MeshGPU&) = delete;

    // Upload from Host MeshSoA to GPU
    bool uploadToDevice(const MeshSoA& host_mesh);

    // Download from GPU to Host
    bool downloadToHost(MeshSoA& host_mesh);

    // GPU compute: face normals and areas
    bool computeFaceNormals();

    // GPU compute: vertex normals (weighted average from face normals)
    bool computeVertexNormals();

    // GPU compute: build vertex-face adjacency list
    bool buildVertexFaceAdjacency();

    // GPU compute: vertex curvature (mean and Gaussian)
    bool computeCurvature();

    // GPU compute: generate feature texture
    bool generateFeatureTexture(FeatureTexture& feature_tex);

    // One-shot initialization: execute all computations
    bool initialize(const MeshSoA& host_mesh);

    // Build 3D grid index for O(1) point query
    bool buildGridIndex(float cell_size = DEFAULT_CELL_SIZE);

    // Build multi-resolution grid index (pyramid of grids)
    bool buildMultiResGridIndex(const float* cell_sizes, int num_levels);

    // Get multi-resolution grid
    const MultiResGridIndex& getMultiResGrid() const { return multi_grid_; }
    bool hasMultiResGrid() const { return multi_grid_initialized_; }
    const GridIndex& getGridLevel(int level) const { return multi_grid_.getLevel(level); }

    // Query single point - returns nearest vertex info
    PointQueryResult queryPoint(float x, float y, float z, int search_radius = 1);

    // Query multiple points (batch)
    bool queryPoints(const float* query_x, const float* query_y, const float* query_z,
                     uint32_t num_queries, uint32_t* result_vertex_ids, float* result_distances,
                     int search_radius = 1);

    // Get device mesh data pointer (returns host-side struct with device pointers)
    MeshSoA* getDeviceMesh() { return &h_mesh_ptrs_; }

    // Get bounding box
    BoundingBox getBoundingBox() const { return bbox_; }

    // Get grid index
    const GridIndex& getGridIndex() const { return grid_; }

    // Convenient grid accessors for rotation search
    float3 getGridMin() const {
        return make_float3(grid_.origin.x, grid_.origin.y, grid_.origin.z);
    }
    float getGridCellSize() const { return grid_.cell_size; }
    int3 getGridDims() const {
        return make_int3(grid_.dim_x, grid_.dim_y, grid_.dim_z);
    }
    const uint32_t* getGridCellCounts() const { return grid_.cell_vertex_count; }
    const uint32_t* getGridCellStarts() const { return grid_.cell_starts; }
    const uint32_t* getGridVertexIndices() const { return grid_.vertex_indices; }

    // Device vertex coordinate accessors (for distance kernel)
    const float* getVerticesX() const { return h_mesh_ptrs_.vertices_x; }
    const float* getVerticesY() const { return h_mesh_ptrs_.vertices_y; }
    const float* getVerticesZ() const { return h_mesh_ptrs_.vertices_z; }

    // Get statistics
    uint32_t getNumVertices() const { return num_vertices_; }
    uint32_t getNumFaces() const { return num_faces_; }
    bool hasGridIndex() const { return grid_initialized_; }

private:
    // Allocate GPU memory
    bool allocateDeviceMemory(uint32_t num_vertices, uint32_t num_faces);

    // Free GPU memory
    void freeDeviceMemory();

    // Compute bounding box
    void computeBoundingBox(const MeshSoA& host_mesh);

private:
    MeshSoA* d_mesh_;           // Device mesh pointer
    MeshSoA h_mesh_ptrs_;       // Host-side storage of device memory pointers

    uint32_t num_vertices_;
    uint32_t num_faces_;
    bool initialized_;

    BoundingBox bbox_;

    // Adjacency list size
    uint32_t adjacency_size_;

    // Grid index for O(1) point query
    GridIndex grid_;
    bool grid_initialized_;

    // Multi-resolution grid index
    MultiResGridIndex multi_grid_;
    bool multi_grid_initialized_;

    // Free grid memory
    void freeGridMemory();
    void freeMultiResGridMemory();
};

// ============================================================================
// CUDA Kernel Launch Declarations (implemented in mesh_kernels.cu)
// ============================================================================

// Compute face normals and areas
extern "C" void launchComputeFaceNormals(
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    float* face_normals_x, float* face_normals_y, float* face_normals_z,
    float* face_areas,
    uint32_t num_faces
);

// Compute vertex normals (weighted average from face normals)
extern "C" void launchComputeVertexNormals(
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    const float* face_normals_x, const float* face_normals_y, const float* face_normals_z,
    const float* face_areas,
    float* vertex_normals_x, float* vertex_normals_y, float* vertex_normals_z,
    uint32_t num_vertices, uint32_t num_faces
);

// Build vertex-face adjacency list (step 1: count)
extern "C" void launchCountVertexFaces(
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    uint32_t* vertex_face_count,
    uint32_t num_vertices, uint32_t num_faces
);

// Build vertex-face adjacency list (step 2: fill)
extern "C" void launchBuildVertexFaceList(
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    const uint32_t* vertex_face_offset,
    uint32_t* vertex_face_indices,
    uint32_t* vertex_face_current,
    uint32_t num_faces
);

// Compute vertex curvature
extern "C" void launchComputeCurvature(
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    const float* vertex_normals_x, const float* vertex_normals_y, const float* vertex_normals_z,
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    const uint32_t* vertex_face_offset,
    const uint32_t* vertex_face_indices,
    float* curvature,
    float* gaussian_curvature,
    uint32_t num_vertices
);

// Generate feature texture
extern "C" void launchGenerateFeatureTexture(
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    const float* normals_x, const float* normals_y, const float* normals_z,
    const float* curvature, const float* gaussian_curvature,
    float* features,
    uint32_t num_vertices,
    float bbox_min_x, float bbox_min_y, float bbox_min_z,
    float bbox_size_x, float bbox_size_y, float bbox_size_z
);

// Build 3D grid index
extern "C" void launchBuildGridIndex(
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    uint32_t* cell_vertex_id,
    uint32_t* cell_vertex_count,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    uint32_t num_vertices
);

// Query nearest vertex (batch)
extern "C" void launchQueryNearestVertex(
    const float* query_x, const float* query_y, const float* query_z,
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    const float* normals_x, const float* normals_y, const float* normals_z,
    const float* curvature, const float* gaussian_curvature,
    const uint32_t* cell_vertex_id,
    uint32_t* result_vertex_id,
    float* result_distance,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    uint32_t num_queries,
    int search_radius
);

// Query single point (for probe input)
extern "C" void launchSinglePointQuery(
    float query_x, float query_y, float query_z,
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    const float* normals_x, const float* normals_y, const float* normals_z,
    const float* curvature, const float* gaussian_curvature,
    const uint32_t* cell_vertex_id,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    int search_radius,
    uint32_t* out_vertex_id,
    float* out_position,
    float* out_normal,
    float* out_curvatures,
    float* out_distance
);

extern "C" void launchSelectVerticesByConstraints(
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    unsigned char* vertex_mask,
    float target_center_x, float target_center_y, float target_center_z,
    float target_radius_squared,
    const float* constraint_x, const float* constraint_y, const float* constraint_z,
    uint32_t constraint_count,
    float membership_radius_squared,
    uint32_t num_vertices
);

extern "C" void launchSelectFacesByVertexMask(
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    const unsigned char* vertex_mask,
    unsigned char* face_mask,
    uint32_t num_faces
);

extern "C" void launchMaskToCounts(
    const unsigned char* mask,
    uint32_t* counts,
    uint32_t count
);

extern "C" void launchScatterSelectedVertexIndices(
    const unsigned char* vertex_mask,
    const uint32_t* vertex_offsets,
    int* vertex_mapping,
    int* selected_vertex_indices,
    uint32_t num_vertices
);

extern "C" void launchScatterSelectedFaces(
    const unsigned char* face_mask,
    const uint32_t* face_offsets,
    const uint32_t* faces_v0,
    const uint32_t* faces_v1,
    const uint32_t* faces_v2,
    const int* vertex_mapping,
    int* selected_face_v0,
    int* selected_face_v1,
    int* selected_face_v2,
    uint32_t num_faces
);
