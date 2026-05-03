#pragma once

#include "Framework/Navigation/navigation_confidence_evaluator.h"

#include <functional>

class NavigationRuntimeCoordinator;

class NavigationEvaluationController
{
public:
    struct Actions
    {
        std::function<void()> startNavigation;
    };

    explicit NavigationEvaluationController(
        Actions actions = {},
        NavigationRuntimeCoordinator* runtimeCoordinator = nullptr);

    void startNavigation() const;
    bool canStartNavigation() const;
    NavigationConfidenceResult confidenceResult() const;

private:
    Actions m_actions;
    NavigationRuntimeCoordinator* m_runtimeCoordinator = nullptr;
};
