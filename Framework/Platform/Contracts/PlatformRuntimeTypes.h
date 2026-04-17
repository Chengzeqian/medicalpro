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

struct PlatformHealthCheckResult
{
    QString name;
    bool passed = false;
    QString detail;
};
