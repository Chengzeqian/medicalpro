#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cstdint>
#include <cstdio>
#include <vector>

// ============================================================================
// CUDA Kernels: Mesh Geometry Feature Computation
// ============================================================================

#define BLOCK_SIZE 256

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            printf("CUDA Error: %s at %s:%d\n", cudaGetErrorString(err), __FILE__, __LINE__); \
        } \
    } while(0)

// ============================================================================
// Kernel 1: Compute Face Normals and Areas
// ============================================================================
__global__ void computeFaceNormalsKernel(
    const float* __restrict__ vertices_x,
    const float* __restrict__ vertices_y,
    const float* __restrict__ vertices_z,
    const uint32_t* __restrict__ faces_v0,
    const uint32_t* __restrict__ faces_v1,
    const uint32_t* __restrict__ faces_v2,
    float* __restrict__ face_normals_x,
    float* __restrict__ face_normals_y,
    float* __restrict__ face_normals_z,
    float* __restrict__ face_areas,
    uint32_t num_faces
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_faces) return;

    uint32_t v0 = faces_v0[idx];
    uint32_t v1 = faces_v1[idx];
    uint32_t v2 = faces_v2[idx];

    float p0x = vertices_x[v0], p0y = vertices_y[v0], p0z = vertices_z[v0];
    float p1x = vertices_x[v1], p1y = vertices_y[v1], p1z = vertices_z[v1];
    float p2x = vertices_x[v2], p2y = vertices_y[v2], p2z = vertices_z[v2];

    float e1x = p1x - p0x, e1y = p1y - p0y, e1z = p1z - p0z;
    float e2x = p2x - p0x, e2y = p2y - p0y, e2z = p2z - p0z;

    float nx = e1y * e2z - e1z * e2y;
    float ny = e1z * e2x - e1x * e2z;
    float nz = e1x * e2y - e1y * e2x;

    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    float area = len * 0.5f;

    if (len > 1e-8f) {
        nx /= len;
        ny /= len;
        nz /= len;
    } else {
        nx = ny = nz = 0.0f;
    }

    face_normals_x[idx] = nx;
    face_normals_y[idx] = ny;
    face_normals_z[idx] = nz;
    face_areas[idx] = area;
}

extern "C" void launchComputeFaceNormals(
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    float* face_normals_x, float* face_normals_y, float* face_normals_z,
    float* face_areas,
    uint32_t num_faces
) {
    int num_blocks = (num_faces + BLOCK_SIZE - 1) / BLOCK_SIZE;
    computeFaceNormalsKernel<<<num_blocks, BLOCK_SIZE>>>(
        vertices_x, vertices_y, vertices_z,
        faces_v0, faces_v1, faces_v2,
        face_normals_x, face_normals_y, face_normals_z,
        face_areas, num_faces
    );
    CUDA_CHECK(cudaDeviceSynchronize());
}

// ============================================================================
// Kernel 2: Compute Vertex Normals (weighted average from face normals)
// ============================================================================
__global__ void accumulateVertexNormalsKernel(
    const uint32_t* __restrict__ faces_v0,
    const uint32_t* __restrict__ faces_v1,
    const uint32_t* __restrict__ faces_v2,
    const float* __restrict__ face_normals_x,
    const float* __restrict__ face_normals_y,
    const float* __restrict__ face_normals_z,
    const float* __restrict__ face_areas,
    float* __restrict__ vertex_normals_x,
    float* __restrict__ vertex_normals_y,
    float* __restrict__ vertex_normals_z,
    uint32_t num_faces
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_faces) return;

    uint32_t v0 = faces_v0[idx];
    uint32_t v1 = faces_v1[idx];
    uint32_t v2 = faces_v2[idx];

    float nx = face_normals_x[idx];
    float ny = face_normals_y[idx];
    float nz = face_normals_z[idx];
    float area = face_areas[idx];

    float wnx = nx * area;
    float wny = ny * area;
    float wnz = nz * area;

    atomicAdd(&vertex_normals_x[v0], wnx);
    atomicAdd(&vertex_normals_y[v0], wny);
    atomicAdd(&vertex_normals_z[v0], wnz);

    atomicAdd(&vertex_normals_x[v1], wnx);
    atomicAdd(&vertex_normals_y[v1], wny);
    atomicAdd(&vertex_normals_z[v1], wnz);

    atomicAdd(&vertex_normals_x[v2], wnx);
    atomicAdd(&vertex_normals_y[v2], wny);
    atomicAdd(&vertex_normals_z[v2], wnz);
}

