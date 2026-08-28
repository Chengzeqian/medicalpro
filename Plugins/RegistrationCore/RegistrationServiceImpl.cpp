#include "RegistrationServiceImpl.h"
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QPair>
#include <QVector3D>
#include <cmath>
#include <limits>
#include <unordered_map>

#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkLandmarkTransform.h>
#include <vtkIterativeClosestPointTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkMath.h>
#include <vtkPointData.h>
#include <vtkFloatArray.h>
#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkTriangle.h>

#include "Framework/Platform/Kernel/platform_service_registry.h"

// MeshGPU DLL header (CUDA-free, pure C++ interface)
#include "algorithms/meshgpu/include/mesh_gpu_runtime_api.h"
#include "algorithms/meshgpu/include/mesh_gpu_interface.h"

// Registration2D3D 插件头文件
#include "../Registration2D3D/Registration2D3DService.h"
#include "../Registration2D3D/Registration2D3DDataStructures.h"

namespace
{
struct LegacyMeshGpuRuntimeApiVTable
{
    using LoadTargetMeshFn = bool (*)(mesh_gpu::MeshGPUInterface*, const std::string&, float);
    using HasTargetMeshFn = bool (*)(const mesh_gpu::MeshGPUInterface*);
    using SetTargetMeshFn = bool (*)(
        mesh_gpu::MeshGPUInterface*,
        const std::vector<mesh_gpu::Point3D>&,
        const std::vector<mesh_gpu::Normal3D>&,
        const std::vector<std::array<int, 3>>&,
        float);
    using SetSourcePointCloudFn = bool (*)(
        mesh_gpu::MeshGPUInterface*,
        const std::vector<mesh_gpu::Point3D>&);
    using RunRegistrationFn = mesh_gpu::RegistrationResult (*)(
        mesh_gpu::MeshGPUInterface*,
        const mesh_gpu::RegistrationParams&);
    using RunRegistrationWithRotationSearchFn = mesh_gpu::RegistrationResult (*)(
        mesh_gpu::MeshGPUInterface*,
        const mesh_gpu::RotationSearchParams&,
        const mesh_gpu::RegistrationParams&);
    using ScoreTransformCandidatesFn = std::vector<mesh_gpu::TransformCandidateScore> (*)(
        mesh_gpu::MeshGPUInterface*,
        const std::vector<mesh_gpu::Transform4x4>&,
        float);

    LoadTargetMeshFn loadTargetMesh = nullptr;
    HasTargetMeshFn hasTargetMesh = nullptr;
    SetTargetMeshFn setTargetMesh = nullptr;
    SetSourcePointCloudFn setSourcePointCloud = nullptr;
    RunRegistrationFn runRegistration = nullptr;
    RunRegistrationWithRotationSearchFn runRegistrationWithRotationSearch = nullptr;
    ScoreTransformCandidatesFn scoreTransformCandidates = nullptr;

    bool isComplete() const
    {
        return loadTargetMesh
            && hasTargetMesh
            && setTargetMesh
            && setSourcePointCloud
            && runRegistration
            && runRegistrationWithRotationSearch
            && scoreTransformCandidates;
    }
};

class LegacyMeshGpuRuntimeApiAdapter final : public mesh_gpu::MeshGPURuntimeApi
{
public:
    LegacyMeshGpuRuntimeApiAdapter(mesh_gpu::MeshGPUInterface* legacyApi,
                                   void (*destroyFn)(mesh_gpu::MeshGPUInterface*),
                                   const LegacyMeshGpuRuntimeApiVTable& vtable)
        : m_legacyApi(legacyApi)
        , m_destroyFn(destroyFn)
        , m_vtable(vtable)
    {
    }

    ~LegacyMeshGpuRuntimeApiAdapter() override
    {
        if (m_legacyApi && m_destroyFn) {
            m_destroyFn(m_legacyApi);
            m_legacyApi = nullptr;
        }
    }

    bool loadTargetMesh(const std::string& meshPath, float cellSize = 1.0f) override
    {
        m_cachedTargetVertices.clear();
        m_cachedTargetNormals.clear();
        m_cachedTargetTriangles.clear();
        return m_legacyApi && m_vtable.loadTargetMesh && m_vtable.loadTargetMesh(m_legacyApi, meshPath, cellSize);
    }

    bool hasTargetMesh() const override
    {
        return m_legacyApi && m_vtable.hasTargetMesh && m_vtable.hasTargetMesh(m_legacyApi);
    }

    bool setTargetMesh(const std::vector<mesh_gpu::Point3D>& vertices,
                       const std::vector<mesh_gpu::Normal3D>& normals,
                       const std::vector<std::array<int, 3>>& triangles,
                       float cellSize = 1.0f) override
    {
        m_cachedTargetVertices = vertices;
        m_cachedTargetNormals = normals;
        m_cachedTargetTriangles = triangles;
        return m_legacyApi
            && m_vtable.setTargetMesh
            && m_vtable.setTargetMesh(m_legacyApi, vertices, normals, triangles, cellSize);
    }

    bool setSourcePointCloud(const std::vector<mesh_gpu::Point3D>& points) override
    {
        return m_legacyApi
            && m_vtable.setSourcePointCloud
            && m_vtable.setSourcePointCloud(m_legacyApi, points);
    }

    mesh_gpu::RuntimeRegistrationResult runRegistration(
        const mesh_gpu::RegistrationParams& params) override
    {
        return toRuntimeResult(m_vtable.runRegistration(m_legacyApi, params));
    }

    mesh_gpu::RuntimeRegistrationResult runRegistrationWithRotationSearch(
        const mesh_gpu::RotationSearchParams& rotationParams,
        const mesh_gpu::RegistrationParams& params) override
    {
        return toRuntimeResult(
            m_vtable.runRegistrationWithRotationSearch(m_legacyApi, rotationParams, params));
    }

    std::vector<mesh_gpu::RuntimeTransformCandidateScore> scoreTransformCandidates(
        const std::vector<mesh_gpu::Transform4x4>& candidates,
        float cutoffMm = 12.0f) override
    {
        const std::vector<mesh_gpu::TransformCandidateScore> legacyScores =
            m_vtable.scoreTransformCandidates(m_legacyApi, candidates, cutoffMm);

        std::vector<mesh_gpu::RuntimeTransformCandidateScore> runtimeScores;
        runtimeScores.reserve(legacyScores.size());
        for (const mesh_gpu::TransformCandidateScore& score : legacyScores) {
            mesh_gpu::RuntimeTransformCandidateScore runtimeScore;
            runtimeScore.candidateIndex = score.candidate_index;
            runtimeScore.score = score.score;
            runtimeScore.meanDistanceMm = score.mean_dist_mm;
            runtimeScore.normalConsistencyScore = 0.0f;
            runtimeScore.curvatureScore = 0.0f;
            runtimeScore.geometryScoreAvailable = false;
            runtimeScore.success = score.success;
            runtimeScores.push_back(runtimeScore);
        }
        return runtimeScores;
    }

    std::vector<mesh_gpu::RuntimeRefineCandidateResult> refineTransformCandidates(
        const std::vector<mesh_gpu::RuntimeRefineCandidateRequest>& candidates,
        const mesh_gpu::RegistrationParams& params) override
    {
        Q_UNUSED(candidates);
        Q_UNUSED(params);
        qWarning() << "[RegistrationService] Legacy MeshGPU runtime does not support batch refine";
        return {};
    }

    mesh_gpu::RuntimeConstraintFilterResult filterSourcePointsByConstraints(
        const std::vector<mesh_gpu::Point3D>& points,
        const mesh_gpu::Point3D& targetRegionCenter,
        float targetRegionRadiusMm,
        float membershipRadiusMm,
        const std::vector<mesh_gpu::Point3D>& constraintPoints,
        int minimumPointCount) override
    {
        mesh_gpu::RuntimeConstraintFilterResult result;
        std::vector<std::pair<double, int> > rankedIndices;
        rankedIndices.reserve(points.size());

        const double radiusSquared =
            targetRegionRadiusMm > 0.0f ? static_cast<double>(targetRegionRadiusMm * targetRegionRadiusMm) : -1.0;
        const double membershipSquared =
            membershipRadiusMm > 0.0f ? static_cast<double>(membershipRadiusMm * membershipRadiusMm) : -1.0;

        for (int index = 0; index < static_cast<int>(points.size()); ++index) {
            const mesh_gpu::Point3D& point = points[static_cast<size_t>(index)];
            const double dx = static_cast<double>(point.x - targetRegionCenter.x);
            const double dy = static_cast<double>(point.y - targetRegionCenter.y);
            const double dz = static_cast<double>(point.z - targetRegionCenter.z);
            const double distanceSquared = dx * dx + dy * dy + dz * dz;
            rankedIndices.push_back(std::make_pair(distanceSquared, index));

            bool matched = radiusSquared > 0.0 && distanceSquared <= radiusSquared;
            if (!matched && membershipSquared > 0.0) {
                for (size_t constraintIndex = 0; constraintIndex < constraintPoints.size(); ++constraintIndex) {
                    const mesh_gpu::Point3D& constraintPoint = constraintPoints[constraintIndex];
                    const double cdx = static_cast<double>(point.x - constraintPoint.x);
                    const double cdy = static_cast<double>(point.y - constraintPoint.y);
                    const double cdz = static_cast<double>(point.z - constraintPoint.z);
                    if (cdx * cdx + cdy * cdy + cdz * cdz <= membershipSquared) {
                        matched = true;
                        break;
                    }
                }
            }

            if (matched) {
                result.selectedIndices.push_back(index);
            }
        }

        if (static_cast<int>(result.selectedIndices.size()) < minimumPointCount) {
            std::sort(
                rankedIndices.begin(),
                rankedIndices.end(),
                [](const std::pair<double, int>& left, const std::pair<double, int>& right) {
                    return left.first < right.first;
                });

            for (size_t rankedIndex = 0; rankedIndex < rankedIndices.size(); ++rankedIndex) {
                const int candidateIndex = rankedIndices[rankedIndex].second;
                if (std::find(
                        result.selectedIndices.begin(),
                        result.selectedIndices.end(),
                        candidateIndex) == result.selectedIndices.end()) {
                    result.selectedIndices.push_back(candidateIndex);
                }
                if (static_cast<int>(result.selectedIndices.size()) >= minimumPointCount) {
                    break;
                }
            }
        }

        result.success = static_cast<int>(result.selectedIndices.size()) >= minimumPointCount;
        return result;
    }

    mesh_gpu::RuntimeConstraintFilterResult filterTargetPointsByConstraints(
        const mesh_gpu::Point3D& targetRegionCenter,
        float targetRegionRadiusMm,
        float membershipRadiusMm,
        const std::vector<mesh_gpu::Point3D>& constraintPoints,
        int minimumPointCount) override
    {
        if (m_cachedTargetVertices.empty()) {
            return mesh_gpu::RuntimeConstraintFilterResult {};
        }

        return filterSourcePointsByConstraints(
            m_cachedTargetVertices,
            targetRegionCenter,
            targetRegionRadiusMm,
            membershipRadiusMm,
            constraintPoints,
            minimumPointCount);
    }

    mesh_gpu::ConstrainedMeshResult buildConstrainedTargetMesh(
        const mesh_gpu::Point3D& targetRegionCenter,
        float targetRegionRadiusMm,
        float membershipRadiusMm,
        const std::vector<mesh_gpu::Point3D>& constraintPoints,
        int minimumPointCount) override
    {
        mesh_gpu::ConstrainedMeshResult result;
        const mesh_gpu::RuntimeConstraintFilterResult pointFilterResult = filterTargetPointsByConstraints(
            targetRegionCenter,
            targetRegionRadiusMm,
            membershipRadiusMm,
            constraintPoints,
            minimumPointCount);
        if (!pointFilterResult.success || pointFilterResult.selectedIndices.empty()) {
            return result;
        }

        std::vector<int> vertexMapping(m_cachedTargetVertices.size(), -1);
        result.vertices.reserve(pointFilterResult.selectedIndices.size());
        result.normals.reserve(pointFilterResult.selectedIndices.size());
        result.original_vertex_indices.reserve(pointFilterResult.selectedIndices.size());

        for (size_t selectedIndexOffset = 0;
             selectedIndexOffset < pointFilterResult.selectedIndices.size();
             ++selectedIndexOffset) {
            const int selectedIndex = pointFilterResult.selectedIndices[selectedIndexOffset];
            if (selectedIndex < 0
                || selectedIndex >= static_cast<int>(m_cachedTargetVertices.size())
                || vertexMapping[static_cast<size_t>(selectedIndex)] >= 0) {
                continue;
            }

            vertexMapping[static_cast<size_t>(selectedIndex)] = static_cast<int>(result.vertices.size());
            result.vertices.push_back(m_cachedTargetVertices[static_cast<size_t>(selectedIndex)]);
            result.normals.push_back(
                static_cast<size_t>(selectedIndex) < m_cachedTargetNormals.size()
                    ? m_cachedTargetNormals[static_cast<size_t>(selectedIndex)]
                    : mesh_gpu::Normal3D {});
            result.original_vertex_indices.push_back(selectedIndex);
        }

        for (size_t triangleIndex = 0; triangleIndex < m_cachedTargetTriangles.size(); ++triangleIndex) {
            const std::array<int, 3>& triangle = m_cachedTargetTriangles[triangleIndex];
            if (triangle[0] < 0 || triangle[1] < 0 || triangle[2] < 0) {
                continue;
            }
            if (triangle[0] >= static_cast<int>(vertexMapping.size())
                || triangle[1] >= static_cast<int>(vertexMapping.size())
                || triangle[2] >= static_cast<int>(vertexMapping.size())) {
                continue;
            }

            const int mapped0 = vertexMapping[static_cast<size_t>(triangle[0])];
            const int mapped1 = vertexMapping[static_cast<size_t>(triangle[1])];
            const int mapped2 = vertexMapping[static_cast<size_t>(triangle[2])];
            if (mapped0 < 0 || mapped1 < 0 || mapped2 < 0) {
                continue;
            }

            result.triangles.push_back({ mapped0, mapped1, mapped2 });
        }

        result.success =
            static_cast<int>(result.vertices.size()) >= minimumPointCount && !result.triangles.empty();
        return result;
    }

private:
    static mesh_gpu::RuntimeRegistrationResult toRuntimeResult(const mesh_gpu::RegistrationResult& result)
    {
        mesh_gpu::RuntimeRegistrationResult runtimeResult;
        runtimeResult.transform = result.transform;
        runtimeResult.rmse = result.rmse;
        runtimeResult.iterations = result.iterations;
        runtimeResult.converged = result.converged;
        return runtimeResult;
    }

    mesh_gpu::MeshGPUInterface* m_legacyApi = nullptr;
    void (*m_destroyFn)(mesh_gpu::MeshGPUInterface*) = nullptr;
    LegacyMeshGpuRuntimeApiVTable m_vtable;
    std::vector<mesh_gpu::Point3D> m_cachedTargetVertices;
    std::vector<mesh_gpu::Normal3D> m_cachedTargetNormals;
    std::vector<std::array<int, 3>> m_cachedTargetTriangles;
};

double squaredDistance(const QVector3D& left, const QVector3D& right)
{
    const QVector3D delta = left - right;
    return static_cast<double>(QVector3D::dotProduct(delta, delta));
}

QVector3D centroidOfPoints(const QList<QVector3D>& points)
{
    if (points.isEmpty()) {
        return QVector3D();
    }

    QVector3D sum;
    for (const QVector3D& point : points) {
        sum += point;
    }
    return sum / static_cast<float>(points.size());
}

double constraintMembershipRadiusMm(double targetRegionRadiusMm)
{
    if (targetRegionRadiusMm > 0.0) {
        return qBound(3.0, targetRegionRadiusMm * 0.30, 8.0);
    }
    return 5.0;
}

bool pointMatchesConstraintSet(
    const QVector3D& point,
    const QList<QVector3D>& constraintPoints,
    const QVector3D& regionCenter,
    double regionRadiusMm,
    double membershipRadiusMm)
{
    if (regionRadiusMm > 0.0 && squaredDistance(point, regionCenter) <= regionRadiusMm * regionRadiusMm) {
        return true;
    }

    if (constraintPoints.isEmpty() || membershipRadiusMm <= 0.0) {
        return false;
    }

    const double thresholdSquared = membershipRadiusMm * membershipRadiusMm;
    for (const QVector3D& constraintPoint : constraintPoints) {
        if (squaredDistance(point, constraintPoint) <= thresholdSquared) {
            return true;
        }
    }
    return false;
}

QList<QVector3D> vectorListFromVariant(const QVariantList& list)
{
    QList<QVector3D> points;
    points.reserve(list.size());
    for (const QVariant& value : list) {
        QVariantList point = value.toList();
        if (point.isEmpty() && value.canConvert<QVector3D>()) {
            const QVector3D vector = value.value<QVector3D>();
            points.append(vector);
            continue;
        }
        if (point.size() < 3) {
            continue;
        }
        points.append(QVector3D(
            point[0].toFloat(),
            point[1].toFloat(),
            point[2].toFloat()));
    }
    return points;
}

QList<QVector3D> vectorListFromFlatVariant(const QVariantList& list)
{
    QList<QVector3D> points;
    if (list.size() < 3 || list.size() % 3 != 0) {
        return points;
    }

    points.reserve(list.size() / 3);
    for (int index = 0; index + 2 < list.size(); index += 3) {
        points.append(QVector3D(
            list.at(index).toFloat(),
            list.at(index + 1).toFloat(),
            list.at(index + 2).toFloat()));
    }
    return points;
}

QList<double> matrixListFromVariant(const QVariant& value)
{
    QList<double> matrixValues;
    const QVariantList matrixList = value.toList();
    matrixValues.reserve(matrixList.size());
    for (const QVariant& matrixValue : matrixList) {
        matrixValues.append(matrixValue.toDouble());
    }
    return matrixValues;
}

vtkSmartPointer<vtkMatrix4x4> matrixFromVariant(const QVariant& value)
{
    const QList<double> matrixValues = matrixListFromVariant(value);
    if (matrixValues.size() != 16) {
        return nullptr;
    }

    vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
    int valueIndex = 0;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            matrix->SetElement(row, column, matrixValues.at(valueIndex++));
        }
    }
    return matrix;
}

QList<QVector3D> transformVectorList(vtkMatrix4x4* transform, const QList<QVector3D>& points)
{
    QList<QVector3D> transformedPoints;
    transformedPoints.reserve(points.size());
    if (transform == nullptr) {
        return transformedPoints;
    }

    for (const QVector3D& point : points) {
        double inputPoint[4] = { point.x(), point.y(), point.z(), 1.0 };
        double outputPoint[4];
        transform->MultiplyPoint(inputPoint, outputPoint);
        transformedPoints.append(QVector3D(
            static_cast<float>(outputPoint[0]),
            static_cast<float>(outputPoint[1]),
            static_cast<float>(outputPoint[2])));
    }
    return transformedPoints;
}

double pairedResidualRmseMm(
    vtkMatrix4x4* transform,
    const QList<QVector3D>& sourcePoints,
    const QList<QVector3D>& targetPoints)
{
    if (transform == nullptr || sourcePoints.size() != targetPoints.size() || sourcePoints.isEmpty()) {
        return -1.0;
    }

    double squaredSum = 0.0;
    for (int index = 0; index < sourcePoints.size(); ++index) {
        double inputPoint[4] = {
            sourcePoints.at(index).x(),
            sourcePoints.at(index).y(),
            sourcePoints.at(index).z(),
            1.0
        };
        double outputPoint[4];
        transform->MultiplyPoint(inputPoint, outputPoint);
        const QVector3D transformedPoint(
            static_cast<float>(outputPoint[0]),
            static_cast<float>(outputPoint[1]),
            static_cast<float>(outputPoint[2]));
        squaredSum += squaredDistance(transformedPoint, targetPoints.at(index));
    }

    return std::sqrt(squaredSum / static_cast<double>(sourcePoints.size()));
}

bool applyPairedResidualGuard(
    vtkSmartPointer<vtkMatrix4x4>& finalMatrix,
    vtkMatrix4x4* fallbackMatrix,
    const QVariantMap& parameters,
    QVariantMap& metadata)
{
    const QVariant pairedSourceVariant =
        parameters.value(QStringLiteral("pairedResidualSourcePoints"));
    const QVariant pairedTargetVariant =
        parameters.value(QStringLiteral("pairedResidualTargetPoints"));
    QList<QVector3D> pairedSourcePoints =
        vectorListFromVariant(pairedSourceVariant.toList());
    QList<QVector3D> pairedTargetPoints =
        vectorListFromVariant(pairedTargetVariant.toList());
    if (pairedSourcePoints.isEmpty()) {
        pairedSourcePoints = vectorListFromFlatVariant(
            parameters.value(QStringLiteral("pairedResidualSourcePointsFlat")).toList());
    }
    if (pairedTargetPoints.isEmpty()) {
        pairedTargetPoints = vectorListFromFlatVariant(
            parameters.value(QStringLiteral("pairedResidualTargetPointsFlat")).toList());
    }
    const bool guardRequested =
        parameters.value(QStringLiteral("enablePairedResidualGuard"), true).toBool()
        && pairedSourcePoints.size() >= 3
        && pairedSourcePoints.size() == pairedTargetPoints.size();

    metadata.insert(QStringLiteral("pairedResidualGuardSourceVariantValid"), pairedSourceVariant.isValid());
    metadata.insert(QStringLiteral("pairedResidualGuardTargetVariantValid"), pairedTargetVariant.isValid());
    metadata.insert(
        QStringLiteral("pairedResidualGuardSourceVariantType"),
        QString::fromLatin1(pairedSourceVariant.typeName()));
    metadata.insert(
        QStringLiteral("pairedResidualGuardTargetVariantType"),
        QString::fromLatin1(pairedTargetVariant.typeName()));
    metadata.insert(QStringLiteral("pairedResidualGuardRequested"), guardRequested);
    metadata.insert(QStringLiteral("pairedResidualGuardPointCount"), pairedSourcePoints.size());
    metadata.insert(QStringLiteral("pairedResidualGuardTargetPointCount"), pairedTargetPoints.size());
    if (!guardRequested) {
        return false;
    }

    vtkSmartPointer<vtkMatrix4x4> parameterFallbackMatrix =
        matrixFromVariant(parameters.value(QStringLiteral("pairedResidualGuardFallbackTransform")));
    vtkMatrix4x4* guardFallbackMatrix =
        parameterFallbackMatrix ? parameterFallbackMatrix : fallbackMatrix;

    const double initialResidualMm =
        pairedResidualRmseMm(guardFallbackMatrix, pairedSourcePoints, pairedTargetPoints);
    const double finalResidualMm =
        pairedResidualRmseMm(finalMatrix, pairedSourcePoints, pairedTargetPoints);
    const double toleranceMm =
        parameters.value(QStringLiteral("pairedResidualGuardToleranceMm"), 1e-4).toDouble();

    metadata.insert(
        QStringLiteral("pairedResidualGuardFallbackTransformOverrideUsed"),
        parameterFallbackMatrix != nullptr);
    metadata.insert(QStringLiteral("pairedResidualGuardInitialMm"), initialResidualMm);
    metadata.insert(QStringLiteral("pairedResidualGuardFinalMm"), finalResidualMm);
    metadata.insert(QStringLiteral("pairedResidualGuardToleranceMm"), toleranceMm);

    if (guardFallbackMatrix == nullptr || initialResidualMm < 0.0 || finalResidualMm < 0.0) {
        metadata.insert(QStringLiteral("pairedResidualGuardApplied"), false);
        metadata.insert(QStringLiteral("pairedResidualGuardReason"), QStringLiteral("insufficient_guard_transform"));
        return false;
    }

    if (finalResidualMm <= initialResidualMm + toleranceMm) {
        metadata.insert(QStringLiteral("pairedResidualGuardApplied"), false);
        metadata.insert(QStringLiteral("pairedResidualGuardReason"), QStringLiteral("final_paired_residual_accepted"));
        return false;
    }

    finalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    finalMatrix->DeepCopy(guardFallbackMatrix);
    metadata.insert(QStringLiteral("pairedResidualGuardApplied"), true);
    metadata.insert(QStringLiteral("pairedResidualGuardReason"), QStringLiteral("final_paired_residual_regressed"));
    metadata.insert(QStringLiteral("pairedResidualGuardRejectedFinalMm"), finalResidualMm);
    metadata.insert(QStringLiteral("pairedResidualGuardAcceptedFinalMm"), initialResidualMm);
    return true;
}

QMap<QString, QList<QVector3D>> constraintRegionsFromVariant(const QVariantMap& map)
{
    QMap<QString, QList<QVector3D>> regions;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        regions.insert(it.key(), vectorListFromVariant(it.value().toList()));
    }
    return regions;
}

QList<QVector3D> flattenConstraintRegions(const QMap<QString, QList<QVector3D>>& regions)
{
    QList<QVector3D> points;
    for (auto it = regions.cbegin(); it != regions.cend(); ++it) {
        points.append(it.value());
    }
    return points;
}

QVector3D meshGpuPointToVector3D(const mesh_gpu::Point3D& point)
{
    return QVector3D(point.x, point.y, point.z);
}

QList<QVector3D> meshGpuPointListToVectorList(const std::vector<mesh_gpu::Point3D>& points)
{
    QList<QVector3D> result;
    result.reserve(static_cast<int>(points.size()));
    for (const mesh_gpu::Point3D& point : points) {
        result.append(meshGpuPointToVector3D(point));
    }
    return result;
}

