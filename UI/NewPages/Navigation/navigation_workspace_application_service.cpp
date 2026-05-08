#include "UI/NewPages/Navigation/navigation_workspace_application_service.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace
{
QList<NavigationPerBoneRegistrationState> parsePerBoneResultsJson(const QVariant& value)
{
    QList<NavigationPerBoneRegistrationState> results;
    const QJsonDocument document = QJsonDocument::fromJson(value.toString().toUtf8());
    if (!document.isArray()) {
        return results;
    }

    const QJsonArray array = document.array();
    results.reserve(array.size());
    for (const QJsonValue& item : array) {
        const QJsonObject object = item.toObject();
        NavigationPerBoneRegistrationState state;
        state.boneAssetId = object.value(QStringLiteral("bone_asset_id")).toString();
        state.boneRegionId = object.value(QStringLiteral("bone_region_id")).toString();
        state.pointCount = object.value(QStringLiteral("point_count")).toInt();
        state.success = object.value(QStringLiteral("success")).toBool();
        state.fre = object.value(QStringLiteral("fre")).toDouble();
        state.targetTre = object.value(QStringLiteral("target_tre")).toDouble();
        state.coverageScore = object.value(QStringLiteral("coverage_score")).toDouble();
        state.transformMatrix = object.value(QStringLiteral("transform_matrix")).toString();
        state.completedAt = QDateTime::currentDateTimeUtc();
        results.append(state);
    }

    return results;
}

QStringList buildPerBoneQualitySummary(const QList<NavigationPerBoneRegistrationState>& states)
{
    QStringList summary;
    for (const NavigationPerBoneRegistrationState& state : states) {
        summary.append(QStringLiteral("%1/%2 FRE=%3 TRE=%4 Coverage=%5")
                           .arg(state.boneAssetId)
                           .arg(state.boneRegionId)
                           .arg(state.fre, 0, 'f', 2)
                           .arg(state.targetTre, 0, 'f', 2)
                           .arg(state.coverageScore, 0, 'f', 2));
    }
    return summary;
}

QString buildEvaluationProcessSummary(const AnkleEvaluationSnapshot& snapshot)
{
    QStringList parts;
    if (snapshot.hasRegistration) {
        parts.append(QStringLiteral("已完成分骨配准"));
    }
    if (snapshot.hasNavigationRun) {
        parts.append(QStringLiteral("已形成导航运行记录"));
    }
    if (snapshot.hasEvaluationReport) {
        parts.append(QStringLiteral("已生成评估报告"));
    }
    return parts.join(QStringLiteral("，"));
}
}

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
    mergeUiRestoreFacts(restoreSnapshot(caseId));
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
        gate.reasonText = gate.allowed ? QStringLiteral("病例工作区已加载") : QStringLiteral("尚未加载病例工作区");
        gate.severity = gate.allowed ? QStringLiteral("ok") : QStringLiteral("warning");
        break;
    case AnkleWorkflowStage::Planning:
        gate.allowed = !m_snapshot.assetState.activeBoneAssets.isEmpty();
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("active_bones_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("规划条件满足") : QStringLiteral("尚未选择参与规划的骨骼");
        gate.severity = gate.allowed ? QStringLiteral("ok") : QStringLiteral("warning");
        break;
    case AnkleWorkflowStage::Registration:
        gate.allowed = m_snapshot.planningState.completed
            && m_snapshot.preparationState.allRequiredInstrumentsCalibrated;
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("planning_or_calibration_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("配准条件满足") : QStringLiteral("规划结果或器械标定尚未完成");
        gate.severity = gate.allowed ? QStringLiteral("ok") : QStringLiteral("warning");
        break;
    case AnkleWorkflowStage::Navigation:
        gate.allowed = m_snapshot.registrationState.fusedNavigationSpaceReady
            && m_snapshot.navigationState.trackerConnected
            && m_snapshot.navigationState.toolVisible
            && m_snapshot.navigationState.allowNavigation;
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("fused_space_or_tracking_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("导航条件满足") : QStringLiteral("融合导航空间或实时跟踪条件未满足");
        gate.severity = gate.allowed ? QStringLiteral("ok") : QStringLiteral("danger");
        break;
    case AnkleWorkflowStage::Evaluation:
        gate.allowed = m_snapshot.navigationState.hasRunRecord
            || m_snapshot.navigationState.hasEvaluationReport
            || m_snapshot.evaluationState.reportReady;
        gate.reasonCode = gate.allowed ? QStringLiteral("ok") : QStringLiteral("evaluation_input_missing");
        gate.reasonText = gate.allowed ? QStringLiteral("评估条件满足") : QStringLiteral("缺少导航记录或评估报告");
        gate.severity = gate.allowed ? QStringLiteral("ok") : QStringLiteral("warning");
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
    m_snapshot.preparationState =
        buildPreparationState(m_snapshot.caseId, m_snapshot.assetState, m_snapshot.calibrationState);
}

void NavigationWorkspaceApplicationService::recordPreparationState(
    const NavigationWorkspacePreparationState& preparationState)
{
    m_snapshot.preparationState = preparationState;
}

void NavigationWorkspaceApplicationService::recordPlanningState(
    const NavigationWorkspacePlanningState& planningState)
{
    m_snapshot.planningState = planningState;
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

void NavigationWorkspaceApplicationService::recordEvaluationState(
    const NavigationWorkspaceEvaluationState& evaluationState)
{
    m_snapshot.evaluationState = evaluationState;
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

void NavigationWorkspaceApplicationService::mergeUiRestoreFacts(
    const NavigationWorkspaceSnapshot& restoredSnapshot)
{
    if (restoredSnapshot.caseId != m_snapshot.caseId) {
        return;
    }

    if (restoredSnapshot.calibrationState.trackingReady
        || restoredSnapshot.calibrationState.started
        || restoredSnapshot.calibrationState.completed
        || restoredSnapshot.calibrationState.requiredPoints > 0
        || restoredSnapshot.calibrationState.collectedPoints > 0
        || !restoredSnapshot.calibrationState.statusText.isEmpty()
        || !restoredSnapshot.calibrationState.geometryId.isEmpty()) {
        m_snapshot.calibrationState = restoredSnapshot.calibrationState;
    }

    if (!restoredSnapshot.preparationState.instrumentCalibrationStates.isEmpty()
        || restoredSnapshot.preparationState.allRequiredInstrumentsCalibrated
        || !restoredSnapshot.preparationState.blockingReasons.isEmpty()) {
        m_snapshot.preparationState = restoredSnapshot.preparationState;
    }

    if (restoredSnapshot.planningState.hasPlanning
        || restoredSnapshot.planningState.completed
        || !restoredSnapshot.planningState.targetBone.isEmpty()
        || !restoredSnapshot.planningState.constraintRegions.isEmpty()
        || !restoredSnapshot.planningState.recommendedPointOrder.isEmpty()) {
        m_snapshot.planningState = restoredSnapshot.planningState;
    }

    if (restoredSnapshot.registrationState.success
        || restoredSnapshot.registrationState.pointCount > 0
        || !restoredSnapshot.registrationState.perBoneResults.isEmpty()
        || restoredSnapshot.registrationState.fusedNavigationSpaceReady
        || !restoredSnapshot.registrationState.fusedNavigationSpacePath.isEmpty()
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
        || !restoredSnapshot.navigationState.activeToolId.isEmpty()
        || !restoredSnapshot.navigationState.latestPoseSummary.isEmpty()
        || !restoredSnapshot.navigationState.blockReasons.isEmpty()
        || !restoredSnapshot.navigationState.summaryText.isEmpty()) {
        m_snapshot.navigationState = restoredSnapshot.navigationState;
    }

    if (restoredSnapshot.evaluationState.hasSummary
        || restoredSnapshot.evaluationState.reportReady
        || !restoredSnapshot.evaluationState.errorMetrics.isEmpty()
        || !restoredSnapshot.evaluationState.perBoneQualitySummary.isEmpty()
        || !restoredSnapshot.evaluationState.navigationProcessSummary.isEmpty()
        || !restoredSnapshot.evaluationState.summaryText.isEmpty()) {
        m_snapshot.evaluationState = restoredSnapshot.evaluationState;
    }
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
    snapshot.calibrationState = {};
    snapshot.preparationState =
        buildPreparationState(caseId, snapshot.assetState, snapshot.calibrationState);
    snapshot.planningState = buildPlanningState(caseId);
    snapshot.registrationState = buildRegistrationState(caseId);
    snapshot.navigationState = buildNavigationState(caseId);
    snapshot.evaluationState = buildEvaluationState(caseId);

    if (m_runtimeState) {
        snapshot.navigationState.activeToolId = m_runtimeState->navigationToolId();
        snapshot.navigationState.toolVisible =
            m_runtimeState->isTrackedInstrumentVisible(snapshot.navigationState.activeToolId);
        snapshot.navigationState.latestPoseSummary =
            m_runtimeState->activeInstrumentPoseSummary(snapshot.navigationState.activeToolId);
    }

    if (m_runtimeState && m_runtimeState->hasTrackingQuality()) {
        snapshot.navigationState.trackerConnected = true;
        snapshot.calibrationState.trackingReady = true;
        snapshot.calibrationState.completed =
            m_runtimeState->trackingQuality().value(QStringLiteral("calibrated")).toBool();
        snapshot.calibrationState.accuracy =
            m_runtimeState->trackingQuality().value(QStringLiteral("calibration_accuracy_mm")).toDouble();
        snapshot.preparationState =
            buildPreparationState(caseId, snapshot.assetState, snapshot.calibrationState);
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
    mergedState.activeToolId = navigationState.activeToolId.isEmpty()
        ? mergedState.activeToolId
        : navigationState.activeToolId;
    const bool onlyGateUpdate = !navigationState.running
        && !navigationState.hasRunRecord
        && !navigationState.hasEvaluationReport
        && !navigationState.exportAvailable
        && navigationState.latestPoseSummary.isEmpty();
    mergedState.toolVisible = onlyGateUpdate ? mergedState.toolVisible : navigationState.toolVisible;
    mergedState.running = onlyGateUpdate ? mergedState.running : navigationState.running;
    mergedState.confidence = navigationState.confidence;
    mergedState.allowNavigation = navigationState.allowNavigation;
    mergedState.blockReasons = navigationState.blockReasons;
    mergedState.hasRunRecord = navigationState.hasRunRecord || mergedState.hasRunRecord;
    mergedState.hasEvaluationReport =
        navigationState.hasEvaluationReport || mergedState.hasEvaluationReport;
    mergedState.exportAvailable = navigationState.exportAvailable || mergedState.exportAvailable;

    if (!navigationState.summaryText.isEmpty()) {
        mergedState.summaryText = navigationState.summaryText;
    }
    if (!navigationState.latestPoseSummary.isEmpty()) {
        mergedState.latestPoseSummary = navigationState.latestPoseSummary;
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

NavigationWorkspacePreparationState NavigationWorkspaceApplicationService::buildPreparationState(
    const QString& caseId,
    const NavigationWorkspaceAssetState& assetState,
    const NavigationWorkspaceCalibrationState& calibrationState) const
{
    NavigationWorkspacePreparationState state;
    for (const NavigationInstrumentGeometryState& binding : assetState.instrumentGeometryBindings) {
        NavigationInstrumentCalibrationState calibrationItem;
        calibrationItem.instrumentId = binding.instrumentId;
        calibrationItem.geometryId = binding.geometryId;
        calibrationItem.started = calibrationState.started;
        calibrationItem.collectedPoints = calibrationState.collectedPoints;
        calibrationItem.requiredPoints = calibrationState.requiredPoints;
        calibrationItem.completed = calibrationState.completed;
        calibrationItem.accuracy = calibrationState.accuracy;
        calibrationItem.completedAt = calibrationState.completedAt;
        state.instrumentCalibrationStates.append(calibrationItem);
    }

    const AnkleEvaluationSnapshot evaluationSnapshot = m_evaluationService.loadEvaluationSnapshot(caseId);
    for (NavigationInstrumentCalibrationState& calibrationItem : state.instrumentCalibrationStates) {
        calibrationItem.completed = calibrationItem.completed || evaluationSnapshot.calibrated;
        calibrationItem.accuracy = calibrationItem.accuracy > 0.0
            ? calibrationItem.accuracy
            : evaluationSnapshot.calibrationAccuracyMm;
    }

    state.allRequiredInstrumentsCalibrated =
        state.instrumentCalibrationStates.isEmpty()
        ? evaluationSnapshot.calibrated
        : std::all_of(
            state.instrumentCalibrationStates.cbegin(),
            state.instrumentCalibrationStates.cend(),
            [](const NavigationInstrumentCalibrationState& item) { return item.completed; });

    if (assetState.activeBoneAssets.isEmpty()) {
        state.blockingReasons.append(QStringLiteral("未选择活动骨骼"));
    }
    if (!assetState.geometryReady) {
        state.blockingReasons.append(QStringLiteral("器械几何文件未完成绑定"));
    }
    if (!state.allRequiredInstrumentsCalibrated) {
        state.blockingReasons.append(QStringLiteral("存在未完成标定的导航器械"));
    }

    return state;
}

NavigationWorkspaceNavigationState NavigationWorkspaceApplicationService::buildNavigationState(
    const QString& caseId) const
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

NavigationWorkspacePlanningState NavigationWorkspaceApplicationService::buildPlanningState(
    const QString& caseId) const
{
    NavigationWorkspacePlanningState state;
    const AnklePlanningData planning = m_planningService.loadPlanning(caseId);
    state.hasPlanning = !planning.primaryBones.isEmpty() || !planning.referenceLandmarks.isEmpty();
    state.targetRegionReady = planning.targetRegionRadiusMm > 0.0;
    state.targetBone = planning.primaryBones.isEmpty() ? QString() : planning.primaryBones.first();
    state.targetRegion = planning.targetRegionRadiusMm > 0.0
        ? QStringLiteral("radius=%1mm").arg(planning.targetRegionRadiusMm, 0, 'f', 1)
        : QString();
    state.referenceBones = planning.primaryBones;
    state.constraintRegions = planning.anatomicalConstraintRegions.keys();
    state.recommendedPointOrder = planning.recommendedPointOrder;
    state.completed = state.targetRegionReady
        && !state.targetBone.isEmpty()
        && !state.recommendedPointOrder.isEmpty();
    state.savedAt = QDateTime::currentDateTimeUtc();
    return state;
}

NavigationWorkspaceRegistrationState NavigationWorkspaceApplicationService::buildRegistrationState(
    const QString& caseId) const
{
    NavigationWorkspaceRegistrationState state;
    const AnkleEvaluationSnapshot snapshot = m_evaluationService.loadEvaluationSnapshot(caseId);
    state.success = snapshot.hasRegistration;
    state.fre = snapshot.fre;
    state.targetTre = snapshot.targetTre;
    state.coverageScore = snapshot.coverageScore;
    state.pointCount = snapshot.registrationMetrics.value(QStringLiteral("point_count")).toInt();
    state.perBoneResults =
        parsePerBoneResultsJson(snapshot.registrationMetrics.value(QStringLiteral("per_bone_results_json")));
    state.fusedNavigationSpaceReady =
        snapshot.registrationMetrics.value(QStringLiteral("fused_navigation_space_ready")).toBool();
    state.fusedNavigationSpacePath =
        snapshot.registrationMetrics.value(QStringLiteral("fused_navigation_space_path")).toString();
    state.fusedCoverageScore =
        snapshot.registrationMetrics.value(QStringLiteral("fused_coverage_score")).toDouble();
    if (!state.fusedNavigationSpaceReady) {
        state.fusionBlockingReasons.append(QStringLiteral("尚未生成融合导航空间"));
    }
    state.completedAt = QDateTime::currentDateTimeUtc();
    return state;
}

NavigationWorkspaceEvaluationState NavigationWorkspaceApplicationService::buildEvaluationState(
    const QString& caseId) const
{
    NavigationWorkspaceEvaluationState state;
    const AnkleEvaluationSnapshot snapshot = m_evaluationService.loadEvaluationSnapshot(caseId);
    const NavigationWorkspaceRegistrationState registrationState = buildRegistrationState(caseId);
    state.hasSummary = snapshot.hasEvaluationReport;
    state.errorMetrics = snapshot.evaluationMetrics;
    state.perBoneQualitySummary = buildPerBoneQualitySummary(registrationState.perBoneResults);
    state.navigationProcessSummary = buildEvaluationProcessSummary(snapshot);
    state.summaryText = snapshot.caseId;
    state.reportReady = snapshot.hasEvaluationReport;
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

NavigationWorkspaceAssetState NavigationWorkspaceApplicationService::buildAssetState(
    const QString& caseId) const
{
    NavigationWorkspaceAssetState state;
    const AnkleCaseManifest manifest = m_repository.loadManifest(caseId);
    const AnkleCaseAssetBindings bindings = m_repository.loadCaseAssetBindings(caseId);

    state.dicomReady = !manifest.dicomDir.isEmpty();
    state.boneModelReady = !manifest.modelAssets.isEmpty();
    if (!manifest.modelAssets.isEmpty()) {
        state.boneModelPath = manifest.modelAssets.first().normalizedPath;
        for (const AnkleModelAsset& asset : manifest.modelAssets) {
            state.selectedBoneAssets.append(asset.boneName);
        }
    }

    state.boundBoneAssets = bindings.boundBoneAssetIds;
    state.activeBoneAssets =
        bindings.activeBoneAssetIds.isEmpty() ? bindings.boundBoneAssetIds : bindings.activeBoneAssetIds;
    state.boundInstrumentIds = bindings.boundInstrumentAssetIds;
    state.activeInstrumentIds =
        bindings.activeInstrumentAssetIds.isEmpty()
        ? bindings.boundInstrumentAssetIds
        : bindings.activeInstrumentAssetIds;

    for (const AnkleInstrumentGeometryBinding& binding : bindings.instrumentGeometryBindings) {
        NavigationInstrumentGeometryState geometryState;
        geometryState.instrumentId = binding.instrumentAssetId;
        geometryState.geometryId = binding.geometryAssetId;
        geometryState.geometryFilePath = binding.geometryFilePath;
        geometryState.geometryReady = !binding.geometryFilePath.isEmpty();
        state.instrumentGeometryBindings.append(geometryState);
    }

    state.geometryReady = !state.instrumentGeometryBindings.isEmpty()
        && std::all_of(
            state.instrumentGeometryBindings.cbegin(),
            state.instrumentGeometryBindings.cend(),
            [](const NavigationInstrumentGeometryState& item) { return item.geometryReady; });
    state.selectedBoneAsset = !state.activeBoneAssets.isEmpty()
        ? state.activeBoneAssets.first()
        : (!state.selectedBoneAssets.isEmpty() ? state.selectedBoneAssets.first() : QString());

    if (!state.instrumentGeometryBindings.isEmpty()) {
        state.selectedInstrumentId = state.instrumentGeometryBindings.first().instrumentId;
        state.geometryId = state.instrumentGeometryBindings.first().geometryId;
        state.geometryFilePath = state.instrumentGeometryBindings.first().geometryFilePath;
    }

    return state;
}

AnkleWorkflowStage NavigationWorkspaceApplicationService::stageFromManifest(
    const QString& workflowStage) const
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
