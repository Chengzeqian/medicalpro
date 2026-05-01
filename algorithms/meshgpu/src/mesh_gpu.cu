#include "mesh_gpu.h"
#include <cuda_runtime.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <numeric>

// ============================================================================
// MeshGPU Class Implementation
// ============================================================================

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            std::cerr << "CUDA Error: " << cudaGetErrorString(err) \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

// Forward declarations for kernels in mesh_kernels.cu
extern "C" void launchBuildCellStartsAndScatter(
    const float*, const float*, const float*,
    const uint32_t*, uint32_t*, uint32_t*,
    float, float, float, float, int, int, int, uint32_t, uint32_t);

MeshGPU::MeshGPU()
    : d_mesh_(nullptr)
    , num_vertices_(0)
    , num_faces_(0)
    , initialized_(false)
    , adjacency_size_(0)
    , grid_initialized_(false)
    , multi_grid_initialized_(false)
{
    memset(&h_mesh_ptrs_, 0, sizeof(MeshSoA));
    memset(&grid_, 0, sizeof(GridIndex));
}

MeshGPU::~MeshGPU() {
    freeMultiResGridMemory();
    freeGridMemory();
    freeDeviceMemory();
}

void MeshGPU::freeGridMemory() {
    if (grid_.cell_vertex_id) cudaFree(grid_.cell_vertex_id);
    if (grid_.cell_vertex_count) cudaFree(grid_.cell_vertex_count);
    if (grid_.cell_starts) cudaFree(grid_.cell_starts);
    if (grid_.vertex_indices) cudaFree(grid_.vertex_indices);
    memset(&grid_, 0, sizeof(GridIndex));
    grid_initialized_ = false;
}

bool MeshGPU::allocateDeviceMemory(uint32_t num_vertices, uint32_t num_faces) {
    // Vertex data
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.vertices_x, num_vertices * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.vertices_y, num_vertices * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.vertices_z, num_vertices * sizeof(float)));

    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.normals_x, num_vertices * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.normals_y, num_vertices * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.normals_z, num_vertices * sizeof(float)));

    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.curvature, num_vertices * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.gaussian_curv, num_vertices * sizeof(float)));

    // Face data
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.faces_v0, num_faces * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.faces_v1, num_faces * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.faces_v2, num_faces * sizeof(uint32_t)));

    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.face_normals_x, num_faces * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.face_normals_y, num_faces * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.face_normals_z, num_faces * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.face_areas, num_faces * sizeof(float)));

    // Adjacency list (allocated later in buildVertexFaceAdjacency)
    h_mesh_ptrs_.vertex_face_offset = nullptr;
    h_mesh_ptrs_.vertex_face_indices = nullptr;

    h_mesh_ptrs_.num_vertices = num_vertices;
    h_mesh_ptrs_.num_faces = num_faces;
    h_mesh_ptrs_.on_device = true;

    num_vertices_ = num_vertices;
    num_faces_ = num_faces;

    return true;
}

void MeshGPU::freeDeviceMemory() {
    if (h_mesh_ptrs_.vertices_x) cudaFree(h_mesh_ptrs_.vertices_x);
    if (h_mesh_ptrs_.vertices_y) cudaFree(h_mesh_ptrs_.vertices_y);
    if (h_mesh_ptrs_.vertices_z) cudaFree(h_mesh_ptrs_.vertices_z);

    if (h_mesh_ptrs_.normals_x) cudaFree(h_mesh_ptrs_.normals_x);
    if (h_mesh_ptrs_.normals_y) cudaFree(h_mesh_ptrs_.normals_y);
    if (h_mesh_ptrs_.normals_z) cudaFree(h_mesh_ptrs_.normals_z);

    if (h_mesh_ptrs_.curvature) cudaFree(h_mesh_ptrs_.curvature);
    if (h_mesh_ptrs_.gaussian_curv) cudaFree(h_mesh_ptrs_.gaussian_curv);

    if (h_mesh_ptrs_.faces_v0) cudaFree(h_mesh_ptrs_.faces_v0);
    if (h_mesh_ptrs_.faces_v1) cudaFree(h_mesh_ptrs_.faces_v1);
    if (h_mesh_ptrs_.faces_v2) cudaFree(h_mesh_ptrs_.faces_v2);

    if (h_mesh_ptrs_.face_normals_x) cudaFree(h_mesh_ptrs_.face_normals_x);
    if (h_mesh_ptrs_.face_normals_y) cudaFree(h_mesh_ptrs_.face_normals_y);
    if (h_mesh_ptrs_.face_normals_z) cudaFree(h_mesh_ptrs_.face_normals_z);
    if (h_mesh_ptrs_.face_areas) cudaFree(h_mesh_ptrs_.face_areas);

    if (h_mesh_ptrs_.vertex_face_offset) cudaFree(h_mesh_ptrs_.vertex_face_offset);
    if (h_mesh_ptrs_.vertex_face_indices) cudaFree(h_mesh_ptrs_.vertex_face_indices);

    memset(&h_mesh_ptrs_, 0, sizeof(MeshSoA));
    initialized_ = false;
}

