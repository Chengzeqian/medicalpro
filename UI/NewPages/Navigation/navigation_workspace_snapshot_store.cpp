#include "UI/NewPages/Navigation/navigation_workspace_snapshot_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QString stageToString(AnkleWorkflowStage stage)
{
    switch (stage) {
    case AnkleWorkflowStage::Preparation:
        return QStringLiteral("preparation");
    case AnkleWorkflowStage::Planning:
        return QStringLiteral("planning");
    case AnkleWorkflowStage::Registration:
        return QStringLiteral("registration");
    case AnkleWorkflowStage::Navigation:
        return QStringLiteral("navigation");
    case AnkleWorkflowStage::Evaluation:
        return QStringLiteral("evaluation");
    }

    return QStringLiteral("preparation");
}

AnkleWorkflowStage stringToStage(const QString& value)
{
    if (value == QStringLiteral("planning")) {
        return AnkleWorkflowStage::Planning;
    }
    if (value == QStringLiteral("registration")) {
        return AnkleWorkflowStage::Registration;
    }
    if (value == QStringLiteral("navigation")) {
        return AnkleWorkflowStage::Navigation;
    }
    if (value == QStringLiteral("evaluation")) {
        return AnkleWorkflowStage::Evaluation;
    }
    return AnkleWorkflowStage::Preparation;
}

QJsonArray toJson(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }
    return array;
}

QStringList stringListFromJson(const QJsonArray& array)
{
    QStringList values;
    values.reserve(array.size());
    for (const QJsonValue& item : array) {
        values.append(item.toString());
    }
    return values;
}

QJsonObject toJson(const NavigationInstrumentGeometryState& state)
{
    QJsonObject object;
    object.insert(QStringLiteral("instrument_id"), state.instrumentId);
    object.insert(QStringLiteral("instrument_display_name"), state.instrumentDisplayName);
    object.insert(QStringLiteral("model_file_path"), state.modelFilePath);
    object.insert(QStringLiteral("geometry_id"), state.geometryId);
    object.insert(QStringLiteral("geometry_file_path"), state.geometryFilePath);
    object.insert(QStringLiteral("tracking_marker_id"), state.trackingMarkerId);
    object.insert(QStringLiteral("geometry_ready"), state.geometryReady);
    return object;
}

NavigationInstrumentGeometryState geometryStateFromJson(const QJsonObject& object)
{
    NavigationInstrumentGeometryState state;
    state.instrumentId = object.value(QStringLiteral("instrument_id")).toString();
    state.instrumentDisplayName = object.value(QStringLiteral("instrument_display_name")).toString();
    state.modelFilePath = object.value(QStringLiteral("model_file_path")).toString();
    state.geometryId = object.value(QStringLiteral("geometry_id")).toString();
    state.geometryFilePath = object.value(QStringLiteral("geometry_file_path")).toString();
    state.trackingMarkerId = object.value(QStringLiteral("tracking_marker_id")).toString();
    state.geometryReady = object.value(QStringLiteral("geometry_ready")).toBool();
    return state;
}

QJsonArray toJson(const QList<NavigationInstrumentGeometryState>& states)
{
    QJsonArray array;
    for (const NavigationInstrumentGeometryState& state : states) {
        array.append(toJson(state));
    }
    return array;
}

QList<NavigationInstrumentGeometryState> geometryStatesFromJson(const QJsonArray& array)
{
    QList<NavigationInstrumentGeometryState> states;
    states.reserve(array.size());
    for (const QJsonValue& value : array) {
        states.append(geometryStateFromJson(value.toObject()));
    }
    return states;
}

