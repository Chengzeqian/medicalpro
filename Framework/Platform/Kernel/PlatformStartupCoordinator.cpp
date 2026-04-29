#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"

#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"

#include <QElapsedTimer>
#include <QThread>

#include <utility>

namespace
{
QString startReasonCode(PlatformStartupCoordinator::PluginStartPath path)
{
    switch (path) {
    case PlatformStartupCoordinator::PluginStartPath::Core:
        return QStringLiteral("core");
    case PlatformStartupCoordinator::PluginStartPath::OnDemand:
        return QStringLiteral("on_demand");
    case PlatformStartupCoordinator::PluginStartPath::Deferred:
        return QStringLiteral("deferred");
    }

    return QStringLiteral("unknown");
}

QString successDetail(PlatformStartupCoordinator::PluginStartPath path)
{
    switch (path) {
    case PlatformStartupCoordinator::PluginStartPath::Core:
        return QStringLiteral("Core plugin start completed");
    case PlatformStartupCoordinator::PluginStartPath::OnDemand:
        return QStringLiteral("On-demand plugin start completed");
    case PlatformStartupCoordinator::PluginStartPath::Deferred:
        return QStringLiteral("Deferred plugin start completed");
    }

    return QStringLiteral("Plugin start completed");
}

QString skipDetail(PlatformStartupCoordinator::PluginStartPath path, PlatformRuntimeMode runtimeMode)
{
    const auto modeLabel = runtimeMode == PlatformRuntimeMode::ObserveOnly
        ? QStringLiteral("observe_only")
        : QStringLiteral("facade_mode");

    switch (path) {
    case PlatformStartupCoordinator::PluginStartPath::Core:
        return QStringLiteral("Core plugin start skipped in %1 mode").arg(modeLabel);
    case PlatformStartupCoordinator::PluginStartPath::OnDemand:
        return QStringLiteral("On-demand plugin start skipped in %1 mode").arg(modeLabel);
    case PlatformStartupCoordinator::PluginStartPath::Deferred:
        return QStringLiteral("Deferred plugin start skipped in %1 mode").arg(modeLabel);
    }

    return QStringLiteral("Plugin start skipped");
}

QString failureDetail(PlatformStartupCoordinator::PluginStartPath path)
{
    switch (path) {
    case PlatformStartupCoordinator::PluginStartPath::Core:
        return QStringLiteral("PlatformStartupCoordinator failed to start core plugin");
    case PlatformStartupCoordinator::PluginStartPath::OnDemand:
        return QStringLiteral("PlatformStartupCoordinator failed to start on-demand plugin");
    case PlatformStartupCoordinator::PluginStartPath::Deferred:
        return QStringLiteral("PlatformStartupCoordinator failed to start deferred plugin");
    }

    return QStringLiteral("PlatformStartupCoordinator failed to start plugin");
}

bool isBlockingStartup(PlatformStartupCoordinator::PluginStartPath path)
{
    return path == PlatformStartupCoordinator::PluginStartPath::Core;
}

bool isSkippedByMode(
    PlatformRuntimeMode runtimeMode,
    PlatformStartupCoordinator::PluginStartPath path)
{
    if (runtimeMode == PlatformRuntimeMode::ObserveOnly) return true;
    return path == PlatformStartupCoordinator::PluginStartPath::Deferred
        && runtimeMode != PlatformRuntimeMode::OrchestrateCore;
}
}

PlatformStartupCoordinator::PlatformStartupCoordinator(
    PlatformRuntimeMode runtimeMode,
    StartPluginFn startPluginFn,
    const QHash<QString, QString>& platformPluginIdToSymbolicName,
    PlatformLifecycleTraceRecorder* recorder)
    : m_runtimeMode(runtimeMode)
    , m_startPluginFn(std::move(startPluginFn))
    , m_platformPluginIdToSymbolicName(platformPluginIdToSymbolicName)
    , m_recorder(recorder)
{
    for (auto it = m_platformPluginIdToSymbolicName.constBegin();
         it != m_platformPluginIdToSymbolicName.constEnd();
         ++it) {
        const auto symbolicName = it.value().trimmed();
        if (symbolicName.isEmpty()) continue;
        m_symbolicNameToPlatformPluginId.insert(symbolicName, it.key());
    }
}

