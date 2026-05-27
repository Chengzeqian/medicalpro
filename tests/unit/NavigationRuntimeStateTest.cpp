#include <QtTest/QtTest>

#include "UI/NewPages/Navigation/navigation_runtime_state.h"

class NavigationRuntimeStateTest : public QObject
{
    Q_OBJECT

private slots:
    void state_keeps_case_context_and_starts_without_runtime_snapshots();
    void state_stores_tracking_quality_snapshot();
    void state_stores_registration_result_snapshot();
    void state_stores_confidence_result_snapshot();
    void state_stores_target_region_and_digital_twin_snapshots();
    void state_tracks_active_instrument_visibility_and_pose_summary();
    void state_clears_runtime_snapshots_when_case_context_changes();
};

void NavigationRuntimeStateTest::state_keeps_case_context_and_starts_without_runtime_snapshots()
{
    NavigationRuntimeState state;

    QVERIFY(state.caseId().isEmpty());
    QVERIFY(state.trackingSessionId().isEmpty());
    QVERIFY(state.navigationToolId().isEmpty());
    QVERIFY(!state.hasTrackingQuality());
    QVERIFY(!state.hasRegistrationResult());
    QVERIFY(!state.hasConfidenceResult());

    state.setCaseContext(QStringLiteral("case-001"), QStringLiteral("tracking-001"), QStringLiteral("tool-001"));

    QCOMPARE(state.caseId(), QStringLiteral("case-001"));
    QCOMPARE(state.trackingSessionId(), QStringLiteral("tracking-001"));
    QCOMPARE(state.navigationToolId(), QStringLiteral("tool-001"));
}

void NavigationRuntimeStateTest::state_stores_tracking_quality_snapshot()
{
    NavigationRuntimeState state;
    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.42);

    state.setTrackingQuality(trackingQuality);

    QVERIFY(state.hasTrackingQuality());
    QCOMPARE(state.trackingQuality().value(QStringLiteral("calibrated")).toBool(), true);
    QCOMPARE(state.trackingQuality().value(QStringLiteral("calibration_accuracy_mm")).toDouble(), 0.42);
}

void NavigationRuntimeStateTest::state_stores_registration_result_snapshot()
{
    NavigationRuntimeState state;
    PointRegistrationResult result;
    result.targetRegionTre = 1.6;
    result.coverageScore = 0.81;

    state.setRegistrationResult(result);

    QVERIFY(state.hasRegistrationResult());
    QCOMPARE(state.registrationResult().targetRegionTre, 1.6);
    QCOMPARE(state.registrationResult().coverageScore, 0.81);
}

void NavigationRuntimeStateTest::state_stores_confidence_result_snapshot()
{
    NavigationRuntimeState state;
    NavigationConfidenceResult result;
    result.allowNavigation = true;
    result.score = 0.88;

    state.setConfidenceResult(result);

    QVERIFY(state.hasConfidenceResult());
    QCOMPARE(state.confidenceResult().allowNavigation, true);
    QCOMPARE(state.confidenceResult().score, 0.88);
}

void NavigationRuntimeStateTest::state_stores_target_region_and_digital_twin_snapshots()
{
    NavigationRuntimeState state;

    DigitalTwinTargetRegionDefinition targetRegion;
    targetRegion.available = true;
    targetRegion.centerPatient = QVector3D(1.0f, 2.0f, 3.0f);
    targetRegion.radiusMm = 6.5;

    TargetRegionNavigationStatus targetStatus;
    targetStatus.targetRegionAvailable = true;
    targetStatus.distanceToTargetMm = 1.8;

    DigitalTwinRiskReport riskReport;
    riskReport.dominantRiskSource = QStringLiteral("registration");

    DigitalTwinState twinState;
    twinState.valid = true;
    twinState.twinConfidenceScore = 0.73;

    state.setTargetRegionDefinition(targetRegion);
    state.setTargetRegionNavigationStatus(targetStatus);
    state.setDigitalTwinRiskReport(riskReport);
    state.setDigitalTwinState(twinState);

    QVERIFY(state.hasTargetRegionDefinition());
    QVERIFY(state.hasTargetRegionNavigationStatus());
    QVERIFY(state.hasDigitalTwinRiskReport());
    QVERIFY(state.hasDigitalTwinState());
    QCOMPARE(state.targetRegionDefinition().centerPatient, QVector3D(1.0f, 2.0f, 3.0f));
    QCOMPARE(state.targetRegionNavigationStatus().distanceToTargetMm, 1.8);
    QCOMPARE(state.digitalTwinRiskReport().dominantRiskSource, QStringLiteral("registration"));
    QCOMPARE(state.digitalTwinState().twinConfidenceScore, 0.73);
}

void NavigationRuntimeStateTest::state_tracks_active_instrument_visibility_and_pose_summary()
{
    NavigationRuntimeState state;

    state.setTrackedInstrumentVisible(QStringLiteral("instrument:probe-main"), true);
    state.setActiveInstrumentPoseSummary(
        QStringLiteral("instrument:probe-main"),
        QStringLiteral("tx=1.0,ty=2.0,tz=3.0"));

    QCOMPARE(state.isTrackedInstrumentVisible(QStringLiteral("instrument:probe-main")), true);
    QCOMPARE(
        state.activeInstrumentPoseSummary(QStringLiteral("instrument:probe-main")),
        QStringLiteral("tx=1.0,ty=2.0,tz=3.0"));
}

void NavigationRuntimeStateTest::state_clears_runtime_snapshots_when_case_context_changes()
{
    NavigationRuntimeState state;
    state.setCaseContext(QStringLiteral("case-001"), QStringLiteral("tracking-001"), QStringLiteral("tool-001"));

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    state.setTrackingQuality(trackingQuality);

    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 1.4;
    state.setRegistrationResult(registrationResult);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = true;
    confidenceResult.score = 0.82;
    state.setConfidenceResult(confidenceResult);

    DigitalTwinTargetRegionDefinition targetRegion;
    targetRegion.available = true;
    state.setTargetRegionDefinition(targetRegion);

    TargetRegionNavigationStatus targetStatus;
    targetStatus.targetRegionAvailable = true;
    state.setTargetRegionNavigationStatus(targetStatus);

    DigitalTwinRiskReport riskReport;
    riskReport.dominantRiskSource = QStringLiteral("tracking");
    state.setDigitalTwinRiskReport(riskReport);

    DigitalTwinState twinState;
    twinState.valid = true;
    state.setDigitalTwinState(twinState);

    state.setCaseContext(QStringLiteral("case-002"), QStringLiteral("tracking-002"), QStringLiteral("tool-002"));

    QCOMPARE(state.caseId(), QStringLiteral("case-002"));
    QCOMPARE(state.trackingSessionId(), QStringLiteral("tracking-002"));
    QCOMPARE(state.navigationToolId(), QStringLiteral("tool-002"));
    QVERIFY(!state.hasTrackingQuality());
    QVERIFY(!state.hasRegistrationResult());
    QVERIFY(!state.hasConfidenceResult());
    QVERIFY(!state.hasTargetRegionDefinition());
    QVERIFY(!state.hasTargetRegionNavigationStatus());
    QVERIFY(!state.hasDigitalTwinRiskReport());
    QVERIFY(!state.hasDigitalTwinState());
}

QTEST_APPLESS_MAIN(NavigationRuntimeStateTest)
#include "NavigationRuntimeStateTest.moc"
