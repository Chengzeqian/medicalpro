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
    QString ctkSymbolicName;
    PlatformStartupPolicy startupPolicy = PlatformStartupPolicy::Disabled;
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred;
    QString entryCapability;
};

struct PlatformPluginDescriptor
{
    QString id;
    QString version;
    QString displayName;
    QString domain;
    bool enabled = true;
    PlatformRuntimeDescriptor runtime;
    PlatformServiceSet provides;
    PlatformServiceSet required;
    PlatformServiceSet optional;
    QStringList healthChecks;
};