__global__ void normalizeVertexNormalsKernel(
    float* __restrict__ vertex_normals_x,
    float* __restrict__ vertex_normals_y,
    float* __restrict__ vertex_normals_z,
    uint32_t num_vertices
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_vertices) return;

    float nx = vertex_normals_x[idx];
    float ny = vertex_normals_y[idx];
    float nz = vertex_normals_z[idx];

    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (len > 1e-8f) {
        vertex_normals_x[idx] = nx / len;
        vertex_normals_y[idx] = ny / len;
        vertex_normals_z[idx] = nz / len;
    } else {
        vertex_normals_x[idx] = 0.0f;
        vertex_normals_y[idx] = 0.0f;
        vertex_normals_z[idx] = 1.0f;
    }
}

extern "C" void launchComputeVertexNormals(
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    const float* face_normals_x, const float* face_normals_y, const float* face_normals_z,
    const float* face_areas,
    float* vertex_normals_x, float* vertex_normals_y, float* vertex_normals_z,
    uint32_t num_vertices, uint32_t num_faces
) {
    CUDA_CHECK(cudaMemset(vertex_normals_x, 0, num_vertices * sizeof(float)));
    CUDA_CHECK(cudaMemset(vertex_normals_y, 0, num_vertices * sizeof(float)));
    CUDA_CHECK(cudaMemset(vertex_normals_z, 0, num_vertices * sizeof(float)));

    int num_blocks_faces = (num_faces + BLOCK_SIZE - 1) / BLOCK_SIZE;
    accumulateVertexNormalsKernel<<<num_blocks_faces, BLOCK_SIZE>>>(
        faces_v0, faces_v1, faces_v2,
        face_normals_x, face_normals_y, face_normals_z,
        face_areas,
        vertex_normals_x, vertex_normals_y, vertex_normals_z,
        num_faces
    );
    CUDA_CHECK(cudaDeviceSynchronize());

    int num_blocks_verts = (num_vertices + BLOCK_SIZE - 1) / BLOCK_SIZE;
    normalizeVertexNormalsKernel<<<num_blocks_verts, BLOCK_SIZE>>>(
        vertex_normals_x, vertex_normals_y, vertex_normals_z, num_vertices
    );
    CUDA_CHECK(cudaDeviceSynchronize());
}

// ============================================================================
// Kernel 3: Build Vertex-Face Adjacency
// ============================================================================
__global__ void countVertexFacesKernel(
    const uint32_t* __restrict__ faces_v0,
    const uint32_t* __restrict__ faces_v1,
    const uint32_t* __restrict__ faces_v2,
    uint32_t* __restrict__ vertex_face_count,
    uint32_t num_faces
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_faces) return;

    atomicAdd(&vertex_face_count[faces_v0[idx]], 1);
    atomicAdd(&vertex_face_count[faces_v1[idx]], 1);
    atomicAdd(&vertex_face_count[faces_v2[idx]], 1);
}

extern "C" void launchCountVertexFaces(
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    uint32_t* vertex_face_count,
    uint32_t num_vertices, uint32_t num_faces
) {
    CUDA_CHECK(cudaMemset(vertex_face_count, 0, num_vertices * sizeof(uint32_t)));

    int num_blocks = (num_faces + BLOCK_SIZE - 1) / BLOCK_SIZE;
    countVertexFacesKernel<<<num_blocks, BLOCK_SIZE>>>(
        faces_v0, faces_v1, faces_v2, vertex_face_count, num_faces
    );
    CUDA_CHECK(cudaDeviceSynchronize());
}

