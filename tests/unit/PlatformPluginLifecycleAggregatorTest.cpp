#include <QtTest>

#include "Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h"
#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Diagnostics/PlatformPluginLifecycleAggregator.h"

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
    bool blockingStartup = true,
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

class PlatformPluginLifecycleAggregatorTest : public QObject
{
    Q_OBJECT

private slots:
    void aggregates_install_start_service_ready_and_warmup();
    void identifies_failed_plugin_and_blocking_point();
    void separates_ready_path_from_warmup_tail();
    void fallback_recovery_hint_is_readable_text();
    void blocking_span_prefers_earliest_failed_blocking_event();
    void blocking_span_falls_back_to_longest_blocking_event_when_no_failure();
    void slowest_plugin_prefers_blocking_plugins_and_uses_tiebreak_order();
};

void PlatformPluginLifecycleAggregatorTest::aggregates_install_start_service_ready_and_warmup()
{
    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginInstallFinished,
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformLifecycleStep::Install,
            PlatformLifecycleResult::Succeeded,
            120,
            120),
        makeEvent(
            PlatformLifecycleEventKind::PluginStartFinished,
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformLifecycleStep::Start,
            PlatformLifecycleResult::Succeeded,
            320,
            200),
        makeEvent(
            PlatformLifecycleEventKind::PluginServiceReady,
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Succeeded,
            460,
            140),
        makeEvent(
            PlatformLifecycleEventKind::PluginWarmupFinished,
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformLifecycleStep::Warmup,
            PlatformLifecycleResult::Succeeded,
            610,
            150,
            false)
    };

    PlatformPluginLifecycleAggregator aggregator;
    const auto aggregation = aggregator.aggregate(
        observation.lifecycleEvents,
        {makeDescriptor(QStringLiteral("org.medicalpro.dicom_viewer"), QStringLiteral("DicomViewer"))},
        observation);

    QCOMPARE(aggregation.pluginLifecycle.size(), 1);
    const auto plugin = aggregation.pluginLifecycle.constFirst();
    QCOMPARE(plugin.pluginId, QStringLiteral("org.medicalpro.dicom_viewer"));
    QCOMPARE(plugin.installMs, 120);
    QCOMPARE(plugin.startMs, 200);
    QCOMPARE(plugin.serviceReadyMs, 140);
    QCOMPARE(plugin.warmupMs, 150);
    QVERIFY(plugin.serviceReadyObserved);
    QVERIFY(plugin.warmupCompleted);
}

void PlatformPluginLifecycleAggregatorTest::identifies_failed_plugin_and_blocking_point()
{
    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginInstallFinished,
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            PlatformLifecycleStep::Install,
            PlatformLifecycleResult::Succeeded,
            100,
            100),
        makeEvent(
            PlatformLifecycleEventKind::PluginFailed,
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            PlatformLifecycleStep::Start,
            PlatformLifecycleResult::Failed,
            420,
            320,
            true,
            QStringLiteral("plugin_start_failed"),
            QStringLiteral("RegistrationCore activation failed"))
    };

    PlatformPluginLifecycleAggregator aggregator;
    const auto aggregation = aggregator.aggregate(
        observation.lifecycleEvents,
        {makeDescriptor(QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore"))},
        observation);

    QCOMPARE(aggregation.summary.slowestPluginId, QStringLiteral("org.medicalpro.registration_core"));
    QVERIFY(!aggregation.summary.blockingSpanLabel.isEmpty());
    QVERIFY(!aggregation.summary.failurePointLabel.isEmpty());
    QCOMPARE(aggregation.pluginLifecycle.size(), 1);
    QCOMPARE(aggregation.pluginLifecycle.constFirst().state, PlatformPluginState::Failed);
    QVERIFY(!aggregation.problems.isEmpty());
    QCOMPARE(aggregation.problems.constFirst().reasonCode, QStringLiteral("plugin_start_failed"));
}

void PlatformPluginLifecycleAggregatorTest::separates_ready_path_from_warmup_tail()
{
    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginServiceReady,
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Succeeded,
            400,
            400,
            true),
        makeEvent(
            PlatformLifecycleEventKind::PluginWarmupFinished,
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformLifecycleStep::Warmup,
            PlatformLifecycleResult::Succeeded,
            950,
            550,
            false)
    };

    PlatformPluginLifecycleAggregator aggregator;
    const auto aggregation = aggregator.aggregate(
        observation.lifecycleEvents,
        {makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement"))},
        observation);

    QCOMPARE(aggregation.summary.startupReadyPathMs, 400);
    QCOMPARE(aggregation.summary.startupWarmupTailMs, 550);
}

void PlatformPluginLifecycleAggregatorTest::fallback_recovery_hint_is_readable_text()
{
    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginFailed,
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformLifecycleStep::Start,
            PlatformLifecycleResult::Failed,
            300,
            180,
            true,
            QStringLiteral("plugin_start_failed"),
            QStringLiteral("activation failed"))
    };

    PlatformPluginLifecycleAggregator aggregator;
    const auto aggregation = aggregator.aggregate(
        observation.lifecycleEvents,
        {makeDescriptor(QStringLiteral("org.medicalpro.dicom_viewer"), QStringLiteral("DicomViewer"))},
        observation);

    QVERIFY(!aggregation.recoveryHints.isEmpty());
    QVERIFY(aggregation.recoveryHints.join(QStringLiteral(" | ")).contains(QStringLiteral("Check plugin dependencies")));
}

