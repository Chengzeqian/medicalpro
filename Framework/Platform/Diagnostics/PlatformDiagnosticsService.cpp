#include "Framework/Platform/Diagnostics/PlatformDiagnosticsService.h"

#include "Framework/Platform/Kernel/PlatformStateStore.h"

PlatformDiagnosticsService::PlatformDiagnosticsService(PlatformStateStore* stateStore)
    : m_stateStore(stateStore)
{
}

PlatformDiagnosticSnapshot PlatformDiagnosticsService::buildSnapshot(const PlatformRuntimeObservation& observation) const
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.frameworkReady = observation.frameworkReady;
    snapshot.startupTrace = observation.startupTrace;

    if (m_stateStore) {
        snapshot.plugins = m_stateStore->pluginSnapshots();
        snapshot.capabilitySnapshot = m_stateStore->capabilitySnapshot();
        snapshot.runtimeMode = snapshot.capabilitySnapshot.runtimeMode;
    }

    if (!observation.startupTrace.isEmpty() && !observation.startupTrace.constLast().success) {
        snapshot.recoveryHints.append(observation.startupTrace.constLast().detail);
    }

    for (const auto& plugin : snapshot.plugins) {
        if (!plugin.missingRequiredServices.isEmpty()) {
            snapshot.recoveryHints.append(
                QStringLiteral("%1 缺少服务：%2").arg(plugin.ctkSymbolicName, plugin.missingRequiredServices.join(QStringLiteral(", "))));
        }
    }

    return snapshot;
}
