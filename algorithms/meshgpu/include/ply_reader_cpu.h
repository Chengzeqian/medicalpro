#pragma once

#include "types_cpu.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

// ============================================================================
// PLY File Reader - CPU Only Version (Supports ASCII and Binary Little Endian)
// ============================================================================

class PLYReader {
public:
    static bool readPLY(const std::string& filename, MeshSoA& mesh);
    static bool writePLY(const std::string& filename, const MeshSoA& mesh, bool binary = true);

private:
    enum class Format {
        ASCII,
        BINARY_LITTLE_ENDIAN,
        BINARY_BIG_ENDIAN,
        UNKNOWN
    };

    struct PropertyInfo {
        std::string name;
        std::string type;
        int offset;
        int size;
    };

    static int getTypeSize(const std::string& type);
};

// ============================================================================
// Implementation
// ============================================================================

inline bool PLYReader::readPLY(const std::string& filename, MeshSoA& mesh) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[PLYReader] Failed to open file: " << filename << std::endl;
        return false;
    }

    std::string line;
    Format format = Format::UNKNOWN;
    uint32_t num_vertices = 0;
    uint32_t num_faces = 0;

    std::vector<PropertyInfo> vertex_properties;
    bool in_vertex_element = false;

    // Parse header
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "format") {
            std::string fmt;
            iss >> fmt;
            if (fmt == "ascii") {
                format = Format::ASCII;
            } else if (fmt == "binary_little_endian") {
                format = Format::BINARY_LITTLE_ENDIAN;
            } else if (fmt == "binary_big_endian") {
                format = Format::BINARY_BIG_ENDIAN;
            }
        }
        else if (token == "element") {
            std::string element_type;
            uint32_t count;
            iss >> element_type >> count;

            if (element_type == "vertex") {
                num_vertices = count;
                in_vertex_element = true;
            } else if (element_type == "face") {
                num_faces = count;
                in_vertex_element = false;
            } else {
                in_vertex_element = false;
            }
        }
        else if (token == "property") {
            if (in_vertex_element) {
                std::string type, name;
                iss >> type >> name;
                PropertyInfo prop;
                prop.name = name;
                prop.type = type;
                prop.size = getTypeSize(type);
                vertex_properties.push_back(prop);
            }
        }
        else if (token == "end_header") {
            break;
        }
    }

    if (format == Format::UNKNOWN) {
        std::cerr << "[PLYReader] Unknown PLY format" << std::endl;
        return false;
    }

    std::cout << "[PLYReader] Format: " << (format == Format::ASCII ? "ASCII" : "Binary") << std::endl;
    std::cout << "[PLYReader] Vertices: " << num_vertices << ", Faces: " << num_faces << std::endl;

    if (num_vertices > MAX_VERTICES || num_faces > MAX_FACES) {
        std::cerr << "[PLYReader] Mesh too large! Max vertices: " << MAX_VERTICES
                  << ", Max faces: " << MAX_FACES << std::endl;
        return false;
    }

    // Allocate Host memory
    mesh.vertices_x = new float[num_vertices];
    mesh.vertices_y = new float[num_vertices];
    mesh.vertices_z = new float[num_vertices];
    mesh.normals_x = new float[num_vertices]();
    mesh.normals_y = new float[num_vertices]();
    mesh.normals_z = new float[num_vertices]();
    mesh.curvature = new float[num_vertices]();
    mesh.gaussian_curv = new float[num_vertices]();

    mesh.faces_v0 = new uint32_t[num_faces];
    mesh.faces_v1 = new uint32_t[num_faces];
    mesh.faces_v2 = new uint32_t[num_faces];
    mesh.face_normals_x = new float[num_faces]();
    mesh.face_normals_y = new float[num_faces]();
    mesh.face_normals_z = new float[num_faces]();
    mesh.face_areas = new float[num_faces]();

    mesh.num_vertices = num_vertices;
    mesh.num_faces = num_faces;
    mesh.on_device = false;

    // Find x, y, z, nx, ny, nz, red, green, blue, alpha property indices
    int x_idx = -1, y_idx = -1, z_idx = -1;
    int nx_idx = -1, ny_idx = -1, nz_idx = -1;
    int r_idx = -1, g_idx = -1, b_idx = -1, a_idx = -1;
    for (size_t i = 0; i < vertex_properties.size(); ++i) {
        if (vertex_properties[i].name == "x") x_idx = (int)i;
        else if (vertex_properties[i].name == "y") y_idx = (int)i;
        else if (vertex_properties[i].name == "z") z_idx = (int)i;
        else if (vertex_properties[i].name == "nx") nx_idx = (int)i;
        else if (vertex_properties[i].name == "ny") ny_idx = (int)i;
        else if (vertex_properties[i].name == "nz") nz_idx = (int)i;
        else if (vertex_properties[i].name == "red") r_idx = (int)i;
        else if (vertex_properties[i].name == "green") g_idx = (int)i;
        else if (vertex_properties[i].name == "blue") b_idx = (int)i;
        else if (vertex_properties[i].name == "alpha") a_idx = (int)i;
    }

    if (x_idx < 0 || y_idx < 0 || z_idx < 0) {
        std::cerr << "[PLYReader] Missing x, y, z properties" << std::endl;
        return false;
    }

    // Check if we have colors
    bool has_colors = (r_idx >= 0 && g_idx >= 0 && b_idx >= 0);
    if (has_colors) {
        mesh.colors_r = new uint8_t[num_vertices];
        mesh.colors_g = new uint8_t[num_vertices];
        mesh.colors_b = new uint8_t[num_vertices];
        mesh.colors_a = new uint8_t[num_vertices];
        // Initialize alpha to 255 if not present
        for (uint32_t i = 0; i < num_vertices; ++i) mesh.colors_a[i] = 255;
        std::cout << "[PLYReader] Found vertex colors (RGBA)" << std::endl;
    } else {
        mesh.colors_r = nullptr;
        mesh.colors_g = nullptr;
        mesh.colors_b = nullptr;
        mesh.colors_a = nullptr;
    }
    mesh.has_colors = has_colors;

    bool has_normals_in_file = (nx_idx >= 0 && ny_idx >= 0 && nz_idx >= 0);
    if (has_normals_in_file) {
        std::cout << "[PLYReader] Found vertex normals" << std::endl;
    }

    // Read vertex data
    if (format == Format::ASCII) {
        for (uint32_t i = 0; i < num_vertices; ++i) {
            std::getline(file, line);
            std::istringstream iss(line);

            std::vector<float> values;
            float val;
            while (iss >> val) {
                values.push_back(val);
            }

            if ((int)values.size() > x_idx) mesh.vertices_x[i] = values[x_idx];
            if ((int)values.size() > y_idx) mesh.vertices_y[i] = values[y_idx];
            if ((int)values.size() > z_idx) mesh.vertices_z[i] = values[z_idx];
            if (has_normals_in_file) {
                if ((int)values.size() > nx_idx) mesh.normals_x[i] = values[nx_idx];
                if ((int)values.size() > ny_idx) mesh.normals_y[i] = values[ny_idx];
                if ((int)values.size() > nz_idx) mesh.normals_z[i] = values[nz_idx];
            }
            if (has_colors) {
                if ((int)values.size() > r_idx) mesh.colors_r[i] = (uint8_t)values[r_idx];
                if ((int)values.size() > g_idx) mesh.colors_g[i] = (uint8_t)values[g_idx];
                if ((int)values.size() > b_idx) mesh.colors_b[i] = (uint8_t)values[b_idx];
                if (a_idx >= 0 && (int)values.size() > a_idx) mesh.colors_a[i] = (uint8_t)values[a_idx];
            }
        }

        // Read face data
        for (uint32_t i = 0; i < num_faces; ++i) {
            std::getline(file, line);
            std::istringstream iss(line);

            int count;
            iss >> count;

            if (count >= 3) {
                uint32_t v0, v1, v2;
                iss >> v0 >> v1 >> v2;
                mesh.faces_v0[i] = v0;
                mesh.faces_v1[i] = v1;
                mesh.faces_v2[i] = v2;
            }
        }
    }
    else if (format == Format::BINARY_LITTLE_ENDIAN) {
        int vertex_size = 0;
        for (const auto& prop : vertex_properties) {
            vertex_size += prop.size;
        }

        std::vector<char> vertex_buffer(vertex_size);
        for (uint32_t i = 0; i < num_vertices; ++i) {
            file.read(vertex_buffer.data(), vertex_size);

            int offset = 0;
            for (size_t j = 0; j < vertex_properties.size(); ++j) {
                if ((int)j == x_idx) {
                    memcpy(&mesh.vertices_x[i], vertex_buffer.data() + offset, sizeof(float));
                } else if ((int)j == y_idx) {
                    memcpy(&mesh.vertices_y[i], vertex_buffer.data() + offset, sizeof(float));
                } else if ((int)j == z_idx) {
                    memcpy(&mesh.vertices_z[i], vertex_buffer.data() + offset, sizeof(float));
                } else if ((int)j == nx_idx) {
                    memcpy(&mesh.normals_x[i], vertex_buffer.data() + offset, sizeof(float));
                } else if ((int)j == ny_idx) {
                    memcpy(&mesh.normals_y[i], vertex_buffer.data() + offset, sizeof(float));
                } else if ((int)j == nz_idx) {
                    memcpy(&mesh.normals_z[i], vertex_buffer.data() + offset, sizeof(float));
                } else if ((int)j == r_idx && has_colors) {
                    mesh.colors_r[i] = *(uint8_t*)(vertex_buffer.data() + offset);
                } else if ((int)j == g_idx && has_colors) {
                    mesh.colors_g[i] = *(uint8_t*)(vertex_buffer.data() + offset);
                } else if ((int)j == b_idx && has_colors) {
                    mesh.colors_b[i] = *(uint8_t*)(vertex_buffer.data() + offset);
                } else if ((int)j == a_idx && has_colors) {
                    mesh.colors_a[i] = *(uint8_t*)(vertex_buffer.data() + offset);
                }
                offset += vertex_properties[j].size;
            }
        }

        for (uint32_t i = 0; i < num_faces; ++i) {
            uint8_t count;
            file.read(reinterpret_cast<char*>(&count), 1);

            if (count >= 3) {
                int32_t v0, v1, v2;
                file.read(reinterpret_cast<char*>(&v0), 4);
                file.read(reinterpret_cast<char*>(&v1), 4);
                file.read(reinterpret_cast<char*>(&v2), 4);
                mesh.faces_v0[i] = v0;
                mesh.faces_v1[i] = v1;
                mesh.faces_v2[i] = v2;

                for (int j = 3; j < count; ++j) {
                    int32_t dummy;
                    file.read(reinterpret_cast<char*>(&dummy), 4);
                }
            }
        }
    }

    file.close();
    std::cout << "[PLYReader] Successfully loaded " << filename << std::endl;

    return true;
}

