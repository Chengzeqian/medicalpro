#include "PointRegistrationServiceImpl.h"
#include "Plugins/RegistrationCore/ankle_registration_utils.h"
#include "Plugins/RegistrationCore/robust_initial_transform.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"
#include "Plugins/OpticalTracking/OpticalTrackingService.h"
#include "Plugins/RegistrationCore/RegistrationService.h"
#include "widgets/PointRegistrationWidget.h"
#include "internal/PointRegistrationVTKWidget.h"
#include <QDebug>
#include <QDateTime>
#include <QtMath>
#include <QElapsedTimer>
#include <QUuid>
#include <QFileInfo>
#include <QHash>
#include <algorithm>

#ifdef VTK_FOUND
#include <vtkLandmarkTransform.h>
#include <vtkPoints.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkPolyData.h>
#include <vtkSTLReader.h>
#endif

namespace {

QHash<const PointRegistrationServiceImpl*, PointRegistrationExecutionOptions>& executionOptionsStore()
{
    static QHash<const PointRegistrationServiceImpl*, PointRegistrationExecutionOptions> store;
    return store;
}

QString refineMethodForRegistrationMethod(const QString& registrationMethodId)
{
    if (registrationMethodId == QStringLiteral("landmark_plus_global_icp")) {
        return QStringLiteral("registration_core_cpu_icp");
    }
    if (registrationMethodId == QStringLiteral("landmark_plus_global_gicp")) {
        return QStringLiteral("registration_core_gpu_gicp");
    }
    if (registrationMethodId == QStringLiteral("ankle_two_stage_constrained")) {
        return QStringLiteral("registration_core_gpu_gicp");
    }
    return QStringLiteral("weighted_landmark_only");
}

bool shouldDelegateSurfaceRefine(const QString& registrationMethodId)
{
    return registrationMethodId == QStringLiteral("landmark_plus_global_icp") ||
        registrationMethodId == QStringLiteral("landmark_plus_global_gicp") ||
        registrationMethodId == QStringLiteral("ankle_two_stage_constrained");
}

bool shouldUseGpuRefine(const QString& registrationMethodId)
{
    return registrationMethodId == QStringLiteral("landmark_plus_global_gicp") ||
        registrationMethodId == QStringLiteral("ankle_two_stage_constrained");
}

void insertMetadataMetric(
    QVariantMap& metrics,
    const QVariantMap& metadata,
    const QString& metadataKey,
    const QString& resultKey)
{
    if (metadata.contains(metadataKey)) {
        metrics.insert(resultKey, metadata.value(metadataKey));
    }
}

void applyParallelSearchMetadataMetrics(QVariantMap& metrics, const QVariantMap& metadata)
{
    insertMetadataMetric(
        metrics,
        metadata,
        QStringLiteral("parallelSearchEnabled"),
        QStringLiteral("parallel_search_enabled"));
    insertMetadataMetric(
        metrics,
        metadata,
        QStringLiteral("candidateCount"),
        QStringLiteral("candidate_count"));
    insertMetadataMetric(
        metrics,
        metadata,
        QStringLiteral("topKCount"),
        QStringLiteral("top_k_count"));
    insertMetadataMetric(
        metrics,
        metadata,
        QStringLiteral("coarseSearchMs"),
        QStringLiteral("coarse_search_ms"));
    insertMetadataMetric(
        metrics,
        metadata,
        QStringLiteral("roiFilterMs"),
        QStringLiteral("roi_filter_ms"));
    insertMetadataMetric(
        metrics,
        metadata,
        QStringLiteral("bestCandidateRank"),
        QStringLiteral("best_candidate_rank"));
    insertMetadataMetric(
        metrics,
        metadata,
        QStringLiteral("coarseScore"),
        QStringLiteral("coarse_score"));
    insertMetadataMetric(
        metrics,
        metadata,
        QStringLiteral("multiResolutionProfile"),
        QStringLiteral("multi_resolution_profile"));
    insertMetadataMetric(
        metrics,
        metadata,
        QStringLiteral("elapsedMs"),
        QStringLiteral("refine_ms"));
}

void insertMetricFallback(QVariantMap& metrics, const QString& key, const QVariant& value)
{
    if (!metrics.contains(key)) {
        metrics.insert(key, value);
    }
}

#ifdef VTK_FOUND
QMatrix4x4 vtkMatrixToQMatrix(vtkMatrix4x4* matrix)
{
    QMatrix4x4 qtMatrix;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            qtMatrix(row, column) = static_cast<float>(matrix->GetElement(row, column));
        }
    }
    return qtMatrix;
}

void copyQMatrixToVtkMatrix(const QMatrix4x4& source, vtkMatrix4x4* target)
{
    if (!target) {
        return;
    }

    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            target->SetElement(row, column, source(row, column));
        }
    }
}

QVariantList vtkMatrixToVariantList(vtkMatrix4x4* matrix)
{
    QVariantList values;
    values.reserve(16);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            values.append(matrix->GetElement(row, column));
        }
    }
    return values;
}

vtkSmartPointer<vtkPolyData> buildProbePointCloud(const QList<QVector3D>& points)
{
    auto vtkPointsData = vtkSmartPointer<vtkPoints>::New();
    for (const auto& point : points) {
        vtkPointsData->InsertNextPoint(point.x(), point.y(), point.z());
    }

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(vtkPointsData);
    return polyData;
}
#endif

double squaredDistance(const QVector3D& left, const QVector3D& right)
{
    const QVector3D delta = left - right;
    return static_cast<double>(QVector3D::dotProduct(delta, delta));
}

QVariantList vectorToVariantList(const QVector3D& point)
{
    return QVariantList { point.x(), point.y(), point.z() };
}

QVariantList vectorListToVariantList(const QList<QVector3D>& points)
{
    QVariantList values;
    values.reserve(points.size());
    for (const QVector3D& point : points) {
        values.append(vectorToVariantList(point));
    }
    return values;
}

QVariantList vectorListToFlatVariantList(const QList<QVector3D>& points)
{
    QVariantList values;
    values.reserve(points.size() * 3);
    for (const QVector3D& point : points) {
        values.append(point.x());
        values.append(point.y());
        values.append(point.z());
    }
    return values;
}