QJsonObject toJson(const NavigationWorkspaceAssetState& assetState)
{
    QJsonObject object;
    object.insert(QStringLiteral("dicom_ready"), assetState.dicomReady);
    object.insert(QStringLiteral("bone_model_ready"), assetState.boneModelReady);
    object.insert(QStringLiteral("bone_model_path"), assetState.boneModelPath);
    object.insert(QStringLiteral("bound_bone_assets"), toJson(assetState.boundBoneAssets));
    object.insert(QStringLiteral("active_bone_assets"), toJson(assetState.activeBoneAssets));
    object.insert(QStringLiteral("bound_instrument_ids"), toJson(assetState.boundInstrumentIds));
    object.insert(QStringLiteral("active_instrument_ids"), toJson(assetState.activeInstrumentIds));
    object.insert(QStringLiteral("instrument_geometry_bindings"), toJson(assetState.instrumentGeometryBindings));
    object.insert(QStringLiteral("geometry_ready"), assetState.geometryReady);
    object.insert(QStringLiteral("selected_bone_assets"), toJson(assetState.selectedBoneAssets));
    object.insert(QStringLiteral("selected_bone_asset"), assetState.selectedBoneAsset);
    object.insert(QStringLiteral("selected_instrument_id"), assetState.selectedInstrumentId);
    object.insert(QStringLiteral("selected_instrument_display_name"), assetState.selectedInstrumentDisplayName);
    object.insert(QStringLiteral("instrument_model_path"), assetState.instrumentModelPath);
    object.insert(QStringLiteral("geometry_file_path"), assetState.geometryFilePath);
    object.insert(QStringLiteral("geometry_id"), assetState.geometryId);
    object.insert(QStringLiteral("tracking_marker_id"), assetState.trackingMarkerId);
    object.insert(QStringLiteral("instrument_service_available"), assetState.instrumentServiceAvailable);
    object.insert(QStringLiteral("tool_visible"), assetState.toolVisible);
    return object;
}

NavigationWorkspaceAssetState assetStateFromJson(const QJsonObject& object)
{
    NavigationWorkspaceAssetState assetState;
    assetState.dicomReady = object.value(QStringLiteral("dicom_ready")).toBool();
    assetState.boneModelReady = object.value(QStringLiteral("bone_model_ready")).toBool();
    assetState.boneModelPath = object.value(QStringLiteral("bone_model_path")).toString();
    assetState.boundBoneAssets = stringListFromJson(object.value(QStringLiteral("bound_bone_assets")).toArray());
    assetState.activeBoneAssets = stringListFromJson(object.value(QStringLiteral("active_bone_assets")).toArray());
    assetState.boundInstrumentIds = stringListFromJson(object.value(QStringLiteral("bound_instrument_ids")).toArray());
    assetState.activeInstrumentIds = stringListFromJson(object.value(QStringLiteral("active_instrument_ids")).toArray());
    assetState.instrumentGeometryBindings =
        geometryStatesFromJson(object.value(QStringLiteral("instrument_geometry_bindings")).toArray());
    assetState.geometryReady = object.value(QStringLiteral("geometry_ready")).toBool();
    assetState.selectedBoneAssets = stringListFromJson(object.value(QStringLiteral("selected_bone_assets")).toArray());
    assetState.selectedBoneAsset = object.value(QStringLiteral("selected_bone_asset")).toString();
    assetState.selectedInstrumentId = object.value(QStringLiteral("selected_instrument_id")).toString();
    assetState.selectedInstrumentDisplayName = object.value(QStringLiteral("selected_instrument_display_name")).toString();
    assetState.instrumentModelPath = object.value(QStringLiteral("instrument_model_path")).toString();
    assetState.geometryFilePath = object.value(QStringLiteral("geometry_file_path")).toString();
    assetState.geometryId = object.value(QStringLiteral("geometry_id")).toString();
    assetState.trackingMarkerId = object.value(QStringLiteral("tracking_marker_id")).toString();
    assetState.instrumentServiceAvailable = object.value(QStringLiteral("instrument_service_available")).toBool();
    assetState.toolVisible = object.value(QStringLiteral("tool_visible")).toBool();
    return assetState;
}

