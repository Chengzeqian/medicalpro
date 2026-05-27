#include <QtTest/QtTest>

#include "Framework/Navigation/ankle_navigation_types.h"
#include "Framework/Navigation/navigation_evaluation_service.h"
#include "Framework/Navigation/navigation_pose_frame.h"
#include "UI/NewPages/Navigation/navigation_runtime_coordinator.h"

class NavigationRuntimeCoordinatorContractTest : public QObject
{
    Q_OBJECT

private slots:
    void coordinator_depends_on_runtime_state_and_recomputes_confidence();
    void coordinator_exposes_evaluation_snapshot_persistence_entry();
    void coordinator_keeps_last_confidence_empty_until_required_snapshots_exist();
    void coordinator_uses_configured_cases_root_for_default_persistence_actions();
    void coordinator_persists_digital_twin_metrics_into_evaluation_report();
};

void NavigationRuntimeCoordinatorContractTest::coordinator_depends_on_runtime_state_and_recomputes_confidence()
{
    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(QStringLiteral("case-001"), QStringLiteral("tracking-001"), QStringLiteral("tool-001"));

    NavigationRuntimeCoordinator coordinator(&runtimeState);

    QCOMPARE(coordinator.runtimeState(), &runtimeState);

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.2);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.98);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.35);

    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.rmsError = 0.7;
    registrationResult.targetRegionTre = 1.1;
    registrationResult.coverageScore = 0.92;
    registrationResult.metrics.insert(QStringLiteral("refined_rms"), 0.6);

    coordinator.handleTrackingQuality(trackingQuality);
    coordinator.handleRegistrationResult(registrationResult);

    QVERIFY(runtimeState.hasTrackingQuality());
    QVERIFY(runtimeState.hasRegistrationResult());

    QVariantMap calibrationSnapshot;
    calibrationSnapshot.insert(QStringLiteral("calibrated"), true);
    calibrationSnapshot.insert(QStringLiteral("calibration_accuracy_mm"), 0.28);
    calibrationSnapshot.insert(QStringLiteral("tracking_jitter_mm"), 0.2);
    calibrationSnapshot.insert(QStringLiteral("visible_frame_ratio"), 0.98);
    coordinator.handleCalibrationCompleted(calibrationSnapshot);

    QCOMPARE(runtimeState.trackingQuality().value(QStringLiteral("calibration_accuracy_mm")).toDouble(), 0.28);

    coordinator.recomputeConfidence();

    QVERIFY(runtimeState.hasConfidenceResult());
    QVERIFY(runtimeState.confidenceResult().allowNavigation);
    QVERIFY(runtimeState.confidenceResult().score >= 0.6);
}

