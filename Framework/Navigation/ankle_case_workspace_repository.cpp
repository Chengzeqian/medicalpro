#include "Framework/Navigation/ankle_case_workspace_repository.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace
{
QStringList toStringList(const QJsonArray& array)
{
    QStringList values;
    values.reserve(array.size());
    for (const QJsonValue& value : array) {
        values.append(value.toString());
    }
    return values;
}

QJsonObject geometryBindingToJson(const AnkleInstrumentGeometryBinding& binding)
{
    QJsonObject object;
    object.insert(QStringLiteral("instrument_asset_id"), binding.instrumentAssetId);
    object.insert(QStringLiteral("geometry_asset_id"), binding.geometryAssetId);
    object.insert(QStringLiteral("geometry_file_path"), binding.geometryFilePath);
    return object;
}

AnkleInstrumentGeometryBinding geometryBindingFromJson(const QJsonObject& object)
{
    AnkleInstrumentGeometryBinding binding;
    binding.instrumentAssetId = object.value(QStringLiteral("instrument_asset_id")).toString();
    binding.geometryAssetId = object.value(QStringLiteral("geometry_asset_id")).toString();
    binding.geometryFilePath = object.value(QStringLiteral("geometry_file_path")).toString();
    return binding;
}
}

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

bool AnkleCaseWorkspaceRepository::saveCaseAssetBindings(const AnkleCaseAssetBindings& bindings) const
{
    if (bindings.caseId.isEmpty() || !ensureDir(caseRoot(bindings.caseId))) {
        return false;
    }

    QJsonArray geometryBindings;
    for (const AnkleInstrumentGeometryBinding& binding : bindings.instrumentGeometryBindings) {
        geometryBindings.append(geometryBindingToJson(binding));
    }

    const QString createdAtIso = bindings.createdAtIso.isEmpty()
        ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
        : bindings.createdAtIso;
    const QString updatedAtIso = bindings.updatedAtIso.isEmpty()
        ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
        : bindings.updatedAtIso;

    QJsonObject root;
    root.insert(QStringLiteral("case_id"), bindings.caseId);
    root.insert(QStringLiteral("bound_bone_asset_ids"), QJsonArray::fromStringList(bindings.boundBoneAssetIds));
    root.insert(QStringLiteral("active_bone_asset_ids"), QJsonArray::fromStringList(bindings.activeBoneAssetIds));
    root.insert(QStringLiteral("bound_instrument_asset_ids"), QJsonArray::fromStringList(bindings.boundInstrumentAssetIds));
    root.insert(QStringLiteral("active_instrument_asset_ids"), QJsonArray::fromStringList(bindings.activeInstrumentAssetIds));
    root.insert(QStringLiteral("instrument_geometry_bindings"), geometryBindings);
    root.insert(QStringLiteral("created_at_iso"), createdAtIso);
    root.insert(QStringLiteral("updated_at_iso"), updatedAtIso);

    QFile file(caseAssetBindingsPath(bindings.caseId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.error() == QFile::NoError;
}

AnkleCaseAssetBindings AnkleCaseWorkspaceRepository::loadCaseAssetBindings(const QString& caseId) const
{
    QFile file(caseAssetBindingsPath(caseId));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    AnkleCaseAssetBindings bindings;
    bindings.caseId = root.value(QStringLiteral("case_id")).toString();
    bindings.boundBoneAssetIds = toStringList(root.value(QStringLiteral("bound_bone_asset_ids")).toArray());
    bindings.activeBoneAssetIds = toStringList(root.value(QStringLiteral("active_bone_asset_ids")).toArray());
    bindings.boundInstrumentAssetIds = toStringList(root.value(QStringLiteral("bound_instrument_asset_ids")).toArray());
    bindings.activeInstrumentAssetIds = toStringList(root.value(QStringLiteral("active_instrument_asset_ids")).toArray());
    bindings.createdAtIso = root.value(QStringLiteral("created_at_iso")).toString();
    bindings.updatedAtIso = root.value(QStringLiteral("updated_at_iso")).toString();

    const QJsonArray geometryBindings = root.value(QStringLiteral("instrument_geometry_bindings")).toArray();
    bindings.instrumentGeometryBindings.reserve(geometryBindings.size());
    for (const QJsonValue& value : geometryBindings) {
        bindings.instrumentGeometryBindings.append(geometryBindingFromJson(value.toObject()));
    }

    return bindings;
}

QString AnkleCaseWorkspaceRepository::caseRoot(const QString& caseId) const
{
    return m_dataRoot + QStringLiteral("/cases/") + caseId;
}

QString AnkleCaseWorkspaceRepository::manifestPath(const QString& caseId) const
{
    return caseRoot(caseId) + QStringLiteral("/case_manifest.json");
}

QString AnkleCaseWorkspaceRepository::caseAssetBindingsPath(const QString& caseId) const
{
    return caseRoot(caseId) + QStringLiteral("/case_asset_bindings.json");
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
