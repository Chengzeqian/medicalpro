#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/StartupOrchestrator.h"

class FRAMEWORK_EXPORT StartupPhaseRegistrar
{
public:
    struct RuntimePhaseHandlers
    {
        StartupOrchestrator::PhaseHandler platformRuntimeInit;
        StartupOrchestrator::PhaseHandler pluginInstallation;
        StartupOrchestrator::PhaseHandler criticalPluginStart;
        StartupOrchestrator::PhaseHandler deferredPluginStart;
        StartupOrchestrator::PhaseHandler serviceWarmup;
    };

    void registerRuntimePhases(
        StartupOrchestrator* orchestrator,
        const RuntimePhaseHandlers& handlers) const;
};
