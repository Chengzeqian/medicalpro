#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"
#include "Framework/Navigation/navigation_evaluation_service.h"
#include "UI/NewPages/Navigation/navigation_runtime_state.h"
#include "UI/NewPages/Navigation/navigation_workspace_application_service.h"

class NavigationWorkspaceApplicationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void service_builds_workspace_snapshot_from_runtime_inputs();
    void service_evaluates_stage_gate_from_workspace_snapshot();
    void service_records_runtime_workspace_states_into_snapshot_truth_source();
    void service_builds_runtime_status_summaries_for_workspace_states();
    void service_preserves_navigation_run_facts_when_recording_gate_updates();
};

void NavigationWorkspaceApplicationServiceTest::service_builds_workspace_snapshot_from_runtime_inputs()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-301");
    manifest.patientId = QStringLiteral("patient-301");
    manifest.patientName = QStringLiteral("Patient 301");
    manifest.surgeryId = QStringLiteral("surgery-301");
    manifest.workflowStage = QStringLiteral("planning");
    manifest.dicomDir = QStringLiteral("dicom/series-001");
    manifest.modelAssets = {
        AnkleModelAsset {
            QStringLiteral("tibia"),
            QStringLiteral("models/tibia.stl"),
            QStringLiteral("models/tibia.stl"),
            QStringLiteral("stl")
        }
    };
    QVERIFY(repository.createCaseWorkspace(manifest));
    QVERIFY(repository.saveManifest(manifest));

    AnklePlanningService planningService(repository);
    AnklePlanningData planning = planningService.createDefaultPlanning(manifest.caseId);
    planning.primaryBones = QStringList { QStringLiteral("tibia") };
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(1.0f, 2.0f, 3.0f));
    planning.targetRegionCenter = QVector3D(4.0f, 5.0f, 6.0f);
    planning.targetRegionRadiusMm = 12.5;
    QVERIFY(planningService.savePlanning(manifest.caseId, planning));

    NavigationEvaluationService evaluationService(tempRoot.path() + QStringLiteral("/cases"));
    AnkleRegistrationRecord registration;
    registration.caseId = manifest.caseId;
    registration.registrationMode = QStringLiteral("optical_point");
    registration.fre = 0.8;
    registration.targetTre = 1.2;
    registration.coverageScore = 0.91;
    QVERIFY(evaluationService.saveRegistrationRecord(registration));

    AnkleNavigationRunRecord run;
    run.caseId = manifest.caseId;
    run.navigationMode = QStringLiteral("live_tracking");
    run.confidenceScore = 0.88;
    QVERIFY(evaluationService.saveNavigationRun(run));

    AnkleEvaluationReport report;
    report.caseId = manifest.caseId;
    report.allowNavigation = true;
    report.confidenceScore = 0.88;
    report.gateReasons = QStringList { QStringLiteral("ready") };
    report.calibrated = true;
    report.calibrationAccuracyMm = 0.42;
    QVERIFY(evaluationService.saveEvaluationReport(report));

    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(manifest.caseId, QStringLiteral("tracking-301"), QStringLiteral("tool-301"));
    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.42);
    runtimeState.setTrackingQuality(trackingQuality);

    NavigationConfidenceResult confidence;
    confidence.allowNavigation = true;
    confidence.score = 0.88;
    confidence.recommendations = QStringList { QStringLiteral("ready") };
    runtimeState.setConfidenceResult(confidence);

    NavigationWorkspaceApplicationService service(tempRoot.path(), &runtimeState);
    const NavigationWorkspaceSnapshot snapshot =
        service.loadWorkspace(manifest.caseId, manifest.patientId, manifest.patientName);

    QCOMPARE(snapshot.caseId, manifest.caseId);
    QCOMPARE(snapshot.caseContext.patientName, manifest.patientName);
    QCOMPARE(snapshot.caseContext.surgeryId, manifest.surgeryId);
    QCOMPARE(snapshot.caseContext.currentStage, AnkleWorkflowStage::Planning);
    QCOMPARE(snapshot.assetState.dicomReady, true);
    QCOMPARE(snapshot.assetState.boneModelReady, true);
    QCOMPARE(snapshot.planningState.hasPlanning, true);
    QCOMPARE(snapshot.registrationState.success, true);
    QCOMPARE(snapshot.navigationState.hasRunRecord, true);
    QCOMPARE(snapshot.navigationState.hasEvaluationReport, true);
    QCOMPARE(snapshot.navigationState.allowNavigation, true);
    QCOMPARE(snapshot.calibrationState.completed, true);
    QCOMPARE(snapshot.calibrationState.accuracy, 0.42);
}