QJsonObject toJson(const NavigationStageGate& gate)
{
    QJsonObject object;
    object.insert(QStringLiteral("requested_stage"), stageToString(gate.requestedStage));
    object.insert(QStringLiteral("allowed"), gate.allowed);
    object.insert(QStringLiteral("reason_code"), gate.reasonCode);
    object.insert(QStringLiteral("reason_text"), gate.reasonText);
    object.insert(QStringLiteral("severity"), gate.severity);
    object.insert(QStringLiteral("last_computed_at"), gate.lastComputedAt.toString(Qt::ISODateWithMs));
    return object;
}

NavigationStageGate gateFromJson(const QJsonObject& object)
{
    NavigationStageGate gate;
    gate.requestedStage = stringToStage(object.value(QStringLiteral("requested_stage")).toString());
    gate.allowed = object.value(QStringLiteral("allowed")).toBool();
    gate.reasonCode = object.value(QStringLiteral("reason_code")).toString();
    gate.reasonText = object.value(QStringLiteral("reason_text")).toString();
    gate.severity = object.value(QStringLiteral("severity")).toString();
    gate.lastComputedAt = QDateTime::fromString(object.value(QStringLiteral("last_computed_at")).toString(), Qt::ISODateWithMs);
    return gate;
}

QJsonObject toJson(const NavigationInstrumentCalibrationState& state)
{
    QJsonObject object;
    object.insert(QStringLiteral("instrument_id"), state.instrumentId);
    object.insert(QStringLiteral("geometry_id"), state.geometryId);
    object.insert(QStringLiteral("started"), state.started);
    object.insert(QStringLiteral("collected_points"), state.collectedPoints);
    object.insert(QStringLiteral("required_points"), state.requiredPoints);
    object.insert(QStringLiteral("completed"), state.completed);
    object.insert(QStringLiteral("accuracy"), state.accuracy);
    object.insert(QStringLiteral("completed_at"), state.completedAt.toString(Qt::ISODateWithMs));
    return object;
}

NavigationInstrumentCalibrationState instrumentCalibrationStateFromJson(const QJsonObject& object)
{
    NavigationInstrumentCalibrationState state;
    state.instrumentId = object.value(QStringLiteral("instrument_id")).toString();
    state.geometryId = object.value(QStringLiteral("geometry_id")).toString();
    state.started = object.value(QStringLiteral("started")).toBool();
    state.collectedPoints = object.value(QStringLiteral("collected_points")).toInt();
    state.requiredPoints = object.value(QStringLiteral("required_points")).toInt();
    state.completed = object.value(QStringLiteral("completed")).toBool();
    state.accuracy = object.value(QStringLiteral("accuracy")).toDouble();
    state.completedAt = QDateTime::fromString(
        object.value(QStringLiteral("completed_at")).toString(),
        Qt::ISODateWithMs);
    return state;
}

QJsonArray toJson(const QList<NavigationInstrumentCalibrationState>& states)
{
    QJsonArray array;
    for (const NavigationInstrumentCalibrationState& state : states) {
        array.append(toJson(state));
    }
    return array;
}

QList<NavigationInstrumentCalibrationState> instrumentCalibrationStatesFromJson(const QJsonArray& array)
{
    QList<NavigationInstrumentCalibrationState> states;
    states.reserve(array.size());
    for (const QJsonValue& value : array) {
        states.append(instrumentCalibrationStateFromJson(value.toObject()));
    }
    return states;
}

QJsonObject toJson(const NavigationWorkspaceCalibrationState& calibrationState)
{
    QJsonObject object;
    object.insert(QStringLiteral("tracking_ready"), calibrationState.trackingReady);
    object.insert(QStringLiteral("started"), calibrationState.started);
    object.insert(QStringLiteral("collected_points"), calibrationState.collectedPoints);
    object.insert(QStringLiteral("required_points"), calibrationState.requiredPoints);
    object.insert(QStringLiteral("completed"), calibrationState.completed);
    object.insert(QStringLiteral("tip_offset"), calibrationState.tipOffset);
    object.insert(QStringLiteral("accuracy"), calibrationState.accuracy);
    object.insert(QStringLiteral("status_text"), calibrationState.statusText);
    object.insert(QStringLiteral("geometry_id"), calibrationState.geometryId);
    object.insert(QStringLiteral("completed_at"), calibrationState.completedAt.toString(Qt::ISODateWithMs));
    return object;
}

