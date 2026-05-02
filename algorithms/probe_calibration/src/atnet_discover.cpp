#include <atnetInterface.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string config_file;
    uint32_t timeout_sec = 30;
    uint32_t max_devices = 0;
    bool set_ip = false;
    std::string serial;
    std::string ip;
    uint16_t port = 3509;
};

struct OutputCollector {
    std::vector<std::string> lines;
};

void printUsage() {
    std::cout << "atnet_discover\n"
              << "Usage:\n"
              << "  atnet_discover [--config <json>] [--timeout <sec>] [--max <n>]\n"
              << "  atnet_discover [--config <json>] --set-ip <serial> <ip> [port]\n\n"
              << "Examples:\n"
              << "  atnet_discover --config ftk_config.json --timeout 20\n"
              << "  atnet_discover --set-ip 13042424579613315880 192.168.1.10 3509\n";
}

enum class ParseResult { Ok, Help, Invalid };

ParseResult parseArgs(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return ParseResult::Help;
        }
        if (arg == "--config") {
            if (i + 1 >= argc) return ParseResult::Invalid;
            opt.config_file = argv[++i];
            continue;
        }
        if (arg == "--timeout") {
            if (i + 1 >= argc) return ParseResult::Invalid;
            opt.timeout_sec = static_cast<uint32_t>(std::stoul(argv[++i]));
            continue;
        }
        if (arg == "--max") {
            if (i + 1 >= argc) return ParseResult::Invalid;
            opt.max_devices = static_cast<uint32_t>(std::stoul(argv[++i]));
            continue;
        }
        if (arg == "--set-ip") {
            if (i + 2 >= argc) return ParseResult::Invalid;
            opt.set_ip = true;
            opt.serial = argv[++i];
            opt.ip = argv[++i];
            if (i + 1 < argc) {
                const std::string maybe_port = argv[i + 1];
                if (!maybe_port.empty() && maybe_port[0] != '-') {
                    opt.port = static_cast<uint16_t>(std::stoul(argv[++i]));
                }
            }
            continue;
        }
        return ParseResult::Invalid;
    }
    return ParseResult::Ok;
}

void outputCallback(const char* message, void* user) {
    if (!message) return;
    std::cout << message << std::endl;
    if (user) {
        auto* collector = static_cast<OutputCollector*>(user);
        collector->lines.emplace_back(message);
    }
}

void progressCallback(const char* message, int old_value, int new_value, void*) {
    if (!message) return;
    std::cout << "[progress] " << message << " " << old_value << " -> " << new_value << std::endl;
}

bool executeCommand(atnetLib lib,
                    const std::string& cmd,
                    std::array<char, 1024>& err_buf,
                    OutputCollector* collector = nullptr) {
    std::cout << ">>> " << cmd << std::endl;
    const atnetError st = atnetExecuteCommand(
        lib, cmd.c_str(), outputCallback, collector, progressCallback, nullptr);
    if (st == atnetError::Ok) {
        return true;
    }

    if (atnetGetError(lib, err_buf.size(), err_buf.data()) == atnetError::Ok) {
        std::cerr << "[atnet] ERROR: " << err_buf.data() << std::endl;
    } else {
        std::cerr << "[atnet] ERROR: command failed, and no extra error message available." << std::endl;
    }
    return false;
}

std::set<std::string> parseSerials(const std::vector<std::string>& lines) {
    std::set<std::string> serials;
    const std::regex pat(R"(\{([0-9]+):)");
    for (const auto& line : lines) {
        for (std::sregex_iterator it(line.begin(), line.end(), pat), end; it != end; ++it) {
            serials.insert((*it)[1].str());
        }
    }
    return serials;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    const ParseResult parse_result = parseArgs(argc, argv, opt);
    if (parse_result == ParseResult::Help) {
        return 0;
    }
    if (parse_result == ParseResult::Invalid) {
        std::cerr << "Invalid arguments.\n";
        printUsage();
        return 1;
    }

    std::array<char, 1024> err_buf{};
    const char* cfg = opt.config_file.empty() ? nullptr : opt.config_file.c_str();

    atnetLib lib = atnetInit(cfg, err_buf.size(), err_buf.data());
    if (lib == nullptr) {
        std::cerr << "[atnet] init failed: " << err_buf.data() << std::endl;
        return 2;
    }

    bool ok = true;
    do {
        const std::string scan_cmd =
            "scan " + std::to_string(opt.timeout_sec) + " " + std::to_string(opt.max_devices);
        ok = executeCommand(lib, scan_cmd, err_buf, nullptr);
        if (!ok) break;

        OutputCollector list_output;
        ok = executeCommand(lib, "list", err_buf, &list_output);
        if (!ok) break;

        const auto serials = parseSerials(list_output.lines);
        if (serials.empty()) {
            std::cout << "[atnet] No devices discovered by atnet." << std::endl;
        } else {
            std::cout << "[atnet] Discovered " << serials.size() << " device(s)." << std::endl;
            for (const auto& sn : serials) {
                executeCommand(lib, "info32 " + sn, err_buf, nullptr);
            }
        }

        if (opt.set_ip) {
            const std::string cmd = "set_ip " + opt.serial + " " + opt.ip + " " + std::to_string(opt.port);
            ok = executeCommand(lib, cmd, err_buf, nullptr);
            if (!ok) break;
            executeCommand(lib, "info32 " + opt.serial, err_buf, nullptr);
        }
    } while (false);

    const atnetError close_st = atnetClose(&lib);
    if (close_st != atnetError::Ok) {
        std::cerr << "[atnet] close failed." << std::endl;
    }

    return ok ? 0 : 3;
}
