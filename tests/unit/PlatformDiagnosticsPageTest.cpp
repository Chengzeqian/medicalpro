#include <QtTest>

#include <QLabel>
#include <QTableWidget>

#include "UI/NewPages/PlatformDiagnosticsPage.h"

class PlatformDiagnosticsPageTest : public QObject
{
    Q_OBJECT

private slots:
    void refreshSnapshot_renders_mode_plugin_and_trace();
    void refreshSnapshot_renders_summary_problem_and_sorted_plugins();
    void refreshSnapshot_renders_timeline_with_blocking_flag();
    void refreshSnapshot_renders_timeline_sorted_by_start_offset();
    void refreshSnapshot_renders_extended_summary_fields_and_highlighted_blocking_point();
    void refreshSnapshot_renders_warmup_tail_as_skipped_by_mode_outside_orchestrate_core();
    void refreshSnapshot_renders_full_plugin_lifecycle_matrix_and_highlights_slowest_plugin();
    void refreshSnapshot_renders_expanded_timeline_fields_with_scope_and_subject();
    void refreshSnapshot_renders_problem_matrix_with_impact_capability_and_recovery_hint();
};

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_mode_plugin_and_trace()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::ObserveOnly;
    snapshot.summary.frameworkReady = true;
    snapshot.startupTrace = {
        {QStringLiteral("critical_start"), QStringLiteral("Start core plugins"), true, 540, QStringLiteral("ok")}
    };

    PlatformPluginLifecycleSnapshot pluginSnapshot;
    pluginSnapshot.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    pluginSnapshot.symbolicName = QStringLiteral("DicomViewer");
    pluginSnapshot.state = PlatformPluginState::Ready;
    snapshot.pluginLifecycle.append(pluginSnapshot);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    QCOMPARE(page.findChild<QLabel*>(QStringLiteral("runtimeModeValueLabel"))->text(), QStringLiteral("observe_only"));
    QCOMPARE(page.findChild<QTableWidget*>(QStringLiteral("pluginTableWidget"))->rowCount(), 1);
    QCOMPARE(page.findChild<QTableWidget*>(QStringLiteral("traceTableWidget"))->rowCount(), 1);
}

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_summary_problem_and_sorted_plugins()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::OrchestrateCore;
    snapshot.summary.frameworkReady = true;
    snapshot.summary.platformReady = false;
    snapshot.summary.startupReadyPathMs = 680;
    snapshot.summary.startupWarmupTailMs = 120;
    snapshot.summary.blockingSpanLabel = QStringLiteral("registration_core start");
    snapshot.summary.failurePointLabel = QStringLiteral("registration_core service_ready_timeout");
    snapshot.recoveryHints = QStringList{
        QStringLiteral("Check service registration chain"),
        QStringLiteral("Retry failed plugin startup")
    };

    PlatformDiagnosticProblem criticalProblem;
    criticalProblem.severity = PlatformDiagnosticSeverity::Critical;
    criticalProblem.pluginId = QStringLiteral("org.medicalpro.registration_core");
    criticalProblem.phaseKey = QStringLiteral("core_start");
    criticalProblem.step = PlatformLifecycleStep::Start;
    criticalProblem.reasonCode = QStringLiteral("plugin_start_failed");
    criticalProblem.detail = QStringLiteral("RegistrationCore activation failed");
    criticalProblem.blockingStartup = true;
    snapshot.problems.append(criticalProblem);

    PlatformDiagnosticProblem warningProblem;
    warningProblem.severity = PlatformDiagnosticSeverity::Warning;
    warningProblem.pluginId = QStringLiteral("org.medicalpro.analytics");
    warningProblem.phaseKey = QStringLiteral("warmup");
    warningProblem.step = PlatformLifecycleStep::Warmup;
    warningProblem.reasonCode = QStringLiteral("warmup_slow");
    warningProblem.detail = QStringLiteral("Warmup exceeded threshold");
    warningProblem.blockingStartup = false;
    snapshot.problems.append(warningProblem);

    PlatformPluginLifecycleSnapshot readyPlugin;
    readyPlugin.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    readyPlugin.symbolicName = QStringLiteral("DicomViewer");
    readyPlugin.state = PlatformPluginState::Ready;
    readyPlugin.startupBlocked = false;
    readyPlugin.blockingMs = 480;
    snapshot.pluginLifecycle.append(readyPlugin);

    PlatformPluginLifecycleSnapshot blockedPlugin;
    blockedPlugin.pluginId = QStringLiteral("org.medicalpro.user_management");
    blockedPlugin.symbolicName = QStringLiteral("UserManagement");
    blockedPlugin.state = PlatformPluginState::Ready;
    blockedPlugin.startupBlocked = true;
    blockedPlugin.blockingMs = 220;
    snapshot.pluginLifecycle.append(blockedPlugin);

    PlatformPluginLifecycleSnapshot failedPlugin;
    failedPlugin.pluginId = QStringLiteral("org.medicalpro.registration_core");
    failedPlugin.symbolicName = QStringLiteral("RegistrationCore");
    failedPlugin.state = PlatformPluginState::Failed;
    failedPlugin.startupBlocked = true;
    failedPlugin.blockingMs = 90;
    snapshot.pluginLifecycle.append(failedPlugin);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    QCOMPARE(page.findChild<QLabel*>(QStringLiteral("runtimeModeValueLabel"))->text(), QStringLiteral("orchestrate_core"));
    QCOMPARE(page.findChild<QLabel*>(QStringLiteral("frameworkReadyValueLabel"))->text(), QStringLiteral("yes"));
    QCOMPARE(page.findChild<QLabel*>(QStringLiteral("platformReadyValueLabel"))->text(), QStringLiteral("no"));
    QCOMPARE(page.findChild<QLabel*>(QStringLiteral("startupReadyPathValueLabel"))->text(), QStringLiteral("680 ms"));
    QCOMPARE(page.findChild<QLabel*>(QStringLiteral("warmupTailValueLabel"))->text(), QStringLiteral("120 ms"));
    QCOMPARE(page.findChild<QLabel*>(QStringLiteral("blockingPointValueLabel"))->text(), QStringLiteral("registration_core start"));
    QCOMPARE(
        page.findChild<QLabel*>(QStringLiteral("failurePointValueLabel"))->text(),
        QStringLiteral("registration_core service_ready_timeout"));
    QCOMPARE(
        page.findChild<QLabel*>(QStringLiteral("recoveryHintsValueLabel"))->text(),
        QStringLiteral("Check service registration chain | Retry failed plugin startup"));

    auto* problemTable = page.findChild<QTableWidget*>(QStringLiteral("problemTableWidget"));
    QCOMPARE(problemTable->rowCount(), 2);
    QCOMPARE(problemTable->item(0, 0)->text(), QStringLiteral("critical"));
    QCOMPARE(problemTable->item(0, 1)->text(), QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(problemTable->item(0, 2)->text(), QStringLiteral("plugin_start_failed"));
    QCOMPARE(problemTable->item(1, 0)->text(), QStringLiteral("warning"));

    auto* pluginTable = page.findChild<QTableWidget*>(QStringLiteral("pluginTableWidget"));
    QCOMPARE(pluginTable->rowCount(), 3);
    QCOMPARE(pluginTable->item(0, 0)->text(), QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(pluginTable->item(1, 0)->text(), QStringLiteral("org.medicalpro.dicom_viewer"));
    QCOMPARE(pluginTable->item(2, 0)->text(), QStringLiteral("org.medicalpro.user_management"));
}

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_timeline_with_blocking_flag()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::FacadeMode;

    PlatformStartupTraceEntry failedEntry;
    failedEntry.phaseKey = QStringLiteral("critical_start");
    failedEntry.phaseLabel = QStringLiteral("Start core plugins");
    failedEntry.success = false;
    failedEntry.result = PlatformLifecycleResult::Timeout;
    failedEntry.blockingStartup = true;
    failedEntry.elapsedMs = 530;
    failedEntry.detail = QStringLiteral("RegistrationCore timeout");
    snapshot.startupTrace.append(failedEntry);

    PlatformStartupTraceEntry warmupEntry;
    warmupEntry.phaseKey = QStringLiteral("plugin_warmup");
    warmupEntry.phaseLabel = QStringLiteral("Warmup optional plugins");
    warmupEntry.success = true;
    warmupEntry.result = PlatformLifecycleResult::Skipped;
    warmupEntry.blockingStartup = false;
    warmupEntry.elapsedMs = 210;
    warmupEntry.detail = QStringLiteral("ok");
    snapshot.startupTrace.append(warmupEntry);

    PlatformStartupTraceEntry degradedEntry;
    degradedEntry.phaseKey = QStringLiteral("plugin_degraded");
    degradedEntry.phaseLabel = QStringLiteral("Optional plugin degraded");
    degradedEntry.success = true;
    degradedEntry.result = PlatformLifecycleResult::Degraded;
    degradedEntry.blockingStartup = false;
    degradedEntry.elapsedMs = 120;
    degradedEntry.detail = QStringLiteral("degraded mode");
    snapshot.startupTrace.append(degradedEntry);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    auto* traceTable = page.findChild<QTableWidget*>(QStringLiteral("traceTableWidget"));
    QCOMPARE(traceTable->rowCount(), 3);
    QCOMPARE(traceTable->item(0, 2)->text(), QStringLiteral("phase"));
    QCOMPARE(traceTable->item(0, 3)->text(), QStringLiteral("Start core plugins"));
    QCOMPARE(traceTable->item(0, 5)->text(), QStringLiteral("timeout"));
    QCOMPARE(traceTable->item(0, 6)->text(), QStringLiteral("yes"));
    QCOMPARE(traceTable->item(1, 5)->text(), QStringLiteral("skipped"));
    QCOMPARE(traceTable->item(2, 5)->text(), QStringLiteral("degraded"));
}

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_timeline_sorted_by_start_offset()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::FacadeMode;

    PlatformStartupTraceEntry endsFirstEntry;
    endsFirstEntry.phaseKey = QStringLiteral("started_late_finished_early");
    endsFirstEntry.phaseLabel = QStringLiteral("Started late, finished early");
    endsFirstEntry.result = PlatformLifecycleResult::Succeeded;
    endsFirstEntry.startOffsetMs = 140;
    endsFirstEntry.elapsedMs = 30;
    endsFirstEntry.success = true;
    snapshot.startupTrace.append(endsFirstEntry);

    PlatformStartupTraceEntry startsFirstEntry;
    startsFirstEntry.phaseKey = QStringLiteral("started_early_finished_late");
    startsFirstEntry.phaseLabel = QStringLiteral("Started early, finished late");
    startsFirstEntry.result = PlatformLifecycleResult::Succeeded;
    startsFirstEntry.startOffsetMs = 50;
    startsFirstEntry.elapsedMs = 220;
    startsFirstEntry.success = true;
    snapshot.startupTrace.append(startsFirstEntry);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    auto* traceTable = page.findChild<QTableWidget*>(QStringLiteral("traceTableWidget"));
    QCOMPARE(traceTable->rowCount(), 2);
    QCOMPARE(traceTable->item(0, 0)->text(), QStringLiteral("50"));
    QCOMPARE(traceTable->item(0, 3)->text(), QStringLiteral("Started early, finished late"));
    QCOMPARE(traceTable->item(1, 0)->text(), QStringLiteral("140"));
    QCOMPARE(traceTable->item(1, 3)->text(), QStringLiteral("Started late, finished early"));
}

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_extended_summary_fields_and_highlighted_blocking_point()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::OrchestrateCore;
    snapshot.summary.frameworkReady = true;
    snapshot.summary.platformReady = false;
    snapshot.summary.startupReadyPathMs = 680;
    snapshot.summary.startupWarmupTailMs = 120;
    snapshot.summary.fullObservedStartupMs = 910;
    snapshot.summary.slowestPluginId = QStringLiteral("org.medicalpro.registration_core");
    snapshot.summary.blockingSpanLabel = QStringLiteral("registration_core service_ready");
    snapshot.summary.failurePointLabel = QStringLiteral("registration_core service_ready_timeout");

    PlatformPluginLifecycleSnapshot plugin;
    plugin.pluginId = QStringLiteral("org.medicalpro.registration_core");
    plugin.symbolicName = QStringLiteral("RegistrationCore");
    plugin.state = PlatformPluginState::Failed;
    snapshot.pluginLifecycle.append(plugin);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    auto* fullObservedStartupValueLabel =
        page.findChild<QLabel*>(QStringLiteral("fullObservedStartupValueLabel"));
    auto* slowestPluginValueLabel = page.findChild<QLabel*>(QStringLiteral("slowestPluginValueLabel"));
    auto* blockingPointValueLabel = page.findChild<QLabel*>(QStringLiteral("blockingPointValueLabel"));

    QVERIFY(fullObservedStartupValueLabel != nullptr);
    QVERIFY(slowestPluginValueLabel != nullptr);
    QVERIFY(blockingPointValueLabel != nullptr);
    QCOMPARE(fullObservedStartupValueLabel->text(), QStringLiteral("910 ms"));
    QCOMPARE(
        slowestPluginValueLabel->text(),
        QStringLiteral("RegistrationCore (org.medicalpro.registration_core)"));
    QVERIFY(blockingPointValueLabel->font().bold());
}

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_warmup_tail_as_skipped_by_mode_outside_orchestrate_core()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::FacadeMode;
    snapshot.summary.frameworkReady = true;
    snapshot.summary.platformReady = true;
    snapshot.summary.startupReadyPathMs = 320;
    snapshot.summary.startupWarmupTailMs = 0;
    snapshot.summary.fullObservedStartupMs = 320;
    snapshot.summary.blockingSpanLabel = QStringLiteral("none");
    snapshot.summary.failurePointLabel = QStringLiteral("none");

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    auto* warmupTailValueLabel = page.findChild<QLabel*>(QStringLiteral("warmupTailValueLabel"));
    auto* blockingPointValueLabel = page.findChild<QLabel*>(QStringLiteral("blockingPointValueLabel"));

    QVERIFY(warmupTailValueLabel != nullptr);
    QVERIFY(blockingPointValueLabel != nullptr);
    QCOMPARE(warmupTailValueLabel->text(), QStringLiteral("skipped_by_mode"));
    QVERIFY(!blockingPointValueLabel->font().bold());
}

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_full_plugin_lifecycle_matrix_and_highlights_slowest_plugin()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::OrchestrateCore;
    snapshot.summary.slowestPluginId = QStringLiteral("org.medicalpro.registration_core");

    PlatformPluginLifecycleSnapshot failedPlugin;
    failedPlugin.pluginId = QStringLiteral("org.medicalpro.registration_core");
    failedPlugin.symbolicName = QStringLiteral("RegistrationCore");
    failedPlugin.displayName = QStringLiteral("Registration Core");
    failedPlugin.bootstrapLevel = PlatformBootstrapLevel::Core;
    failedPlugin.startupPolicy = PlatformStartupPolicy::Eager;
    failedPlugin.state = PlatformPluginState::Failed;
    failedPlugin.installMs = 80;
    failedPlugin.startMs = 140;
    failedPlugin.serviceReadyMs = 510;
    failedPlugin.warmupMs = 0;
    failedPlugin.blockingMs = 730;
    failedPlugin.slowestStep = PlatformLifecycleStep::ServiceReady;
    failedPlugin.lastReasonCode = QStringLiteral("service_ready_timeout");
    failedPlugin.recoveryHints = QStringList{QStringLiteral("Check service registration chain")};

    PlatformPluginLifecycleSnapshot readyPlugin;
    readyPlugin.pluginId = QStringLiteral("org.medicalpro.user_management");
    readyPlugin.symbolicName = QStringLiteral("UserManagement");
    readyPlugin.displayName = QStringLiteral("User Management");
    readyPlugin.bootstrapLevel = PlatformBootstrapLevel::Core;
    readyPlugin.startupPolicy = PlatformStartupPolicy::Eager;
    readyPlugin.state = PlatformPluginState::Ready;
    readyPlugin.installMs = 30;
    readyPlugin.startMs = 60;
    readyPlugin.serviceReadyMs = 90;
    readyPlugin.warmupMs = 40;
    readyPlugin.blockingMs = 180;
    readyPlugin.slowestStep = PlatformLifecycleStep::ServiceReady;

    snapshot.pluginLifecycle = {readyPlugin, failedPlugin};

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    auto* pluginTable = page.findChild<QTableWidget*>(QStringLiteral("pluginTableWidget"));
    QVERIFY(pluginTable != nullptr);
    QCOMPARE(pluginTable->columnCount(), 14);
    QCOMPARE(pluginTable->horizontalHeaderItem(0)->text(), QStringLiteral("Plugin ID"));
    QCOMPARE(pluginTable->horizontalHeaderItem(3)->text(), QStringLiteral("Startup Policy"));
    QCOMPARE(pluginTable->horizontalHeaderItem(13)->text(), QStringLiteral("Recovery"));
    QCOMPARE(pluginTable->item(0, 0)->text(), QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(pluginTable->item(0, 2)->text(), QStringLiteral("core"));
    QCOMPARE(pluginTable->item(0, 3)->text(), QStringLiteral("eager"));
    QCOMPARE(pluginTable->item(0, 8)->text(), QStringLiteral("0"));
    QCOMPARE(pluginTable->item(0, 12)->text(), QStringLiteral("service_ready_timeout"));
    QCOMPARE(pluginTable->item(0, 13)->text(), QStringLiteral("Check service registration chain"));
    QVERIFY(pluginTable->item(0, 0)->font().bold());
    QVERIFY(!pluginTable->item(1, 0)->font().bold());
}

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_expanded_timeline_fields_with_scope_and_subject()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::OrchestrateCore;

    PlatformStartupTraceEntry pluginEntry;
    pluginEntry.phaseKey = QStringLiteral("registration_service_ready");
    pluginEntry.phaseLabel = QStringLiteral("Registration service ready");
    pluginEntry.pluginId = QStringLiteral("org.medicalpro.registration_core");
    pluginEntry.symbolicName = QStringLiteral("RegistrationCore");
    pluginEntry.step = PlatformLifecycleStep::ServiceReady;
    pluginEntry.result = PlatformLifecycleResult::Timeout;
    pluginEntry.blockingStartup = true;
    pluginEntry.startOffsetMs = 280;
    pluginEntry.elapsedMs = 420;
    pluginEntry.detail = QStringLiteral("RegistrationService missing");

    PlatformStartupTraceEntry phaseEntry;
    phaseEntry.phaseKey = QStringLiteral("warmup_phase");
    phaseEntry.phaseLabel = QStringLiteral("Warmup optional plugins");
    phaseEntry.step = PlatformLifecycleStep::Warmup;
    phaseEntry.result = PlatformLifecycleResult::Skipped;
    phaseEntry.blockingStartup = false;
    phaseEntry.startOffsetMs = 760;
    phaseEntry.elapsedMs = 90;
    phaseEntry.detail = QStringLiteral("skipped by mode");

    snapshot.startupTrace = {phaseEntry, pluginEntry};

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    auto* traceTable = page.findChild<QTableWidget*>(QStringLiteral("traceTableWidget"));
    QVERIFY(traceTable != nullptr);
    QCOMPARE(traceTable->columnCount(), 8);
    QCOMPARE(traceTable->item(0, 0)->text(), QStringLiteral("280"));
    QCOMPARE(traceTable->item(0, 2)->text(), QStringLiteral("plugin"));
    QCOMPARE(traceTable->item(0, 3)->text(), QStringLiteral("RegistrationCore"));
    QCOMPARE(traceTable->item(0, 4)->text(), QStringLiteral("service_ready"));
    QCOMPARE(traceTable->item(0, 5)->text(), QStringLiteral("timeout"));
    QCOMPARE(traceTable->item(1, 2)->text(), QStringLiteral("phase"));
    QCOMPARE(traceTable->item(1, 3)->text(), QStringLiteral("Warmup optional plugins"));
}

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_problem_matrix_with_impact_capability_and_recovery_hint()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.summary.runtimeMode = PlatformRuntimeMode::OrchestrateCore;

    PlatformDiagnosticProblem problem;
    problem.severity = PlatformDiagnosticSeverity::Critical;
    problem.pluginId = QStringLiteral("org.medicalpro.registration_core");
    problem.reasonCode = QStringLiteral("service_ready_timeout");
    problem.detail = QStringLiteral("RegistrationService missing");
    problem.impactCapabilities = QStringList{
        QStringLiteral("navigation.registration"),
        QStringLiteral("navigation.guidance")
    };
    problem.recoveryHints = QStringList{
        QStringLiteral("Check service registration chain"),
        QStringLiteral("Verify required services are available")
    };
    snapshot.problems.append(problem);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    auto* problemTable = page.findChild<QTableWidget*>(QStringLiteral("problemTableWidget"));
    QVERIFY(problemTable != nullptr);
    QCOMPARE(problemTable->columnCount(), 6);
    QCOMPARE(problemTable->item(0, 0)->text(), QStringLiteral("critical"));
    QCOMPARE(problemTable->item(0, 1)->text(), QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(problemTable->item(0, 2)->text(), QStringLiteral("service_ready_timeout"));
    QCOMPARE(problemTable->item(0, 3)->text(), QStringLiteral("navigation.registration | navigation.guidance"));
    QCOMPARE(
        problemTable->item(0, 5)->text(),
        QStringLiteral("Check service registration chain | Verify required services are available"));
}

QTEST_MAIN(PlatformDiagnosticsPageTest)
#include "PlatformDiagnosticsPageTest.moc"
