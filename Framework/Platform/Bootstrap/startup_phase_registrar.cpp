#include "Framework/Platform/Bootstrap/startup_phase_registrar.h"

#include <QtGlobal>

void StartupPhaseRegistrar::registerRuntimePhases(
    StartupOrchestrator* orchestrator,
    const RuntimePhaseHandlers& handlers) const
{
    Q_ASSERT(orchestrator != nullptr);
    if (!orchestrator) {
        return;
    }

    orchestrator->registerPhaseHandler(StartupPhase::PlatformRuntimeInit, handlers.platformRuntimeInit);
    orchestrator->registerPhaseHandler(StartupPhase::PluginInstallation, handlers.pluginInstallation);
    orchestrator->registerPhaseHandler(StartupPhase::CriticalPluginStart, handlers.criticalPluginStart);
    orchestrator->registerPhaseHandler(StartupPhase::DeferredPluginStart, handlers.deferredPluginStart);
    orchestrator->registerPhaseHandler(StartupPhase::ServiceWarmup, handlers.serviceWarmup);
}
