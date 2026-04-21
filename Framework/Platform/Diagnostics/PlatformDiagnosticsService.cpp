#include "Framework/Platform/Diagnostics/PlatformDiagnosticsService.h"

#include "Framework/Platform/Kernel/PlatformStateStore.h"

#include <QHash>
#include <QtGlobal>

#include <algorithm>

namespace
{
void appendUnique(QStringList& target, const QString& value)
{
    if (value.isEmpty()) return;
    if (!target.contains(value)) target.append(value);
}

QHash<QString, QString> pluginIdBySymbolicName(const QVector<PlatformPluginDescriptor>& descriptors)
{
    QHash<QString, QString> mapping;
    mapping.reserve(descriptors.size());
    for (const auto& descriptor : descriptors) {
        const auto ctkSymbolicName = descriptor.runtime.ctkSymbolicName.trimmed();
        if (ctkSymbolicName.isEmpty()) continue;
        mapping.insert(ctkSymbolicName, descriptor.id);
    }
    return mapping;
}

QString problemDedupeKey(const PlatformDiagnosticProblem& problem)
{
    return QStringLiteral("%1|%2|%3|%4")
        .arg(problem.reasonCode, problem.pluginId, QString::number(static_cast<int>(problem.step)), problem.detail);
}

int severityRank(PlatformDiagnosticSeverity severity)
{
    switch (severity) {
    case PlatformDiagnosticSeverity::Critical:
        return 4;
    case PlatformDiagnosticSeverity::Error:
        return 3;
    case PlatformDiagnosticSeverity::Warning:
        return 2;
    case PlatformDiagnosticSeverity::Info:
        return 1;
    }
    return 0;
}

void sortProblems(QVector<PlatformDiagnosticProblem>& problems)
{
    std::sort(
        problems.begin(),
        problems.end(),
        [](const PlatformDiagnosticProblem& lhs, const PlatformDiagnosticProblem& rhs) {
            const auto lhsSeverity = severityRank(lhs.severity);
            const auto rhsSeverity = severityRank(rhs.severity);
            if (lhsSeverity != rhsSeverity) return lhsSeverity > rhsSeverity;
            if (lhs.blockingStartup != rhs.blockingStartup) return lhs.blockingStartup;
            if (lhs.firstOffsetMs != rhs.firstOffsetMs) return lhs.firstOffsetMs < rhs.firstOffsetMs;
            return lhs.pluginId < rhs.pluginId;
        });
}
}

PlatformDiagnosticsService::PlatformDiagnosticsService(PlatformStateStore* stateStore)
    : m_stateStore(stateStore)
{
}