__global__ void buildVertexFaceListKernel(
    const uint32_t* __restrict__ faces_v0,
    const uint32_t* __restrict__ faces_v1,
    const uint32_t* __restrict__ faces_v2,
    const uint32_t* __restrict__ vertex_face_offset,
    uint32_t* __restrict__ vertex_face_indices,
    uint32_t* __restrict__ vertex_face_current,
    uint32_t num_faces
) {
    uint32_t face_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (face_idx >= num_faces) return;

    uint32_t v0 = faces_v0[face_idx];
    uint32_t v1 = faces_v1[face_idx];
    uint32_t v2 = faces_v2[face_idx];

    uint32_t pos0 = atomicAdd(&vertex_face_current[v0], 1);
    uint32_t pos1 = atomicAdd(&vertex_face_current[v1], 1);
    uint32_t pos2 = atomicAdd(&vertex_face_current[v2], 1);

    vertex_face_indices[vertex_face_offset[v0] + pos0] = face_idx;
    vertex_face_indices[vertex_face_offset[v1] + pos1] = face_idx;
    vertex_face_indices[vertex_face_offset[v2] + pos2] = face_idx;
}

extern "C" void launchBuildVertexFaceList(
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    const uint32_t* vertex_face_offset,
    uint32_t* vertex_face_indices,
    uint32_t* vertex_face_current,
    uint32_t num_faces
) {
    int num_blocks = (num_faces + BLOCK_SIZE - 1) / BLOCK_SIZE;
    buildVertexFaceListKernel<<<num_blocks, BLOCK_SIZE>>>(
        faces_v0, faces_v1, faces_v2,
        vertex_face_offset, vertex_face_indices, vertex_face_current,
        num_faces
    );
    CUDA_CHECK(cudaDeviceSynchronize());
}

// ============================================================================
// Kernel 4: Compute Curvature
// ============================================================================
__global__ void computeCurvatureKernel(
    const float* __restrict__ vertices_x,
    const float* __restrict__ vertices_y,
    const float* __restrict__ vertices_z,
    const float* __restrict__ vertex_normals_x,
    const float* __restrict__ vertex_normals_y,
    const float* __restrict__ vertex_normals_z,
    const uint32_t* __restrict__ faces_v0,
    const uint32_t* __restrict__ faces_v1,
    const uint32_t* __restrict__ faces_v2,
    const uint32_t* __restrict__ vertex_face_offset,
    const uint32_t* __restrict__ vertex_face_indices,
    float* __restrict__ curvature,
    float* __restrict__ gaussian_curvature,
    uint32_t num_vertices
) {
    uint32_t vi = blockIdx.x * blockDim.x + threadIdx.x;
    if (vi >= num_vertices) return;

    float px = vertices_x[vi];
    float py = vertices_y[vi];
    float pz = vertices_z[vi];

    uint32_t start = vertex_face_offset[vi];
    uint32_t end = vertex_face_offset[vi + 1];

    float angle_sum = 0.0f;
    float area_sum = 0.0f;
    float mean_curv_sum = 0.0f;

    for (uint32_t i = start; i < end; ++i) {
        uint32_t face_idx = vertex_face_indices[i];

        uint32_t fv0 = faces_v0[face_idx];
        uint32_t fv1 = faces_v1[face_idx];
        uint32_t fv2 = faces_v2[face_idx];

        uint32_t vj, vk;
        if (fv0 == vi) {
            vj = fv1; vk = fv2;
        } else if (fv1 == vi) {
            vj = fv2; vk = fv0;
        } else {
            vj = fv0; vk = fv1;
        }

        float e1x = vertices_x[vj] - px;
        float e1y = vertices_y[vj] - py;
        float e1z = vertices_z[vj] - pz;

        float e2x = vertices_x[vk] - px;
        float e2y = vertices_y[vk] - py;
        float e2z = vertices_z[vk] - pz;

        float len1 = sqrtf(e1x*e1x + e1y*e1y + e1z*e1z);
        float len2 = sqrtf(e2x*e2x + e2y*e2y + e2z*e2z);

        if (len1 > 1e-8f && len2 > 1e-8f) {
            float dot = (e1x*e2x + e1y*e2y + e1z*e2z) / (len1 * len2);
            dot = fminf(fmaxf(dot, -1.0f), 1.0f);
            float angle = acosf(dot);
            angle_sum += angle;

            float cx = e1y*e2z - e1z*e2y;
            float cy = e1z*e2x - e1x*e2z;
            float cz = e1x*e2y - e1y*e2x;
            float area = 0.5f * sqrtf(cx*cx + cy*cy + cz*cz);
            area_sum += area / 3.0f;

            float tan_angle = tanf(angle);
            float cot_weight = 1.0f / (tan_angle + 1e-8f);
            mean_curv_sum += cot_weight * (len1 + len2) * 0.5f;
        }
    }

    float gauss_curv = 0.0f;
    if (area_sum > 1e-8f) {
        gauss_curv = (2.0f * 3.14159265f - angle_sum) / area_sum;
    }

    float mean_curv = 0.0f;
    if (area_sum > 1e-8f && (end - start) > 0) {
        mean_curv = mean_curv_sum / (4.0f * area_sum);
    }

    curvature[vi] = fabsf(mean_curv);
    gaussian_curvature[vi] = gauss_curv;
}