void PlatformPluginLifecycleAggregatorTest::blocking_span_prefers_earliest_failed_blocking_event()
{
    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginStartFinished,
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformLifecycleStep::Start,
            PlatformLifecycleResult::Succeeded,
            480,
            480,
            true),
        makeEvent(
            PlatformLifecycleEventKind::PluginFailed,
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            PlatformLifecycleStep::Start,
            PlatformLifecycleResult::Failed,
            240,
            160,
            true,
            QStringLiteral("plugin_start_failed"),
            QStringLiteral("earliest failed blocking span")),
        makeEvent(
            PlatformLifecycleEventKind::PluginFailed,
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Timeout,
            360,
            300,
            true,
            QStringLiteral("service_ready_timeout"),
            QStringLiteral("later timeout blocking span"))
    };

    PlatformPluginLifecycleAggregator aggregator;
    const auto aggregation = aggregator.aggregate(
        observation.lifecycleEvents,
        {
            makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement")),
            makeDescriptor(QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore")),
            makeDescriptor(QStringLiteral("org.medicalpro.dicom_viewer"), QStringLiteral("DicomViewer"))
        },
        observation);

    QCOMPARE(aggregation.summary.blockingSpanLabel, QStringLiteral("org.medicalpro.registration_core start"));
}

void PlatformPluginLifecycleAggregatorTest::blocking_span_falls_back_to_longest_blocking_event_when_no_failure()
{
    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginServiceReady,
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Succeeded,
            520,
            120,
            true),
        makeEvent(
            PlatformLifecycleEventKind::PluginStartFinished,
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            PlatformLifecycleStep::Start,
            PlatformLifecycleResult::Succeeded,
            360,
            360,
            true)
    };

    PlatformPluginLifecycleAggregator aggregator;
    const auto aggregation = aggregator.aggregate(
        observation.lifecycleEvents,
        {
            makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement")),
            makeDescriptor(QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore"))
        },
        observation);

    QCOMPARE(aggregation.summary.blockingSpanLabel, QStringLiteral("org.medicalpro.registration_core start"));
}

void PlatformPluginLifecycleAggregatorTest::slowest_plugin_prefers_blocking_plugins_and_uses_tiebreak_order()
{
    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.lifecycleEvents = {
        makeEvent(
            PlatformLifecycleEventKind::PluginServiceReady,
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Succeeded,
            300,
            300,
            true),
        makeEvent(
            PlatformLifecycleEventKind::PluginStartFinished,
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            PlatformLifecycleStep::Start,
            PlatformLifecycleResult::Succeeded,
            300,
            300,
            true),
        makeEvent(
            PlatformLifecycleEventKind::PluginServiceReady,
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformLifecycleStep::ServiceReady,
            PlatformLifecycleResult::Succeeded,
            500,
            500,
            false),
        makeEvent(
            PlatformLifecycleEventKind::PluginWarmupFinished,
            QStringLiteral("org.medicalpro.optical_tracking"),
            QStringLiteral("OpticalTracking"),
            PlatformLifecycleStep::Warmup,
            PlatformLifecycleResult::Succeeded,
            650,
            650,
            false)
    };

    PlatformPluginLifecycleAggregator aggregator;
    const auto aggregation = aggregator.aggregate(
        observation.lifecycleEvents,
        {
            makeDescriptor(QStringLiteral("org.medicalpro.user_management"), QStringLiteral("UserManagement")),
            makeDescriptor(QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore")),
            makeDescriptor(QStringLiteral("org.medicalpro.dicom_viewer"), QStringLiteral("DicomViewer")),
            makeDescriptor(QStringLiteral("org.medicalpro.optical_tracking"), QStringLiteral("OpticalTracking"))
        },
        observation);

    QCOMPARE(aggregation.summary.slowestPluginId, QStringLiteral("org.medicalpro.user_management"));

    bool foundUserManagement = false;
    bool foundRegistrationCore = false;
    for (const auto& plugin : aggregation.pluginLifecycle) {
        if (plugin.pluginId == QStringLiteral("org.medicalpro.user_management")) {
            foundUserManagement = true;
            QCOMPARE(plugin.startupBlocked, true);
        }
        if (plugin.pluginId == QStringLiteral("org.medicalpro.registration_core")) {
            foundRegistrationCore = true;
            QCOMPARE(plugin.startupBlocked, true);
        }
    }
    QVERIFY(foundUserManagement);
    QVERIFY(foundRegistrationCore);
}

QTEST_APPLESS_MAIN(PlatformPluginLifecycleAggregatorTest)
#include "PlatformPluginLifecycleAggregatorTest.moc"
