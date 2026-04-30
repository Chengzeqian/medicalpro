#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/StartupOrchestrator.h"

#include <utility>

class FRAMEWORK_EXPORT StartupPhaseRegistrar
{
public:
    struct RuntimePhaseHandlers
    {
        explicit RuntimePhaseHandlers(
            StartupOrchestrator::PhaseHandler platformRuntimeInitHandler,
            StartupOrchestrator::PhaseHandler pluginInstallationHandler,
            StartupOrchestrator::PhaseHandler criticalPluginStartHandler,
            StartupOrchestrator::PhaseHandler deferredPluginStartHandler,
            StartupOrchestrator::PhaseHandler serviceWarmupHandler)
            : platformRuntimeInit(std::move(platformRuntimeInitHandler))
            , pluginInstallation(std::move(pluginInstallationHandler))
            , criticalPluginStart(std::move(criticalPluginStartHandler))
            , deferredPluginStart(std::move(deferredPluginStartHandler))
            , serviceWarmup(std::move(serviceWarmupHandler))
        {
        }

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