bool PlatformStartupCoordinator::shouldInitializeFramework() const
{
    return m_runtimeMode != PlatformRuntimeMode::ObserveOnly;
}

bool PlatformStartupCoordinator::shouldInitializeFramework(bool ctkRuntimeRequired) const
{
    if (m_runtimeMode == PlatformRuntimeMode::ObserveOnly) return false;
    if (m_runtimeMode == PlatformRuntimeMode::FacadeMode) return ctkRuntimeRequired;
    return true;
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

bool PlatformStartupCoordinator::installManagedPlugins(
    const PlatformManagedPluginPlan& plan,
    const InstallManagedPluginFn& installManagedPluginFn)
{
    if (!shouldInstallPlugins()) return true;

    for (const auto& entry : plan.installEntries) {
        if (!entry.requiresBundleInstall) {
            if (m_recorder) {
                m_recorder->recordPluginStepStarted(
                    entry.pluginId,
                    entry.symbolicName,
                    PlatformLifecycleStep::Install,
                    false);
                m_recorder->recordPluginStepFinished(
                    entry.pluginId,
                    entry.symbolicName,
                    PlatformLifecycleStep::Install,
                    PlatformLifecycleResult::Skipped,
                    QStringLiteral("platform_module_pre_registered"),
                    QStringLiteral("Platform module is pre-registered; bundle install skipped"));
            }
            continue;
        }

        if (m_recorder) {
            m_recorder->recordPluginStepStarted(
                entry.pluginId,
                entry.symbolicName,
                PlatformLifecycleStep::Install,
                false);
        }

        const bool installed = installManagedPluginFn && installManagedPluginFn(entry);
        if (!installed) {
            if (m_recorder) {
                m_recorder->recordPluginStepFinished(
                    entry.pluginId,
                    entry.symbolicName,
                    PlatformLifecycleStep::Install,
                    PlatformLifecycleResult::Failed,
                    QStringLiteral("install_failed"),
                    QStringLiteral("Managed bundle install failed"));
            }
            return false;
        }

        if (m_recorder) {
            m_recorder->recordPluginStepFinished(
                entry.pluginId,
                entry.symbolicName,
                PlatformLifecycleStep::Install,
                PlatformLifecycleResult::Succeeded,
                QStringLiteral("install_succeeded"),
                QStringLiteral("Managed bundle install succeeded"));
        }
    }

    return true;
}

bool PlatformStartupCoordinator::startCorePlugin(const QString& pluginId)
{
    const auto outcome = startPluginForPath(resolvePlatformPluginTarget(pluginId), PluginStartPath::Core);
    return outcome == StartOutcome::Started || outcome == StartOutcome::AlreadyStarted;
}

bool PlatformStartupCoordinator::startDeferredPlugins(const QStringList& pluginIds, bool stopOnFailure)
{
    bool success = true;
    for (const auto& symbolicName : pluginIds) {
        if (symbolicName.isEmpty()) continue;

        const auto outcome = startPluginForPath(resolveDeferredPluginTarget(symbolicName), PluginStartPath::Deferred);
        if (outcome == StartOutcome::Failed) {
            success = false;
            if (stopOnFailure) break;
        }
        if (outcome == StartOutcome::Skipped) {
            continue;
        }
    }

    return success;
}

bool PlatformStartupCoordinator::ensureReady(const QString& pluginId)
{
    const auto outcome = startPluginForPath(resolvePlatformPluginTarget(pluginId), PluginStartPath::OnDemand);
    return outcome == StartOutcome::Started || outcome == StartOutcome::AlreadyStarted;
}

PlatformServiceReadyOutcome PlatformStartupCoordinator::waitForServiceReady(
    const PlatformManagedPluginPlanEntry& entry,
    const PlatformServiceReadyProbeSet& probes,
    int pollIntervalMs) const
{
    PlatformServiceReadyOutcome outcome;
    if (m_recorder) {
        m_recorder->recordPluginStepStarted(
            entry.pluginId,
            entry.symbolicName,
            PlatformLifecycleStep::ServiceReady,
            false);
    }

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < entry.serviceReadyTimeoutMs) {
        outcome.missingServices = probes.missingServicesFn ? probes.missingServicesFn(entry.requiredServices) : QStringList{};
        outcome.missingPlugins = probes.missingPluginsFn ? probes.missingPluginsFn(entry.pluginId) : QStringList{};
        outcome.missingCapabilities = probes.missingCapabilitiesFn ? probes.missingCapabilitiesFn(entry.pluginId) : QStringList{};

        if (outcome.missingServices.isEmpty()
            && outcome.missingPlugins.isEmpty()
            && outcome.missingCapabilities.isEmpty()) {
            outcome.finalState = PlatformPluginState::Ready;
            outcome.reasonCode = QStringLiteral("service_ready");
            outcome.detail = QStringLiteral("Required services and dependencies are ready");
            if (m_recorder) {
                m_recorder->recordPluginStepFinished(
                    entry.pluginId,
                    entry.symbolicName,
                    PlatformLifecycleStep::ServiceReady,
                    PlatformLifecycleResult::Succeeded,
                    outcome.reasonCode,
                    outcome.detail);
            }
            return outcome;
        }

        QThread::msleep(pollIntervalMs);
    }

    outcome.success = false;
    outcome.finalState = PlatformPluginState::Failed;
    outcome.reasonCode = QStringLiteral("service_ready_timeout");
    outcome.detail = QStringLiteral("Timed out while waiting for managed plugin service readiness");
    if (m_recorder) {
        m_recorder->recordPluginStepFinished(
            entry.pluginId,
            entry.symbolicName,
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Timeout,
            outcome.reasonCode,
            outcome.detail);
    }
    return outcome;
}