double truncatedNearestVertexRmseMm(
    const std::vector<mesh_gpu::Point3D>& sourcePoints,
    const std::vector<mesh_gpu::Point3D>& targetVertices,
    double cutoffMm,
    double cellSizeMm)
{
    if (sourcePoints.empty() || targetVertices.empty()) {
        return std::numeric_limits<double>::max();
    }

    const double effectiveCellSizeMm = cellSizeMm > 0.0 ? cellSizeMm : 2.5;
    const double cutoffSquared =
        cutoffMm > 0.0
            ? cutoffMm * cutoffMm
            : std::numeric_limits<double>::max();
    const auto cellCoord = [effectiveCellSizeMm](float value) {
        return static_cast<int>(std::floor(static_cast<double>(value) / effectiveCellSizeMm));
    };
    const auto cellKey = [](int x, int y, int z) {
        constexpr long long bias = 1LL << 20;
        return ((static_cast<long long>(x) + bias) << 42)
            | ((static_cast<long long>(y) + bias) << 21)
            | (static_cast<long long>(z) + bias);
    };

    std::unordered_map<long long, std::vector<int>> targetGrid;
    targetGrid.reserve(targetVertices.size());
    for (int targetIndex = 0; targetIndex < static_cast<int>(targetVertices.size()); ++targetIndex) {
        const mesh_gpu::Point3D& targetPoint = targetVertices[static_cast<size_t>(targetIndex)];
        targetGrid[cellKey(
            cellCoord(targetPoint.x),
            cellCoord(targetPoint.y),
            cellCoord(targetPoint.z))].push_back(targetIndex);
    }

    double sumSquared = 0.0;
    for (const mesh_gpu::Point3D& sourcePoint : sourcePoints) {
        double bestSquared = std::numeric_limits<double>::max();
        const int sourceCellX = cellCoord(sourcePoint.x);
        const int sourceCellY = cellCoord(sourcePoint.y);
        const int sourceCellZ = cellCoord(sourcePoint.z);
        for (int dzCell = -2; dzCell <= 2; ++dzCell) {
            for (int dyCell = -2; dyCell <= 2; ++dyCell) {
                for (int dxCell = -2; dxCell <= 2; ++dxCell) {
                    const auto cellIt = targetGrid.find(cellKey(
                        sourceCellX + dxCell,
                        sourceCellY + dyCell,
                        sourceCellZ + dzCell));
                    if (cellIt == targetGrid.end()) {
                        continue;
                    }
                    for (int targetIndex : cellIt->second) {
                        const mesh_gpu::Point3D& targetPoint =
                            targetVertices[static_cast<size_t>(targetIndex)];
                        const double dx = static_cast<double>(sourcePoint.x - targetPoint.x);
                        const double dy = static_cast<double>(sourcePoint.y - targetPoint.y);
                        const double dz = static_cast<double>(sourcePoint.z - targetPoint.z);
                        const double distanceSquared = dx * dx + dy * dy + dz * dz;
                        if (distanceSquared < bestSquared) {
                            bestSquared = distanceSquared;
                        }
                    }
                }
            }
        }
        if (bestSquared == std::numeric_limits<double>::max()) {
            bestSquared = cutoffSquared;
        }
        sumSquared += std::min(bestSquared, cutoffSquared);
    }

    return std::sqrt(sumSquared / static_cast<double>(sourcePoints.size()));
}

struct CandidateRegionMetrics
{
    double hitRatio = 0.0;
    double coverageScore = 0.0;
};

CandidateRegionMetrics evaluateCandidateRegionMetrics(
    const QMatrix4x4& deltaTransform,
    const QList<QVector3D>& activeSourcePoints,
    const QList<QVector3D>& constraintPoints,
    const QVector3D& targetRegionCenter,
    double targetRegionRadiusMm)
{
    CandidateRegionMetrics metrics;
    if (activeSourcePoints.empty()) {
        return metrics;
    }

    const bool hasConstraintPoints = !constraintPoints.isEmpty();
    const bool hasTargetRegion = targetRegionRadiusMm > 0.0;
    if (!hasConstraintPoints && !hasTargetRegion) {
        return metrics;
    }

    const QVector3D effectiveCenter =
        hasTargetRegion ? targetRegionCenter : centroidOfPoints(constraintPoints);
    const double membershipRadiusMm = constraintMembershipRadiusMm(targetRegionRadiusMm);
    const int pointCount = static_cast<int>(activeSourcePoints.size());
    const int sampleCount = qMin(pointCount, 512);
    const int sampleStep = qMax(1, pointCount / qMax(sampleCount, 1));

    QVector<bool> coveredConstraintFlags(constraintPoints.size(), false);
    int matchedPointCount = 0;
    int processedPointCount = 0;

    for (int index = 0; index < pointCount && processedPointCount < sampleCount; index += sampleStep) {
        const QVector3D transformedPoint = deltaTransform.map(activeSourcePoints.at(index));
        const bool matched = pointMatchesConstraintSet(
            transformedPoint,
            constraintPoints,
            effectiveCenter,
            targetRegionRadiusMm,
            membershipRadiusMm);
        if (matched) {
            ++matchedPointCount;
            if (hasConstraintPoints && membershipRadiusMm > 0.0) {
                const double membershipRadiusSquared = membershipRadiusMm * membershipRadiusMm;
                for (int constraintIndex = 0; constraintIndex < constraintPoints.size(); ++constraintIndex) {
                    if (coveredConstraintFlags.at(constraintIndex)) {
                        continue;
                    }
                    if (squaredDistance(transformedPoint, constraintPoints.at(constraintIndex))
                        <= membershipRadiusSquared) {
                        coveredConstraintFlags[constraintIndex] = true;
                    }
                }
            }
        }
        ++processedPointCount;
    }

    if (processedPointCount <= 0) {
        return metrics;
    }

    metrics.hitRatio = static_cast<double>(matchedPointCount) / static_cast<double>(processedPointCount);
    if (hasConstraintPoints) {
        int coveredConstraintCount = 0;
        for (bool covered : coveredConstraintFlags) {
            if (covered) {
                ++coveredConstraintCount;
            }
        }
        const double constraintCoverage =
            static_cast<double>(coveredConstraintCount) / static_cast<double>(constraintPoints.size());
        metrics.coverageScore = hasTargetRegion
            ? (constraintCoverage * 0.65 + metrics.hitRatio * 0.35)
            : constraintCoverage;
    } else {
        metrics.coverageScore = metrics.hitRatio;
    }
    return metrics;
}

double candidateNormalConsistencyPrior(const CandidateInitialTransform& candidate)
{
    const double rotationMagnitudeDeg =
        static_cast<double>(candidate.rotationDeltaDeg.length());
    return qBound(0.0, 1.0 - rotationMagnitudeDeg / 8.0, 1.0);
}

double candidateCurvaturePrior(const CandidateInitialTransform& candidate)
{
    const double translationMagnitudeMm =
        static_cast<double>(candidate.translationDeltaMm.length());
    return qBound(0.0, 1.0 - translationMagnitudeMm / 4.0, 1.0);
}

vtkSmartPointer<vtkPolyData> buildConstrainedTargetPolyData(
    vtkPolyData* target,
    const QList<QVector3D>& constraintPoints,
    const QVector3D& targetRegionCenter,
    double targetRegionRadiusMm,
    int* selectedPointCount,
    int* selectedTriangleCount)
{
    if (selectedPointCount) {
        *selectedPointCount = 0;
    }
    if (selectedTriangleCount) {
        *selectedTriangleCount = 0;
    }
    if (!target || target->GetNumberOfPoints() == 0) {
        return nullptr;
    }

    const QVector3D effectiveCenter =
        targetRegionRadiusMm > 0.0 ? targetRegionCenter : centroidOfPoints(constraintPoints);
    const double membershipRadiusMm = constraintMembershipRadiusMm(targetRegionRadiusMm);
    QVector<int> pointMapping(target->GetNumberOfPoints(), -1);

    auto selectedPoints = vtkSmartPointer<vtkPoints>::New();
    for (vtkIdType pointIndex = 0; pointIndex < target->GetNumberOfPoints(); ++pointIndex) {
        double rawPoint[3];
        target->GetPoint(pointIndex, rawPoint);
        const QVector3D point(
            static_cast<float>(rawPoint[0]),
            static_cast<float>(rawPoint[1]),
            static_cast<float>(rawPoint[2]));
        if (!pointMatchesConstraintSet(
                point,
                constraintPoints,
                effectiveCenter,
                targetRegionRadiusMm,
                membershipRadiusMm)) {
            continue;
        }

        pointMapping[pointIndex] = static_cast<int>(selectedPoints->GetNumberOfPoints());
        selectedPoints->InsertNextPoint(rawPoint);
    }

    if (selectedPointCount) {
        *selectedPointCount = static_cast<int>(selectedPoints->GetNumberOfPoints());
    }
    if (selectedPoints->GetNumberOfPoints() < 3) {
        return nullptr;
    }

    auto selectedTriangles = vtkSmartPointer<vtkCellArray>::New();
    for (vtkIdType cellIndex = 0; cellIndex < target->GetNumberOfCells(); ++cellIndex) {
        vtkCell* cell = target->GetCell(cellIndex);
        if (!cell || cell->GetNumberOfPoints() != 3) {
            continue;
        }

        const int mapped0 = pointMapping[cell->GetPointId(0)];
        const int mapped1 = pointMapping[cell->GetPointId(1)];
        const int mapped2 = pointMapping[cell->GetPointId(2)];
        if (mapped0 < 0 || mapped1 < 0 || mapped2 < 0) {
            continue;
        }

        auto triangle = vtkSmartPointer<vtkTriangle>::New();
        triangle->GetPointIds()->SetId(0, mapped0);
        triangle->GetPointIds()->SetId(1, mapped1);
        triangle->GetPointIds()->SetId(2, mapped2);
        selectedTriangles->InsertNextCell(triangle);
    }

    if (selectedTriangleCount) {
        *selectedTriangleCount = static_cast<int>(selectedTriangles->GetNumberOfCells());
    }
    if (selectedTriangles->GetNumberOfCells() <= 0) {
        return nullptr;
    }

    auto constrained = vtkSmartPointer<vtkPolyData>::New();
    constrained->SetPoints(selectedPoints);
    constrained->SetPolys(selectedTriangles);
    constrained->BuildCells();
    constrained->BuildLinks();
    return constrained;
}

std::vector<mesh_gpu::Point3D> buildConstrainedTargetVertexCloud(
    vtkPolyData* target,
    const QList<QVector3D>& constraintPoints,
    const QVector3D& targetRegionCenter,
    double targetRegionRadiusMm,
    const std::vector<mesh_gpu::Point3D>& sourcePoints,
    double sourceBoundsPaddingMm,
    int* selectedPointCount)
{
    if (selectedPointCount) {
        *selectedPointCount = 0;
    }
    if (!target || target->GetNumberOfPoints() == 0) {
        return {};
    }

    const QVector3D effectiveCenter =
        targetRegionRadiusMm > 0.0 ? targetRegionCenter : centroidOfPoints(constraintPoints);
    const double membershipRadiusMm = constraintMembershipRadiusMm(targetRegionRadiusMm);
    bool useSourceBounds = !sourcePoints.empty() && sourceBoundsPaddingMm > 0.0;
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double minZ = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    double maxZ = std::numeric_limits<double>::lowest();
    if (useSourceBounds) {
        for (const mesh_gpu::Point3D& sourcePoint : sourcePoints) {
            minX = std::min(minX, static_cast<double>(sourcePoint.x));
            minY = std::min(minY, static_cast<double>(sourcePoint.y));
            minZ = std::min(minZ, static_cast<double>(sourcePoint.z));
            maxX = std::max(maxX, static_cast<double>(sourcePoint.x));
            maxY = std::max(maxY, static_cast<double>(sourcePoint.y));
            maxZ = std::max(maxZ, static_cast<double>(sourcePoint.z));
        }
        minX -= sourceBoundsPaddingMm;
        minY -= sourceBoundsPaddingMm;
        minZ -= sourceBoundsPaddingMm;
        maxX += sourceBoundsPaddingMm;
        maxY += sourceBoundsPaddingMm;
        maxZ += sourceBoundsPaddingMm;
    }

    std::vector<mesh_gpu::Point3D> selectedVertices;
    selectedVertices.reserve(static_cast<size_t>(qMin<vtkIdType>(target->GetNumberOfPoints(), 131072)));
    for (int pass = 0; pass < 2; ++pass) {
        selectedVertices.clear();
        const bool applySourceBounds = useSourceBounds && pass == 0;
        for (vtkIdType pointIndex = 0; pointIndex < target->GetNumberOfPoints(); ++pointIndex) {
            double rawPoint[3];
            target->GetPoint(pointIndex, rawPoint);
            if (applySourceBounds
                && (rawPoint[0] < minX || rawPoint[0] > maxX
                    || rawPoint[1] < minY || rawPoint[1] > maxY
                    || rawPoint[2] < minZ || rawPoint[2] > maxZ)) {
                continue;
            }

            const QVector3D point(
                static_cast<float>(rawPoint[0]),
                static_cast<float>(rawPoint[1]),
                static_cast<float>(rawPoint[2]));
            if (!pointMatchesConstraintSet(
                    point,
                    constraintPoints,
                    effectiveCenter,
                    targetRegionRadiusMm,
                    membershipRadiusMm)) {
                continue;
            }

            selectedVertices.push_back(mesh_gpu::Point3D(
                static_cast<float>(rawPoint[0]),
                static_cast<float>(rawPoint[1]),
                static_cast<float>(rawPoint[2])));
        }
        if (!applySourceBounds || selectedVertices.size() >= 3) {
            break;
        }
    }

    if (selectedPointCount) {
        *selectedPointCount = static_cast<int>(selectedVertices.size());
    }
    return selectedVertices;
}

vtkSmartPointer<vtkPolyData> buildSelectedTargetPolyData(
    vtkPolyData* target,
    const std::vector<int>& selectedIndices,
    int* selectedPointCount,
    int* selectedTriangleCount)
{
    if (selectedPointCount) {
        *selectedPointCount = 0;
    }
    if (selectedTriangleCount) {
        *selectedTriangleCount = 0;
    }
    if (!target || target->GetNumberOfPoints() == 0 || selectedIndices.empty()) {
        return nullptr;
    }

    QVector<int> pointMapping(target->GetNumberOfPoints(), -1);
    auto selectedPoints = vtkSmartPointer<vtkPoints>::New();
    selectedPoints->Allocate(static_cast<vtkIdType>(selectedIndices.size()));

    for (size_t selectedIndexOffset = 0; selectedIndexOffset < selectedIndices.size(); ++selectedIndexOffset) {
        const int pointIndex = selectedIndices[selectedIndexOffset];
        if (pointIndex < 0 || pointIndex >= target->GetNumberOfPoints() || pointMapping[pointIndex] >= 0) {
            continue;
        }

        double rawPoint[3];
        target->GetPoint(pointIndex, rawPoint);
        pointMapping[pointIndex] = static_cast<int>(selectedPoints->GetNumberOfPoints());
        selectedPoints->InsertNextPoint(rawPoint);
    }

    if (selectedPointCount) {
        *selectedPointCount = static_cast<int>(selectedPoints->GetNumberOfPoints());
    }
    if (selectedPoints->GetNumberOfPoints() < 3) {
        return nullptr;
    }

    auto selectedTriangles = vtkSmartPointer<vtkCellArray>::New();
    for (vtkIdType cellIndex = 0; cellIndex < target->GetNumberOfCells(); ++cellIndex) {
        vtkCell* cell = target->GetCell(cellIndex);
        if (!cell || cell->GetNumberOfPoints() != 3) {
            continue;
        }

        const int mapped0 = pointMapping[cell->GetPointId(0)];
        const int mapped1 = pointMapping[cell->GetPointId(1)];
        const int mapped2 = pointMapping[cell->GetPointId(2)];
        if (mapped0 < 0 || mapped1 < 0 || mapped2 < 0) {
            continue;
        }

        auto triangle = vtkSmartPointer<vtkTriangle>::New();
        triangle->GetPointIds()->SetId(0, mapped0);
        triangle->GetPointIds()->SetId(1, mapped1);
        triangle->GetPointIds()->SetId(2, mapped2);
        selectedTriangles->InsertNextCell(triangle);
    }

    if (selectedTriangleCount) {
        *selectedTriangleCount = static_cast<int>(selectedTriangles->GetNumberOfCells());
    }
    if (selectedTriangles->GetNumberOfCells() <= 0) {
        return nullptr;
    }

    auto constrained = vtkSmartPointer<vtkPolyData>::New();
    constrained->SetPoints(selectedPoints);
    constrained->SetPolys(selectedTriangles);
    constrained->BuildCells();
    constrained->BuildLinks();
    return constrained;
}

vtkSmartPointer<vtkPolyData> buildTargetPolyDataFromRuntimeMesh(
    const mesh_gpu::ConstrainedMeshResult& runtimeMesh,
    int* selectedPointCount,
    int* selectedTriangleCount)
{
    if (selectedPointCount) {
        *selectedPointCount = 0;
    }
    if (selectedTriangleCount) {
        *selectedTriangleCount = 0;
    }
    if (!runtimeMesh.success || runtimeMesh.vertices.empty() || runtimeMesh.triangles.empty()) {
        return nullptr;
    }

    auto selectedPoints = vtkSmartPointer<vtkPoints>::New();
    selectedPoints->Allocate(static_cast<vtkIdType>(runtimeMesh.vertices.size()));
    for (size_t index = 0; index < runtimeMesh.vertices.size(); ++index) {
        const mesh_gpu::Point3D& point = runtimeMesh.vertices[index];
        selectedPoints->InsertNextPoint(point.x, point.y, point.z);
    }

    auto normals = vtkSmartPointer<vtkFloatArray>::New();
    normals->SetName("Normals");
    normals->SetNumberOfComponents(3);
    normals->SetNumberOfTuples(static_cast<vtkIdType>(runtimeMesh.normals.size()));
    for (size_t index = 0; index < runtimeMesh.normals.size(); ++index) {
        const mesh_gpu::Normal3D& normal = runtimeMesh.normals[index];
        float tuple[3] = { normal.nx, normal.ny, normal.nz };
        normals->SetTypedTuple(static_cast<vtkIdType>(index), tuple);
    }

    auto selectedTriangles = vtkSmartPointer<vtkCellArray>::New();
    for (size_t triangleIndex = 0; triangleIndex < runtimeMesh.triangles.size(); ++triangleIndex) {
        const std::array<int, 3>& triangle = runtimeMesh.triangles[triangleIndex];
        if (triangle[0] < 0
            || triangle[1] < 0
            || triangle[2] < 0
            || triangle[0] >= static_cast<int>(runtimeMesh.vertices.size())
            || triangle[1] >= static_cast<int>(runtimeMesh.vertices.size())
            || triangle[2] >= static_cast<int>(runtimeMesh.vertices.size())) {
            continue;
        }

        auto vtkTriangleCell = vtkSmartPointer<vtkTriangle>::New();
        vtkTriangleCell->GetPointIds()->SetId(0, triangle[0]);
        vtkTriangleCell->GetPointIds()->SetId(1, triangle[1]);
        vtkTriangleCell->GetPointIds()->SetId(2, triangle[2]);
        selectedTriangles->InsertNextCell(vtkTriangleCell);
    }

    if (selectedPointCount) {
        *selectedPointCount = static_cast<int>(runtimeMesh.vertices.size());
    }
    if (selectedTriangleCount) {
        *selectedTriangleCount = static_cast<int>(selectedTriangles->GetNumberOfCells());
    }
    if (selectedPoints->GetNumberOfPoints() < 3 || selectedTriangles->GetNumberOfCells() <= 0) {
        return nullptr;
    }

    auto constrained = vtkSmartPointer<vtkPolyData>::New();
    constrained->SetPoints(selectedPoints);
    constrained->SetPolys(selectedTriangles);
    constrained->GetPointData()->SetNormals(normals);
    constrained->BuildCells();
    constrained->BuildLinks();
    return constrained;
}

QList<mesh_gpu::Point3D> buildConstrainedSourcePointCloud(
    vtkPolyData* source,
    const QList<QVector3D>& constraintPoints,
    const QVector3D& targetRegionCenter,
    double targetRegionRadiusMm,
    vtkMatrix4x4* initialTransform,
    int minimumPointCount)
{
    QList<mesh_gpu::Point3D> constrainedPoints;
    if (!source || source->GetNumberOfPoints() == 0) {
        return constrainedPoints;
    }

    const QVector3D effectiveCenter =
        targetRegionRadiusMm > 0.0 ? targetRegionCenter : centroidOfPoints(constraintPoints);
    const double membershipRadiusMm = constraintMembershipRadiusMm(targetRegionRadiusMm);
    QVector<QPair<double, mesh_gpu::Point3D>> rankedPoints;
    rankedPoints.reserve(static_cast<int>(source->GetNumberOfPoints()));

    for (vtkIdType pointIndex = 0; pointIndex < source->GetNumberOfPoints(); ++pointIndex) {
        double rawPoint[3];
        source->GetPoint(pointIndex, rawPoint);
        double transformedPoint[3] = { rawPoint[0], rawPoint[1], rawPoint[2] };
        if (initialTransform) {
            double point4[4] = { rawPoint[0], rawPoint[1], rawPoint[2], 1.0 };
            double result4[4];
            initialTransform->MultiplyPoint(point4, result4);
            transformedPoint[0] = result4[0];
            transformedPoint[1] = result4[1];
            transformedPoint[2] = result4[2];
        }
        const QVector3D point(
            static_cast<float>(transformedPoint[0]),
            static_cast<float>(transformedPoint[1]),
            static_cast<float>(transformedPoint[2]));
        const mesh_gpu::Point3D meshPoint {
            static_cast<float>(transformedPoint[0]),
            static_cast<float>(transformedPoint[1]),
            static_cast<float>(transformedPoint[2])
        };

        rankedPoints.append(qMakePair(squaredDistance(point, effectiveCenter), meshPoint));
        if (pointMatchesConstraintSet(
                point,
                constraintPoints,
                effectiveCenter,
                targetRegionRadiusMm,
                membershipRadiusMm)) {
            constrainedPoints.append(meshPoint);
        }
    }

    if (constrainedPoints.size() >= minimumPointCount) {
        return constrainedPoints;
    }

    std::sort(
        rankedPoints.begin(),
        rankedPoints.end(),
        [](const QPair<double, mesh_gpu::Point3D>& left, const QPair<double, mesh_gpu::Point3D>& right) {
            return left.first < right.first;
        });

    for (const auto& rankedPoint : rankedPoints) {
        bool exists = false;
        for (const auto& existing : constrainedPoints) {
            if (qFuzzyCompare(existing.x, rankedPoint.second.x)
                && qFuzzyCompare(existing.y, rankedPoint.second.y)
                && qFuzzyCompare(existing.z, rankedPoint.second.z)) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            constrainedPoints.append(rankedPoint.second);
        }
        if (constrainedPoints.size() >= minimumPointCount) {
            break;
        }
    }

    return constrainedPoints;
}

std::vector<mesh_gpu::Point3D> toStdVector(const QList<QVector3D>& points)
{
    std::vector<mesh_gpu::Point3D> result;
    result.reserve(static_cast<size_t>(points.size()));
    for (const auto& point : points) {
        result.push_back(mesh_gpu::Point3D(point.x(), point.y(), point.z()));
    }
    return result;
}

InitialAdmissionPolicy initialAdmissionPolicyFromParameters(const QVariantMap& parameters)
{
    InitialAdmissionPolicy policy;
    policy.fastPathMaxCoarseScoreMm =
        parameters.value(QStringLiteral("initialAdmissionFastPathMaxCoarseScoreMm"), 1.0).toDouble();
    policy.fastPathMinRegionHitRatio =
        parameters.value(QStringLiteral("initialAdmissionFastPathMinRegionHitRatio"), 0.80).toDouble();
    policy.fastPathMinCoverageScore =
        parameters.value(QStringLiteral("initialAdmissionFastPathMinCoverageScore"), 0.70).toDouble();
    policy.refineMaxCoarseScoreMm =
        parameters.value(QStringLiteral("initialAdmissionRefineMaxCoarseScoreMm"), 5.0).toDouble();
    policy.refineMinRegionHitRatio =
        parameters.value(QStringLiteral("initialAdmissionRefineMinRegionHitRatio"), 0.45).toDouble();
    policy.refineMinCoverageScore =
        parameters.value(QStringLiteral("initialAdmissionRefineMinCoverageScore"), 0.35).toDouble();
    policy.recoveryMaxCoarseScoreMm =
        parameters.value(QStringLiteral("initialAdmissionRecoveryMaxCoarseScoreMm"), 8.0).toDouble();
    policy.robustResidualFastPathMaxMm =
        parameters.value(QStringLiteral("initialAdmissionRobustResidualFastPathMaxMm"), 3.0).toDouble();
    policy.robustResidualRecoveryMaxMm =
        parameters.value(QStringLiteral("initialAdmissionRobustResidualRecoveryMaxMm"), 8.0).toDouble();
    return policy;
}

InitialAdmissionEvidence initialAdmissionEvidenceFromParameters(const QVariantMap& parameters)
{
    InitialAdmissionEvidence evidence;
    evidence.hasRobustInitialMetrics =
        parameters.value(QStringLiteral("robustInitialAvailable"), false).toBool()
        || parameters.contains(QStringLiteral("robustInitialRmsMm"));
    evidence.robustInitialRmsMm =
        parameters.value(QStringLiteral("robustInitialRmsMm"), 0.0).toDouble();
    evidence.robustInitialConfidence =
        parameters.value(QStringLiteral("robustInitialConfidence"), 0.0).toDouble();
    evidence.robustInitialInlierCount =
        parameters.value(QStringLiteral("robustInitialInlierCount"), 0).toInt();
    return evidence;
}

void mergeInitialAdmissionDecision(QVariantMap& metadata, const InitialAdmissionDecision& decision)
{
    metadata.insert(QStringLiteral("initialAdmissionAccepted"), decision.accepted);
    metadata.insert(QStringLiteral("initialAdmissionAction"), decision.action);
    metadata.insert(QStringLiteral("initialAdmissionReason"), decision.reason);
    metadata.insert(QStringLiteral("initialAdmissionRecoveryAction"), decision.recoveryAction);
    metadata.insert(QStringLiteral("initialAdmissionIdentityCoarseScoreMm"), decision.identityCoarseScoreMm);
    metadata.insert(QStringLiteral("initialAdmissionBestCoarseScoreMm"), decision.bestCoarseScoreMm);
    metadata.insert(QStringLiteral("initialAdmissionIdentityRegionHitRatio"), decision.identityRegionHitRatio);
    metadata.insert(QStringLiteral("initialAdmissionIdentityCoverageScore"), decision.identityCoverageScore);
    metadata.insert(QStringLiteral("initialAdmissionRobustMetricsUsed"), decision.robustInitialMetricsUsed);
    metadata.insert(QStringLiteral("initialAdmissionRobustInitialRmsMm"), decision.robustInitialRmsMm);
    metadata.insert(QStringLiteral("initialAdmissionRobustInitialConfidence"), decision.robustInitialConfidence);
    metadata.insert(QStringLiteral("initialAdmissionRobustInitialInlierCount"), decision.robustInitialInlierCount);
    metadata.insert(QStringLiteral("initialAdmissionDecision"), initialAdmissionDecisionToVariantMap(decision));
}
}

