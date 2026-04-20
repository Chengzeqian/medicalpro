#include <QtTest>

#include "Framework/Platform/Diagnostics/PlatformDiagnosticsService.h"
#include "Framework/Platform/Kernel/PlatformStateStore.h"

#include <atomic>
#include <thread>

namespace
{
PlatformPluginDescriptor makeDescriptor(
    const QString& pluginId,
    const QString& ctkSymbolicName,
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Core,
    PlatformStartupPolicy startupPolicy = PlatformStartupPolicy::Eager)
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = pluginId;
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = ctkSymbolicName;
    descriptor.domain = QStringLiteral("test");
    descriptor.runtime.ctkSymbolicName = ctkSymbolicName;
    descriptor.runtime.bootstrapLevel = bootstrapLevel;
    descriptor.runtime.startupPolicy = startupPolicy;
    descriptor.provides.capabilities = QStringList{QStringLiteral("%1.capability").arg(pluginId)};
    return descriptor;
}

PlatformLifecycleEvent makeEvent(
    PlatformLifecycleEventKind kind,
    const QString& pluginId,
    const QString& ctkSymbolicName,
    PlatformLifecycleStep step,
    PlatformLifecycleResult result,
    qint64 offsetMs,
    qint64 durationMs,
    bool blockingStartup,
    const QString& reasonCode = {},
    const QString& detail = {})
{
    PlatformLifecycleEvent event;
    event.kind = kind;
    event.pluginId = pluginId;
    event.ctkSymbolicName = ctkSymbolicName;
    event.step = step;
    event.result = result;
    event.offsetMs = offsetMs;
    event.durationMs = durationMs;
    event.blockingStartup = blockingStartup;
    event.reasonCode = reasonCode;
    event.detail = detail;
    return event;
}
}

class PlatformDiagnosticsServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void buildSnapshot_includes_mode_trace_and_recovery_hint();
    void buildSnapshot_identifies_slowest_plugin_and_failure_point();
    void buildSnapshot_falls_back_to_first_degraded_failure_point();
    void buildSnapshot_prefers_reason_code_recovery_hints_over_raw_detail();
    void buildSnapshot_detects_ctk_platform_state_mismatch();
    void buildSnapshot_does_not_report_mismatch_when_platform_state_is_ready();
    void buildSnapshot_deduplicates_and_sorts_problems_after_mismatch_append();
    void stateStore_supports_cross_thread_read_write();
};

void PlatformDiagnosticsServiceTest::buildSnapshot_includes_mode_trace_and_recovery_hint()
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = QStringLiteral("org.medicalpro.dicom_viewer");
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = QStringLiteral("DicomViewer");
    descriptor.domain = QStringLiteral("imaging");
    descriptor.runtime.ctkSymbolicName = QStringLiteral("DicomViewer");
    descriptor.provides.capabilities = QStringList{QStringLiteral("imaging.data")};

    PlatformStateStore store;
    store.replaceDescriptors({descriptor});
    store.setRuntimeMode(PlatformRuntimeMode::ObserveOnly);

    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.installedPlugins = QStringList{QStringLiteral("DicomViewer")};
    observation.startupTrace = {
        {QStringLiteral("plugin_install"), QStringLiteral("Install plugins"), true, 210, QStringLiteral("installed 5 plugins")},
        {QStringLiteral("critical_start"), QStringLiteral("Start core plugins"), false, 820, QStringLiteral("DicomViewerService missing")}
    };

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot(observation);

    QCOMPARE(snapshot.runtimeMode, PlatformRuntimeMode::ObserveOnly);
    QVERIFY(snapshot.frameworkReady);
    QCOMPARE(snapshot.startupTrace.size(), 2);
    QVERIFY(snapshot.recoveryHints.join(QStringLiteral(" | ")).contains(QStringLiteral("DicomViewerService")));
}

