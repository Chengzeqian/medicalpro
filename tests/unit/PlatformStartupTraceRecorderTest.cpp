#include <QtTest>

#include <atomic>
#include <thread>

#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"

class PlatformStartupTraceRecorderTest : public QObject
{
    Q_OBJECT

private slots:
    void records_phase_and_plugin_spans();
    void records_skipped_step_for_runtime_mode();
    void records_service_ready_without_fake_start_event();
    void service_ready_span_survives_plugin_identity_resolution_change();
    void preserves_non_mode_skip_as_step_finish_event();
    void concurrent_reads_return_stable_snapshots_while_recording();
};

void PlatformStartupTraceRecorderTest::records_phase_and_plugin_spans()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::OrchestrateCore);

    recorder.recordPhaseStarted(QStringLiteral("framework_init"), QStringLiteral("Initialize framework"), true);
    QTest::qSleep(1);
    recorder.recordPhaseFinished(
        QStringLiteral("framework_init"),
        PlatformLifecycleResult::Succeeded,
        QString(),
        QStringLiteral("framework ready"));

    recorder.recordPluginStepStarted(
        QStringLiteral("org.medicalpro.viewer"),
        QStringLiteral("ViewerPlugin"),
        PlatformLifecycleStep::Start,
        true);
    QTest::qSleep(1);
    recorder.recordPluginStepFinished(
        QStringLiteral("org.medicalpro.viewer"),
        QStringLiteral("ViewerPlugin"),
        PlatformLifecycleStep::Start,
        PlatformLifecycleResult::Succeeded,
        QString(),
        QStringLiteral("plugin ready"));

    recorder.finishSession();

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();

    QCOMPARE(trace.size(), 2);
    QCOMPARE(events.size(), 6);

    const auto& phaseSpan = trace.at(0);
    QCOMPARE(phaseSpan.spanId, QStringLiteral("phase:framework_init"));
    QCOMPARE(phaseSpan.parentSpanId, QStringLiteral("startup_session"));
    QCOMPARE(phaseSpan.phaseKey, QStringLiteral("framework_init"));
    QCOMPARE(phaseSpan.phaseLabel, QStringLiteral("Initialize framework"));
    QCOMPARE(phaseSpan.pluginId, QString());
    QCOMPARE(phaseSpan.step, PlatformLifecycleStep::None);
    QCOMPARE(phaseSpan.result, PlatformLifecycleResult::Succeeded);
    QVERIFY(phaseSpan.success);
    QVERIFY(phaseSpan.blockingStartup);
    QVERIFY(phaseSpan.endOffsetMs >= phaseSpan.startOffsetMs);
    QVERIFY(phaseSpan.elapsedMs >= 1);
    QCOMPARE(phaseSpan.reasonCode, QString());
    QCOMPARE(phaseSpan.detail, QStringLiteral("framework ready"));

    const auto& pluginSpan = trace.at(1);
    QCOMPARE(pluginSpan.spanId, QStringLiteral("plugin:org.medicalpro.viewer:start"));
    QCOMPARE(pluginSpan.parentSpanId, QStringLiteral("startup_session"));
    QCOMPARE(pluginSpan.phaseKey, QStringLiteral("org.medicalpro.viewer"));
    QCOMPARE(pluginSpan.phaseLabel, QStringLiteral("start"));
    QCOMPARE(pluginSpan.pluginId, QStringLiteral("org.medicalpro.viewer"));
    QCOMPARE(pluginSpan.symbolicName, QStringLiteral("ViewerPlugin"));
    QCOMPARE(pluginSpan.step, PlatformLifecycleStep::Start);
    QCOMPARE(pluginSpan.result, PlatformLifecycleResult::Succeeded);
    QVERIFY(pluginSpan.success);
    QVERIFY(pluginSpan.blockingStartup);
    QVERIFY(pluginSpan.endOffsetMs >= pluginSpan.startOffsetMs);
    QVERIFY(pluginSpan.elapsedMs >= 1);
    QCOMPARE(pluginSpan.detail, QStringLiteral("plugin ready"));

    QCOMPARE(events.at(1).kind, PlatformLifecycleEventKind::PhaseStarted);
    QCOMPARE(events.at(2).kind, PlatformLifecycleEventKind::PhaseFinished);
    QCOMPARE(events.at(3).kind, PlatformLifecycleEventKind::PluginStartStarted);
    QCOMPARE(events.at(4).kind, PlatformLifecycleEventKind::PluginStartFinished);
    QVERIFY(!events.at(1).sessionId.isEmpty());
    QVERIFY(events.at(1).blockingStartup);
    QCOMPARE(events.at(4).reasonCode, QString());
    QCOMPARE(events.at(4).durationMs, pluginSpan.elapsedMs);
}

