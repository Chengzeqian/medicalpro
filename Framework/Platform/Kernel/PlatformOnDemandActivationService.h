#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h"
#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"

class PlatformStateStore;

class FRAMEWORK_EXPORT PlatformOnDemandActivationService
{
public:
    using InstallPluginFn = std::function<bool(const PlatformOnDemandActivationPlanEntry&)>;

    PlatformOnDemandActivationService(
        QVector<PlatformPluginDescriptor> descriptors,
        QString pluginDirectory,
        PlatformStartupCoordinator* startupCoordinator,
        PlatformStateStore* stateStore,
        InstallPluginFn installPluginFn,
        PlatformOnDemandProbeSet probeSet);

    void setStateStore(PlatformStateStore* stateStore);
    bool ensureReady(const QString& pluginId);

private:
    QVector<PlatformPluginDescriptor> m_descriptors;
    QString m_pluginDirectory;
    PlatformStartupCoordinator* m_startupCoordinator = nullptr;
    PlatformStateStore* m_stateStore = nullptr;
    InstallPluginFn m_installPluginFn;
    PlatformOnDemandProbeSet m_probeSet;
};
