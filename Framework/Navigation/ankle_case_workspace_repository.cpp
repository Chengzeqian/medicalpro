#include "Framework/Navigation/ankle_case_workspace_repository.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

AnkleCaseWorkspaceRepository::AnkleCaseWorkspaceRepository(const QString& dataRoot)
    : m_dataRoot(dataRoot)
{
}

bool AnkleCaseWorkspaceRepository::createCaseWorkspace(AnkleCaseManifest& manifest) const
{
    const QString root = caseRoot(manifest.caseId);
    const QStringList stageDirs = {
        QStringLiteral("dicom"),
        QStringLiteral("segmentation"),
        QStringLiteral("models"),
        QStringLiteral("planning"),
        QStringLiteral("registration"),
        QStringLiteral("navigation"),
        QStringLiteral("evaluation")
    };

    if (!ensureDir(root)) {
        return false;
    }

    for (const QString& dirName : stageDirs) {
        if (!ensureDir(stagePath(manifest.caseId, dirName))) {
            return false;
        }
    }

    if (manifest.createdAtIso.isEmpty()) {
        manifest.createdAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }

    manifest.updatedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return saveManifest(manifest);
}

bool AnkleCaseWorkspaceRepository::saveManifest(const AnkleCaseManifest& manifest) const
{
    if (!ensureDir(caseRoot(manifest.caseId))) {
        return false;
    }

    QFile manifestFile(manifestPath(manifest.caseId));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    manifestFile.write(QJsonDocument(toJson(manifest)).toJson(QJsonDocument::Indented));
    return manifestFile.error() == QFile::NoError;
}

AnkleCaseManifest AnkleCaseWorkspaceRepository::loadManifest(const QString& caseId) const
{
    QFile manifestFile(manifestPath(caseId));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    if (!document.isObject()) {
        return {};
    }

    return fromJson(document.object());
}

QString AnkleCaseWorkspaceRepository::caseRoot(const QString& caseId) const
{
    return m_dataRoot + QStringLiteral("/cases/") + caseId;
}

QString AnkleCaseWorkspaceRepository::manifestPath(const QString& caseId) const
{
    return caseRoot(caseId) + QStringLiteral("/case_manifest.json");
}

QString AnkleCaseWorkspaceRepository::stagePath(const QString& caseId, const QString& stageName) const
{
    return caseRoot(caseId) + QStringLiteral("/") + stageName;
}

QJsonObject AnkleCaseWorkspaceRepository::toJson(const AnkleCaseManifest& manifest) const
{
    QJsonArray modelAssets;
    for (const AnkleModelAsset& asset : manifest.modelAssets) {
        QJsonObject assetObject;
        assetObject.insert(QStringLiteral("bone_name"), asset.boneName);
        assetObject.insert(QStringLiteral("source_path"), asset.sourcePath);
        assetObject.insert(QStringLiteral("normalized_path"), asset.normalizedPath);
        assetObject.insert(QStringLiteral("source_type"), asset.sourceType);
        modelAssets.append(assetObject);
    }

    QJsonObject object;
    object.insert(QStringLiteral("case_id"), manifest.caseId);
    object.insert(QStringLiteral("patient_id"), manifest.patientId);
    object.insert(QStringLiteral("patient_name"), manifest.patientName);
    object.insert(QStringLiteral("surgery_id"), manifest.surgeryId);
    object.insert(QStringLiteral("dicom_dir"), manifest.dicomDir);
    object.insert(QStringLiteral("workflow_stage"), manifest.workflowStage);
    object.insert(QStringLiteral("created_at_iso"), manifest.createdAtIso);
    object.insert(QStringLiteral("updated_at_iso"), manifest.updatedAtIso);
    object.insert(QStringLiteral("model_assets"), modelAssets);
    return object;
}

AnkleCaseManifest AnkleCaseWorkspaceRepository::fromJson(const QJsonObject& object) const
{
    AnkleCaseManifest manifest;
    manifest.caseId = object.value(QStringLiteral("case_id")).toString();
    manifest.patientId = object.value(QStringLiteral("patient_id")).toString();
    manifest.patientName = object.value(QStringLiteral("patient_name")).toString();
    manifest.surgeryId = object.value(QStringLiteral("surgery_id")).toString();
    manifest.dicomDir = object.value(QStringLiteral("dicom_dir")).toString();
    manifest.workflowStage = object.value(QStringLiteral("workflow_stage")).toString();
    manifest.createdAtIso = object.value(QStringLiteral("created_at_iso")).toString();
    manifest.updatedAtIso = object.value(QStringLiteral("updated_at_iso")).toString();

    const QJsonArray modelAssets = object.value(QStringLiteral("model_assets")).toArray();
    for (const QJsonValue& value : modelAssets) {
        const QJsonObject assetObject = value.toObject();
        AnkleModelAsset asset;
        asset.boneName = assetObject.value(QStringLiteral("bone_name")).toString();
        asset.sourcePath = assetObject.value(QStringLiteral("source_path")).toString();
        asset.normalizedPath = assetObject.value(QStringLiteral("normalized_path")).toString();
        asset.sourceType = assetObject.value(QStringLiteral("source_type")).toString();
        manifest.modelAssets.append(asset);
    }

    return manifest;
}

bool AnkleCaseWorkspaceRepository::ensureDir(const QString& path) const
{
    QDir dir;
    return dir.exists(path) || dir.mkpath(path);
}
