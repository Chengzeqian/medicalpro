#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"
#include "Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.h"
#include "Framework/Platform/Diagnostics/PlatformRuntimeSnapshotCollector.h"

class PlatformStateStore;

class FRAMEWORK_EXPORT PlatformDiagnosticsService
{
public:
    explicit PlatformDiagnosticsService(PlatformStateStore* stateStore);
    PlatformDiagnosticSnapshot buildSnapshot(const PlatformRuntimeObservation& observation) const;

private:
    PlatformPluginLifecycleAggregator m_aggregator;
    PlatformStateStore* m_stateStore;
};
