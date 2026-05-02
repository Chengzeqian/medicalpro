#pragma once

// Always include cmath and algorithm for math functions and std::min/max
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cfloat>

// Check if CUDA runtime is available (either via nvcc or direct include)
#if defined(__CUDACC__) || defined(__CUDA_RUNTIME_H__)
#ifndef __CUDA_RUNTIME_H__
#include <cuda_runtime.h>
#endif
#define HOST_DEVICE __host__ __device__
#else
#define HOST_DEVICE
// Define int3 for non-CUDA builds only
struct int3 { int x, y, z; };
#endif

// ============================================================================
// Mesh GPU Data Structure Definitions
// ============================================================================

constexpr int MAX_VERTICES = 200000;
constexpr int MAX_FACES = 400000;

struct float3_t {
    float x, y, z;

    __host__ __device__ float3_t() : x(0), y(0), z(0) {}
    __host__ __device__ float3_t(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    __host__ __device__ float3_t operator+(const float3_t& other) const {
        return float3_t(x + other.x, y + other.y, z + other.z);
    }

    __host__ __device__ float3_t operator-(const float3_t& other) const {
        return float3_t(x - other.x, y - other.y, z - other.z);
    }

    __host__ __device__ float3_t operator*(float s) const {
        return float3_t(x * s, y * s, z * s);
    }

    __host__ __device__ float dot(const float3_t& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    __host__ __device__ float3_t cross(const float3_t& other) const {
        return float3_t(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    __host__ __device__ float length() const {
        return sqrtf(x * x + y * y + z * z);
    }

    __host__ __device__ float3_t normalized() const {
        float len = length();
        if (len > 1e-8f) {
            return float3_t(x / len, y / len, z / len);
        }
        return float3_t(0, 0, 0);
    }
};

struct Triangle {
    uint32_t v0, v1, v2;

    __host__ __device__ Triangle() : v0(0), v1(0), v2(0) {}
    __host__ __device__ Triangle(uint32_t _v0, uint32_t _v1, uint32_t _v2)
        : v0(_v0), v1(_v1), v2(_v2) {}
};

// ============================================================================
// Mesh Data Structure - Structure of Arrays (SoA) for GPU
// ============================================================================
struct MeshSoA {
    float* vertices_x;
    float* vertices_y;
    float* vertices_z;

    float* normals_x;
    float* normals_y;
    float* normals_z;

    float* curvature;
    float* gaussian_curv;

    uint32_t* faces_v0;
    uint32_t* faces_v1;
    uint32_t* faces_v2;

    float* face_normals_x;
    float* face_normals_y;
    float* face_normals_z;

    float* face_areas;

    uint32_t* vertex_face_offset;
    uint32_t* vertex_face_indices;

    uint32_t num_vertices;
    uint32_t num_faces;

    bool on_device;
};

// ============================================================================
// Feature Texture
// ============================================================================
struct FeatureTexture {
    float* features;

    uint32_t num_vertices;
    uint32_t feature_dim;

    bool on_device;
};

constexpr int FEATURE_NX = 0;
constexpr int FEATURE_NY = 1;
constexpr int FEATURE_NZ = 2;
constexpr int FEATURE_MEAN_CURV = 3;
constexpr int FEATURE_GAUSS_CURV = 4;
constexpr int FEATURE_POS_X = 5;
constexpr int FEATURE_POS_Y = 6;
constexpr int FEATURE_DIM = 7;

// ============================================================================
// 3D Grid Index for O(1) Point Query
// ============================================================================
constexpr float DEFAULT_CELL_SIZE = 2.0f;  // 2mm per cell
constexpr int MAX_GRID_DIM = 128;          // Max grid dimension per axis
constexpr uint32_t EMPTY_CELL = 0xFFFFFFFF; // Marker for empty cell

struct GridIndex {
    // Grid parameters
    float cell_size;           // Size of each cell (mm)
    int dim_x, dim_y, dim_z;   // Grid dimensions
    float3_t origin;           // Grid origin (min point of bbox)

    // Device pointers
    uint32_t* cell_vertex_id;  // [dim_x * dim_y * dim_z] - one vertex per cell
    uint32_t* cell_vertex_count; // [dim_x * dim_y * dim_z] - count of vertices per cell

    // Sorted vertex list for distance queries
    uint32_t* cell_starts;       // [total_cells] - prefix sum of cell_vertex_count
    uint32_t* vertex_indices;    // [total_vertices_indexed] - vertex IDs sorted by cell
    uint32_t total_vertices_indexed;

    uint32_t total_cells;
    bool on_device;

    // Convert world position to grid cell index
    __host__ __device__ int3 posToCell(float x, float y, float z) const {
        int3 cell;
        cell.x = (int)((x - origin.x) / cell_size);
        cell.y = (int)((y - origin.y) / cell_size);
        cell.z = (int)((z - origin.z) / cell_size);

        // Clamp to valid range
#ifdef __CUDACC__
        cell.x = max(0, min(cell.x, dim_x - 1));
        cell.y = max(0, min(cell.y, dim_y - 1));
        cell.z = max(0, min(cell.z, dim_z - 1));
#else
        cell.x = std::max(0, std::min(cell.x, dim_x - 1));
        cell.y = std::max(0, std::min(cell.y, dim_y - 1));
        cell.z = std::max(0, std::min(cell.z, dim_z - 1));
#endif

        return cell;
    }

    // Convert cell index to linear index
    __host__ __device__ uint32_t cellToLinear(int cx, int cy, int cz) const {
        return (uint32_t)(cz * dim_y * dim_x + cy * dim_x + cx);
    }

    // Query: get vertex ID at position (returns EMPTY_CELL if none)
    __host__ __device__ uint32_t queryVertex(float x, float y, float z) const {
        int3 cell = posToCell(x, y, z);
        uint32_t idx = cellToLinear(cell.x, cell.y, cell.z);
        return cell_vertex_id[idx];
    }
};

// ============================================================================
// Multi-Resolution Grid Index (pyramid of grids at different cell sizes)
// ============================================================================
constexpr int MAX_GRID_LEVELS = 3;

struct MultiResGridIndex {
    GridIndex levels[MAX_GRID_LEVELS];
    int num_levels;
    float cell_sizes[MAX_GRID_LEVELS];   // e.g., {4.0, 2.0, 1.0} (coarse to fine)

    MultiResGridIndex() : num_levels(0) {
        for (int i = 0; i < MAX_GRID_LEVELS; ++i) {
            cell_sizes[i] = 0.0f;
            memset(&levels[i], 0, sizeof(GridIndex));
        }
    }

    const GridIndex& getLevel(int level) const {
        int l = (level >= 0 && level < num_levels) ? level : num_levels - 1;
        return levels[l];
    }
};

// Query result structure
struct PointQueryResult {
    uint32_t vertex_id;        // Nearest vertex ID (EMPTY_CELL if not found)
    float3_t position;         // Vertex position
    float3_t normal;           // Vertex normal
    float mean_curvature;      // Mean curvature
    float gaussian_curvature;  // Gaussian curvature
    float distance;            // Distance to query point
    bool valid;                // Whether result is valid
};

// ============================================================================
// 4x4 Transformation Matrix (for rigid body transform)
// ============================================================================
struct Matrix4x4 {
    float m[16];  // Row-major: m[row*4 + col]

    __host__ __device__ Matrix4x4() {
        // Initialize to identity
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    // Create rotation matrix around Z axis (angle in radians)
    static Matrix4x4 rotationZ(float angle) {
        Matrix4x4 mat;
        float c = cosf(angle);
        float s = sinf(angle);
        mat.m[0] = c;  mat.m[1] = -s; mat.m[2] = 0;  mat.m[3] = 0;
        mat.m[4] = s;  mat.m[5] = c;  mat.m[6] = 0;  mat.m[7] = 0;
        mat.m[8] = 0;  mat.m[9] = 0;  mat.m[10] = 1; mat.m[11] = 0;
        mat.m[12] = 0; mat.m[13] = 0; mat.m[14] = 0; mat.m[15] = 1;
        return mat;
    }

    // Create rotation matrix around X axis (angle in radians)
    static Matrix4x4 rotationX(float angle) {
        Matrix4x4 mat;
        float c = cosf(angle);
        float s = sinf(angle);
        mat.m[0] = 1;  mat.m[1] = 0;  mat.m[2] = 0;  mat.m[3] = 0;
        mat.m[4] = 0;  mat.m[5] = c;  mat.m[6] = -s; mat.m[7] = 0;
        mat.m[8] = 0;  mat.m[9] = s;  mat.m[10] = c; mat.m[11] = 0;
        mat.m[12] = 0; mat.m[13] = 0; mat.m[14] = 0; mat.m[15] = 1;
        return mat;
    }

    // Create rotation matrix around Y axis (angle in radians)
    static Matrix4x4 rotationY(float angle) {
        Matrix4x4 mat;
        float c = cosf(angle);
        float s = sinf(angle);
        mat.m[0] = c;  mat.m[1] = 0;  mat.m[2] = s;  mat.m[3] = 0;
        mat.m[4] = 0;  mat.m[5] = 1;  mat.m[6] = 0;  mat.m[7] = 0;
        mat.m[8] = -s; mat.m[9] = 0;  mat.m[10] = c; mat.m[11] = 0;
        mat.m[12] = 0; mat.m[13] = 0; mat.m[14] = 0; mat.m[15] = 1;
        return mat;
    }

    // Create translation matrix
    static Matrix4x4 translation(float tx, float ty, float tz) {
        Matrix4x4 mat;
        mat.m[3] = tx;
        mat.m[7] = ty;
        mat.m[11] = tz;
        return mat;
    }

    // Matrix multiplication
    __host__ __device__ Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i * 4 + j] = 0;
                for (int k = 0; k < 4; ++k) {
                    result.m[i * 4 + j] += m[i * 4 + k] * other.m[k * 4 + j];
                }
            }
        }
        return result;
    }

    // Transform a point (assumes w=1)
    __host__ __device__ float3_t transformPoint(const float3_t& p) const {
        float3_t result;
        result.x = m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3];
        result.y = m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7];
        result.z = m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11];
        return result;
    }

    // Transform a vector (no translation, assumes w=0)
    __host__ __device__ float3_t transformVector(const float3_t& v) const {
        float3_t result;
        result.x = m[0] * v.x + m[1] * v.y + m[2] * v.z;
        result.y = m[4] * v.x + m[5] * v.y + m[6] * v.z;
        result.z = m[8] * v.x + m[9] * v.y + m[10] * v.z;
        return result;
    }

    // Get inverse (for rigid transforms: R^T and -R^T * t)
    Matrix4x4 inverse() const {
        Matrix4x4 result;
        // Transpose rotation part
        result.m[0] = m[0]; result.m[1] = m[4]; result.m[2] = m[8];
        result.m[4] = m[1]; result.m[5] = m[5]; result.m[6] = m[9];
        result.m[8] = m[2]; result.m[9] = m[6]; result.m[10] = m[10];
        // Translation: -R^T * t
        result.m[3] = -(result.m[0] * m[3] + result.m[1] * m[7] + result.m[2] * m[11]);
        result.m[7] = -(result.m[4] * m[3] + result.m[5] * m[7] + result.m[6] * m[11]);
        result.m[11] = -(result.m[8] * m[3] + result.m[9] * m[7] + result.m[10] * m[11]);
        result.m[15] = 1.0f;
        return result;
    }
};

