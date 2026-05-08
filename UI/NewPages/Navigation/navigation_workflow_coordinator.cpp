#include "UI/NewPages/Navigation/navigation_workflow_coordinator.h"

#include "UI/NewPages/Navigation/navigation_evaluation_controller.h"
#include "UI/NewPages/Navigation/navigation_runtime_coordinator.h"
#include "UI/NewPages/Navigation/navigation_workspace_application_service.h"
#include "UI/NewPages/Navigation/preparation_planning_controller.h"
#include "UI/NewPages/Navigation/registration_controller.h"

NavigationWorkflowCoordinator::NavigationWorkflowCoordinator(
    NavigationWorkflowContext* context,
    PreparationPlanningController* preparationPlanningController,
    RegistrationController* registrationController,
    NavigationEvaluationController* navigationEvaluationController,
    NavigationRuntimeCoordinator* runtimeCoordinator,
    StageApplier stageApplier,
    NavigationWorkspaceApplicationService* workspaceApplicationService)
    : m_context(context)
    , m_preparationPlanningController(preparationPlanningController)
    , m_registrationController(registrationController)
    , m_navigationEvaluationController(navigationEvaluationController)
    , m_runtimeCoordinator(runtimeCoordinator)
    , m_stageApplier(std::move(stageApplier))
    , m_workspaceApplicationService(workspaceApplicationService)
{
}

bool NavigationWorkflowCoordinator::tryEnterStage(AnkleWorkflowStage stage) const
{
    if (!m_workspaceApplicationService) {
        enterStage(stage);
        return true;
    }

    const NavigationStageGate gate = m_workspaceApplicationService->evaluateStageGate(stage);
    if (!gate.allowed) {
        return false;
    }

    enterStage(stage);
    return true;
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
    if (!tryEnterStage(AnkleWorkflowStage::Planning)) {
        return;
    }

    if (m_preparationPlanningController) {
        m_preparationPlanningController->loadDicom();
        if (m_workspaceApplicationService) {
            m_workspaceApplicationService->recordPreparationState(
                m_preparationPlanningController->currentPreparationState());
            m_workspaceApplicationService->recordPlanningState(
                m_preparationPlanningController->currentPlanningState());
            m_workspaceApplicationService->persistSnapshot();
        }
    }
}

void NavigationWorkflowCoordinator::handleComputeRegistration() const
{
    if (!tryEnterStage(AnkleWorkflowStage::Registration)) {
        return;
    }

    if (m_registrationController) {
        const NavigationWorkspaceRegistrationState registrationState =
            m_registrationController->computePerBoneRegistration();
        if (m_workspaceApplicationService) {
            m_workspaceApplicationService->recordRegistrationState(registrationState);
            m_workspaceApplicationService->persistSnapshot();
        }
    }
}

void NavigationWorkflowCoordinator::handleStartNavigation() const
{
    if (!m_navigationEvaluationController) {
        return;
    }

    m_navigationEvaluationController->startNavigation();
    if (m_workspaceApplicationService) {
        m_workspaceApplicationService->recordEvaluationState(
            m_navigationEvaluationController->currentEvaluationState());
        m_workspaceApplicationService->persistSnapshot();
    }
}