extern "C" void launchComputeCurvature(
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    const float* vertex_normals_x, const float* vertex_normals_y, const float* vertex_normals_z,
    const uint32_t* faces_v0, const uint32_t* faces_v1, const uint32_t* faces_v2,
    const uint32_t* vertex_face_offset,
    const uint32_t* vertex_face_indices,
    float* curvature,
    float* gaussian_curvature,
    uint32_t num_vertices
) {
    int num_blocks = (num_vertices + BLOCK_SIZE - 1) / BLOCK_SIZE;
    computeCurvatureKernel<<<num_blocks, BLOCK_SIZE>>>(
        vertices_x, vertices_y, vertices_z,
        vertex_normals_x, vertex_normals_y, vertex_normals_z,
        faces_v0, faces_v1, faces_v2,
        vertex_face_offset, vertex_face_indices,
        curvature, gaussian_curvature,
        num_vertices
    );
    CUDA_CHECK(cudaDeviceSynchronize());
}

// ============================================================================
// Kernel 5: Generate Feature Texture
// ============================================================================
__global__ void generateFeatureTextureKernel(
    const float* __restrict__ vertices_x,
    const float* __restrict__ vertices_y,
    const float* __restrict__ vertices_z,
    const float* __restrict__ normals_x,
    const float* __restrict__ normals_y,
    const float* __restrict__ normals_z,
    const float* __restrict__ curvature,
    const float* __restrict__ gaussian_curvature,
    float* __restrict__ features,
    uint32_t num_vertices,
    float bbox_min_x, float bbox_min_y, float bbox_min_z,
    float bbox_size_x, float bbox_size_y, float bbox_size_z
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_vertices) return;

    const int FEAT_DIM = 7;
    float* feat = features + idx * FEAT_DIM;

    feat[0] = normals_x[idx];
    feat[1] = normals_y[idx];
    feat[2] = normals_z[idx];

    feat[3] = tanhf(curvature[idx] * 0.1f);
    feat[4] = tanhf(gaussian_curvature[idx] * 0.01f);

    if (bbox_size_x > 1e-8f)
        feat[5] = (vertices_x[idx] - bbox_min_x) / bbox_size_x;
    else
        feat[5] = 0.5f;

    if (bbox_size_y > 1e-8f)
        feat[6] = (vertices_y[idx] - bbox_min_y) / bbox_size_y;
    else
        feat[6] = 0.5f;
}

