#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

class PlatformStateStore;

class FRAMEWORK_EXPORT PlatformDiagnosticsService
{
public:
    explicit PlatformDiagnosticsService(PlatformStateStore* stateStore);
    PlatformDiagnosticSnapshot buildSnapshot(const PlatformRuntimeObservation& observation) const;

private:
    PlatformStateStore* m_stateStore;
};