void NavigationWorkspaceApplicationServiceTest::service_evaluates_stage_gate_from_workspace_snapshot()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-302");
    manifest.patientId = QStringLiteral("patient-302");
    manifest.patientName = QStringLiteral("Patient 302");
    manifest.surgeryId = QStringLiteral("surgery-302");
    manifest.workflowStage = QStringLiteral("registration");
    QVERIFY(repository.createCaseWorkspace(manifest));

    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(manifest.caseId, QStringLiteral("tracking-302"), QStringLiteral("tool-302"));

    NavigationWorkspaceApplicationService service(tempRoot.path(), &runtimeState);
    const NavigationWorkspaceSnapshot snapshot =
        service.loadWorkspace(manifest.caseId, manifest.patientId, manifest.patientName);

    QCOMPARE(snapshot.caseContext.currentStage, AnkleWorkflowStage::Registration);

    const NavigationStageGate gate = service.evaluateStageGate(AnkleWorkflowStage::Navigation);
    QCOMPARE(gate.requestedStage, AnkleWorkflowStage::Navigation);
    QCOMPARE(gate.allowed, false);
    QCOMPARE(gate.reasonCode, QStringLiteral("registration_or_navigation_gate_missing"));
}

void NavigationWorkspaceApplicationServiceTest::service_records_runtime_workspace_states_into_snapshot_truth_source()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-303");
    manifest.patientId = QStringLiteral("patient-303");
    manifest.patientName = QStringLiteral("Patient 303");
    manifest.surgeryId = QStringLiteral("surgery-303");
    manifest.workflowStage = QStringLiteral("registration");
    QVERIFY(repository.createCaseWorkspace(manifest));
    QVERIFY(repository.saveManifest(manifest));

    NavigationRuntimeState runtimeState;
    runtimeState.setCaseContext(manifest.caseId, QStringLiteral("tracking-303"), QStringLiteral("tool-303"));

    NavigationWorkspaceApplicationService service(tempRoot.path(), &runtimeState);
    service.loadWorkspace(manifest.caseId, manifest.patientId, manifest.patientName);

    NavigationWorkspaceCalibrationState calibrationState;
    calibrationState.trackingReady = true;
    calibrationState.started = true;
    calibrationState.collectedPoints = 6;
    calibrationState.requiredPoints = 8;
    calibrationState.completed = false;
    calibrationState.statusText = QStringLiteral("collecting");
    calibrationState.geometryId = QStringLiteral("probe-303");
    service.recordCalibrationState(calibrationState);

    NavigationWorkspaceRegistrationState registrationState;
    registrationState.pointCount = 5;
    registrationState.success = true;
    registrationState.fre = 0.73;
    registrationState.targetTre = 1.11;
    registrationState.coverageScore = 0.93;
    registrationState.translationX = 10.0;
    registrationState.translationY = 20.0;
    registrationState.translationZ = 30.0;
    registrationState.rotationX = 1.0;
    registrationState.rotationY = 2.0;
    registrationState.rotationZ = 3.0;
    registrationState.transformMatrix = QStringLiteral("1,0,0,10;0,1,0,20;0,0,1,30;0,0,0,1");
    service.recordRegistrationState(registrationState);

    NavigationWorkspaceNavigationState navigationState;
    navigationState.trackerConnected = true;
    navigationState.toolVisible = true;
    navigationState.running = true;
    navigationState.confidence = 0.87;
    navigationState.allowNavigation = true;
    navigationState.blockReasons = QStringList { QStringLiteral("ready") };
    navigationState.hasRunRecord = true;
    navigationState.summaryText = QStringLiteral("live run");
    service.recordNavigationState(navigationState);

    QVERIFY(service.persistSnapshot());

    const NavigationWorkspaceSnapshot restored = service.restoreSnapshot(manifest.caseId);
    QCOMPARE(restored.calibrationState.trackingReady, true);
    QCOMPARE(restored.calibrationState.collectedPoints, 6);
    QCOMPARE(restored.calibrationState.requiredPoints, 8);
    QCOMPARE(restored.calibrationState.statusText, QStringLiteral("进行中（6/8）"));
    QCOMPARE(restored.registrationState.success, true);
    QCOMPARE(restored.registrationState.fre, 0.73);
    QCOMPARE(restored.registrationState.pointCount, 5);
    QCOMPARE(restored.registrationState.translationX, 10.0);
    QCOMPARE(restored.registrationState.rotationZ, 3.0);
    QCOMPARE(restored.navigationState.running, true);
    QCOMPARE(restored.navigationState.confidence, 0.87);
    QCOMPARE(restored.navigationState.summaryText, QStringLiteral("导航运行中"));

    const NavigationWorkspaceSnapshot reloaded =
        service.loadWorkspace(manifest.caseId, manifest.patientId, manifest.patientName);
    QCOMPARE(reloaded.calibrationState.trackingReady, true);
    QCOMPARE(reloaded.calibrationState.collectedPoints, 6);
    QCOMPARE(reloaded.registrationState.success, true);
    QCOMPARE(reloaded.registrationState.pointCount, 5);
    QCOMPARE(reloaded.navigationState.running, true);
    QCOMPARE(reloaded.navigationState.hasRunRecord, true);
    QCOMPARE(reloaded.navigationState.summaryText, QStringLiteral("导航运行中"));

    const NavigationStageGate gate = service.evaluateStageGate(AnkleWorkflowStage::Navigation);
    QCOMPARE(gate.allowed, true);
}