QList<QVector3D> flattenConstraintRegionPoints(const QMap<QString, QList<QVector3D>>& regions)
{
    QList<QVector3D> points;
    for (auto it = regions.cbegin(); it != regions.cend(); ++it) {
        points.append(it.value());
    }
    return points;
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

double constraintMembershipRadiusMm(const TargetRegistrationRegion& region)
{
    if (region.radiusMm > 0.0) {
        return qBound(4.0, region.radiusMm * 0.35, 12.0);
    }
    return 6.0;
}

bool matchesConstraintCloud(
    const QVector3D& point,
    const QList<QVector3D>& constraintPoints,
    double membershipRadiusMm)
{
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

QList<int> selectConstraintRefinePairIndices(
    const QVector<RegistrationPoint>& points,
    const TargetRegistrationRegion& region,
    const QMap<QString, QList<QVector3D>>& regions)
{
    const QList<QVector3D> constraintPoints = flattenConstraintRegionPoints(regions);
    const bool hasTargetRegion = region.radiusMm > 0.0;
    if (!hasTargetRegion && constraintPoints.isEmpty()) {
        return {};
    }

    QList<int> selectedIndices;
    QVector<QPair<double, int>> rankedCandidates;
    const QVector3D rankingCenter = hasTargetRegion ? region.origin : centroidOfPoints(constraintPoints);
    const double regionRadiusSquared = region.radiusMm * region.radiusMm;
    const double membershipRadiusMm = constraintMembershipRadiusMm(region);

    for (int index = 0; index < points.size(); ++index) {
        const RegistrationPoint& point = points[index];
        if (!point.isComplete()) {
            continue;
        }

        const bool inTargetRegion = hasTargetRegion
            && squaredDistance(point.sourcePosition, region.origin) <= regionRadiusSquared;
        const bool inConstraintRegion = matchesConstraintCloud(
            point.sourcePosition,
            constraintPoints,
            membershipRadiusMm);
        if (inTargetRegion || inConstraintRegion) {
            selectedIndices.append(index);
        }

        rankedCandidates.append(qMakePair(squaredDistance(point.sourcePosition, rankingCenter), index));
    }

    if (selectedIndices.size() >= 3) {
        return selectedIndices;
    }

    std::sort(
        rankedCandidates.begin(),
        rankedCandidates.end(),
        [](const QPair<double, int>& left, const QPair<double, int>& right) {
            return left.first < right.first;
        });

    for (const auto& candidate : rankedCandidates) {
        if (!selectedIndices.contains(candidate.second)) {
            selectedIndices.append(candidate.second);
        }
        if (selectedIndices.size() >= 3) {
            break;
        }
    }

    return selectedIndices;
}

bool trackingStatusFlag(const QVariantMap& status, const QString& key, bool defaultValue)
{
    if (status.contains(key)) {
        return status.value(key).toBool();
    }
    return defaultValue;
}

double trackingStatusDouble(const QVariantMap& status, const QString& key, double defaultValue)
{
    if (status.contains(key)) {
        return status.value(key).toDouble();
    }
    return defaultValue;
}

double trackingQualityRatio(const QVariantMap& status)
{
    if (status.contains(QStringLiteral("qualityScore"))) {
        return qBound(0.0, status.value(QStringLiteral("qualityScore")).toDouble() / 100.0, 1.0);
    }
    return qBound(0.0, trackingStatusDouble(status, QStringLiteral("quality"), 0.0), 1.0);
}

double trackingErrorMmFromStatus(const QVariantMap& status)
{
    if (status.contains(QStringLiteral("trackingErrorMm"))) {
        return status.value(QStringLiteral("trackingErrorMm")).toDouble();
    }
    if (status.contains(QStringLiteral("tracking_error_mm"))) {
        return status.value(QStringLiteral("tracking_error_mm")).toDouble();
    }
    return trackingStatusDouble(status, QStringLiteral("calibrationAccuracy"), 0.0);
}

bool trackingStatusCanSample(const QVariantMap& status)
{
    const bool visible = trackingStatusFlag(status, QStringLiteral("visible"), false);
    const bool calibrated = trackingStatusFlag(status, QStringLiteral("calibrated"), false);
    return visible && calibrated && trackingQualityRatio(status) >= 0.70;
}

ProbeTipFrameSample makeProbeTipFrameSample(
    const QVariantMap& status,
    const QList<double>& position,
    double timestampMs)
{
    ProbeTipFrameSample sample;
    sample.timestampMs = timestampMs;
    sample.trackingErrorMm = trackingErrorMmFromStatus(status);

    if (!trackingStatusCanSample(status) || position.size() < 3) {
        sample.valid = false;
        return sample;
    }

    sample.valid = true;
    sample.tipPositionMm = QVector3D(
        static_cast<float>(position.at(0)),
        static_cast<float>(position.at(1)),
        static_cast<float>(position.at(2)));
    return sample;
}

StableProbePointResult collectStableOpticalProbePoint(
    OpticalTrackingService* trackingService,
    const QString& sessionId,
    const QString& probeToolId)
{
    StableProbePointOptions options;
    options.minimumAcceptedFrames = 5;
    options.maxTrackingErrorMm = 0.50;
    options.maxJitterRmsMm = 0.35;

    QList<ProbeTipFrameSample> samples;
    samples.reserve(6);
    for (int frameIndex = 0; frameIndex < 6; ++frameIndex) {
        const QVariantMap status = trackingService->getToolStatus(sessionId, probeToolId);
        const QList<double> position = trackingService->getToolPosition(sessionId, probeToolId);
        samples.append(makeProbeTipFrameSample(
            status,
            position,
            static_cast<double>(QDateTime::currentMSecsSinceEpoch())));
    }

    return collectStableProbePoint(samples, options);
}

}

PointRegistrationServiceImpl::PointRegistrationServiceImpl(QObject* parent)
    : PointRegistrationService(parent)
    , m_transformMode(TransformMode::RigidBody)
    , m_hasValidResult(false)
    , m_renderingPaused(false)
    , m_probePointSource(ProbePointSource::Manual)
    , m_probeSimulator(nullptr)
    , m_segmentationService(nullptr)
    , m_trackingService(nullptr)
    , m_serviceRegistry(nullptr)
{
#ifdef VTK_FOUND
    m_landmarkTransform = vtkSmartPointer<vtkLandmarkTransform>::New();
#endif
    m_transformMatrix.setToIdentity();

    // 创建探针模拟器
    m_probeSimulator = new ProbeSimulator(this);
    m_probeSimulator->setDefaultTransform();

    // 初始化会话
    m_currentSession.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_currentSession.state = RegistrationSessionState::Idle;

    logMessage("INFO", QString::fromUtf8("点配准服务实例已创建，会话ID: %1").arg(m_currentSession.sessionId));
}

PointRegistrationServiceImpl::~PointRegistrationServiceImpl()
{
    executionOptionsStore().remove(this);
    logMessage("INFO", "点配准服务实例销毁中...");
    m_createdWidgets.clear();
    m_vtkWidgets.clear();
    logMessage("INFO", "点配准服务实例已销毁");
}

void PointRegistrationServiceImpl::invalidateRegistrationState()
{
    m_hasValidResult = false;
    m_lastResult = PointRegistrationResult();
    m_transformMatrix.setToIdentity();
}

// ========== 点管理实现 ==========

int PointRegistrationServiceImpl::addPoint(const QString& name)
{
    RegistrationPoint point;
    point.name = name.isEmpty() ? QString("P%1").arg(m_points.size() + 1) : name;
    m_points.append(point);
    int index = m_points.size() - 1;
    
    logMessage("INFO", QString("添加配准点: %1 (索引: %2)").arg(point.name).arg(index));
    emit pointAdded(index, point.name);
    return index;
}

bool PointRegistrationServiceImpl::removePoint(int index)
{
    if (index < 0 || index >= m_points.size()) {
        m_lastError = QString("无效的点索引: %1").arg(index);
        return false;
    }
    
    QString name = m_points[index].name;
    m_points.removeAt(index);
    invalidateRegistrationState();
    
    logMessage("INFO", QString("移除配准点: %1").arg(name));
    emit pointRemoved(index);
    return true;
}

void PointRegistrationServiceImpl::clearPoints()
{
    m_points.clear();
    invalidateRegistrationState();
    
    logMessage("INFO", "清空所有配准点");
    emit pointsCleared();
}

int PointRegistrationServiceImpl::pointCount() const
{
    return m_points.size();
}

RegistrationPoint PointRegistrationServiceImpl::getPoint(int index) const
{
    if (index < 0 || index >= m_points.size()) {
        return RegistrationPoint();
    }
    return m_points[index];
}

QVector<RegistrationPoint> PointRegistrationServiceImpl::getAllPoints() const
{
    return m_points;
}

bool PointRegistrationServiceImpl::setSourcePosition(int index, const QVector3D& position)
{
    if (index < 0 || index >= m_points.size()) {
        m_lastError = QString("无效的点索引: %1").arg(index);
        return false;
    }
    
    m_points[index].sourcePosition = position;
    m_points[index].hasSource = true;
    invalidateRegistrationState();
    
    logMessage("INFO", QString("设置源点 %1: (%2, %3, %4)")
        .arg(m_points[index].name)
        .arg(position.x(), 0, 'f', 2)
        .arg(position.y(), 0, 'f', 2)
        .arg(position.z(), 0, 'f', 2));
    
    emit pointUpdated(index);
    return true;
}

bool PointRegistrationServiceImpl::setTargetPosition(int index, const QVector3D& position)
{
    if (index < 0 || index >= m_points.size()) {
        m_lastError = QString("无效的点索引: %1").arg(index);
        return false;
    }
    
    m_points[index].targetPosition = position;
    m_points[index].hasTarget = true;
    invalidateRegistrationState();
    
    logMessage("INFO", QString("设置目标点 %1: (%2, %3, %4)")
        .arg(m_points[index].name)
        .arg(position.x(), 0, 'f', 2)
        .arg(position.y(), 0, 'f', 2)
        .arg(position.z(), 0, 'f', 2));
    
    emit pointUpdated(index);
    return true;
}

bool PointRegistrationServiceImpl::setPointName(int index, const QString& name)
{
    if (index < 0 || index >= m_points.size()) {
        m_lastError = QString("无效的点索引: %1").arg(index);
        return false;
    }
    
    m_points[index].name = name;
    emit pointUpdated(index);
    return true;
}

// ========== 配准执行实现 ==========

void PointRegistrationServiceImpl::setTransformMode(TransformMode mode)
{
    m_transformMode = mode;
    invalidateRegistrationState();
    logMessage("INFO", QString("设置变换模式: %1").arg(transformModeToString(mode)));
}

TransformMode PointRegistrationServiceImpl::getTransformMode() const
{
    return m_transformMode;
}

void PointRegistrationServiceImpl::setExecutionOptions(const PointRegistrationExecutionOptions& options)
{
    executionOptionsStore().insert(this, options);
    invalidateRegistrationState();
}

PointRegistrationExecutionOptions PointRegistrationServiceImpl::executionOptions() const
{
    return executionOptionsStore().value(this, PointRegistrationExecutionOptions{});
}

void PointRegistrationServiceImpl::setTargetRegistrationRegion(const TargetRegistrationRegion& region)
{
    m_targetRegion = region;
    invalidateRegistrationState();
}

void PointRegistrationServiceImpl::setPlanningConstraintContext(const QVariantMap& context)
{
    m_planningConstraintContext = context;
    invalidateRegistrationState();
}

void PointRegistrationServiceImpl::setPlanningConstraintRegions(
    const QMap<QString, QList<QVector3D>>& regions)
{
    m_planningConstraintRegions = regions;
    invalidateRegistrationState();
}

bool PointRegistrationServiceImpl::canExecuteRegistration() const
{
    int validCount = 0;
    for (const auto& point : m_points) {
        if (point.isComplete()) {
            validCount++;
        }
    }
    return validCount >= 3;
}

PointRegistrationResult PointRegistrationServiceImpl::executeRegistration()
{
    QElapsedTimer timer;
    timer.start();
    const PointRegistrationExecutionOptions options = executionOptions();
    const QString registrationMethodId = options.registrationMethodId;

    PointRegistrationResult result;
    result.timestamp = QDateTime::currentDateTime();

    emit registrationStarted();
    emit progressUpdated(0, "开始配准...");

    int validCount = 0;
    QList<QVector3D> coarseSourcePoints;
    QList<QVector3D> coarseTargetPoints;
    QList<double> coarseWeights;
    QList<int> completePointIndices;

    for (int pointIndex = 0; pointIndex < m_points.size(); ++pointIndex) {
        const auto& point = m_points[pointIndex];
        if (!point.isComplete()) {
            continue;
        }

        validCount++;
        completePointIndices.append(pointIndex);
        coarseSourcePoints.append(point.sourcePosition);
        coarseTargetPoints.append(point.targetPosition);
        coarseWeights.append(1.0);
    }

    if (validCount < 3) {
        result.errorMessage = QString("有效点对数量不足: %1 (至少需要3个)").arg(validCount);
        m_lastError = result.errorMessage;
        invalidateRegistrationState();
        logMessage("ERROR", result.errorMessage);
        emit registrationFailed(result.errorMessage);
        return result;
    }

    result.pointCount = validCount;
    emit progressUpdated(20, QString("找到 %1 个有效点对").arg(validCount));

    const WeightedRigidRegistrationResult coarseResult =
        AnkleRegistrationUtils::solveWeightedRigid(coarseSourcePoints, coarseTargetPoints, coarseWeights);
    RobustInitialTransformOptions robustInitialOptions;
    robustInitialOptions.inlierResidualThresholdMm = 5.0;
    robustInitialOptions.minimumInlierCount = 3;
    const RobustInitialTransformResult robustInitial =
        estimateRobustInitialTransform(coarseSourcePoints, coarseTargetPoints, robustInitialOptions);
    const bool useRobustInitial =
        robustInitial.success && m_transformMode == TransformMode::RigidBody;

    const QList<int> constrainedPairIndices =
        registrationMethodId == QStringLiteral("ankle_two_stage_constrained")
        ? selectConstraintRefinePairIndices(m_points, m_targetRegion, m_planningConstraintRegions)
        : QList<int> {};
    QList<QVector3D> constrainedTargetPoints;
    for (const int pointIndex : constrainedPairIndices) {
        if (pointIndex < 0 || pointIndex >= m_points.size() || !m_points[pointIndex].isComplete()) {
            continue;
        }
        constrainedTargetPoints.append(m_points[pointIndex].targetPosition);
    }
    const bool useConstraintRefinePoints =
        registrationMethodId == QStringLiteral("ankle_two_stage_constrained")
        && constrainedTargetPoints.size() >= 3;

#ifdef VTK_FOUND
    vtkSmartPointer<vtkPoints> sourcePoints = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkPoints> targetPoints = vtkSmartPointer<vtkPoints>::New();

    for (int i = 0; i < coarseSourcePoints.size(); ++i) {
        const QVector3D& sourcePoint = coarseSourcePoints[i];
        const QVector3D& targetPoint = coarseTargetPoints[i];
        sourcePoints->InsertNextPoint(sourcePoint.x(), sourcePoint.y(), sourcePoint.z());
        targetPoints->InsertNextPoint(targetPoint.x(), targetPoint.y(), targetPoint.z());
    }

    emit progressUpdated(40, "设置变换参数...");

    // 配置变换模式
    m_landmarkTransform->SetSourceLandmarks(sourcePoints);
    m_landmarkTransform->SetTargetLandmarks(targetPoints);

    switch (m_transformMode) {
        case TransformMode::RigidBody:
            m_landmarkTransform->SetModeToRigidBody();
            break;
        case TransformMode::Similarity:
            m_landmarkTransform->SetModeToSimilarity();
            break;
        case TransformMode::Affine:
            m_landmarkTransform->SetModeToAffine();
            break;
    }

    emit progressUpdated(60, "执行配准计算...");

    // 执行配准
    m_landmarkTransform->Update();

    auto finalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    finalMatrix->DeepCopy(m_landmarkTransform->GetMatrix());
    if (useRobustInitial) {
        copyQMatrixToVtkMatrix(robustInitial.transform, finalMatrix);
    }
    QString refineMethod = refineMethodForRegistrationMethod(registrationMethodId);
    bool delegatedGpuRefine = false;
    QVariantMap refineMetadata;
    double refineElapsedMs = -1.0;

    if (shouldDelegateSurfaceRefine(registrationMethodId) &&
        m_modelPolyData &&
        m_modelPolyData->GetNumberOfPoints() > 0) {
        if (auto* coreRegistrationService = registrationService()) {
            emit progressUpdated(70, QString::fromUtf8("委托 RegistrationCore 执行表面精配准..."));

            const QList<QVector3D> refineTargetPoints =
                useConstraintRefinePoints ? constrainedTargetPoints : coarseTargetPoints;
            auto probePointCloud = buildProbePointCloud(refineTargetPoints);
            auto coarseCtToPhysical = vtkSmartPointer<vtkMatrix4x4>::New();
            coarseCtToPhysical->DeepCopy(finalMatrix);

            auto coarsePhysicalToCt = vtkSmartPointer<vtkMatrix4x4>::New();
            vtkMatrix4x4::Invert(coarseCtToPhysical, coarsePhysicalToCt);
            const QString coreRegistrationId =
                QStringLiteral("point_registration_gpu_%1").arg(m_currentSession.sessionId);

            QVariantMap parameters;
            parameters.insert(QStringLiteral("useGPU"), shouldUseGpuRefine(registrationMethodId));
            parameters.insert(QStringLiteral("registrationId"), coreRegistrationId);
            parameters.insert(QStringLiteral("initialTransform"), vtkMatrixToVariantList(coarsePhysicalToCt));
            parameters.insert(QStringLiteral("maxIterations"), 50);
            parameters.insert(QStringLiteral("distanceThreshold"), 10.0);
            parameters.insert(QStringLiteral("usePointToPlane"), shouldUseGpuRefine(registrationMethodId));
            parameters.insert(QStringLiteral("registrationMethodId"), registrationMethodId);
            parameters.insert(QStringLiteral("enableParallelInitialSearch"), options.enableParallelInitialSearch);
            parameters.insert(QStringLiteral("enableConstraintParallelFilter"), options.enableConstraintParallelFilter);
            parameters.insert(QStringLiteral("candidateCount"), options.candidateCount);
            parameters.insert(QStringLiteral("topKCandidateCount"), options.topKCandidateCount);
            parameters.insert(QStringLiteral("multiResolutionProfileId"), options.multiResolutionProfileId);
            parameters.insert(QStringLiteral("constrainedPointCount"), refineTargetPoints.size());
            parameters.insert(QStringLiteral("constraintRefineUsed"), useConstraintRefinePoints);
            parameters.insert(QStringLiteral("constraintRegionCount"), m_planningConstraintRegions.size());
            parameters.insert(
                QStringLiteral("initialTransformSource"),
                useRobustInitial ? QStringLiteral("probe_guided_robust") : QStringLiteral("weighted_landmark"));
            parameters.insert(QStringLiteral("robustInitialAvailable"), robustInitial.success);
            parameters.insert(QStringLiteral("robustInitialConfidence"), robustInitial.confidence);
            parameters.insert(QStringLiteral("robustInitialInlierCount"), robustInitial.inlierCount);
            parameters.insert(QStringLiteral("robustInitialOutlierCount"), robustInitial.rejectedOutlierCount);
            parameters.insert(QStringLiteral("robustInitialRmsMm"), robustInitial.inlierRmsMm);
            parameters.insert(QStringLiteral("robustInitialReason"), robustInitial.reason);
            parameters.insert(QStringLiteral("pairedResidualSourcePoints"), vectorListToVariantList(coarseSourcePoints));
            parameters.insert(QStringLiteral("pairedResidualTargetPoints"), vectorListToVariantList(coarseTargetPoints));
            parameters.insert(QStringLiteral("pairedResidualSourcePointsFlat"), vectorListToFlatVariantList(coarseSourcePoints));
            parameters.insert(QStringLiteral("pairedResidualTargetPointsFlat"), vectorListToFlatVariantList(coarseTargetPoints));
            parameters.insert(QStringLiteral("enablePairedResidualGuard"), true);
            parameters.insert(
                QStringLiteral("constraintRegionKeys"),
                m_planningConstraintRegions.keys().join(QStringLiteral("|")));
            if (m_targetRegion.radiusMm > 0.0) {
                parameters.insert(QStringLiteral("targetRegionCenterX"), m_targetRegion.origin.x());
                parameters.insert(QStringLiteral("targetRegionCenterY"), m_targetRegion.origin.y());
                parameters.insert(QStringLiteral("targetRegionCenterZ"), m_targetRegion.origin.z());
                parameters.insert(QStringLiteral("targetRegionRadiusMm"), m_targetRegion.radiusMm);
            }
            if (!m_planningConstraintContext.isEmpty()) {
                for (auto it = m_planningConstraintContext.cbegin(); it != m_planningConstraintContext.cend(); ++it) {
                    parameters.insert(it.key(), it.value());
                }
            }
            if (!m_planningConstraintRegions.isEmpty()) {
                QVariantMap serializedRegions;
                for (auto it = m_planningConstraintRegions.cbegin(); it != m_planningConstraintRegions.cend(); ++it) {
                    serializedRegions.insert(it.key(), vectorListToVariantList(it.value()));
                }
                parameters.insert(QStringLiteral("constraintRegions"), serializedRegions);
            }
            if (registrationMethodId == QStringLiteral("ankle_two_stage_constrained")) {
                parameters.insert(QStringLiteral("curvatureWeightMode"), 4);
            }

            QElapsedTimer refineTimer;
            refineTimer.start();
            if (auto refinedPhysicalToCt =
                    coreRegistrationService->performICPRegistrationAdvanced(
                        probePointCloud, m_modelPolyData, parameters)) {
                refineElapsedMs = static_cast<double>(refineTimer.elapsed());
                vtkMatrix4x4::Invert(refinedPhysicalToCt, finalMatrix);
                delegatedGpuRefine = shouldUseGpuRefine(registrationMethodId);
                refineMetadata =
                    coreRegistrationService->getRegistrationInfo(coreRegistrationId)
                        .value(QStringLiteral("metadata")).toMap();
                applyParallelSearchMetadataMetrics(result.metrics, refineMetadata);
                logMessage("INFO", QString::fromUtf8("RegistrationCore 表面精配准已接管当前主链"));
            } else {
                refineElapsedMs = static_cast<double>(refineTimer.elapsed());
                refineMethod = QStringLiteral("weighted_landmark_only");
                logMessage("WARNING", QString::fromUtf8("RegistrationCore 表面精配准失败，回退到加权地标结果"));
            }
        } else {
            logMessage("WARNING", QString::fromUtf8("RegistrationService 不可用，保持加权地标配准结果"));
        }
    }

    // 转换为QMatrix4x4
    m_transformMatrix = vtkMatrixToQMatrix(finalMatrix);
    result.transformMatrix = m_transformMatrix;

    // 提取平移分量
    result.translationX = finalMatrix->GetElement(0, 3);
    result.translationY = finalMatrix->GetElement(1, 3);
    result.translationZ = finalMatrix->GetElement(2, 3);

    // 计算欧拉角
    calculateEulerAngles(m_transformMatrix, result.rotationX, result.rotationY, result.rotationZ);

    emit progressUpdated(80, "计算配准精度...");

    double sumSquaredError = 0.0;
    result.maxError = 0.0;
    double sumError = 0.0;
    double constrainedSquaredError = 0.0;
    int constrainedErrorCount = 0;

    for (const int pointIndex : completePointIndices) {
        const auto& point = m_points[pointIndex];
        const double error = calculatePointError(point.sourcePosition, point.targetPosition, m_transformMatrix);
        result.pointErrors.append(error);
        sumSquaredError += error * error;
        sumError += error;
        if (error > result.maxError) {
            result.maxError = error;
        }
        if (constrainedPairIndices.contains(pointIndex)) {
            constrainedSquaredError += error * error;
            ++constrainedErrorCount;
        }
    }

    result.rmsError = qSqrt(sumSquaredError / validCount);
    result.meanError = sumError / validCount;
    result.success = true;
    m_hasValidResult = true;

    QList<int> roiPointIndices;
    if (m_targetRegion.radiusMm > 0.0) {
        roiPointIndices =
            AnkleRegistrationUtils::selectRoiPointIndices(
                coarseSourcePoints,
                m_targetRegion.origin,
                m_targetRegion.radiusMm);
    } else if (!coarseSourcePoints.isEmpty()) {
        roiPointIndices =
            AnkleRegistrationUtils::selectRoiPointIndices(coarseSourcePoints, coarseSourcePoints.first(), 30.0);
    }

    if (constrainedErrorCount > 0) {
        result.targetRegionTre = qSqrt(constrainedSquaredError / constrainedErrorCount);
    } else {
        result.targetRegionTre = delegatedGpuRefine
            ? result.rmsError
            : (coarseResult.success ? coarseResult.weightedRmsError : result.rmsError);
    }
    if (useConstraintRefinePoints) {
        result.coverageScore =
            validCount > 0 ? qMin(1.0, static_cast<double>(constrainedTargetPoints.size()) / validCount) : 0.0;
    } else {
        result.coverageScore =
            validCount > 0 ? qMin(1.0, static_cast<double>(roiPointIndices.size()) / validCount) : 0.0;
    }
    result.metrics.insert(QStringLiteral("registration_mode"), registrationMethodId);
    result.metrics.insert(QStringLiteral("coarse_method"), QStringLiteral("weighted_landmark"));
    result.metrics.insert(
        QStringLiteral("initial_method"),
        useRobustInitial ? QStringLiteral("probe_guided_robust") : QStringLiteral("weighted_landmark"));
    result.metrics.insert(QStringLiteral("initial_confidence"), robustInitial.confidence);
    result.metrics.insert(QStringLiteral("initial_inlier_count"), robustInitial.inlierCount);
    result.metrics.insert(QStringLiteral("initial_outlier_count"), robustInitial.rejectedOutlierCount);
    result.metrics.insert(QStringLiteral("initial_rms_mm"), robustInitial.inlierRmsMm);
    result.metrics.insert(QStringLiteral("initial_quality_reason"), robustInitial.reason);
    result.metrics.insert(QStringLiteral("refine_method"), refineMethod);
    result.metrics.insert(QStringLiteral("delegated_gpu_refine"), delegatedGpuRefine);
    result.metrics.insert(QStringLiteral("coarse_rms"), coarseResult.weightedRmsError);
    result.metrics.insert(QStringLiteral("refined_rms"), result.rmsError);
    result.metrics.insert(QStringLiteral("roi_point_count"), roiPointIndices.size());
    result.metrics.insert(QStringLiteral("constraint_refine_used"), useConstraintRefinePoints);
    result.metrics.insert(
        QStringLiteral("constraint_refine_pair_count"),
        useConstraintRefinePoints ? constrainedTargetPoints.size() : 0);
    result.metrics.insert(QStringLiteral("constraint_payload_region_count"), m_planningConstraintRegions.size());
    result.metrics.insert(QStringLiteral("coarse_translation_x"), coarseResult.translation.x());
    result.metrics.insert(QStringLiteral("coarse_translation_y"), coarseResult.translation.y());
    result.metrics.insert(QStringLiteral("coarse_translation_z"), coarseResult.translation.z());
    insertMetricFallback(
        result.metrics,
        QStringLiteral("candidate_count"),
        refineMetadata.value(QStringLiteral("candidateCount"), options.candidateCount));
    insertMetricFallback(
        result.metrics,
        QStringLiteral("top_k_count"),
        refineMetadata.value(QStringLiteral("topKCount"), options.topKCandidateCount));
    insertMetricFallback(
        result.metrics,
        QStringLiteral("parallel_search_enabled"),
        refineMetadata.value(
            QStringLiteral("parallelSearchEnabled"),
            options.enableParallelInitialSearch));
    insertMetricFallback(
        result.metrics,
        QStringLiteral("multi_resolution_profile"),
        refineMetadata.value(
            QStringLiteral("multiResolutionProfile"),
            options.multiResolutionProfileId));
    result.durationMs = timer.elapsed();
    insertMetricFallback(
        result.metrics,
        QStringLiteral("refine_ms"),
        refineMetadata.contains(QStringLiteral("elapsedMs"))
            ? refineMetadata.value(QStringLiteral("elapsedMs"))
            : QVariant(refineElapsedMs >= 0.0 ? refineElapsedMs : 0.0));

    logMessage("INFO",
               QStringLiteral("配准成功: RMS=%1 mm, MaxErr=%2 mm, 耗时=%3 ms")
                   .arg(result.rmsError, 0, 'f', 3)
                   .arg(result.maxError, 0, 'f', 3)
                   .arg(result.durationMs, 0, 'f', 1));

#else
    result.errorMessage = "VTK未启用，无法执行配准";
    m_lastError = result.errorMessage;
    invalidateRegistrationState();
    logMessage("ERROR", result.errorMessage);
    emit registrationFailed(result.errorMessage);
    return result;
#endif

    m_lastResult = result;
    emit progressUpdated(100, "配准完成");
    emit registrationCompleted(result);
    return result;
}

PointRegistrationResult PointRegistrationServiceImpl::getLastResult() const
{
    return m_lastResult;
}

QMatrix4x4 PointRegistrationServiceImpl::getTransformMatrix() const
{
    return m_transformMatrix;
}

QVector3D PointRegistrationServiceImpl::transformPoint(const QVector3D& point) const
{
    if (!m_hasValidResult) {
        return point;
    }
    return m_transformMatrix.map(point);
}

// ========== Widget工厂实现 ==========

QWidget* PointRegistrationServiceImpl::createRegistrationWidget(QWidget* parent)
{
    logMessage("INFO", "创建配准Widget...");
    cleanupDestroyedWidgets();

    try {
        PointRegistrationWidget* widget = new PointRegistrationWidget(this, parent);
        m_createdWidgets.append(QPointer<QWidget>(widget));

        logMessage("INFO", QString("Widget创建成功，当前跟踪 %1 个Widget").arg(m_createdWidgets.size()));
        return widget;
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("创建Widget异常: %1").arg(e.what()));
        return nullptr;
    } catch (...) {
        logMessage("ERROR", "创建Widget时发生未知异常");
        return nullptr;
    }
}