RegistrationServiceImpl::RegistrationServiceImpl(QObject* parent)
    : registration_core::RegistrationService(parent)
    , m_serviceRegistry(nullptr)
    , m_defaultLandmarkMode(VTK_LANDMARK_RIGIDBODY)
    , m_enableICPCentroids(true)
    , m_defaultICPMaxIterations(100)
    , m_defaultICPMaxLandmarks(200)
{
    qDebug() << "[RegistrationService] Initialized";
}

void RegistrationServiceImpl::setServiceRegistry(PlatformServiceRegistry* serviceRegistry)
{
    m_serviceRegistry = serviceRegistry;
}

Registration2D3DService* RegistrationServiceImpl::getRegistration2D3DService()
{
    if (!m_serviceRegistry) {
        qWarning() << "[RegistrationService] Platform service registry unavailable";
        return nullptr;
    }

    auto* service = qobject_cast<Registration2D3DService*>(
        m_serviceRegistry->service(QStringLiteral("Registration2D3DService")));
    if (!service) {
        qWarning() << "[RegistrationService] Registration2D3D service not available";
        return nullptr;
    }

    return service;
}

RegistrationServiceImpl::~RegistrationServiceImpl()
{
    // 清理 MeshGPU DLL
    if (m_meshGPU && m_destroyMeshGPU) {
        m_destroyMeshGPU(m_meshGPU);
        m_meshGPU = nullptr;
    }
    if (m_meshGPULib.isLoaded()) {
        m_meshGPULib.unload();
    }

    QMutexLocker locker(&m_mutex);
    m_registrations.clear();
}

// ==================== MeshGPU DLL 集成 ====================

bool RegistrationServiceImpl::loadMeshGPUDLL(const QString& dllPath)
{
    if (m_meshGPULoaded) return true;

    QString path = dllPath;
    if (path.isEmpty()) {
        // 默认路径：ICPtry/MeshGPU/build/Release/MeshGPULib.dll
        path = QCoreApplication::applicationDirPath() + "/MeshGPULib.dll";
    }

    qDebug() << "[RegistrationService] Attempting MeshGPU DLL load from:" << path;
    m_meshGPULib.setFileName(path);
    if (!m_meshGPULib.load()) {
        qWarning() << "[RegistrationService] MeshGPU DLL load failed:" << m_meshGPULib.errorString();
        return false;
    }

    m_createMeshGPU = reinterpret_cast<CreateMeshGPUFn>(m_meshGPULib.resolve("CreateMeshGPURuntimeApi"));
    m_destroyMeshGPU = reinterpret_cast<DestroyMeshGPUFn>(m_meshGPULib.resolve("DestroyMeshGPURuntimeApi"));

    if (m_createMeshGPU && m_destroyMeshGPU) {
        m_meshGPU = m_createMeshGPU();
        if (!m_meshGPU) {
            qWarning() << "[RegistrationService] MeshGPU DLL: CreateMeshGPURuntimeApi returned null";
            m_meshGPULib.unload();
            return false;
        }
        m_meshGPULegacyRuntime = false;
        m_meshGPUCandidateScoringAvailable = true;
    } else {
        const auto createLegacyMeshGPU =
            reinterpret_cast<CreateLegacyMeshGPUFn>(m_meshGPULib.resolve("CreateMeshGPUInterface"));
        const auto destroyLegacyMeshGPU =
            reinterpret_cast<DestroyLegacyMeshGPUFn>(m_meshGPULib.resolve("DestroyMeshGPUInterface"));
        LegacyMeshGpuRuntimeApiVTable legacyVTable;
        legacyVTable.loadTargetMesh = reinterpret_cast<LegacyMeshGpuRuntimeApiVTable::LoadTargetMeshFn>(
            m_meshGPULib.resolve("?loadTargetMesh@MeshGPUInterface@mesh_gpu@@QEAA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@M@Z"));
        legacyVTable.hasTargetMesh = reinterpret_cast<LegacyMeshGpuRuntimeApiVTable::HasTargetMeshFn>(
            m_meshGPULib.resolve("?hasTargetMesh@MeshGPUInterface@mesh_gpu@@QEBA_NXZ"));
        legacyVTable.setTargetMesh = reinterpret_cast<LegacyMeshGpuRuntimeApiVTable::SetTargetMeshFn>(
            m_meshGPULib.resolve("?setTargetMesh@MeshGPUInterface@mesh_gpu@@QEAA_NAEBV?$vector@UPoint3D@mesh_gpu@@V?$allocator@UPoint3D@mesh_gpu@@@std@@@std@@AEBV?$vector@UNormal3D@mesh_gpu@@V?$allocator@UNormal3D@mesh_gpu@@@std@@@4@AEBV?$vector@V?$array@H$02@std@@V?$allocator@V?$array@H$02@std@@@2@@4@M@Z"));
        legacyVTable.setSourcePointCloud =
            reinterpret_cast<LegacyMeshGpuRuntimeApiVTable::SetSourcePointCloudFn>(
                m_meshGPULib.resolve("?setSourcePointCloud@MeshGPUInterface@mesh_gpu@@QEAA_NAEBV?$vector@UPoint3D@mesh_gpu@@V?$allocator@UPoint3D@mesh_gpu@@@std@@@std@@@Z"));
        legacyVTable.runRegistration = reinterpret_cast<LegacyMeshGpuRuntimeApiVTable::RunRegistrationFn>(
            m_meshGPULib.resolve("?runRegistration@MeshGPUInterface@mesh_gpu@@QEAA?AURegistrationResult@2@AEBURegistrationParams@2@@Z"));
        legacyVTable.runRegistrationWithRotationSearch =
            reinterpret_cast<LegacyMeshGpuRuntimeApiVTable::RunRegistrationWithRotationSearchFn>(
                m_meshGPULib.resolve("?runRegistrationWithRotationSearch@MeshGPUInterface@mesh_gpu@@QEAA?AURegistrationResult@2@AEBURotationSearchParams@2@AEBURegistrationParams@2@@Z"));
        legacyVTable.scoreTransformCandidates =
            reinterpret_cast<LegacyMeshGpuRuntimeApiVTable::ScoreTransformCandidatesFn>(
                m_meshGPULib.resolve("?scoreTransformCandidates@MeshGPUInterface@mesh_gpu@@QEAA?AV?$vector@UTransformCandidateScore@mesh_gpu@@V?$allocator@UTransformCandidateScore@mesh_gpu@@@std@@@std@@AEBV?$vector@UTransform4x4@mesh_gpu@@V?$allocator@UTransform4x4@mesh_gpu@@@std@@@4@M@Z"));

        if (!createLegacyMeshGPU || !destroyLegacyMeshGPU || !legacyVTable.isComplete()) {
            qWarning() << "[RegistrationService] MeshGPU DLL: failed to resolve runtime or legacy factory functions";
            m_meshGPULib.unload();
            return false;
        }

        mesh_gpu::MeshGPUInterface* legacyMeshGPU = createLegacyMeshGPU();
        if (!legacyMeshGPU) {
            qWarning() << "[RegistrationService] MeshGPU DLL: CreateMeshGPUInterface returned null";
            m_meshGPULib.unload();
            return false;
        }

        m_meshGPU = new LegacyMeshGpuRuntimeApiAdapter(legacyMeshGPU, destroyLegacyMeshGPU, legacyVTable);
        m_destroyMeshGPU = [](mesh_gpu::MeshGPURuntimeApi* instance) {
            delete instance;
        };
        m_meshGPULegacyRuntime = true;
        m_meshGPUCandidateScoringAvailable = false;
        qWarning() << "[RegistrationService] MeshGPU DLL runtime API missing; using legacy interface compatibility adapter";
    }

    m_meshGPULoaded = true;
    qDebug() << "[RegistrationService] MeshGPU DLL loaded from:" << path;
    return true;
}

std::vector<float> RegistrationServiceImpl::polyDataToFloatArray(vtkPolyData* polyData)
{
    std::vector<float> result;
    if (!polyData) return result;

    vtkIdType n = polyData->GetNumberOfPoints();
    result.resize(n * 3);
    for (vtkIdType i = 0; i < n; ++i) {
        double p[3];
        polyData->GetPoint(i, p);
        result[i * 3 + 0] = static_cast<float>(p[0]);
        result[i * 3 + 1] = static_cast<float>(p[1]);
        result[i * 3 + 2] = static_cast<float>(p[2]);
    }
    return result;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::meshGPUTransformToVTK(const float* data16)
{
    vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            matrix->SetElement(i, j, static_cast<double>(data16[i * 4 + j]));
    return matrix;
}

QMatrix4x4 RegistrationServiceImpl::vtkMatrix4x4ToQMatrix(vtkMatrix4x4* matrix)
{
    QMatrix4x4 result;
    if (!matrix) {
        return result;
    }

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result(row, col) = static_cast<float>(matrix->GetElement(row, col));
        }
    }
    return result;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::qMatrix4x4ToVtkMatrix(const QMatrix4x4& matrix)
{
    vtkSmartPointer<vtkMatrix4x4> result = vtkSmartPointer<vtkMatrix4x4>::New();
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result->SetElement(row, col, static_cast<double>(matrix(row, col)));
        }
    }
    return result;
}

mesh_gpu::Transform4x4 RegistrationServiceImpl::qMatrixToMeshGpuTransform(const QMatrix4x4& matrix)
{
    mesh_gpu::Transform4x4 result;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result(row, col) = matrix(row, col);
        }
    }
    return result;
}

QList<CandidateEvaluationResult> RegistrationServiceImpl::evaluateCandidateTransformsGpu(
    const QList<CandidateInitialTransform>& candidates,
    const QList<QVector3D>& activeSourcePoints,
    const QList<QVector3D>& constraintPoints,
    const QVector3D& targetRegionCenter,
    double targetRegionRadiusMm,
    const QVariantMap& parameters)
{
    if (!m_meshGPU || candidates.isEmpty()) {
        return {};
    }

    std::vector<mesh_gpu::Transform4x4> transforms;
    transforms.reserve(static_cast<size_t>(candidates.size()));
    for (const CandidateInitialTransform& candidate : candidates) {
        transforms.push_back(qMatrixToMeshGpuTransform(candidate.transformMatrix));
    }

    const float cutoffMm = parameters.value(QStringLiteral("candidateScoreCutoffMm"), 12.0f).toFloat();
    const std::vector<mesh_gpu::RuntimeTransformCandidateScore> runtimeScores =
        m_meshGPU->scoreTransformCandidates(transforms, cutoffMm);

    QList<CandidateEvaluationResult> results;
    results.reserve(static_cast<int>(runtimeScores.size()));
    for (const mesh_gpu::RuntimeTransformCandidateScore& runtimeScore : runtimeScores) {
        if (runtimeScore.candidateIndex < 0 || runtimeScore.candidateIndex >= candidates.size()) {
            continue;
        }

        const CandidateInitialTransform& candidate = candidates.at(runtimeScore.candidateIndex);
        const CandidateRegionMetrics regionMetrics = evaluateCandidateRegionMetrics(
            candidate.transformMatrix,
            activeSourcePoints,
            constraintPoints,
            targetRegionCenter,
            targetRegionRadiusMm);
        CandidateEvaluationResult result;
        result.candidateId = candidate.candidateId;
        result.coarseScore = runtimeScore.meanDistanceMm;
        result.targetRegionHitRatio = regionMetrics.hitRatio;
        result.coverageScore = regionMetrics.coverageScore;
        result.converged = runtimeScore.success;
        if (runtimeScore.geometryScoreAvailable) {
            result.normalConsistencyScore = qBound(
                0.0,
                static_cast<double>(runtimeScore.normalConsistencyScore),
                1.0);
            result.curvatureScore = qBound(
                0.0,
                static_cast<double>(runtimeScore.curvatureScore),
                1.0);
            result.geometryScoreAvailable = true;
        } else {
            result.normalConsistencyScore = candidateNormalConsistencyPrior(candidate);
            result.curvatureScore = candidateCurvaturePrior(candidate);
        }
        results.append(result);
    }
    return results;
}