NavigationWorkspaceCalibrationState calibrationStateFromJson(const QJsonObject& object)
{
    NavigationWorkspaceCalibrationState calibrationState;
    calibrationState.trackingReady = object.value(QStringLiteral("tracking_ready")).toBool();
    calibrationState.started = object.value(QStringLiteral("started")).toBool();
    calibrationState.collectedPoints = object.value(QStringLiteral("collected_points")).toInt();
    calibrationState.requiredPoints = object.value(QStringLiteral("required_points")).toInt();
    calibrationState.completed = object.value(QStringLiteral("completed")).toBool();
    calibrationState.tipOffset = object.value(QStringLiteral("tip_offset")).toDouble();
    calibrationState.accuracy = object.value(QStringLiteral("accuracy")).toDouble();
    calibrationState.statusText = object.value(QStringLiteral("status_text")).toString();
    calibrationState.geometryId = object.value(QStringLiteral("geometry_id")).toString();
    calibrationState.completedAt = QDateTime::fromString(
        object.value(QStringLiteral("completed_at")).toString(),
        Qt::ISODateWithMs);
    return calibrationState;
}

QJsonObject toJson(const NavigationWorkspacePreparationState& preparationState)
{
    QJsonObject object;
    object.insert(
        QStringLiteral("instrument_calibration_states"),
        toJson(preparationState.instrumentCalibrationStates));
    object.insert(
        QStringLiteral("all_required_instruments_calibrated"),
        preparationState.allRequiredInstrumentsCalibrated);
    object.insert(QStringLiteral("blocking_reasons"), toJson(preparationState.blockingReasons));
    return object;
}

NavigationWorkspacePreparationState preparationStateFromJson(const QJsonObject& object)
{
    NavigationWorkspacePreparationState preparationState;
    preparationState.instrumentCalibrationStates =
        instrumentCalibrationStatesFromJson(
            object.value(QStringLiteral("instrument_calibration_states")).toArray());
    preparationState.allRequiredInstrumentsCalibrated =
        object.value(QStringLiteral("all_required_instruments_calibrated")).toBool();
    preparationState.blockingReasons =
        stringListFromJson(object.value(QStringLiteral("blocking_reasons")).toArray());
    return preparationState;
}

QJsonObject toJson(const NavigationWorkspacePlanningState& planningState)
{
    QJsonObject object;
    object.insert(QStringLiteral("has_planning"), planningState.hasPlanning);
    object.insert(QStringLiteral("target_region_ready"), planningState.targetRegionReady);
    object.insert(QStringLiteral("target_bone"), planningState.targetBone);
    object.insert(QStringLiteral("target_region"), planningState.targetRegion);
    object.insert(QStringLiteral("reference_bones"), toJson(planningState.referenceBones));
    object.insert(QStringLiteral("constraint_regions"), toJson(planningState.constraintRegions));
    object.insert(QStringLiteral("recommended_point_order"), toJson(planningState.recommendedPointOrder));
    object.insert(QStringLiteral("completed"), planningState.completed);
    object.insert(QStringLiteral("saved_at"), planningState.savedAt.toString(Qt::ISODateWithMs));
    return object;
}

NavigationWorkspacePlanningState planningStateFromJson(const QJsonObject& object)
{
    NavigationWorkspacePlanningState planningState;
    planningState.hasPlanning = object.value(QStringLiteral("has_planning")).toBool();
    planningState.targetRegionReady = object.value(QStringLiteral("target_region_ready")).toBool();
    planningState.targetBone = object.value(QStringLiteral("target_bone")).toString();
    planningState.targetRegion = object.value(QStringLiteral("target_region")).toString();
    planningState.referenceBones = stringListFromJson(object.value(QStringLiteral("reference_bones")).toArray());
    planningState.constraintRegions = stringListFromJson(object.value(QStringLiteral("constraint_regions")).toArray());
    planningState.recommendedPointOrder =
        stringListFromJson(object.value(QStringLiteral("recommended_point_order")).toArray());
    planningState.completed = object.value(QStringLiteral("completed")).toBool();
    planningState.savedAt = QDateTime::fromString(
        object.value(QStringLiteral("saved_at")).toString(),
        Qt::ISODateWithMs);
    return planningState;
}