void PlatformStartupTraceRecorderTest::records_skipped_step_for_runtime_mode()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::ObserveOnly);

    recorder.recordPluginStepStarted(
        QStringLiteral("org.medicalpro.viewer"),
        QStringLiteral("ViewerPlugin"),
        PlatformLifecycleStep::Start,
        true);
    recorder.recordPluginStepFinished(
        QStringLiteral("org.medicalpro.viewer"),
        QStringLiteral("ViewerPlugin"),
        PlatformLifecycleStep::Start,
        PlatformLifecycleResult::Skipped,
        QStringLiteral("runtime_mode"),
        QStringLiteral("observe_only skips plugin start"));

    recorder.finishSession();

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();

    QCOMPARE(trace.size(), 1);
    QCOMPARE(events.at(2).kind, PlatformLifecycleEventKind::PluginSkippedByMode);
    QCOMPARE(events.at(2).runtimeMode, PlatformRuntimeMode::ObserveOnly);
    QVERIFY(events.at(2).blockingStartup);
    QCOMPARE(events.at(2).reasonCode, QStringLiteral("runtime_mode"));

    const auto& entry = trace.constFirst();
    QCOMPARE(entry.spanId, QStringLiteral("plugin:org.medicalpro.viewer:start"));
    QCOMPARE(entry.parentSpanId, QStringLiteral("startup_session"));
    QCOMPARE(entry.phaseKey, QStringLiteral("org.medicalpro.viewer"));
    QCOMPARE(entry.phaseLabel, QStringLiteral("start"));
    QCOMPARE(entry.pluginId, QStringLiteral("org.medicalpro.viewer"));
    QCOMPARE(entry.symbolicName, QStringLiteral("ViewerPlugin"));
    QCOMPARE(entry.step, PlatformLifecycleStep::Start);
    QCOMPARE(entry.result, PlatformLifecycleResult::Skipped);
    QVERIFY(entry.success);
    QVERIFY(entry.blockingStartup);
    QCOMPARE(entry.reasonCode, QStringLiteral("runtime_mode"));
    QCOMPARE(entry.detail, QStringLiteral("observe_only skips plugin start"));
}

void PlatformStartupTraceRecorderTest::records_service_ready_without_fake_start_event()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::OrchestrateCore);

    recorder.recordPluginStepStarted(
        QStringLiteral("org.medicalpro.viewer"),
        QStringLiteral("ViewerPlugin"),
        PlatformLifecycleStep::ServiceReady,
        true);
    QTest::qSleep(1);
    recorder.recordPluginStepFinished(
        QStringLiteral("org.medicalpro.viewer"),
        QStringLiteral("ViewerPlugin"),
        PlatformLifecycleStep::ServiceReady,
        PlatformLifecycleResult::Succeeded,
        QString(),
        QStringLiteral("service became ready"));

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();

    QCOMPARE(events.size(), 2);
    QCOMPARE(events.at(0).kind, PlatformLifecycleEventKind::StartupSessionStarted);
    QCOMPARE(events.at(1).kind, PlatformLifecycleEventKind::PluginServiceReady);
    QCOMPARE(events.at(1).step, PlatformLifecycleStep::ServiceReady);
    QCOMPARE(events.at(1).result, PlatformLifecycleResult::Succeeded);

    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().step, PlatformLifecycleStep::ServiceReady);
    QVERIFY(trace.constFirst().success);
}

