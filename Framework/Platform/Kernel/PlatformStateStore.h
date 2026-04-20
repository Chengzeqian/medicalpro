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
    void setPluginState(const QString& pluginId, PlatformPluginState state);
    QVector<PlatformPluginDescriptor> descriptors() const;
    PlatformCapabilitySnapshot capabilitySnapshot() const;
    QVector<PlatformPluginRuntimeSnapshot> pluginSnapshots() const;

private:
    bool isCapabilityUnlocked(const QString& capability) const;
    bool isPluginReady(const QString& pluginId) const;
    void refreshSnapshots();
    void refreshSnapshot(const QString& pluginId);

    PlatformRuntimeMode m_runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QStringList m_descriptorOrder;
    QHash<QString, PlatformPluginDescriptor> m_descriptors;
    QHash<QString, PlatformPluginRuntimeSnapshot> m_snapshots;
    mutable QReadWriteLock m_lock;
};
