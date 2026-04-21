#include "Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h"

#include <QDebug>

#include <utility>

LegacyNavigationAdapter::LegacyNavigationAdapter(EnsureReadyFn ensureReadyFn)
    : m_ensureReadyFn(std::move(ensureReadyFn))
{
}

bool LegacyNavigationAdapter::ensureReady(const QString& pluginId)
{
    const auto trimmedPluginId = pluginId.trimmed();
    if (trimmedPluginId.isEmpty() || !m_ensureReadyFn) {
        qWarning() << "[LegacyNavigationAdapter] Missing governed on-demand activation callback for plugin id:" << trimmedPluginId;
        return false;
    }
    return m_ensureReadyFn(trimmedPluginId);
}