// ========== 纯VTK Widget工厂实现 ==========

QWidget* PointRegistrationServiceImpl::createVTKWidget(QWidget* parent)
{
    logMessage("INFO", "创建纯VTK Widget...");

    // 清理已销毁的Widget
    m_vtkWidgets.removeAll(QPointer<QWidget>());

    try {
        PointRegistrationVTKWidget* widget = new PointRegistrationVTKWidget(this, parent);
        m_vtkWidgets.append(QPointer<QWidget>(widget));

#ifdef VTK_FOUND
        // If a model is already loaded before the view is created (e.g. loaded from planning tab),
        // push it into the newly created VTK widget so the user can immediately pick points.
        if (m_modelPolyData && m_modelPolyData->GetNumberOfPoints() > 0) {
            widget->loadModel(m_modelPolyData, m_modelName);
        }
#endif

        logMessage("INFO", QString("VTK Widget创建成功，当前跟踪 %1 个VTK Widget").arg(m_vtkWidgets.size()));
        return widget;
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("创建VTK Widget异常: %1").arg(e.what()));
        return nullptr;
    } catch (...) {
        logMessage("ERROR", "创建VTK Widget时发生未知异常");
        return nullptr;
    }
}

// ========== VTK渲染控制实现 ==========

void PointRegistrationServiceImpl::pauseRendering()
{
    m_renderingPaused = true;

    // 暂停所有VTK Widget
    for (auto& widgetPtr : m_vtkWidgets) {
        if (widgetPtr) {
            PointRegistrationVTKWidget* vtkWidget = qobject_cast<PointRegistrationVTKWidget*>(widgetPtr.data());
            if (vtkWidget) {
                vtkWidget->pauseRendering();
            }
        }
    }

    // 也暂停旧版Widget
    for (auto& widgetPtr : m_createdWidgets) {
        if (widgetPtr) {
            widgetPtr->setUpdatesEnabled(false);
        }
    }

    logMessage("INFO", "VTK渲染已暂停");
}