QJsonObject toJson(const NavigationPerBoneRegistrationState& state)
{
    QJsonObject object;
    object.insert(QStringLiteral("bone_asset_id"), state.boneAssetId);
    object.insert(QStringLiteral("bone_region_id"), state.boneRegionId);
    object.insert(QStringLiteral("point_count"), state.pointCount);
    object.insert(QStringLiteral("success"), state.success);
    object.insert(QStringLiteral("fre"), state.fre);
    object.insert(QStringLiteral("target_tre"), state.targetTre);
    object.insert(QStringLiteral("coverage_score"), state.coverageScore);
    object.insert(QStringLiteral("transform_matrix"), state.transformMatrix);
    object.insert(QStringLiteral("completed_at"), state.completedAt.toString(Qt::ISODateWithMs));
    return object;
}

NavigationPerBoneRegistrationState perBoneRegistrationStateFromJson(const QJsonObject& object)
{
    NavigationPerBoneRegistrationState state;
    state.boneAssetId = object.value(QStringLiteral("bone_asset_id")).toString();
    state.boneRegionId = object.value(QStringLiteral("bone_region_id")).toString();
    state.pointCount = object.value(QStringLiteral("point_count")).toInt();
    state.success = object.value(QStringLiteral("success")).toBool();
    state.fre = object.value(QStringLiteral("fre")).toDouble();
    state.targetTre = object.value(QStringLiteral("target_tre")).toDouble();
    state.coverageScore = object.value(QStringLiteral("coverage_score")).toDouble();
    state.transformMatrix = object.value(QStringLiteral("transform_matrix")).toString();
    state.completedAt = QDateTime::fromString(
        object.value(QStringLiteral("completed_at")).toString(),
        Qt::ISODateWithMs);
    return state;
}

QJsonArray toJson(const QList<NavigationPerBoneRegistrationState>& states)
{
    QJsonArray array;
    for (const NavigationPerBoneRegistrationState& state : states) {
        array.append(toJson(state));
    }
    return array;
}

QList<NavigationPerBoneRegistrationState> perBoneRegistrationStatesFromJson(const QJsonArray& array)
{
    QList<NavigationPerBoneRegistrationState> states;
    states.reserve(array.size());
    for (const QJsonValue& value : array) {
        states.append(perBoneRegistrationStateFromJson(value.toObject()));
    }
    return states;
}

QJsonObject toJson(const NavigationWorkspaceRegistrationState& registrationState)
{
    QJsonObject object;
    object.insert(QStringLiteral("point_count"), registrationState.pointCount);
    object.insert(QStringLiteral("success"), registrationState.success);
    object.insert(QStringLiteral("fre"), registrationState.fre);
    object.insert(QStringLiteral("target_tre"), registrationState.targetTre);
    object.insert(QStringLiteral("coverage_score"), registrationState.coverageScore);
    object.insert(QStringLiteral("registration_method_id"), registrationState.registrationMethodId);
    object.insert(QStringLiteral("point_selection_strategy_id"), registrationState.pointSelectionStrategyId);
    object.insert(QStringLiteral("refine_method"), registrationState.refineMethod);
    object.insert(QStringLiteral("constraint_refine_used"), registrationState.constraintRefineUsed);
    object.insert(QStringLiteral("constraint_region_count"), registrationState.constraintRegionCount);
    object.insert(QStringLiteral("target_region_radius_mm"), registrationState.targetRegionRadiusMm);
    object.insert(QStringLiteral("translation_x"), registrationState.translationX);
    object.insert(QStringLiteral("translation_y"), registrationState.translationY);
    object.insert(QStringLiteral("translation_z"), registrationState.translationZ);
    object.insert(QStringLiteral("rotation_x"), registrationState.rotationX);
    object.insert(QStringLiteral("rotation_y"), registrationState.rotationY);
    object.insert(QStringLiteral("rotation_z"), registrationState.rotationZ);
    object.insert(QStringLiteral("transform_matrix"), registrationState.transformMatrix);
    object.insert(QStringLiteral("per_bone_results"), toJson(registrationState.perBoneResults));
    object.insert(
        QStringLiteral("fused_navigation_space_ready"),
        registrationState.fusedNavigationSpaceReady);
    object.insert(
        QStringLiteral("fused_navigation_space_path"),
        registrationState.fusedNavigationSpacePath);
    object.insert(
        QStringLiteral("fused_coverage_score"),
        registrationState.fusedCoverageScore);
    object.insert(
        QStringLiteral("fusion_blocking_reasons"),
        toJson(registrationState.fusionBlockingReasons));
    object.insert(QStringLiteral("completed_at"), registrationState.completedAt.toString(Qt::ISODateWithMs));
    return object;
}