void NavigationRuntimeCoordinatorContractTest::coordinator_exposes_evaluation_snapshot_persistence_entry()
{
    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(QStringLiteral("case-002"), QStringLiteral("tracking-002"), QStringLiteral("tool-002"));

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.3);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.96);
    trackingQuality.insert(QStringLiteral("tracking_profile"), QStringLiteral("stable"));
    trackingQuality.insert(QStringLiteral("tracking_confidence_score"), 0.91);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.41);
    runtimeState.setTrackingQuality(trackingQuality);

    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 1.3;
    registrationResult.coverageScore = 0.87;
    registrationResult.metrics.insert(QStringLiteral("registration_mode"), QStringLiteral("ankle_two_stage_constrained"));
    runtimeState.setRegistrationResult(registrationResult);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = true;
    confidenceResult.score = 0.84;
    confidenceResult.recommendations.append(QStringLiteral("可进入导航"));
    runtimeState.setConfidenceResult(confidenceResult);

    bool loadCalled = false;
    bool saveCalled = false;
    bool exportCalled = false;
    bool summaryCalled = false;
    QString loadedCaseId;
    QString exportedMetricsCaseId;
    QString exportedSummaryCaseId;
    AnkleEvaluationReport savedReport;

    NavigationRuntimeCoordinator::PersistenceActions persistenceActions;
    persistenceActions.loadEvaluationSnapshot = [&loadCalled, &loadedCaseId](const QString& caseId) -> AnkleEvaluationSnapshot {
        loadCalled = true;
        loadedCaseId = caseId;
        AnkleEvaluationSnapshot snapshot;
        snapshot.caseId = caseId;
        snapshot.hasEvaluationReport = true;
        snapshot.translationErrorMm = 2.4;
        snapshot.rotationErrorDeg = 1.7;
        snapshot.evaluationMetrics.insert(QStringLiteral("existing_metric"), 12.0);
        return snapshot;
    };
    persistenceActions.saveEvaluationReport = [&saveCalled, &savedReport](const AnkleEvaluationReport& report) -> bool {
        saveCalled = true;
        savedReport = report;
        return true;
    };
    persistenceActions.exportMetricsCsv = [&exportCalled, &exportedMetricsCaseId](const QString& caseId) -> bool {
        exportCalled = true;
        exportedMetricsCaseId = caseId;
        return true;
    };
    persistenceActions.exportCaseSummary = [&summaryCalled, &exportedSummaryCaseId](const QString& caseId) -> bool {
        summaryCalled = true;
        exportedSummaryCaseId = caseId;
        return true;
    };

    NavigationRuntimeCoordinator coordinator(&runtimeState, persistenceActions);

    coordinator.persistEvaluationReportSnapshot(true);

    QVERIFY(loadCalled);
    QVERIFY(saveCalled);
    QVERIFY(exportCalled);
    QVERIFY(summaryCalled);
    QCOMPARE(loadedCaseId, QStringLiteral("case-002"));
    QCOMPARE(exportedMetricsCaseId, QStringLiteral("case-002"));
    QCOMPARE(exportedSummaryCaseId, QStringLiteral("case-002"));
    QCOMPARE(savedReport.caseId, QStringLiteral("case-002"));
    QCOMPARE(savedReport.translationErrorMm, 1.3);
    QCOMPARE(savedReport.rotationErrorDeg, 1.7);
    QCOMPARE(savedReport.allowNavigation, true);
    QCOMPARE(savedReport.confidenceScore, 0.84);
    QCOMPARE(savedReport.calibrated, true);
    QCOMPARE(savedReport.calibrationAccuracyMm, 0.41);
    QCOMPARE(savedReport.metrics.value(QStringLiteral("registration_mode")).toString(), QStringLiteral("ankle_two_stage_constrained"));
    QCOMPARE(savedReport.metrics.value(QStringLiteral("target_region_tre_mm")).toDouble(), 1.3);
    QCOMPARE(savedReport.metrics.value(QStringLiteral("coverage_score")).toDouble(), 0.87);
    QCOMPARE(savedReport.metrics.value(QStringLiteral("tracking_jitter_mm")).toDouble(), 0.3);
    QCOMPARE(savedReport.metrics.value(QStringLiteral("allow_navigation")).toBool(), true);
}

void NavigationRuntimeCoordinatorContractTest::coordinator_keeps_last_confidence_empty_until_required_snapshots_exist()
{
    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(QStringLiteral("case-003"), QStringLiteral("tracking-003"), QStringLiteral("tool-003"));

    NavigationRuntimeCoordinator coordinator(&runtimeState);

    coordinator.recomputeConfidence();
    QVERIFY(!runtimeState.hasConfidenceResult());

    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.rmsError = 0.6;
    registrationResult.targetRegionTre = 1.4;
    registrationResult.coverageScore = 0.82;
    registrationResult.metrics.insert(QStringLiteral("refined_rms"), 0.5);
    coordinator.handleRegistrationResult(registrationResult);
    coordinator.recomputeConfidence();
    QVERIFY(!runtimeState.hasConfidenceResult());

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.25);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.97);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.32);
    coordinator.handleTrackingQuality(trackingQuality);
    coordinator.recomputeConfidence();

    QVERIFY(runtimeState.hasConfidenceResult());
}