void PointRegistrationServiceImpl::resumeRendering()
{
    m_renderingPaused = false;

    // 恢复所有VTK Widget
    for (auto& widgetPtr : m_vtkWidgets) {
        if (widgetPtr) {
            PointRegistrationVTKWidget* vtkWidget = qobject_cast<PointRegistrationVTKWidget*>(widgetPtr.data());
            if (vtkWidget) {
                vtkWidget->resumeRendering();
            }
        }
    }

    // 也恢复旧版Widget
    for (auto& widgetPtr : m_createdWidgets) {
        if (widgetPtr) {
            widgetPtr->setUpdatesEnabled(true);
            widgetPtr->update();
        }
    }

    logMessage("INFO", "VTK渲染已恢复");
}

// ========== 模型加载实现 ==========

bool PointRegistrationServiceImpl::loadModelFromSegmentation(const QString& segmentationTaskId,
                                                              const QString& bodyPart)
{
#ifdef VTK_FOUND
    if (!m_segmentationService) {
        m_lastError = QString::fromUtf8("分割服务未设置");
        logMessage("ERROR", m_lastError);
        emit modelLoaded(false, m_lastError);
        return false;
    }

    m_currentSession.state = RegistrationSessionState::ModelLoading;
    emit sessionStateChanged(m_currentSession.state);

    // 注意：此处需要根据实际的 SegmentationService 接口调用
    // vtkSmartPointer<vtkPolyData> polyData = m_segmentationService->getSegmentationMesh(segmentationTaskId, bodyPart);
    // 暂时使用占位实现
    m_lastError = QString::fromUtf8("分割服务集成待实现");
    logMessage("WARNING", m_lastError);

    m_modelSource = segmentationTaskId;
    m_modelName = bodyPart.isEmpty() ? QString::fromUtf8("分割模型") : bodyPart;

    // TODO: 实际加载实现
    emit modelLoaded(false, m_lastError);
    return false;
#else
    m_lastError = QString::fromUtf8("VTK未启用");
    emit modelLoaded(false, m_lastError);
    return false;
#endif
}