extern "C" void launchGenerateFeatureTexture(
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    const float* normals_x, const float* normals_y, const float* normals_z,
    const float* curvature, const float* gaussian_curvature,
    float* features,
    uint32_t num_vertices,
    float bbox_min_x, float bbox_min_y, float bbox_min_z,
    float bbox_size_x, float bbox_size_y, float bbox_size_z
) {
    int num_blocks = (num_vertices + BLOCK_SIZE - 1) / BLOCK_SIZE;
    generateFeatureTextureKernel<<<num_blocks, BLOCK_SIZE>>>(
        vertices_x, vertices_y, vertices_z,
        normals_x, normals_y, normals_z,
        curvature, gaussian_curvature,
        features, num_vertices,
        bbox_min_x, bbox_min_y, bbox_min_z,
        bbox_size_x, bbox_size_y, bbox_size_z
    );
    CUDA_CHECK(cudaDeviceSynchronize());
}

// ============================================================================
// Kernel 6: Build Grid Index
// ============================================================================
constexpr uint32_t EMPTY_CELL_VALUE = 0xFFFFFFFF;

__global__ void buildGridIndexKernel(
    const float* __restrict__ vertices_x,
    const float* __restrict__ vertices_y,
    const float* __restrict__ vertices_z,
    uint32_t* __restrict__ cell_vertex_id,
    uint32_t* __restrict__ cell_vertex_count,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    uint32_t num_vertices
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_vertices) return;

    float px = vertices_x[idx];
    float py = vertices_y[idx];
    float pz = vertices_z[idx];

    // Compute cell index
    int cx = (int)((px - origin_x) / cell_size);
    int cy = (int)((py - origin_y) / cell_size);
    int cz = (int)((pz - origin_z) / cell_size);

    // Clamp to valid range
    cx = max(0, min(cx, dim_x - 1));
    cy = max(0, min(cy, dim_y - 1));
    cz = max(0, min(cz, dim_z - 1));

    // Linear index
    uint32_t cell_idx = cz * dim_y * dim_x + cy * dim_x + cx;

    // Atomically store this vertex ID (last one wins, which is fine for V1)
    atomicExch(&cell_vertex_id[cell_idx], idx);
    atomicAdd(&cell_vertex_count[cell_idx], 1);
}

extern "C" void launchBuildGridIndex(
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    uint32_t* cell_vertex_id,
    uint32_t* cell_vertex_count,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    uint32_t num_vertices
) {
    uint32_t total_cells = dim_x * dim_y * dim_z;

    // Initialize cells to empty
    CUDA_CHECK(cudaMemset(cell_vertex_id, 0xFF, total_cells * sizeof(uint32_t)));
    CUDA_CHECK(cudaMemset(cell_vertex_count, 0, total_cells * sizeof(uint32_t)));

    int num_blocks = (num_vertices + BLOCK_SIZE - 1) / BLOCK_SIZE;
    buildGridIndexKernel<<<num_blocks, BLOCK_SIZE>>>(
        vertices_x, vertices_y, vertices_z,
        cell_vertex_id, cell_vertex_count,
        origin_x, origin_y, origin_z,
        cell_size, dim_x, dim_y, dim_z,
        num_vertices
    );
    CUDA_CHECK(cudaDeviceSynchronize());
}

