#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <functional>

#include <QSet>
#include <QString>

class FRAMEWORK_EXPORT PlatformStartupCoordinator
{
public:
    using StartPluginFn = std::function<bool(const QString&)>;

    PlatformStartupCoordinator(PlatformRuntimeMode runtimeMode, StartPluginFn startPluginFn);
    bool shouldInitializeFramework() const;
    bool shouldInstallPlugins() const;
    bool shouldStartCorePlugins() const;
    bool shouldStartDeferredPlugins() const;
    bool shouldWarmupServices() const;
    bool ensureReady(const QString& pluginId);
    PlatformRuntimeMode runtimeMode() const;

private:
    PlatformRuntimeMode m_runtimeMode;
    StartPluginFn m_startPluginFn;
    QSet<QString> m_startedOnDemandPlugins;
};