void PlatformDiagnosticsServiceTest::buildSnapshot_identifies_slowest_plugin_and_failure_point()
{
    PlatformStateStore store;
    store.replaceDescriptors({
        makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement")),
        makeDescriptor(QStringLiteral("org.medicalpro.dicom_viewer"), QStringLiteral("DicomViewer"))
    });
    store.setRuntimeMode(PlatformRuntimeMode::OrchestrateCore);
    store.setPluginState(QStringLiteral("org.medicalpro.user_management"), PlatformPluginState::Ready);
    store.setPluginState(QStringLiteral("org.medicalpro.dicom_viewer"), PlatformPluginState::Failed);

    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginServiceReady,
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Succeeded,
            220,
            220,
            true),
        makeEvent(
            PlatformLifecycleEventKind::PluginFailed,
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Failed,
            820,
            600,
            true,
            QStringLiteral("service_ready_timeout"),
            QStringLiteral("DicomViewerService missing"))
    };

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot(observation);

    QCOMPARE(snapshot.summary.runtimeMode, PlatformRuntimeMode::OrchestrateCore);
    QCOMPARE(snapshot.summary.slowestPluginId, QStringLiteral("org.medicalpro.dicom_viewer"));
    QVERIFY(!snapshot.summary.failurePointLabel.isEmpty());
    QVERIFY(!snapshot.problems.isEmpty());
    QCOMPARE(snapshot.problems.constFirst().reasonCode, QStringLiteral("service_ready_timeout"));
}

void PlatformDiagnosticsServiceTest::buildSnapshot_falls_back_to_first_degraded_failure_point()
{
    PlatformStateStore store;
    store.replaceDescriptors({
        makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement")),
        makeDescriptor(QStringLiteral("org.medicalpro.dicom_viewer"), QStringLiteral("DicomViewer"))
    });
    store.setRuntimeMode(PlatformRuntimeMode::FacadeMode);
    store.setPluginState(QStringLiteral("org.medicalpro.user_management"), PlatformPluginState::Degraded);
    store.setPluginState(QStringLiteral("org.medicalpro.dicom_viewer"), PlatformPluginState::Ready);

    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginDegraded,
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Degraded,
            260,
            80,
            true,
            QStringLiteral("service_missing"),
            QStringLiteral("UserManagement optional service missing")),
        makeEvent(
            PlatformLifecycleEventKind::PluginDegraded,
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformLifecycleStep::Warmup,
            PlatformLifecycleResult::Degraded,
            480,
            120,
            false,
            QStringLiteral("warmup_failed"),
            QStringLiteral("Dicom cache warmup failed"))
    };

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot(observation);

    QCOMPARE(snapshot.summary.failurePointLabel, QStringLiteral("org.medicalpro.user_management service_ready"));
}

void PlatformDiagnosticsServiceTest::buildSnapshot_prefers_reason_code_recovery_hints_over_raw_detail()
{
    PlatformStateStore store;
    store.replaceDescriptors({
        makeDescriptor(QStringLiteral("org.medicalpro.dicom_viewer"), QStringLiteral("DicomViewer"))
    });
    store.setRuntimeMode(PlatformRuntimeMode::OrchestrateCore);
    store.setPluginState(QStringLiteral("org.medicalpro.dicom_viewer"), PlatformPluginState::Failed);

    PlatformRuntimeObservation observation;
    observation.frameworkReady = false;
    observation.startupTrace = {
        {
            QStringLiteral("critical_start"),
            QStringLiteral("Start core plugins"),
            false,
            820,
            QStringLiteral("Raw detail that should not become the top-level hint")
        }
    };
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginFailed,
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Timeout,
            820,
            600,
            true,
            QStringLiteral("service_ready_timeout"),
            QStringLiteral("Raw detail that should not become the top-level hint"))
    };

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot(observation);

    QVERIFY(snapshot.recoveryHints.contains(QStringLiteral(
        "Check service registration chain and verify required services are available.")));
    QVERIFY(!snapshot.recoveryHints.contains(QStringLiteral(
        "Raw detail that should not become the top-level hint")));
}

void PlatformDiagnosticsServiceTest::buildSnapshot_detects_ctk_platform_state_mismatch()
{
    PlatformStateStore store;
    store.replaceDescriptors({
        makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement"))
    });
    store.setRuntimeMode(PlatformRuntimeMode::FacadeMode);
    store.setPluginState(QStringLiteral("org.medicalpro.user_management"), PlatformPluginState::Discovered);

    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.startedPlugins = QStringList{QStringLiteral("UserManagement")};
    observation.pluginStates.insert(QStringLiteral("UserManagement"), QStringLiteral("ACTIVE"));

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot(observation);

    QVERIFY(!snapshot.problems.isEmpty());
    QCOMPARE(snapshot.problems.constFirst().reasonCode, QStringLiteral("ctk_platform_state_mismatch"));
    QCOMPARE(snapshot.problems.constFirst().pluginId, QStringLiteral("org.medicalpro.user_management"));
}

