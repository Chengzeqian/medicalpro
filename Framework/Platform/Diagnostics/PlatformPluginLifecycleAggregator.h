#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

#include <QString>
#include <QVector>

struct FRAMEWORK_EXPORT PlatformPluginLifecycleAggregation
{
    PlatformDiagnosticSummary summary;
    QVector<PlatformPluginLifecycleSnapshot> pluginLifecycle;
    QVector<PlatformDiagnosticProblem> problems;
    QStringList recoveryHints;
};

class FRAMEWORK_EXPORT PlatformPluginLifecycleAggregator
{
public:
    PlatformPluginLifecycleAggregation aggregate(
        const QVector<PlatformLifecycleEvent>& lifecycleEvents,
        const QVector<PlatformPluginDescriptor>& descriptors,
        const PlatformRuntimeObservation& observation) const;
};