bool PointRegistrationServiceImpl::loadModelFromPolyData(vtkPolyData* polyData,
                                                          const QString& modelName)
{
#ifdef VTK_FOUND
    if (!polyData) {
        m_lastError = QString::fromUtf8("无效的 vtkPolyData 指针");
        logMessage("ERROR", m_lastError);
        emit modelLoaded(false, m_lastError);
        return false;
    }

    m_currentSession.state = RegistrationSessionState::ModelLoading;
    emit sessionStateChanged(m_currentSession.state);

    // 复制数据
    m_modelPolyData = vtkSmartPointer<vtkPolyData>::New();
    m_modelPolyData->DeepCopy(polyData);

    m_modelName = modelName.isEmpty() ? QString::fromUtf8("外部模型") : modelName;
    m_modelSource = QString::fromUtf8("PolyData");

    int numPoints = m_modelPolyData->GetNumberOfPoints();
    int numCells = m_modelPolyData->GetNumberOfCells();

    QString info = QString::fromUtf8("模型加载成功: %1 (%2 点, %3 面)")
                       .arg(m_modelName)
                       .arg(numPoints)
                       .arg(numCells);
    logMessage("INFO", info);

    // 更新所有 VTK Widget
    for (auto& widgetPtr : m_vtkWidgets) {
        if (widgetPtr) {
            PointRegistrationVTKWidget* vtkWidget = qobject_cast<PointRegistrationVTKWidget*>(widgetPtr.data());
            if (vtkWidget) {
                vtkWidget->loadModel(m_modelPolyData);
            }
        }
    }

    m_currentSession.state = RegistrationSessionState::PointCollection;
    m_currentSession.modelSource = m_modelSource;
    emit sessionStateChanged(m_currentSession.state);
    emit modelLoaded(true, info);
    return true;
#else
    m_lastError = QString::fromUtf8("VTK未启用");
    emit modelLoaded(false, m_lastError);
    return false;
#endif
}

