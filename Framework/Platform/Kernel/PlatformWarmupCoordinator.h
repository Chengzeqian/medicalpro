#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformRuntimeTypes.h"
#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"

#include <functional>

class PlatformLifecycleTraceRecorder;

struct PlatformWarmupOutcome
{
    bool success = true;
    PlatformLifecycleResult result = PlatformLifecycleResult::Succeeded;
    QString reasonCode;
    QString detail;
};

class FRAMEWORK_EXPORT PlatformWarmupCoordinator
{
public:
    using WarmupStepFn = std::function<PlatformLifecycleResult(const PlatformManagedPluginPlanEntry&)>;

    explicit PlatformWarmupCoordinator(PlatformLifecycleTraceRecorder* recorder = nullptr);

    PlatformWarmupOutcome run(
        const PlatformManagedPluginPlan& plan,
        PlatformRuntimeMode runtimeMode,
        const WarmupStepFn& warmupStepFn) const;

private:
    PlatformLifecycleTraceRecorder* m_recorder = nullptr;
};
