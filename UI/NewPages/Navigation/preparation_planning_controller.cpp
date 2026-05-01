#include "UI/NewPages/Navigation/preparation_planning_controller.h"

PreparationPlanningController::PreparationPlanningController(Actions actions)
    : m_actions(std::move(actions))
{
}

void PreparationPlanningController::loadDicom() const
{
    if (m_actions.loadDicom) {
        m_actions.loadDicom();
    }
}