// ============================================================================
// Kernel 7: Query Nearest Vertex in Grid (with local search)
// ============================================================================
__global__ void queryNearestVertexKernel(
    const float* __restrict__ query_x,
    const float* __restrict__ query_y,
    const float* __restrict__ query_z,
    const float* __restrict__ vertices_x,
    const float* __restrict__ vertices_y,
    const float* __restrict__ vertices_z,
    const float* __restrict__ normals_x,
    const float* __restrict__ normals_y,
    const float* __restrict__ normals_z,
    const float* __restrict__ curvature,
    const float* __restrict__ gaussian_curvature,
    const uint32_t* __restrict__ cell_vertex_id,
    uint32_t* __restrict__ result_vertex_id,
    float* __restrict__ result_distance,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    uint32_t num_queries,
    int search_radius
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_queries) return;

    float qx = query_x[idx];
    float qy = query_y[idx];
    float qz = query_z[idx];

    // Get center cell
    int cx = (int)((qx - origin_x) / cell_size);
    int cy = (int)((qy - origin_y) / cell_size);
    int cz = (int)((qz - origin_z) / cell_size);

    cx = max(0, min(cx, dim_x - 1));
    cy = max(0, min(cy, dim_y - 1));
    cz = max(0, min(cz, dim_z - 1));

    // Search in neighborhood
    uint32_t best_vid = EMPTY_CELL_VALUE;
    float best_dist_sq = 1e30f;

    for (int dz = -search_radius; dz <= search_radius; ++dz) {
        for (int dy = -search_radius; dy <= search_radius; ++dy) {
            for (int dx = -search_radius; dx <= search_radius; ++dx) {
                int nx = cx + dx;
                int ny = cy + dy;
                int nz = cz + dz;

                if (nx >= 0 && nx < dim_x && ny >= 0 && ny < dim_y && nz >= 0 && nz < dim_z) {
                    uint32_t cell_idx = nz * dim_y * dim_x + ny * dim_x + nx;
                    uint32_t vid = cell_vertex_id[cell_idx];

                    if (vid != EMPTY_CELL_VALUE) {
                        float vx = vertices_x[vid];
                        float vy = vertices_y[vid];
                        float vz = vertices_z[vid];

                        float dist_sq = (vx - qx) * (vx - qx) +
                                       (vy - qy) * (vy - qy) +
                                       (vz - qz) * (vz - qz);

                        if (dist_sq < best_dist_sq) {
                            best_dist_sq = dist_sq;
                            best_vid = vid;
                        }
                    }
                }
            }
        }
    }

    result_vertex_id[idx] = best_vid;
    result_distance[idx] = sqrtf(best_dist_sq);
}

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
) {
    int num_blocks = (num_queries + BLOCK_SIZE - 1) / BLOCK_SIZE;
    queryNearestVertexKernel<<<num_blocks, BLOCK_SIZE>>>(
        query_x, query_y, query_z,
        vertices_x, vertices_y, vertices_z,
        normals_x, normals_y, normals_z,
        curvature, gaussian_curvature,
        cell_vertex_id,
        result_vertex_id, result_distance,
        origin_x, origin_y, origin_z,
        cell_size, dim_x, dim_y, dim_z,
        num_queries, search_radius
    );
    CUDA_CHECK(cudaDeviceSynchronize());
}

