#include "Framework/Platform/Kernel/PlatformStateStore.h"

void PlatformStateStore::replaceDescriptors(const QVector<PlatformPluginDescriptor>& descriptors)
{
    m_descriptorOrder.clear();
    m_descriptors.clear();
    m_snapshots.clear();

    for (const auto& descriptor : descriptors) {
        m_descriptorOrder.append(descriptor.id);
        m_descriptors.insert(descriptor.id, descriptor);

        PlatformPluginRuntimeSnapshot snapshot;
        snapshot.pluginId = descriptor.id;
        snapshot.ctkSymbolicName = descriptor.runtime.ctkSymbolicName;
        m_snapshots.insert(descriptor.id, snapshot);
    }

    refreshSnapshots();
}

void PlatformStateStore::setRuntimeMode(PlatformRuntimeMode runtimeMode)
{
    m_runtimeMode = runtimeMode;
}

void PlatformStateStore::setPluginState(const QString& pluginId, PlatformPluginState state)
{
    if (!m_snapshots.contains(pluginId)) return;

    auto snapshot = m_snapshots.value(pluginId);
    snapshot.state = state;
    m_snapshots.insert(pluginId, snapshot);
    refreshSnapshots();
}

PlatformCapabilitySnapshot PlatformStateStore::capabilitySnapshot() const
{
    PlatformCapabilitySnapshot snapshot;
    snapshot.runtimeMode = m_runtimeMode;

    for (const auto& pluginId : m_descriptorOrder) {
        const auto descriptor = m_descriptors.value(pluginId);
        const auto pluginSnapshot = m_snapshots.value(pluginId);
        const bool ready = pluginSnapshot.state == PlatformPluginState::Ready;

        if (pluginSnapshot.state == PlatformPluginState::Degraded
            || pluginSnapshot.state == PlatformPluginState::Failed) {
            snapshot.degradedPlugins.append(pluginId);
        }

        for (const auto& capability : descriptor.provides.capabilities) {
            if (ready) snapshot.unlockedCapabilities.append(capability);
            else snapshot.lockedCapabilities.append(capability);
        }
    }

    snapshot.platformReady = snapshot.lockedCapabilities.isEmpty() && snapshot.degradedPlugins.isEmpty();
    return snapshot;
}

QVector<PlatformPluginRuntimeSnapshot> PlatformStateStore::pluginSnapshots() const
{
    QVector<PlatformPluginRuntimeSnapshot> snapshots;
    snapshots.reserve(m_descriptorOrder.size());

    for (const auto& pluginId : m_descriptorOrder) {
        snapshots.append(m_snapshots.value(pluginId));
    }

    return snapshots;
}

bool PlatformStateStore::isCapabilityUnlocked(const QString& capability) const
{
    for (const auto& pluginId : m_descriptorOrder) {
        const auto descriptor = m_descriptors.value(pluginId);
        if (!descriptor.provides.capabilities.contains(capability)) continue;
        if (isPluginReady(pluginId)) return true;
    }

    return false;
}

bool PlatformStateStore::isPluginReady(const QString& pluginId) const
{
    return m_snapshots.value(pluginId).state == PlatformPluginState::Ready;
}

void PlatformStateStore::refreshSnapshots()
{
    for (const auto& pluginId : m_descriptorOrder) {
        refreshSnapshot(pluginId);
    }
}

void PlatformStateStore::refreshSnapshot(const QString& pluginId)
{
    auto snapshot = m_snapshots.value(pluginId);
    const auto descriptor = m_descriptors.value(pluginId);

    snapshot.missingRequiredServices.clear();
    snapshot.missingRequiredCapabilities.clear();
    snapshot.missingRequiredPlugins.clear();

    for (const auto& requiredCapability : descriptor.required.capabilities) {
        if (!isCapabilityUnlocked(requiredCapability)) snapshot.missingRequiredCapabilities.append(requiredCapability);
    }

    for (const auto& requiredPluginId : descriptor.required.plugins) {
        if (!isPluginReady(requiredPluginId)) snapshot.missingRequiredPlugins.append(requiredPluginId);
    }

    m_snapshots.insert(pluginId, snapshot);
}
