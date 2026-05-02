#include "UI/NewPages/Navigation/navigation_evaluation_controller.h"

NavigationEvaluationController::NavigationEvaluationController(Actions actions)
    : m_actions(std::move(actions))
{
}

void NavigationEvaluationController::startNavigation() const
{
    if (m_actions.startNavigation) {
        m_actions.startNavigation();
    }
}