NavigationWorkspaceRegistrationState registrationStateFromJson(const QJsonObject& object)
{
    NavigationWorkspaceRegistrationState registrationState;
    registrationState.pointCount = object.value(QStringLiteral("point_count")).toInt();
    registrationState.success = object.value(QStringLiteral("success")).toBool();
    registrationState.fre = object.value(QStringLiteral("fre")).toDouble();
    registrationState.targetTre = object.value(QStringLiteral("target_tre")).toDouble();
    registrationState.coverageScore = object.value(QStringLiteral("coverage_score")).toDouble();
    registrationState.registrationMethodId = object.value(QStringLiteral("registration_method_id")).toString();
    registrationState.pointSelectionStrategyId = object.value(QStringLiteral("point_selection_strategy_id")).toString();
    registrationState.refineMethod = object.value(QStringLiteral("refine_method")).toString();
    registrationState.constraintRefineUsed = object.value(QStringLiteral("constraint_refine_used")).toBool();
    registrationState.constraintRegionCount = object.value(QStringLiteral("constraint_region_count")).toInt();
    registrationState.targetRegionRadiusMm = object.value(QStringLiteral("target_region_radius_mm")).toDouble();
    registrationState.translationX = object.value(QStringLiteral("translation_x")).toDouble();
    registrationState.translationY = object.value(QStringLiteral("translation_y")).toDouble();
    registrationState.translationZ = object.value(QStringLiteral("translation_z")).toDouble();
    registrationState.rotationX = object.value(QStringLiteral("rotation_x")).toDouble();
    registrationState.rotationY = object.value(QStringLiteral("rotation_y")).toDouble();
    registrationState.rotationZ = object.value(QStringLiteral("rotation_z")).toDouble();
    registrationState.transformMatrix = object.value(QStringLiteral("transform_matrix")).toString();
    registrationState.perBoneResults =
        perBoneRegistrationStatesFromJson(object.value(QStringLiteral("per_bone_results")).toArray());
    registrationState.fusedNavigationSpaceReady =
        object.value(QStringLiteral("fused_navigation_space_ready")).toBool();
    registrationState.fusedNavigationSpacePath =
        object.value(QStringLiteral("fused_navigation_space_path")).toString();
    registrationState.fusedCoverageScore =
        object.value(QStringLiteral("fused_coverage_score")).toDouble();
    registrationState.fusionBlockingReasons =
        stringListFromJson(object.value(QStringLiteral("fusion_blocking_reasons")).toArray());
    registrationState.completedAt = QDateTime::fromString(
        object.value(QStringLiteral("completed_at")).toString(),
        Qt::ISODateWithMs);
    return registrationState;
}

