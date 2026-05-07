#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "Framework/Navigation/navigation_confidence_evaluator.h"
#include "UI/NewPages/Navigation/navigation_workflow_coordinator.h"
#include "UI/NewPages/Navigation/navigation_runtime_coordinator.h"
#include "UI/NewPages/Navigation/navigation_runtime_state.h"
#include "UI/NewPages/Navigation/navigation_workspace_application_service.h"
#include "UI/NewPages/Navigation/preparation_planning_controller.h"
#include "UI/NewPages/Navigation/registration_controller.h"
#include "UI/NewPages/Navigation/navigation_evaluation_controller.h"

class NavigationWorkflowCoordinatorTest : public QObject
{
    Q_OBJECT

private slots:
    void coordinator_updates_stage_and_routes_registration_start();
    void coordinator_defers_stage_entry_to_workspace_gate();
    void registration_controller_submits_result_to_runtime_coordinator();
    void navigation_controller_reads_allow_navigation_from_runtime_coordinator();
    void coordinator_routes_navigation_start_without_entering_navigation_stage();
    void coordinator_does_not_preemptively_block_navigation_when_runtime_gate_is_stale();
};

void NavigationWorkflowCoordinatorTest::coordinator_updates_stage_and_routes_registration_start()
{
    NavigationWorkflowContext context;
    int registrationStartCount = 0;

    PreparationPlanningController preparationController;
    RegistrationController registrationController({
        .computeRegistration = [&registrationStartCount]() { ++registrationStartCount; }
    });
    NavigationEvaluationController navigationController;

    NavigationWorkflowCoordinator coordinator(
        &context,
        &preparationController,
        &registrationController,
        &navigationController);

    coordinator.handleComputeRegistration();

    QCOMPARE(context.currentStage(), AnkleWorkflowStage::Registration);
    QCOMPARE(registrationStartCount, 1);
}

void NavigationWorkflowCoordinatorTest::coordinator_defers_stage_entry_to_workspace_gate()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationWorkflowContext context;
    context.setCaseIdentity(QStringLiteral("ankle-case-401"), 401, QStringLiteral("Patient 401"));

    NavigationRuntimeState runtimeState;
    NavigationWorkspaceApplicationService workspaceApplicationService(tempRoot.path(), &runtimeState);
    workspaceApplicationService.loadWorkspace(
        QStringLiteral("ankle-case-401"),
        QStringLiteral("patient-401"),
        QStringLiteral("Patient 401"));

    int registrationStartCount = 0;
    bool stageApplied = false;

    PreparationPlanningController preparationController;
    RegistrationController registrationController({
        .computeRegistration = [&registrationStartCount]() { ++registrationStartCount; }
    });
    NavigationEvaluationController navigationController;

    NavigationWorkflowCoordinator coordinator(
        &context,
        &preparationController,
        &registrationController,
        &navigationController,
        nullptr,
        [&stageApplied](AnkleWorkflowStage) { stageApplied = true; },
        &workspaceApplicationService);

    coordinator.handleComputeRegistration();

    QCOMPARE(context.currentStage(), AnkleWorkflowStage::Preparation);
    QCOMPARE(registrationStartCount, 0);
    QVERIFY(!stageApplied);
}

void NavigationWorkflowCoordinatorTest::registration_controller_submits_result_to_runtime_coordinator()
{
    NavigationRuntimeState runtimeState;
    NavigationRuntimeCoordinator runtimeCoordinator(&runtimeState);
    RegistrationController registrationController({}, &runtimeCoordinator);

    PointRegistrationResult result;
    result.success = true;
    result.targetRegionTre = 1.2;
    result.coverageScore = 0.9;

    QVERIFY(!runtimeState.hasRegistrationResult());

    registrationController.handleRegistrationCompleted(result);

    QVERIFY(runtimeState.hasRegistrationResult());
    QCOMPARE(runtimeState.registrationResult().targetRegionTre, 1.2);
    QCOMPARE(runtimeState.registrationResult().coverageScore, 0.9);
}

void NavigationWorkflowCoordinatorTest::navigation_controller_reads_allow_navigation_from_runtime_coordinator()
{
    NavigationRuntimeState runtimeState;
    NavigationRuntimeCoordinator runtimeCoordinator(&runtimeState);
    NavigationEvaluationController navigationController({}, &runtimeCoordinator);

    QVERIFY(!navigationController.canStartNavigation());

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = true;
    confidenceResult.score = 0.81;
    runtimeState.setConfidenceResult(confidenceResult);

    QVERIFY(navigationController.canStartNavigation());
}

void NavigationWorkflowCoordinatorTest::coordinator_routes_navigation_start_without_entering_navigation_stage()
{
    NavigationWorkflowContext context;
    int navigationStartCount = 0;

    PreparationPlanningController preparationController;
    RegistrationController registrationController;
    NavigationRuntimeState runtimeState;
    NavigationRuntimeCoordinator runtimeCoordinator(&runtimeState);
    NavigationEvaluationController navigationController({
        .startNavigation = [&navigationStartCount]() { ++navigationStartCount; }
    }, &runtimeCoordinator);

    NavigationWorkflowCoordinator coordinator(
        &context,
        &preparationController,
        &registrationController,
        &navigationController,
        &runtimeCoordinator);

    coordinator.handleStartNavigation();
    QCOMPARE(context.currentStage(), AnkleWorkflowStage::Preparation);
    QCOMPARE(navigationStartCount, 1);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = true;
    confidenceResult.score = 0.88;
    runtimeState.setConfidenceResult(confidenceResult);

    coordinator.handleStartNavigation();
    QCOMPARE(context.currentStage(), AnkleWorkflowStage::Preparation);
    QCOMPARE(navigationStartCount, 2);
}

void NavigationWorkflowCoordinatorTest::coordinator_does_not_preemptively_block_navigation_when_runtime_gate_is_stale()
{
    NavigationWorkflowContext context;
    int navigationStartCount = 0;

    PreparationPlanningController preparationController;
    RegistrationController registrationController;
    NavigationRuntimeState runtimeState;
    NavigationRuntimeCoordinator runtimeCoordinator(&runtimeState);
    NavigationEvaluationController navigationController({
        .startNavigation = [&navigationStartCount]() { ++navigationStartCount; }
    }, &runtimeCoordinator);

    NavigationWorkflowCoordinator coordinator(
        &context,
        &preparationController,
        &registrationController,
        &navigationController,
        &runtimeCoordinator);

    NavigationConfidenceResult staleConfidenceResult;
    staleConfidenceResult.allowNavigation = false;
    staleConfidenceResult.score = 0.2;
    runtimeState.setConfidenceResult(staleConfidenceResult);

    coordinator.handleStartNavigation();

    QCOMPARE(context.currentStage(), AnkleWorkflowStage::Preparation);
    QCOMPARE(navigationStartCount, 1);
}

QTEST_APPLESS_MAIN(NavigationWorkflowCoordinatorTest)
#include "NavigationWorkflowCoordinatorTest.moc"
