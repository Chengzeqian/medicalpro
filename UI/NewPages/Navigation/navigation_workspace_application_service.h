#pragma once

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"
#include "Framework/Navigation/navigation_evaluation_service.h"
#include "UI/NewPages/Navigation/navigation_runtime_state.h"
#include "UI/NewPages/Navigation/navigation_workspace_snapshot_store.h"
#include "UI/NewPages/Navigation/navigation_workspace_types.h"

class NavigationWorkspaceApplicationService
{
public:
    explicit NavigationWorkspaceApplicationService(
        const QString& dataRoot,
        NavigationRuntimeState* runtimeState = nullptr);

    NavigationWorkspaceSnapshot loadWorkspace(
        const QString& caseId,
        const QString& patientId,
        const QString& patientName);
    NavigationWorkspaceSnapshot currentSnapshot() const;
    NavigationStageGate evaluateStageGate(AnkleWorkflowStage stage);
    void recordCalibrationState(const NavigationWorkspaceCalibrationState& calibrationState);
    void recordRegistrationState(const NavigationWorkspaceRegistrationState& registrationState);
    void recordNavigationState(const NavigationWorkspaceNavigationState& navigationState);
    bool persistSnapshot() const;
    NavigationWorkspaceSnapshot restoreSnapshot(const QString& caseId) const;

private:
    NavigationWorkspaceNavigationState mergeNavigationState(
        const NavigationWorkspaceNavigationState& navigationState) const;
    NavigationWorkspaceCalibrationState buildCalibrationSummary(
        const NavigationWorkspaceCalibrationState& calibrationState) const;
    NavigationWorkspaceNavigationState buildNavigationSummary(
        const NavigationWorkspaceNavigationState& navigationState) const;
    NavigationWorkspaceSnapshot buildSnapshot(
        const QString& caseId,
        const QString& patientId,
        const QString& patientName) const;
    NavigationWorkspaceNavigationState buildNavigationState(const QString& caseId) const;
    NavigationWorkspacePlanningState buildPlanningState(const QString& caseId) const;
    NavigationWorkspaceRegistrationState buildRegistrationState(const QString& caseId) const;
    NavigationWorkspaceEvaluationState buildEvaluationState(const QString& caseId) const;
    NavigationWorkspaceAssetState buildAssetState(const QString& caseId) const;
    AnkleWorkflowStage stageFromManifest(const QString& workflowStage) const;
    QString caseRoot(const QString& caseId) const;

    QString m_dataRoot;
    QString m_casesRoot;
    NavigationRuntimeState* m_runtimeState = nullptr;
    AnkleCaseWorkspaceRepository m_repository;
    AnklePlanningService m_planningService;
    NavigationEvaluationService m_evaluationService;
    NavigationWorkspaceSnapshot m_snapshot;
};