QJsonObject toJson(const NavigationWorkspaceNavigationState& navigationState)
{
    QJsonObject object;
    object.insert(QStringLiteral("tracker_connected"), navigationState.trackerConnected);
    object.insert(QStringLiteral("active_tool_id"), navigationState.activeToolId);
    object.insert(QStringLiteral("tool_visible"), navigationState.toolVisible);
    object.insert(QStringLiteral("running"), navigationState.running);
    object.insert(QStringLiteral("confidence"), navigationState.confidence);
    object.insert(QStringLiteral("allow_navigation"), navigationState.allowNavigation);
    object.insert(QStringLiteral("block_reasons"), toJson(navigationState.blockReasons));
    object.insert(QStringLiteral("latest_pose_summary"), navigationState.latestPoseSummary);
    object.insert(QStringLiteral("has_run_record"), navigationState.hasRunRecord);
    object.insert(QStringLiteral("has_evaluation_report"), navigationState.hasEvaluationReport);
    object.insert(QStringLiteral("summary_text"), navigationState.summaryText);
    object.insert(QStringLiteral("export_available"), navigationState.exportAvailable);
    return object;
}

NavigationWorkspaceNavigationState navigationStateFromJson(const QJsonObject& object)
{
    NavigationWorkspaceNavigationState navigationState;
    navigationState.trackerConnected = object.value(QStringLiteral("tracker_connected")).toBool();
    navigationState.activeToolId = object.value(QStringLiteral("active_tool_id")).toString();
    navigationState.toolVisible = object.value(QStringLiteral("tool_visible")).toBool();
    navigationState.running = object.value(QStringLiteral("running")).toBool();
    navigationState.confidence = object.value(QStringLiteral("confidence")).toDouble();
    navigationState.allowNavigation = object.value(QStringLiteral("allow_navigation")).toBool();
    navigationState.blockReasons = stringListFromJson(object.value(QStringLiteral("block_reasons")).toArray());
    navigationState.latestPoseSummary = object.value(QStringLiteral("latest_pose_summary")).toString();
    navigationState.hasRunRecord = object.value(QStringLiteral("has_run_record")).toBool();
    navigationState.hasEvaluationReport = object.value(QStringLiteral("has_evaluation_report")).toBool();
    navigationState.summaryText = object.value(QStringLiteral("summary_text")).toString();
    navigationState.exportAvailable = object.value(QStringLiteral("export_available")).toBool();
    return navigationState;
}

QJsonObject toJson(const NavigationWorkspaceEvaluationState& evaluationState)
{
    QJsonObject object;
    object.insert(QStringLiteral("has_summary"), evaluationState.hasSummary);
    object.insert(QStringLiteral("error_metrics"), QJsonObject::fromVariantMap(evaluationState.errorMetrics));
    object.insert(QStringLiteral("per_bone_quality_summary"), toJson(evaluationState.perBoneQualitySummary));
    object.insert(QStringLiteral("navigation_process_summary"), evaluationState.navigationProcessSummary);
    object.insert(QStringLiteral("summary_text"), evaluationState.summaryText);
    object.insert(QStringLiteral("report_ready"), evaluationState.reportReady);
    object.insert(QStringLiteral("exportable_artifacts"), toJson(evaluationState.exportableArtifacts));
    object.insert(QStringLiteral("last_updated_at"), evaluationState.lastUpdatedAt.toString(Qt::ISODateWithMs));
    return object;
}

NavigationWorkspaceEvaluationState evaluationStateFromJson(const QJsonObject& object)
{
    NavigationWorkspaceEvaluationState evaluationState;
    evaluationState.hasSummary = object.value(QStringLiteral("has_summary")).toBool();
    evaluationState.errorMetrics = object.value(QStringLiteral("error_metrics")).toObject().toVariantMap();
    evaluationState.perBoneQualitySummary =
        stringListFromJson(object.value(QStringLiteral("per_bone_quality_summary")).toArray());
    evaluationState.navigationProcessSummary =
        object.value(QStringLiteral("navigation_process_summary")).toString();
    evaluationState.summaryText = object.value(QStringLiteral("summary_text")).toString();
    evaluationState.reportReady = object.value(QStringLiteral("report_ready")).toBool();
    evaluationState.exportableArtifacts =
        stringListFromJson(object.value(QStringLiteral("exportable_artifacts")).toArray());
    evaluationState.lastUpdatedAt = QDateTime::fromString(
        object.value(QStringLiteral("last_updated_at")).toString(),
        Qt::ISODateWithMs);
    return evaluationState;
}

