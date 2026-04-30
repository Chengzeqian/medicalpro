#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/StartupOrchestrator.h"

#include <utility>

class FRAMEWORK_EXPORT StartupPhaseRegistrar
{
public:
    struct PlatformRuntimeInitPhaseHandler
    {
        explicit PlatformRuntimeInitPhaseHandler(StartupOrchestrator::PhaseHandler handler)
            : m_handler(std::move(handler))
        {
        }

        const StartupOrchestrator::PhaseHandler& handler() const { return m_handler; }

    private:
        StartupOrchestrator::PhaseHandler m_handler;
    };

    struct PluginInstallationPhaseHandler
    {
        explicit PluginInstallationPhaseHandler(StartupOrchestrator::PhaseHandler handler)
            : m_handler(std::move(handler))
        {
        }

        const StartupOrchestrator::PhaseHandler& handler() const { return m_handler; }

    private:
        StartupOrchestrator::PhaseHandler m_handler;
    };

    struct CriticalPluginStartPhaseHandler
    {
        explicit CriticalPluginStartPhaseHandler(StartupOrchestrator::PhaseHandler handler)
            : m_handler(std::move(handler))
        {
        }

        const StartupOrchestrator::PhaseHandler& handler() const { return m_handler; }

    private:
        StartupOrchestrator::PhaseHandler m_handler;
    };

    struct DeferredPluginStartPhaseHandler
    {
        explicit DeferredPluginStartPhaseHandler(StartupOrchestrator::PhaseHandler handler)
            : m_handler(std::move(handler))
        {
        }

        const StartupOrchestrator::PhaseHandler& handler() const { return m_handler; }

    private:
        StartupOrchestrator::PhaseHandler m_handler;
    };

    struct ServiceWarmupPhaseHandler
    {
        explicit ServiceWarmupPhaseHandler(StartupOrchestrator::PhaseHandler handler)
            : m_handler(std::move(handler))
        {
        }

        const StartupOrchestrator::PhaseHandler& handler() const { return m_handler; }

    private:
        StartupOrchestrator::PhaseHandler m_handler;
    };

    class RuntimePhaseHandlers
    {
    public:
        explicit RuntimePhaseHandlers(
            PlatformRuntimeInitPhaseHandler platformRuntimeInitHandler,
            PluginInstallationPhaseHandler pluginInstallationHandler,
            CriticalPluginStartPhaseHandler criticalPluginStartHandler,
            DeferredPluginStartPhaseHandler deferredPluginStartHandler,
            ServiceWarmupPhaseHandler serviceWarmupHandler)
            : m_platformRuntimeInit(platformRuntimeInitHandler.handler())
            , m_pluginInstallation(pluginInstallationHandler.handler())
            , m_criticalPluginStart(criticalPluginStartHandler.handler())
            , m_deferredPluginStart(deferredPluginStartHandler.handler())
            , m_serviceWarmup(serviceWarmupHandler.handler())
        {
        }

        const StartupOrchestrator::PhaseHandler& platformRuntimeInit() const { return m_platformRuntimeInit; }
        const StartupOrchestrator::PhaseHandler& pluginInstallation() const { return m_pluginInstallation; }
        const StartupOrchestrator::PhaseHandler& criticalPluginStart() const { return m_criticalPluginStart; }
        const StartupOrchestrator::PhaseHandler& deferredPluginStart() const { return m_deferredPluginStart; }
        const StartupOrchestrator::PhaseHandler& serviceWarmup() const { return m_serviceWarmup; }

    private:
        StartupOrchestrator::PhaseHandler m_platformRuntimeInit;
        StartupOrchestrator::PhaseHandler m_pluginInstallation;
        StartupOrchestrator::PhaseHandler m_criticalPluginStart;
        StartupOrchestrator::PhaseHandler m_deferredPluginStart;
        StartupOrchestrator::PhaseHandler m_serviceWarmup;
    };

    void registerRuntimePhases(
        StartupOrchestrator* orchestrator,
        const RuntimePhaseHandlers& handlers) const;
};