PlatformDiagnosticSnapshot PlatformDiagnosticsService::buildSnapshot(const PlatformRuntimeObservation& observation) const
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.startupTrace = observation.startupTrace;
    snapshot.frameworkReady = observation.frameworkReady;

    QVector<PlatformPluginDescriptor> descriptors;
    QStringList startupScopePluginIds;
    QStringList governedPluginIds;
    if (m_stateStore) {
        descriptors = m_stateStore->descriptors();
        startupScopePluginIds = m_stateStore->startupScopePluginIds();
        governedPluginIds = m_stateStore->governedPluginIds();
        snapshot.plugins = m_stateStore->pluginSnapshots();
        snapshot.capabilitySnapshot = m_stateStore->capabilitySnapshot();
        snapshot.runtimeMode = snapshot.capabilitySnapshot.runtimeMode;
    }

    if (governedPluginIds.isEmpty()) {
        for (const auto& descriptor : descriptors) {
            governedPluginIds.append(descriptor.id);
        }
    }

    snapshot.startupScopePluginIds = startupScopePluginIds;
    snapshot.governedPluginIds = governedPluginIds;
    snapshot.managedPluginIds = startupScopePluginIds;
    for (const auto& descriptor : descriptors) {
        if (governedPluginIds.contains(descriptor.id)) continue;

        snapshot.excludedPluginIds.append(descriptor.id);

        PlatformDiagnosticProblem problem;
        problem.severity = PlatformDiagnosticSeverity::Info;
        problem.pluginId = descriptor.id;
        problem.reasonCode = QStringLiteral("excluded_from_governed_scope");
        problem.detail = QStringLiteral("Plugin is available in descriptors but excluded from the current governed scope");
        snapshot.problems.append(problem);
    }

    const auto aggregation = m_aggregator.aggregate(observation.lifecycleEvents, descriptors, observation);
    snapshot.summary = aggregation.summary;
    snapshot.pluginLifecycle = aggregation.pluginLifecycle;
    snapshot.problems += aggregation.problems;
    snapshot.recoveryHints = aggregation.recoveryHints;

    if (m_stateStore) {
        snapshot.summary.runtimeMode = snapshot.runtimeMode;
        snapshot.summary.platformReady = snapshot.capabilitySnapshot.platformReady;
    } else {
        snapshot.runtimeMode = snapshot.summary.runtimeMode;
    }
    snapshot.summary.frameworkReady = observation.frameworkReady;
    snapshot.frameworkReady = snapshot.summary.frameworkReady;
    snapshot.runtimeMode = snapshot.summary.runtimeMode;

    if (snapshot.recoveryHints.isEmpty()
        && !observation.startupTrace.isEmpty()
        && !observation.startupTrace.constLast().success) {
        appendUnique(snapshot.recoveryHints, observation.startupTrace.constLast().detail);
    }

    for (const auto& plugin : snapshot.plugins) {
        if (plugin.missingRequiredServices.isEmpty()) continue;
        appendUnique(
            snapshot.recoveryHints,
            QStringLiteral("%1 missing services: %2")
                .arg(plugin.ctkSymbolicName, plugin.missingRequiredServices.join(QStringLiteral(", "))));
    }

    const auto symbolicNameToPluginId = pluginIdBySymbolicName(descriptors);
    QHash<QString, PlatformPluginState> platformStateByPluginId;
    platformStateByPluginId.reserve(snapshot.plugins.size());
    for (const auto& plugin : snapshot.plugins) {
        platformStateByPluginId.insert(plugin.pluginId, plugin.state);
    }

    for (const auto& startedSymbolicName : observation.startedPlugins) {
        const auto pluginId = symbolicNameToPluginId.value(startedSymbolicName);
        if (pluginId.isEmpty()) continue;
        if (!platformStateByPluginId.contains(pluginId)) continue;

        const auto platformState = platformStateByPluginId.value(pluginId);
        const auto ctkState = observation.pluginStates.value(startedSymbolicName);
        const bool ctkLooksActive = ctkState.compare(QStringLiteral("ACTIVE"), Qt::CaseInsensitive) == 0
            || ctkState.isEmpty();
        const bool platformLooksReady = platformState == PlatformPluginState::Ready
            || platformState == PlatformPluginState::Starting;
        if (!ctkLooksActive || platformLooksReady) continue;

        PlatformDiagnosticProblem problem;
        problem.severity = PlatformDiagnosticSeverity::Error;
        problem.pluginId = pluginId;
        problem.step = PlatformLifecycleStep::Start;
        problem.reasonCode = QStringLiteral("ctk_platform_state_mismatch");
        problem.detail = QStringLiteral("CTK reports plugin %1 as started but platform state is %2")
                             .arg(startedSymbolicName)
                             .arg(static_cast<int>(platformState));
        problem.recoveryHints = QStringList{
            QStringLiteral("Check platform state update chain for missing install/start/service_ready events.")
        };

        const auto dedupeKey = problemDedupeKey(problem);
        bool exists = false;
        for (const auto& existing : snapshot.problems) {
            if (problemDedupeKey(existing) != dedupeKey) continue;
            exists = true;
            break;
        }
        if (!exists) snapshot.problems.append(problem);
        appendUnique(snapshot.recoveryHints, problem.recoveryHints.constFirst());
    }

    sortProblems(snapshot.problems);
    QStringList dedupedHints;
    for (const auto& hint : snapshot.recoveryHints) {
        appendUnique(dedupedHints, hint);
    }
    snapshot.recoveryHints = dedupedHints;

    return snapshot;
}