void MeshGPU::computeBoundingBox(const MeshSoA& host_mesh) {
    bbox_.min_pt = float3_t(FLT_MAX, FLT_MAX, FLT_MAX);
    bbox_.max_pt = float3_t(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (uint32_t i = 0; i < host_mesh.num_vertices; ++i) {
        bbox_.min_pt.x = std::min(bbox_.min_pt.x, host_mesh.vertices_x[i]);
        bbox_.min_pt.y = std::min(bbox_.min_pt.y, host_mesh.vertices_y[i]);
        bbox_.min_pt.z = std::min(bbox_.min_pt.z, host_mesh.vertices_z[i]);

        bbox_.max_pt.x = std::max(bbox_.max_pt.x, host_mesh.vertices_x[i]);
        bbox_.max_pt.y = std::max(bbox_.max_pt.y, host_mesh.vertices_y[i]);
        bbox_.max_pt.z = std::max(bbox_.max_pt.z, host_mesh.vertices_z[i]);
    }
}

bool MeshGPU::uploadToDevice(const MeshSoA& host_mesh) {
    // Free old memory
    freeDeviceMemory();

    // Compute bounding box
    computeBoundingBox(host_mesh);

    // Allocate GPU memory
    if (!allocateDeviceMemory(host_mesh.num_vertices, host_mesh.num_faces)) {
        return false;
    }

    // Upload vertex data
    CUDA_CHECK(cudaMemcpy(h_mesh_ptrs_.vertices_x, host_mesh.vertices_x,
                          num_vertices_ * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(h_mesh_ptrs_.vertices_y, host_mesh.vertices_y,
                          num_vertices_ * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(h_mesh_ptrs_.vertices_z, host_mesh.vertices_z,
                          num_vertices_ * sizeof(float), cudaMemcpyHostToDevice));

    // Upload face data
    CUDA_CHECK(cudaMemcpy(h_mesh_ptrs_.faces_v0, host_mesh.faces_v0,
                          num_faces_ * sizeof(uint32_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(h_mesh_ptrs_.faces_v1, host_mesh.faces_v1,
                          num_faces_ * sizeof(uint32_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(h_mesh_ptrs_.faces_v2, host_mesh.faces_v2,
                          num_faces_ * sizeof(uint32_t), cudaMemcpyHostToDevice));

    std::cout << "[MeshGPU] Uploaded to device: " << num_vertices_ << " vertices, "
              << num_faces_ << " faces" << std::endl;

    return true;
}

bool MeshGPU::downloadToHost(MeshSoA& host_mesh) {
    if (!initialized_) {
        std::cerr << "[MeshGPU] Not initialized" << std::endl;
        return false;
    }

    // Download normals
    CUDA_CHECK(cudaMemcpy(host_mesh.normals_x, h_mesh_ptrs_.normals_x,
                          num_vertices_ * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(host_mesh.normals_y, h_mesh_ptrs_.normals_y,
                          num_vertices_ * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(host_mesh.normals_z, h_mesh_ptrs_.normals_z,
                          num_vertices_ * sizeof(float), cudaMemcpyDeviceToHost));

    // Download curvature
    CUDA_CHECK(cudaMemcpy(host_mesh.curvature, h_mesh_ptrs_.curvature,
                          num_vertices_ * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(host_mesh.gaussian_curv, h_mesh_ptrs_.gaussian_curv,
                          num_vertices_ * sizeof(float), cudaMemcpyDeviceToHost));

    // Download face normals and areas
    CUDA_CHECK(cudaMemcpy(host_mesh.face_normals_x, h_mesh_ptrs_.face_normals_x,
                          num_faces_ * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(host_mesh.face_normals_y, h_mesh_ptrs_.face_normals_y,
                          num_faces_ * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(host_mesh.face_normals_z, h_mesh_ptrs_.face_normals_z,
                          num_faces_ * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(host_mesh.face_areas, h_mesh_ptrs_.face_areas,
                          num_faces_ * sizeof(float), cudaMemcpyDeviceToHost));

    return true;
}

bool MeshGPU::computeFaceNormals() {
    std::cout << "[MeshGPU] Computing face normals..." << std::endl;

    launchComputeFaceNormals(
        h_mesh_ptrs_.vertices_x, h_mesh_ptrs_.vertices_y, h_mesh_ptrs_.vertices_z,
        h_mesh_ptrs_.faces_v0, h_mesh_ptrs_.faces_v1, h_mesh_ptrs_.faces_v2,
        h_mesh_ptrs_.face_normals_x, h_mesh_ptrs_.face_normals_y, h_mesh_ptrs_.face_normals_z,
        h_mesh_ptrs_.face_areas,
        num_faces_
    );

    return true;
}

bool MeshGPU::computeVertexNormals() {
    std::cout << "[MeshGPU] Computing vertex normals..." << std::endl;

    launchComputeVertexNormals(
        h_mesh_ptrs_.faces_v0, h_mesh_ptrs_.faces_v1, h_mesh_ptrs_.faces_v2,
        h_mesh_ptrs_.face_normals_x, h_mesh_ptrs_.face_normals_y, h_mesh_ptrs_.face_normals_z,
        h_mesh_ptrs_.face_areas,
        h_mesh_ptrs_.normals_x, h_mesh_ptrs_.normals_y, h_mesh_ptrs_.normals_z,
        num_vertices_, num_faces_
    );

    return true;
}

bool MeshGPU::buildVertexFaceAdjacency() {
    std::cout << "[MeshGPU] Building vertex-face adjacency..." << std::endl;

    // Allocate count array
    uint32_t* d_vertex_face_count;
    CUDA_CHECK(cudaMalloc(&d_vertex_face_count, (num_vertices_ + 1) * sizeof(uint32_t)));
    CUDA_CHECK(cudaMemset(d_vertex_face_count, 0, (num_vertices_ + 1) * sizeof(uint32_t)));

    // Step 1: Count faces per vertex
    launchCountVertexFaces(
        h_mesh_ptrs_.faces_v0, h_mesh_ptrs_.faces_v1, h_mesh_ptrs_.faces_v2,
        d_vertex_face_count,
        num_vertices_, num_faces_
    );

    // Download counts to Host and compute prefix sum (offsets)
    std::vector<uint32_t> h_count(num_vertices_ + 1);
    CUDA_CHECK(cudaMemcpy(h_count.data(), d_vertex_face_count,
                          num_vertices_ * sizeof(uint32_t), cudaMemcpyDeviceToHost));

    // Compute prefix sum
    std::vector<uint32_t> h_offset(num_vertices_ + 1);
    h_offset[0] = 0;
    for (uint32_t i = 0; i < num_vertices_; ++i) {
        h_offset[i + 1] = h_offset[i] + h_count[i];
    }
    adjacency_size_ = h_offset[num_vertices_];

    std::cout << "[MeshGPU] Adjacency list size: " << adjacency_size_ << std::endl;

    // Allocate adjacency list memory
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.vertex_face_offset, (num_vertices_ + 1) * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&h_mesh_ptrs_.vertex_face_indices, adjacency_size_ * sizeof(uint32_t)));

    // Upload offsets
    CUDA_CHECK(cudaMemcpy(h_mesh_ptrs_.vertex_face_offset, h_offset.data(),
                          (num_vertices_ + 1) * sizeof(uint32_t), cudaMemcpyHostToDevice));

    // Reset counter for filling
    uint32_t* d_vertex_face_current;
    CUDA_CHECK(cudaMalloc(&d_vertex_face_current, num_vertices_ * sizeof(uint32_t)));
    CUDA_CHECK(cudaMemset(d_vertex_face_current, 0, num_vertices_ * sizeof(uint32_t)));

    // Step 2: Fill adjacency list
    launchBuildVertexFaceList(
        h_mesh_ptrs_.faces_v0, h_mesh_ptrs_.faces_v1, h_mesh_ptrs_.faces_v2,
        h_mesh_ptrs_.vertex_face_offset,
        h_mesh_ptrs_.vertex_face_indices,
        d_vertex_face_current,
        num_faces_
    );

    // Cleanup temporary memory
    cudaFree(d_vertex_face_count);
    cudaFree(d_vertex_face_current);

    return true;
}

bool MeshGPU::computeCurvature() {
    std::cout << "[MeshGPU] Computing curvature..." << std::endl;

    if (h_mesh_ptrs_.vertex_face_offset == nullptr) {
        std::cerr << "[MeshGPU] Adjacency not built. Call buildVertexFaceAdjacency() first." << std::endl;
        return false;
    }

    launchComputeCurvature(
        h_mesh_ptrs_.vertices_x, h_mesh_ptrs_.vertices_y, h_mesh_ptrs_.vertices_z,
        h_mesh_ptrs_.normals_x, h_mesh_ptrs_.normals_y, h_mesh_ptrs_.normals_z,
        h_mesh_ptrs_.faces_v0, h_mesh_ptrs_.faces_v1, h_mesh_ptrs_.faces_v2,
        h_mesh_ptrs_.vertex_face_offset,
        h_mesh_ptrs_.vertex_face_indices,
        h_mesh_ptrs_.curvature,
        h_mesh_ptrs_.gaussian_curv,
        num_vertices_
    );

    return true;
}

bool MeshGPU::generateFeatureTexture(FeatureTexture& feature_tex) {
    std::cout << "[MeshGPU] Generating feature texture..." << std::endl;

    // Allocate feature texture memory
    feature_tex.num_vertices = num_vertices_;
    feature_tex.feature_dim = FEATURE_DIM;
    feature_tex.on_device = true;

    CUDA_CHECK(cudaMalloc(&feature_tex.features, num_vertices_ * FEATURE_DIM * sizeof(float)));

    float3_t bbox_size = bbox_.size();

    launchGenerateFeatureTexture(
        h_mesh_ptrs_.vertices_x, h_mesh_ptrs_.vertices_y, h_mesh_ptrs_.vertices_z,
        h_mesh_ptrs_.normals_x, h_mesh_ptrs_.normals_y, h_mesh_ptrs_.normals_z,
        h_mesh_ptrs_.curvature, h_mesh_ptrs_.gaussian_curv,
        feature_tex.features,
        num_vertices_,
        bbox_.min_pt.x, bbox_.min_pt.y, bbox_.min_pt.z,
        bbox_size.x, bbox_size.y, bbox_size.z
    );

    return true;
}

bool MeshGPU::initialize(const MeshSoA& host_mesh) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "[MeshGPU] Initializing..." << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. Upload to GPU
    if (!uploadToDevice(host_mesh)) {
        std::cerr << "[MeshGPU] Failed to upload mesh to device" << std::endl;
        return false;
    }

    // 2. Compute face normals
    if (!computeFaceNormals()) {
        std::cerr << "[MeshGPU] Failed to compute face normals" << std::endl;
        return false;
    }

    // 3. Compute vertex normals
    if (!computeVertexNormals()) {
        std::cerr << "[MeshGPU] Failed to compute vertex normals" << std::endl;
        return false;
    }

    // 4. Build adjacency list
    if (!buildVertexFaceAdjacency()) {
        std::cerr << "[MeshGPU] Failed to build adjacency" << std::endl;
        return false;
    }

    // 5. Compute curvature
    if (!computeCurvature()) {
        std::cerr << "[MeshGPU] Failed to compute curvature" << std::endl;
        return false;
    }

    initialized_ = true;

    std::cout << "========================================" << std::endl;
    std::cout << "[MeshGPU] Initialization complete!" << std::endl;
    std::cout << "  Vertices: " << num_vertices_ << std::endl;
    std::cout << "  Faces: " << num_faces_ << std::endl;
    std::cout << "  BBox: [" << bbox_.min_pt.x << ", " << bbox_.min_pt.y << ", " << bbox_.min_pt.z << "] - ["
              << bbox_.max_pt.x << ", " << bbox_.max_pt.y << ", " << bbox_.max_pt.z << "]" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return true;
}

// ============================================================================
// Grid Index Implementation
// ============================================================================

bool MeshGPU::buildGridIndex(float cell_size) {
    if (!initialized_) {
        std::cerr << "[MeshGPU] Not initialized. Call initialize() first." << std::endl;
        return false;
    }

    std::cout << "[MeshGPU] Building grid index (cell_size=" << cell_size << "mm)..." << std::endl;

    // Free existing grid
    freeGridMemory();

    // Compute grid dimensions
    float3_t bbox_size = bbox_.size();
    grid_.cell_size = cell_size;
    grid_.origin = bbox_.min_pt;

    grid_.dim_x = std::min((int)ceilf(bbox_size.x / cell_size) + 1, MAX_GRID_DIM);
    grid_.dim_y = std::min((int)ceilf(bbox_size.y / cell_size) + 1, MAX_GRID_DIM);
    grid_.dim_z = std::min((int)ceilf(bbox_size.z / cell_size) + 1, MAX_GRID_DIM);

    grid_.total_cells = grid_.dim_x * grid_.dim_y * grid_.dim_z;

    std::cout << "[MeshGPU] Grid dimensions: " << grid_.dim_x << " x " << grid_.dim_y << " x " << grid_.dim_z
              << " = " << grid_.total_cells << " cells" << std::endl;

    // Allocate grid memory
    CUDA_CHECK(cudaMalloc(&grid_.cell_vertex_id, grid_.total_cells * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&grid_.cell_vertex_count, grid_.total_cells * sizeof(uint32_t)));

    grid_.on_device = true;

    // Build grid index
    launchBuildGridIndex(
        h_mesh_ptrs_.vertices_x, h_mesh_ptrs_.vertices_y, h_mesh_ptrs_.vertices_z,
        grid_.cell_vertex_id, grid_.cell_vertex_count,
        grid_.origin.x, grid_.origin.y, grid_.origin.z,
        grid_.cell_size,
        grid_.dim_x, grid_.dim_y, grid_.dim_z,
        num_vertices_
    );

    // Count occupied cells
    std::vector<uint32_t> h_counts(grid_.total_cells);
    cudaMemcpy(h_counts.data(), grid_.cell_vertex_count, grid_.total_cells * sizeof(uint32_t), cudaMemcpyDeviceToHost);

    uint32_t occupied_cells = 0;
    uint32_t max_vertices_in_cell = 0;
    for (uint32_t i = 0; i < grid_.total_cells; ++i) {
        if (h_counts[i] > 0) {
            occupied_cells++;
            max_vertices_in_cell = std::max(max_vertices_in_cell, h_counts[i]);
        }
    }

    float occupancy = (float)occupied_cells / grid_.total_cells * 100.0f;
    std::cout << "[MeshGPU] Grid built: " << occupied_cells << "/" << grid_.total_cells
              << " cells occupied (" << occupancy << "%)" << std::endl;
    std::cout << "[MeshGPU] Max vertices per cell: " << max_vertices_in_cell << std::endl;

    // Build sorted vertex list for distance queries
    CUDA_CHECK(cudaMalloc(&grid_.cell_starts, grid_.total_cells * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&grid_.vertex_indices, num_vertices_ * sizeof(uint32_t)));

    launchBuildCellStartsAndScatter(
        h_mesh_ptrs_.vertices_x, h_mesh_ptrs_.vertices_y, h_mesh_ptrs_.vertices_z,
        grid_.cell_vertex_count, grid_.cell_starts, grid_.vertex_indices,
        grid_.origin.x, grid_.origin.y, grid_.origin.z,
        grid_.cell_size, grid_.dim_x, grid_.dim_y, grid_.dim_z,
        grid_.total_cells, num_vertices_
    );
    grid_.total_vertices_indexed = num_vertices_;
    std::cout << "[MeshGPU] Sorted vertex list built for distance queries." << std::endl;

    grid_initialized_ = true;
    return true;
}

void MeshGPU::freeMultiResGridMemory() {
    for (int i = 0; i < multi_grid_.num_levels; ++i) {
        GridIndex& g = multi_grid_.levels[i];
        if (g.cell_vertex_id) cudaFree(g.cell_vertex_id);
        if (g.cell_vertex_count) cudaFree(g.cell_vertex_count);
        if (g.cell_starts) cudaFree(g.cell_starts);
        if (g.vertex_indices) cudaFree(g.vertex_indices);
        memset(&g, 0, sizeof(GridIndex));
    }
    multi_grid_.num_levels = 0;
    multi_grid_initialized_ = false;
}

bool MeshGPU::buildMultiResGridIndex(const float* cell_sizes, int num_levels) {
    if (!initialized_) {
        std::cerr << "[MeshGPU] Not initialized. Call initialize() first." << std::endl;
        return false;
    }

    if (num_levels < 1 || num_levels > MAX_GRID_LEVELS) {
        std::cerr << "[MeshGPU] num_levels must be 1-" << MAX_GRID_LEVELS << std::endl;
        return false;
    }

    freeMultiResGridMemory();

    float3_t bbox_size = bbox_.size();
    multi_grid_.num_levels = num_levels;

    std::cout << "[MeshGPU] Building multi-resolution grid (" << num_levels << " levels)..." << std::endl;

    for (int lv = 0; lv < num_levels; ++lv) {
        float cs = cell_sizes[lv];
        multi_grid_.cell_sizes[lv] = cs;
        GridIndex& g = multi_grid_.levels[lv];

        g.cell_size = cs;
        g.origin = bbox_.min_pt;
        g.dim_x = std::min((int)ceilf(bbox_size.x / cs) + 1, MAX_GRID_DIM);
        g.dim_y = std::min((int)ceilf(bbox_size.y / cs) + 1, MAX_GRID_DIM);
        g.dim_z = std::min((int)ceilf(bbox_size.z / cs) + 1, MAX_GRID_DIM);
        g.total_cells = g.dim_x * g.dim_y * g.dim_z;
        g.on_device = true;

        cudaMalloc(&g.cell_vertex_id, g.total_cells * sizeof(uint32_t));
        cudaMalloc(&g.cell_vertex_count, g.total_cells * sizeof(uint32_t));

        launchBuildGridIndex(
            h_mesh_ptrs_.vertices_x, h_mesh_ptrs_.vertices_y, h_mesh_ptrs_.vertices_z,
            g.cell_vertex_id, g.cell_vertex_count,
            g.origin.x, g.origin.y, g.origin.z,
            g.cell_size, g.dim_x, g.dim_y, g.dim_z,
            num_vertices_
        );

        cudaMalloc(&g.cell_starts, g.total_cells * sizeof(uint32_t));
        cudaMalloc(&g.vertex_indices, num_vertices_ * sizeof(uint32_t));

        launchBuildCellStartsAndScatter(
            h_mesh_ptrs_.vertices_x, h_mesh_ptrs_.vertices_y, h_mesh_ptrs_.vertices_z,
            g.cell_vertex_count, g.cell_starts, g.vertex_indices,
            g.origin.x, g.origin.y, g.origin.z,
            g.cell_size, g.dim_x, g.dim_y, g.dim_z,
            g.total_cells, num_vertices_
        );
        g.total_vertices_indexed = num_vertices_;

        std::cout << "  Level " << lv << ": cell_size=" << cs << "mm, dims="
                  << g.dim_x << "x" << g.dim_y << "x" << g.dim_z
                  << " (" << g.total_cells << " cells)" << std::endl;
    }

    // Set default grid to finest level for backward compatibility
    // (Don't free the old grid_, just overwrite the struct - the finest level owns the memory)
    // Actually, keep grid_ independent. Users who call buildGridIndex separately get a separate grid.

    multi_grid_initialized_ = true;
    std::cout << "[MeshGPU] Multi-resolution grid built successfully." << std::endl;
    return true;
}

PointQueryResult MeshGPU::queryPoint(float x, float y, float z, int search_radius) {
    PointQueryResult result;
    result.valid = false;
    result.vertex_id = EMPTY_CELL;

    if (!grid_initialized_) {
        std::cerr << "[MeshGPU] Grid not built. Call buildGridIndex() first." << std::endl;
        return result;
    }

    float position[3], normal[3], curvatures[2];

    launchSinglePointQuery(
        x, y, z,
        h_mesh_ptrs_.vertices_x, h_mesh_ptrs_.vertices_y, h_mesh_ptrs_.vertices_z,
        h_mesh_ptrs_.normals_x, h_mesh_ptrs_.normals_y, h_mesh_ptrs_.normals_z,
        h_mesh_ptrs_.curvature, h_mesh_ptrs_.gaussian_curv,
        grid_.cell_vertex_id,
        grid_.origin.x, grid_.origin.y, grid_.origin.z,
        grid_.cell_size,
        grid_.dim_x, grid_.dim_y, grid_.dim_z,
        search_radius,
        &result.vertex_id,
        position,
        normal,
        curvatures,
        &result.distance
    );

    if (result.vertex_id != EMPTY_CELL) {
        result.valid = true;
        result.position = float3_t(position[0], position[1], position[2]);
        result.normal = float3_t(normal[0], normal[1], normal[2]);
        result.mean_curvature = curvatures[0];
        result.gaussian_curvature = curvatures[1];
    }

    return result;
}

bool MeshGPU::queryPoints(const float* query_x, const float* query_y, const float* query_z,
                          uint32_t num_queries, uint32_t* result_vertex_ids, float* result_distances,
                          int search_radius) {
    if (!grid_initialized_) {
        std::cerr << "[MeshGPU] Grid not built. Call buildGridIndex() first." << std::endl;
        return false;
    }

    // Allocate device memory for queries
    float *d_qx, *d_qy, *d_qz;
    uint32_t *d_result_vid;
    float *d_result_dist;

    CUDA_CHECK(cudaMalloc(&d_qx, num_queries * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_qy, num_queries * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_qz, num_queries * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_result_vid, num_queries * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&d_result_dist, num_queries * sizeof(float)));

    // Upload queries
    CUDA_CHECK(cudaMemcpy(d_qx, query_x, num_queries * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_qy, query_y, num_queries * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_qz, query_z, num_queries * sizeof(float), cudaMemcpyHostToDevice));

    // Launch batch query
    launchQueryNearestVertex(
        d_qx, d_qy, d_qz,
        h_mesh_ptrs_.vertices_x, h_mesh_ptrs_.vertices_y, h_mesh_ptrs_.vertices_z,
        h_mesh_ptrs_.normals_x, h_mesh_ptrs_.normals_y, h_mesh_ptrs_.normals_z,
        h_mesh_ptrs_.curvature, h_mesh_ptrs_.gaussian_curv,
        grid_.cell_vertex_id,
        d_result_vid, d_result_dist,
        grid_.origin.x, grid_.origin.y, grid_.origin.z,
        grid_.cell_size,
        grid_.dim_x, grid_.dim_y, grid_.dim_z,
        num_queries, search_radius
    );

    // Download results
    CUDA_CHECK(cudaMemcpy(result_vertex_ids, d_result_vid, num_queries * sizeof(uint32_t), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(result_distances, d_result_dist, num_queries * sizeof(float), cudaMemcpyDeviceToHost));

    // Cleanup
    cudaFree(d_qx);
    cudaFree(d_qy);
    cudaFree(d_qz);
    cudaFree(d_result_vid);
    cudaFree(d_result_dist);

    return true;
}
