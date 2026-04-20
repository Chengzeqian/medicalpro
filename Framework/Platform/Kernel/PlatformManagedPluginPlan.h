#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

#include <QVector>

struct PlatformManagedPluginPlanEntry
{
    QString pluginId;
    QString displayName;
    QString ctkSymbolicName;
    QString bundleFilePath;
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred;
    PlatformStartupPolicy startupPolicy = PlatformStartupPolicy::Disabled;
    QStringList requiredPlugins;
    QStringList requiredCapabilities;
    QStringList requiredServices;
    QStringList healthChecks;
    int serviceReadyTimeoutMs = 0;
};

struct PlatformManagedPluginPlan
{
    PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QVector<PlatformManagedPluginPlanEntry> installEntries;
    QStringList managedPluginIds;
    QStringList corePluginIds;
};

class FRAMEWORK_EXPORT PlatformManagedPluginPlanBuilder
{
public:
    static PlatformManagedPluginPlan build(
        const PlatformRuntimeConfig& runtimeConfig,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& pluginDirectory,
        QString* error = nullptr);
};
