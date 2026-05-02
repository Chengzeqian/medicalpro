#pragma once

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "mesh_gpu_interface.h"

namespace mesh_gpu {

struct BackendCliOptions {
    BackendConfig backend_config;
    std::vector<std::string> positional_args;
    bool show_help = false;
    bool valid = true;
    std::string error_message;
};

inline std::string toLowerCopy(const std::string& input) {
    std::string value = input;
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

inline bool parseIntValue(const std::string& text, int& value) {
    try {
        size_t used = 0;
        int parsed = std::stoi(text, &used, 10);
        if (used != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

inline bool parseBackendKindText(const std::string& text, ComputeBackendKind& kind) {
    const std::string lower = toLowerCopy(text);
    if (lower == "auto") {
        kind = ComputeBackendKind::AUTO;
        return true;
    }
    if (lower == "cuda") {
        kind = ComputeBackendKind::CUDA;
        return true;
    }
    if (lower == "ascend") {
        kind = ComputeBackendKind::ASCEND;
        return true;
    }
    if (lower == "cpu") {
        kind = ComputeBackendKind::CPU;
        return true;
    }
    return false;
}

inline BackendCliOptions parseBackendCliOptions(int argc, char** argv) {
    BackendCliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            options.show_help = true;
            continue;
        }

        if (arg == "--backend-strict") {
            options.backend_config.strict = true;
            continue;
        }

        if (arg == "--backend-nonstrict") {
            options.backend_config.strict = false;
            continue;
        }

        if (arg.rfind("--backend=", 0) == 0) {
            const std::string value = arg.substr(std::string("--backend=").size());
            if (!parseBackendKindText(value, options.backend_config.requested)) {
                options.valid = false;
                options.error_message = "Invalid backend value: " + value;
                return options;
            }
            continue;
        }

        if (arg == "--backend") {
            if (i + 1 >= argc) {
                options.valid = false;
                options.error_message = "--backend requires a value";
                return options;
            }
            const std::string value = argv[++i];
            if (!parseBackendKindText(value, options.backend_config.requested)) {
                options.valid = false;
                options.error_message = "Invalid backend value: " + value;
                return options;
            }
            continue;
        }

        if (arg.rfind("--device=", 0) == 0) {
            const std::string value = arg.substr(std::string("--device=").size());
            int parsed = 0;
            if (!parseIntValue(value, parsed)) {
                options.valid = false;
                options.error_message = "Invalid device id: " + value;
                return options;
            }
            options.backend_config.device_id = parsed;
            continue;
        }

        if (arg == "--device") {
            if (i + 1 >= argc) {
                options.valid = false;
                options.error_message = "--device requires a value";
                return options;
            }
            const std::string value = argv[++i];
            int parsed = 0;
            if (!parseIntValue(value, parsed)) {
                options.valid = false;
                options.error_message = "Invalid device id: " + value;
                return options;
            }
            options.backend_config.device_id = parsed;
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            options.valid = false;
            options.error_message = "Unknown option: " + arg;
            return options;
        }

        options.positional_args.push_back(arg);
    }

    return options;
}

inline std::string buildBackendCliUsage(const char* exe_name, const char* positional_usage) {
    std::stringstream ss;
    ss << "Usage: " << exe_name << " " << positional_usage << " [options]\n";
    ss << "Options:\n";
    ss << "  --backend <auto|cuda|ascend|cpu>   Select backend (default: auto)\n";
    ss << "  --device <id>                       Backend device id (default: 0)\n";
    ss << "  --backend-strict                    Disable fallback\n";
    ss << "  --backend-nonstrict                 Enable fallback (default)\n";
    ss << "  -h, --help                          Show this help";
    return ss.str();
}

inline bool configureBackendForCudaOnlyExecutable(const BackendCliOptions& options,
                                                  const char* executable_name) {
    MeshGPUInterface probe;
    probe.setBackendConfig(options.backend_config);

    const BackendStatus status = probe.getBackendStatus();
    std::cout << "[Backend] " << probe.getBackendInfo() << std::endl;

    if (status.selected == ComputeBackendKind::CUDA && status.initialized) {
        return true;
    }

    std::cerr << "[" << executable_name << "] This executable currently supports CUDA kernels only."
              << " Resolved backend=" << MeshGPUInterface::backendKindToString(status.selected)
              << ", initialized=" << (status.initialized ? "yes" : "no") << std::endl;
    return false;
}

} // namespace mesh_gpu
