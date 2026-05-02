#include "UI/NewPages/Navigation/navigation_workflow_coordinator.h"

#include "UI/NewPages/Navigation/navigation_evaluation_controller.h"
#include "UI/NewPages/Navigation/preparation_planning_controller.h"
#include "UI/NewPages/Navigation/registration_controller.h"

NavigationWorkflowCoordinator::NavigationWorkflowCoordinator(
    NavigationWorkflowContext* context,
    PreparationPlanningController* preparationPlanningController,
    RegistrationController* registrationController,
    NavigationEvaluationController* navigationEvaluationController,
    StageApplier stageApplier)
    : m_context(context)
    , m_preparationPlanningController(preparationPlanningController)
    , m_registrationController(registrationController)
    , m_navigationEvaluationController(navigationEvaluationController)
    , m_stageApplier(std::move(stageApplier))
{
}

void NavigationWorkflowCoordinator::enterStage(AnkleWorkflowStage stage) const
{
    if (m_context) {
        m_context->setCurrentStage(stage);
    }

    if (m_stageApplier) {
        m_stageApplier(stage);
    }
}

void NavigationWorkflowCoordinator::handleLoadDicom() const
{
    enterStage(AnkleWorkflowStage::Planning);
    if (m_preparationPlanningController) {
        m_preparationPlanningController->loadDicom();
    }
}

void NavigationWorkflowCoordinator::handleComputeRegistration() const
{
    enterStage(AnkleWorkflowStage::Registration);
    if (m_registrationController) {
        m_registrationController->computeRegistration();
    }
}

void NavigationWorkflowCoordinator::handleStartNavigation() const
{
    enterStage(AnkleWorkflowStage::Navigation);
    if (m_navigationEvaluationController) {
        m_navigationEvaluationController->startNavigation();
    }
}
