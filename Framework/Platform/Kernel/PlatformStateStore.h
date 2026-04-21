#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

#include <QHash>
#include <QReadWriteLock>
#include <QStringList>
#include <QVector>

class FRAMEWORK_EXPORT PlatformStateStore
{
public:
    void replaceDescriptors(const QVector<PlatformPluginDescriptor>& descriptors);
    void setRuntimeMode(PlatformRuntimeMode runtimeMode);
    void setStartupScopePluginIds(const QStringList& pluginIds);
    QStringList startupScopePluginIds() const;
    void setGovernedPluginIds(const QStringList& pluginIds);
    QStringList governedPluginIds() const;
    void setManagedPluginIds(const QStringList& pluginIds);
    QStringList managedPluginIds() const;
    void setPluginState(const QString& pluginId, PlatformPluginState state);
    QVector<PlatformPluginDescriptor> descriptors() const;
    PlatformCapabilitySnapshot capabilitySnapshot() const;
    QVector<PlatformPluginRuntimeSnapshot> pluginSnapshots() const;

private:
    bool isStartupScopePlugin(const QString& pluginId) const;
    bool isGovernedPlugin(const QString& pluginId) const;
    bool isCapabilityUnlocked(const QString& capability) const;
    bool isPluginReady(const QString& pluginId) const;
    void refreshSnapshots();
    void refreshSnapshot(const QString& pluginId);

    PlatformRuntimeMode m_runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QStringList m_descriptorOrder;
    QStringList m_startupScopePluginIds;
    QStringList m_governedPluginIds;
    QHash<QString, PlatformPluginDescriptor> m_descriptors;
    QHash<QString, PlatformPluginRuntimeSnapshot> m_snapshots;
    mutable QReadWriteLock m_lock;
};
