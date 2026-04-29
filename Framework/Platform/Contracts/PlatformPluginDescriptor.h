#pragma once

#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"

#include <QString>
#include <QStringList>

struct PlatformServiceSet
{
    QStringList services;
    QStringList capabilities;
    QStringList plugins;
};

struct PlatformRuntimeDescriptor
{
    QString symbolicName;
    PlatformStartupPolicy startupPolicy = PlatformStartupPolicy::Disabled;
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred;
    QString entryCapability;
};

struct PlatformDiagnosticsDescriptor
{
    QStringList requiredServices;
    int serviceReadyTimeoutMs = 0;
    QStringList warmupTasks;
    int warmupTimeoutMs = 0;
    bool warmupImpactsReady = false;
    QStringList degradeOn;
};

struct PlatformPluginDescriptor
{
    QString id;
    QString version;
    QString displayName;
    QString domain;
    bool enabled = true;
    PlatformRuntimeDescriptor runtime;
    PlatformDiagnosticsDescriptor diagnostics;
    PlatformServiceSet provides;
    PlatformServiceSet required;
    PlatformServiceSet optional;
    QStringList healthChecks;
};
