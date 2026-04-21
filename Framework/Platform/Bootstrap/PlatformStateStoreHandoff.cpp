#include "Framework/Platform/Bootstrap/PlatformStateStoreHandoff.h"

#include "Framework/Platform/Kernel/PlatformStateStore.h"

void copyPlatformStateStore(PlatformStateStore* target, const PlatformStateStore* source)
{
    if (!target || !source) {
        return;
    }

    const auto capabilitySnapshot = source->capabilitySnapshot();
    target->replaceDescriptors(source->descriptors());
    target->setRuntimeMode(capabilitySnapshot.runtimeMode);
    target->setStartupScopePluginIds(source->startupScopePluginIds());
    target->setGovernedPluginIds(source->governedPluginIds());

    for (const auto& snapshot : source->pluginSnapshots()) {
        target->setPluginState(snapshot.pluginId, snapshot.state);
    }
}