PlatformOnDemandActivationOutcome PlatformStartupCoordinator::activateOnDemand(
    const PlatformOnDemandActivationPlan& plan,
    const InstallOnDemandPluginFn& installOnDemandPluginFn,
    const PlatformOnDemandProbeSet& probes,
    int pollIntervalMs)
{
    PlatformOnDemandActivationOutcome outcome;
    outcome.targetPluginId = plan.targetPluginId.trimmed();

    if (plan.activationEntries.isEmpty()) {
        outcome.reasonCode = QStringLiteral("descriptor_missing");
        outcome.detail = QStringLiteral("On-demand activation plan is empty");
        return outcome;
    }

    const auto& targetEntry = plan.activationEntries.constLast();
    const auto makeTarget = [](const PlatformOnDemandActivationPlanEntry& entry) {
        ResolvedPluginTarget target;
        target.managed = true;
        target.platformPluginId = entry.pluginId.trimmed();
        target.symbolicName = entry.symbolicName.trimmed();
        return target;
    };

    if (m_runtimeMode == PlatformRuntimeMode::ObserveOnly) {
        const auto startOutcome = startPluginForPath(makeTarget(targetEntry), PluginStartPath::OnDemand);
        if (startOutcome == StartOutcome::Skipped) {
            outcome.reasonCode = QStringLiteral("skipped_by_mode");
            outcome.detail = QStringLiteral("On-demand plugin start skipped in observe_only mode");
            outcome.result = PlatformLifecycleResult::Skipped;
            outcome.finalState = PlatformPluginState::Discovered;
            return outcome;
        }

        outcome.reasonCode = QStringLiteral("start_failed");
        outcome.detail = QStringLiteral("On-demand plugin start failed");
        return outcome;
    }

    if (probes.currentStateFn && probes.currentStateFn(targetEntry.pluginId) == PlatformPluginState::Ready) {
        outcome.success = true;
        outcome.result = PlatformLifecycleResult::Succeeded;
        outcome.finalState = PlatformPluginState::Ready;
        outcome.reasonCode = QStringLiteral("already_ready");
        outcome.detail = QStringLiteral("Target plugin is already ready");
        return outcome;
    }

    for (const auto& entry : plan.activationEntries) {
        if (entry.requiresBundleInstall && (!installOnDemandPluginFn || !installOnDemandPluginFn(entry))) {
            outcome.reasonCode = QStringLiteral("install_failed");
            outcome.detail = QStringLiteral("On-demand bundle install failed");
            return outcome;
        }

        const auto startOutcome = startPluginForPath(makeTarget(entry), PluginStartPath::OnDemand);
        if (startOutcome == StartOutcome::Failed) {
            outcome.reasonCode = QStringLiteral("start_failed");
            outcome.detail = QStringLiteral("On-demand plugin start failed");
            return outcome;
        }

        PlatformManagedPluginPlanEntry managedEntry;
        managedEntry.pluginId = entry.pluginId;
        managedEntry.displayName = entry.displayName;
        managedEntry.symbolicName = entry.symbolicName;
        managedEntry.bundleFilePath = entry.bundleFilePath;
        managedEntry.bootstrapLevel = PlatformBootstrapLevel::Deferred;
        managedEntry.startupPolicy = PlatformStartupPolicy::OnDemand;
        managedEntry.requiredPlugins = entry.requiredPlugins;
        managedEntry.requiredCapabilities = entry.requiredCapabilities;
        managedEntry.requiredServices = entry.requiredServices;
        managedEntry.healthChecks = entry.healthChecks;
        managedEntry.serviceReadyTimeoutMs = entry.serviceReadyTimeoutMs;

        const auto serviceOutcome = waitForServiceReady(
            managedEntry,
            {
                probes.missingServicesFn,
                probes.missingPluginsFn,
                probes.missingCapabilitiesFn
            },
            pollIntervalMs);
        if (!serviceOutcome.success) {
            outcome.result = PlatformLifecycleResult::Timeout;
            outcome.finalState = serviceOutcome.finalState;
            outcome.reasonCode = serviceOutcome.reasonCode;
            outcome.detail = serviceOutcome.detail;
            outcome.missingServices = serviceOutcome.missingServices;
            outcome.missingPlugins = serviceOutcome.missingPlugins;
            outcome.missingCapabilities = serviceOutcome.missingCapabilities;
            return outcome;
        }
    }

    outcome.healthCheckResults = probes.runHealthChecksFn
        ? probes.runHealthChecksFn(targetEntry.pluginId, targetEntry.healthChecks)
        : QVector<PlatformHealthCheckResult>{};
    for (const auto& healthCheckResult : outcome.healthCheckResults) {
        if (healthCheckResult.passed) continue;
        outcome.reasonCode = QStringLiteral("health_check_failed");
        outcome.detail = healthCheckResult.detail;
        outcome.result = PlatformLifecycleResult::Failed;
        outcome.finalState = PlatformPluginState::Failed;
        return outcome;
    }

    outcome.success = true;
    outcome.result = PlatformLifecycleResult::Succeeded;
    outcome.finalState = PlatformPluginState::Ready;
    outcome.reasonCode = QStringLiteral("service_ready");
    outcome.detail = QStringLiteral("On-demand plugin is ready");
    return outcome;
}

