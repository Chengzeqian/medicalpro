#include "UI/NewPages/Navigation/navigation_evaluation_controller.h"

#include "UI/NewPages/Navigation/navigation_runtime_coordinator.h"

NavigationEvaluationController::NavigationEvaluationController(
    Actions actions,
    NavigationRuntimeCoordinator* runtimeCoordinator)
    : m_actions(std::move(actions))
    , m_runtimeCoordinator(runtimeCoordinator)
{
}

void NavigationEvaluationController::startNavigation() const
{
    if (m_actions.startNavigation) {
        m_actions.startNavigation();
    }
}

bool NavigationEvaluationController::canStartNavigation() const
{
    return confidenceResult().allowNavigation;
}

NavigationConfidenceResult NavigationEvaluationController::confidenceResult() const
{
    if (!m_runtimeCoordinator || !m_runtimeCoordinator->runtimeState()) {
        return {};
    }

    const auto* runtimeState = m_runtimeCoordinator->runtimeState();
    if (!runtimeState->hasConfidenceResult()) {
        return {};
    }

    return runtimeState->confidenceResult();
}

NavigationWorkspaceEvaluationState NavigationEvaluationController::currentEvaluationState() const
{
    return m_actions.resolveEvaluationState ? m_actions.resolveEvaluationState() : NavigationWorkspaceEvaluationState();
}