void PlatformStartupTraceRecorderTest::service_ready_span_survives_plugin_identity_resolution_change()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::FacadeMode);

    recorder.recordPluginStepStarted(
        QString(),
        QStringLiteral("DicomViewer"),
        PlatformLifecycleStep::ServiceReady,
        true);
    QTest::qSleep(2);
    recorder.recordPluginStepFinished(
        QStringLiteral("org.medicalpro.dicom_viewer"),
        QStringLiteral("DicomViewer"),
        PlatformLifecycleStep::ServiceReady,
        PlatformLifecycleResult::Succeeded,
        QString(),
        QStringLiteral("service ready"));

    const auto trace = recorder.startupTrace();
    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().step, PlatformLifecycleStep::ServiceReady);
    QVERIFY2(trace.constFirst().elapsedMs >= 1, "ServiceReady elapsedMs should reflect the started span");
}

void PlatformStartupTraceRecorderTest::preserves_non_mode_skip_as_step_finish_event()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::FacadeMode);

    recorder.recordPluginStepStarted(
        QStringLiteral("org.medicalpro.viewer"),
        QStringLiteral("ViewerPlugin"),
        PlatformLifecycleStep::Warmup,
        false);
    recorder.recordPluginStepFinished(
        QStringLiteral("org.medicalpro.viewer"),
        QStringLiteral("ViewerPlugin"),
        PlatformLifecycleStep::Warmup,
        PlatformLifecycleResult::Skipped,
        QStringLiteral("warmup_not_required"),
        QStringLiteral("warmup intentionally skipped"));

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();

    QCOMPARE(events.size(), 3);
    QCOMPARE(events.at(1).kind, PlatformLifecycleEventKind::PluginWarmupStarted);
    QCOMPARE(events.at(2).kind, PlatformLifecycleEventKind::PluginWarmupFinished);
    QCOMPARE(events.at(2).result, PlatformLifecycleResult::Skipped);
    QCOMPARE(events.at(2).reasonCode, QStringLiteral("warmup_not_required"));

    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().result, PlatformLifecycleResult::Skipped);
    QVERIFY(trace.constFirst().success);
    QCOMPARE(trace.constFirst().reasonCode, QStringLiteral("warmup_not_required"));
}

void PlatformStartupTraceRecorderTest::concurrent_reads_return_stable_snapshots_while_recording()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::OrchestrateCore);

    constexpr int iterations = 200;
    std::atomic<bool> writerFinished = false;
    int snapshotReads = 0;
    bool eventsCountRegressed = false;
    bool traceCountRegressed = false;
    int previousEventsCount = 0;
    int previousTraceCount = 0;

    std::thread writer([&recorder, &writerFinished]() {
        for (int i = 0; i < iterations; ++i) {
            const auto pluginId = QStringLiteral("org.medicalpro.viewer_%1").arg(i);
            const auto ctkSymbolicName = QStringLiteral("ViewerPlugin%1").arg(i);
            recorder.recordPluginStepStarted(
                pluginId,
                ctkSymbolicName,
                PlatformLifecycleStep::Start,
                (i % 2) == 0);
            recorder.recordPluginStepFinished(
                pluginId,
                ctkSymbolicName,
                PlatformLifecycleStep::Start,
                PlatformLifecycleResult::Succeeded,
                QString(),
                QStringLiteral("plugin ready"));
        }

        writerFinished.store(true);
    });

    while (!writerFinished.load()) {
        const auto events = recorder.lifecycleEvents();
        const auto trace = recorder.startupTrace();
        if (events.size() < previousEventsCount) {
            eventsCountRegressed = true;
            break;
        }
        if (trace.size() < previousTraceCount) {
            traceCountRegressed = true;
            break;
        }

        previousEventsCount = events.size();
        previousTraceCount = trace.size();
        ++snapshotReads;
        std::this_thread::yield();
    }

    writer.join();

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();
    QVERIFY2(!eventsCountRegressed, "Lifecycle event snapshot count regressed while recording");
    QVERIFY2(!traceCountRegressed, "Startup trace snapshot count regressed while recording");
    QVERIFY(snapshotReads > 0);
    QCOMPARE(trace.size(), iterations);
    QCOMPARE(events.size(), 1 + (iterations * 2));
}

QTEST_APPLESS_MAIN(PlatformStartupTraceRecorderTest)
#include "PlatformStartupTraceRecorderTest.moc"
