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
    const QStringList& activeInstrumentIds,
    const QList<NavigationInstrumentCalibrationState>& calibrationStates) const
{
    NavigationWorkspacePreparationState state;

    for (const QString& instrumentId : activeInstrumentIds) {
        const auto it = std::find_if(
            calibrationStates.cbegin(),
            calibrationStates.cend(),
            [&instrumentId](const NavigationInstrumentCalibrationState& item) {
                return item.instrumentId == instrumentId;
            });
        if (it != calibrationStates.cend()) {
            state.instrumentCalibrationStates.append(*it);
        }
    }

    state.allRequiredInstrumentsCalibrated =
        !state.instrumentCalibrationStates.isEmpty()
        && std::all_of(
            state.instrumentCalibrationStates.cbegin(),
            state.instrumentCalibrationStates.cend(),
            [](const NavigationInstrumentCalibrationState& item) { return item.completed; });

    if (activeInstrumentIds.isEmpty()) {
        state.blockingReasons.append(QStringLiteral("未选择活动器械"));
    }
    if (state.instrumentCalibrationStates.isEmpty()) {
        state.blockingReasons.append(QStringLiteral("尚未配置导航器械标定"));
    } else if (!state.allRequiredInstrumentsCalibrated) {
        state.blockingReasons.append(QStringLiteral("存在未完成标定的器械"));
    }
    return state;
}
