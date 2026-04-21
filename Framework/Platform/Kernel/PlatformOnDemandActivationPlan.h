#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"

#include <QVector>

struct PlatformOnDemandActivationPlanEntry
{
    QString pluginId;
    QString displayName;
    QString ctkSymbolicName;
    QString bundleFilePath;
    QStringList requiredPlugins;
    QStringList requiredCapabilities;
    QStringList requiredServices;
    QStringList healthChecks;
    int serviceReadyTimeoutMs = 0;
    bool target = false;
};

struct PlatformOnDemandActivationPlan
{
    QString targetPluginId;
    QVector<PlatformOnDemandActivationPlanEntry> activationEntries;
};

class FRAMEWORK_EXPORT PlatformOnDemandActivationPlanBuilder
{
public:
    static PlatformOnDemandActivationPlan build(
        const QString& targetPluginId,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const QString& pluginDirectory,
        QString* error = nullptr);
};
