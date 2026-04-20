#pragma once

#include <QString>

enum class PlatformStartupPolicy
{
    Eager,
    OnDemand,
    Disabled
};

enum class PlatformBootstrapLevel
{
    Core,
    Deferred
};

enum class PlatformPluginState
{
    Discovered,
    Installed,
    Starting,
    Ready,
    Degraded,
    Failed
};

enum class PlatformRuntimeMode
{
    ObserveOnly,
    FacadeMode,
    OrchestrateCore
};

enum class PlatformLifecycleEventKind
{
    StartupSessionStarted,
    StartupSessionFinished,
    PhaseStarted,
    PhaseFinished,
    PluginInstallStarted,
    PluginInstallFinished,
    PluginStartStarted,
    PluginStartFinished,
    PluginServiceReady,
    PluginWarmupStarted,
    PluginWarmupFinished,
    PluginFailed,
    PluginDegraded,
    PluginSkippedByMode
};

enum class PlatformLifecycleStep
{
    None,
    Install,
    Start,
    ServiceReady,
    Warmup
};

enum class PlatformLifecycleResult
{
    Running,
    Succeeded,
    Failed,
    Degraded,
    Skipped,
    Timeout
};

enum class PlatformDiagnosticSeverity
{
    Info,
    Warning,
    Error,
    Critical
};

struct PlatformHealthCheckResult
{
    QString name;
    bool passed = false;
    QString detail;
};
