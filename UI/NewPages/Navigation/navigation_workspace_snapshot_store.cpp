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

QJsonObject toJson(const NavigationWorkspaceRegistrationState& registrationState)
{
    QJsonObject object;
    object.insert(QStringLiteral("point_count"), registrationState.pointCount);
    object.insert(QStringLiteral("success"), registrationState.success);
    object.insert(QStringLiteral("fre"), registrationState.fre);
    object.insert(QStringLiteral("target_tre"), registrationState.targetTre);
    object.insert(QStringLiteral("coverage_score"), registrationState.coverageScore);
    object.insert(QStringLiteral("translation_x"), registrationState.translationX);
    object.insert(QStringLiteral("translation_y"), registrationState.translationY);
    object.insert(QStringLiteral("translation_z"), registrationState.translationZ);
    object.insert(QStringLiteral("rotation_x"), registrationState.rotationX);
    object.insert(QStringLiteral("rotation_y"), registrationState.rotationY);
    object.insert(QStringLiteral("rotation_z"), registrationState.rotationZ);
    object.insert(QStringLiteral("transform_matrix"), registrationState.transformMatrix);
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
    registrationState.translationX = object.value(QStringLiteral("translation_x")).toDouble();
    registrationState.translationY = object.value(QStringLiteral("translation_y")).toDouble();
    registrationState.translationZ = object.value(QStringLiteral("translation_z")).toDouble();
    registrationState.rotationX = object.value(QStringLiteral("rotation_x")).toDouble();
    registrationState.rotationY = object.value(QStringLiteral("rotation_y")).toDouble();
    registrationState.rotationZ = object.value(QStringLiteral("rotation_z")).toDouble();
    registrationState.transformMatrix = object.value(QStringLiteral("transform_matrix")).toString();
    registrationState.completedAt = QDateTime::fromString(
        object.value(QStringLiteral("completed_at")).toString(),
        Qt::ISODateWithMs);
    return registrationState;
}

QJsonObject toJson(const NavigationWorkspaceNavigationState& navigationState)
{
    QJsonObject object;
    object.insert(QStringLiteral("tracker_connected"), navigationState.trackerConnected);
    object.insert(QStringLiteral("tool_visible"), navigationState.toolVisible);
    object.insert(QStringLiteral("running"), navigationState.running);
    object.insert(QStringLiteral("confidence"), navigationState.confidence);
    object.insert(QStringLiteral("allow_navigation"), navigationState.allowNavigation);
    object.insert(QStringLiteral("block_reasons"), toJson(navigationState.blockReasons));
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
    navigationState.toolVisible = object.value(QStringLiteral("tool_visible")).toBool();
    navigationState.running = object.value(QStringLiteral("running")).toBool();
    navigationState.confidence = object.value(QStringLiteral("confidence")).toDouble();
    navigationState.allowNavigation = object.value(QStringLiteral("allow_navigation")).toBool();
    navigationState.blockReasons = stringListFromJson(object.value(QStringLiteral("block_reasons")).toArray());
    navigationState.hasRunRecord = object.value(QStringLiteral("has_run_record")).toBool();
    navigationState.hasEvaluationReport = object.value(QStringLiteral("has_evaluation_report")).toBool();
    navigationState.summaryText = object.value(QStringLiteral("summary_text")).toString();
    navigationState.exportAvailable = object.value(QStringLiteral("export_available")).toBool();
    return navigationState;
}

QJsonObject toJson(const NavigationWorkspaceSnapshot& snapshot)
{
    QJsonObject object;
    object.insert(QStringLiteral("case_id"), snapshot.caseId);
    object.insert(QStringLiteral("current_stage"), stageToString(snapshot.caseContext.currentStage));
    object.insert(QStringLiteral("stage_gate"), toJson(snapshot.stageGate));
    object.insert(QStringLiteral("calibration_state"), toJson(snapshot.calibrationState));
    object.insert(QStringLiteral("registration_state"), toJson(snapshot.registrationState));
    object.insert(QStringLiteral("navigation_state"), toJson(snapshot.navigationState));
    object.insert(QStringLiteral("last_refreshed_at"), snapshot.lastRefreshedAt.toString(Qt::ISODateWithMs));
    return object;
}

NavigationWorkspaceSnapshot snapshotFromJson(const QJsonObject& object)
{
    NavigationWorkspaceSnapshot snapshot;
    snapshot.caseId = object.value(QStringLiteral("case_id")).toString();
    snapshot.caseContext.currentStage = stringToStage(object.value(QStringLiteral("current_stage")).toString());
    snapshot.stageGate = gateFromJson(object.value(QStringLiteral("stage_gate")).toObject());
    snapshot.calibrationState = calibrationStateFromJson(object.value(QStringLiteral("calibration_state")).toObject());
    snapshot.registrationState = registrationStateFromJson(object.value(QStringLiteral("registration_state")).toObject());
    snapshot.navigationState = navigationStateFromJson(object.value(QStringLiteral("navigation_state")).toObject());
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
