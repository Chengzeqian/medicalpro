#include "Framework/Platform/Bootstrap/startup_phase_registrar.h"

#include <stdexcept>

void StartupPhaseRegistrar::registerRuntimePhases(
    StartupOrchestrator* orchestrator,
    const RuntimePhaseHandlers& handlers) const
{
    if (!orchestrator) {
        throw std::invalid_argument("StartupPhaseRegistrar requires a valid StartupOrchestrator");
    }
    if (!handlers.platformRuntimeInit()) throw std::invalid_argument("PlatformRuntimeInit handler must not be empty");
    if (!handlers.pluginInstallation()) throw std::invalid_argument("PluginInstallation handler must not be empty");
    if (!handlers.criticalPluginStart()) throw std::invalid_argument("CriticalPluginStart handler must not be empty");
    if (!handlers.deferredPluginStart()) throw std::invalid_argument("DeferredPluginStart handler must not be empty");
    if (!handlers.serviceWarmup()) throw std::invalid_argument("ServiceWarmup handler must not be empty");

    orchestrator->registerPhaseHandler(StartupPhase::PlatformRuntimeInit, handlers.platformRuntimeInit());
    orchestrator->registerPhaseHandler(StartupPhase::PluginInstallation, handlers.pluginInstallation());
    orchestrator->registerPhaseHandler(StartupPhase::CriticalPluginStart, handlers.criticalPluginStart());
    orchestrator->registerPhaseHandler(StartupPhase::DeferredPluginStart, handlers.deferredPluginStart());
    orchestrator->registerPhaseHandler(StartupPhase::ServiceWarmup, handlers.serviceWarmup());
}
