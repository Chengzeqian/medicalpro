#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformFacadePorts.h"

#include <functional>

#include <QHash>

class FRAMEWORK_EXPORT LegacyNavigationAdapter : public INavigationFacadePort
{
public:
    using StartPluginFn = std::function<bool(const QString&)>;

    explicit LegacyNavigationAdapter(
        StartPluginFn startPluginFn = {},
        const QHash<QString, QString>& platformPluginIdToCtkSymbolicName = {});

    bool ensureReady(const QString& pluginId) override;

private:
    static QHash<QString, QString> loadPlatformPluginIdMapping();

    StartPluginFn m_startPluginFn;
    QHash<QString, QString> m_platformPluginIdToCtkSymbolicName;
};
