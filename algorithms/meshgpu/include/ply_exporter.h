#pragma once

#include "types.h"
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <cuda_runtime.h>

// ============================================================================
// PLY Exporter - Export point clouds to PLY files for visualization
// Supports both MeshSoA (with faces) and raw point clouds (no faces)
// ============================================================================

class PLYExporter {
public:
    // Export SourcePointCloud to PLY (points only, no faces)
    // Automatically downloads from GPU if needed
    static bool exportSourceCloud(
        const std::string& filename,
        const SourcePointCloud& cloud,
        bool binary = true
    );

    // Export SourcePointCloud with a transformation applied
    // Useful for exporting source at different registration stages
    static bool exportSourceCloudTransformed(
        const std::string& filename,
        const SourcePointCloud& cloud,
        const Matrix4x4& transform,
        bool binary = true
    );

    // Export MeshSoA to PLY (with faces)
    static bool exportMesh(
        const std::string& filename,
        const MeshSoA& mesh,
        bool binary = true
    );

    // Export raw point arrays from GPU
    static bool exportPointsFromGPU(
        const std::string& filename,
        const float* d_x, const float* d_y, const float* d_z,
        const float* d_nx, const float* d_ny, const float* d_nz,  // Can be nullptr
        uint32_t num_points,
        bool binary = true
    );

    // Export raw point arrays from Host
    static bool exportPointsFromHost(
        const std::string& filename,
        const float* h_x, const float* h_y, const float* h_z,
        const float* h_nx, const float* h_ny, const float* h_nz,  // Can be nullptr
        uint32_t num_points,
        bool binary = true
    );

    // Create output directory if it doesn't exist
    static bool ensureDirectoryExists(const std::string& dir_path);
};

// ============================================================================
// Implementation
// ============================================================================

inline bool PLYExporter::ensureDirectoryExists(const std::string& dir_path) {
    // Use system command to create directory (cross-platform would need more work)
    #ifdef _WIN32
    std::string cmd = "if not exist \"" + dir_path + "\" mkdir \"" + dir_path + "\"";
    #else
    std::string cmd = "mkdir -p \"" + dir_path + "\"";
    #endif
    int result = system(cmd.c_str());
    return result == 0;
}

inline bool PLYExporter::exportPointsFromHost(
    const std::string& filename,
    const float* h_x, const float* h_y, const float* h_z,
    const float* h_nx, const float* h_ny, const float* h_nz,
    uint32_t num_points,
    bool binary
) {
    std::ofstream file;

    if (binary) {
        file.open(filename, std::ios::binary);
    } else {
        file.open(filename);
    }

    if (!file.is_open()) {
        std::cerr << "[PLYExporter] Failed to create file: " << filename << std::endl;
        return false;
    }

    bool has_normals = (h_nx != nullptr && h_ny != nullptr && h_nz != nullptr);

    std::cout << "[PLYExporter] Writing " << filename << std::endl;
    std::cout << "  Points: " << num_points << std::endl;
    std::cout << "  Normals: " << (has_normals ? "yes" : "no") << std::endl;
    std::cout << "  Format: " << (binary ? "binary_little_endian" : "ascii") << std::endl;

    // Write header
    file << "ply\n";
    file << "format " << (binary ? "binary_little_endian" : "ascii") << " 1.0\n";
    file << "element vertex " << num_points << "\n";
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";

    if (has_normals) {
        file << "property float nx\n";
        file << "property float ny\n";
        file << "property float nz\n";
    }

    file << "end_header\n";

    if (binary) {
        // Write vertex data (binary)
        for (uint32_t i = 0; i < num_points; ++i) {
            file.write(reinterpret_cast<const char*>(&h_x[i]), sizeof(float));
            file.write(reinterpret_cast<const char*>(&h_y[i]), sizeof(float));
            file.write(reinterpret_cast<const char*>(&h_z[i]), sizeof(float));

            if (has_normals) {
                file.write(reinterpret_cast<const char*>(&h_nx[i]), sizeof(float));
                file.write(reinterpret_cast<const char*>(&h_ny[i]), sizeof(float));
                file.write(reinterpret_cast<const char*>(&h_nz[i]), sizeof(float));
            }
        }
    } else {
        // Write vertex data (ASCII)
        for (uint32_t i = 0; i < num_points; ++i) {
            file << h_x[i] << " " << h_y[i] << " " << h_z[i];
            if (has_normals) {
                file << " " << h_nx[i] << " " << h_ny[i] << " " << h_nz[i];
            }
            file << "\n";
        }
    }

    file.close();
    std::cout << "[PLYExporter] Successfully saved " << filename << std::endl;

    return true;
}