QJsonObject toJson(const NavigationWorkspaceSnapshot& snapshot)
{
    QJsonObject object;
    object.insert(QStringLiteral("case_id"), snapshot.caseId);
    object.insert(QStringLiteral("current_stage"), stageToString(snapshot.caseContext.currentStage));
    object.insert(QStringLiteral("asset_state"), toJson(snapshot.assetState));
    object.insert(QStringLiteral("stage_gate"), toJson(snapshot.stageGate));
    object.insert(QStringLiteral("calibration_state"), toJson(snapshot.calibrationState));
    object.insert(QStringLiteral("preparation_state"), toJson(snapshot.preparationState));
    object.insert(QStringLiteral("planning_state"), toJson(snapshot.planningState));
    object.insert(QStringLiteral("registration_state"), toJson(snapshot.registrationState));
    object.insert(QStringLiteral("navigation_state"), toJson(snapshot.navigationState));
    object.insert(QStringLiteral("evaluation_state"), toJson(snapshot.evaluationState));
    object.insert(QStringLiteral("last_refreshed_at"), snapshot.lastRefreshedAt.toString(Qt::ISODateWithMs));
    return object;
}

NavigationWorkspaceSnapshot snapshotFromJson(const QJsonObject& object)
{
    NavigationWorkspaceSnapshot snapshot;
    snapshot.caseId = object.value(QStringLiteral("case_id")).toString();
    snapshot.caseContext.currentStage = stringToStage(object.value(QStringLiteral("current_stage")).toString());
    snapshot.assetState = assetStateFromJson(object.value(QStringLiteral("asset_state")).toObject());
    snapshot.stageGate = gateFromJson(object.value(QStringLiteral("stage_gate")).toObject());
    snapshot.calibrationState = calibrationStateFromJson(object.value(QStringLiteral("calibration_state")).toObject());
    snapshot.preparationState = preparationStateFromJson(object.value(QStringLiteral("preparation_state")).toObject());
    snapshot.planningState = planningStateFromJson(object.value(QStringLiteral("planning_state")).toObject());
    snapshot.registrationState = registrationStateFromJson(object.value(QStringLiteral("registration_state")).toObject());
    snapshot.navigationState = navigationStateFromJson(object.value(QStringLiteral("navigation_state")).toObject());
    snapshot.evaluationState = evaluationStateFromJson(object.value(QStringLiteral("evaluation_state")).toObject());
    snapshot.lastRefreshedAt = QDateTime::fromString(object.value(QStringLiteral("last_refreshed_at")).toString(), Qt::ISODateWithMs);
    return snapshot;
}
}

NavigationWorkspaceSnapshotStore::NavigationWorkspaceSnapshotStore(const QString& caseRoot)
    : m_caseRoot(caseRoot)
{
}

bool NavigationWorkspaceSnapshotStore::persistSnapshot(const NavigationWorkspaceSnapshot& snapshot) const
{
    QDir dir;
    if (!dir.mkpath(QFileInfo(snapshotPath()).absolutePath())) {
        return false;
    }

    QFile file(snapshotPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(toJson(snapshot)).toJson(QJsonDocument::Indented));
    return file.error() == QFile::NoError;
}

NavigationWorkspaceSnapshot NavigationWorkspaceSnapshotStore::loadSnapshot() const
{
    QFile file(snapshotPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {};
    }

    return snapshotFromJson(document.object());
}

QString NavigationWorkspaceSnapshotStore::latestSnapshotPath() const
{
    return snapshotPath();
}

QString NavigationWorkspaceSnapshotStore::snapshotPath() const
{
    return m_caseRoot + QStringLiteral("/navigation/workspace_snapshot.json");
}
