#include "Framework/Platform/Bootstrap/startup_phase_registrar.h"

#include <QtGlobal>

void StartupPhaseRegistrar::registerRuntimePhases(
    StartupOrchestrator* orchestrator,
    const RuntimePhaseHandlers& handlers) const
{
    Q_ASSERT(orchestrator != nullptr);
    Q_ASSERT(handlers.platformRuntimeInit);
    Q_ASSERT(handlers.pluginInstallation);
    Q_ASSERT(handlers.criticalPluginStart);
    Q_ASSERT(handlers.deferredPluginStart);
    Q_ASSERT(handlers.serviceWarmup);
    if (!orchestrator
        || !handlers.platformRuntimeInit
        || !handlers.pluginInstallation
        || !handlers.criticalPluginStart
        || !handlers.deferredPluginStart
        || !handlers.serviceWarmup) {
        return;
    }

    orchestrator->registerPhaseHandler(StartupPhase::PlatformRuntimeInit, handlers.platformRuntimeInit);
    orchestrator->registerPhaseHandler(StartupPhase::PluginInstallation, handlers.pluginInstallation);
    orchestrator->registerPhaseHandler(StartupPhase::CriticalPluginStart, handlers.criticalPluginStart);
    orchestrator->registerPhaseHandler(StartupPhase::DeferredPluginStart, handlers.deferredPluginStart);
    orchestrator->registerPhaseHandler(StartupPhase::ServiceWarmup, handlers.serviceWarmup);
}
