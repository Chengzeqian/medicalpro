#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h"
#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"
#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <functional>

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

class PlatformLifecycleTraceRecorder;

struct PlatformServiceReadyProbeSet
{
    std::function<QStringList(const QStringList&)> missingServicesFn;
    std::function<QStringList(const QString&)> missingPluginsFn;
    std::function<QStringList(const QString&)> missingCapabilitiesFn;
};

struct PlatformServiceReadyOutcome
{
    bool success = true;
    PlatformPluginState finalState = PlatformPluginState::Ready;
    QString reasonCode;
    QString detail;
    QStringList missingServices;
    QStringList missingPlugins;
    QStringList missingCapabilities;
};

struct PlatformOnDemandProbeSet
{
    std::function<PlatformPluginState(const QString&)> currentStateFn;
    std::function<QStringList(const QStringList&)> missingServicesFn;
    std::function<QStringList(const QString&)> missingPluginsFn;
    std::function<QStringList(const QString&)> missingCapabilitiesFn;
    std::function<QVector<PlatformHealthCheckResult>(const QString&, const QStringList&)> runHealthChecksFn;
};

struct PlatformOnDemandActivationOutcome
{
    bool success = false;
    PlatformLifecycleResult result = PlatformLifecycleResult::Failed;
    PlatformPluginState finalState = PlatformPluginState::Failed;
    QString reasonCode;
    QString detail;
    QString targetPluginId;
    QStringList missingServices;
    QStringList missingPlugins;
    QStringList missingCapabilities;
    QVector<PlatformHealthCheckResult> healthCheckResults;
};

class FRAMEWORK_EXPORT PlatformStartupCoordinator
{
public:
    using StartPluginFn = std::function<bool(const QString&)>;
    using InstallManagedPluginFn = std::function<bool(const PlatformManagedPluginPlanEntry&)>;
    using InstallOnDemandPluginFn = std::function<bool(const PlatformOnDemandActivationPlanEntry&)>;
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
    bool installManagedPlugins(
        const PlatformManagedPluginPlan& plan,
        const InstallManagedPluginFn& installManagedPluginFn);
    bool startCorePlugin(const QString& pluginId);
    bool startDeferredPlugins(const QStringList& pluginIds, bool stopOnFailure = false);
    bool ensureReady(const QString& pluginId);
    PlatformServiceReadyOutcome waitForServiceReady(
        const PlatformManagedPluginPlanEntry& entry,
        const PlatformServiceReadyProbeSet& probes,
        int pollIntervalMs = 50) const;
    PlatformOnDemandActivationOutcome activateOnDemand(
        const PlatformOnDemandActivationPlan& plan,
        const InstallOnDemandPluginFn& installOnDemandPluginFn,
        const PlatformOnDemandProbeSet& probes,
        int pollIntervalMs = 50);
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
