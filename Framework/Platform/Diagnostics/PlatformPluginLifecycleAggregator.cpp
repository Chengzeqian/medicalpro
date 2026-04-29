#include "Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.h"

#include <QHash>
#include <QtGlobal>

#include <algorithm>
#include <limits>

namespace
{
struct PluginContext
{
    PlatformPluginLifecycleSnapshot snapshot;
    QStringList impactCapabilities;
};

PlatformPluginState promoteState(PlatformPluginState current, PlatformPluginState next)
{
    const auto rank = [](PlatformPluginState state) {
        switch (state) {
        case PlatformPluginState::Discovered:
            return 0;
        case PlatformPluginState::Installed:
            return 1;
        case PlatformPluginState::Starting:
            return 2;
        case PlatformPluginState::Ready:
            return 3;
        case PlatformPluginState::Degraded:
            return 4;
        case PlatformPluginState::Failed:
            return 5;
        }
        return 0;
    };

    return rank(next) > rank(current) ? next : current;
}

void appendUnique(QStringList& target, const QString& value)
{
    if (value.isEmpty()) return;
    if (!target.contains(value)) target.append(value);
}

void appendAllUnique(QStringList& target, const QStringList& values)
{
    for (const auto& value : values) {
        appendUnique(target, value);
    }
}

PlatformPluginState stateFromRuntimeText(const QString& runtimeState)
{
    const auto normalized = runtimeState.trimmed().toUpper();
    if (normalized == QStringLiteral("ACTIVE")) return PlatformPluginState::Ready;
    if (normalized == QStringLiteral("STARTING")) return PlatformPluginState::Starting;
    if (normalized == QStringLiteral("RESOLVED") || normalized == QStringLiteral("INSTALLED")) {
        return PlatformPluginState::Installed;
    }
    return PlatformPluginState::Discovered;
}

QString toEventLabel(const PlatformLifecycleEvent& event)
{
    const auto pluginLabel = !event.pluginId.isEmpty() ? event.pluginId : event.symbolicName;
    if (!pluginLabel.isEmpty()) {
        switch (event.step) {
        case PlatformLifecycleStep::Install:
            return QStringLiteral("%1 install").arg(pluginLabel);
        case PlatformLifecycleStep::Start:
            return QStringLiteral("%1 start").arg(pluginLabel);
        case PlatformLifecycleStep::ServiceReady:
            return QStringLiteral("%1 service_ready").arg(pluginLabel);
        case PlatformLifecycleStep::Warmup:
            return QStringLiteral("%1 warmup").arg(pluginLabel);
        case PlatformLifecycleStep::None:
            break;
        }
    }

    if (!event.phaseLabel.isEmpty()) return event.phaseLabel;
    if (!event.phaseKey.isEmpty()) return event.phaseKey;
    return QStringLiteral("unknown");
}

PlatformDiagnosticSeverity severityForEvent(const PlatformLifecycleEvent& event)
{
    if (event.result == PlatformLifecycleResult::Failed || event.result == PlatformLifecycleResult::Timeout) {
        return event.blockingStartup ? PlatformDiagnosticSeverity::Critical : PlatformDiagnosticSeverity::Error;
    }
    if (event.result == PlatformLifecycleResult::Degraded) return PlatformDiagnosticSeverity::Warning;
    if (event.kind == PlatformLifecycleEventKind::PluginSkippedByMode) return PlatformDiagnosticSeverity::Info;
    return PlatformDiagnosticSeverity::Info;
}

int stateSeverityRank(PlatformPluginState state)
{
    switch (state) {
    case PlatformPluginState::Failed:
        return 6;
    case PlatformPluginState::Degraded:
        return 5;
    case PlatformPluginState::Starting:
        return 4;
    case PlatformPluginState::Installed:
        return 3;
    case PlatformPluginState::Discovered:
        return 2;
    case PlatformPluginState::Ready:
        return 1;
    }
    return 0;
}

bool isFailedOrTimedOut(PlatformLifecycleResult result)
{
    return result == PlatformLifecycleResult::Failed || result == PlatformLifecycleResult::Timeout;
}

bool isReadyPathBlockingEvent(const PlatformLifecycleEvent& event)
{
    return event.blockingStartup
        && event.step != PlatformLifecycleStep::Warmup
        && event.result != PlatformLifecycleResult::Running;
}

int slowestStepPriority(PlatformLifecycleStep step)
{
    switch (step) {
    case PlatformLifecycleStep::ServiceReady:
        return 4;
    case PlatformLifecycleStep::Start:
        return 3;
    case PlatformLifecycleStep::Install:
        return 2;
    case PlatformLifecycleStep::Warmup:
        return 1;
    case PlatformLifecycleStep::None:
        return 0;
    }
    return 0;
}

QStringList stableRecoveryHintsForReasonCode(const QString& reasonCode)
{
    if (reasonCode == QStringLiteral("descriptor_missing")) {
        return {QStringLiteral("Check whether plugins/descriptors are deployed completely.")};
    }
    if (reasonCode == QStringLiteral("plugin_install_failed")) {
        return {QStringLiteral("Check plugin binaries, dependent DLLs, and runtime artifact layout.")};
    }
    if (reasonCode == QStringLiteral("plugin_start_failed")) {
        return {QStringLiteral("Check plugin dependencies and retry the failed plugin startup.")};
    }
    if (reasonCode == QStringLiteral("service_missing")) {
        return {QStringLiteral("Check service registration, interface names, and whether health checks are too strict.")};
    }
    if (reasonCode == QStringLiteral("service_ready_timeout")) {
        return {QStringLiteral("Check service registration chain and verify required services are available.")};
    }
    if (reasonCode == QStringLiteral("warmup_failed")) {
        return {QStringLiteral("Check whether warmup tasks depend on undeclared resources or slow prerequisites.")};
    }
    if (reasonCode == QStringLiteral("skipped_by_mode")) {
        return {QStringLiteral("Check whether the current runtime mode intentionally skips this stage.")};
    }
    if (reasonCode == QStringLiteral("runtime_platform_state_mismatch")) {
        return {QStringLiteral("Check whether runtime active state and platform ready conditions are aligned.")};
    }
    return {};
}

bool isBetterSlowestPluginCandidate(
    const PlatformPluginLifecycleSnapshot& candidate,
    const PlatformPluginLifecycleSnapshot& currentBest)
{
    if (candidate.startupBlocked != currentBest.startupBlocked) {
        return candidate.startupBlocked;
    }
    if (candidate.blockingMs != currentBest.blockingMs) {
        return candidate.blockingMs > currentBest.blockingMs;
    }
    const auto candidateStepPriority = slowestStepPriority(candidate.slowestStep);
    const auto currentStepPriority = slowestStepPriority(currentBest.slowestStep);
    if (candidateStepPriority != currentStepPriority) {
        return candidateStepPriority > currentStepPriority;
    }
    return candidate.pluginId < currentBest.pluginId;
}
}

