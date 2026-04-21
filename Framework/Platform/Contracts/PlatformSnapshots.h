#pragma once

#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

struct PlatformStartupTraceEntry
{
    PlatformStartupTraceEntry() = default;
    PlatformStartupTraceEntry(
        const QString& phaseKeyValue,
        const QString& phaseLabelValue,
        bool successValue,
        qint64 elapsedMsValue,
        const QString& detailValue)
        : phaseKey(phaseKeyValue)
        , phaseLabel(phaseLabelValue)
        , success(successValue)
        , elapsedMs(elapsedMsValue)
        , detail(detailValue)
    {
    }

    QString spanId;
    QString parentSpanId;
    QString phaseKey;
    QString phaseLabel;
    QString pluginId;
    QString ctkSymbolicName;
    PlatformLifecycleStep step = PlatformLifecycleStep::None;
    PlatformLifecycleResult result = PlatformLifecycleResult::Running;
    bool success = false;
    bool blockingStartup = false;
    qint64 startOffsetMs = 0;
    qint64 endOffsetMs = 0;
    qint64 elapsedMs = 0;
    QString reasonCode;
    QString detail;
};

struct PlatformLifecycleEvent
{
    QString sessionId;
    PlatformLifecycleEventKind kind = PlatformLifecycleEventKind::StartupSessionStarted;
    PlatformLifecycleStep step = PlatformLifecycleStep::None;
    PlatformLifecycleResult result = PlatformLifecycleResult::Running;
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QString phaseKey;
    QString phaseLabel;
    QString pluginId;
    QString ctkSymbolicName;
    qint64 offsetMs = 0;
    qint64 durationMs = 0;
    bool blockingStartup = false;
    bool critical = false;
    QString reasonCode;
    QString detail;
    QStringList missingServices;
    QStringList missingCapabilities;
    QStringList missingPlugins;
    QStringList recoveryHints;
};

struct PlatformPluginLifecycleSnapshot
{
    QString pluginId;
    QString ctkSymbolicName;
    QString displayName;
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred;
    PlatformStartupPolicy startupPolicy = PlatformStartupPolicy::Disabled;
    PlatformPluginState state = PlatformPluginState::Discovered;
    qint64 installMs = 0;
    qint64 startMs = 0;
    qint64 serviceReadyMs = 0;
    qint64 warmupMs = 0;
    qint64 blockingMs = 0;
    PlatformLifecycleStep slowestStep = PlatformLifecycleStep::None;
    bool serviceReadyObserved = false;
    bool warmupCompleted = false;
    bool startupBlocked = false;
    QString lastReasonCode;
    QString lastDetail;
    QStringList missingRequiredServices;
    QStringList missingRequiredCapabilities;
    QStringList missingRequiredPlugins;
    QStringList degradedReasons;
    QStringList failureReasons;
    QStringList recoveryHints;
};

struct PlatformPluginRuntimeSnapshot
{
    QString pluginId;
    QString ctkSymbolicName;
    PlatformPluginState state = PlatformPluginState::Discovered;
    QStringList missingRequiredServices;
    QStringList missingRequiredCapabilities;
    QStringList missingRequiredPlugins;
};

struct PlatformCapabilitySnapshot
{
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    bool platformReady = false;
    QStringList unlockedCapabilities;
    QStringList lockedCapabilities;
    QStringList degradedPlugins;
    QStringList startupScopePluginIds;
    QStringList governedPluginIds;
};

struct PlatformDiagnosticSummary
{
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    bool frameworkReady = false;
    bool platformReady = false;
    qint64 startupReadyPathMs = 0;
    qint64 startupWarmupTailMs = 0;
    qint64 fullObservedStartupMs = 0;
    QString slowestPhaseKey;
    QString slowestPluginId;
    QString blockingSpanLabel;
    QString failurePointLabel;
};

struct PlatformDiagnosticProblem
{
    PlatformDiagnosticSeverity severity = PlatformDiagnosticSeverity::Info;
    QString pluginId;
    QString phaseKey;
    PlatformLifecycleStep step = PlatformLifecycleStep::None;
    QString reasonCode;
    QString detail;
    bool blockingStartup = false;
    qint64 firstOffsetMs = 0;
    QStringList impactCapabilities;
    QStringList recoveryHints;
};

struct PlatformDiagnosticSnapshot
{
    PlatformDiagnosticSummary summary;
    PlatformCapabilitySnapshot capabilitySnapshot;
    QVector<PlatformPluginLifecycleSnapshot> pluginLifecycle;
    QVector<PlatformStartupTraceEntry> startupTrace;
    QVector<PlatformDiagnosticProblem> problems;
    QStringList recoveryHints;
    QStringList startupScopePluginIds;
    QStringList governedPluginIds;
    QStringList managedPluginIds;
    QStringList excludedPluginIds;

    // Compatibility bridge for existing callers until diagnostics page fully migrates.
    bool frameworkReady = false;
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QVector<PlatformPluginRuntimeSnapshot> plugins;
};
