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
    const QHash<QString, QString>& platformPluginIdToCtkSymbolicName,
    PlatformLifecycleTraceRecorder* recorder)
    : m_runtimeMode(runtimeMode)
    , m_startPluginFn(std::move(startPluginFn))
    , m_platformPluginIdToCtkSymbolicName(platformPluginIdToCtkSymbolicName)
    , m_recorder(recorder)
{
    for (auto it = m_platformPluginIdToCtkSymbolicName.constBegin();
         it != m_platformPluginIdToCtkSymbolicName.constEnd();
         ++it) {
        const auto ctkSymbolicName = it.value().trimmed();
        if (ctkSymbolicName.isEmpty()) continue;
        m_ctkSymbolicNameToPlatformPluginId.insert(ctkSymbolicName, it.key());
    }
}

bool PlatformStartupCoordinator::shouldInitializeFramework() const
{
    return m_runtimeMode != PlatformRuntimeMode::ObserveOnly;
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
        if (m_recorder) {
            m_recorder->recordPluginStepStarted(
                entry.pluginId,
                entry.ctkSymbolicName,
                PlatformLifecycleStep::Install,
                false);
        }

        const bool installed = installManagedPluginFn && installManagedPluginFn(entry);
        if (!installed) {
            if (m_recorder) {
                m_recorder->recordPluginStepFinished(
                    entry.pluginId,
                    entry.ctkSymbolicName,
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
                entry.ctkSymbolicName,
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
    for (const auto& ctkSymbolicName : pluginIds) {
        if (ctkSymbolicName.isEmpty()) continue;

        const auto outcome = startPluginForPath(resolveDeferredPluginTarget(ctkSymbolicName), PluginStartPath::Deferred);
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
            return outcome;
        }

        QThread::msleep(pollIntervalMs);
    }

    outcome.success = false;
    outcome.finalState = PlatformPluginState::Failed;
    outcome.reasonCode = QStringLiteral("service_ready_timeout");
    outcome.detail = QStringLiteral("Timed out while waiting for managed plugin service readiness");
    return outcome;
}

PlatformStartupCoordinator::ResolvedPluginTarget PlatformStartupCoordinator::resolvePlatformPluginTarget(
    const QString& platformPluginId) const
{
    ResolvedPluginTarget target;
    target.platformPluginId = platformPluginId.trimmed();
    if (target.platformPluginId.isEmpty()) return target;
    if (m_platformPluginIdToCtkSymbolicName.contains(target.platformPluginId)) {
        target.ctkSymbolicName = m_platformPluginIdToCtkSymbolicName.value(target.platformPluginId).trimmed();
        target.managed = !target.ctkSymbolicName.isEmpty();
    }
    return target;
}

PlatformStartupCoordinator::ResolvedPluginTarget PlatformStartupCoordinator::resolveDeferredPluginTarget(
    const QString& ctkSymbolicName) const
{
    ResolvedPluginTarget target;
    target.ctkSymbolicName = ctkSymbolicName.trimmed();
    if (target.ctkSymbolicName.isEmpty()) return target;
    if (m_ctkSymbolicNameToPlatformPluginId.contains(target.ctkSymbolicName)) {
        target.platformPluginId = m_ctkSymbolicNameToPlatformPluginId.value(target.ctkSymbolicName).trimmed();
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
        target.ctkSymbolicName,
        PlatformLifecycleStep::Start,
        blockingStartup);
    m_recorder->recordPluginStepFinished(
        target.platformPluginId,
        target.ctkSymbolicName,
        PlatformLifecycleStep::Start,
        PlatformLifecycleResult::Failed,
        reasonCode,
        detail.isEmpty() ? failureDetail(path) : detail);
}

PlatformStartupCoordinator::StartOutcome PlatformStartupCoordinator::startPluginForPath(
    const ResolvedPluginTarget& target,
    PluginStartPath path)
{
    if (target.ctkSymbolicName.isEmpty()) {
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
        : QStringLiteral("ctk:%1").arg(target.ctkSymbolicName);

    if (!target.managed && path == PluginStartPath::Deferred) {
        if (isSkippedByMode(m_runtimeMode, path)) return StartOutcome::Skipped;
        if (m_startedPlugins.contains(startupKey)) return StartOutcome::AlreadyStarted;
        if (!m_startPluginFn || !m_startPluginFn(target.ctkSymbolicName)) return StartOutcome::Failed;
        m_startedPlugins.insert(startupKey);
        return StartOutcome::Started;
    }

    if (isSkippedByMode(m_runtimeMode, path)) {
        if (m_recorder) {
            m_recorder->recordPluginStepStarted(
                target.platformPluginId,
                target.ctkSymbolicName,
                PlatformLifecycleStep::Start,
                blockingStartup);
            m_recorder->recordPluginStepFinished(
                target.platformPluginId,
                target.ctkSymbolicName,
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
            target.ctkSymbolicName,
            PlatformLifecycleStep::Start,
            blockingStartup);
    }

    if (!m_startPluginFn || !m_startPluginFn(target.ctkSymbolicName)) {
        if (m_recorder) {
            m_recorder->recordPluginStepFinished(
                target.platformPluginId,
                target.ctkSymbolicName,
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
            target.ctkSymbolicName,
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
