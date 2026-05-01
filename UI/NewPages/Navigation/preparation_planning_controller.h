#pragma once

#include <functional>

class PreparationPlanningController
{
public:
    struct Actions
    {
        std::function<void()> loadDicom;
    };

    explicit PreparationPlanningController(Actions actions = {});

    void loadDicom() const;

private:
    Actions m_actions;
};