void PlatformDiagnosticsServiceTest::buildSnapshot_does_not_report_mismatch_when_platform_state_is_ready()
{
    PlatformStateStore store;
    store.replaceDescriptors({
        makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement"))
    });
    store.setRuntimeMode(PlatformRuntimeMode::FacadeMode);
    store.setPluginState(QStringLiteral("org.medicalpro.user_management"), PlatformPluginState::Ready);

    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.startedPlugins = QStringList{QStringLiteral("UserManagement")};
    observation.pluginStates.insert(QStringLiteral("UserManagement"), QStringLiteral("ACTIVE"));

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot(observation);

    bool foundMismatch = false;
    for (const auto& problem : snapshot.problems) {
        if (problem.reasonCode == QStringLiteral("ctk_platform_state_mismatch")) {
            foundMismatch = true;
            break;
        }
    }
    QVERIFY(!foundMismatch);
}

void PlatformDiagnosticsServiceTest::buildSnapshot_deduplicates_and_sorts_problems_after_mismatch_append()
{
    PlatformStateStore store;
    store.replaceDescriptors({
        makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement"))
    });
    store.setRuntimeMode(PlatformRuntimeMode::FacadeMode);
    store.setPluginState(QStringLiteral("org.medicalpro.user_management"), PlatformPluginState::Discovered);

    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.startedPlugins = QStringList{QStringLiteral("UserManagement")};
    observation.pluginStates.insert(QStringLiteral("UserManagement"), QStringLiteral("ACTIVE"));
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginFailed,
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformLifecycleStep::Start,
            PlatformLifecycleResult::Failed,
            200,
            120,
            true,
            QStringLiteral("plugin_start_failed"),
            QStringLiteral("startup failed")),
        makeEvent(
            PlatformLifecycleEventKind::PluginFailed,
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformLifecycleStep::Start,
            PlatformLifecycleResult::Failed,
            220,
            100,
            false,
            QStringLiteral("ctk_platform_state_mismatch"),
            QStringLiteral("CTK reports plugin UserManagement as started but platform state is 0"))
    };

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot(observation);

    int mismatchCount = 0;
    for (const auto& problem : snapshot.problems) {
        if (problem.reasonCode == QStringLiteral("ctk_platform_state_mismatch")) mismatchCount++;
    }
    QCOMPARE(mismatchCount, 1);
    QVERIFY(!snapshot.problems.isEmpty());
    QCOMPARE(snapshot.problems.constFirst().reasonCode, QStringLiteral("plugin_start_failed"));
}

void PlatformDiagnosticsServiceTest::stateStore_supports_cross_thread_read_write()
{
    PlatformStateStore store;
    store.replaceDescriptors({
        makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement")),
        makeDescriptor(QStringLiteral("org.medicalpro.dicom_viewer"), QStringLiteral("DicomViewer"))
    });
    store.setRuntimeMode(PlatformRuntimeMode::OrchestrateCore);

    std::atomic_bool failed{false};
    std::thread writer([&store, &failed]() {
        for (int i = 0; i < 1000; ++i) {
            store.setPluginState(
                QStringLiteral("org.medicalpro.user_management"),
                i % 2 == 0 ? PlatformPluginState::Starting : PlatformPluginState::Ready);
            store.setPluginState(
                QStringLiteral("org.medicalpro.dicom_viewer"),
                i % 2 == 0 ? PlatformPluginState::Installed : PlatformPluginState::Ready);

            if (store.pluginSnapshots().size() != 2) {
                failed.store(true);
                return;
            }
        }
    });

    std::thread reader([&store, &failed]() {
        for (int i = 0; i < 1000; ++i) {
            const auto snapshots = store.pluginSnapshots();
            const auto capability = store.capabilitySnapshot();
            if (snapshots.size() != 2 || capability.runtimeMode != PlatformRuntimeMode::OrchestrateCore) {
                failed.store(true);
                return;
            }
        }
    });

    writer.join();
    reader.join();
    QVERIFY(!failed.load());
}

QTEST_APPLESS_MAIN(PlatformDiagnosticsServiceTest)
#include "PlatformDiagnosticsServiceTest.moc"