bool PointRegistrationServiceImpl::loadModelFromFile(const QString& filePath)
{
#ifdef VTK_FOUND
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        m_lastError = QString::fromUtf8("文件不存在: %1").arg(filePath);
        logMessage("ERROR", m_lastError);
        emit modelLoaded(false, m_lastError);
        return false;
    }

    m_currentSession.state = RegistrationSessionState::ModelLoading;
    emit sessionStateChanged(m_currentSession.state);

    QString suffix = fileInfo.suffix().toLower();

    if (suffix == "stl") {
        vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
        reader->SetFileName(filePath.toStdString().c_str());
        reader->Update();

        m_modelPolyData = reader->GetOutput();

        if (!m_modelPolyData || m_modelPolyData->GetNumberOfPoints() == 0) {
            m_lastError = QString::fromUtf8("STL文件读取失败或为空");
            logMessage("ERROR", m_lastError);
            m_currentSession.state = RegistrationSessionState::Failed;
            emit sessionStateChanged(m_currentSession.state);
            emit modelLoaded(false, m_lastError);
            return false;
        }

        m_modelName = fileInfo.baseName();
        m_modelSource = filePath;

        int numPoints = m_modelPolyData->GetNumberOfPoints();
        int numCells = m_modelPolyData->GetNumberOfCells();

        QString info = QString::fromUtf8("STL模型加载成功: %1 (%2 点, %3 面)")
                           .arg(m_modelName)
                           .arg(numPoints)
                           .arg(numCells);
        logMessage("INFO", info);

        // 更新所有 VTK Widget
        for (auto& widgetPtr : m_vtkWidgets) {
            if (widgetPtr) {
                PointRegistrationVTKWidget* vtkWidget = qobject_cast<PointRegistrationVTKWidget*>(widgetPtr.data());
                if (vtkWidget) {
                    vtkWidget->loadModel(m_modelPolyData);
                }
            }
        }

        m_currentSession.state = RegistrationSessionState::PointCollection;
        m_currentSession.modelSource = filePath;
        emit sessionStateChanged(m_currentSession.state);
        emit modelLoaded(true, info);
        return true;
    } else {
        m_lastError = QString::fromUtf8("不支持的文件格式: %1").arg(suffix);
        logMessage("ERROR", m_lastError);
        emit modelLoaded(false, m_lastError);
        return false;
    }
