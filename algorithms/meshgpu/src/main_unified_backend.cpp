#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "backend_runtime_cli.h"
#include "mesh_gpu_interface.h"
#include "ply_reader_cpu.h"

namespace {

void freeCpuMesh(MeshSoA& mesh) {
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
    delete[] mesh.vertex_face_offset;
    delete[] mesh.vertex_face_indices;
    delete[] mesh.colors_r;
    delete[] mesh.colors_g;
    delete[] mesh.colors_b;
    delete[] mesh.colors_a;
    mesh = MeshSoA{};
}

void printTransform(const mesh_gpu::Transform4x4& t) {
    for (int r = 0; r < 4; ++r) {
        std::cout << "  [";
        for (int c = 0; c < 4; ++c) {
            std::cout << t(r, c);
            if (c < 3) {
                std::cout << ", ";
            }
        }
        std::cout << "]\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    mesh_gpu::BackendCliOptions cli = mesh_gpu::parseBackendCliOptions(argc, argv);
    if (!cli.valid) {
        std::cerr << "[Args] " << cli.error_message << "\n";
        std::cerr << mesh_gpu::buildBackendCliUsage(argv[0], "[input_mesh.ply]") << "\n";
        return 2;
    }
    if (cli.show_help) {
        std::cout << mesh_gpu::buildBackendCliUsage(argv[0], "[input_mesh.ply]") << "\n";
        return 0;
    }
    if (cli.positional_args.size() > 1) {
        std::cerr << "[Args] Too many positional arguments.\n";
        std::cerr << mesh_gpu::buildBackendCliUsage(argv[0], "[input_mesh.ply]") << "\n";
        return 2;
    }

    std::string mesh_path = "E:/ICPtry/fixed_tibia_normalized.ply";
    if (!cli.positional_args.empty()) {
        mesh_path = cli.positional_args[0];
    }

    mesh_gpu::MeshGPUInterface iface;
    iface.setBackendConfig(cli.backend_config);
    std::cout << "[Backend] " << iface.getBackendInfo() << "\n";

    if (!iface.loadTargetMesh(mesh_path, 2.0f)) {
        std::cerr << "[Unified] loadTargetMesh failed.\n";
        return 1;
    }

    MeshSoA host_mesh{};
    if (!PLYReader::readPLY(mesh_path, host_mesh)) {
        std::cerr << "[Unified] failed to read mesh for source generation.\n";
        return 1;
    }

    const uint32_t stride = std::max<uint32_t>(1, host_mesh.num_vertices / 8000);
    std::vector<mesh_gpu::Point3D> source_points;
    source_points.reserve(host_mesh.num_vertices / stride + 1);
    for (uint32_t i = 0; i < host_mesh.num_vertices; i += stride) {
        source_points.emplace_back(
            host_mesh.vertices_x[i] + 5.0f,
            host_mesh.vertices_y[i] - 3.0f,
            host_mesh.vertices_z[i] + 2.0f);
    }
    freeCpuMesh(host_mesh);

    if (!iface.setSourcePointCloud(source_points)) {
        std::cerr << "[Unified] setSourcePointCloud failed.\n";
        return 1;
    }

    mesh_gpu::RegistrationParams params;
    params.max_iterations = 40;
    params.verbose = false;
    params.distance_threshold = 20.0f;

    const auto result = iface.runRegistration(params);

    std::cout << "[Unified] Registration result:\n";
    std::cout << "  rmse=" << result.rmse << "\n";
    std::cout << "  iterations=" << result.iterations << "\n";
    std::cout << "  converged=" << (result.converged ? "yes" : "no") << "\n";
    std::cout << "  transform=\n";
    printTransform(result.transform);

    return result.converged ? 0 : 1;
}