inline bool PLYExporter::exportPointsFromGPU(
    const std::string& filename,
    const float* d_x, const float* d_y, const float* d_z,
    const float* d_nx, const float* d_ny, const float* d_nz,
    uint32_t num_points,
    bool binary
) {
    // Allocate host memory and download
    std::vector<float> h_x(num_points), h_y(num_points), h_z(num_points);
    std::vector<float> h_nx, h_ny, h_nz;

    size_t bytes = num_points * sizeof(float);

    cudaMemcpy(h_x.data(), d_x, bytes, cudaMemcpyDeviceToHost);
    cudaMemcpy(h_y.data(), d_y, bytes, cudaMemcpyDeviceToHost);
    cudaMemcpy(h_z.data(), d_z, bytes, cudaMemcpyDeviceToHost);

    bool has_normals = (d_nx != nullptr && d_ny != nullptr && d_nz != nullptr);
    if (has_normals) {
        h_nx.resize(num_points);
        h_ny.resize(num_points);
        h_nz.resize(num_points);
        cudaMemcpy(h_nx.data(), d_nx, bytes, cudaMemcpyDeviceToHost);
        cudaMemcpy(h_ny.data(), d_ny, bytes, cudaMemcpyDeviceToHost);
        cudaMemcpy(h_nz.data(), d_nz, bytes, cudaMemcpyDeviceToHost);
    }

    return exportPointsFromHost(
        filename,
        h_x.data(), h_y.data(), h_z.data(),
        has_normals ? h_nx.data() : nullptr,
        has_normals ? h_ny.data() : nullptr,
        has_normals ? h_nz.data() : nullptr,
        num_points,
        binary
    );
}

inline bool PLYExporter::exportSourceCloud(
    const std::string& filename,
    const SourcePointCloud& cloud,
    bool binary
) {
    if (!cloud.on_device || cloud.num_points == 0) {
        std::cerr << "[PLYExporter] Invalid source cloud" << std::endl;
        return false;
    }

    return exportPointsFromGPU(
        filename,
        cloud.points_x, cloud.points_y, cloud.points_z,
        cloud.normals_x, cloud.normals_y, cloud.normals_z,
        cloud.num_points,
        binary
    );
}

inline bool PLYExporter::exportSourceCloudTransformed(
    const std::string& filename,
    const SourcePointCloud& cloud,
    const Matrix4x4& transform,
    bool binary
) {
    if (!cloud.on_device || cloud.num_points == 0) {
        std::cerr << "[PLYExporter] Invalid source cloud" << std::endl;
        return false;
    }

    uint32_t num_points = cloud.num_points;
    size_t bytes = num_points * sizeof(float);

    // Download from GPU
    std::vector<float> h_x(num_points), h_y(num_points), h_z(num_points);
    cudaMemcpy(h_x.data(), cloud.points_x, bytes, cudaMemcpyDeviceToHost);
    cudaMemcpy(h_y.data(), cloud.points_y, bytes, cudaMemcpyDeviceToHost);
    cudaMemcpy(h_z.data(), cloud.points_z, bytes, cudaMemcpyDeviceToHost);

    std::vector<float> h_nx, h_ny, h_nz;
    bool has_normals = (cloud.normals_x != nullptr);
    if (has_normals) {
        h_nx.resize(num_points);
        h_ny.resize(num_points);
        h_nz.resize(num_points);
        cudaMemcpy(h_nx.data(), cloud.normals_x, bytes, cudaMemcpyDeviceToHost);
        cudaMemcpy(h_ny.data(), cloud.normals_y, bytes, cudaMemcpyDeviceToHost);
        cudaMemcpy(h_nz.data(), cloud.normals_z, bytes, cudaMemcpyDeviceToHost);
    }

    // Apply transformation
    std::vector<float> t_x(num_points), t_y(num_points), t_z(num_points);
    std::vector<float> t_nx, t_ny, t_nz;
    if (has_normals) {
        t_nx.resize(num_points);
        t_ny.resize(num_points);
        t_nz.resize(num_points);
    }

    for (uint32_t i = 0; i < num_points; ++i) {
        float3_t p(h_x[i], h_y[i], h_z[i]);
        float3_t tp = transform.transformPoint(p);
        t_x[i] = tp.x;
        t_y[i] = tp.y;
        t_z[i] = tp.z;

        if (has_normals) {
            float3_t n(h_nx[i], h_ny[i], h_nz[i]);
            float3_t tn = transform.transformVector(n).normalized();
            t_nx[i] = tn.x;
            t_ny[i] = tn.y;
            t_nz[i] = tn.z;
        }
    }

    return exportPointsFromHost(
        filename,
        t_x.data(), t_y.data(), t_z.data(),
        has_normals ? t_nx.data() : nullptr,
        has_normals ? t_ny.data() : nullptr,
        has_normals ? t_nz.data() : nullptr,
        num_points,
        binary
    );
}

