#include "Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h"

#include "Framework/CTKManager.h"
#include "Framework/StartupOrchestrator.h"

PlatformRuntimeObservation CtkRuntimeSnapshotCollector::collect() const
{
    PlatformRuntimeObservation observation;
    auto* ctkManager = CTKManager::instance();

    if (ctkManager) {
        observation.frameworkReady = ctkManager->isCTKAvailable();
        observation.installedPlugins = ctkManager->getInstalledPlugins();
        observation.startedPlugins = ctkManager->getStartedPlugins();
        observation.loadedPlugins = ctkManager->getLoadedPlugins();
        observation.pluginStates = ctkManager->getPluginStatus();
    }

    auto* startupOrchestrator = StartupOrchestrator::instance();
    if (startupOrchestrator) {
        observation.lifecycleEvents = startupOrchestrator->getLifecycleEvents();
        observation.startupTrace = startupOrchestrator->getStartupTraceEntries();
    }

    return observation;
}