QVariantMap RegistrationServiceImpl::buildParallelSearchReport(
    const QList<CandidateInitialTransform>& candidateTransforms,
    const QList<CandidateEvaluationResult>& topKCandidateScores,
    const QVariantMap& parameters,
    qint64 coarseSearchMs,
    bool constraintParallelFilterEnabled,
    qint64 roiFilterMs,
    int multiResolutionLevelCount,
    int refineCandidateCount,
    qint64 refineMs,
    int bestCandidateRank,
    const QVariantMap& batchRefineMetrics) const
{
    const bool parallelSearchEnabled = !candidateTransforms.isEmpty() && !topKCandidateScores.isEmpty();

    QVariantMap report;
    report.insert(QStringLiteral("parallelSearchEnabled"), parallelSearchEnabled);
    report.insert(QStringLiteral("candidateCount"), candidateTransforms.size());
    report.insert(QStringLiteral("topKCount"), topKCandidateScores.size());
    report.insert(QStringLiteral("coarseSearchMs"), coarseSearchMs);
    report.insert(
        QStringLiteral("multiResolutionProfile"),
        parameters.value(
            QStringLiteral("multiResolutionProfileId"),
            QStringLiteral("ankle_roi_two_level")).toString());
    report.insert(QStringLiteral("constraintParallelFilterEnabled"), constraintParallelFilterEnabled);
    report.insert(QStringLiteral("roiFilterMs"), roiFilterMs);
    report.insert(QStringLiteral("multiResolutionLevelCount"), multiResolutionLevelCount);
    report.insert(QStringLiteral("refineCandidateCount"), refineCandidateCount);
    report.insert(QStringLiteral("refineMs"), refineMs);

    if (!topKCandidateScores.isEmpty()) {
        QVariantList topKCandidateDetails;
        topKCandidateDetails.reserve(topKCandidateScores.size());
        for (const CandidateEvaluationResult& candidateScore : topKCandidateScores) {
            topKCandidateDetails.append(candidateEvaluationToVariantMap(candidateScore));
        }
        report.insert(QStringLiteral("topKCandidateDetails"), topKCandidateDetails);

        const CandidateEvaluationResult& bestCandidate = topKCandidateScores.first();
        report.insert(QStringLiteral("bestCandidateId"), bestCandidate.candidateId);
        report.insert(QStringLiteral("bestCandidateRank"), bestCandidateRank >= 0 ? bestCandidateRank : 0);
        report.insert(QStringLiteral("coarseScore"), bestCandidate.coarseScore);
        report.insert(QStringLiteral("targetRegionHitRatio"), bestCandidate.targetRegionHitRatio);
        report.insert(QStringLiteral("coverageScore"), bestCandidate.coverageScore);
    }

    report.unite(batchRefineMetrics);

    return report;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::performGICPRegistration(
    vtkPolyData* source,
    vtkPolyData* target,
    vtkMatrix4x4* initialTransform,
    const QVariantMap& parameters,
    const QString& registrationId)
{
    if (!m_meshGPULoaded) {
        qWarning() << "[RegistrationService] GICP: MeshGPU DLL not loaded, falling back to VTK ICP";
        return nullptr; // caller will fallback
    }

    emit registrationStarted(registrationId, "gicp");
    QElapsedTimer timer;
    timer.start();

    try {
        qDebug() << "[RegistrationService] GPU-GICP stage: begin"
                 << "registrationId=" << registrationId;
        if (parameters.value(QStringLiteral("useBatchRefineAsFinalResult"), false).toBool()) {
            const QList<QVariant> matrixVariantList =
                parameters.value(QStringLiteral("batchRefineFinalTransform")).toList();
            QList<double> matrixValues;
            matrixValues.reserve(matrixVariantList.size());
            for (const QVariant& value : matrixVariantList) {
                matrixValues.append(value.toDouble());
            }

            vtkSmartPointer<vtkMatrix4x4> precomputedFinalMatrix = listToMatrix(matrixValues);
            if (precomputedFinalMatrix) {
                mesh_gpu::RuntimeRegistrationResult result;
                result.rmse = parameters.value(QStringLiteral("batchRefineFinalRmse")).toFloat();
                result.iterations = parameters.value(QStringLiteral("batchRefineFinalIterations")).toInt();
                result.converged = parameters.value(QStringLiteral("batchRefineFinalConverged")).toBool();
                const qint64 elapsedMs = timer.elapsed();
                const QVariantMap parallelSearchReport =
                    parameters.value(QStringLiteral("parallelSearchReport")).toMap();
                const QMap<QString, QList<QVector3D>> constraintRegions =
                    constraintRegionsFromVariant(parameters.value(QStringLiteral("constraintRegions")).toMap());

                RegistrationRecord record;
                record.registrationId = registrationId;
                record.type = "gicp";
                record.transform = precomputedFinalMatrix;
                record.timestamp = QDateTime::currentMSecsSinceEpoch();
                record.fre = static_cast<double>(result.rmse);
                record.numPoints = static_cast<int>(source->GetNumberOfPoints());

                QVariantMap metadata;
                metadata["algorithm"] = "GPU-GICP";
                metadata["iterations"] = result.iterations;
                metadata["converged"] = result.converged;
                metadata["rmse"] = static_cast<double>(result.rmse);
                metadata["elapsedMs"] = elapsedMs;
                metadata["sourcePoints"] = static_cast<int>(source->GetNumberOfPoints());
                metadata["targetPoints"] = static_cast<int>(target->GetNumberOfPoints());
                metadata["useRotationSearch"] = parameters.value("useRotationSearch", false).toBool();
                metadata["constraintRegionCount"] = constraintRegions.size();
                metadata["constraintRegionKeys"] =
                    parameters.value(QStringLiteral("constraintRegionKeys")).toString();
                metadata["coreConstraintApplied"] =
                    parallelSearchReport.value(QStringLiteral("parallelPrecomputedConstraintApplied")).toBool();
                metadata["coreConstraintSourcePointCount"] =
                    parallelSearchReport.value(QStringLiteral("parallelPrecomputedSourcePointCount")).toInt();
                metadata["coreConstraintTargetPointCount"] =
                    parallelSearchReport.value(QStringLiteral("parallelPrecomputedTargetPointCount")).toInt();
                metadata["coreConstraintTargetTriangleCount"] =
                    parallelSearchReport.value(QStringLiteral("parallelPrecomputedTargetTriangleCount")).toInt();
                metadata["runtimeSourceConstraintFilterUsed"] =
                    parallelSearchReport.value(QStringLiteral("runtimeSourceConstraintFilterUsed")).toBool();
                metadata["finalResultSource"] =
                    parallelSearchReport.value(
                        QStringLiteral("precomputedFinalResultSource"),
                        QStringLiteral("parallel_batch_refine")).toString();
                metadata["precomputedBatchRefineFastPath"] = true;
                metadata["finalStageTargetPrepared"] = false;
                metadata.unite(parallelSearchReport);
                if (applyPairedResidualGuard(
                        precomputedFinalMatrix,
                        initialTransform,
                        parameters,
                        metadata)) {
                    metadata["finalResultSource"] = QStringLiteral("paired_residual_guard_initial_fallback");
                    result.rmse = static_cast<float>(
                        metadata.value(QStringLiteral("pairedResidualGuardAcceptedFinalMm")).toDouble());
                    result.iterations = 0;
                    result.converged = true;
                    metadata["rmse"] = static_cast<double>(result.rmse);
                    metadata["iterations"] = result.iterations;
                    metadata["converged"] = result.converged;
                }
                metadata["pipelineElapsedMs"] =
                    metadata.value(QStringLiteral("parallelSearchTotalMs")).toLongLong() + elapsedMs;
                record.metadata = metadata;
                record.transform = precomputedFinalMatrix;
                record.fre = static_cast<double>(result.rmse);

                saveRecord(registrationId, record);

                qDebug() << "[RegistrationService] GPU-GICP stage: using parallel batch refine fast path"
                         << "registrationId=" << registrationId
                         << "rmse=" << result.rmse
                         << "iterations=" << result.iterations
                         << "converged=" << result.converged;
                qDebug() << "[RegistrationService] GPU-GICP completed:"
                         << "ID=" << registrationId
                         << "RMSE=" << result.rmse << "mm"
                         << "Iterations=" << result.iterations
                         << "Time=" << elapsedMs << "ms"
                         << "Converged=" << result.converged;

                QVariantMap resultInfo;
                resultInfo["registrationId"] = registrationId;
                resultInfo["type"] = "gicp";
                resultInfo["rmse"] = static_cast<double>(result.rmse);
                resultInfo["iterations"] = result.iterations;
                resultInfo["converged"] = result.converged;
                resultInfo["elapsedMs"] = elapsedMs;
                emit registrationCompleted(registrationId, resultInfo);

                if (result.rmse > 3.0f) {
                    QVariantMap quality;
                    quality["rmse"] = static_cast<double>(result.rmse);
                    emit registrationQualityWarning(registrationId, quality,
                        QString("High GICP RMSE: %1 mm").arg(result.rmse, 0, 'f', 2));
                }

                return precomputedFinalMatrix;
            }

            qWarning() << "[RegistrationService] Invalid parallel batch refine transform; falling back to runtime GICP"
                       << "registrationId=" << registrationId;
        }
        // 将 target mesh 加载到 MeshGPU
        // 先尝试从文件加载（如果参数中提供了路径）
        QString targetMeshPath = parameters.value("targetMeshPath").toString();
        const QMap<QString, QList<QVector3D>> constraintRegions =
            constraintRegionsFromVariant(parameters.value(QStringLiteral("constraintRegions")).toMap());
        const QList<QVector3D> flattenedConstraintPoints = flattenConstraintRegions(constraintRegions);
        const bool constraintParallelFilterEnabled =
            parameters.value(QStringLiteral("enableConstraintParallelFilter"), false).toBool();
        const double targetRegionRadiusMm = parameters.value(QStringLiteral("targetRegionRadiusMm"), 0.0).toDouble();
        const QVector3D targetRegionCenter(
            parameters.value(QStringLiteral("targetRegionCenterX"), 0.0).toFloat(),
            parameters.value(QStringLiteral("targetRegionCenterY"), 0.0).toFloat(),
            parameters.value(QStringLiteral("targetRegionCenterZ"), 0.0).toFloat());
        const QVector3D effectiveTargetRegionCenter =
            targetRegionRadiusMm > 0.0 ? targetRegionCenter : centroidOfPoints(flattenedConstraintPoints);
        const bool hasTargetConstraint = !flattenedConstraintPoints.isEmpty() || targetRegionRadiusMm > 0.0;
        const bool preferCpuTargetConstraintPreupload =
            hasTargetConstraint && targetMeshPath.isEmpty() && !m_meshGPU->hasTargetMesh();
        QString coreConstraintTargetBuildSource = QStringLiteral("none");
        if (!targetMeshPath.isEmpty()) {
            qDebug() << "[RegistrationService] GPU-GICP stage: loadTargetMesh(file)"
                     << "path=" << targetMeshPath;
            if (!m_meshGPU->loadTargetMesh(targetMeshPath.toStdString())) {
                qWarning() << "[RegistrationService] GICP: failed to load target mesh from file";
                return nullptr;
            }
            qDebug() << "[RegistrationService] GPU-GICP stage: loadTargetMesh(file) done";
        } else if (preferCpuTargetConstraintPreupload) {
            qDebug() << "[RegistrationService] GPU-GICP stage: skip full target upload before ROI constraint"
                     << "targetPoints=" << target->GetNumberOfPoints()
                     << "targetCells=" << target->GetNumberOfCells();
        } else if (!m_meshGPU->hasTargetMesh()) {
            // 从 vtkPolyData 转换点云设置为 target
            // MeshGPU 需要 PLY 文件或 vertices+normals+triangles
            // 这里用 setTargetMesh 接口
            vtkIdType nVerts = target->GetNumberOfPoints();
            vtkIdType nTris = target->GetNumberOfCells();
            qDebug() << "[RegistrationService] GPU-GICP stage: setTargetMesh(begin)"
                     << "vertices=" << nVerts
                     << "cells=" << nTris;

            std::vector<mesh_gpu::Point3D> vertices(nVerts);
            std::vector<mesh_gpu::Normal3D> normals(nVerts);
            std::vector<std::array<int, 3>> triangles;

            for (vtkIdType i = 0; i < nVerts; ++i) {
                double p[3];
                target->GetPoint(i, p);
                vertices[i] = {static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])};
            }

            // 法向量
            vtkDataArray* normalArray = target->GetPointData() ? target->GetPointData()->GetNormals() : nullptr;
            if (normalArray) {
                for (vtkIdType i = 0; i < nVerts; ++i) {
                    double n[3];
                    normalArray->GetTuple(i, n);
                    normals[i] = {static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2])};
                }
            }

            // 三角面
            triangles.reserve(nTris);
            for (vtkIdType i = 0; i < nTris; ++i) {
                vtkCell* cell = target->GetCell(i);
                if (cell && cell->GetNumberOfPoints() == 3) {
                    triangles.push_back({
                        static_cast<int>(cell->GetPointId(0)),
                        static_cast<int>(cell->GetPointId(1)),
                        static_cast<int>(cell->GetPointId(2))
                    });
                }
            }

            float cellSize = parameters.value("cellSize", 1.0).toFloat();
            qDebug() << "[RegistrationService] GPU-GICP stage: setTargetMesh(call)"
                     << "triangleCount=" << triangles.size()
                     << "cellSize=" << cellSize;
            if (!m_meshGPU->setTargetMesh(vertices, normals, triangles, cellSize)) {
                qWarning() << "[RegistrationService] GICP: failed to set target mesh";
                return nullptr;
            }
            qDebug() << "[RegistrationService] GPU-GICP stage: setTargetMesh(done)";
        } else {
            qDebug() << "[RegistrationService] GPU-GICP stage: target mesh already cached";
        }

        // 设置源点云
        int coreConstraintTargetPointCount = 0;
        int coreConstraintTargetTriangleCount = 0;
        vtkSmartPointer<vtkPolyData> constrainedTarget;
        mesh_gpu::ConstrainedMeshResult constrainedTargetMesh;
        bool hasRuntimeConstrainedTargetMesh = false;
        if (hasTargetConstraint) {
            if (constraintParallelFilterEnabled && !preferCpuTargetConstraintPreupload) {
                constrainedTargetMesh = m_meshGPU->buildConstrainedTargetMesh(
                    mesh_gpu::Point3D(
                        effectiveTargetRegionCenter.x(),
                        effectiveTargetRegionCenter.y(),
                        effectiveTargetRegionCenter.z()),
                    static_cast<float>(targetRegionRadiusMm),
                    static_cast<float>(constraintMembershipRadiusMm(targetRegionRadiusMm)),
                    toStdVector(flattenedConstraintPoints),
                    3);
                if (constrainedTargetMesh.success) {
                    constrainedTarget = buildTargetPolyDataFromRuntimeMesh(
                        constrainedTargetMesh,
                        &coreConstraintTargetPointCount,
                        &coreConstraintTargetTriangleCount);
                    hasRuntimeConstrainedTargetMesh =
                        constrainedTarget && coreConstraintTargetPointCount >= 3 && coreConstraintTargetTriangleCount > 0;
                    coreConstraintTargetBuildSource = QStringLiteral("runtime_existing_target");
                }
            }

            if (!constrainedTarget) {
                constrainedTarget = buildConstrainedTargetPolyData(
                    target,
                    flattenedConstraintPoints,
                    effectiveTargetRegionCenter,
                    targetRegionRadiusMm,
                    &coreConstraintTargetPointCount,
                    &coreConstraintTargetTriangleCount);
                if (constrainedTarget
                    && coreConstraintTargetPointCount >= 3
                    && coreConstraintTargetTriangleCount > 0) {
                    coreConstraintTargetBuildSource =
                        preferCpuTargetConstraintPreupload
                            ? QStringLiteral("cpu_preupload")
                            : QStringLiteral("cpu_fallback");
                }
            }
        }

        vtkPolyData* activeTarget =
            constrainedTarget && coreConstraintTargetPointCount >= 3 && coreConstraintTargetTriangleCount > 0
            ? constrainedTarget
            : target;

        if (activeTarget != target) {
            qDebug() << "[RegistrationService] GPU-GICP stage: constrained target mesh activated"
                     << "selectedPoints=" << coreConstraintTargetPointCount
                     << "selectedTriangles=" << coreConstraintTargetTriangleCount;
            std::vector<mesh_gpu::Point3D> vertices;
            std::vector<mesh_gpu::Normal3D> normals;
            std::vector<std::array<int, 3>> triangles;

            if (hasRuntimeConstrainedTargetMesh) {
                vertices = constrainedTargetMesh.vertices;
                normals = constrainedTargetMesh.normals;
                triangles = constrainedTargetMesh.triangles;
            } else {
                vertices.resize(activeTarget->GetNumberOfPoints());
                normals.resize(activeTarget->GetNumberOfPoints());
                for (vtkIdType i = 0; i < activeTarget->GetNumberOfPoints(); ++i) {
                    double p[3];
                    activeTarget->GetPoint(i, p);
                    vertices[static_cast<size_t>(i)] = {
                        static_cast<float>(p[0]),
                        static_cast<float>(p[1]),
                        static_cast<float>(p[2])
                    };
                }

                vtkDataArray* constrainedNormals =
                    activeTarget->GetPointData() ? activeTarget->GetPointData()->GetNormals() : nullptr;
                if (constrainedNormals) {
                    for (vtkIdType i = 0; i < activeTarget->GetNumberOfPoints(); ++i) {
                        double n[3];
                        constrainedNormals->GetTuple(i, n);
                        normals[static_cast<size_t>(i)] = {
                            static_cast<float>(n[0]),
                            static_cast<float>(n[1]),
                            static_cast<float>(n[2])
                        };
                    }
                }

                triangles.reserve(activeTarget->GetNumberOfCells());
                for (vtkIdType i = 0; i < activeTarget->GetNumberOfCells(); ++i) {
                    vtkCell* cell = activeTarget->GetCell(i);
                    if (cell && cell->GetNumberOfPoints() == 3) {
                        triangles.push_back({
                            static_cast<int>(cell->GetPointId(0)),
                            static_cast<int>(cell->GetPointId(1)),
                            static_cast<int>(cell->GetPointId(2))
                        });
                    }
                }
            }

            float cellSize = parameters.value("cellSize", 1.0).toFloat();
            if (!m_meshGPU->setTargetMesh(vertices, normals, triangles, cellSize)) {
                qWarning() << "[RegistrationService] GICP: failed to set constrained target mesh";
                return nullptr;
            }
        }

        const bool hasSourceConstraintFiltering =
            !flattenedConstraintPoints.isEmpty() || targetRegionRadiusMm > 0.0;
        const QList<mesh_gpu::Point3D> constrainedSourcePoints =
            hasSourceConstraintFiltering && !constraintParallelFilterEnabled
            ? buildConstrainedSourcePointCloud(
                source,
                flattenedConstraintPoints,
                targetRegionCenter,
                targetRegionRadiusMm,
                initialTransform,
                3)
            : QList<mesh_gpu::Point3D> {};

        vtkIdType nSource = source->GetNumberOfPoints();
        std::vector<mesh_gpu::Point3D> sourcePoints(nSource);

        // 如果有初始变换，先应用
        if (initialTransform) {
            for (vtkIdType i = 0; i < nSource; ++i) {
                double p[3], out[3];
                source->GetPoint(i, p);
                double pt[4] = {p[0], p[1], p[2], 1.0};
                double res[4];
                initialTransform->MultiplyPoint(pt, res);
                sourcePoints[i] = {static_cast<float>(res[0]), static_cast<float>(res[1]), static_cast<float>(res[2])};
            }
        } else {
            for (vtkIdType i = 0; i < nSource; ++i) {
                double p[3];
                source->GetPoint(i, p);
                sourcePoints[i] = {static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])};
            }
        }

        std::vector<mesh_gpu::Point3D> activeSourcePoints = sourcePoints;
        bool runtimeSourceConstraintFilterUsed = false;
        if (hasSourceConstraintFiltering && constraintParallelFilterEnabled) {
            const mesh_gpu::RuntimeConstraintFilterResult runtimeFilterResult = m_meshGPU->filterSourcePointsByConstraints(
                sourcePoints,
                mesh_gpu::Point3D(
                    effectiveTargetRegionCenter.x(),
                    effectiveTargetRegionCenter.y(),
                    effectiveTargetRegionCenter.z()),
                static_cast<float>(targetRegionRadiusMm),
                static_cast<float>(constraintMembershipRadiusMm(targetRegionRadiusMm)),
                toStdVector(flattenedConstraintPoints),
                3);
            if (runtimeFilterResult.success && !runtimeFilterResult.selectedIndices.empty()) {
                activeSourcePoints.clear();
                activeSourcePoints.reserve(runtimeFilterResult.selectedIndices.size());
                for (size_t selectedIndexOffset = 0;
                     selectedIndexOffset < runtimeFilterResult.selectedIndices.size();
                     ++selectedIndexOffset) {
                    const int selectedIndex = runtimeFilterResult.selectedIndices[selectedIndexOffset];
                    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(sourcePoints.size())) {
                        activeSourcePoints.push_back(sourcePoints[static_cast<size_t>(selectedIndex)]);
                    }
                }
                runtimeSourceConstraintFilterUsed = true;
            }
            if (!runtimeSourceConstraintFilterUsed) {
                const QList<mesh_gpu::Point3D> fallbackConstrainedSourcePoints = buildConstrainedSourcePointCloud(
                    source,
                    flattenedConstraintPoints,
                    targetRegionCenter,
                    targetRegionRadiusMm,
                    initialTransform,
                    3);
                if (!fallbackConstrainedSourcePoints.isEmpty()) {
                    activeSourcePoints.assign(
                        fallbackConstrainedSourcePoints.cbegin(),
                        fallbackConstrainedSourcePoints.cend());
                }
            }
        } else if (!constrainedSourcePoints.isEmpty()) {
            activeSourcePoints.assign(constrainedSourcePoints.cbegin(), constrainedSourcePoints.cend());
        }

        qDebug() << "[RegistrationService] GPU-GICP stage: setSourcePointCloud(call)"
                 << "sourcePoints=" << activeSourcePoints.size();
        if (!m_meshGPU->setSourcePointCloud(activeSourcePoints)) {
            qWarning() << "[RegistrationService] GICP: failed to set source point cloud";
            return nullptr;
        }
        qDebug() << "[RegistrationService] GPU-GICP stage: setSourcePointCloud(done)";

        // 配置配准参数
        mesh_gpu::RegistrationParams regParams;
        regParams.max_iterations = parameters.value("maxIterations", 50).toInt();
        regParams.convergence_threshold = parameters.value("convergenceThreshold", 1e-6f).toFloat();
        regParams.distance_threshold = parameters.value("distanceThreshold", 10.0f).toFloat();
        regParams.use_point_to_plane = parameters.value("usePointToPlane", true).toBool();
        regParams.verbose = parameters.value("verbose", false).toBool();

        int cwMode = parameters.value("curvatureWeightMode", 0).toInt();
        regParams.curvature_weight_mode = static_cast<mesh_gpu::CurvatureWeightMode>(cwMode);

        // 执行配准
        bool useRotationSearch = parameters.value("useRotationSearch", false).toBool();
        mesh_gpu::RuntimeRegistrationResult result;
        vtkSmartPointer<vtkMatrix4x4> precomputedFinalMatrix = nullptr;

        if (parameters.value(QStringLiteral("useBatchRefineAsFinalResult"), false).toBool()) {
            const QList<QVariant> matrixVariantList =
                parameters.value(QStringLiteral("batchRefineFinalTransform")).toList();
            QList<double> matrixValues;
            matrixValues.reserve(matrixVariantList.size());
            for (const QVariant& value : matrixVariantList) {
                matrixValues.append(value.toDouble());
            }
            precomputedFinalMatrix = listToMatrix(matrixValues);
            if (precomputedFinalMatrix) {
                result.rmse = parameters.value(QStringLiteral("batchRefineFinalRmse")).toFloat();
                result.iterations = parameters.value(QStringLiteral("batchRefineFinalIterations")).toInt();
                result.converged = parameters.value(QStringLiteral("batchRefineFinalConverged")).toBool();
                qDebug() << "[RegistrationService] GPU-GICP stage: using parallel batch refine result as final output"
                         << "registrationId=" << registrationId
                         << "rmse=" << result.rmse
                         << "iterations=" << result.iterations
                         << "converged=" << result.converged;
            } else {
                qWarning() << "[RegistrationService] Invalid parallel batch refine transform; falling back to runtime GICP"
                           << "registrationId=" << registrationId;
            }
        }

        if (!precomputedFinalMatrix) {
            if (useRotationSearch) {
                qDebug() << "[RegistrationService] GPU-GICP stage: runRegistrationWithRotationSearch(call)";
                result = m_meshGPU->runRegistrationWithRotationSearch(
                    mesh_gpu::RotationSearchParams(), regParams);
            } else {
                qDebug() << "[RegistrationService] GPU-GICP stage: runRegistration(call)";
                result = m_meshGPU->runRegistration(regParams);
            }
            qDebug() << "[RegistrationService] GPU-GICP stage: registration(done)"
                     << "rmse=" << result.rmse
                     << "iterations=" << result.iterations
                     << "converged=" << result.converged;
        }

        qint64 elapsedMs = timer.elapsed();

        // 转换结果
        vtkSmartPointer<vtkMatrix4x4> gicpMatrix =
            precomputedFinalMatrix ? nullptr : meshGPUTransformToVTK(result.transform.data);

        // 如果有初始变换，合成最终变换
        vtkSmartPointer<vtkMatrix4x4> finalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        if (precomputedFinalMatrix) {
            finalMatrix->DeepCopy(precomputedFinalMatrix);
        } else if (initialTransform) {
            vtkMatrix4x4::Multiply4x4(gicpMatrix, initialTransform, finalMatrix);
        } else {
            finalMatrix->DeepCopy(gicpMatrix);
        }

        // 保存配准记录
        RegistrationRecord record;
        record.registrationId = registrationId;
        record.type = "gicp";
        record.transform = finalMatrix;
        record.timestamp = QDateTime::currentMSecsSinceEpoch();
        record.fre = static_cast<double>(result.rmse);
        record.numPoints = static_cast<int>(nSource);

        QVariantMap metadata;
        metadata["algorithm"] = "GPU-GICP";
        metadata["iterations"] = result.iterations;
        metadata["converged"] = result.converged;
        metadata["rmse"] = static_cast<double>(result.rmse);
        metadata["elapsedMs"] = elapsedMs;
        metadata["sourcePoints"] = static_cast<int>(nSource);
        metadata["targetPoints"] = static_cast<int>(target->GetNumberOfPoints());
        metadata["useRotationSearch"] = useRotationSearch;
        metadata["constraintRegionCount"] = constraintRegions.size();
        metadata["constraintRegionKeys"] = parameters.value(QStringLiteral("constraintRegionKeys")).toString();
        metadata["coreConstraintApplied"] = activeTarget != target || !constrainedSourcePoints.isEmpty();
        metadata["coreConstraintSourcePointCount"] = static_cast<int>(activeSourcePoints.size());
        metadata["coreConstraintTargetPointCount"] =
            activeTarget != target ? coreConstraintTargetPointCount : static_cast<int>(target->GetNumberOfPoints());
        metadata["coreConstraintTargetTriangleCount"] =
            activeTarget != target ? coreConstraintTargetTriangleCount : static_cast<int>(target->GetNumberOfCells());
        metadata["coreConstraintTargetBuildSource"] = coreConstraintTargetBuildSource;
        metadata["runtimeSourceConstraintFilterUsed"] = runtimeSourceConstraintFilterUsed;
        metadata["finalResultSource"] =
            precomputedFinalMatrix ? QStringLiteral("parallel_batch_refine") : QStringLiteral("gpu_gicp");
        metadata["precomputedBatchRefineFastPath"] = false;
        metadata["finalStageTargetPrepared"] = true;
        metadata.unite(parameters.value(QStringLiteral("parallelSearchReport")).toMap());
        if (applyPairedResidualGuard(
                finalMatrix,
                initialTransform,
                parameters,
                metadata)) {
            metadata["finalResultSource"] = QStringLiteral("paired_residual_guard_initial_fallback");
            result.rmse = static_cast<float>(
                metadata.value(QStringLiteral("pairedResidualGuardAcceptedFinalMm")).toDouble());
            result.iterations = 0;
            result.converged = true;
            metadata["rmse"] = static_cast<double>(result.rmse);
            metadata["iterations"] = result.iterations;
            metadata["converged"] = result.converged;
        }
        metadata["pipelineElapsedMs"] =
            metadata.value(QStringLiteral("parallelSearchTotalMs")).toLongLong() + elapsedMs;
        record.metadata = metadata;
        record.transform = finalMatrix;
        record.fre = static_cast<double>(result.rmse);

        saveRecord(registrationId, record);

        qDebug() << "[RegistrationService] GPU-GICP completed:"
                 << "ID=" << registrationId
                 << "RMSE=" << result.rmse << "mm"
                 << "Iterations=" << result.iterations
                 << "Time=" << elapsedMs << "ms"
                 << "Converged=" << result.converged;

        QVariantMap resultInfo;
        resultInfo["registrationId"] = registrationId;
        resultInfo["type"] = "gicp";
        resultInfo["rmse"] = static_cast<double>(result.rmse);
        resultInfo["iterations"] = result.iterations;
        resultInfo["converged"] = result.converged;
        resultInfo["elapsedMs"] = elapsedMs;
        emit registrationCompleted(registrationId, resultInfo);

        if (result.rmse > 3.0f) {
            QVariantMap quality;
            quality["rmse"] = static_cast<double>(result.rmse);
            emit registrationQualityWarning(registrationId, quality,
                QString("High GICP RMSE: %1 mm").arg(result.rmse, 0, 'f', 2));
        }

        return finalMatrix;

    } catch (const std::exception& ex) {
        m_lastError = QString("GPU-GICP registration failed: %1").arg(ex.what());
        qCritical() << "[RegistrationService]" << m_lastError;
        emit registrationFailed(registrationId, m_lastError);
        return nullptr;
    }
}

// ==================== Landmark 配准 ====================

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::performLandmarkRegistration(
    vtkPoints* sourcePoints,
    vtkPoints* targetPoints,
    const QString& registrationId)
{
    if (!validatePointSets(sourcePoints, targetPoints)) {
        return nullptr;
    }

    QString regId = registrationId.isEmpty() ? generateRegistrationId("landmark") : registrationId;

    emit registrationStarted(regId, "landmark");

    try {
        // 创建 Landmark Transform
        vtkSmartPointer<vtkLandmarkTransform> landmarkTransform =
            vtkSmartPointer<vtkLandmarkTransform>::New();

        landmarkTransform->SetSourceLandmarks(sourcePoints);
        landmarkTransform->SetTargetLandmarks(targetPoints);
        landmarkTransform->SetModeToRigidBody();  // Rigid 变换（刚体）
        landmarkTransform->Update();

        // 获取变换矩阵
        vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
        matrix->DeepCopy(landmarkTransform->GetMatrix());

        // 计算 FRE
        double fre = computeRMSError(sourcePoints, targetPoints, matrix);

        // 保存配准记录
        RegistrationRecord record;
        record.registrationId = regId;
        record.type = "landmark";
        record.transform = matrix;
        record.timestamp = QDateTime::currentMSecsSinceEpoch();
        record.fre = fre;
        record.numPoints = sourcePoints->GetNumberOfPoints();

        // 保存源点和目标点的副本（用于后续 TRE 计算）
        record.sourcePoints = vtkSmartPointer<vtkPoints>::New();
        record.sourcePoints->DeepCopy(sourcePoints);
        record.targetPoints = vtkSmartPointer<vtkPoints>::New();
        record.targetPoints->DeepCopy(targetPoints);

        saveRecord(regId, record);

        qDebug() << "[RegistrationService] Landmark registration completed:"
                 << "ID=" << regId
                 << "Points=" << record.numPoints
                 << "FRE=" << fre << "mm";

        // 发射完成信号
        QVariantMap result;
        result["registrationId"] = regId;
        result["type"] = "landmark";
        result["fre"] = fre;
        result["numPoints"] = record.numPoints;
        emit registrationCompleted(regId, result);

        // 质量检查
        if (fre > 5.0) {  // 如果 FRE > 5mm，发出警告
            QVariantMap quality;
            quality["fre"] = fre;
            emit registrationQualityWarning(regId, quality,
                QString("High FRE detected: %1 mm. Consider re-selecting landmarks.").arg(fre, 0, 'f', 2));
        }

        return matrix;

    } catch (const std::exception& ex) {
        m_lastError = QString("Landmark registration failed: %1").arg(ex.what());
        qCritical() << "[RegistrationService]" << m_lastError;
        emit registrationFailed(regId, m_lastError);
        return nullptr;
    }
}

QList<double> RegistrationServiceImpl::performLandmarkRegistrationList(
    const QList<QList<double>>& sourcePoints,
    const QList<QList<double>>& targetPoints)
{
    vtkSmartPointer<vtkPoints> srcVtk = listToVtkPoints(sourcePoints);
    vtkSmartPointer<vtkPoints> tgtVtk = listToVtkPoints(targetPoints);

    if (!srcVtk || !tgtVtk) {
        return QList<double>();
    }

    vtkSmartPointer<vtkMatrix4x4> matrix = performLandmarkRegistration(srcVtk, tgtVtk);
    if (!matrix) {
        return QList<double>();
    }

    return matrixToList(matrix);
}