PlatformStartupCoordinator::ResolvedPluginTarget PlatformStartupCoordinator::resolvePlatformPluginTarget(
    const QString& platformPluginId) const
{
    ResolvedPluginTarget target;
    target.platformPluginId = platformPluginId.trimmed();
    if (target.platformPluginId.isEmpty()) return target;
    if (m_platformPluginIdToSymbolicName.contains(target.platformPluginId)) {
        target.symbolicName = m_platformPluginIdToSymbolicName.value(target.platformPluginId).trimmed();
        target.managed = !target.symbolicName.isEmpty();
    }
    return target;
}

PlatformStartupCoordinator::ResolvedPluginTarget PlatformStartupCoordinator::resolveDeferredPluginTarget(
    const QString& symbolicName) const
{
    ResolvedPluginTarget target;
    target.symbolicName = symbolicName.trimmed();
    if (target.symbolicName.isEmpty()) return target;
    if (m_symbolicNameToPlatformPluginId.contains(target.symbolicName)) {
        target.platformPluginId = m_symbolicNameToPlatformPluginId.value(target.symbolicName).trimmed();
        target.managed = !target.platformPluginId.isEmpty();
    }
    return target;
}

void PlatformStartupCoordinator::recordPluginFailure(
    const ResolvedPluginTarget& target,
    PluginStartPath path,
    const QString& reasonCode,
    const QString& detail,
    bool blockingStartup)
{
    if (!m_recorder) return;

    m_recorder->recordPluginStepStarted(
        target.platformPluginId,
        target.symbolicName,
        PlatformLifecycleStep::Start,
        blockingStartup);
    m_recorder->recordPluginStepFinished(
        target.platformPluginId,
        target.symbolicName,
        PlatformLifecycleStep::Start,
        PlatformLifecycleResult::Failed,
        reasonCode,
        detail.isEmpty() ? failureDetail(path) : detail);
}

