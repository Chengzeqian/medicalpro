#include "Framework/Platform/Kernel/PlatformStateStore.h"

#include <QReadLocker>
#include <QWriteLocker>

void PlatformStateStore::replaceDescriptors(const QVector<PlatformPluginDescriptor>& descriptors)
{
    QWriteLocker locker(&m_lock);

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
    QWriteLocker locker(&m_lock);
    m_runtimeMode = runtimeMode;
}

void PlatformStateStore::setStartupScopePluginIds(const QStringList& pluginIds)
{
    QWriteLocker locker(&m_lock);
    m_startupScopePluginIds.clear();

    for (const auto& pluginId : pluginIds) {
        const auto normalizedPluginId = pluginId.trimmed();
        if (normalizedPluginId.isEmpty()) continue;
        if (m_startupScopePluginIds.contains(normalizedPluginId)) continue;
        m_startupScopePluginIds.append(normalizedPluginId);
    }

    refreshSnapshots();
}

QStringList PlatformStateStore::startupScopePluginIds() const
{
    QReadLocker locker(&m_lock);
    return m_startupScopePluginIds;
}

void PlatformStateStore::setGovernedPluginIds(const QStringList& pluginIds)
{
    QWriteLocker locker(&m_lock);
    m_governedPluginIds.clear();

    for (const auto& pluginId : pluginIds) {
        const auto normalizedPluginId = pluginId.trimmed();
        if (normalizedPluginId.isEmpty()) continue;
        if (m_governedPluginIds.contains(normalizedPluginId)) continue;
        m_governedPluginIds.append(normalizedPluginId);
    }

    refreshSnapshots();
}

QStringList PlatformStateStore::governedPluginIds() const
{
    QReadLocker locker(&m_lock);
    return m_governedPluginIds;
}

void PlatformStateStore::setManagedPluginIds(const QStringList& pluginIds)
{
    setStartupScopePluginIds(pluginIds);
}

QStringList PlatformStateStore::managedPluginIds() const
{
    return startupScopePluginIds();
}

void PlatformStateStore::setPluginState(const QString& pluginId, PlatformPluginState state)
{
    QWriteLocker locker(&m_lock);
    if (!m_snapshots.contains(pluginId)) return;

    auto snapshot = m_snapshots.value(pluginId);
    snapshot.state = state;
    m_snapshots.insert(pluginId, snapshot);
    refreshSnapshots();
}

QVector<PlatformPluginDescriptor> PlatformStateStore::descriptors() const
{
    QReadLocker locker(&m_lock);
    QVector<PlatformPluginDescriptor> descriptors;
    descriptors.reserve(m_descriptorOrder.size());

    for (const auto& pluginId : m_descriptorOrder) {
        if (!m_descriptors.contains(pluginId)) continue;
        descriptors.append(m_descriptors.value(pluginId));
    }

    return descriptors;
}

PlatformCapabilitySnapshot PlatformStateStore::capabilitySnapshot() const
{
    QReadLocker locker(&m_lock);
    PlatformCapabilitySnapshot snapshot;
    snapshot.runtimeMode = m_runtimeMode;
    snapshot.startupScopePluginIds = m_startupScopePluginIds;
    snapshot.governedPluginIds = m_governedPluginIds;

    QStringList startupLockedCapabilities;
    QStringList startupDegradedPlugins;

    for (const auto& pluginId : m_descriptorOrder) {
        if (!isGovernedPlugin(pluginId)) continue;

        const auto descriptor = m_descriptors.value(pluginId);
        const auto pluginSnapshot = m_snapshots.value(pluginId);
        const bool ready = pluginSnapshot.state == PlatformPluginState::Ready;

        for (const auto& capability : descriptor.provides.capabilities) {
            if (ready) snapshot.unlockedCapabilities.append(capability);
            else snapshot.lockedCapabilities.append(capability);
        }

        if (!isStartupScopePlugin(pluginId)) continue;
        if (pluginSnapshot.state == PlatformPluginState::Degraded
            || pluginSnapshot.state == PlatformPluginState::Failed) {
            startupDegradedPlugins.append(pluginId);
            snapshot.degradedPlugins.append(pluginId);
        }

        if (!ready) {
            startupLockedCapabilities.append(descriptor.provides.capabilities);
        }
    }

    snapshot.platformReady = startupLockedCapabilities.isEmpty() && startupDegradedPlugins.isEmpty();
    return snapshot;
}

QVector<PlatformPluginRuntimeSnapshot> PlatformStateStore::pluginSnapshots() const
{
    QReadLocker locker(&m_lock);
    QVector<PlatformPluginRuntimeSnapshot> snapshots;
    snapshots.reserve(m_descriptorOrder.size());

    for (const auto& pluginId : m_descriptorOrder) {
        snapshots.append(m_snapshots.value(pluginId));
    }

    return snapshots;
}

bool PlatformStateStore::isStartupScopePlugin(const QString& pluginId) const
{
    return m_startupScopePluginIds.isEmpty() || m_startupScopePluginIds.contains(pluginId);
}

bool PlatformStateStore::isGovernedPlugin(const QString& pluginId) const
{
    return m_governedPluginIds.isEmpty() || m_governedPluginIds.contains(pluginId);
}

bool PlatformStateStore::isCapabilityUnlocked(const QString& capability) const
{
    for (const auto& pluginId : m_descriptorOrder) {
        if (!isGovernedPlugin(pluginId)) continue;

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