// ==================== ICP 配准 ====================

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::performICPRegistration(
    vtkPolyData* source,
    vtkPolyData* target,
    vtkMatrix4x4* initialTransform,
    int maxIterations,
    const QString& registrationId)
{
    if (!source || !target) {
        m_lastError = "Source or target mesh is null";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    if (source->GetNumberOfPoints() == 0 || target->GetNumberOfPoints() == 0) {
        m_lastError = "Source or target mesh has no points";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    QString regId = registrationId.isEmpty() ? generateRegistrationId("icp") : registrationId;

    emit registrationStarted(regId, "icp");

    try {
        // 应用初始变换（如果提供）
        vtkSmartPointer<vtkPolyData> transformedSource = source;
        if (initialTransform) {
            vtkSmartPointer<vtkTransformPolyDataFilter> transformFilter =
                vtkSmartPointer<vtkTransformPolyDataFilter>::New();
            vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
            transform->SetMatrix(initialTransform);
            transformFilter->SetTransform(transform);
            transformFilter->SetInputData(source);
            transformFilter->Update();

            transformedSource = vtkSmartPointer<vtkPolyData>::New();
            transformedSource->DeepCopy(transformFilter->GetOutput());
        }

        // 创建 ICP Transform
        vtkSmartPointer<vtkIterativeClosestPointTransform> icp =
            vtkSmartPointer<vtkIterativeClosestPointTransform>::New();

        icp->SetSource(transformedSource);
        icp->SetTarget(target);
        icp->GetLandmarkTransform()->SetModeToRigidBody();  // Rigid 变换
        icp->SetMaximumNumberOfIterations(maxIterations);
        icp->SetMaximumNumberOfLandmarks(m_defaultICPMaxLandmarks);

        if (m_enableICPCentroids) {
            icp->StartByMatchingCentroidsOn();
        }

        icp->Modified();
        icp->Update();

        // 获取变换矩阵
        vtkSmartPointer<vtkMatrix4x4> icpMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        icpMatrix->DeepCopy(icp->GetMatrix());

        // 如果有初始变换，需要合成最终变换
        vtkSmartPointer<vtkMatrix4x4> finalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        if (initialTransform) {
            vtkMatrix4x4::Multiply4x4(icpMatrix, initialTransform, finalMatrix);
        } else {
            finalMatrix->DeepCopy(icpMatrix);
        }

        // 获取 ICP 误差
        double meanDistance = icp->GetMeanDistance();

        // 保存配准记录
        RegistrationRecord record;
        record.registrationId = regId;
        record.type = "icp";
        record.transform = finalMatrix;
        record.timestamp = QDateTime::currentMSecsSinceEpoch();
        record.fre = meanDistance;
        record.numPoints = source->GetNumberOfPoints();

        QVariantMap metadata;
        metadata["maxIterations"] = maxIterations;
        metadata["meanDistance"] = meanDistance;
        metadata["sourcePoints"] = static_cast<int>(source->GetNumberOfPoints());
        metadata["targetPoints"] = static_cast<int>(target->GetNumberOfPoints());
        record.metadata = metadata;

        saveRecord(regId, record);

        qDebug() << "[RegistrationService] ICP registration completed:"
                 << "ID=" << regId
                 << "Iterations=" << maxIterations
                 << "MeanDistance=" << meanDistance << "mm";

        // 发射完成信号
        QVariantMap result;
        result["registrationId"] = regId;
        result["type"] = "icp";
        result["meanDistance"] = meanDistance;
        result["maxIterations"] = maxIterations;
        emit registrationCompleted(regId, result);

        // 质量检查
        if (meanDistance > 3.0) {
            QVariantMap quality;
            quality["meanDistance"] = meanDistance;
            emit registrationQualityWarning(regId, quality,
                QString("High ICP mean distance: %1 mm").arg(meanDistance, 0, 'f', 2));
        }

        return finalMatrix;

    } catch (const std::exception& ex) {
        m_lastError = QString("ICP registration failed: %1").arg(ex.what());
        qCritical() << "[RegistrationService]" << m_lastError;
        emit registrationFailed(regId, m_lastError);
        return nullptr;
    }
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::performICPRegistrationAdvanced(
    vtkPolyData* source,
    vtkPolyData* target,
    const QVariantMap& parameters)
{
    if (!source || !target) {
        m_lastError = "Source or target mesh is null";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    if (source->GetNumberOfPoints() == 0 || target->GetNumberOfPoints() == 0) {
        m_lastError = "Source or target mesh has no points";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    // GPU-GICP 路径：如果请求 useGPU 且 DLL 可用
    bool useGPU = parameters.value("useGPU", false).toBool();
    QVariantMap effectiveParameters = parameters;
    if (useGPU) {
        qDebug() << "[RegistrationService] Advanced ICP requested GPU refinement:"
                 << "sourcePoints=" << source->GetNumberOfPoints()
                 << "targetPoints=" << target->GetNumberOfPoints()
                 << "registrationId=" << parameters.value("registrationId").toString();
        if (!m_meshGPULoaded) {
            loadMeshGPUDLL();
        }
        if (m_meshGPULoaded) {
            QString registrationId = parameters.value("registrationId", QString()).toString();
            if (registrationId.isEmpty()) {
                registrationId = generateRegistrationId("gicp");
            }

            // 解析初始变换
            vtkSmartPointer<vtkMatrix4x4> initialMatrix = nullptr;
            if (parameters.contains("initialTransform")) {
                QList<QVariant> matrixList = parameters.value("initialTransform").toList();
                if (matrixList.size() == 16) {
                    QList<double> matrixValues;
                    for (const QVariant& v : matrixList) {
                        matrixValues.append(v.toDouble());
                    }
                    initialMatrix = listToMatrix(matrixValues);
                }
            }
            const bool enableParallelInitialSearch =
                parameters.value(QStringLiteral("enableParallelInitialSearch"), false).toBool()
                && parameters.value(QStringLiteral("registrationMethodId")).toString()
                    == QStringLiteral("ankle_two_stage_constrained");
            const bool canRunParallelInitialSearch =
                enableParallelInitialSearch && initialMatrix && m_meshGPUCandidateScoringAvailable;
            QList<CandidateInitialTransform> candidateTransforms;
            QList<CandidateEvaluationResult> candidateScores;
            QList<CandidateEvaluationResult> topKCandidateScores;
            QElapsedTimer coarseSearchTimer;
            QElapsedTimer refineTimer;
            QElapsedTimer parallelSearchTotalTimer;
            qint64 roiFilterMs = 0;
            qint64 refineMs = 0;
            int refineCandidateCount = 0;
            int bestCandidateRank = -1;
            int parallelScoredCandidateCount = 0;
            const bool batchRefineRequested = enableParallelInitialSearch && initialMatrix;
            bool batchRefineEnabled = false;
            QString batchRefineFallback;
            bool runtimeSourceConstraintFilterUsed = false;
            bool preparedRuntimeStateForBatchRefine = false;
            float preparedRuntimeStateCellSize = 0.0f;
            QVariantMap gpuParameters = parameters;
            if (initialMatrix && !gpuParameters.contains(QStringLiteral("pairedResidualGuardFallbackTransform"))) {
                gpuParameters.insert(
                    QStringLiteral("pairedResidualGuardFallbackTransform"),
                    parameters.value(QStringLiteral("initialTransform")));
            }

            if (canRunParallelInitialSearch) {
                parallelSearchTotalTimer.start();
                ParallelSearchPlan plan;
                plan.candidateCount = parameters.value(QStringLiteral("candidateCount"), 64).toInt();
                plan.topKCount = parameters.value(QStringLiteral("topKCandidateCount"), 4).toInt();
                plan.multiResolutionProfileId = parameters.value(
                    QStringLiteral("multiResolutionProfileId"),
                    QStringLiteral("ankle_roi_two_level")).toString();
                const QList<double> multiResolutionCellSizes =
                    resolveMultiResolutionCellSizes(plan.multiResolutionProfileId);
                const bool constraintParallelFilterEnabled =
                    parameters.value(QStringLiteral("enableConstraintParallelFilter"), false).toBool();

                const QVector3D targetRegionCenter(
                    parameters.value(QStringLiteral("targetRegionCenterX"), 0.0).toFloat(),
                    parameters.value(QStringLiteral("targetRegionCenterY"), 0.0).toFloat(),
                    parameters.value(QStringLiteral("targetRegionCenterZ"), 0.0).toFloat());
                const float cellSize = parameters.value(QStringLiteral("cellSize"), 1.0).toFloat();
                const QString targetMeshPath = parameters.value(QStringLiteral("targetMeshPath")).toString();
                const QMap<QString, QList<QVector3D>> constraintRegions =
                    constraintRegionsFromVariant(parameters.value(QStringLiteral("constraintRegions")).toMap());
                const QList<QVector3D> flattenedConstraintPoints = flattenConstraintRegions(constraintRegions);
                const double targetRegionRadiusMm =
                    parameters.value(QStringLiteral("targetRegionRadiusMm"), 0.0).toDouble();
                const QVector3D effectiveTargetRegionCenter =
                    targetRegionRadiusMm > 0.0 ? targetRegionCenter : centroidOfPoints(flattenedConstraintPoints);
                const bool initialAdmissionGateEnabled =
                    parameters.value(QStringLiteral("enableInitialAdmissionGate"), true).toBool();
                const InitialAdmissionPolicy initialAdmissionPolicy =
                    initialAdmissionPolicyFromParameters(parameters);
                const InitialAdmissionEvidence initialAdmissionEvidence =
                    initialAdmissionEvidenceFromParameters(parameters);
                if (initialAdmissionGateEnabled
                    && initialAdmissionEvidence.hasRobustInitialMetrics
                    && initialAdmissionEvidence.robustInitialRmsMm
                        > initialAdmissionPolicy.robustResidualRecoveryMaxMm) {
                    CandidateEvaluationResult identityCandidateScore;
                    identityCandidateScore.candidateId = QStringLiteral("candidate_000");
                    identityCandidateScore.coarseScore = initialAdmissionEvidence.robustInitialRmsMm;
                    identityCandidateScore.converged = false;

                    const InitialAdmissionDecision initialAdmissionDecision =
                        assessInitialAdmission(
                            identityCandidateScore,
                            QList<CandidateEvaluationResult> { identityCandidateScore },
                            initialAdmissionPolicy,
                            initialAdmissionEvidence);

                    QVariantMap parallelSearchReport = buildParallelSearchReport(
                        candidateTransforms,
                        QList<CandidateEvaluationResult> { identityCandidateScore },
                        parameters,
                        0,
                        constraintParallelFilterEnabled,
                        0,
                        0,
                        0,
                        0,
                        0);
                    mergeInitialAdmissionDecision(parallelSearchReport, initialAdmissionDecision);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelSearchEnabled"),
                        false);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelSearchRequested"),
                        true);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelSearchFallback"),
                        QStringLiteral("initial_admission_rejected_before_gpu_scoring"));
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedConstraintApplied"),
                        false);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedSourcePointCount"),
                        static_cast<int>(source->GetNumberOfPoints()));
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedTargetPointCount"),
                        static_cast<int>(target->GetNumberOfPoints()));
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedTargetTriangleCount"),
                        static_cast<int>(target->GetNumberOfCells()));
                    parallelSearchReport.insert(
                        QStringLiteral("parallelSearchTotalMs"),
                        parallelSearchTotalTimer.isValid() ? parallelSearchTotalTimer.elapsed() : 0);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelScoredCandidateCount"),
                        0);
                    parallelSearchReport.insert(QStringLiteral("batchRefineRequested"), batchRefineRequested);
                    parallelSearchReport.insert(QStringLiteral("batchRefineEnabled"), false);
                    parallelSearchReport.insert(
                        QStringLiteral("batchRefineFallback"),
                        QStringLiteral("initial_admission_rejected"));
                    parallelSearchReport.insert(
                        QStringLiteral("runtimeSourceConstraintFilterUsed"),
                        false);

                    RegistrationRecord record;
                    record.registrationId = registrationId;
                    record.type = QStringLiteral("gicp_rejected");
                    record.transform = vtkSmartPointer<vtkMatrix4x4>::New();
                    record.transform->DeepCopy(initialMatrix);
                    record.timestamp = QDateTime::currentMSecsSinceEpoch();
                    record.fre = initialAdmissionDecision.robustInitialRmsMm;
                    record.numPoints = static_cast<int>(source->GetNumberOfPoints());

                    QVariantMap metadata;
                    metadata.insert(QStringLiteral("algorithm"), QStringLiteral("GPU-GICP"));
                    metadata.insert(QStringLiteral("converged"), false);
                    metadata.insert(QStringLiteral("rmse"), initialAdmissionDecision.robustInitialRmsMm);
                    metadata.insert(QStringLiteral("elapsedMs"), 0);
                    metadata.insert(QStringLiteral("sourcePoints"), static_cast<int>(source->GetNumberOfPoints()));
                    metadata.insert(QStringLiteral("targetPoints"), static_cast<int>(target->GetNumberOfPoints()));
                    metadata.insert(
                        QStringLiteral("finalResultSource"),
                        QStringLiteral("parallel_initial_admission_rejected"));
                    metadata.insert(QStringLiteral("registrationRejected"), true);
                    metadata.unite(parallelSearchReport);
                    metadata.insert(
                        QStringLiteral("pipelineElapsedMs"),
                        metadata.value(QStringLiteral("parallelSearchTotalMs")).toLongLong());
                    record.metadata = metadata;
                    saveRecord(registrationId, record);

                    m_lastError = QStringLiteral("Initial admission rejected: ")
                        + initialAdmissionDecision.reason;
                    emit registrationFailed(registrationId, m_lastError);
                    return nullptr;
                }
                bool earlyCpuIdentityProbeAccepted = false;
                const bool cpuIdentityProbeFastPathEnabled =
                    parameters.value(QStringLiteral("enableCpuIdentityProbeFastPath"), true).toBool()
                    && parameters.value(QStringLiteral("enableConfidentInitialFastPath"), true).toBool();
                if (cpuIdentityProbeFastPathEnabled && plan.candidateCount >= 64) {
                    std::vector<mesh_gpu::Point3D> sourcePoints(source->GetNumberOfPoints());
                    for (vtkIdType i = 0; i < source->GetNumberOfPoints(); ++i) {
                        double point[3];
                        source->GetPoint(i, point);
                        sourcePoints[static_cast<size_t>(i)] = {
                            static_cast<float>(point[0]),
                            static_cast<float>(point[1]),
                            static_cast<float>(point[2])
                        };
                    }

                    std::vector<mesh_gpu::Point3D> transformedSourcePoints = sourcePoints;
                    for (vtkIdType i = 0; i < source->GetNumberOfPoints(); ++i) {
                        double point[3];
                        source->GetPoint(i, point);
                        double point4[4] = { point[0], point[1], point[2], 1.0 };
                        double result4[4];
                        initialMatrix->MultiplyPoint(point4, result4);
                        transformedSourcePoints[static_cast<size_t>(i)] = {
                            static_cast<float>(result4[0]),
                            static_cast<float>(result4[1]),
                            static_cast<float>(result4[2])
                        };
                    }

                    std::vector<mesh_gpu::Point3D> activeSourcePoints = transformedSourcePoints;
                    const bool hasSourceConstraintFiltering =
                        !flattenedConstraintPoints.isEmpty() || targetRegionRadiusMm > 0.0;
                    if (hasSourceConstraintFiltering) {
                        const QList<mesh_gpu::Point3D> constrainedSourcePoints =
                            buildConstrainedSourcePointCloud(
                                source,
                                flattenedConstraintPoints,
                                targetRegionCenter,
                                targetRegionRadiusMm,
                                initialMatrix,
                                3);
                        if (!constrainedSourcePoints.isEmpty()) {
                            activeSourcePoints.assign(
                                constrainedSourcePoints.cbegin(),
                                constrainedSourcePoints.cend());
                        }
                    }

                    coarseSearchTimer.start();
                    const double cutoffMm =
                        parameters.value(QStringLiteral("candidateScoreCutoffMm"), 12.0).toDouble();
                    const double cpuProbeCellSizeMm =
                        parameters.value(QStringLiteral("cpuIdentityProbeCellSizeMm"), 2.5).toDouble();
                    const double cpuProbeSourceBoundsPaddingMm =
                        parameters.value(
                            QStringLiteral("cpuIdentityProbeSourceBoundsPaddingMm"),
                            cutoffMm + cpuProbeCellSizeMm * 2.0).toDouble();
                    int cpuProbeTargetPointCount = 0;
                    const std::vector<mesh_gpu::Point3D> cpuProbeTargetVertices =
                        buildConstrainedTargetVertexCloud(
                            target,
                            flattenedConstraintPoints,
                            effectiveTargetRegionCenter,
                            targetRegionRadiusMm,
                            activeSourcePoints,
                            cpuProbeSourceBoundsPaddingMm,
                            &cpuProbeTargetPointCount);
                    const double parallelCpuIdentityProbeScoreMm =
                        truncatedNearestVertexRmseMm(
                            activeSourcePoints,
                            cpuProbeTargetVertices,
                            cutoffMm,
                            cpuProbeCellSizeMm);
                    parallelScoredCandidateCount = 1;
                    const QList<QVector3D> activeSourcePointVectors =
                        meshGpuPointListToVectorList(activeSourcePoints);
                    QMatrix4x4 identityDelta;
                    identityDelta.setToIdentity();
                    const CandidateRegionMetrics regionMetrics = evaluateCandidateRegionMetrics(
                        identityDelta,
                        activeSourcePointVectors,
                        flattenedConstraintPoints,
                        effectiveTargetRegionCenter,
                        targetRegionRadiusMm);

                    CandidateEvaluationResult identityCandidateScore;
                    identityCandidateScore.candidateId = QStringLiteral("candidate_000");
                    identityCandidateScore.coarseScore = parallelCpuIdentityProbeScoreMm;
                    identityCandidateScore.targetRegionHitRatio = regionMetrics.hitRatio;
                    identityCandidateScore.coverageScore = regionMetrics.coverageScore;
                    identityCandidateScore.converged = true;
                    identityCandidateScore.multiResolutionLevel = 0;
                    identityCandidateScore.normalConsistencyScore = 1.0;
                    identityCandidateScore.curvatureScore = 1.0;
                    const InitialAdmissionDecision initialAdmissionDecision =
                        assessInitialAdmission(
                            identityCandidateScore,
                            QList<CandidateEvaluationResult> { identityCandidateScore },
                            initialAdmissionPolicyFromParameters(parameters),
                            initialAdmissionEvidenceFromParameters(parameters));
                    if (initialAdmissionDecision.action == QStringLiteral("fast_path")) {
                        candidateTransforms = buildCandidateInitialTransforms(
                            vtkMatrix4x4ToQMatrix(initialMatrix),
                            targetRegionCenter,
                            plan);

                        candidateScores.append(identityCandidateScore);
                        topKCandidateScores.append(identityCandidateScore);

                        QVariantMap batchRefineMetrics;
                        mergeInitialAdmissionDecision(batchRefineMetrics, initialAdmissionDecision);
                        batchRefineMetrics.insert(
                            QStringLiteral("adaptiveRefineCandidateSelectionEnabled"),
                            parameters.value(
                                QStringLiteral("enableAdaptiveRefineCandidateSelection"),
                                true).toBool());
                        batchRefineMetrics.insert(
                            QStringLiteral("adaptiveRefineCandidateSelectionApplied"),
                            false);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathApplied"),
                            true);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathMaxCoarseScoreMm"),
                            initialAdmissionPolicyFromParameters(parameters).fastPathMaxCoarseScoreMm);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathIdentityScoreWindowMm"),
                            parameters.value(
                                QStringLiteral("confidentInitialFastPathIdentityScoreWindowMm"),
                                0.35).toDouble());
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathBestCoarseScoreMm"),
                            parallelCpuIdentityProbeScoreMm);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathIdentityCoarseScoreMm"),
                            parallelCpuIdentityProbeScoreMm);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathStoppedAfterCoarseLevel"),
                            true);
                        batchRefineMetrics.insert(
                            QStringLiteral("bestBatchRefineCandidateId"),
                            QStringLiteral("candidate_000"));
                        batchRefineMetrics.insert(
                            QStringLiteral("bestBatchRefineSelectionRank"),
                            0);
                        batchRefineMetrics.insert(
                            QStringLiteral("bestBatchRefineTransformApplied"),
                            true);
                        batchRefineMetrics.insert(
                            QStringLiteral("bestBatchRefineRmse"),
                            parallelCpuIdentityProbeScoreMm);
                        batchRefineMetrics.insert(
                            QStringLiteral("precomputedFinalResultSource"),
                            QStringLiteral("parallel_confident_initial"));

                        QVariantList precomputedTransform;
                        const QList<double> transformValues = matrixToList(initialMatrix);
                        precomputedTransform.reserve(transformValues.size());
                        for (double value : transformValues) {
                            precomputedTransform.append(value);
                        }
                        gpuParameters.insert(QStringLiteral("useBatchRefineAsFinalResult"), true);
                        gpuParameters.insert(QStringLiteral("batchRefineFinalTransform"), precomputedTransform);
                        gpuParameters.insert(
                            QStringLiteral("batchRefineFinalRmse"),
                            parallelCpuIdentityProbeScoreMm);
                        gpuParameters.insert(QStringLiteral("batchRefineFinalIterations"), 0);
                        gpuParameters.insert(QStringLiteral("batchRefineFinalConverged"), true);

                        QVariantMap parallelSearchReport = buildParallelSearchReport(
                            candidateTransforms,
                            topKCandidateScores,
                            parameters,
                            coarseSearchTimer.elapsed(),
                            constraintParallelFilterEnabled,
                            0,
                            1,
                            0,
                            0,
                            0,
                            batchRefineMetrics);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelPrecomputedConstraintApplied"),
                            hasSourceConstraintFiltering
                                || activeSourcePoints.size() != transformedSourcePoints.size()
                                || cpuProbeTargetPointCount != target->GetNumberOfPoints());
                        parallelSearchReport.insert(
                            QStringLiteral("parallelPrecomputedSourcePointCount"),
                            static_cast<int>(activeSourcePoints.size()));
                        parallelSearchReport.insert(
                            QStringLiteral("parallelPrecomputedTargetPointCount"),
                            cpuProbeTargetPointCount);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelPrecomputedTargetTriangleCount"),
                            0);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelSearchTotalMs"),
                            parallelSearchTotalTimer.isValid() ? parallelSearchTotalTimer.elapsed() : 0);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelScoredCandidateCount"),
                            parallelScoredCandidateCount);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelCpuIdentityProbeUsed"),
                            true);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelCpuIdentityProbeScoreMm"),
                            parallelCpuIdentityProbeScoreMm);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelCpuIdentityProbeTargetPointCount"),
                            cpuProbeTargetPointCount);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelCpuIdentityProbeEarlyAccepted"),
                            true);
                        parallelSearchReport.insert(QStringLiteral("batchRefineRequested"), batchRefineRequested);
                        parallelSearchReport.insert(QStringLiteral("batchRefineEnabled"), false);
                        parallelSearchReport.insert(
                            QStringLiteral("batchRefineFallback"),
                            QStringLiteral("confident_initial_fast_path"));
                        parallelSearchReport.insert(
                            QStringLiteral("runtimeSourceConstraintFilterUsed"),
                            false);
                        gpuParameters.insert(QStringLiteral("parallelSearchReport"), parallelSearchReport);
                        earlyCpuIdentityProbeAccepted = true;
                    }
                }
                if (!earlyCpuIdentityProbeAccepted) {
                QElapsedTimer roiFilterTimer;
                roiFilterTimer.start();
                int coarseConstraintTargetPointCount = 0;
                int coarseConstraintTargetTriangleCount = 0;
                vtkSmartPointer<vtkPolyData> constrainedTarget;
                mesh_gpu::ConstrainedMeshResult constrainedTargetMesh;
                bool hasRuntimeConstrainedTargetMesh = false;
                if (!flattenedConstraintPoints.isEmpty() || targetRegionRadiusMm > 0.0) {
                    if (constraintParallelFilterEnabled) {
                        constrainedTargetMesh = m_meshGPU->buildConstrainedTargetMesh(
                            mesh_gpu::Point3D(
                                effectiveTargetRegionCenter.x(),
                                effectiveTargetRegionCenter.y(),
                                effectiveTargetRegionCenter.z()),
                            static_cast<float>(targetRegionRadiusMm),
                            static_cast<float>(constraintMembershipRadiusMm(targetRegionRadiusMm)),
                            toStdVector(flattenedConstraintPoints),
                            3);
                        if (constrainedTargetMesh.success) {
                            constrainedTarget = buildTargetPolyDataFromRuntimeMesh(
                                constrainedTargetMesh,
                                &coarseConstraintTargetPointCount,
                                &coarseConstraintTargetTriangleCount);
                            hasRuntimeConstrainedTargetMesh =
                                constrainedTarget
                                && coarseConstraintTargetPointCount >= 3
                                && coarseConstraintTargetTriangleCount > 0;
                        }
                    }

                    if (!constrainedTarget) {
                        constrainedTarget = buildConstrainedTargetPolyData(
                            target,
                            flattenedConstraintPoints,
                            effectiveTargetRegionCenter,
                            targetRegionRadiusMm,
                            &coarseConstraintTargetPointCount,
                            &coarseConstraintTargetTriangleCount);
                    }
                }

                vtkPolyData* activeTarget =
                    constrainedTarget
                        && coarseConstraintTargetPointCount >= 3
                        && coarseConstraintTargetTriangleCount > 0
                    ? constrainedTarget
                    : target;

                const bool hasSourceConstraintFiltering =
                    !flattenedConstraintPoints.isEmpty() || targetRegionRadiusMm > 0.0;
                const QList<mesh_gpu::Point3D> constrainedSourcePoints =
                    hasSourceConstraintFiltering && !constraintParallelFilterEnabled
                    ? buildConstrainedSourcePointCloud(
                        source,
                        flattenedConstraintPoints,
                        targetRegionCenter,
                        targetRegionRadiusMm,
                        initialMatrix,
                        3)
                    : QList<mesh_gpu::Point3D> {};
                roiFilterMs = roiFilterTimer.elapsed();

                coarseSearchTimer.start();
                std::vector<mesh_gpu::Point3D> targetVertices;
                std::vector<mesh_gpu::Normal3D> targetNormals;
                std::vector<std::array<int, 3>> targetTriangles;

                if (hasRuntimeConstrainedTargetMesh) {
                    targetVertices = constrainedTargetMesh.vertices;
                    targetNormals = constrainedTargetMesh.normals;
                    targetTriangles = constrainedTargetMesh.triangles;
                } else {
                    targetVertices.resize(activeTarget->GetNumberOfPoints());
                    targetNormals.resize(activeTarget->GetNumberOfPoints());
                    targetTriangles.reserve(static_cast<size_t>(activeTarget->GetNumberOfCells()));

                    for (vtkIdType i = 0; i < activeTarget->GetNumberOfPoints(); ++i) {
                        double point[3];
                        activeTarget->GetPoint(i, point);
                        targetVertices[static_cast<size_t>(i)] = {
                            static_cast<float>(point[0]),
                            static_cast<float>(point[1]),
                            static_cast<float>(point[2])
                        };
                    }

                    vtkDataArray* normalArray =
                        activeTarget->GetPointData() ? activeTarget->GetPointData()->GetNormals() : nullptr;
                    if (normalArray) {
                        for (vtkIdType i = 0; i < activeTarget->GetNumberOfPoints(); ++i) {
                            double normal[3];
                            normalArray->GetTuple(i, normal);
                            targetNormals[static_cast<size_t>(i)] = {
                                static_cast<float>(normal[0]),
                                static_cast<float>(normal[1]),
                                static_cast<float>(normal[2])
                            };
                        }
                    }

                    for (vtkIdType i = 0; i < activeTarget->GetNumberOfCells(); ++i) {
                        vtkCell* cell = activeTarget->GetCell(i);
                        if (cell && cell->GetNumberOfPoints() == 3) {
                            targetTriangles.push_back({
                                static_cast<int>(cell->GetPointId(0)),
                                static_cast<int>(cell->GetPointId(1)),
                                static_cast<int>(cell->GetPointId(2))
                            });
                        }
                    }
                }

                std::vector<mesh_gpu::Point3D> sourcePoints(source->GetNumberOfPoints());
                for (vtkIdType i = 0; i < source->GetNumberOfPoints(); ++i) {
                    double point[3];
                    source->GetPoint(i, point);
                    sourcePoints[static_cast<size_t>(i)] = {
                        static_cast<float>(point[0]),
                        static_cast<float>(point[1]),
                        static_cast<float>(point[2])
                    };
                }

                std::vector<mesh_gpu::Point3D> transformedSourcePoints = sourcePoints;
                if (initialMatrix) {
                    for (vtkIdType i = 0; i < source->GetNumberOfPoints(); ++i) {
                        double point[3];
                        source->GetPoint(i, point);
                        double point4[4] = { point[0], point[1], point[2], 1.0 };
                        double result4[4];
                        initialMatrix->MultiplyPoint(point4, result4);
                        transformedSourcePoints[static_cast<size_t>(i)] = {
                            static_cast<float>(result4[0]),
                            static_cast<float>(result4[1]),
                            static_cast<float>(result4[2])
                        };
                    }
                }

                std::vector<mesh_gpu::Point3D> activeSourcePoints = transformedSourcePoints;
                if (constraintParallelFilterEnabled && hasSourceConstraintFiltering) {
                    const mesh_gpu::RuntimeConstraintFilterResult runtimeFilterResult = m_meshGPU->filterSourcePointsByConstraints(
                        transformedSourcePoints,
                        mesh_gpu::Point3D(
                            effectiveTargetRegionCenter.x(),
                            effectiveTargetRegionCenter.y(),
                            effectiveTargetRegionCenter.z()),
                        static_cast<float>(targetRegionRadiusMm),
                        static_cast<float>(constraintMembershipRadiusMm(targetRegionRadiusMm)),
                        toStdVector(flattenedConstraintPoints),
                        3);
                    if (runtimeFilterResult.success && !runtimeFilterResult.selectedIndices.empty()) {
                        activeSourcePoints.clear();
                        activeSourcePoints.reserve(runtimeFilterResult.selectedIndices.size());
                        for (size_t selectedIndexOffset = 0;
                             selectedIndexOffset < runtimeFilterResult.selectedIndices.size();
                             ++selectedIndexOffset) {
                            const int selectedIndex = runtimeFilterResult.selectedIndices[selectedIndexOffset];
                            if (selectedIndex >= 0 && selectedIndex < static_cast<int>(transformedSourcePoints.size())) {
                                activeSourcePoints.push_back(transformedSourcePoints[static_cast<size_t>(selectedIndex)]);
                            }
                        }
                        runtimeSourceConstraintFilterUsed = true;
                    }
                    if (!runtimeSourceConstraintFilterUsed && !constrainedSourcePoints.isEmpty()) {
                        activeSourcePoints.assign(constrainedSourcePoints.cbegin(), constrainedSourcePoints.cend());
                    }
                } else if (!constrainedSourcePoints.isEmpty()) {
                    activeSourcePoints.assign(constrainedSourcePoints.cbegin(), constrainedSourcePoints.cend());
                }

                const QList<QVector3D> activeSourcePointVectors = meshGpuPointListToVectorList(activeSourcePoints);
                candidateTransforms = buildCandidateInitialTransforms(
                    vtkMatrix4x4ToQMatrix(initialMatrix),
                    targetRegionCenter,
                    plan);

                vtkSmartPointer<vtkMatrix4x4> inverseInitialMatrix = invertMatrix(initialMatrix);
                QList<CandidateInitialTransform> deltaCandidates;
                deltaCandidates.reserve(candidateTransforms.size());
                for (const CandidateInitialTransform& candidateTransform : candidateTransforms) {
                    CandidateInitialTransform deltaCandidate = candidateTransform;
                    vtkSmartPointer<vtkMatrix4x4> absoluteCandidateMatrix =
                        qMatrix4x4ToVtkMatrix(candidateTransform.transformMatrix);
                    vtkSmartPointer<vtkMatrix4x4> deltaMatrix = multiplyMatrix(
                        absoluteCandidateMatrix,
                        inverseInitialMatrix);
                    deltaCandidate.transformMatrix = vtkMatrix4x4ToQMatrix(deltaMatrix);
                    deltaCandidates.append(deltaCandidate);
                }

                bool parallelCpuIdentityProbeUsed = false;
                double parallelCpuIdentityProbeScoreMm = std::numeric_limits<double>::max();
                QList<CandidateInitialTransform> activeCandidates = deltaCandidates;
                int executedMultiResolutionLevelCount = 0;
                const bool cpuIdentityProbeFastPathEnabled =
                    parameters.value(QStringLiteral("enableCpuIdentityProbeFastPath"), true).toBool()
                    && parameters.value(QStringLiteral("enableConfidentInitialFastPath"), true).toBool();
                if (cpuIdentityProbeFastPathEnabled && plan.candidateCount >= 64) {
                    const double cutoffMm =
                        parameters.value(QStringLiteral("candidateScoreCutoffMm"), 12.0).toDouble();
                    const double cpuProbeCellSizeMm =
                        parameters.value(QStringLiteral("cpuIdentityProbeCellSizeMm"), 2.5).toDouble();
                    parallelCpuIdentityProbeScoreMm =
                        truncatedNearestVertexRmseMm(
                            activeSourcePoints,
                            targetVertices,
                            cutoffMm,
                            cpuProbeCellSizeMm);
                    parallelScoredCandidateCount = 1;
                    const double confidentInitialFastPathMaxCoarseScoreMm =
                        parameters.value(
                            QStringLiteral("confidentInitialFastPathMaxCoarseScoreMm"),
                            1.0).toDouble();
                    if (parallelCpuIdentityProbeScoreMm <= confidentInitialFastPathMaxCoarseScoreMm) {
                        QMatrix4x4 identityDelta;
                        identityDelta.setToIdentity();
                        const CandidateRegionMetrics regionMetrics = evaluateCandidateRegionMetrics(
                            identityDelta,
                            activeSourcePointVectors,
                            flattenedConstraintPoints,
                            effectiveTargetRegionCenter,
                            targetRegionRadiusMm);

                        CandidateEvaluationResult identityCandidateScore;
                        identityCandidateScore.candidateId = QStringLiteral("candidate_000");
                        identityCandidateScore.coarseScore = parallelCpuIdentityProbeScoreMm;
                        identityCandidateScore.targetRegionHitRatio = regionMetrics.hitRatio;
                        identityCandidateScore.coverageScore = regionMetrics.coverageScore;
                        identityCandidateScore.converged = true;
                        identityCandidateScore.multiResolutionLevel = 0;
                        identityCandidateScore.normalConsistencyScore = 1.0;
                        identityCandidateScore.curvatureScore = 1.0;
                        candidateScores.append(identityCandidateScore);
                        topKCandidateScores.append(identityCandidateScore);
                        executedMultiResolutionLevelCount = 1;
                        parallelCpuIdentityProbeUsed = true;
                        activeCandidates.clear();
                    }
                }
                for (int levelIndex = 0; levelIndex < multiResolutionCellSizes.size() && !activeCandidates.isEmpty(); ++levelIndex) {
                    QVariantMap levelParameters = parameters;
                    levelParameters.insert(QStringLiteral("cellSize"), multiResolutionCellSizes.at(levelIndex));
                    const float levelCellSize = static_cast<float>(multiResolutionCellSizes.at(levelIndex));
                    const bool targetPrepared =
                        !targetMeshPath.isEmpty() && activeTarget == target
                        ? m_meshGPU->loadTargetMesh(targetMeshPath.toStdString(), levelCellSize)
                        : m_meshGPU->setTargetMesh(
                            targetVertices,
                            targetNormals,
                            targetTriangles,
                            levelCellSize);
                    const bool sourcePrepared = targetPrepared && m_meshGPU->setSourcePointCloud(activeSourcePoints);
                    if (!sourcePrepared) {
                        qWarning() << "[RegistrationService] Parallel search skipped because MeshGPU runtime preparation failed"
                                   << "registrationId=" << registrationId
                                   << "levelIndex=" << levelIndex
                                   << "targetPrepared=" << targetPrepared
                                   << "sourcePrepared=" << sourcePrepared;
                        activeCandidates.clear();
                        break;
                    }
                    preparedRuntimeStateForBatchRefine = true;
                    preparedRuntimeStateCellSize = levelCellSize;

                    ++executedMultiResolutionLevelCount;
                    const QString identityCandidateId = QStringLiteral("candidate_000");
                    const bool identityProbeFastPathEnabled =
                        parameters.value(QStringLiteral("enableIdentityProbeFastPath"), true).toBool();
                    bool identityProbeAccepted = false;
                    if (identityProbeFastPathEnabled && levelIndex == 0 && plan.candidateCount >= 64) {
                        const QList<CandidateInitialTransform> identityCandidates =
                            filterCandidatesByIds(activeCandidates, QStringList { identityCandidateId });
                        const QList<CandidateEvaluationResult> identityCandidateScores =
                            evaluateCandidateTransformsGpu(
                                identityCandidates,
                                activeSourcePointVectors,
                                flattenedConstraintPoints,
                                effectiveTargetRegionCenter,
                                targetRegionRadiusMm,
                                levelParameters);
                        parallelScoredCandidateCount += identityCandidateScores.size();
                        if (!identityCandidateScores.isEmpty()) {
                            const double confidentInitialFastPathMaxCoarseScoreMm =
                                parameters.value(
                                    QStringLiteral("confidentInitialFastPathMaxCoarseScoreMm"),
                                    1.0).toDouble();
                            if (identityCandidateScores.first().coarseScore
                                <= confidentInitialFastPathMaxCoarseScoreMm) {
                                candidateScores = identityCandidateScores;
                                topKCandidateScores = identityCandidateScores;
                                identityProbeAccepted = true;
                            }
                        }
                    }
                    if (identityProbeAccepted) {
                        break;
                    }

                    candidateScores = evaluateCandidateTransformsGpu(
                        activeCandidates,
                        activeSourcePointVectors,
                        flattenedConstraintPoints,
                        effectiveTargetRegionCenter,
                        targetRegionRadiusMm,
                        levelParameters);
                    parallelScoredCandidateCount += candidateScores.size();
                    const int levelTopKCount =
                        levelIndex == multiResolutionCellSizes.size() - 1
                        ? plan.topKCount
                        : qMin(plan.topKCount * 2, candidateScores.size());
                    topKCandidateScores = selectTopKCandidates(candidateScores, levelTopKCount);
                    if (levelIndex < multiResolutionCellSizes.size() - 1) {
                        const bool topKContainsIdentityCandidate = std::any_of(
                            topKCandidateScores.cbegin(),
                            topKCandidateScores.cend(),
                            [&identityCandidateId](const CandidateEvaluationResult& result) {
                                return result.candidateId == identityCandidateId;
                            });
                        if (!topKContainsIdentityCandidate) {
                            for (const CandidateEvaluationResult& candidateScore : candidateScores) {
                                if (candidateScore.candidateId != identityCandidateId) {
                                    continue;
                                }
                                if (topKCandidateScores.size() >= levelTopKCount
                                    && !topKCandidateScores.isEmpty()) {
                                    topKCandidateScores.removeLast();
                                }
                                topKCandidateScores.append(candidateScore);
                                break;
                            }
                        }
                        const bool earlyConfidentInitialFastPathEnabled =
                            parameters.value(
                                QStringLiteral("enableEarlyConfidentInitialFastPath"),
                                true).toBool();
                        if (earlyConfidentInitialFastPathEnabled && plan.candidateCount >= 64) {
                            int identityCandidateTopKIndex = -1;
                            for (int topKIndex = 0; topKIndex < topKCandidateScores.size(); ++topKIndex) {
                                if (topKCandidateScores.at(topKIndex).candidateId == identityCandidateId) {
                                    identityCandidateTopKIndex = topKIndex;
                                    break;
                                }
                            }
                            const double confidentInitialFastPathMaxCoarseScoreMm =
                                parameters.value(
                                    QStringLiteral("confidentInitialFastPathMaxCoarseScoreMm"),
                                    1.0).toDouble();
                            const double confidentInitialFastPathIdentityScoreWindowMm =
                                parameters.value(
                                    QStringLiteral("confidentInitialFastPathIdentityScoreWindowMm"),
                                    0.35).toDouble();
                            const double bestCoarseScore =
                                !topKCandidateScores.isEmpty()
                                    ? topKCandidateScores.first().coarseScore
                                    : std::numeric_limits<double>::max();
                            const double identityCoarseScore =
                                identityCandidateTopKIndex >= 0
                                    ? topKCandidateScores.at(identityCandidateTopKIndex).coarseScore
                                    : std::numeric_limits<double>::max();
                            if (identityCandidateTopKIndex >= 0
                                && identityCoarseScore <= confidentInitialFastPathMaxCoarseScoreMm
                                && identityCoarseScore
                                    <= bestCoarseScore + confidentInitialFastPathIdentityScoreWindowMm) {
                                break;
                            }
                        }
                        activeCandidates = filterCandidatesByIds(
                            activeCandidates,
                            candidateIds(topKCandidateScores));
                    }
                }

                if (!topKCandidateScores.isEmpty()) {
                    if (topKCandidateScores.size() > plan.topKCount) {
                        topKCandidateScores = selectTopKCandidates(topKCandidateScores, plan.topKCount);
                    }
                    const QString identityCandidateId = QStringLiteral("candidate_000");
                    const bool topKContainsIdentityCandidate = std::any_of(
                        topKCandidateScores.cbegin(),
                        topKCandidateScores.cend(),
                        [&identityCandidateId](const CandidateEvaluationResult& result) {
                            return result.candidateId == identityCandidateId;
                        });
                    if (!topKContainsIdentityCandidate) {
                        for (const CandidateEvaluationResult& candidateScore : candidateScores) {
                            if (candidateScore.candidateId != identityCandidateId) {
                                continue;
                            }
                            if (topKCandidateScores.size() >= plan.topKCount && !topKCandidateScores.isEmpty()) {
                                topKCandidateScores.removeLast();
                            }
                            topKCandidateScores.append(candidateScore);
                            break;
                        }
                    }

                    QVariantMap batchRefineMetrics;
                    const bool adaptiveRefineCandidateSelectionEnabled =
                        parameters.value(QStringLiteral("enableAdaptiveRefineCandidateSelection"), true).toBool();
                    const bool confidentInitialFastPathEnabled =
                        parameters.value(QStringLiteral("enableConfidentInitialFastPath"), true).toBool();
                    const double confidentInitialFastPathMaxCoarseScoreMm =
                        parameters.value(
                            QStringLiteral("confidentInitialFastPathMaxCoarseScoreMm"),
                            1.0).toDouble();
                    const double confidentInitialFastPathIdentityScoreWindowMm =
                        parameters.value(
                            QStringLiteral("confidentInitialFastPathIdentityScoreWindowMm"),
                            0.35).toDouble();
                    int identityCandidateTopKIndex = -1;
                    for (int topKIndex = 0; topKIndex < topKCandidateScores.size(); ++topKIndex) {
                        if (topKCandidateScores.at(topKIndex).candidateId == identityCandidateId) {
                            identityCandidateTopKIndex = topKIndex;
                            break;
                        }
                    }
                    const double bestCoarseScore =
                        !topKCandidateScores.isEmpty()
                            ? topKCandidateScores.first().coarseScore
                            : std::numeric_limits<double>::max();
                    const double identityCoarseScore =
                        identityCandidateTopKIndex >= 0
                            ? topKCandidateScores.at(identityCandidateTopKIndex).coarseScore
                            : std::numeric_limits<double>::max();
                    CandidateEvaluationResult identityAdmissionCandidate;
                    bool identityAdmissionCandidateFound = identityCandidateTopKIndex >= 0;
                    if (identityAdmissionCandidateFound) {
                        identityAdmissionCandidate = topKCandidateScores.at(identityCandidateTopKIndex);
                    } else {
                        identityAdmissionCandidate.candidateId = identityCandidateId;
                        identityAdmissionCandidate.coarseScore =
                            initialAdmissionPolicyFromParameters(parameters).recoveryMaxCoarseScoreMm + 1.0;
                    }
                    const InitialAdmissionDecision initialAdmissionDecision =
                        assessInitialAdmission(
                            identityAdmissionCandidate,
                            topKCandidateScores,
                            initialAdmissionPolicyFromParameters(parameters),
                            initialAdmissionEvidenceFromParameters(parameters));
                    const bool initialAdmissionGateEnabled =
                        parameters.value(QStringLiteral("enableInitialAdmissionGate"), true).toBool();
                    mergeInitialAdmissionDecision(batchRefineMetrics, initialAdmissionDecision);
                    if (initialAdmissionGateEnabled
                        && initialAdmissionDecision.action == QStringLiteral("reject")) {
                        refineCandidateCount = 0;
                        refineMs = 0;
                        bestCandidateRank = identityCandidateTopKIndex;
                        batchRefineFallback = QStringLiteral("initial_admission_rejected");
                        batchRefineMetrics.insert(
                            QStringLiteral("bestBatchRefineTransformApplied"),
                            false);
                        batchRefineMetrics.insert(
                            QStringLiteral("precomputedFinalResultSource"),
                            QStringLiteral("parallel_initial_admission_rejected"));

                        QVariantMap parallelSearchReport = buildParallelSearchReport(
                            candidateTransforms,
                            topKCandidateScores,
                            parameters,
                            coarseSearchTimer.isValid() ? coarseSearchTimer.elapsed() : 0,
                            constraintParallelFilterEnabled,
                            roiFilterMs,
                            executedMultiResolutionLevelCount,
                            refineCandidateCount,
                            refineMs,
                            bestCandidateRank,
                            batchRefineMetrics);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelPrecomputedConstraintApplied"),
                            activeTarget != target || activeSourcePoints.size() != sourcePoints.size());
                        parallelSearchReport.insert(
                            QStringLiteral("parallelPrecomputedSourcePointCount"),
                            static_cast<int>(activeSourcePoints.size()));
                        parallelSearchReport.insert(
                            QStringLiteral("parallelPrecomputedTargetPointCount"),
                            activeTarget != target
                                ? coarseConstraintTargetPointCount
                                : static_cast<int>(target->GetNumberOfPoints()));
                        parallelSearchReport.insert(
                            QStringLiteral("parallelPrecomputedTargetTriangleCount"),
                            activeTarget != target
                                ? coarseConstraintTargetTriangleCount
                                : static_cast<int>(target->GetNumberOfCells()));
                        parallelSearchReport.insert(
                            QStringLiteral("parallelSearchTotalMs"),
                            parallelSearchTotalTimer.isValid() ? parallelSearchTotalTimer.elapsed() : 0);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelScoredCandidateCount"),
                            parallelScoredCandidateCount);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelCpuIdentityProbeUsed"),
                            parallelCpuIdentityProbeUsed);
                        parallelSearchReport.insert(
                            QStringLiteral("parallelCpuIdentityProbeScoreMm"),
                            parallelCpuIdentityProbeScoreMm);
                        parallelSearchReport.insert(QStringLiteral("batchRefineRequested"), batchRefineRequested);
                        parallelSearchReport.insert(QStringLiteral("batchRefineEnabled"), false);
                        parallelSearchReport.insert(QStringLiteral("batchRefineFallback"), batchRefineFallback);
                        parallelSearchReport.insert(
                            QStringLiteral("runtimeSourceConstraintFilterUsed"),
                            runtimeSourceConstraintFilterUsed);

                        RegistrationRecord record;
                        record.registrationId = registrationId;
                        record.type = QStringLiteral("gicp_rejected");
                        record.transform = vtkSmartPointer<vtkMatrix4x4>::New();
                        record.transform->DeepCopy(initialMatrix);
                        record.timestamp = QDateTime::currentMSecsSinceEpoch();
                        record.fre = initialAdmissionDecision.identityCoarseScoreMm;
                        record.numPoints = static_cast<int>(source->GetNumberOfPoints());

                        QVariantMap metadata;
                        metadata.insert(QStringLiteral("algorithm"), QStringLiteral("GPU-GICP"));
                        metadata.insert(QStringLiteral("converged"), false);
                        metadata.insert(QStringLiteral("rmse"), initialAdmissionDecision.identityCoarseScoreMm);
                        metadata.insert(QStringLiteral("elapsedMs"), 0);
                        metadata.insert(QStringLiteral("sourcePoints"), static_cast<int>(source->GetNumberOfPoints()));
                        metadata.insert(QStringLiteral("targetPoints"), static_cast<int>(target->GetNumberOfPoints()));
                        metadata.insert(
                            QStringLiteral("finalResultSource"),
                            QStringLiteral("parallel_initial_admission_rejected"));
                        metadata.insert(QStringLiteral("registrationRejected"), true);
                        metadata.unite(parallelSearchReport);
                        metadata.insert(
                            QStringLiteral("pipelineElapsedMs"),
                            metadata.value(QStringLiteral("parallelSearchTotalMs")).toLongLong());
                        record.metadata = metadata;
                        saveRecord(registrationId, record);

                        m_lastError = QStringLiteral("Initial admission rejected: ")
                            + initialAdmissionDecision.reason;
                        emit registrationFailed(registrationId, m_lastError);
                        return nullptr;
                    }
                    const bool confidentInitialFastPathApplied =
                        confidentInitialFastPathEnabled
                        && plan.candidateCount >= 64
                        && identityCandidateTopKIndex >= 0
                        && initialAdmissionDecision.action == QStringLiteral("fast_path")
                        && identityCoarseScore <= confidentInitialFastPathMaxCoarseScoreMm
                        && identityCoarseScore
                            <= bestCoarseScore + confidentInitialFastPathIdentityScoreWindowMm;
                    vtkSmartPointer<vtkMatrix4x4> confidentFastPathMatrix = nullptr;
                    QString confidentFastPathCandidateId;
                    if (confidentInitialFastPathApplied) {
                        confidentFastPathCandidateId = identityCandidateId;
                        confidentFastPathMatrix = initialMatrix;
                    }

                    if (confidentFastPathMatrix != nullptr) {
                        refineCandidateCount = 0;
                        refineMs = 0;
                        bestCandidateRank = identityCandidateTopKIndex;
                        batchRefineFallback = QStringLiteral("confident_initial_fast_path");
                        batchRefineMetrics.insert(
                            QStringLiteral("adaptiveRefineCandidateSelectionEnabled"),
                            adaptiveRefineCandidateSelectionEnabled);
                        batchRefineMetrics.insert(
                            QStringLiteral("adaptiveRefineCandidateSelectionApplied"),
                            false);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathApplied"),
                            true);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathMaxCoarseScoreMm"),
                            confidentInitialFastPathMaxCoarseScoreMm);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathIdentityScoreWindowMm"),
                            confidentInitialFastPathIdentityScoreWindowMm);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathBestCoarseScoreMm"),
                            bestCoarseScore);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathIdentityCoarseScoreMm"),
                            identityCoarseScore);
                        batchRefineMetrics.insert(
                            QStringLiteral("confidentInitialFastPathStoppedAfterCoarseLevel"),
                            executedMultiResolutionLevelCount < multiResolutionCellSizes.size());
                        batchRefineMetrics.insert(
                            QStringLiteral("bestBatchRefineCandidateId"),
                            confidentFastPathCandidateId);
                        batchRefineMetrics.insert(
                            QStringLiteral("bestBatchRefineSelectionRank"),
                            identityCandidateTopKIndex);
                        batchRefineMetrics.insert(
                            QStringLiteral("bestBatchRefineTransformApplied"),
                            true);
                        batchRefineMetrics.insert(
                            QStringLiteral("bestBatchRefineRmse"),
                            identityCoarseScore);
                        batchRefineMetrics.insert(
                            QStringLiteral("precomputedFinalResultSource"),
                            QStringLiteral("parallel_confident_initial"));

                        QVariantList precomputedTransform;
                        const QList<double> transformValues = matrixToList(confidentFastPathMatrix);
                        precomputedTransform.reserve(transformValues.size());
                        for (double value : transformValues) {
                            precomputedTransform.append(value);
                        }
                        gpuParameters.insert(QStringLiteral("useBatchRefineAsFinalResult"), true);
                        gpuParameters.insert(QStringLiteral("batchRefineFinalTransform"), precomputedTransform);
                        gpuParameters.insert(
                            QStringLiteral("batchRefineFinalRmse"),
                            identityCoarseScore);
                        gpuParameters.insert(QStringLiteral("batchRefineFinalIterations"), 0);
                        gpuParameters.insert(QStringLiteral("batchRefineFinalConverged"), true);
                    } else {
                    const QList<CandidateEvaluationResult> refineCandidateScores =
                        selectRefineCandidates(
                            topKCandidateScores,
                            topKCandidateScores.size(),
                            adaptiveRefineCandidateSelectionEnabled);
                    refineTimer.start();
                    refineCandidateCount = refineCandidateScores.size();
                    QVariantList refineCandidateIdList;
                    refineCandidateIdList.reserve(refineCandidateScores.size());
                    for (const CandidateEvaluationResult& refineCandidateScore : refineCandidateScores) {
                        refineCandidateIdList.append(refineCandidateScore.candidateId);
                    }
                    batchRefineMetrics.insert(
                        QStringLiteral("adaptiveRefineCandidateSelectionEnabled"),
                        adaptiveRefineCandidateSelectionEnabled);
                    batchRefineMetrics.insert(
                        QStringLiteral("adaptiveRefineCandidateSelectionApplied"),
                        refineCandidateScores.size() < topKCandidateScores.size());
                    batchRefineMetrics.insert(
                        QStringLiteral("refineCandidateIds"),
                        refineCandidateIdList);

                    std::vector<mesh_gpu::RuntimeRefineCandidateRequest> refineRequests;
                    refineRequests.reserve(static_cast<size_t>(refineCandidateScores.size()));
                    QList<vtkSmartPointer<vtkMatrix4x4>> candidateInitialMatrices;
                    candidateInitialMatrices.reserve(refineCandidateScores.size());

                    for (int scoreIndex = 0; scoreIndex < refineCandidateScores.size(); ++scoreIndex) {
                        const QString candidateId = refineCandidateScores.at(scoreIndex).candidateId;
                        for (const CandidateInitialTransform& deltaCandidate : deltaCandidates) {
                            if (deltaCandidate.candidateId != candidateId) {
                                continue;
                            }

                            mesh_gpu::RuntimeRefineCandidateRequest refineRequest;
                            refineRequest.candidateIndex = scoreIndex;
                            refineRequest.initialTransform =
                                qMatrixToMeshGpuTransform(deltaCandidate.transformMatrix);
                            refineRequests.push_back(refineRequest);
                            candidateInitialMatrices.append(
                                qMatrix4x4ToVtkMatrix(deltaCandidate.transformMatrix));
                            break;
                        }
                    }

                    mesh_gpu::RegistrationParams regParams;
                    regParams.max_iterations = parameters.value("maxIterations", 50).toInt();
                    regParams.convergence_threshold =
                        parameters.value("convergenceThreshold", 1e-6f).toFloat();
                    regParams.distance_threshold = parameters.value("distanceThreshold", 10.0f).toFloat();
                    regParams.use_point_to_plane = parameters.value("usePointToPlane", true).toBool();
                    regParams.verbose = parameters.value("verbose", false).toBool();
                    regParams.curvature_weight_mode = static_cast<mesh_gpu::CurvatureWeightMode>(
                        parameters.value("curvatureWeightMode", 0).toInt());

                    const bool allowPreparedStateReuse =
                        parameters.value(
                            QStringLiteral("reusePreparedRuntimeStateForBatchRefine"),
                            true).toBool();
                    const bool reusePreparedRuntimeState =
                        allowPreparedStateReuse && preparedRuntimeStateForBatchRefine;
                    const float refineCellSize =
                        reusePreparedRuntimeState
                            ? preparedRuntimeStateCellSize
                            : parameters.value(QStringLiteral("cellSize"), 1.0).toFloat();
                    const bool refineTargetPrepared =
                        reusePreparedRuntimeState
                            ? true
                            : (!targetMeshPath.isEmpty() && activeTarget == target
                                ? m_meshGPU->loadTargetMesh(targetMeshPath.toStdString(), refineCellSize)
                                : m_meshGPU->setTargetMesh(
                                    targetVertices,
                                    targetNormals,
                                    targetTriangles,
                                    refineCellSize));
                    const bool refineSourcePrepared =
                        reusePreparedRuntimeState
                            ? true
                            : refineTargetPrepared && m_meshGPU->setSourcePointCloud(activeSourcePoints);
                    batchRefineMetrics.insert(
                        QStringLiteral("batchRefineReusedPreparedRuntimeState"),
                        reusePreparedRuntimeState);
                    batchRefineMetrics.insert(
                        QStringLiteral("batchRefineTargetPrepared"),
                        !reusePreparedRuntimeState && refineTargetPrepared);
                    batchRefineMetrics.insert(
                        QStringLiteral("batchRefineSourcePrepared"),
                        !reusePreparedRuntimeState && refineSourcePrepared);
                    batchRefineMetrics.insert(QStringLiteral("batchRefineCellSize"), refineCellSize);

                    if (refineSourcePrepared && !refineRequests.empty()) {
                        const std::vector<mesh_gpu::RuntimeRefineCandidateResult> refineResults =
                            m_meshGPU->refineTransformCandidates(refineRequests, regParams);
                        refineMs = refineTimer.elapsed();

                        if (!refineResults.empty()) {
                            batchRefineEnabled = true;
                            const QString identityCandidateId = QStringLiteral("candidate_000");
                            const float refineRmseTieWindowMm = 0.02f;
                            const float acceptableUnconvergedBatchRefineRmseMm =
                                parameters.value(
                                    QStringLiteral("acceptableUnconvergedBatchRefineRmseMm"),
                                    1.0f).toFloat();
                            const auto refineResultAcceptedForSelection =
                                [acceptableUnconvergedBatchRefineRmseMm](
                                    const mesh_gpu::RuntimeRefineCandidateResult& refineResult) {
                                    return refineResult.success
                                        || (refineResult.iterations > 0
                                            && refineResult.rmse > 0.0f
                                            && refineResult.rmse <= acceptableUnconvergedBatchRefineRmseMm);
                                };
                            batchRefineMetrics.insert(
                                QStringLiteral("acceptableUnconvergedBatchRefineRmseMm"),
                                acceptableUnconvergedBatchRefineRmseMm);
                            QVariantList batchRefineCandidateDetails;
                            batchRefineCandidateDetails.reserve(static_cast<int>(refineResults.size()));
                            for (const mesh_gpu::RuntimeRefineCandidateResult& refineResult : refineResults) {
                                QVariantMap detail;
                                detail.insert(QStringLiteral("candidate_index"), refineResult.candidateIndex);
                                if (refineResult.candidateIndex >= 0
                                    && refineResult.candidateIndex < refineCandidateScores.size()) {
                                    detail.insert(
                                        QStringLiteral("candidate_id"),
                                        refineCandidateScores.at(refineResult.candidateIndex).candidateId);
                                    detail.insert(
                                        QStringLiteral("coarse_score"),
                                        refineCandidateScores.at(refineResult.candidateIndex).coarseScore);
                                }
                                detail.insert(QStringLiteral("rmse"), refineResult.rmse);
                                detail.insert(QStringLiteral("iterations"), refineResult.iterations);
                                detail.insert(QStringLiteral("converged"), refineResult.converged);
                                detail.insert(QStringLiteral("success"), refineResult.success);
                                detail.insert(
                                    QStringLiteral("accepted_for_selection"),
                                    refineResultAcceptedForSelection(refineResult));
                                batchRefineCandidateDetails.append(detail);
                            }
                            batchRefineMetrics.insert(
                                QStringLiteral("batchRefineCandidateDetails"),
                                batchRefineCandidateDetails);
                            int selectedRank = -1;
                            float bestRefineScore = std::numeric_limits<float>::max();
                            int bestRefineIterations = 0;
                            bool bestRefineConverged = false;
                            vtkSmartPointer<vtkMatrix4x4> bestRefinedMatrix = nullptr;

                            for (size_t resultIndex = 0; resultIndex < refineResults.size(); ++resultIndex) {
                                const mesh_gpu::RuntimeRefineCandidateResult& refineResult =
                                    refineResults[resultIndex];
                                if (!refineResultAcceptedForSelection(refineResult)) {
                                    continue;
                                }
                                if (refineResult.candidateIndex < 0
                                    || refineResult.candidateIndex >= candidateInitialMatrices.size()) {
                                    continue;
                                }
                                const CandidateEvaluationResult& currentCandidateScore =
                                    refineCandidateScores.at(refineResult.candidateIndex);
                                const CandidateEvaluationResult* bestCandidateScore =
                                    selectedRank >= 0 && selectedRank < refineCandidateScores.size()
                                    ? &refineCandidateScores.at(selectedRank)
                                    : nullptr;
                                const bool currentIsIdentityCandidate =
                                    currentCandidateScore.candidateId == identityCandidateId;
                                const bool bestIsIdentityCandidate =
                                    bestCandidateScore
                                    && bestCandidateScore->candidateId == identityCandidateId;

                                bool shouldReplaceBest = selectedRank < 0;
                                if (!shouldReplaceBest && refineResult.rmse + refineRmseTieWindowMm < bestRefineScore) {
                                    shouldReplaceBest = true;
                                }
                                if (!shouldReplaceBest
                                    && qAbs(refineResult.rmse - bestRefineScore) <= refineRmseTieWindowMm) {
                                    if (currentIsIdentityCandidate && !bestIsIdentityCandidate) {
                                        shouldReplaceBest = true;
                                    } else if (bestCandidateScore
                                               && currentCandidateScore.coarseScore + 1e-4
                                                   < bestCandidateScore->coarseScore) {
                                        shouldReplaceBest = true;
                                    } else if (bestCandidateScore
                                               && qAbs(
                                                   currentCandidateScore.coarseScore
                                                   - bestCandidateScore->coarseScore) <= 1e-4
                                               && refineResult.candidateIndex < selectedRank) {
                                        shouldReplaceBest = true;
                                    }
                                }

                                if (shouldReplaceBest) {
                                    bestRefineScore = refineResult.rmse;
                                    selectedRank = refineResult.candidateIndex;
                                    bestRefineIterations = refineResult.iterations;
                                    bestRefineConverged = refineResult.converged;
                                    vtkSmartPointer<vtkMatrix4x4> refinedDeltaMatrix =
                                        meshGPUTransformToVTK(refineResult.transform.data);
                                    if (refinedDeltaMatrix && initialMatrix) {
                                        bestRefinedMatrix = multiplyMatrix(refinedDeltaMatrix, initialMatrix);
                                    } else {
                                        bestRefinedMatrix = refinedDeltaMatrix;
                                    }
                                }
                            }

                            if (selectedRank >= 0
                                && selectedRank < candidateInitialMatrices.size()
                                && bestRefinedMatrix != nullptr) {
                                initialMatrix = bestRefinedMatrix;
                                const QString selectedCandidateId =
                                    refineCandidateScores.at(selectedRank).candidateId;
                                bestCandidateRank = -1;
                                for (int topKIndex = 0; topKIndex < topKCandidateScores.size(); ++topKIndex) {
                                    if (topKCandidateScores.at(topKIndex).candidateId == selectedCandidateId) {
                                        bestCandidateRank = topKIndex;
                                        break;
                                    }
                                }
                                if (bestCandidateRank < 0) {
                                    bestCandidateRank = selectedRank;
                                }
                                batchRefineMetrics.insert(QStringLiteral("bestBatchRefineRmse"), bestRefineScore);
                                batchRefineMetrics.insert(
                                    QStringLiteral("bestBatchRefineCandidateId"),
                                    selectedCandidateId);
                                batchRefineMetrics.insert(
                                    QStringLiteral("bestBatchRefineSelectionRank"),
                                    selectedRank);
                                batchRefineMetrics.insert(
                                    QStringLiteral("bestBatchRefineTransformApplied"),
                                    true);
                                QVariantList batchRefineTransform;
                                const QList<double> batchRefineTransformValues = matrixToList(bestRefinedMatrix);
                                batchRefineTransform.reserve(batchRefineTransformValues.size());
                                for (double value : batchRefineTransformValues) {
                                    batchRefineTransform.append(value);
                                }
                                gpuParameters.insert(QStringLiteral("useBatchRefineAsFinalResult"), true);
                                gpuParameters.insert(
                                    QStringLiteral("batchRefineFinalTransform"),
                                    batchRefineTransform);
                                gpuParameters.insert(QStringLiteral("batchRefineFinalRmse"), bestRefineScore);
                                gpuParameters.insert(
                                    QStringLiteral("batchRefineFinalIterations"),
                                    bestRefineIterations);
                                gpuParameters.insert(
                                    QStringLiteral("batchRefineFinalConverged"),
                                    bestRefineConverged);
                            } else {
                                batchRefineFallback = QStringLiteral("runtime_batch_refine_returned_no_successful_results");
                                batchRefineMetrics.insert(
                                    QStringLiteral("bestBatchRefineTransformApplied"),
                                    false);
                            }
                        } else {
                            batchRefineFallback = QStringLiteral("runtime_batch_refine_returned_no_results");
                            batchRefineMetrics.insert(
                                QStringLiteral("bestBatchRefineTransformApplied"),
                                false);
                        }
                    } else {
                        refineMs = refineTimer.elapsed();
                        batchRefineFallback = QStringLiteral("runtime_batch_refine_preparation_failed");
                        batchRefineMetrics.insert(
                            QStringLiteral("bestBatchRefineTransformApplied"),
                            false);
                    }
                    }

                    QVariantMap parallelSearchReport = buildParallelSearchReport(
                        candidateTransforms,
                        topKCandidateScores,
                        parameters,
                        coarseSearchTimer.isValid() ? coarseSearchTimer.elapsed() : 0,
                        constraintParallelFilterEnabled,
                        roiFilterMs,
                        executedMultiResolutionLevelCount,
                        refineCandidateCount,
                        refineMs,
                        bestCandidateRank,
                        batchRefineMetrics);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedConstraintApplied"),
                        activeTarget != target || activeSourcePoints.size() != sourcePoints.size());
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedSourcePointCount"),
                        static_cast<int>(activeSourcePoints.size()));
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedTargetPointCount"),
                        activeTarget != target
                            ? coarseConstraintTargetPointCount
                            : static_cast<int>(target->GetNumberOfPoints()));
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedTargetTriangleCount"),
                        activeTarget != target
                            ? coarseConstraintTargetTriangleCount
                            : static_cast<int>(target->GetNumberOfCells()));
                    parallelSearchReport.insert(
                        QStringLiteral("parallelSearchTotalMs"),
                        parallelSearchTotalTimer.isValid() ? parallelSearchTotalTimer.elapsed() : 0);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelScoredCandidateCount"),
                        parallelScoredCandidateCount);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelCpuIdentityProbeUsed"),
                        parallelCpuIdentityProbeUsed);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelCpuIdentityProbeScoreMm"),
                        parallelCpuIdentityProbeScoreMm);
                    parallelSearchReport.insert(QStringLiteral("batchRefineRequested"), batchRefineRequested);
                    parallelSearchReport.insert(QStringLiteral("batchRefineEnabled"), batchRefineEnabled);
                    parallelSearchReport.insert(QStringLiteral("batchRefineFallback"), batchRefineFallback);
                    parallelSearchReport.insert(
                        QStringLiteral("runtimeSourceConstraintFilterUsed"),
                        runtimeSourceConstraintFilterUsed);
                    gpuParameters.insert(QStringLiteral("parallelSearchReport"), parallelSearchReport);
                } else {
                    QVariantMap parallelSearchReport = buildParallelSearchReport(
                        candidateTransforms,
                        topKCandidateScores,
                        parameters,
                        coarseSearchTimer.isValid() ? coarseSearchTimer.elapsed() : 0,
                        constraintParallelFilterEnabled,
                        roiFilterMs,
                        executedMultiResolutionLevelCount,
                        refineCandidateCount,
                        refineMs,
                        bestCandidateRank);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedConstraintApplied"),
                        activeTarget != target || activeSourcePoints.size() != sourcePoints.size());
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedSourcePointCount"),
                        static_cast<int>(activeSourcePoints.size()));
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedTargetPointCount"),
                        activeTarget != target
                            ? coarseConstraintTargetPointCount
                            : static_cast<int>(target->GetNumberOfPoints()));
                    parallelSearchReport.insert(
                        QStringLiteral("parallelPrecomputedTargetTriangleCount"),
                        activeTarget != target
                            ? coarseConstraintTargetTriangleCount
                            : static_cast<int>(target->GetNumberOfCells()));
                    parallelSearchReport.insert(
                        QStringLiteral("parallelSearchTotalMs"),
                        parallelSearchTotalTimer.isValid() ? parallelSearchTotalTimer.elapsed() : 0);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelScoredCandidateCount"),
                        parallelScoredCandidateCount);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelCpuIdentityProbeUsed"),
                        parallelCpuIdentityProbeUsed);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelCpuIdentityProbeScoreMm"),
                        parallelCpuIdentityProbeScoreMm);
                    parallelSearchReport.insert(QStringLiteral("batchRefineRequested"), batchRefineRequested);
                    parallelSearchReport.insert(QStringLiteral("batchRefineEnabled"), batchRefineEnabled);
                    parallelSearchReport.insert(QStringLiteral("batchRefineFallback"), batchRefineFallback);
                    parallelSearchReport.insert(
                        QStringLiteral("runtimeSourceConstraintFilterUsed"),
                        runtimeSourceConstraintFilterUsed);
                    gpuParameters.insert(QStringLiteral("parallelSearchReport"), parallelSearchReport);
                }
                }
            } else {
                QVariantMap parallelSearchReport = buildParallelSearchReport(
                    candidateTransforms,
                    topKCandidateScores,
                    parameters,
                    coarseSearchTimer.isValid() ? coarseSearchTimer.elapsed() : 0,
                    parameters.value(QStringLiteral("enableConstraintParallelFilter"), false).toBool(),
                    roiFilterMs,
                    0,
                    refineCandidateCount,
                    refineMs,
                    bestCandidateRank);
                if (enableParallelInitialSearch && initialMatrix && !m_meshGPUCandidateScoringAvailable) {
                    parallelSearchReport.insert(QStringLiteral("parallelSearchRequested"), true);
                    parallelSearchReport.insert(QStringLiteral("parallelSearchEnabled"), false);
                    parallelSearchReport.insert(
                        QStringLiteral("parallelSearchFallback"),
                        QStringLiteral("legacy_runtime_without_candidate_scoring"));
                    parallelSearchReport.insert(QStringLiteral("legacyRuntimeCompatibility"), true);
                    parallelSearchReport.insert(
                        QStringLiteral("candidateCountRequested"),
                        parameters.value(QStringLiteral("candidateCount"), 64).toInt());
                    parallelSearchReport.insert(
                        QStringLiteral("topKCountRequested"),
                        parameters.value(QStringLiteral("topKCandidateCount"), 4).toInt());
                    parallelSearchReport.insert(QStringLiteral("batchRefineRequested"), batchRefineRequested);
                    parallelSearchReport.insert(QStringLiteral("batchRefineEnabled"), false);
                    parallelSearchReport.insert(
                        QStringLiteral("batchRefineFallback"),
                        QStringLiteral("legacy_runtime_without_batch_refine"));
                }
                parallelSearchReport.insert(
                    QStringLiteral("runtimeSourceConstraintFilterUsed"),
                    runtimeSourceConstraintFilterUsed);
                gpuParameters.insert(
                    QStringLiteral("parallelSearchReport"),
                    parallelSearchReport);
            }

            effectiveParameters = gpuParameters;
            if (m_meshGPULegacyRuntime) {
                qWarning() << "[RegistrationService] Legacy MeshGPU runtime detected; skipping GPU registration and falling back to VTK ICP:"
                           << "registrationId=" << registrationId;
            } else {
                qDebug() << "[RegistrationService] Dispatching GPU-GICP:"
                         << "registrationId=" << registrationId
                         << "hasInitialTransform=" << (initialMatrix != nullptr)
                         << "useRotationSearch=" << parameters.value("useRotationSearch", false).toBool()
                         << "distanceThreshold=" << parameters.value("distanceThreshold", 10.0f).toFloat()
                         << "maxIterations=" << parameters.value("maxIterations", 50).toInt();
            }

            vtkSmartPointer<vtkMatrix4x4> result = nullptr;
            if (!m_meshGPULegacyRuntime) {
                result = performGICPRegistration(source, target, initialMatrix, effectiveParameters, registrationId);
            }
            if (result) return result;

            qWarning() << "[RegistrationService] GPU-GICP returned no result, falling back to VTK ICP:"
                       << "registrationId=" << registrationId
                       << "lastError=" << m_lastError;
        } else {
            qWarning() << "[RegistrationService] MeshGPU DLL unavailable, falling back to VTK ICP:"
                       << "registrationId=" << parameters.value("registrationId").toString()
                       << "applicationDir=" << QCoreApplication::applicationDirPath();
        }
    }

    // 原有 VTK CPU ICP 路径

    // 解析高级参数
    int maxIterations = effectiveParameters.value("maxIterations", m_defaultICPMaxIterations).toInt();
    int maxLandmarks = effectiveParameters.value("maxLandmarks", m_defaultICPMaxLandmarks).toInt();
    bool startByMatchingCentroids = effectiveParameters.value("startByMatchingCentroids", m_enableICPCentroids).toBool();
    bool checkMeanDistance = effectiveParameters.value("checkMeanDistance", false).toBool();
    double maxMeanDistance = effectiveParameters.value("maxMeanDistance", 0.01).toDouble();
    QString transformMode = effectiveParameters.value("transformMode", "rigid").toString().toLower();
    QString registrationId = effectiveParameters.value("registrationId", QString()).toString();

    if (registrationId.isEmpty()) {
        registrationId = generateRegistrationId("icp_adv");
    }

    emit registrationStarted(registrationId, "icp_advanced");

    try {
        // 解析初始变换矩阵（如果提供）
        vtkSmartPointer<vtkMatrix4x4> initialMatrix = nullptr;
        if (effectiveParameters.contains("initialTransform")) {
            QList<QVariant> matrixList = effectiveParameters.value("initialTransform").toList();
            if (matrixList.size() == 16) {
                QList<double> matrixValues;
                for (const QVariant& v : matrixList) {
                    matrixValues.append(v.toDouble());
                }
                initialMatrix = listToMatrix(matrixValues);
            }
        }

        // 应用初始变换（如果提供）
        vtkSmartPointer<vtkPolyData> transformedSource = source;
        if (initialMatrix) {
            vtkSmartPointer<vtkTransformPolyDataFilter> transformFilter =
                vtkSmartPointer<vtkTransformPolyDataFilter>::New();
            vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
            transform->SetMatrix(initialMatrix);
            transformFilter->SetTransform(transform);
            transformFilter->SetInputData(source);
            transformFilter->Update();

            transformedSource = vtkSmartPointer<vtkPolyData>::New();
            transformedSource->DeepCopy(transformFilter->GetOutput());
        }

        // 创建 ICP Transform
        vtkSmartPointer<vtkIterativeClosestPointTransform> icp =
            vtkSmartPointer<vtkIterativeClosestPointTransform>::New();

        icp->SetSource(transformedSource);
        icp->SetTarget(target);

        // 设置变换模式
        if (transformMode == "similarity") {
            icp->GetLandmarkTransform()->SetModeToSimilarity();
        } else if (transformMode == "affine") {
            icp->GetLandmarkTransform()->SetModeToAffine();
        } else {
            icp->GetLandmarkTransform()->SetModeToRigidBody();
        }

        // 设置迭代参数
        icp->SetMaximumNumberOfIterations(maxIterations);
        icp->SetMaximumNumberOfLandmarks(maxLandmarks);

        // 设置质心匹配
        if (startByMatchingCentroids) {
            icp->StartByMatchingCentroidsOn();
        } else {
            icp->StartByMatchingCentroidsOff();
        }

        // 设置均值距离检查
        if (checkMeanDistance) {
            icp->CheckMeanDistanceOn();
            icp->SetMaximumMeanDistance(maxMeanDistance);
        } else {
            icp->CheckMeanDistanceOff();
        }

        icp->Modified();
        icp->Update();

        // 获取变换矩阵
        vtkSmartPointer<vtkMatrix4x4> icpMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        icpMatrix->DeepCopy(icp->GetMatrix());

        // 如果有初始变换，需要合成最终变换
        vtkSmartPointer<vtkMatrix4x4> finalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        if (initialMatrix) {
            vtkMatrix4x4::Multiply4x4(icpMatrix, initialMatrix, finalMatrix);
        } else {
            finalMatrix->DeepCopy(icpMatrix);
        }

        // 获取 ICP 统计信息
        double meanDistance = icp->GetMeanDistance();
        int actualIterations = icp->GetNumberOfIterations();

        // 保存配准记录
        RegistrationRecord record;
        record.registrationId = registrationId;
        record.type = "icp_advanced";
        record.transform = finalMatrix;
        record.timestamp = QDateTime::currentMSecsSinceEpoch();
        record.fre = meanDistance;
        record.numPoints = source->GetNumberOfPoints();

        QVariantMap metadata;
        metadata["maxIterations"] = maxIterations;
        metadata["actualIterations"] = actualIterations;
        metadata["maxLandmarks"] = maxLandmarks;
        metadata["transformMode"] = transformMode;
        metadata["startByMatchingCentroids"] = startByMatchingCentroids;
        metadata["checkMeanDistance"] = checkMeanDistance;
        metadata["maxMeanDistance"] = maxMeanDistance;
        metadata["meanDistance"] = meanDistance;
        metadata["sourcePoints"] = static_cast<int>(source->GetNumberOfPoints());
        metadata["targetPoints"] = static_cast<int>(target->GetNumberOfPoints());
        metadata.unite(effectiveParameters.value(QStringLiteral("parallelSearchReport")).toMap());
        record.metadata = metadata;

        saveRecord(registrationId, record);

        qDebug() << "[RegistrationService] Advanced ICP registration completed:"
                 << "ID=" << registrationId
                 << "Iterations=" << actualIterations << "/" << maxIterations
                 << "MeanDistance=" << meanDistance << "mm"
                 << "Mode=" << transformMode;

        // 发射完成信号
        QVariantMap result;
        result["registrationId"] = registrationId;
        result["type"] = "icp_advanced";
        result["meanDistance"] = meanDistance;
        result["actualIterations"] = actualIterations;
        result["maxIterations"] = maxIterations;
        result["transformMode"] = transformMode;
        emit registrationCompleted(registrationId, result);

        // 质量检查
        if (meanDistance > 3.0) {
            QVariantMap quality;
            quality["meanDistance"] = meanDistance;
            emit registrationQualityWarning(registrationId, quality,
                QString("High ICP mean distance: %1 mm").arg(meanDistance, 0, 'f', 2));
        }

        return finalMatrix;

    } catch (const std::exception& ex) {
        m_lastError = QString("Advanced ICP registration failed: %1").arg(ex.what());
        qCritical() << "[RegistrationService]" << m_lastError;
        emit registrationFailed(registrationId, m_lastError);
        return nullptr;
    }
}

