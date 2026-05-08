#pragma once

#include "UI/NewPages/Navigation/navigation_workspace_types.h"

#include <functional>

class PreparationPlanningController
{
public:
    struct Actions
    {
        std::function<void()> loadDicom;
        std::function<NavigationWorkspacePreparationState()> resolvePreparationState;
        std::function<NavigationWorkspacePlanningState()> resolvePlanningState;
    };

    explicit PreparationPlanningController(Actions actions = {});

    void loadDicom() const;
    NavigationWorkspacePreparationState currentPreparationState() const;
    NavigationWorkspacePlanningState currentPlanningState() const;
    NavigationWorkspacePreparationState buildPreparationState(
        const QStringList& activeBones,
        const QList<NavigationInstrumentCalibrationState>& calibrationStates) const;

private:
    Actions m_actions;
};