inline bool PLYExporter::exportMesh(
    const std::string& filename,
    const MeshSoA& mesh,
    bool binary
) {
    std::ofstream file;

    if (binary) {
        file.open(filename, std::ios::binary);
    } else {
        file.open(filename);
    }

    if (!file.is_open()) {
        std::cerr << "[PLYExporter] Failed to create file: " << filename << std::endl;
        return false;
    }

    bool has_normals = (mesh.normals_x != nullptr && mesh.normals_y != nullptr && mesh.normals_z != nullptr);

    std::cout << "[PLYExporter] Writing mesh " << filename << std::endl;
    std::cout << "  Vertices: " << mesh.num_vertices << std::endl;
    std::cout << "  Faces: " << mesh.num_faces << std::endl;
    std::cout << "  Normals: " << (has_normals ? "yes" : "no") << std::endl;
    std::cout << "  Format: " << (binary ? "binary_little_endian" : "ascii") << std::endl;

    // Write header
    file << "ply\n";
    file << "format " << (binary ? "binary_little_endian" : "ascii") << " 1.0\n";
    file << "element vertex " << mesh.num_vertices << "\n";
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";

    if (has_normals) {
        file << "property float nx\n";
        file << "property float ny\n";
        file << "property float nz\n";
    }

    file << "element face " << mesh.num_faces << "\n";
    file << "property list uchar int vertex_indices\n";
    file << "end_header\n";

    if (binary) {
        // Write vertex data (binary)
        for (uint32_t i = 0; i < mesh.num_vertices; ++i) {
            file.write(reinterpret_cast<const char*>(&mesh.vertices_x[i]), sizeof(float));
            file.write(reinterpret_cast<const char*>(&mesh.vertices_y[i]), sizeof(float));
            file.write(reinterpret_cast<const char*>(&mesh.vertices_z[i]), sizeof(float));

            if (has_normals) {
                file.write(reinterpret_cast<const char*>(&mesh.normals_x[i]), sizeof(float));
                file.write(reinterpret_cast<const char*>(&mesh.normals_y[i]), sizeof(float));
                file.write(reinterpret_cast<const char*>(&mesh.normals_z[i]), sizeof(float));
            }
        }

        // Write face data (binary)
        for (uint32_t i = 0; i < mesh.num_faces; ++i) {
            uint8_t count = 3;
            file.write(reinterpret_cast<const char*>(&count), sizeof(uint8_t));

            int32_t v0 = mesh.faces_v0[i];
            int32_t v1 = mesh.faces_v1[i];
            int32_t v2 = mesh.faces_v2[i];
            file.write(reinterpret_cast<const char*>(&v0), sizeof(int32_t));
            file.write(reinterpret_cast<const char*>(&v1), sizeof(int32_t));
            file.write(reinterpret_cast<const char*>(&v2), sizeof(int32_t));
        }
    } else {
        // Write vertex data (ASCII)
        for (uint32_t i = 0; i < mesh.num_vertices; ++i) {
            file << mesh.vertices_x[i] << " " << mesh.vertices_y[i] << " " << mesh.vertices_z[i];
            if (has_normals) {
                file << " " << mesh.normals_x[i] << " " << mesh.normals_y[i] << " " << mesh.normals_z[i];
            }
            file << "\n";
        }

        // Write face data (ASCII)
        for (uint32_t i = 0; i < mesh.num_faces; ++i) {
            file << "3 " << mesh.faces_v0[i] << " " << mesh.faces_v1[i] << " " << mesh.faces_v2[i] << "\n";
        }
    }

    file.close();
    std::cout << "[PLYExporter] Successfully saved " << filename << std::endl;

    return true;
}
