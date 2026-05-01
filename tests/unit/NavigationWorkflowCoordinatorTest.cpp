#include <QtTest/QtTest>

#include "UI/NewPages/Navigation/navigation_workflow_coordinator.h"
#include "UI/NewPages/Navigation/preparation_planning_controller.h"
#include "UI/NewPages/Navigation/registration_controller.h"
#include "UI/NewPages/Navigation/navigation_evaluation_controller.h"

class NavigationWorkflowCoordinatorTest : public QObject
{
    Q_OBJECT

private slots:
    void coordinator_updates_stage_and_routes_registration_start();
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

QTEST_APPLESS_MAIN(NavigationWorkflowCoordinatorTest)
#include "NavigationWorkflowCoordinatorTest.moc"
