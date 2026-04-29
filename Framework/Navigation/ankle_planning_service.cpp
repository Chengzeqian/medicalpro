#include "Framework/Navigation/ankle_planning_service.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace
{
QJsonObject vectorToJson(const QVector3D& vector)
{
    QJsonObject object;
    object.insert(QStringLiteral("x"), vector.x());
    object.insert(QStringLiteral("y"), vector.y());
    object.insert(QStringLiteral("z"), vector.z());
    return object;
}

QVector3D vectorFromJson(const QJsonObject& object)
{
    return QVector3D(
        static_cast<float>(object.value(QStringLiteral("x")).toDouble()),
        static_cast<float>(object.value(QStringLiteral("y")).toDouble()),
        static_cast<float>(object.value(QStringLiteral("z")).toDouble()));
}

QJsonObject quaternionToJson(const QQuaternion& quaternion)
{
    QJsonObject object;
    object.insert(QStringLiteral("scalar"), quaternion.scalar());
    object.insert(QStringLiteral("x"), quaternion.x());
    object.insert(QStringLiteral("y"), quaternion.y());
    object.insert(QStringLiteral("z"), quaternion.z());
    return object;
}

QQuaternion quaternionFromJson(const QJsonObject& object)
{
    return QQuaternion(
        static_cast<float>(object.value(QStringLiteral("scalar")).toDouble()),
        static_cast<float>(object.value(QStringLiteral("x")).toDouble()),
        static_cast<float>(object.value(QStringLiteral("y")).toDouble()),
        static_cast<float>(object.value(QStringLiteral("z")).toDouble()));
}
}

AnklePlanningService::AnklePlanningService(const AnkleCaseWorkspaceRepository& repository)
    : m_repository(repository)
{
}

AnklePlanningData AnklePlanningService::createDefaultPlanning(const QString& caseId) const
{
    AnklePlanningData planning;
    planning.caseId = caseId;
    planning.planningFileVersion = QStringLiteral("1.0");
    return planning;
}

bool AnklePlanningService::savePlanning(const QString& caseId, const AnklePlanningData& planning) const
{
    QDir dir;
    if (!dir.exists(m_repository.stagePath(caseId, QStringLiteral("planning"))) &&
        !dir.mkpath(m_repository.stagePath(caseId, QStringLiteral("planning")))) {
        return false;
    }

    QFile planningFile(planningPath(caseId));
    if (!planningFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    planningFile.write(QJsonDocument(toJson(planning)).toJson(QJsonDocument::Indented));
    return planningFile.error() == QFile::NoError;
}

AnklePlanningData AnklePlanningService::loadPlanning(const QString& caseId) const
{
    QFile planningFile(planningPath(caseId));
    if (!planningFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(planningFile.readAll());
    if (!document.isObject()) {
        return {};
    }

    return fromJson(document.object());
}

QVariantMap AnklePlanningService::buildDashboardReadiness(const QString& caseId) const
{
    const AnkleCaseManifest manifest = m_repository.loadManifest(caseId);
    const AnklePlanningData planning = loadPlanning(caseId);

    QVariantMap readiness;
    readiness.insert(QStringLiteral("case_ready"), !manifest.caseId.isEmpty());
    readiness.insert(QStringLiteral("dicom_ready"), !manifest.dicomDir.isEmpty());
    readiness.insert(QStringLiteral("planning_ready"), !planning.primaryBones.isEmpty() && !planning.referenceLandmarks.isEmpty());
    readiness.insert(QStringLiteral("registration_ready"), false);
    readiness.insert(QStringLiteral("evaluation_ready"), false);
    return readiness;
}

QString AnklePlanningService::planningPath(const QString& caseId) const
{
    return m_repository.stagePath(caseId, QStringLiteral("planning")) + QStringLiteral("/planning.json");
}

QJsonObject AnklePlanningService::toJson(const AnklePlanningData& planning) const
{
    QJsonObject landmarksObject;
    for (auto it = planning.referenceLandmarks.cbegin(); it != planning.referenceLandmarks.cend(); ++it) {
        landmarksObject.insert(it.key(), vectorToJson(it.value()));
    }

    QJsonArray bonesArray;
    for (const QString& bone : planning.primaryBones) {
        bonesArray.append(bone);
    }

    QJsonArray orderArray;
    for (const QString& pointName : planning.recommendedPointOrder) {
        orderArray.append(pointName);
    }

    QJsonObject object;
    object.insert(QStringLiteral("case_id"), planning.caseId);
    object.insert(QStringLiteral("primary_bones"), bonesArray);
    object.insert(QStringLiteral("reference_landmarks"), landmarksObject);
    object.insert(QStringLiteral("recommended_point_order"), orderArray);
    object.insert(QStringLiteral("target_translation"), vectorToJson(planning.targetTranslation));
    object.insert(QStringLiteral("target_orientation"), quaternionToJson(planning.targetOrientation));
    object.insert(QStringLiteral("planning_file_version"), planning.planningFileVersion);
    return object;
}

AnklePlanningData AnklePlanningService::fromJson(const QJsonObject& object) const
{
    AnklePlanningData planning;
    planning.caseId = object.value(QStringLiteral("case_id")).toString();
    planning.planningFileVersion = object.value(QStringLiteral("planning_file_version")).toString();

    const QJsonArray bonesArray = object.value(QStringLiteral("primary_bones")).toArray();
    for (const QJsonValue& value : bonesArray) {
        planning.primaryBones.append(value.toString());
    }

    const QJsonObject landmarksObject = object.value(QStringLiteral("reference_landmarks")).toObject();
    for (auto it = landmarksObject.begin(); it != landmarksObject.end(); ++it) {
        planning.referenceLandmarks.insert(it.key(), vectorFromJson(it.value().toObject()));
    }

    const QJsonArray orderArray = object.value(QStringLiteral("recommended_point_order")).toArray();
    for (const QJsonValue& value : orderArray) {
        planning.recommendedPointOrder.append(value.toString());
    }

    planning.targetTranslation = vectorFromJson(object.value(QStringLiteral("target_translation")).toObject());
    planning.targetOrientation = quaternionFromJson(object.value(QStringLiteral("target_orientation")).toObject());
    return planning;
}
