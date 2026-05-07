#include "UI/NewPages/Navigation/navigation_workspace_application_service.h"

#include <QDateTime>

NavigationWorkspaceApplicationService::NavigationWorkspaceApplicationService(
    const QString& dataRoot,
    NavigationRuntimeState* runtimeState)
    : m_dataRoot(dataRoot)
    , m_casesRoot(dataRoot + QStringLiteral("/cases"))
    , m_runtimeState(runtimeState)
    , m_repository(dataRoot)
    , m_planningService(m_repository)
    , m_evaluationService(m_casesRoot)
{
}

NavigationWorkspaceSnapshot NavigationWorkspaceApplicationService::loadWorkspace(
    const QString& caseId,
    const QString& patientId,
    const QString& patientName)
{
    m_snapshot = buildSnapshot(caseId, patientId, patientName);
    const NavigationWorkspaceSnapshot restoredSnapshot = restoreSnapshot(caseId);
    if (restoredSnapshot.caseId == caseId) {
        if (restoredSnapshot.calibrationState.trackingReady
            || restoredSnapshot.calibrationState.started
            || restoredSnapshot.calibrationState.completed
            || restoredSnapshot.calibrationState.requiredPoints > 0
            || restoredSnapshot.calibrationState.collectedPoints > 0
            || !restoredSnapshot.calibrationState.statusText.isEmpty()
            || !restoredSnapshot.calibrationState.geometryId.isEmpty()) {
            m_snapshot.calibrationState = restoredSnapshot.calibrationState;
        }

        if (restoredSnapshot.registrationState.success
            || restoredSnapshot.registrationState.pointCount > 0
            || restoredSnapshot.registrationState.fre > 0.0
            || restoredSnapshot.registrationState.targetTre > 0.0
            || restoredSnapshot.registrationState.coverageScore > 0.0
            || !restoredSnapshot.registrationState.transformMatrix.isEmpty()) {
            m_snapshot.registrationState = restoredSnapshot.registrationState;
        }

        if (restoredSnapshot.navigationState.trackerConnected
            || restoredSnapshot.navigationState.toolVisible
            || restoredSnapshot.navigationState.running
            || restoredSnapshot.navigationState.confidence > 0.0
            || restoredSnapshot.navigationState.allowNavigation
            || restoredSnapshot.navigationState.hasRunRecord
            || restoredSnapshot.navigationState.hasEvaluationReport
            || restoredSnapshot.navigationState.exportAvailable
            || !restoredSnapshot.navigationState.blockReasons.isEmpty()
            || !restoredSnapshot.navigationState.summaryText.isEmpty()) {
            m_snapshot.navigationState = restoredSnapshot.navigationState;
        }
    }

    m_snapshot.stageGate = evaluateStageGate(m_snapshot.caseContext.currentStage);
    return m_snapshot;
}

NavigationWorkspaceSnapshot NavigationWorkspaceApplicationService::currentSnapshot() const
{
    return m_snapshot;
}

