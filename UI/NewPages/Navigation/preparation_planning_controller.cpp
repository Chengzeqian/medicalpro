#include "UI/NewPages/Navigation/preparation_planning_controller.h"

#include <algorithm>

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

NavigationWorkspacePreparationState PreparationPlanningController::currentPreparationState() const
{
    return m_actions.resolvePreparationState ? m_actions.resolvePreparationState() : NavigationWorkspacePreparationState();
}

NavigationWorkspacePlanningState PreparationPlanningController::currentPlanningState() const
{
    return m_actions.resolvePlanningState ? m_actions.resolvePlanningState() : NavigationWorkspacePlanningState();
}

NavigationWorkspacePreparationState PreparationPlanningController::buildPreparationState(
    const QStringList& activeBones,
    const QList<NavigationInstrumentCalibrationState>& calibrationStates) const
{
    NavigationWorkspacePreparationState state;
    state.instrumentCalibrationStates = calibrationStates;
    state.allRequiredInstrumentsCalibrated =
        !calibrationStates.isEmpty()
        && std::all_of(
            calibrationStates.cbegin(),
            calibrationStates.cend(),
            [](const NavigationInstrumentCalibrationState& item) { return item.completed; });
    if (activeBones.isEmpty()) {
        state.blockingReasons.append(QStringLiteral("未选择活动骨骼"));
    }
    if (calibrationStates.isEmpty()) {
        state.blockingReasons.append(QStringLiteral("尚未配置导航器械标定"));
    } else if (!state.allRequiredInstrumentsCalibrated) {
        state.blockingReasons.append(QStringLiteral("存在未完成标定的器械"));
    }
    return state;
}