// ==================== 配准质量评估 ====================

double RegistrationServiceImpl::computeRegistrationError(vtkPoints* sourcePoints,
                                                         vtkPoints* targetPoints)
{
    if (!validatePointSets(sourcePoints, targetPoints, 1)) {
        return -1.0;
    }

    return computeRMSError(sourcePoints, targetPoints, nullptr);
}

double RegistrationServiceImpl::computeRegistrationErrorList(
    const QList<QList<double>>& sourcePoints,
    const QList<QList<double>>& targetPoints,
    const QList<double>& transform)
{
    vtkSmartPointer<vtkPoints> srcVtk = listToVtkPoints(sourcePoints);
    vtkSmartPointer<vtkPoints> tgtVtk = listToVtkPoints(targetPoints);
    vtkSmartPointer<vtkMatrix4x4> matrix = listToMatrix(transform);

    if (!srcVtk || !tgtVtk || !matrix) {
        return -1.0;
    }

    return computeRMSError(srcVtk, tgtVtk, matrix);
}

double RegistrationServiceImpl::computeFRE(const QString& registrationId)
{
    QMutexLocker locker(&m_mutex);
    const RegistrationRecord* record = findRecord(registrationId);

    if (!record) {
        m_lastError = QString("Registration not found: %1").arg(registrationId);
        return -1.0;
    }

    return record->fre;
}

