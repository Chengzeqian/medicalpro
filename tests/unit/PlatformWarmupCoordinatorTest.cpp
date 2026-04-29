#include <QtTest/QtTest>

#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"
#include "Framework/Platform/Kernel/PlatformWarmupCoordinator.h"

class PlatformWarmupCoordinatorTest : public QObject
{
    Q_OBJECT

private slots:
    void run_skips_all_warmup_entries_outside_orchestrate_core();
    void run_with_empty_phase1_plan_returns_non_blocking_success();
};

void PlatformWarmupCoordinatorTest::run_skips_all_warmup_entries_outside_orchestrate_core()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::FacadeMode);
    bool warmupInvoked = false;

    PlatformManagedPluginPlan plan;
    PlatformManagedPluginPlanEntry entry;
    entry.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    entry.symbolicName = QStringLiteral("DicomViewer");
    plan.installEntries = QVector<PlatformManagedPluginPlanEntry>{entry};

    PlatformWarmupCoordinator coordinator(&recorder);
    const auto outcome = coordinator.run(
        plan,
        PlatformRuntimeMode::FacadeMode,
        [&warmupInvoked](const PlatformManagedPluginPlanEntry&) -> PlatformLifecycleResult {
            warmupInvoked = true;
            return PlatformLifecycleResult::Succeeded;
        });

    QVERIFY(outcome.success);
    QCOMPARE(outcome.result, PlatformLifecycleResult::Skipped);
    QVERIFY(!warmupInvoked);
}

void PlatformWarmupCoordinatorTest::run_with_empty_phase1_plan_returns_non_blocking_success()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::OrchestrateCore);

    PlatformWarmupCoordinator coordinator(&recorder);
    const auto outcome = coordinator.run({}, PlatformRuntimeMode::OrchestrateCore, {});

    QVERIFY(outcome.success);
    QCOMPARE(outcome.result, PlatformLifecycleResult::Succeeded);
}

QTEST_APPLESS_MAIN(PlatformWarmupCoordinatorTest)
#include "PlatformWarmupCoordinatorTest.moc"