// ============================================================================
// Simulated Probe Point Cloud (Source for registration)
// ============================================================================
struct SourcePointCloud {
    float* points_x;       // Device pointer
    float* points_y;       // Device pointer
    float* points_z;       // Device pointer
    float* normals_x;      // Device pointer (optional)
    float* normals_y;      // Device pointer (optional)
    float* normals_z;      // Device pointer (optional)

    uint32_t num_points;
    bool on_device;

    Matrix4x4 ground_truth_transform;  // Known transform applied during simulation
    float noise_stddev;                // Standard deviation of added noise
};

// ============================================================================
// Bounding Box
// ============================================================================
struct BoundingBox {
    float3_t min_pt;
    float3_t max_pt;

    __host__ __device__ float3_t center() const {
        return float3_t(
            (min_pt.x + max_pt.x) * 0.5f,
            (min_pt.y + max_pt.y) * 0.5f,
            (min_pt.z + max_pt.z) * 0.5f
        );
    }

    __host__ __device__ float3_t size() const {
        return float3_t(
            max_pt.x - min_pt.x,
            max_pt.y - min_pt.y,
            max_pt.z - min_pt.z
        );
    }

    __host__ __device__ float diagonal() const {
        float3_t s = size();
        return sqrtf(s.x * s.x + s.y * s.y + s.z * s.z);
    }
};
