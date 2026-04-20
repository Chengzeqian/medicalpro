#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <functional>

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

class PlatformLifecycleTraceRecorder;

class FRAMEWORK_EXPORT PlatformStartupCoordinator
{
public:
    using StartPluginFn = std::function<bool(const QString&)>;
    enum class PluginStartPath
    {
        Core,
        OnDemand,
        Deferred
    };

    PlatformStartupCoordinator(
        PlatformRuntimeMode runtimeMode,
        StartPluginFn startPluginFn,
        const QHash<QString, QString>& platformPluginIdToCtkSymbolicName = {},
        PlatformLifecycleTraceRecorder* recorder = nullptr);
    bool shouldInitializeFramework() const;
    bool shouldInstallPlugins() const;
    bool shouldStartCorePlugins() const;
    bool shouldStartDeferredPlugins() const;
    bool shouldWarmupServices() const;
    bool startCorePlugin(const QString& pluginId);
    bool startDeferredPlugins(const QStringList& pluginIds, bool stopOnFailure = false);
    bool ensureReady(const QString& pluginId);
    PlatformRuntimeMode runtimeMode() const;

private:
    struct ResolvedPluginTarget
    {
        bool managed = false;
        QString platformPluginId;
        QString ctkSymbolicName;
    };

    enum class StartOutcome
    {
        Started,
        AlreadyStarted,
        Skipped,
        Failed
    };

    ResolvedPluginTarget resolvePlatformPluginTarget(const QString& platformPluginId) const;
    ResolvedPluginTarget resolveDeferredPluginTarget(const QString& ctkSymbolicName) const;
    StartOutcome startPluginForPath(const ResolvedPluginTarget& target, PluginStartPath path);
    void recordPluginFailure(
        const ResolvedPluginTarget& target,
        PluginStartPath path,
        const QString& reasonCode,
        const QString& detail,
        bool blockingStartup);

    PlatformRuntimeMode m_runtimeMode;
    StartPluginFn m_startPluginFn;
    QHash<QString, QString> m_platformPluginIdToCtkSymbolicName;
    QHash<QString, QString> m_ctkSymbolicNameToPlatformPluginId;
    PlatformLifecycleTraceRecorder* m_recorder = nullptr;
    QSet<QString> m_startedPlugins;
};