// ============================================================================
// Single Point Query (CPU callable, for probe input)
// ============================================================================
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
    // Output (host memory)
    uint32_t* out_vertex_id,
    float* out_position,  // [3]
    float* out_normal,    // [3]
    float* out_curvatures, // [2]
    float* out_distance
) {
    // Allocate device memory for single query
    float *d_qx, *d_qy, *d_qz;
    uint32_t *d_result_vid;
    float *d_result_dist;

    CUDA_CHECK(cudaMalloc(&d_qx, sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_qy, sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_qz, sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_result_vid, sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&d_result_dist, sizeof(float)));

    CUDA_CHECK(cudaMemcpy(d_qx, &query_x, sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_qy, &query_y, sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_qz, &query_z, sizeof(float), cudaMemcpyHostToDevice));

    // Launch kernel with single query
    queryNearestVertexKernel<<<1, 1>>>(
        d_qx, d_qy, d_qz,
        vertices_x, vertices_y, vertices_z,
        normals_x, normals_y, normals_z,
        curvature, gaussian_curvature,
        cell_vertex_id,
        d_result_vid, d_result_dist,
        origin_x, origin_y, origin_z,
        cell_size, dim_x, dim_y, dim_z,
        1, search_radius
    );
    CUDA_CHECK(cudaDeviceSynchronize());

    // Copy result back
    uint32_t vid;
    float dist;
    CUDA_CHECK(cudaMemcpy(&vid, d_result_vid, sizeof(uint32_t), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(&dist, d_result_dist, sizeof(float), cudaMemcpyDeviceToHost));

    *out_vertex_id = vid;
    *out_distance = dist;

    // If valid vertex found, copy its attributes
    if (vid != EMPTY_CELL_VALUE) {
        CUDA_CHECK(cudaMemcpy(&out_position[0], vertices_x + vid, sizeof(float), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&out_position[1], vertices_y + vid, sizeof(float), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&out_position[2], vertices_z + vid, sizeof(float), cudaMemcpyDeviceToHost));

        CUDA_CHECK(cudaMemcpy(&out_normal[0], normals_x + vid, sizeof(float), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&out_normal[1], normals_y + vid, sizeof(float), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&out_normal[2], normals_z + vid, sizeof(float), cudaMemcpyDeviceToHost));

        CUDA_CHECK(cudaMemcpy(&out_curvatures[0], curvature + vid, sizeof(float), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&out_curvatures[1], gaussian_curvature + vid, sizeof(float), cudaMemcpyDeviceToHost));
    }

    // Cleanup
    cudaFree(d_qx);
    cudaFree(d_qy);
    cudaFree(d_qz);
    cudaFree(d_result_vid);
    cudaFree(d_result_dist);
}

// ============================================================================
// Kernel: Scatter vertices into sorted-by-cell list
// ============================================================================

__global__ void scatterVerticesKernel(
    const float* __restrict__ vertices_x,
    const float* __restrict__ vertices_y,
    const float* __restrict__ vertices_z,
    const uint32_t* __restrict__ cell_starts,
    uint32_t* __restrict__ cell_write_offset,
    uint32_t* __restrict__ vertex_indices,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    uint32_t num_vertices
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_vertices) return;

    float px = vertices_x[idx];
    float py = vertices_y[idx];
    float pz = vertices_z[idx];

    int cx = (int)((px - origin_x) / cell_size);
    int cy = (int)((py - origin_y) / cell_size);
    int cz = (int)((pz - origin_z) / cell_size);

    cx = max(0, min(cx, dim_x - 1));
    cy = max(0, min(cy, dim_y - 1));
    cz = max(0, min(cz, dim_z - 1));

    uint32_t cell_idx = cz * dim_y * dim_x + cy * dim_x + cx;
    uint32_t offset = atomicAdd(&cell_write_offset[cell_idx], 1);
    vertex_indices[cell_starts[cell_idx] + offset] = idx;
}

extern "C" void launchBuildCellStartsAndScatter(
    const float* vertices_x, const float* vertices_y, const float* vertices_z,
    const uint32_t* cell_vertex_count,
    uint32_t* cell_starts,
    uint32_t* vertex_indices,
    float origin_x, float origin_y, float origin_z,
    float cell_size,
    int dim_x, int dim_y, int dim_z,
    uint32_t total_cells,
    uint32_t num_vertices
) {
    // Step 1: Compute exclusive prefix sum on CPU (grid is small)
    std::vector<uint32_t> h_counts(total_cells);
    std::vector<uint32_t> h_starts(total_cells);
    CUDA_CHECK(cudaMemcpy(h_counts.data(), cell_vertex_count,
                          total_cells * sizeof(uint32_t), cudaMemcpyDeviceToHost));

    uint32_t running = 0;
    for (uint32_t i = 0; i < total_cells; ++i) {
        h_starts[i] = running;
        running += h_counts[i];
    }
    CUDA_CHECK(cudaMemcpy(cell_starts, h_starts.data(),
                          total_cells * sizeof(uint32_t), cudaMemcpyHostToDevice));

    // Step 2: Scatter vertices — need temp write offset array
    uint32_t* d_write_offset = nullptr;
    CUDA_CHECK(cudaMalloc(&d_write_offset, total_cells * sizeof(uint32_t)));
    CUDA_CHECK(cudaMemset(d_write_offset, 0, total_cells * sizeof(uint32_t)));

    int num_blocks = (num_vertices + BLOCK_SIZE - 1) / BLOCK_SIZE;
    scatterVerticesKernel<<<num_blocks, BLOCK_SIZE>>>(
        vertices_x, vertices_y, vertices_z,
        cell_starts, d_write_offset, vertex_indices,
        origin_x, origin_y, origin_z,
        cell_size, dim_x, dim_y, dim_z,
        num_vertices
    );
    CUDA_CHECK(cudaDeviceSynchronize());
    cudaFree(d_write_offset);
}
