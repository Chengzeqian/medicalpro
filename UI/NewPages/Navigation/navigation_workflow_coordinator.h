#pragma once

#include "UI/NewPages/Navigation/navigation_workflow_context.h"

#include <functional>

class NavigationEvaluationController;
class PreparationPlanningController;
class RegistrationController;

class NavigationWorkflowCoordinator
{
public:
    using StageApplier = std::function<void(AnkleWorkflowStage)>;

    NavigationWorkflowCoordinator(
        NavigationWorkflowContext* context,
        PreparationPlanningController* preparationPlanningController,
        RegistrationController* registrationController,
        NavigationEvaluationController* navigationEvaluationController,
        StageApplier stageApplier = {});

    void enterStage(AnkleWorkflowStage stage) const;
    void handleLoadDicom() const;
    void handleComputeRegistration() const;
    void handleStartNavigation() const;

private:
    NavigationWorkflowContext* m_context = nullptr;
    PreparationPlanningController* m_preparationPlanningController = nullptr;
    RegistrationController* m_registrationController = nullptr;
    NavigationEvaluationController* m_navigationEvaluationController = nullptr;
    StageApplier m_stageApplier;
};