void NavigationRuntimeCoordinatorContractTest::coordinator_uses_configured_cases_root_for_default_persistence_actions()
{
    const QString casesRoot = QDir::temp().filePath(QStringLiteral("medicalpro_navigation_runtime_coordinator_cases_root_test"));
    QDir().mkpath(casesRoot);

    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(QStringLiteral("case-004"), QStringLiteral("tracking-004"), QStringLiteral("tool-004"));

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.22);
    runtimeState.setTrackingQuality(trackingQuality);

    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 1.5;
    registrationResult.coverageScore = 0.83;
    runtimeState.setRegistrationResult(registrationResult);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = true;
    confidenceResult.score = 0.79;
    runtimeState.setConfidenceResult(confidenceResult);

    NavigationRuntimeCoordinator coordinator(&runtimeState);
    coordinator.setCasesRoot(casesRoot);
    coordinator.persistEvaluationReportSnapshot();

    NavigationEvaluationService evaluationService(casesRoot);
    const AnkleEvaluationSnapshot snapshot = evaluationService.loadEvaluationSnapshot(QStringLiteral("case-004"));

    QVERIFY(snapshot.hasEvaluationReport);
    QCOMPARE(snapshot.caseId, QStringLiteral("case-004"));
    QCOMPARE(snapshot.translationErrorMm, 1.5);
    QCOMPARE(snapshot.allowNavigation, true);
}

void NavigationRuntimeCoordinatorContractTest::coordinator_persists_digital_twin_metrics_into_evaluation_report()
{
    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(
        QStringLiteral("case-twin-export-001"),
        QStringLiteral("tracking-001"),
        QStringLiteral("instrument:probe-main"));

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.86);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.80);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.74);

    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 2.4;
    registrationResult.coverageScore = 0.66;

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = false;
    confidenceResult.score = 0.39;

    AnkleEvaluationReport savedReport;
    NavigationRuntimeCoordinator::PersistenceActions persistenceActions;
    persistenceActions.loadEvaluationSnapshot = [](const QString& caseId) {
        AnkleEvaluationSnapshot snapshot;
        snapshot.caseId = caseId;
        return snapshot;
    };
    persistenceActions.saveEvaluationReport = [&savedReport](const AnkleEvaluationReport& report) {
        savedReport = report;
        return true;
    };
    persistenceActions.exportMetricsCsv = [](const QString&) { return true; };
    persistenceActions.exportCaseSummary = [](const QString&) { return true; };

    NavigationRuntimeCoordinator coordinator(&runtimeState, persistenceActions);

    DigitalTwinTargetRegionDefinition targetRegion;
    targetRegion.available = true;
    targetRegion.centerPatient = QVector3D(0.0f, 0.0f, 0.0f);
    targetRegion.radiusMm = 5.0;
    coordinator.setTargetRegionDefinition(targetRegion);

    QMatrix4x4 markerToTool;
    coordinator.handleCalibrationTransform(markerToTool);
    QMatrix4x4 patientToVtkWorld;
    coordinator.handleRegistrationTransform(patientToVtkWorld);

    NavigationPoseFrame frame;
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.trackingVisible = true;
    frame.timestamp = QDateTime::currentDateTimeUtc();
    frame.trackingToMarker.translate(9.0f, 0.0f, 0.0f);

    coordinator.handleTrackingQuality(trackingQuality);
    coordinator.handleRegistrationResult(registrationResult);
    runtimeState.setConfidenceResult(confidenceResult);
    coordinator.handlePoseFrame(frame);
    coordinator.persistEvaluationReportSnapshot();

    QVERIFY(runtimeState.hasDigitalTwinState());
    QVERIFY(runtimeState.hasTargetRegionNavigationStatus());
    QVERIFY(savedReport.metrics.contains(QStringLiteral("twin_confidence_score")));
    QVERIFY(savedReport.metrics.contains(QStringLiteral("target_region_distance_mm")));
    QVERIFY(savedReport.metrics.contains(QStringLiteral("local_risk_score")));
    QVERIFY(savedReport.metrics.contains(QStringLiteral("dominant_risk_source")));
    QVERIFY(savedReport.metrics.contains(QStringLiteral("re_register_recommended")));
}

QTEST_APPLESS_MAIN(NavigationRuntimeCoordinatorContractTest)
#include "NavigationRuntimeCoordinatorContractTest.moc"