double RegistrationServiceImpl::computeTRE(const QString& registrationId,
                                          const QList<double>& targetPoint)
{
    QMutexLocker locker(&m_mutex);
    const RegistrationRecord* record = findRecord(registrationId);

    if (!record) {
        m_lastError = QString("Registration not found: %1").arg(registrationId);
        return -1.0;
    }

    if (targetPoint.size() < 3) {
        m_lastError = "Target point must have at least 3 coordinates";
        return -1.0;
    }

    // 使用统计学方法计算 TRE
    // 基于 Fitzpatrick 的 TRE 理论公式：
    // TRE^2 ≈ FRE^2 * (1/N + d^2 / (sum of squared distances from fiducials to centroid))
    return computeStatisticalTRE(record, targetPoint);
}

double RegistrationServiceImpl::computeStatisticalTRE(const RegistrationRecord* record,
                                                       const QList<double>& targetPoint)
{
    if (!record || !record->sourcePoints || record->numPoints < 3) {
        // 回退到简化计算
        if (record) {
            return record->fre * 1.5;  // 粗略估计
        }
        return -1.0;
    }

    double point[3] = { targetPoint[0], targetPoint[1], targetPoint[2] };

    // 计算配准点的质心
    double centroid[3] = {0, 0, 0};
    vtkIdType numPoints = record->sourcePoints->GetNumberOfPoints();

    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        record->sourcePoints->GetPoint(i, p);
        centroid[0] += p[0];
        centroid[1] += p[1];
        centroid[2] += p[2];
    }
    centroid[0] /= numPoints;
    centroid[1] /= numPoints;
    centroid[2] /= numPoints;

    // 计算目标点到质心的距离
    double d2 = std::pow(point[0] - centroid[0], 2) +
                std::pow(point[1] - centroid[1], 2) +
                std::pow(point[2] - centroid[2], 2);

    // 计算配准点到质心的平方距离之和（各轴分开）
    double sumSqDistX = 0.0, sumSqDistY = 0.0, sumSqDistZ = 0.0;
    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        record->sourcePoints->GetPoint(i, p);
        sumSqDistX += std::pow(p[0] - centroid[0], 2);
        sumSqDistY += std::pow(p[1] - centroid[1], 2);
        sumSqDistZ += std::pow(p[2] - centroid[2], 2);
    }

    // 计算总方差
    double totalVariance = sumSqDistX + sumSqDistY + sumSqDistZ;

    if (totalVariance < 1e-10) {
        // 所有点重合，无法计算有意义的 TRE
        return record->fre;
    }

    // Fitzpatrick TRE 公式（简化版）：
    // TRE^2 ≈ FRE^2 * (1/N + d^2/f^2)
    // 其中 N 是配准点数量，d 是目标点到质心的距离，f^2 是配准点的总方差

    double fre2 = record->fre * record->fre;
    double tre2 = fre2 * (1.0 / numPoints + d2 / totalVariance);

    // 对于三维情况，还需要考虑各轴的分布
    // 更精确的公式需要计算协方差矩阵的逆，这里用简化公式

    // 返回 TRE 的 RMS 值
    return std::sqrt(tre2);
}

double RegistrationServiceImpl::computeDistanceToFiducialCentroid(const RegistrationRecord* record,
                                                                    const double point[3])
{
    if (!record || !record->sourcePoints || record->numPoints == 0) {
        return 0.0;
    }

    // 计算质心
    double centroid[3] = {0, 0, 0};
    vtkIdType numPoints = record->sourcePoints->GetNumberOfPoints();

    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        record->sourcePoints->GetPoint(i, p);
        centroid[0] += p[0];
        centroid[1] += p[1];
        centroid[2] += p[2];
    }
    centroid[0] /= numPoints;
    centroid[1] /= numPoints;
    centroid[2] /= numPoints;

    // 计算距离
    return std::sqrt(
        std::pow(point[0] - centroid[0], 2) +
        std::pow(point[1] - centroid[1], 2) +
        std::pow(point[2] - centroid[2], 2)
    );
}

