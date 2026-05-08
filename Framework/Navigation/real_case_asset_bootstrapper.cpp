#include "Framework/Navigation/real_case_asset_bootstrapper.h"

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <vtkOBJReader.h>
#include <vtkPLYReader.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkXMLPolyDataReader.h>

#include <limits>

namespace
{
bool copyModelIntoWorkspace(
    const QString& sourcePath,
    const QString& destinationPath)
{
    if (sourcePath.isEmpty() || destinationPath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        return false;
    }

    QFile::remove(destinationPath);
    return QFile::copy(sourcePath, destinationPath);
}

AnkleModelAsset createModelAsset(
    const QString& boneName,
    const QString& sourcePath,
    const QString& normalizedPath,
    const QString& sourceType)
{
    AnkleModelAsset asset;
    asset.boneName = boneName;
    asset.sourcePath = sourcePath;
    asset.normalizedPath = normalizedPath;
    asset.sourceType = sourceType;
    return asset;
}

vtkSmartPointer<vtkPolyData> loadPolyDataFromPath(const QString& modelPath)
{
    const auto clonePolyData = [](vtkPolyData* source) {
        if (!source) {
            return vtkSmartPointer<vtkPolyData> {};
        }

        vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
        polyData->DeepCopy(source);
        return polyData;
    };

    const QString suffix = QFileInfo(modelPath).suffix().toLower();

    if (suffix == QStringLiteral("stl")) {
        auto reader = vtkSmartPointer<vtkSTLReader>::New();
        reader->SetFileName(modelPath.toUtf8().constData());
        reader->Update();
        return clonePolyData(reader->GetOutput());
    }

    if (suffix == QStringLiteral("obj")) {
        auto reader = vtkSmartPointer<vtkOBJReader>::New();
        reader->SetFileName(modelPath.toUtf8().constData());
        reader->Update();
        return clonePolyData(reader->GetOutput());
    }

    if (suffix == QStringLiteral("ply")) {
        auto reader = vtkSmartPointer<vtkPLYReader>::New();
        reader->SetFileName(modelPath.toUtf8().constData());
        reader->Update();
        return clonePolyData(reader->GetOutput());
    }

    if (suffix == QStringLiteral("vtp")) {
        auto reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
        reader->SetFileName(modelPath.toUtf8().constData());
        reader->Update();
        return clonePolyData(reader->GetOutput());
    }

    return {};
}

QList<QVector3D> sampleMeshPoints(vtkPolyData* polyData, int desiredCount)
{
    if (!polyData || polyData->GetNumberOfPoints() <= 0 || desiredCount <= 0) {
        return {};
    }

    QList<QVector3D> sampledPoints;
    const vtkIdType pointCount = polyData->GetNumberOfPoints();
    const vtkIdType stride = qMax<vtkIdType>(1, pointCount / qMax(1, desiredCount));

    sampledPoints.reserve(qMin(desiredCount, static_cast<int>(pointCount)));
    for (vtkIdType pointIndex = 0;
         pointIndex < pointCount && sampledPoints.size() < desiredCount;
         pointIndex += stride) {
        double point[3] = { 0.0, 0.0, 0.0 };
        polyData->GetPoint(pointIndex, point);
        sampledPoints.append(QVector3D(
            static_cast<float>(point[0]),
            static_cast<float>(point[1]),
            static_cast<float>(point[2])));
    }

    if (sampledPoints.size() < qMin(desiredCount, static_cast<int>(pointCount))) {
        for (vtkIdType pointIndex = pointCount - 1;
             pointIndex >= 0 && sampledPoints.size() < qMin(desiredCount, static_cast<int>(pointCount));
             --pointIndex) {
            double point[3] = { 0.0, 0.0, 0.0 };
            polyData->GetPoint(pointIndex, point);
            const QVector3D candidate(
                static_cast<float>(point[0]),
                static_cast<float>(point[1]),
                static_cast<float>(point[2]));
            if (!sampledPoints.contains(candidate)) {
                sampledPoints.append(candidate);
            }
        }
    }

    return sampledPoints;
}

QVector3D centroidOfPoints(const QList<QVector3D>& points)
{
    if (points.isEmpty()) {
        return {};
    }

    QVector3D centroid;
    for (const QVector3D& point : points) {
        centroid += point;
    }
    return centroid / static_cast<float>(points.size());
}

QList<QVector3D> selectPointsByAxisBand(
    const QList<QVector3D>& points,
    int axis,
    bool selectLowerBand,
    double bandRatio)
{
    if (points.isEmpty()) {
        return {};
    }

    float minValue = std::numeric_limits<float>::max();
    float maxValue = std::numeric_limits<float>::lowest();
    for (const QVector3D& point : points) {
        const float value = axis == 0 ? point.x() : (axis == 1 ? point.y() : point.z());
        minValue = qMin(minValue, value);
        maxValue = qMax(maxValue, value);
    }

    const float span = maxValue - minValue;
    if (span <= 0.0f) {
        return points;
    }

    const float ratio = static_cast<float>(qBound(0.05, bandRatio, 0.95));
    const float threshold = selectLowerBand
        ? (minValue + span * ratio)
        : (maxValue - span * ratio);

    QList<QVector3D> selectedPoints;
    for (const QVector3D& point : points) {
        const float value = axis == 0 ? point.x() : (axis == 1 ? point.y() : point.z());
        if ((selectLowerBand && value <= threshold) || (!selectLowerBand && value >= threshold)) {
            selectedPoints.append(point);
        }
    }

    return selectedPoints.isEmpty() ? points : selectedPoints;
}
}

