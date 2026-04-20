#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

#include <QStringList>
#include <QVector>

struct FRAMEWORK_EXPORT PlatformRuntimeObservation
{
    bool frameworkReady = false;
    QStringList installedPlugins;
    QStringList startedPlugins;
    QVector<PlatformStartupTraceEntry> startupTrace;
};

class FRAMEWORK_EXPORT CtkRuntimeSnapshotCollector
{
public:
    PlatformRuntimeObservation collect() const;
};
