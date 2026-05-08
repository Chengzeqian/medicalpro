#pragma once

#include "Framework/Navigation/navigation_confidence_evaluator.h"
#include "UI/NewPages/Navigation/navigation_workspace_types.h"

#include <functional>

class NavigationRuntimeCoordinator;

class NavigationEvaluationController
{
public:
    struct Actions
    {
        std::function<void()> startNavigation;
        std::function<NavigationWorkspaceEvaluationState()> resolveEvaluationState;
    };

    explicit NavigationEvaluationController(
        Actions actions = {},
        NavigationRuntimeCoordinator* runtimeCoordinator = nullptr);

    void startNavigation() const;
    bool canStartNavigation() const;
    NavigationConfidenceResult confidenceResult() const;
    NavigationWorkspaceEvaluationState currentEvaluationState() const;

private:
    Actions m_actions;
    NavigationRuntimeCoordinator* m_runtimeCoordinator = nullptr;
};
