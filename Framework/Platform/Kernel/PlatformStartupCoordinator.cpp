#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"

#include <utility>

PlatformStartupCoordinator::PlatformStartupCoordinator(PlatformRuntimeMode runtimeMode, StartPluginFn startPluginFn)
    : m_runtimeMode(runtimeMode)
    , m_startPluginFn(std::move(startPluginFn))
{
}

bool PlatformStartupCoordinator::shouldInitializeFramework() const
{
    return m_runtimeMode != PlatformRuntimeMode::ObserveOnly;
}

bool PlatformStartupCoordinator::shouldInstallPlugins() const
{
    return m_runtimeMode != PlatformRuntimeMode::ObserveOnly;
}

bool PlatformStartupCoordinator::shouldStartCorePlugins() const
{
    return m_runtimeMode != PlatformRuntimeMode::ObserveOnly;
}

bool PlatformStartupCoordinator::shouldStartDeferredPlugins() const
{
    return m_runtimeMode == PlatformRuntimeMode::OrchestrateCore;
}

bool PlatformStartupCoordinator::shouldWarmupServices() const
{
    return m_runtimeMode == PlatformRuntimeMode::OrchestrateCore;
}

bool PlatformStartupCoordinator::ensureReady(const QString& pluginId)
{
    if (m_runtimeMode == PlatformRuntimeMode::ObserveOnly) return false;
    if (pluginId.isEmpty()) return false;
    if (m_startedOnDemandPlugins.contains(pluginId)) return true;
    if (!m_startPluginFn || !m_startPluginFn(pluginId)) return false;

    m_startedOnDemandPlugins.insert(pluginId);
    return true;
}

PlatformRuntimeMode PlatformStartupCoordinator::runtimeMode() const
{
    return m_runtimeMode;
}