bool RealCaseAssetBootstrapper::bootstrap(const RealCaseAssetBootstrapRequest& request) const
{
    if (request.dataRoot.isEmpty() ||
        request.caseId.isEmpty() ||
        request.patientId.isEmpty() ||
        request.tibiaModelPath.isEmpty() ||
        request.talusModelPath.isEmpty() ||
        !QFileInfo::exists(request.tibiaModelPath) ||
        !QFileInfo::exists(request.talusModelPath)) {
        return false;
    }

    AnkleCaseWorkspaceRepository repository(request.dataRoot);

    AnkleCaseManifest manifest;
    manifest.caseId = request.caseId;
    manifest.patientId = request.patientId;
    manifest.patientName = request.patientName;
    manifest.surgeryId = request.surgeryId;
    manifest.workflowStage = QStringLiteral("planning");

    if (!repository.createCaseWorkspace(manifest)) {
        return false;
    }

    const QString modelsDir = repository.stagePath(request.caseId, QStringLiteral("models"));
    const QString workspaceTibiaPath = modelsDir + QStringLiteral("/tibia.stl");
    const QString workspaceTalusPath = modelsDir + QStringLiteral("/talus.stl");

    if (!copyModelIntoWorkspace(request.tibiaModelPath, workspaceTibiaPath) ||
        !copyModelIntoWorkspace(request.talusModelPath, workspaceTalusPath)) {
        return false;
    }

    manifest.modelAssets = {
        createModelAsset(
            QStringLiteral("tibia"),
            workspaceTibiaPath,
            QStringLiteral("models/tibia.stl"),
            QStringLiteral("stl")),
        createModelAsset(
            QStringLiteral("talus"),
            workspaceTalusPath,
            QStringLiteral("models/talus.stl"),
            QStringLiteral("stl"))
    };

    if (!repository.saveManifest(manifest)) {
        return false;
    }

    AnkleCaseAssetBindings bindings;
    bindings.caseId = request.caseId;
    bindings.boundBoneAssetIds = QStringList { QStringLiteral("bone:tibia"), QStringLiteral("bone:talus") };
    bindings.activeBoneAssetIds = bindings.boundBoneAssetIds;
    bindings.boundInstrumentAssetIds = request.defaultInstrumentAssetIds;
    bindings.activeInstrumentAssetIds = request.defaultInstrumentAssetIds;
    bindings.instrumentGeometryBindings = request.defaultInstrumentGeometryBindings;
    bindings.createdAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    bindings.updatedAtIso = bindings.createdAtIso;

    if (!repository.saveCaseAssetBindings(bindings)) {
        return false;
    }

    AnklePlanningService planningService(repository);
    AnklePlanningData planning = planningService.createDefaultPlanning(request.caseId);
    planning.primaryBones = QStringList { QStringLiteral("tibia"), QStringLiteral("talus") };

    const QList<QVector3D> tibiaPoints = sampleMeshPoints(loadPolyDataFromPath(workspaceTibiaPath), 12);
    const QList<QVector3D> talusPoints = sampleMeshPoints(loadPolyDataFromPath(workspaceTalusPath), 12);
    if (tibiaPoints.size() < 3 || talusPoints.size() < 3) {
        return false;
    }

    const QVector3D tibiaCenter = centroidOfPoints(tibiaPoints);
    const QVector3D talusCenter = centroidOfPoints(talusPoints);
    const QList<QVector3D> tibiaDistalRegion = selectPointsByAxisBand(tibiaPoints, 2, true, 0.45);
    const QList<QVector3D> talusDomeRegion = selectPointsByAxisBand(talusPoints, 2, false, 0.45);
    const QVector3D targetRegionCenter = request.targetRegionCenter.isNull()
        ? talusCenter
        : request.targetRegionCenter;

    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), tibiaCenter);
    planning.referenceLandmarks.insert(QStringLiteral("talus_center"), talusCenter);
    planning.anatomicalConstraintRegions.insert(QStringLiteral("tibia_distal_region"), tibiaDistalRegion);
    planning.anatomicalConstraintRegions.insert(QStringLiteral("talus_dome_region"), talusDomeRegion);
    planning.anatomicalConstraintRegionMetadata.insert(
        QStringLiteral("tibia_distal_region"),
        AnkleConstraintRegionMetadata {
            QStringLiteral("tibia"),
            QStringLiteral("distal_region"),
            QStringLiteral("real_case_asset_bootstrapper"),
            QStringLiteral("1.0")
        });
    planning.anatomicalConstraintRegionMetadata.insert(
        QStringLiteral("talus_dome_region"),
        AnkleConstraintRegionMetadata {
            QStringLiteral("talus"),
            QStringLiteral("dome_region"),
            QStringLiteral("real_case_asset_bootstrapper"),
            QStringLiteral("1.0")
        });
    planning.targetRegionCenter = targetRegionCenter;
    planning.targetRegionRadiusMm = request.targetRegionRadiusMm > 0.0
        ? request.targetRegionRadiusMm
        : 15.0;

    return planningService.savePlanning(request.caseId, planning);
}