inline int PLYReader::getTypeSize(const std::string& type) {
    if (type == "float" || type == "float32") return 4;
    if (type == "double" || type == "float64") return 8;
    if (type == "int" || type == "int32") return 4;
    if (type == "uint" || type == "uint32") return 4;
    if (type == "short" || type == "int16") return 2;
    if (type == "ushort" || type == "uint16") return 2;
    if (type == "char" || type == "int8") return 1;
    if (type == "uchar" || type == "uint8") return 1;
    return 4;
}

// ============================================================================
// PLY Writer Implementation
// ============================================================================
inline bool PLYReader::writePLY(const std::string& filename, const MeshSoA& mesh, bool binary) {
    std::ofstream file;

    if (binary) {
        file.open(filename, std::ios::binary);
    } else {
        file.open(filename);
    }

    if (!file.is_open()) {
        std::cerr << "[PLYWriter] Failed to create file: " << filename << std::endl;
        return false;
    }

    // Check if we have normals and colors
    bool has_normals = (mesh.normals_x != nullptr && mesh.normals_y != nullptr && mesh.normals_z != nullptr);
    bool has_colors = mesh.has_colors && mesh.colors_r != nullptr;

    std::cout << "[PLYWriter] Writing " << filename << std::endl;
    std::cout << "  Vertices: " << mesh.num_vertices << std::endl;
    std::cout << "  Faces: " << mesh.num_faces << std::endl;
    std::cout << "  Format: " << (binary ? "binary_little_endian" : "ascii") << std::endl;
    std::cout << "  Has normals: " << (has_normals ? "YES" : "NO") << std::endl;
    std::cout << "  Has colors: " << (has_colors ? "YES" : "NO") << std::endl;

    // Write header
    file << "ply\n";
    file << "format " << (binary ? "binary_little_endian" : "ascii") << " 1.0\n";
    file << "comment Normalized by MeshGPU normalize_mesh tool\n";
    file << "element vertex " << mesh.num_vertices << "\n";
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";

    if (has_normals) {
        file << "property float nx\n";
        file << "property float ny\n";
        file << "property float nz\n";
    }

    if (has_colors) {
        file << "property uchar red\n";
        file << "property uchar green\n";
        file << "property uchar blue\n";
        file << "property uchar alpha\n";
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

            if (has_colors) {
                file.write(reinterpret_cast<const char*>(&mesh.colors_r[i]), sizeof(uint8_t));
                file.write(reinterpret_cast<const char*>(&mesh.colors_g[i]), sizeof(uint8_t));
                file.write(reinterpret_cast<const char*>(&mesh.colors_b[i]), sizeof(uint8_t));
                file.write(reinterpret_cast<const char*>(&mesh.colors_a[i]), sizeof(uint8_t));
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
            if (has_colors) {
                file << " " << (int)mesh.colors_r[i] << " " << (int)mesh.colors_g[i]
                     << " " << (int)mesh.colors_b[i] << " " << (int)mesh.colors_a[i];
            }
            file << "\n";
        }

        // Write face data (ASCII)
        for (uint32_t i = 0; i < mesh.num_faces; ++i) {
            file << "3 " << mesh.faces_v0[i] << " " << mesh.faces_v1[i] << " " << mesh.faces_v2[i] << "\n";
        }
    }

    file.close();
    std::cout << "[PLYWriter] Successfully saved " << filename << std::endl;

    return true;
}
