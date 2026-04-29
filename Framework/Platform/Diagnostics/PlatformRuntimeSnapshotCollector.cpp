#include "Framework/Platform/Diagnostics/PlatformRuntimeSnapshotCollector.h"

#include "Framework/Platform/Kernel/platform_plugin_host.h"
#include "Framework/StartupOrchestrator.h"

namespace
{
QString pluginStateFor(const PlatformPluginHost& host, const QString& pluginId)
{
    if (host.isModuleStarted(pluginId)) return QStringLiteral("ACTIVE");
    if (host.hasActivator(pluginId)) return QStringLiteral("RESOLVED");
    return QStringLiteral("UNKNOWN");
}
}

PlatformRuntimeObservation PlatformRuntimeSnapshotCollector::collect() const
{
    PlatformRuntimeObservation observation;
    auto& host = PlatformPluginHost::sharedInstance();
    observation.installedPlugins = host.registeredPluginIds();
    observation.loadedPlugins = observation.installedPlugins;
    observation.frameworkReady = !observation.installedPlugins.isEmpty();
    for (const QString& pluginId : observation.installedPlugins) {
        if (host.isModuleStarted(pluginId)) observation.startedPlugins.append(pluginId);
        observation.pluginStates.insert(pluginId, pluginStateFor(host, pluginId));
    }

    auto* startupOrchestrator = StartupOrchestrator::instance();
    if (startupOrchestrator) {
        observation.lifecycleEvents = startupOrchestrator->getLifecycleEvents();
        observation.startupTrace = startupOrchestrator->getStartupTraceEntries();
    }

    return observation;
}