void NavigationWorkspaceApplicationServiceTest::service_builds_runtime_status_summaries_for_workspace_states()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-304");
    manifest.patientId = QStringLiteral("patient-304");
    manifest.patientName = QStringLiteral("Patient 304");
    manifest.surgeryId = QStringLiteral("surgery-304");
    QVERIFY(repository.createCaseWorkspace(manifest));
    QVERIFY(repository.saveManifest(manifest));

    NavigationWorkspaceApplicationService service(tempRoot.path(), nullptr);
    service.loadWorkspace(manifest.caseId, manifest.patientId, manifest.patientName);

    NavigationWorkspaceCalibrationState calibrationState;
    calibrationState.trackingReady = true;
    calibrationState.started = true;
    calibrationState.collectedPoints = 8;
    calibrationState.requiredPoints = 8;
    calibrationState.completed = true;
    calibrationState.accuracy = 0.41;
    service.recordCalibrationState(calibrationState);

    NavigationWorkspaceNavigationState navigationState;
    navigationState.trackerConnected = true;
    navigationState.running = true;
    navigationState.allowNavigation = true;
    navigationState.confidence = 0.92;
    service.recordNavigationState(navigationState);

    const NavigationWorkspaceSnapshot snapshot = service.currentSnapshot();
    QVERIFY(snapshot.calibrationState.statusText.contains(QStringLiteral("已完成")));
    QVERIFY(snapshot.navigationState.summaryText.contains(QStringLiteral("导航运行中")));

    navigationState.running = false;
    navigationState.hasRunRecord = true;
    service.recordNavigationState(navigationState);
    QVERIFY(service.currentSnapshot().navigationState.summaryText.contains(QStringLiteral("导航已暂停")));
}

void NavigationWorkspaceApplicationServiceTest::service_preserves_navigation_run_facts_when_recording_gate_updates()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-305");
    manifest.patientId = QStringLiteral("patient-305");
    manifest.patientName = QStringLiteral("Patient 305");
    manifest.surgeryId = QStringLiteral("surgery-305");
    QVERIFY(repository.createCaseWorkspace(manifest));
    QVERIFY(repository.saveManifest(manifest));

    NavigationWorkspaceApplicationService service(tempRoot.path(), nullptr);
    service.loadWorkspace(manifest.caseId, manifest.patientId, manifest.patientName);

    NavigationWorkspaceNavigationState navigationState;
    navigationState.trackerConnected = true;
    navigationState.running = true;
    navigationState.allowNavigation = true;
    navigationState.confidence = 0.93;
    service.recordNavigationState(navigationState);

    NavigationWorkspaceNavigationState gateUpdate;
    gateUpdate.trackerConnected = true;
    gateUpdate.allowNavigation = false;
    gateUpdate.confidence = 0.44;
    gateUpdate.blockReasons = QStringList { QStringLiteral("registration missing") };
    service.recordNavigationState(gateUpdate);

    const NavigationWorkspaceNavigationState snapshot = service.currentSnapshot().navigationState;
    QCOMPARE(snapshot.running, true);
    QCOMPARE(snapshot.hasRunRecord, false);
    QCOMPARE(snapshot.hasEvaluationReport, false);
    QCOMPARE(snapshot.allowNavigation, false);
    QCOMPARE(snapshot.confidence, 0.44);
    QCOMPARE(snapshot.summaryText, QStringLiteral("导航运行中"));
}

QTEST_APPLESS_MAIN(NavigationWorkspaceApplicationServiceTest)
#include "NavigationWorkspaceApplicationServiceTest.moc"
