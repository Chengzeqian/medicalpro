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

QJsonArray vectorListToJson(const QList<QVector3D>& vectors)
{
    QJsonArray array;
    for (const QVector3D& vector : vectors) {
        array.append(vectorToJson(vector));
    }
    return array;
}

QList<QVector3D> vectorListFromJson(const QJsonArray& array)
{
    QList<QVector3D> vectors;
    vectors.reserve(array.size());
    for (const QJsonValue& value : array) {
        vectors.append(vectorFromJson(value.toObject()));
    }
    return vectors;
}

QJsonObject constraintRegionMetadataToJson(const AnkleConstraintRegionMetadata& metadata)
{
    QJsonObject object;
    object.insert(QStringLiteral("bone_name"), metadata.boneName);
    object.insert(QStringLiteral("region_role"), metadata.regionRole);
    object.insert(QStringLiteral("source"), metadata.source);
    object.insert(QStringLiteral("version"), metadata.version);
    return object;
}

AnkleConstraintRegionMetadata constraintRegionMetadataFromJson(const QJsonObject& object)
{
    AnkleConstraintRegionMetadata metadata;
    metadata.boneName = object.value(QStringLiteral("bone_name")).toString();
    metadata.regionRole = object.value(QStringLiteral("region_role")).toString();
    metadata.source = object.value(QStringLiteral("source")).toString();
    metadata.version = object.value(QStringLiteral("version")).toString();
    return metadata;
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

    QJsonObject constraintRegionsObject;
    for (auto it = planning.anatomicalConstraintRegions.cbegin();
         it != planning.anatomicalConstraintRegions.cend();
         ++it) {
        constraintRegionsObject.insert(it.key(), vectorListToJson(it.value()));
    }

    QJsonObject constraintRegionMetadataObject;
    for (auto it = planning.anatomicalConstraintRegionMetadata.cbegin();
         it != planning.anatomicalConstraintRegionMetadata.cend();
         ++it) {
        constraintRegionMetadataObject.insert(it.key(), constraintRegionMetadataToJson(it.value()));
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
    object.insert(QStringLiteral("anatomical_constraint_regions"), constraintRegionsObject);
    object.insert(QStringLiteral("anatomical_constraint_region_metadata"), constraintRegionMetadataObject);
    object.insert(QStringLiteral("recommended_point_order"), orderArray);
    object.insert(QStringLiteral("target_region_center"), vectorToJson(planning.targetRegionCenter));
    object.insert(QStringLiteral("target_region_radius_mm"), planning.targetRegionRadiusMm);
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

    const QJsonObject constraintRegionsObject = object.value(QStringLiteral("anatomical_constraint_regions")).toObject();
    for (auto it = constraintRegionsObject.begin(); it != constraintRegionsObject.end(); ++it) {
        planning.anatomicalConstraintRegions.insert(it.key(), vectorListFromJson(it.value().toArray()));
    }

    const QJsonObject constraintRegionMetadataObject =
        object.value(QStringLiteral("anatomical_constraint_region_metadata")).toObject();
    for (auto it = constraintRegionMetadataObject.begin(); it != constraintRegionMetadataObject.end(); ++it) {
        planning.anatomicalConstraintRegionMetadata.insert(it.key(), constraintRegionMetadataFromJson(it.value().toObject()));
    }

    const QJsonArray orderArray = object.value(QStringLiteral("recommended_point_order")).toArray();
    for (const QJsonValue& value : orderArray) {
        planning.recommendedPointOrder.append(value.toString());
    }

    planning.targetRegionCenter = vectorFromJson(object.value(QStringLiteral("target_region_center")).toObject());
    planning.targetRegionRadiusMm = object.value(QStringLiteral("target_region_radius_mm")).toDouble();
    planning.targetTranslation = vectorFromJson(object.value(QStringLiteral("target_translation")).toObject());
    planning.targetOrientation = quaternionFromJson(object.value(QStringLiteral("target_orientation")).toObject());
    return planning;
}
