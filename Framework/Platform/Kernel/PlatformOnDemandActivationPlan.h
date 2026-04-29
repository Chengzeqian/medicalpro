#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"

#include <functional>
#include <QVector>

struct PlatformOnDemandActivationPlanEntry
{
    QString pluginId;
    QString displayName;
    QString symbolicName;
    QString bundleFilePath;
    QStringList requiredPlugins;
    QStringList requiredCapabilities;
    QStringList requiredServices;
    QStringList healthChecks;
    int serviceReadyTimeoutMs = 0;
    bool target = false;
    bool requiresBundleInstall = true;
};

struct PlatformOnDemandActivationPlan
{
    QString targetPluginId;
    QVector<PlatformOnDemandActivationPlanEntry> activationEntries;
};

class FRAMEWORK_EXPORT PlatformOnDemandActivationPlanBuilder
{
public:
    using PlatformModuleAvailabilityFn = std::function<bool(const QString& symbolicName)>;

    static PlatformOnDemandActivationPlan build(
        const QString& targetPluginId,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& pluginDirectory,
        QString* error = nullptr);

    static PlatformOnDemandActivationPlan build(
        const QString& targetPluginId,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& pluginDirectory,
        const PlatformModuleAvailabilityFn& isPlatformModuleAvailable,
        QString* error = nullptr);
};