void RegistrationServiceImpl::computeFiducialCovariance(const RegistrationRecord* record,
                                                         double centroid[3],
                                                         double covariance[3][3])
{
    // 初始化
    for (int i = 0; i < 3; ++i) {
        centroid[i] = 0.0;
        for (int j = 0; j < 3; ++j) {
            covariance[i][j] = 0.0;
        }
    }

    if (!record || !record->sourcePoints || record->numPoints == 0) {
        return;
    }

    vtkIdType numPoints = record->sourcePoints->GetNumberOfPoints();

    // 计算质心
    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        record->sourcePoints->GetPoint(i, p);
        centroid[0] += p[0];
        centroid[1] += p[1];
        centroid[2] += p[2];
    }
    centroid[0] /= numPoints;
    centroid[1] /= numPoints;
    centroid[2] /= numPoints;

    // 计算协方差矩阵
    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        record->sourcePoints->GetPoint(i, p);

        double diff[3] = {
            p[0] - centroid[0],
            p[1] - centroid[1],
            p[2] - centroid[2]
        };

        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                covariance[j][k] += diff[j] * diff[k];
            }
        }
    }

    // 归一化
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            covariance[i][j] /= numPoints;
        }
    }
}

QVariantMap RegistrationServiceImpl::evaluateRegistrationQuality(const QString& registrationId)
{
    QMutexLocker locker(&m_mutex);
    const RegistrationRecord* record = findRecord(registrationId);

    if (!record) {
        m_lastError = QString("Registration not found: %1").arg(registrationId);
        return QVariantMap();
    }

    QVariantMap quality;
    quality["registrationId"] = registrationId;
    quality["type"] = record->type;
    quality["fre"] = record->fre;
    quality["numPoints"] = record->numPoints;

    // 使用增强的 TRE 统计计算
    double treMax = 0.0;
    double treMean = 0.0;
    double treMin = std::numeric_limits<double>::max();
    QList<double> treValues;

    if (record->sourcePoints && record->numPoints >= 3) {
        // 计算质心和协方差
        double centroid[3];
        double covariance[3][3];
        computeFiducialCovariance(record, centroid, covariance);

        // 计算各个测试点的 TRE（使用配准点周围的采样点）
        vtkIdType numPoints = record->sourcePoints->GetNumberOfPoints();

        // 计算配准点边界框
        double bounds[6] = {
            std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest(),
            std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest(),
            std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest()
        };

        for (vtkIdType i = 0; i < numPoints; ++i) {
            double p[3];
            record->sourcePoints->GetPoint(i, p);
            bounds[0] = std::min(bounds[0], p[0]); bounds[1] = std::max(bounds[1], p[0]);
            bounds[2] = std::min(bounds[2], p[1]); bounds[3] = std::max(bounds[3], p[1]);
            bounds[4] = std::min(bounds[4], p[2]); bounds[5] = std::max(bounds[5], p[2]);
        }

        // 在边界框扩展区域采样测试点
        double margin = 50.0;  // 50mm 边距
        double spacing = 20.0; // 20mm 间隔

        int samplesCollected = 0;
        for (double x = bounds[0] - margin; x <= bounds[1] + margin && samplesCollected < 100; x += spacing) {
            for (double y = bounds[2] - margin; y <= bounds[3] + margin && samplesCollected < 100; y += spacing) {
                for (double z = bounds[4] - margin; z <= bounds[5] + margin && samplesCollected < 100; z += spacing) {
                    QList<double> testPoint = {x, y, z};
                    double tre = computeStatisticalTRE(record, testPoint);

                    if (tre >= 0) {
                        treValues.append(tre);
                        treMax = std::max(treMax, tre);
                        treMin = std::min(treMin, tre);
                        treMean += tre;
                        samplesCollected++;
                    }
                }
            }
        }

        if (!treValues.isEmpty()) {
            treMean /= treValues.size();
        } else {
            // 回退到简化估计
            treMax = record->fre * 2.0;
            treMean = record->fre * 1.2;
            treMin = record->fre;
        }

        // 计算 TRE 标准差
        double treStdDev = 0.0;
        if (treValues.size() > 1) {
            for (double tre : treValues) {
                treStdDev += std::pow(tre - treMean, 2);
            }
            treStdDev = std::sqrt(treStdDev / (treValues.size() - 1));
        }

        quality["tre_max"] = treMax;
        quality["tre_min"] = treMin;
        quality["tre_mean"] = treMean;
        quality["tre_std"] = treStdDev;
        quality["tre_samples"] = treValues.size();

        // 计算配准点分布特征
        double totalVariance = covariance[0][0] + covariance[1][1] + covariance[2][2];
        quality["fiducial_spread"] = std::sqrt(totalVariance);
        quality["centroid_x"] = centroid[0];
        quality["centroid_y"] = centroid[1];
        quality["centroid_z"] = centroid[2];

    } else {
        // 回退到简化估计
        treMax = record->fre * 1.5;
        treMean = record->fre;
        quality["tre_max"] = treMax;
        quality["tre_mean"] = treMean;
    }

    // 评估质量等级
    QString qualityLevel = evaluateQualityLevel(record->fre, treMax);
    quality["quality"] = qualityLevel;

    // 生成建议
    QString recommendation = generateRecommendation(record->fre, treMax, record->numPoints);
    quality["recommendation"] = recommendation;

    // 添加质量分数（0-100）
    double qualityScore = 100.0;
    if (record->fre > 1.0) qualityScore -= (record->fre - 1.0) * 10.0;
    if (treMax > 2.0) qualityScore -= (treMax - 2.0) * 5.0;
    if (record->numPoints < 5) qualityScore -= (5 - record->numPoints) * 5.0;
    qualityScore = std::max(0.0, std::min(100.0, qualityScore));
    quality["score"] = qualityScore;

    return quality;
}

// ==================== 变换矩阵操作 ====================

bool RegistrationServiceImpl::saveRegistrationResult(const QString& registrationId,
                                                     vtkMatrix4x4* transform,
                                                     const QVariantMap& metadata)
{
    if (!transform) {
        m_lastError = "Transform matrix is null";
        return false;
    }

    RegistrationRecord record;
    record.registrationId = registrationId;
    record.type = metadata.value("type", "manual").toString();
    record.transform = vtkSmartPointer<vtkMatrix4x4>::New();
    record.transform->DeepCopy(transform);
    record.timestamp = QDateTime::currentMSecsSinceEpoch();
    record.metadata = metadata;

    saveRecord(registrationId, record);
    return true;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::loadRegistrationResult(const QString& registrationId)
{
    QMutexLocker locker(&m_mutex);
    const RegistrationRecord* record = findRecord(registrationId);

    if (!record) {
        m_lastError = QString("Registration not found: %1").arg(registrationId);
        return nullptr;
    }

    return record->transform;
}

QStringList RegistrationServiceImpl::getRegistrationList() const
{
    QMutexLocker locker(&m_mutex);
    return m_registrations.keys();
}

QVariantMap RegistrationServiceImpl::getRegistrationInfo(const QString& registrationId) const
{
    QMutexLocker locker(&m_mutex);
    const RegistrationRecord* record = findRecord(registrationId);

    if (!record) {
        return QVariantMap();
    }

    QVariantMap info;
    info["registrationId"] = record->registrationId;
    info["type"] = record->type;
    info["timestamp"] = record->timestamp;
    info["fre"] = record->fre;
    info["numPoints"] = record->numPoints;
    info["metadata"] = record->metadata;

    // 手动转换矩阵为列表（因为 matrixToList 不是 const）
    QList<double> transformList;
    if (record->transform) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                transformList.append(record->transform->GetElement(i, j));
            }
        }
    }
    info["transform"] = QVariant::fromValue(transformList);

    return info;
}

bool RegistrationServiceImpl::deleteRegistration(const QString& registrationId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_registrations.contains(registrationId)) {
        m_lastError = QString("Registration not found: %1").arg(registrationId);
        return false;
    }

    m_registrations.remove(registrationId);
    qDebug() << "[RegistrationService] Deleted registration:" << registrationId;
    return true;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::invertMatrix(vtkMatrix4x4* matrix)
{
    if (!matrix) {
        m_lastError = "Input matrix is null";
        return nullptr;
    }

    vtkSmartPointer<vtkMatrix4x4> inverse = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkMatrix4x4::Invert(matrix, inverse);
    return inverse;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::multiplyMatrix(vtkMatrix4x4* matrix1,
                                                                       vtkMatrix4x4* matrix2)
{
    if (!matrix1 || !matrix2) {
        m_lastError = "Input matrix is null";
        return nullptr;
    }

    vtkSmartPointer<vtkMatrix4x4> result = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkMatrix4x4::Multiply4x4(matrix1, matrix2, result);
    return result;
}

QList<double> RegistrationServiceImpl::transformPoint(const QList<double>& point,
                                                      vtkMatrix4x4* transform)
{
    if (point.size() < 3 || !transform) {
        return QList<double>();
    }

    double in[3] = { point[0], point[1], point[2] };
    double out[3];

    transformPoint(in, out, transform);

    QList<double> result;
    result << out[0] << out[1] << out[2];

    return result;
}

vtkSmartPointer<vtkPoints> RegistrationServiceImpl::transformPoints(vtkPoints* points,
                                                                     vtkMatrix4x4* transform)
{
    if (!points || !transform) {
        return nullptr;
    }

    vtkSmartPointer<vtkPoints> transformedPoints = vtkSmartPointer<vtkPoints>::New();
    transformedPoints->SetNumberOfPoints(points->GetNumberOfPoints());

    for (vtkIdType i = 0; i < points->GetNumberOfPoints(); ++i) {
        double in[3], out[3];
        points->GetPoint(i, in);
        transformPoint(in, out, transform);
        transformedPoints->SetPoint(i, out);
    }

    return transformedPoints;
}

// ==================== 2D-3D 配准支持 ====================

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::perform2D3DRegistration(
    const QString& image2D,
    vtkPolyData* model3D,
    vtkMatrix4x4* initialTransform,
    const QVariantMap& parameters)
{
    Q_UNUSED(model3D);  // 2D-3D 配准使用 CT 体积数据而非 mesh

    // 获取 Registration2D3D 服务
    Registration2D3DService* reg2D3DService = getRegistration2D3DService();

    if (!reg2D3DService) {
        m_lastError = "Registration2D3D service not available. Please ensure the Registration2D3D plugin is loaded.";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    // 检查 Python 环境
    if (!reg2D3DService->isPythonInitialized()) {
        // 尝试初始化 Python 环境
        QString pythonHome = parameters.value("pythonHome", QString()).toString();
        QString scriptsPath = parameters.value("scriptsPath", QString()).toString();

        if (pythonHome.isEmpty() || scriptsPath.isEmpty()) {
            m_lastError = "Python environment not initialized. Please provide pythonHome and scriptsPath parameters.";
            qWarning() << "[RegistrationService]" << m_lastError;
            return nullptr;
        }

        if (!reg2D3DService->initializePythonEnvironment(pythonHome, scriptsPath)) {
            m_lastError = QString("Failed to initialize Python environment: %1").arg(reg2D3DService->getLastError());
            qWarning() << "[RegistrationService]" << m_lastError;
            return nullptr;
        }
    }

    // 构建 2D-3D 配准参数
    Registration2D3DParameters regParams;

    // 必需参数
    regParams.ctPath = parameters.value("ctPath", QString()).toString();
    regParams.xrayApPath = image2D;  // 使用传入的 image2D 作为 AP 视角
    regParams.xrayLatPath = parameters.value("xrayLatPath", QString()).toString();

    if (regParams.ctPath.isEmpty()) {
        m_lastError = "CT path is required for 2D-3D registration";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    if (regParams.xrayApPath.isEmpty() && regParams.xrayLatPath.isEmpty()) {
        m_lastError = "At least one X-ray image (AP or LAT) is required";
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    // 可选参数
    regParams.jingguPath = parameters.value("jingguPath", QString()).toString();
    regParams.outputDirectory = parameters.value("outputDirectory", QString()).toString();
    regParams.generateDRR = parameters.value("generateDRR", true).toBool();

    // 初始参数（从初始变换矩阵或参数中获取）
    if (initialTransform) {
        // 从变换矩阵提取旋转和平移参数
        // 简化处理：直接使用变换矩阵的元素
        regParams.initParams = {
            0.0, 0.0, 0.0,  // 旋转角度
            initialTransform->GetElement(0, 3),
            initialTransform->GetElement(1, 3),
            initialTransform->GetElement(2, 3)
        };
    } else if (parameters.contains("initParams")) {
        QList<QVariant> initList = parameters.value("initParams").toList();
        regParams.initParams.clear();
        for (const QVariant& v : initList) {
            regParams.initParams.append(v.toDouble());
        }
    }

    // 搜索范围
    if (parameters.contains("searchRange")) {
        QList<QVariant> rangeList = parameters.value("searchRange").toList();
        regParams.searchRange.clear();
        for (const QVariant& v : rangeList) {
            regParams.searchRange.append(v.toInt());
        }
    }

    // 优化参数
    regParams.kdTreeNum = parameters.value("kdTreeNum", 50).toInt();

    // 图像翻转设置
    regParams.apUpDown = parameters.value("apUpDown", false).toBool();
    regParams.apHorizontal = parameters.value("apHorizontal", false).toBool();
    regParams.latUpDown = parameters.value("latUpDown", false).toBool();
    regParams.latHorizontal = parameters.value("latHorizontal", false).toBool();

    // 验证参数
    QString validationError;
    if (!reg2D3DService->validateParameters(regParams, validationError)) {
        m_lastError = QString("Invalid 2D-3D registration parameters: %1").arg(validationError);
        qWarning() << "[RegistrationService]" << m_lastError;
        return nullptr;
    }

    // 生成配准 ID
    QString registrationId = parameters.value("registrationId", QString()).toString();
    if (registrationId.isEmpty()) {
        registrationId = generateRegistrationId("2d3d");
    }

    emit registrationStarted(registrationId, "2d3d");

    qDebug() << "[RegistrationService] Starting 2D-3D registration:"
             << "ID=" << registrationId
             << "CT=" << regParams.ctPath
             << "AP=" << regParams.xrayApPath
             << "LAT=" << regParams.xrayLatPath;

    // 执行同步配准
    Registration2D3DResult result;
    bool success = reg2D3DService->executeRegistrationSync(regParams, result);

    if (!success || result.status != Registration2D3DResult::Completed) {
        m_lastError = QString("2D-3D registration failed: %1").arg(
            result.errorMessage.isEmpty() ? reg2D3DService->getLastError() : result.errorMessage);
        qCritical() << "[RegistrationService]" << m_lastError;
        emit registrationFailed(registrationId, m_lastError);
        return nullptr;
    }

    // 从配准结果构建变换矩阵
    // 使用 AP 和 LAT 的平均结果
    double rx = (result.apResult.rx + result.latResult.rx) / 2.0;
    double ry = (result.apResult.ry + result.latResult.ry) / 2.0;
    double rz = (result.apResult.rz + result.latResult.rz) / 2.0;
    double tx = (result.apResult.tx + result.latResult.tx) / 2.0;
    double ty = (result.apResult.ty + result.latResult.ty) / 2.0;
    double tz = (result.apResult.tz + result.latResult.tz) / 2.0;

    // 创建变换矩阵
    vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
    transform->Identity();
    transform->Translate(tx, ty, tz);
    transform->RotateX(rx);
    transform->RotateY(ry);
    transform->RotateZ(rz);

    vtkSmartPointer<vtkMatrix4x4> resultMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    resultMatrix->DeepCopy(transform->GetMatrix());

    // 保存配准记录
    RegistrationRecord record;
    record.registrationId = registrationId;
    record.type = "2d3d";
    record.transform = resultMatrix;
    record.timestamp = QDateTime::currentMSecsSinceEpoch();
    record.fre = result.finalMetric;  // 使用配准度量值作为 FRE 的替代
    record.numPoints = 0;  // 2D-3D 配准不使用点

    QVariantMap metadata;
    metadata["ctPath"] = regParams.ctPath;
    metadata["xrayApPath"] = regParams.xrayApPath;
    metadata["xrayLatPath"] = regParams.xrayLatPath;
    metadata["duration"] = result.durationSeconds;
    metadata["totalIterations"] = result.totalIterations;
    metadata["finalMetric"] = result.finalMetric;
    metadata["apMetric"] = result.apResult.goMetric;
    metadata["latMetric"] = result.latResult.goMetric;
    metadata["apParams"] = QVariantList{result.apResult.rx, result.apResult.ry, result.apResult.rz,
                                        result.apResult.tx, result.apResult.ty, result.apResult.tz};
    metadata["latParams"] = QVariantList{result.latResult.rx, result.latResult.ry, result.latResult.rz,
                                         result.latResult.tx, result.latResult.ty, result.latResult.tz};
    if (!result.apResult.drrImagePath.isEmpty()) {
        metadata["apDRRPath"] = result.apResult.drrImagePath;
    }
    if (!result.latResult.drrImagePath.isEmpty()) {
        metadata["latDRRPath"] = result.latResult.drrImagePath;
    }
    record.metadata = metadata;

    saveRecord(registrationId, record);

    qDebug() << "[RegistrationService] 2D-3D registration completed:"
             << "ID=" << registrationId
             << "Duration=" << result.durationSeconds << "s"
             << "FinalMetric=" << result.finalMetric;

    // 发射完成信号
    QVariantMap resultInfo;
    resultInfo["registrationId"] = registrationId;
    resultInfo["type"] = "2d3d";
    resultInfo["duration"] = result.durationSeconds;
    resultInfo["finalMetric"] = result.finalMetric;
    resultInfo["totalIterations"] = result.totalIterations;
    emit registrationCompleted(registrationId, resultInfo);

    return resultMatrix;
}

// ==================== 工具方法 ====================

QList<double> RegistrationServiceImpl::matrixToList(vtkMatrix4x4* matrix)
{
    QList<double> list;
    if (!matrix) {
        return list;
    }

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            list.append(matrix->GetElement(i, j));
        }
    }

    return list;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::listToMatrix(const QList<double>& list)
{
    if (list.size() != 16) {
        m_lastError = "Matrix list must have exactly 16 elements";
        return nullptr;
    }

    vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
    int index = 0;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            matrix->SetElement(i, j, list[index++]);
        }
    }

    return matrix;
}

bool RegistrationServiceImpl::exportMatrix(vtkMatrix4x4* matrix,
                                          const QString& filePath,
                                          const QString& format)
{
    if (!matrix) {
        m_lastError = "Matrix is null";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file: %1").arg(filePath);
        return false;
    }

    QTextStream out(&file);

    if (format == "txt") {
        // 文本格式
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                out << matrix->GetElement(i, j);
                if (j < 3) out << " ";
            }
            out << "\n";
        }
    } else if (format == "json") {
        // JSON 格式
        QJsonObject json;
        QJsonArray matrixArray;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                matrixArray.append(matrix->GetElement(i, j));
            }
        }
        json["matrix"] = matrixArray;
        json["timestamp"] = QDateTime::currentMSecsSinceEpoch();

        QJsonDocument doc(json);
        out << doc.toJson();
    } else {
        m_lastError = QString("Unsupported format: %1").arg(format);
        file.close();
        return false;
    }

    file.close();
    return true;
}

vtkSmartPointer<vtkMatrix4x4> RegistrationServiceImpl::importMatrix(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file: %1").arg(filePath);
        return nullptr;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();

    // 尝试 JSON 格式
    if (content.trimmed().startsWith("{")) {
        QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject json = doc.object();
            QJsonArray matrixArray = json["matrix"].toArray();

            if (matrixArray.size() == 16) {
                int index = 0;
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        matrix->SetElement(i, j, matrixArray[index++].toDouble());
                    }
                }
                return matrix;
            }
        }
    }

    // 尝试文本格式
    QStringList lines = content.split('\n', Qt::SkipEmptyParts);
    if (lines.size() == 4) {
        for (int i = 0; i < 4; ++i) {
            QStringList values = lines[i].split(' ', Qt::SkipEmptyParts);
            if (values.size() == 4) {
                for (int j = 0; j < 4; ++j) {
                    matrix->SetElement(i, j, values[j].toDouble());
                }
            }
        }
        return matrix;
    }

    m_lastError = "Invalid matrix file format";
    return nullptr;
}

QString RegistrationServiceImpl::getLastError() const
{
    return m_lastError;
}

// ==================== Private Methods ====================

QString RegistrationServiceImpl::generateRegistrationId(const QString& prefix)
{
    return QString("%1_%2_%3")
        .arg(prefix)
        .arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"))
        .arg(qrand() % 10000, 4, 10, QChar('0'));
}

bool RegistrationServiceImpl::validatePointSets(vtkPoints* sourcePoints, vtkPoints* targetPoints, int minPoints)
{
    if (!sourcePoints || !targetPoints) {
        m_lastError = "Source or target points are null";
        qWarning() << "[RegistrationService]" << m_lastError;
        return false;
    }

    vtkIdType numSource = sourcePoints->GetNumberOfPoints();
    vtkIdType numTarget = targetPoints->GetNumberOfPoints();

    if (numSource != numTarget) {
        m_lastError = QString("Point count mismatch: source=%1, target=%2")
            .arg(numSource).arg(numTarget);
        qWarning() << "[RegistrationService]" << m_lastError;
        return false;
    }

    if (numSource < minPoints) {
        m_lastError = QString("Insufficient points: %1 (minimum %2 required)")
            .arg(numSource).arg(minPoints);
        qWarning() << "[RegistrationService]" << m_lastError;
        return false;
    }

    return true;
}

vtkSmartPointer<vtkPoints> RegistrationServiceImpl::listToVtkPoints(const QList<QList<double>>& pointList)
{
    if (pointList.isEmpty()) {
        return nullptr;
    }

    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->SetNumberOfPoints(pointList.size());

    for (int i = 0; i < pointList.size(); ++i) {
        const QList<double>& point = pointList[i];
        if (point.size() >= 3) {
            points->SetPoint(i, point[0], point[1], point[2]);
        }
    }

    return points;
}

double RegistrationServiceImpl::computeRMSError(vtkPoints* sourcePoints,
                                                vtkPoints* targetPoints,
                                                vtkMatrix4x4* transform)
{
    if (!sourcePoints || !targetPoints) {
        return -1.0;
    }

    vtkIdType numPoints = sourcePoints->GetNumberOfPoints();
    if (numPoints == 0 || numPoints != targetPoints->GetNumberOfPoints()) {
        return -1.0;
    }

    double sumSquaredError = 0.0;

    for (vtkIdType i = 0; i < numPoints; ++i) {
        double sourcePoint[3], targetPoint[3];
        sourcePoints->GetPoint(i, sourcePoint);
        targetPoints->GetPoint(i, targetPoint);

        // 如果提供了变换，先变换源点
        if (transform) {
            double transformedPoint[3];
            transformPoint(sourcePoint, transformedPoint, transform);
            sourcePoint[0] = transformedPoint[0];
            sourcePoint[1] = transformedPoint[1];
            sourcePoint[2] = transformedPoint[2];
        }

        // 计算欧氏距离
        double dx = sourcePoint[0] - targetPoint[0];
        double dy = sourcePoint[1] - targetPoint[1];
        double dz = sourcePoint[2] - targetPoint[2];

        sumSquaredError += (dx * dx + dy * dy + dz * dz);
    }

    double rms = std::sqrt(sumSquaredError / numPoints);
    return rms;
}

void RegistrationServiceImpl::transformPoint(double in[3], double out[3], vtkMatrix4x4* matrix)
{
    double point[4] = { in[0], in[1], in[2], 1.0 };
    double transformed[4];

    for (int i = 0; i < 4; ++i) {
        transformed[i] = 0.0;
        for (int j = 0; j < 4; ++j) {
            transformed[i] += matrix->GetElement(i, j) * point[j];
        }
    }

    out[0] = transformed[0] / transformed[3];
    out[1] = transformed[1] / transformed[3];
    out[2] = transformed[2] / transformed[3];
}

RegistrationRecord* RegistrationServiceImpl::findRecord(const QString& registrationId)
{
    auto it = m_registrations.find(registrationId);
    return (it != m_registrations.end()) ? &it.value() : nullptr;
}

const RegistrationRecord* RegistrationServiceImpl::findRecord(const QString& registrationId) const
{
    auto it = m_registrations.find(registrationId);
    return (it != m_registrations.end()) ? &it.value() : nullptr;
}

void RegistrationServiceImpl::saveRecord(const QString& registrationId, const RegistrationRecord& record)
{
    QMutexLocker locker(&m_mutex);
    m_registrations[registrationId] = record;
    qDebug() << "[RegistrationService] Saved registration record:" << registrationId;
}

QString RegistrationServiceImpl::evaluateQualityLevel(double fre, double treMax)
{
    if (fre < 2.0 && treMax < 3.0) {
        return "excellent";
    } else if (fre < 3.0 && treMax < 5.0) {
        return "good";
    } else if (fre < 5.0 && treMax < 8.0) {
        return "acceptable";
    } else {
        return "poor";
    }
}

QString RegistrationServiceImpl::generateRecommendation(double fre, double treMax, int numPoints)
{
    QStringList recommendations;

    if (fre > 5.0) {
        recommendations << "High FRE detected. Consider re-selecting landmarks for better accuracy.";
    }

    if (treMax > 8.0) {
        recommendations << "High TRE detected. Registration may not be suitable for navigation.";
    }

    if (numPoints < 5) {
        recommendations << QString("Only %1 points used. Consider using more points (≥5) for better stability.").arg(numPoints);
    }

    if (fre < 2.0 && numPoints >= 5) {
        recommendations << "Registration quality is excellent. Safe to proceed with navigation.";
    }

    if (recommendations.isEmpty()) {
        return "Registration quality is acceptable.";
    }

    return recommendations.join(" ");
}