#else
    m_lastError = QString::fromUtf8("VTK未启用");
    emit modelLoaded(false, m_lastError);
    return false;
#endif
}

QString PointRegistrationServiceImpl::getModelInfo() const
{
#ifdef VTK_FOUND
    if (!m_modelPolyData) {
        return QString::fromUtf8("未加载模型");
    }

    return QString::fromUtf8("模型: %1\n来源: %2\n点数: %3\n面数: %4")
               .arg(m_modelName)
               .arg(m_modelSource)
               .arg(m_modelPolyData->GetNumberOfPoints())
               .arg(m_modelPolyData->GetNumberOfCells());
#else
    return QString::fromUtf8("VTK未启用");
#endif
}

bool PointRegistrationServiceImpl::hasModel() const
{
#ifdef VTK_FOUND
    return m_modelPolyData != nullptr && m_modelPolyData->GetNumberOfPoints() > 0;
#else
    return false;
#endif
}

// ========== 探针点采集实现 ==========

void PointRegistrationServiceImpl::setProbePointSource(ProbePointSource source)
{
    m_probePointSource = source;
    m_currentSession.probeSource = source;
    logMessage("INFO", QString::fromUtf8("设置探针数据来源: %1").arg(probeSourceToString(source)));
}

ProbePointSource PointRegistrationServiceImpl::getProbePointSource() const
{
    return m_probePointSource;
}

bool PointRegistrationServiceImpl::captureProbePoint(int pointIndex)
{
    if (pointIndex < 0 || pointIndex >= m_points.size()) {
        m_lastError = QString::fromUtf8("无效的点索引: %1").arg(pointIndex);
        return false;
    }

    QVector3D probePosition;

    switch (m_probePointSource) {
        case ProbePointSource::Manual:
            // 手动模式：使用已设置的 targetPosition
            if (m_points[pointIndex].hasTarget) {
                probePosition = m_points[pointIndex].targetPosition;
            } else {
                m_lastError = QString::fromUtf8("手动模式下需要先设置目标点坐标");
                return false;
            }
            break;

        case ProbePointSource::Simulated:
            // 模拟模式：根据 CT 点生成模拟探针点
            if (!m_points[pointIndex].hasSource) {
                m_lastError = QString::fromUtf8("模拟模式需要先设置源点（CT点）");
                return false;
            }
            probePosition = m_probeSimulator->generateProbePoint(m_points[pointIndex].sourcePosition);
            setTargetPosition(pointIndex, probePosition);
            break;

        case ProbePointSource::OpticalTracking:
            if (!m_trackingService) {
                m_lastError = QString::fromUtf8("光学跟踪服务未设置");
                return false;
            }
            if (m_trackingSessionId.isEmpty() || m_probeToolId.isEmpty()) {
                m_lastError = QString::fromUtf8("光学跟踪会话或探针工具未设置");
                return false;
            }
            {
                const StableProbePointResult stablePoint =
                    collectStableOpticalProbePoint(m_trackingService, m_trackingSessionId, m_probeToolId);
                if (!stablePoint.accepted) {
                    m_lastError = QString::fromUtf8("光学探针稳定采点失败: %1").arg(stablePoint.reason);
                    return false;
                }
                probePosition = stablePoint.pointMm;
                m_currentProbePosition = probePosition;
                logMessage(
                    "INFO",
                    QString::fromUtf8("光学探针稳定采点: frames=%1, rejected=%2, jitter=%3mm, confidence=%4")
                        .arg(stablePoint.acceptedFrameCount)
                        .arg(stablePoint.rejectedFrameCount)
                        .arg(stablePoint.jitterRmsMm, 0, 'f', 3)
                        .arg(stablePoint.confidence, 0, 'f', 3));
            }
            if (probePosition.isNull()) {
                m_lastError = QString::fromUtf8("光学探针稳定采点结果为空");
                return false;
            }
            setTargetPosition(pointIndex, probePosition);
            break;
    }

    logMessage("INFO", QString::fromUtf8("采集探针点 %1: (%2, %3, %4)")
                   .arg(m_points[pointIndex].name)
                   .arg(probePosition.x(), 0, 'f', 2)
                   .arg(probePosition.y(), 0, 'f', 2)
                   .arg(probePosition.z(), 0, 'f', 2));

    emit probePointCaptured(pointIndex, probePosition);
    return true;
}

void PointRegistrationServiceImpl::setTrackingSession(const QString& sessionId,
                                                       const QString& probeToolId)
{
    m_trackingSessionId = sessionId;
    m_probeToolId = probeToolId;
    m_currentSession.trackingSessionId = sessionId;
    m_currentSession.probeToolId = probeToolId;

    logMessage("INFO", QString::fromUtf8("设置跟踪会话: %1, 探针工具: %2")
                   .arg(sessionId, probeToolId));
}