PlatformStartupCoordinator::StartOutcome PlatformStartupCoordinator::startPluginForPath(
    const ResolvedPluginTarget& target,
    PluginStartPath path)
{
    if (target.symbolicName.isEmpty()) {
        recordPluginFailure(
            target,
            path,
            QStringLiteral("plugin_mapping_missing"),
            QStringLiteral("PlatformStartupCoordinator could not resolve plugin identity mapping"),
            isBlockingStartup(path));
        return StartOutcome::Failed;
    }

    const auto blockingStartup = isBlockingStartup(path);
    if (!target.managed && path != PluginStartPath::Deferred) {
        recordPluginFailure(
            target,
            path,
            QStringLiteral("plugin_mapping_missing"),
            QStringLiteral("PlatformStartupCoordinator could not resolve plugin identity mapping"),
            blockingStartup);
        return StartOutcome::Failed;
    }

    const QString startupKey = target.managed
        ? target.platformPluginId
        : QStringLiteral("symbolic:%1").arg(target.symbolicName);

    if (!target.managed && path == PluginStartPath::Deferred) {
        if (isSkippedByMode(m_runtimeMode, path)) return StartOutcome::Skipped;
        if (m_startedPlugins.contains(startupKey)) return StartOutcome::AlreadyStarted;
        if (!m_startPluginFn || !m_startPluginFn(target.symbolicName)) return StartOutcome::Failed;
        m_startedPlugins.insert(startupKey);
        return StartOutcome::Started;
    }

    if (isSkippedByMode(m_runtimeMode, path)) {
        if (m_recorder) {
            m_recorder->recordPluginStepStarted(
                target.platformPluginId,
                target.symbolicName,
                PlatformLifecycleStep::Start,
                blockingStartup);
            m_recorder->recordPluginStepFinished(
                target.platformPluginId,
                target.symbolicName,
                PlatformLifecycleStep::Start,
                PlatformLifecycleResult::Skipped,
                QStringLiteral("skipped_by_mode"),
                skipDetail(path, m_runtimeMode));
        }
        return StartOutcome::Skipped;
    }

    if (m_startedPlugins.contains(startupKey)) return StartOutcome::AlreadyStarted;

    if (m_recorder) {
        m_recorder->recordPluginStepStarted(
            target.platformPluginId,
            target.symbolicName,
            PlatformLifecycleStep::Start,
            blockingStartup);
    }

    if (!m_startPluginFn || !m_startPluginFn(target.symbolicName)) {
        if (m_recorder) {
            m_recorder->recordPluginStepFinished(
                target.platformPluginId,
                target.symbolicName,
                PlatformLifecycleStep::Start,
                PlatformLifecycleResult::Failed,
                QStringLiteral("plugin_start_failed"),
                failureDetail(path));
        }
        return StartOutcome::Failed;
    }

    m_startedPlugins.insert(startupKey);
    if (m_recorder) {
        m_recorder->recordPluginStepFinished(
            target.platformPluginId,
            target.symbolicName,
            PlatformLifecycleStep::Start,
            PlatformLifecycleResult::Succeeded,
            startReasonCode(path),
            successDetail(path));
    }

    return StartOutcome::Started;
}

PlatformRuntimeMode PlatformStartupCoordinator::runtimeMode() const
{
    return m_runtimeMode;
}