NavigationStageGate NavigationWorkspaceApplicationService::evaluateStageGate(AnkleWorkflowStage stage)
{
    NavigationStageGate gate;
    gate.requestedStage = stage;
    gate.lastComputedAt = QDateTime::currentDateTimeUtc();

    switch (stage) {
    case AnkleWorkflowStage::Preparation:
        gate.allowed = !m_snapshot.caseId.isEmpty();
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("case_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("病例工作区已加载") : QStringLiteral("未加载病例工作区");
        break;
    case AnkleWorkflowStage::Planning:
        gate.allowed = !m_snapshot.caseId.isEmpty() && m_snapshot.assetState.boneModelReady;
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("bone_asset_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("规划条件满足") : QStringLiteral("骨骼资产未就绪");
        break;
    case AnkleWorkflowStage::Registration:
        gate.allowed = m_snapshot.planningState.hasPlanning && m_snapshot.calibrationState.completed;
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("calibration_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("配准条件满足") : QStringLiteral("标定未完成");
        break;
    case AnkleWorkflowStage::Navigation:
        gate.allowed = m_snapshot.registrationState.success && m_snapshot.navigationState.allowNavigation;
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("registration_or_navigation_gate_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("导航条件满足") : QStringLiteral("配准或导航门禁未满足");
        break;
    case AnkleWorkflowStage::Evaluation:
        gate.allowed = m_snapshot.navigationState.hasRunRecord || m_snapshot.navigationState.hasEvaluationReport;
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("evaluation_input_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("评估条件满足") : QStringLiteral("缺少导航记录或评估报告");
        break;
    }

    m_snapshot.caseContext.currentStage = stage;
    m_snapshot.stageGate = gate;
    return gate;
}

void NavigationWorkspaceApplicationService::recordCalibrationState(
    const NavigationWorkspaceCalibrationState& calibrationState)
{
    m_snapshot.calibrationState = buildCalibrationSummary(calibrationState);
}

void NavigationWorkspaceApplicationService::recordRegistrationState(
    const NavigationWorkspaceRegistrationState& registrationState)
{
    m_snapshot.registrationState = registrationState;
}

void NavigationWorkspaceApplicationService::recordNavigationState(
    const NavigationWorkspaceNavigationState& navigationState)
{
    m_snapshot.navigationState = buildNavigationSummary(mergeNavigationState(navigationState));
}

bool NavigationWorkspaceApplicationService::persistSnapshot() const
{
    NavigationWorkspaceSnapshotStore store(caseRoot(m_snapshot.caseId));
    return store.persistSnapshot(m_snapshot);
}

NavigationWorkspaceSnapshot NavigationWorkspaceApplicationService::restoreSnapshot(const QString& caseId) const
{
    NavigationWorkspaceSnapshotStore store(caseRoot(caseId));
    return store.loadSnapshot();
}

NavigationWorkspaceSnapshot NavigationWorkspaceApplicationService::buildSnapshot(
    const QString& caseId,
    const QString& patientId,
    const QString& patientName) const
{
    NavigationWorkspaceSnapshot snapshot;
    const AnkleCaseManifest manifest = m_repository.loadManifest(caseId);
    snapshot.caseId = caseId;
    snapshot.caseContext.caseId = caseId;
    snapshot.caseContext.patientId = patientId;
    snapshot.caseContext.patientName = patientName;
    snapshot.caseContext.surgeryId = manifest.surgeryId;
    snapshot.caseContext.currentStage = stageFromManifest(manifest.workflowStage);
    snapshot.caseContext.lastUpdatedAt = QDateTime::currentDateTimeUtc();
    snapshot.assetState = buildAssetState(caseId);
    snapshot.planningState = buildPlanningState(caseId);
    snapshot.registrationState = buildRegistrationState(caseId);
    snapshot.navigationState = buildNavigationState(caseId);
    snapshot.evaluationState = buildEvaluationState(caseId);

    if (m_runtimeState && m_runtimeState->hasTrackingQuality()) {
        snapshot.navigationState.trackerConnected = true;
        snapshot.calibrationState.completed =
            m_runtimeState->trackingQuality().value(QStringLiteral("calibrated")).toBool();
        snapshot.calibrationState.accuracy =
            m_runtimeState->trackingQuality().value(QStringLiteral("calibration_accuracy_mm")).toDouble();
    }

    if (m_runtimeState && m_runtimeState->hasConfidenceResult()) {
        snapshot.navigationState.confidence = m_runtimeState->confidenceResult().score;
        snapshot.navigationState.allowNavigation = m_runtimeState->confidenceResult().allowNavigation;
        snapshot.navigationState.blockReasons = m_runtimeState->confidenceResult().recommendations;
    }

    snapshot.lastRefreshedAt = QDateTime::currentDateTimeUtc();
    return snapshot;
}

NavigationWorkspaceCalibrationState NavigationWorkspaceApplicationService::buildCalibrationSummary(
    const NavigationWorkspaceCalibrationState& calibrationState) const
{
    NavigationWorkspaceCalibrationState state = calibrationState;
    if (!state.trackingReady) {
        state.statusText = QStringLiteral("未连接追踪器械");
        return state;
    }

    if (state.completed) {
        state.statusText = QStringLiteral("已完成");
        return state;
    }

    if (!state.started) {
        state.statusText = QStringLiteral("待开始");
        return state;
    }

    if (state.requiredPoints > 0) {
        state.statusText = QStringLiteral("进行中（%1/%2）")
            .arg(state.collectedPoints)
            .arg(state.requiredPoints);
        return state;
    }

    state.statusText = QStringLiteral("进行中");
    return state;
}

NavigationWorkspaceNavigationState NavigationWorkspaceApplicationService::mergeNavigationState(
    const NavigationWorkspaceNavigationState& navigationState) const
{
    NavigationWorkspaceNavigationState mergedState = m_snapshot.navigationState;
    mergedState.trackerConnected = navigationState.trackerConnected;
    mergedState.toolVisible = navigationState.toolVisible;
    const bool onlyGateUpdate = !navigationState.running
        && !navigationState.hasRunRecord
        && !navigationState.hasEvaluationReport
        && !navigationState.exportAvailable;
    mergedState.running = onlyGateUpdate ? mergedState.running : navigationState.running;
    mergedState.confidence = navigationState.confidence;
    mergedState.allowNavigation = navigationState.allowNavigation;
    mergedState.blockReasons = navigationState.blockReasons;
    mergedState.hasRunRecord = navigationState.hasRunRecord || mergedState.hasRunRecord;
    mergedState.hasEvaluationReport = navigationState.hasEvaluationReport || mergedState.hasEvaluationReport;
    mergedState.exportAvailable = navigationState.exportAvailable || mergedState.exportAvailable;

    if (!navigationState.summaryText.isEmpty()) {
        mergedState.summaryText = navigationState.summaryText;
    }

    return mergedState;
}

NavigationWorkspaceNavigationState NavigationWorkspaceApplicationService::buildNavigationSummary(
    const NavigationWorkspaceNavigationState& navigationState) const
{
    NavigationWorkspaceNavigationState state = navigationState;
    if (state.running) {
        state.summaryText = QStringLiteral("导航运行中");
        return state;
    }

    if (state.hasRunRecord) {
        state.summaryText = QStringLiteral("导航已暂停");
        return state;
    }

    if (state.allowNavigation) {
        state.summaryText = QStringLiteral("导航已就绪");
        return state;
    }

    state.summaryText = QStringLiteral("导航未就绪");
    return state;
}

NavigationWorkspaceNavigationState NavigationWorkspaceApplicationService::buildNavigationState(const QString& caseId) const
{
    NavigationWorkspaceNavigationState state;
    const AnkleEvaluationSnapshot snapshot = m_evaluationService.loadEvaluationSnapshot(caseId);
    state.hasRunRecord = snapshot.hasNavigationRun;
    state.hasEvaluationReport = snapshot.hasEvaluationReport;
    state.confidence = snapshot.evaluationConfidenceScore;
    state.allowNavigation = snapshot.allowNavigation;
    state.blockReasons = snapshot.gateReasons;
    state.summaryText = snapshot.caseId;
    state.exportAvailable = snapshot.hasEvaluationReport;
    return state;
}

NavigationWorkspacePlanningState NavigationWorkspaceApplicationService::buildPlanningState(const QString& caseId) const
{
    NavigationWorkspacePlanningState state;
    const AnklePlanningData planning = m_planningService.loadPlanning(caseId);
    state.hasPlanning = !planning.primaryBones.isEmpty() || !planning.referenceLandmarks.isEmpty();
    state.targetRegionReady = planning.targetRegionRadiusMm > 0.0;
    state.referenceBones = planning.primaryBones;
    state.recommendedPointOrder = planning.recommendedPointOrder;
    state.savedAt = QDateTime::currentDateTimeUtc();
    return state;
}

NavigationWorkspaceRegistrationState NavigationWorkspaceApplicationService::buildRegistrationState(const QString& caseId) const
{
    NavigationWorkspaceRegistrationState state;
    const AnkleEvaluationSnapshot snapshot = m_evaluationService.loadEvaluationSnapshot(caseId);
    state.success = snapshot.hasRegistration;
    state.fre = snapshot.fre;
    state.targetTre = snapshot.targetTre;
    state.coverageScore = snapshot.coverageScore;
    state.pointCount = snapshot.registrationMetrics.value(QStringLiteral("point_count")).toInt();
    return state;
}

NavigationWorkspaceEvaluationState NavigationWorkspaceApplicationService::buildEvaluationState(const QString& caseId) const
{
    NavigationWorkspaceEvaluationState state;
    const AnkleEvaluationSnapshot snapshot = m_evaluationService.loadEvaluationSnapshot(caseId);
    state.hasSummary = snapshot.hasEvaluationReport;
    state.summaryText = snapshot.caseId;
    if (snapshot.hasEvaluationReport) {
        state.exportableArtifacts = QStringList {
            QStringLiteral("evaluation_report.json"),
            QStringLiteral("evaluation_metrics.csv"),
            QStringLiteral("case_evaluation_summary.json")
        };
    }
    state.lastUpdatedAt = QDateTime::currentDateTimeUtc();
    return state;
}

NavigationWorkspaceAssetState NavigationWorkspaceApplicationService::buildAssetState(const QString& caseId) const
{
    NavigationWorkspaceAssetState state;
    const AnkleCaseManifest manifest = m_repository.loadManifest(caseId);
    state.dicomReady = !manifest.dicomDir.isEmpty();
    state.boneModelReady = !manifest.modelAssets.isEmpty();
    if (!manifest.modelAssets.isEmpty()) {
        state.boneModelPath = manifest.modelAssets.first().normalizedPath;
        for (const AnkleModelAsset& asset : manifest.modelAssets) {
            state.selectedBoneAssets.append(asset.boneName);
        }
        state.selectedBoneAsset = manifest.modelAssets.first().boneName;
    }
    return state;
}

AnkleWorkflowStage NavigationWorkspaceApplicationService::stageFromManifest(const QString& workflowStage) const
{
    if (workflowStage == QStringLiteral("planning")) {
        return AnkleWorkflowStage::Planning;
    }
    if (workflowStage == QStringLiteral("registration")) {
        return AnkleWorkflowStage::Registration;
    }
    if (workflowStage == QStringLiteral("navigation")) {
        return AnkleWorkflowStage::Navigation;
    }
    if (workflowStage == QStringLiteral("evaluation")) {
        return AnkleWorkflowStage::Evaluation;
    }
    return AnkleWorkflowStage::Preparation;
}

QString NavigationWorkspaceApplicationService::caseRoot(const QString& caseId) const
{
    return m_repository.caseRoot(caseId);
}
