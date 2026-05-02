#include "ascend_backend_plugin.h"
#include "ascend_runtime_loader.h"
#include "ply_reader_cpu.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using DynamicLibHandle = mesh_gpu::ascend_runtime::DynamicLibHandle;

struct CliOptions {
    std::string mesh_path;
    int device_id = 0;
    bool strict = true;
    float cell_size = 2.0f;
    int max_iterations = 5;
    int source_limit = 0;  // 0 means "use all mesh vertices"
    float offset_x = 5.0f;
    float offset_y = -3.0f;
    float offset_z = 2.0f;
    float tolerance = 0.35f;
    std::string report_json;
    bool show_help = false;
    bool valid = true;
    std::string error_message;
};

bool parseInt(const std::string& text, int& value) {
    try {
        size_t used = 0;
        const int parsed = std::stoi(text, &used, 10);
        if (used != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseFloat(const std::string& text, float& value) {
    try {
        size_t used = 0;
        const float parsed = std::stof(text, &used);
        if (used != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

std::string buildUsage(const char* exe_name) {
    std::stringstream ss;
    ss << "Usage: " << exe_name << " --mesh <target.ply> [options]\n";
    ss << "Options:\n";
    ss << "  --device <id>           Ascend device id (default: 0)\n";
    ss << "  --strict                Strict backend init (default)\n";
    ss << "  --nonstrict             Allow fallback behavior in plugin\n";
    ss << "  --cell-size <float>     Cell size for target mesh load (default: 2.0)\n";
    ss << "  --max-iterations <int>  Registration max iterations (default: 5)\n";
    ss << "  --source-limit <int>    Number of source points sampled from mesh (0=all)\n";
    ss << "  --offset-x <float>      Synthetic source offset X (default: 5.0)\n";
    ss << "  --offset-y <float>      Synthetic source offset Y (default: -3.0)\n";
    ss << "  --offset-z <float>      Synthetic source offset Z (default: 2.0)\n";
    ss << "  --tolerance <float>     Translation tolerance for pass/fail (default: 0.35)\n";
    ss << "  --report-json <path>    Optional JSON report output path\n";
    ss << "  -h, --help              Show help\n";
    return ss.str();
}

CliOptions parseArgs(int argc, char** argv) {
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            options.show_help = true;
            continue;
        }

        auto require_value = [&](const std::string& key, std::string& out) -> bool {
            if (i + 1 >= argc) {
                options.valid = false;
                options.error_message = key + " requires a value.";
                return false;
            }
            out = argv[++i];
            return true;
        };

        if (arg == "--strict") {
            options.strict = true;
            continue;
        }
        if (arg == "--nonstrict") {
            options.strict = false;
            continue;
        }
        if (arg == "--mesh") {
            std::string value;
            if (!require_value(arg, value)) {
                return options;
            }
            options.mesh_path = value;
            continue;
        }
        if (arg == "--report-json") {
            std::string value;
            if (!require_value(arg, value)) {
                return options;
            }
            options.report_json = value;
            continue;
        }

        auto parse_int_flag = [&](int& target) -> bool {
            std::string value;
            if (!require_value(arg, value)) {
                return false;
            }
            if (!parseInt(value, target)) {
                options.valid = false;
                options.error_message = "Invalid integer for " + arg + ": " + value;
                return false;
            }
            return true;
        };
        auto parse_float_flag = [&](float& target) -> bool {
            std::string value;
            if (!require_value(arg, value)) {
                return false;
            }
            if (!parseFloat(value, target)) {
                options.valid = false;
                options.error_message = "Invalid float for " + arg + ": " + value;
                return false;
            }
            return true;
        };

        if (arg == "--device") {
            if (!parse_int_flag(options.device_id)) {
                return options;
            }
            continue;
        }
        if (arg == "--max-iterations") {
            if (!parse_int_flag(options.max_iterations)) {
                return options;
            }
            continue;
        }
        if (arg == "--source-limit") {
            if (!parse_int_flag(options.source_limit)) {
                return options;
            }
            continue;
        }
        if (arg == "--cell-size") {
            if (!parse_float_flag(options.cell_size)) {
                return options;
            }
            continue;
        }
        if (arg == "--offset-x") {
            if (!parse_float_flag(options.offset_x)) {
                return options;
            }
            continue;
        }
        if (arg == "--offset-y") {
            if (!parse_float_flag(options.offset_y)) {
                return options;
            }
            continue;
        }
        if (arg == "--offset-z") {
            if (!parse_float_flag(options.offset_z)) {
                return options;
            }
            continue;
        }
        if (arg == "--tolerance") {
            if (!parse_float_flag(options.tolerance)) {
                return options;
            }
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            options.valid = false;
            options.error_message = "Unknown option: " + arg;
            return options;
        }

        if (options.mesh_path.empty()) {
            options.mesh_path = arg;
        } else {
            options.valid = false;
            options.error_message = "Unexpected positional argument: " + arg;
            return options;
        }
    }

    if (!options.show_help && options.mesh_path.empty()) {
        options.valid = false;
        options.error_message = "Missing required --mesh <target.ply>.";
    }
    if (options.source_limit < 0) {
        options.valid = false;
        options.error_message = "--source-limit must be >= 0.";
    }
    if (options.max_iterations <= 0) {
        options.valid = false;
        options.error_message = "--max-iterations must be > 0.";
    }
    if (options.tolerance <= 0.0f) {
        options.valid = false;
        options.error_message = "--tolerance must be > 0.";
    }

    return options;
}

std::vector<std::string> getPluginCandidates() {
    std::vector<std::string> candidates;
    const char* env_path = std::getenv("MESHGPU_ASCEND_PLUGIN");
    if (env_path && env_path[0] != '\0') {
        candidates.emplace_back(env_path);
    }
#if defined(_WIN32)
    candidates.emplace_back("mesgpu_ascend_backend.dll");
    candidates.emplace_back("MeshGPUAscendBackend.dll");
#else
    candidates.emplace_back("libmesgpu_ascend_backend.so");
    candidates.emplace_back("libMeshGPUAscendBackend.so");
#endif
    return candidates;
}

bool loadBackendApi(DynamicLibHandle& out_handle,
                    const mesh_gpu::AscendBackendApiV1*& out_api,
                    std::string& out_path,
                    std::string& detail) {
    out_handle = nullptr;
    out_api = nullptr;
    out_path.clear();

    for (const auto& candidate : getPluginCandidates()) {
        DynamicLibHandle handle = mesh_gpu::ascend_runtime::openDynamicLibrary(candidate);
        if (!handle) {
            continue;
        }

        auto fn = reinterpret_cast<mesh_gpu::GetAscendBackendApiV1Fn>(
            mesh_gpu::ascend_runtime::getDynamicSymbol(handle, "MeshGPU_GetAscendBackendApiV1"));
        if (!fn) {
            mesh_gpu::ascend_runtime::closeDynamicLibrary(handle);
            continue;
        }

        const mesh_gpu::AscendBackendApiV1* api = fn();
        if (!api) {
            mesh_gpu::ascend_runtime::closeDynamicLibrary(handle);
            continue;
        }
        if (api->abi_version != mesh_gpu::kAscendBackendApiV1) {
            mesh_gpu::ascend_runtime::closeDynamicLibrary(handle);
            continue;
        }
        if (!api->create_context || !api->destroy_context || !api->load_target_mesh ||
            !api->set_source_point_cloud || !api->run_registration) {
            mesh_gpu::ascend_runtime::closeDynamicLibrary(handle);
            continue;
        }

        out_handle = handle;
        out_api = api;
        out_path = candidate;
        detail = "Loaded Ascend backend plugin: " + candidate;
        return true;
    }

    detail = "Failed to load Ascend backend plugin. Set MESHGPU_ASCEND_PLUGIN to plugin path.";
    return false;
}

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

bool buildSyntheticSourcePoints(const std::string& mesh_path,
                                int source_limit,
                                float dx,
                                float dy,
                                float dz,
                                std::vector<mesh_gpu::Point3D>& points,
                                std::string& detail) {
    MeshSoA mesh{};
    if (!PLYReader::readPLY(mesh_path, mesh)) {
        detail = "Failed to read mesh for source point generation: " + mesh_path;
        return false;
    }

    if (mesh.num_vertices == 0 || !mesh.vertices_x || !mesh.vertices_y || !mesh.vertices_z) {
        freeCpuMesh(mesh);
        detail = "Mesh has no usable vertices.";
        return false;
    }

    const int total_vertices = static_cast<int>(mesh.num_vertices);
    const int count = (source_limit > 0) ? std::min(source_limit, total_vertices) : total_vertices;
    points.clear();
    points.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        const std::uint64_t scaled = static_cast<std::uint64_t>(i) * static_cast<std::uint64_t>(total_vertices);
        const int index = static_cast<int>(scaled / static_cast<std::uint64_t>(count));
        const int clamped = std::min(std::max(index, 0), total_vertices - 1);
        points.emplace_back(
            mesh.vertices_x[clamped] + dx,
            mesh.vertices_y[clamped] + dy,
            mesh.vertices_z[clamped] + dz);
    }

    freeCpuMesh(mesh);
    detail = "Generated synthetic source points: " + std::to_string(points.size());
    return !points.empty();
}

std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16u);
    for (char c : input) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

void writeReportJson(const std::string& path,
                     bool pass,
                     const std::string& plugin_path,
                     int source_count,
                     float tx,
                     float ty,
                     float tz,
                     float expected_tx,
                     float expected_ty,
                     float expected_tz,
                     float max_error,
                     float rmse,
                     int iterations,
                     const std::string& message) {
    if (path.empty()) {
        return;
    }
    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        return;
    }

    ofs << "{\n";
    ofs << "  \"pass\": " << (pass ? "true" : "false") << ",\n";
    ofs << "  \"plugin_path\": \"" << jsonEscape(plugin_path) << "\",\n";
    ofs << "  \"source_count\": " << source_count << ",\n";
    ofs << std::fixed << std::setprecision(6);
    ofs << "  \"translation\": [" << tx << ", " << ty << ", " << tz << "],\n";
    ofs << "  \"expected_translation\": [" << expected_tx << ", " << expected_ty << ", " << expected_tz << "],\n";
    ofs << "  \"max_translation_error\": " << max_error << ",\n";
    ofs << "  \"rmse\": " << rmse << ",\n";
    ofs << "  \"iterations\": " << iterations << ",\n";
    ofs << "  \"message\": \"" << jsonEscape(message) << "\"\n";
    ofs << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
    const CliOptions options = parseArgs(argc, argv);
    if (!options.valid) {
        std::cerr << "[Args] " << options.error_message << "\n" << buildUsage(argv[0]) << std::endl;
        return 2;
    }
    if (options.show_help) {
        std::cout << buildUsage(argv[0]) << std::endl;
        return 0;
    }

    DynamicLibHandle plugin_handle = nullptr;
    const mesh_gpu::AscendBackendApiV1* api = nullptr;
    std::string plugin_path;
    std::string load_detail;
    if (!loadBackendApi(plugin_handle, api, plugin_path, load_detail)) {
        std::cerr << "[Plugin] " << load_detail << std::endl;
        return 1;
    }
    std::cout << "[Plugin] " << load_detail << std::endl;

    void* context = nullptr;
    const char* msg = nullptr;
    if (!api->create_context(options.device_id, options.strict, &context, &msg) || !context) {
        std::cerr << "[Context] create_context failed: " << (msg ? msg : "<no message>") << std::endl;
        mesh_gpu::ascend_runtime::closeDynamicLibrary(plugin_handle);
        return 1;
    }
    std::cout << "[Context] " << (msg ? msg : "created") << std::endl;

    auto cleanup = [&]() {
        if (api && api->destroy_context && context) {
            api->destroy_context(context);
            context = nullptr;
        }
        if (plugin_handle) {
            mesh_gpu::ascend_runtime::closeDynamicLibrary(plugin_handle);
            plugin_handle = nullptr;
        }
    };

    msg = nullptr;
    if (!api->load_target_mesh(context, options.mesh_path.c_str(), options.cell_size, &msg)) {
        std::cerr << "[Target] load_target_mesh failed: " << (msg ? msg : "<no message>") << std::endl;
        cleanup();
        return 1;
    }
    std::cout << "[Target] " << (msg ? msg : "loaded") << std::endl;

    std::vector<mesh_gpu::Point3D> source_points;
    std::string source_detail;
    if (!buildSyntheticSourcePoints(options.mesh_path,
                                    options.source_limit,
                                    options.offset_x,
                                    options.offset_y,
                                    options.offset_z,
                                    source_points,
                                    source_detail)) {
        std::cerr << "[Source] " << source_detail << std::endl;
        cleanup();
        return 1;
    }
    std::cout << "[Source] " << source_detail << std::endl;

    msg = nullptr;
    if (!api->set_source_point_cloud(
            context, source_points.data(), static_cast<int>(source_points.size()), &msg)) {
        std::cerr << "[Source] set_source_point_cloud failed: " << (msg ? msg : "<no message>") << std::endl;
        cleanup();
        return 1;
    }
    std::cout << "[Source] " << (msg ? msg : "set") << std::endl;

    mesh_gpu::RegistrationParams params;
    params.max_iterations = options.max_iterations;

    mesh_gpu::AscendRegistrationResultView result{};
    const mesh_gpu::Transform4x4 identity;
    msg = nullptr;
    if (!api->run_registration(context, &identity, &params, &result, &msg)) {
        std::cerr << "[Run] run_registration failed: " << (msg ? msg : "<no message>") << std::endl;
        cleanup();
        return 1;
    }

    const float tx = result.transform(0, 3);
    const float ty = result.transform(1, 3);
    const float tz = result.transform(2, 3);
    const float expected_tx = -options.offset_x;
    const float expected_ty = -options.offset_y;
    const float expected_tz = -options.offset_z;
    const float err_x = std::fabs(tx - expected_tx);
    const float err_y = std::fabs(ty - expected_ty);
    const float err_z = std::fabs(tz - expected_tz);
    const float max_error = std::max(err_x, std::max(err_y, err_z));
    const bool pass = (max_error <= options.tolerance);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "[Result] translation=(" << tx << ", " << ty << ", " << tz << ")\n";
    std::cout << "[Result] expected=(" << expected_tx << ", " << expected_ty << ", " << expected_tz << ")\n";
    std::cout << "[Result] max_error=" << max_error << ", tolerance=" << options.tolerance << "\n";
    std::cout << "[Result] rmse=" << result.rmse << ", iterations=" << result.iterations
              << ", converged=" << (result.converged ? "yes" : "no") << "\n";
    std::cout << "[Result] message=" << (msg ? msg : "<no message>") << "\n";

    writeReportJson(options.report_json,
                    pass,
                    plugin_path,
                    static_cast<int>(source_points.size()),
                    tx,
                    ty,
                    tz,
                    expected_tx,
                    expected_ty,
                    expected_tz,
                    max_error,
                    result.rmse,
                    result.iterations,
                    msg ? std::string(msg) : std::string());

    cleanup();
    return pass ? 0 : 1;
}

