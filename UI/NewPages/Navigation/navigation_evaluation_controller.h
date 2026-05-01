#pragma once

#include <functional>

class NavigationEvaluationController
{
public:
    struct Actions
    {
        std::function<void()> startNavigation;
    };

    explicit NavigationEvaluationController(Actions actions = {});

    void startNavigation() const;

private:
    Actions m_actions;
};
