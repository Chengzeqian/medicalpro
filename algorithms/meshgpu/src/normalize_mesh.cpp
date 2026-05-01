// ============================================================================
// Mesh Normalizer Tool
// PCA-based mesh alignment - aligns principal axis to Z
// Usage: normalize_mesh input.ply [output.ply]
// ============================================================================

#include <iostream>
#include <string>
#include "types_cpu.h"
#include "ply_reader_cpu.h"
#include "mesh_normalizer_cpu.h"

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
    // Free color arrays if allocated
    if (mesh.colors_r) delete[] mesh.colors_r;
    if (mesh.colors_g) delete[] mesh.colors_g;
    if (mesh.colors_b) delete[] mesh.colors_b;
    if (mesh.colors_a) delete[] mesh.colors_a;
}

int main(int argc, char** argv) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Mesh Normalizer - PCA Alignment Tool" << std::endl;
    std::cout << "========================================" << std::endl;

    if (argc < 2) {
        std::cout << "\nUsage: " << argv[0] << " input.ply [output.ply]" << std::endl;
        std::cout << "\nThis tool performs PCA-based mesh normalization:" << std::endl;
        std::cout << "  1. Centers mesh at origin" << std::endl;
        std::cout << "  2. Aligns principal axis (longest) to Z axis" << std::endl;
        std::cout << "  3. Ensures correct orientation (wider end up for bones)" << std::endl;
        std::cout << "  4. Fixes handedness (prevents mirroring)" << std::endl;
        std::cout << "\nIf output path is not specified, saves to <input>_normalized.ply" << std::endl;
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path;

    if (argc >= 3) {
        output_path = argv[2];
    } else {
        // Generate output filename
        size_t dot_pos = input_path.rfind('.');
        if (dot_pos != std::string::npos) {
            output_path = input_path.substr(0, dot_pos) + "_normalized.ply";
        } else {
            output_path = input_path + "_normalized.ply";
        }
    }

    std::cout << "\nInput:  " << input_path << std::endl;
    std::cout << "Output: " << output_path << std::endl;

    // Load mesh
    MeshSoA mesh;
    if (!PLYReader::readPLY(input_path, mesh)) {
        std::cerr << "Failed to load input mesh!" << std::endl;
        return 1;
    }

    // Check if normalization is needed
    bool needs_norm = MeshNormalizer::needsNormalization(mesh);
    if (!needs_norm) {
        std::cout << "\n[INFO] Mesh appears to be already aligned (Z is longest axis)." << std::endl;
        std::cout << "       Proceeding with normalization anyway for consistency." << std::endl;
    }

    // Normalize
    MeshNormalizer::NormalizationResult result = MeshNormalizer::normalize(mesh);

    // Save result
    if (!PLYReader::writePLY(output_path, mesh, true)) {
        std::cerr << "Failed to save output mesh!" << std::endl;
        freeMeshHost(mesh);
        return 1;
    }

    // Summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "Normalization Complete!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Eigenvalue ratio: " << result.eigenvalue_ratio << std::endl;
    std::cout << "  Z flipped: " << (result.flipped_z ? "Yes" : "No") << std::endl;
    std::cout << "  Mirrored: " << (result.mirrored ? "Yes" : "No") << std::endl;
    std::cout << "  Output: " << output_path << std::endl;
    std::cout << "========================================\n" << std::endl;

    freeMeshHost(mesh);
    return 0;
}