PlatformPluginLifecycleAggregation PlatformPluginLifecycleAggregator::aggregate(
    const QVector<PlatformLifecycleEvent>& lifecycleEvents,
    const QVector<PlatformPluginDescriptor>& descriptors,
    const PlatformRuntimeObservation& observation) const
{
    PlatformPluginLifecycleAggregation aggregation;
    aggregation.summary.frameworkReady = observation.frameworkReady;

    QHash<QString, PluginContext> contextsByPluginId;
    QHash<QString, QString> pluginIdBySymbolicName;
    contextsByPluginId.reserve(descriptors.size());
    pluginIdBySymbolicName.reserve(descriptors.size());

    for (const auto& descriptor : descriptors) {
        PluginContext context;
        context.snapshot.pluginId = descriptor.id;
        context.snapshot.symbolicName = descriptor.runtime.symbolicName;
        context.snapshot.displayName = descriptor.displayName;
        context.snapshot.bootstrapLevel = descriptor.runtime.bootstrapLevel;
        context.snapshot.startupPolicy = descriptor.runtime.startupPolicy;
        context.impactCapabilities = descriptor.provides.capabilities;
        contextsByPluginId.insert(descriptor.id, context);
        if (!descriptor.runtime.symbolicName.isEmpty()) {
            pluginIdBySymbolicName.insert(descriptor.runtime.symbolicName.trimmed(), descriptor.id);
        }
    }

    const auto ensureContext = [&contextsByPluginId, &pluginIdBySymbolicName](
                                   const QString& pluginId,
                                   const QString& symbolicName) -> PluginContext& {
        QString resolvedPluginId = pluginId.trimmed();
        if (resolvedPluginId.isEmpty() && !symbolicName.isEmpty()) {
            resolvedPluginId = pluginIdBySymbolicName.value(symbolicName.trimmed());
        }
        if (resolvedPluginId.isEmpty()) {
            resolvedPluginId = !symbolicName.isEmpty()
                ? QStringLiteral("symbolic:%1").arg(symbolicName.trimmed())
                : QStringLiteral("unknown_plugin");
        }

        if (!contextsByPluginId.contains(resolvedPluginId)) {
            PluginContext context;
            context.snapshot.pluginId = resolvedPluginId;
            context.snapshot.symbolicName = symbolicName.trimmed();
            context.snapshot.displayName = symbolicName.trimmed();
            contextsByPluginId.insert(resolvedPluginId, context);
            if (!symbolicName.isEmpty()) {
                pluginIdBySymbolicName.insert(symbolicName.trimmed(), resolvedPluginId);
            }
        }

        auto& context = contextsByPluginId[resolvedPluginId];
        if (context.snapshot.symbolicName.isEmpty()) {
            context.snapshot.symbolicName = symbolicName.trimmed();
        }
        if (context.snapshot.displayName.isEmpty()) {
            context.snapshot.displayName = symbolicName.trimmed();
        }
        return context;
    };

    for (const auto& symbolicName : observation.installedPlugins) {
        auto& context = ensureContext(QString(), symbolicName);
        context.snapshot.state = promoteState(context.snapshot.state, PlatformPluginState::Installed);
    }
    for (const auto& symbolicName : observation.startedPlugins) {
        auto& context = ensureContext(QString(), symbolicName);
        context.snapshot.state = promoteState(context.snapshot.state, PlatformPluginState::Starting);
    }
    for (const auto& symbolicName : observation.loadedPlugins) {
        auto& context = ensureContext(QString(), symbolicName);
        context.snapshot.state = promoteState(context.snapshot.state, PlatformPluginState::Starting);
    }
    for (auto it = observation.pluginStates.constBegin(); it != observation.pluginStates.constEnd(); ++it) {
        auto& context = ensureContext(QString(), it.key());
        context.snapshot.state = promoteState(context.snapshot.state, stateFromRuntimeText(it.value()));
    }

    qint64 readyPathEndMs = 0;
    qint64 warmupEndMs = 0;
    qint64 fullObservedStartupMs = 0;
    qint64 slowestPhaseDurationMs = -1;
    qint64 failureOffsetMs = std::numeric_limits<qint64>::max();
    qint64 degradedOffsetMs = std::numeric_limits<qint64>::max();
    qint64 firstFailedBlockingOffsetMs = std::numeric_limits<qint64>::max();
    qint64 firstFailedBlockingDurationMs = -1;
    qint64 longestBlockingDurationMs = -1;
    qint64 longestBlockingOffsetMs = std::numeric_limits<qint64>::max();
    QString blockingSpanLabel;
    QString failurePointLabel;
    QString degradedPointLabel;
    QString firstFailedBlockingLabel;
    QString longestBlockingLabel;
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    bool runtimeModeObserved = false;

    for (const auto& event : lifecycleEvents) {
        if (!runtimeModeObserved) {
            runtimeMode = event.runtimeMode;
            runtimeModeObserved = true;
        }

        const auto eventEndMs = event.offsetMs;
        fullObservedStartupMs = qMax(fullObservedStartupMs, eventEndMs);
        if (event.kind == PlatformLifecycleEventKind::PhaseFinished
            && event.durationMs >= slowestPhaseDurationMs) {
            slowestPhaseDurationMs = event.durationMs;
            aggregation.summary.slowestPhaseKey = !event.phaseKey.isEmpty() ? event.phaseKey : event.phaseLabel;
        }

        const bool pluginRelated = !event.pluginId.isEmpty()
            || !event.symbolicName.isEmpty()
            || event.step != PlatformLifecycleStep::None;
        if (!pluginRelated) continue;

        auto& context = ensureContext(event.pluginId, event.symbolicName);
        if (!event.reasonCode.isEmpty()) context.snapshot.lastReasonCode = event.reasonCode;
        if (!event.detail.isEmpty()) context.snapshot.lastDetail = event.detail;
        appendAllUnique(context.snapshot.missingRequiredServices, event.missingServices);
        appendAllUnique(context.snapshot.missingRequiredCapabilities, event.missingCapabilities);
        appendAllUnique(context.snapshot.missingRequiredPlugins, event.missingPlugins);
        auto eventRecoveryHints = stableRecoveryHintsForReasonCode(event.reasonCode);
        appendAllUnique(eventRecoveryHints, event.recoveryHints);
        appendAllUnique(context.snapshot.recoveryHints, eventRecoveryHints);
        context.snapshot.startupBlocked = context.snapshot.startupBlocked || isReadyPathBlockingEvent(event);

        if (isFailedOrTimedOut(event.result)) {
            context.snapshot.state = PlatformPluginState::Failed;
            appendUnique(
                context.snapshot.failureReasons,
                !event.reasonCode.isEmpty() ? event.reasonCode : event.detail);
        } else if (event.result == PlatformLifecycleResult::Degraded
                   && context.snapshot.state != PlatformPluginState::Failed) {
            context.snapshot.state = PlatformPluginState::Degraded;
            appendUnique(
                context.snapshot.degradedReasons,
                !event.reasonCode.isEmpty() ? event.reasonCode : event.detail);
        }

        if (event.step == PlatformLifecycleStep::Install && event.durationMs > 0) {
            context.snapshot.installMs = qMax(context.snapshot.installMs, event.durationMs);
            if (context.snapshot.state == PlatformPluginState::Discovered) {
                context.snapshot.state = PlatformPluginState::Installed;
            }
        }
        if (event.step == PlatformLifecycleStep::Start && event.durationMs > 0) {
            context.snapshot.startMs = qMax(context.snapshot.startMs, event.durationMs);
            if (context.snapshot.state == PlatformPluginState::Installed
                || context.snapshot.state == PlatformPluginState::Discovered) {
                context.snapshot.state = PlatformPluginState::Starting;
            }
        }
        if (event.step == PlatformLifecycleStep::ServiceReady && event.durationMs > 0) {
            context.snapshot.serviceReadyMs = qMax(context.snapshot.serviceReadyMs, event.durationMs);
        }
        if (event.step == PlatformLifecycleStep::Warmup && event.durationMs > 0) {
            context.snapshot.warmupMs = qMax(context.snapshot.warmupMs, event.durationMs);
        }

        if (event.step == PlatformLifecycleStep::ServiceReady
            && event.result == PlatformLifecycleResult::Succeeded) {
            context.snapshot.serviceReadyObserved = true;
            if (context.snapshot.state != PlatformPluginState::Failed
                && context.snapshot.state != PlatformPluginState::Degraded) {
                context.snapshot.state = PlatformPluginState::Ready;
            }
        }
        if (event.step == PlatformLifecycleStep::Warmup
            && event.result == PlatformLifecycleResult::Succeeded) {
            context.snapshot.warmupCompleted = true;
        }

        if (isReadyPathBlockingEvent(event)) {
            readyPathEndMs = qMax(readyPathEndMs, eventEndMs);

            if (isFailedOrTimedOut(event.result)
                && (event.offsetMs < firstFailedBlockingOffsetMs
                    || (event.offsetMs == firstFailedBlockingOffsetMs
                        && event.durationMs > firstFailedBlockingDurationMs))) {
                firstFailedBlockingOffsetMs = event.offsetMs;
                firstFailedBlockingDurationMs = event.durationMs;
                firstFailedBlockingLabel = toEventLabel(event);
            }

            if (event.durationMs > longestBlockingDurationMs
                || (event.durationMs == longestBlockingDurationMs
                    && event.offsetMs < longestBlockingOffsetMs)) {
                longestBlockingDurationMs = event.durationMs;
                longestBlockingOffsetMs = event.offsetMs;
                longestBlockingLabel = toEventLabel(event);
            }
        }

        if (event.step == PlatformLifecycleStep::Warmup
            && event.result != PlatformLifecycleResult::Running) {
            warmupEndMs = qMax(warmupEndMs, eventEndMs);
        }

        if (isFailedOrTimedOut(event.result) && event.offsetMs <= failureOffsetMs) {
            failureOffsetMs = event.offsetMs;
            failurePointLabel = toEventLabel(event);
        }
        if (event.result == PlatformLifecycleResult::Degraded && event.offsetMs <= degradedOffsetMs) {
            degradedOffsetMs = event.offsetMs;
            degradedPointLabel = toEventLabel(event);
        }

        const bool shouldCreateProblem = event.result == PlatformLifecycleResult::Failed
            || event.result == PlatformLifecycleResult::Timeout
            || event.result == PlatformLifecycleResult::Degraded
            || event.kind == PlatformLifecycleEventKind::PluginSkippedByMode;
        if (!shouldCreateProblem) continue;

        PlatformDiagnosticProblem problem;
        problem.severity = severityForEvent(event);
        problem.pluginId = context.snapshot.pluginId;
        problem.phaseKey = event.phaseKey;
        problem.step = event.step;
        problem.reasonCode = event.reasonCode;
        problem.detail = event.detail;
        problem.blockingStartup = event.blockingStartup;
        problem.firstOffsetMs = qMax<qint64>(0, event.offsetMs - event.durationMs);
        problem.impactCapabilities = context.impactCapabilities;
        problem.recoveryHints = eventRecoveryHints;
        aggregation.problems.append(problem);
    }

    aggregation.summary.runtimeMode = runtimeMode;
    aggregation.summary.frameworkReady = observation.frameworkReady;
    aggregation.summary.startupReadyPathMs = readyPathEndMs;
    aggregation.summary.startupWarmupTailMs = warmupEndMs > readyPathEndMs
        ? warmupEndMs - readyPathEndMs
        : 0;
    aggregation.summary.fullObservedStartupMs = qMax(fullObservedStartupMs, qMax(readyPathEndMs, warmupEndMs));
    blockingSpanLabel = !firstFailedBlockingLabel.isEmpty() ? firstFailedBlockingLabel : longestBlockingLabel;
    aggregation.summary.blockingSpanLabel = blockingSpanLabel.isEmpty() ? QStringLiteral("none") : blockingSpanLabel;
    aggregation.summary.failurePointLabel = failurePointLabel.isEmpty()
        ? (degradedPointLabel.isEmpty() ? QStringLiteral("none") : degradedPointLabel)
        : failurePointLabel;

    QStringList recoveryHints;
    PlatformPluginLifecycleSnapshot slowestPluginCandidate;
    bool hasSlowestPluginCandidate = false;
    for (auto it = contextsByPluginId.begin(); it != contextsByPluginId.end(); ++it) {
        auto& snapshot = it.value().snapshot;
        snapshot.blockingMs = snapshot.installMs + snapshot.startMs + snapshot.serviceReadyMs;

        qint64 slowestDuration = snapshot.installMs;
        snapshot.slowestStep = snapshot.installMs > 0 ? PlatformLifecycleStep::Install : PlatformLifecycleStep::None;
        if (snapshot.startMs > slowestDuration) {
            slowestDuration = snapshot.startMs;
            snapshot.slowestStep = PlatformLifecycleStep::Start;
        }
        if (snapshot.serviceReadyMs > slowestDuration) {
            slowestDuration = snapshot.serviceReadyMs;
            snapshot.slowestStep = PlatformLifecycleStep::ServiceReady;
        }
        if (snapshot.warmupMs > slowestDuration) {
            snapshot.slowestStep = PlatformLifecycleStep::Warmup;
        }

        if (snapshot.serviceReadyObserved
            && snapshot.state != PlatformPluginState::Failed
            && snapshot.state != PlatformPluginState::Degraded) {
            snapshot.state = PlatformPluginState::Ready;
        }

        if (snapshot.state == PlatformPluginState::Failed && snapshot.recoveryHints.isEmpty()) {
            appendUnique(
                snapshot.recoveryHints,
                QStringLiteral("Check plugin installation and startup logs, then fix the failure reason."));
        }
        if (snapshot.state == PlatformPluginState::Degraded && snapshot.recoveryHints.isEmpty()) {
            appendUnique(
                snapshot.recoveryHints,
                QStringLiteral("Check missing dependencies and evaluate degraded-mode readiness."));
        }

        appendAllUnique(recoveryHints, snapshot.recoveryHints);

        if (!hasSlowestPluginCandidate
            || isBetterSlowestPluginCandidate(snapshot, slowestPluginCandidate)) {
            slowestPluginCandidate = snapshot;
            hasSlowestPluginCandidate = true;
        }

        aggregation.pluginLifecycle.append(snapshot);
    }

    if (hasSlowestPluginCandidate) {
        aggregation.summary.slowestPluginId = slowestPluginCandidate.pluginId;
    } else if (!aggregation.pluginLifecycle.isEmpty()) {
        aggregation.summary.slowestPluginId = aggregation.pluginLifecycle.constFirst().pluginId;
    }

    std::sort(
        aggregation.pluginLifecycle.begin(),
        aggregation.pluginLifecycle.end(),
        [](const PlatformPluginLifecycleSnapshot& lhs, const PlatformPluginLifecycleSnapshot& rhs) {
            const auto lhsRank = stateSeverityRank(lhs.state);
            const auto rhsRank = stateSeverityRank(rhs.state);
            if (lhsRank != rhsRank) return lhsRank > rhsRank;
            if (lhs.blockingMs != rhs.blockingMs) return lhs.blockingMs > rhs.blockingMs;
            return lhs.pluginId < rhs.pluginId;
        });

    std::sort(
        aggregation.problems.begin(),
        aggregation.problems.end(),
        [](const PlatformDiagnosticProblem& lhs, const PlatformDiagnosticProblem& rhs) {
            if (lhs.severity != rhs.severity) return lhs.severity > rhs.severity;
            if (lhs.blockingStartup != rhs.blockingStartup) return lhs.blockingStartup;
            return lhs.firstOffsetMs < rhs.firstOffsetMs;
        });

    for (const auto& problem : aggregation.problems) appendAllUnique(recoveryHints, problem.recoveryHints);

    aggregation.summary.platformReady = observation.frameworkReady;
    for (const auto& plugin : aggregation.pluginLifecycle) {
        if (plugin.state == PlatformPluginState::Failed || plugin.state == PlatformPluginState::Degraded) {
            aggregation.summary.platformReady = false;
            break;
        }
    }

    aggregation.recoveryHints = recoveryHints;
    return aggregation;
}
