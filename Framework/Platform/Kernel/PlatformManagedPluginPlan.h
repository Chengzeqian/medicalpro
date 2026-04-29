#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

#include <functional>
#include <QVector>

struct PlatformManagedPluginPlanEntry
{
    QString pluginId;
    QString displayName;
    QString symbolicName;
    QString bundleFilePath;
    bool requiresBundleInstall = true;
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
    using PlatformModuleAvailabilityFn = std::function<bool(const QString& symbolicName)>;

    static PlatformManagedPluginPlan build(
        const PlatformRuntimeConfig& runtimeConfig,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& pluginDirectory,
        QString* error = nullptr);

    static PlatformManagedPluginPlan build(
        const PlatformRuntimeConfig& runtimeConfig,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& pluginDirectory,
        const PlatformModuleAvailabilityFn& isPlatformModuleAvailable,
        QString* error = nullptr);
};
