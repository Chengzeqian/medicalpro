#include "Framework/Platform/Kernel/PlatformOnDemandActivationService.h"

#include "Framework/Platform/Kernel/PlatformStateStore.h"

#include <utility>

PlatformOnDemandActivationService::PlatformOnDemandActivationService(
    QVector<PlatformPluginDescriptor> descriptors,
    QString pluginDirectory,
    PlatformStartupCoordinator* startupCoordinator,
    PlatformStateStore* stateStore,
    InstallPluginFn installPluginFn,
    PlatformOnDemandProbeSet probeSet,
    PlatformModuleAvailabilityFn isPlatformModuleAvailable)
    : m_descriptors(std::move(descriptors))
    , m_pluginDirectory(std::move(pluginDirectory))
    , m_startupCoordinator(startupCoordinator)
    , m_stateStore(stateStore)
    , m_installPluginFn(std::move(installPluginFn))
    , m_probeSet(std::move(probeSet))
    , m_isPlatformModuleAvailable(std::move(isPlatformModuleAvailable))
{
}

void PlatformOnDemandActivationService::setStateStore(PlatformStateStore* stateStore)
{
    m_stateStore = stateStore;
}

bool PlatformOnDemandActivationService::ensureReady(const QString& pluginId)
{
    const auto normalizedPluginId = pluginId.trimmed();
    if (normalizedPluginId.isEmpty() || !m_startupCoordinator) {
        if (m_stateStore && !normalizedPluginId.isEmpty()) {
            m_stateStore->setPluginState(normalizedPluginId, PlatformPluginState::Failed);
        }
        return false;
    }

    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        normalizedPluginId,
        m_descriptors,
        m_pluginDirectory,
        m_isPlatformModuleAvailable,
        &error);
    if (!error.isEmpty() || plan.activationEntries.isEmpty()) {
        if (m_stateStore) {
            m_stateStore->setPluginState(normalizedPluginId, PlatformPluginState::Failed);
        }
        return false;
    }

    const auto outcome = m_startupCoordinator->activateOnDemand(
        plan,
        m_installPluginFn,
        m_probeSet);

    if (m_stateStore) {
        m_stateStore->setPluginState(normalizedPluginId, outcome.finalState);
    }

    return outcome.success;
}
