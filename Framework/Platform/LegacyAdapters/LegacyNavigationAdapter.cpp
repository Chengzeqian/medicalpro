#include "Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h"

#include "Framework/CTKManager.h"

bool LegacyNavigationAdapter::ensureReady(const QString& pluginId)
{
    return CTKManager::instance()->startPlugin(pluginId);
}