QVector3D PointRegistrationServiceImpl::getCurrentProbePosition() const
{
    if (m_probePointSource == ProbePointSource::OpticalTracking) {
        if (!m_trackingService || m_trackingSessionId.isEmpty() || m_probeToolId.isEmpty()) {
            return QVector3D();
        }

        const QVariantMap status = m_trackingService->getToolStatus(m_trackingSessionId, m_probeToolId);
        const QList<double> position = m_trackingService->getToolPosition(m_trackingSessionId, m_probeToolId);
        if (!trackingStatusCanSample(status) || position.size() < 3) {
            return QVector3D();
        }
        return QVector3D(
            static_cast<float>(position.at(0)),
            static_cast<float>(position.at(1)),
            static_cast<float>(position.at(2)));
    }

    return m_currentProbePosition;
}

// ========== 模拟数据实现 ==========

QVector3D PointRegistrationServiceImpl::generateSimulatedProbePoint(int pointIndex, double noiseLevel)
{
    if (pointIndex < 0 || pointIndex >= m_points.size()) {
        m_lastError = QString::fromUtf8("无效的点索引: %1").arg(pointIndex);
        return QVector3D();
    }

    if (!m_points[pointIndex].hasSource) {
        m_lastError = QString::fromUtf8("点 %1 未设置源点坐标").arg(m_points[pointIndex].name);
        return QVector3D();
    }

    m_probeSimulator->setNoiseLevel(noiseLevel);
    QVector3D probePoint = m_probeSimulator->generateProbePoint(m_points[pointIndex].sourcePosition);

    // 自动设置目标点
    setTargetPosition(pointIndex, probePoint);

    logMessage("INFO", QString::fromUtf8("生成模拟探针点 %1: (%2, %3, %4) 噪声水平: %5mm")
                   .arg(m_points[pointIndex].name)
                   .arg(probePoint.x(), 0, 'f', 2)
                   .arg(probePoint.y(), 0, 'f', 2)
                   .arg(probePoint.z(), 0, 'f', 2)
                   .arg(noiseLevel, 0, 'f', 2));

    emit probePointCaptured(pointIndex, probePoint);
    return probePoint;
}

int PointRegistrationServiceImpl::generateAllSimulatedProbePoints(double noiseLevel)
{
    int generatedCount = 0;

    m_probeSimulator->setNoiseLevel(noiseLevel);

    for (int i = 0; i < m_points.size(); ++i) {
        if (m_points[i].hasSource) {
            QVector3D probePoint = m_probeSimulator->generateProbePoint(m_points[i].sourcePosition);
            setTargetPosition(i, probePoint);
            emit probePointCaptured(i, probePoint);
            generatedCount++;
        }
    }

    logMessage("INFO", QString::fromUtf8("批量生成模拟探针点: %1/%2 个, 噪声水平: %3mm")
                   .arg(generatedCount)
                   .arg(m_points.size())
                   .arg(noiseLevel, 0, 'f', 2));

    return generatedCount;
}

void PointRegistrationServiceImpl::setSimulationTransform(const QMatrix4x4& transform)
{
    m_probeSimulator->setTransformMatrix(transform);
    logMessage("INFO", QString::fromUtf8("设置模拟变换矩阵"));
}

QMatrix4x4 PointRegistrationServiceImpl::getSimulationTransform() const
{
    return m_probeSimulator->getTransformMatrix();
}

// ========== 配准应用实现 ==========

bool PointRegistrationServiceImpl::applyRegistrationToNavigation(const QString& registrationId)
{
    if (!m_hasValidResult) {
        m_lastError = QString::fromUtf8("没有有效的配准结果可应用");
        return false;
    }

    // 更新会话状态
    m_currentSession.registrationMatrix = m_transformMatrix;
    m_currentSession.result = m_lastResult;
    m_currentSession.completedAt = QDateTime::currentDateTime();
    m_currentSession.state = RegistrationSessionState::Completed;

    logMessage("INFO", QString::fromUtf8("配准结果已应用，配准ID: %1, RMS误差: %2mm")
                   .arg(registrationId)
                   .arg(m_lastResult.rmsError, 0, 'f', 3));

    emit sessionStateChanged(m_currentSession.state);
    emit registrationApplied(registrationId);
    return true;
}

RegistrationSession PointRegistrationServiceImpl::getCurrentSession() const
{
    return m_currentSession;
}

// ========== 服务依赖注入 ==========

void PointRegistrationServiceImpl::setSegmentationService(SegmentationService* service)
{
    m_segmentationService = service;
    logMessage("INFO", QString::fromUtf8("分割服务已设置: %1")
                   .arg(service ? "有效" : "空"));
}

void PointRegistrationServiceImpl::setTrackingService(OpticalTrackingService* service)
{
    m_trackingService = service;
    logMessage("INFO", QString::fromUtf8("跟踪服务已设置: %1")
                   .arg(service ? "有效" : "空"));
}

void PointRegistrationServiceImpl::setServiceRegistry(PlatformServiceRegistry* serviceRegistry)
{
    m_serviceRegistry = serviceRegistry;
    logMessage("INFO", QString::fromUtf8("平台服务注册表已设置: %1")
                   .arg(serviceRegistry ? "有效" : "空"));
}

// ========== 错误处理实现 ==========

QString PointRegistrationServiceImpl::getLastError() const
{
    return m_lastError;
}

// ========== 私有方法 ==========

double PointRegistrationServiceImpl::calculatePointError(
    const QVector3D& source, const QVector3D& target, const QMatrix4x4& transform) const
{
    QVector3D transformed = transform.map(source);
    return (transformed - target).length();
}

void PointRegistrationServiceImpl::calculateEulerAngles(
    const QMatrix4x4& matrix, double& rx, double& ry, double& rz) const
{
    // 从旋转矩阵提取ZYX欧拉角
    double r00 = matrix(0, 0);
    double r10 = matrix(1, 0);
    double r20 = matrix(2, 0);
    double r21 = matrix(2, 1);
    double r22 = matrix(2, 2);

    // 计算Y轴旋转角
    ry = qAsin(-r20);

    // 检查是否在奇异点附近
    if (qAbs(qCos(ry)) > 1e-6) {
        rx = qAtan2(r21, r22);
        rz = qAtan2(r10, r00);
    } else {
        // 奇异点处理
        rx = qAtan2(-matrix(1, 2), matrix(1, 1));
        rz = 0;
    }

    // 转换为度
    rx = qRadiansToDegrees(rx);
    ry = qRadiansToDegrees(ry);
    rz = qRadiansToDegrees(rz);
}

void PointRegistrationServiceImpl::cleanupDestroyedWidgets()
{
    m_createdWidgets.removeAll(QPointer<QWidget>());
}

void PointRegistrationServiceImpl::logMessage(const QString& level, const QString& message) const
{
    QString formattedMsg = QString("[PointRegistrationService][%1] %2").arg(level, message);

    if (level == "ERROR") {
        qCritical().noquote() << formattedMsg;
    } else if (level == "WARNING") {
        qWarning().noquote() << formattedMsg;
    } else {
        qDebug().noquote() << formattedMsg;
    }
}

registration_core::RegistrationService* PointRegistrationServiceImpl::registrationService() const
{
    if (!m_serviceRegistry) {
        return nullptr;
    }

    return dynamic_cast<registration_core::RegistrationService*>(
        m_serviceRegistry->service(QStringLiteral("RegistrationService")));
}
