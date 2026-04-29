#include "Framework/Platform/Kernel/PlatformWarmupCoordinator.h"

#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"

namespace
{
QString warmupReasonCode(PlatformLifecycleResult result)
{
    switch (result) {
    case PlatformLifecycleResult::Succeeded:
        return QStringLiteral("warmup_ready");
    case PlatformLifecycleResult::Skipped:
        return QStringLiteral("warmup_skipped");
    case PlatformLifecycleResult::Degraded:
        return QStringLiteral("warmup_degraded");
    case PlatformLifecycleResult::Timeout:
        return QStringLiteral("warmup_timeout");
    case PlatformLifecycleResult::Failed:
    case PlatformLifecycleResult::Running:
        break;
    }

    return QStringLiteral("warmup_failed");
}

QString warmupDetail(PlatformLifecycleResult result)
{
    switch (result) {
    case PlatformLifecycleResult::Succeeded:
        return QStringLiteral("Managed warmup completed");
    case PlatformLifecycleResult::Skipped:
        return QStringLiteral("Managed warmup step skipped");
    case PlatformLifecycleResult::Degraded:
        return QStringLiteral("Managed warmup completed with degradation");
    case PlatformLifecycleResult::Timeout:
        return QStringLiteral("Managed warmup step timed out");
    case PlatformLifecycleResult::Failed:
    case PlatformLifecycleResult::Running:
        break;
    }

    return QStringLiteral("Managed warmup step failed");
}
}

PlatformWarmupCoordinator::PlatformWarmupCoordinator(PlatformLifecycleTraceRecorder* recorder)
    : m_recorder(recorder)
{
}

PlatformWarmupOutcome PlatformWarmupCoordinator::run(
    const PlatformManagedPluginPlan& plan,
    PlatformRuntimeMode runtimeMode,
    const WarmupStepFn& warmupStepFn) const
{
    if (runtimeMode != PlatformRuntimeMode::OrchestrateCore) {
        for (const auto& entry : plan.installEntries) {
            if (!m_recorder) continue;
            m_recorder->recordPluginStepStarted(
                entry.pluginId,
                entry.symbolicName,
                PlatformLifecycleStep::Warmup,
                false);
            m_recorder->recordPluginStepFinished(
                entry.pluginId,
                entry.symbolicName,
                PlatformLifecycleStep::Warmup,
                PlatformLifecycleResult::Skipped,
                QStringLiteral("skipped_by_mode"),
                QStringLiteral("Warmup skipped outside orchestrate_core"));
        }

        return {
            true,
            PlatformLifecycleResult::Skipped,
            QStringLiteral("skipped_by_mode"),
            QStringLiteral("Warmup skipped outside orchestrate_core")
        };
    }

    if (plan.installEntries.isEmpty() || !warmupStepFn) {
        return {
            true,
            PlatformLifecycleResult::Succeeded,
            QStringLiteral("no_warmup_tasks"),
            QStringLiteral("No managed warmup tasks configured")
        };
    }

    auto overallResult = PlatformLifecycleResult::Succeeded;
    for (const auto& entry : plan.installEntries) {
        if (m_recorder) {
            m_recorder->recordPluginStepStarted(
                entry.pluginId,
                entry.symbolicName,
                PlatformLifecycleStep::Warmup,
                false);
        }

        auto stepResult = warmupStepFn(entry);
        if (stepResult == PlatformLifecycleResult::Running) {
            stepResult = PlatformLifecycleResult::Succeeded;
        }

        if (m_recorder) {
            m_recorder->recordPluginStepFinished(
                entry.pluginId,
                entry.symbolicName,
                PlatformLifecycleStep::Warmup,
                stepResult,
                warmupReasonCode(stepResult),
                warmupDetail(stepResult));
        }

        if (stepResult == PlatformLifecycleResult::Failed || stepResult == PlatformLifecycleResult::Timeout) {
            return {
                false,
                stepResult,
                warmupReasonCode(stepResult),
                warmupDetail(stepResult)
            };
        }

        if (stepResult == PlatformLifecycleResult::Degraded) {
            overallResult = PlatformLifecycleResult::Degraded;
        }
    }

    return {
        true,
        overallResult,
        warmupReasonCode(overallResult),
        warmupDetail(overallResult)
    };
}
