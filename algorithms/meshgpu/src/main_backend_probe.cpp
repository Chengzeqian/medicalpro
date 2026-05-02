#include <iostream>

#include "backend_runtime_cli.h"
#include "mesh_gpu_interface.h"

int main(int argc, char** argv) {
    mesh_gpu::BackendCliOptions cli = mesh_gpu::parseBackendCliOptions(argc, argv);
    if (!cli.valid) {
        std::cerr << "[Args] " << cli.error_message << std::endl;
        std::cerr << mesh_gpu::buildBackendCliUsage(argv[0], "") << std::endl;
        return 2;
    }
    if (cli.show_help) {
        std::cout << mesh_gpu::buildBackendCliUsage(argv[0], "") << std::endl;
        return 0;
    }
    if (!cli.positional_args.empty()) {
        std::cerr << "[Args] backend_probe does not accept positional args." << std::endl;
        return 2;
    }

    const auto status = mesh_gpu::MeshGPUInterface::probeBackend(cli.backend_config);
    std::cout << "Probe:\n";
    std::cout << "  requested=" << mesh_gpu::MeshGPUInterface::backendKindToString(status.requested) << "\n";
    std::cout << "  selected=" << mesh_gpu::MeshGPUInterface::backendKindToString(status.selected) << "\n";
    std::cout << "  initialized=" << (status.initialized ? "yes" : "no") << "\n";
    std::cout << "  cuda_available=" << (status.cuda_available ? "yes" : "no") << "\n";
    std::cout << "  ascend_runtime_available=" << (status.ascend_runtime_available ? "yes" : "no") << "\n";
    std::cout << "  ascend_plugin_available=" << (status.ascend_plugin_available ? "yes" : "no") << "\n";
    std::cout << "  cpu_available=" << (status.cpu_available ? "yes" : "no") << "\n";
    std::cout << "  message=" << status.message << "\n";

    mesh_gpu::MeshGPUInterface iface;
    iface.setBackendConfig(cli.backend_config);
    std::cout << "\nInstance status:\n  " << iface.getBackendInfo() << std::endl;

    return status.initialized ? 0 : 1;
}
