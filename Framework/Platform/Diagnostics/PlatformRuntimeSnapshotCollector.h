#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

#include <QMap>
#include <QStringList>
#include <QVector>

struct FRAMEWORK_EXPORT PlatformRuntimeObservation
{
    bool frameworkReady = false;
    QStringList installedPlugins;
    QStringList startedPlugins;
    QStringList loadedPlugins;
    QMap<QString, QString> pluginStates;
    QVector<PlatformLifecycleEvent> lifecycleEvents;
    QVector<PlatformStartupTraceEntry> startupTrace;
};

class FRAMEWORK_EXPORT PlatformRuntimeSnapshotCollector
{
public:
    PlatformRuntimeObservation collect() const;
};
