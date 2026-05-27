#include "UI/NewPages/Navigation/navigation_runtime_coordinator.h"

#include "Framework/Navigation/navigation_digital_twin_state_builder.h"
#include "Framework/Navigation/navigation_evaluation_service.h"

namespace
{
const QVariantMap mergeTrackingQuality(const QVariantMap& currentTrackingQuality, const QVariantMap& updates)
{
    QVariantMap mergedTrackingQuality = currentTrackingQuality;

    for (auto it = updates.cbegin(); it != updates.cend(); ++it) {
        mergedTrackingQuality.insert(it.key(), it.value());
    }

    return mergedTrackingQuality;
}

QString buildPoseSummary(const NavigationTransformResult& transformResult)
{
    if (!transformResult.valid) {
        return transformResult.failureCode;
    }

    const QVector3D translation = transformResult.vtkToolTransform.column(3).toVector3D();
    return QStringLiteral("tx=%1,ty=%2,tz=%3")
        .arg(translation.x(), 0, 'f', 2)
        .arg(translation.y(), 0, 'f', 2)
        .arg(translation.z(), 0, 'f', 2);
}
}

NavigationRuntimeCoordinator::NavigationRuntimeCoordinator(
    NavigationRuntimeState* runtimeState,
    PersistenceActions persistenceActions)
    : m_runtimeState(runtimeState)
    , m_persistenceActions(std::move(persistenceActions))
{
    if (!m_persistenceActions.loadEvaluationSnapshot
        || !m_persistenceActions.saveEvaluationReport
        || !m_persistenceActions.exportMetricsCsv
        || !m_persistenceActions.exportCaseSummary) {
        m_persistenceActions = createDefaultPersistenceActions();
    }
}

NavigationRuntimeState* NavigationRuntimeCoordinator::runtimeState() const
{
    return m_runtimeState;
}

void NavigationRuntimeCoordinator::setCasesRoot(const QString& casesRoot)
{
    m_casesRoot = casesRoot;
    if (!m_persistenceActions.loadEvaluationSnapshot
        || !m_persistenceActions.saveEvaluationReport
        || !m_persistenceActions.exportMetricsCsv
        || !m_persistenceActions.exportCaseSummary) {
        m_persistenceActions = createDefaultPersistenceActions();
    }
}

void NavigationRuntimeCoordinator::setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& targetRegionDefinition)
{
    if (!m_runtimeState) {
        return;
    }

    m_runtimeState->setTargetRegionDefinition(targetRegionDefinition);
    refreshDigitalTwinState();
}

void NavigationRuntimeCoordinator::clearTargetRegionDefinition()
{
    if (!m_runtimeState) {
        return;
    }

    m_runtimeState->clearTargetRegionDefinition();
    m_runtimeState->clearTargetRegionNavigationStatus();
    m_runtimeState->clearDigitalTwinRiskReport();
    m_runtimeState->clearDigitalTwinState();
}

void NavigationRuntimeCoordinator::clearPoseTrackingState()
{
    m_transformGraph.clearLatestPoseFrame();
    refreshDigitalTwinState();
}

void NavigationRuntimeCoordinator::clearRegistrationTransform()
{
    m_transformGraph.clearPatientToVtkWorldTransform();
    refreshDigitalTwinState();
}

void NavigationRuntimeCoordinator::handleRegistrationResult(const PointRegistrationResult& registrationResult)
{
    if (!m_runtimeState) {
        return;
    }

    m_runtimeState->setRegistrationResult(registrationResult);
    refreshDigitalTwinState();
}

void NavigationRuntimeCoordinator::handleTrackingQuality(const QVariantMap& trackingQuality)
{
    if (!m_runtimeState) {
        return;
    }

    m_runtimeState->setTrackingQuality(trackingQuality);
    refreshDigitalTwinState();
}

void NavigationRuntimeCoordinator::handleCalibrationCompleted(const QVariantMap& calibrationResult)
{
    if (!m_runtimeState) {
        return;
    }

    const QVariantMap currentTrackingQuality = m_runtimeState->trackingQuality();
    m_runtimeState->setTrackingQuality(mergeTrackingQuality(currentTrackingQuality, calibrationResult));
    refreshDigitalTwinState();
}

void NavigationRuntimeCoordinator::handleCalibrationTransform(const QMatrix4x4& markerToToolTransform)
{
    m_transformGraph.setMarkerToToolTransform(markerToToolTransform);
    refreshDigitalTwinState();
}

void NavigationRuntimeCoordinator::handleRegistrationTransform(const QMatrix4x4& patientToVtkWorldTransform)
{
    m_transformGraph.setPatientToVtkWorldTransform(patientToVtkWorldTransform);
    refreshDigitalTwinState();
}

void NavigationRuntimeCoordinator::handlePoseFrame(const NavigationPoseFrame& frame)
{
    if (!m_runtimeState) {
        return;
    }

    m_transformGraph.setLatestPoseFrame(frame);
    m_runtimeState->setTrackedInstrumentVisible(frame.toolId, frame.trackingVisible);
    refreshDigitalTwinState();
}

NavigationDisplayState NavigationRuntimeCoordinator::buildDisplayState(
    const QStringList& boneModelPaths,
    const QString& activeToolModelPath) const
{
    NavigationDisplayState state;
    state.boneModelPaths = boneModelPaths;
    state.activeToolModelPath = activeToolModelPath;
    if (!m_runtimeState) {
        state.statusText = QStringLiteral("runtime_unavailable");
        return state;
    }

    state.activeToolId = m_runtimeState->navigationToolId();
    state.toolVisible = m_runtimeState->isTrackedInstrumentVisible(state.activeToolId);

    const NavigationTransformResult transformResult = m_transformGraph.compute();
    state.validPose = transformResult.valid;
    state.statusText = transformResult.valid ? QStringLiteral("ok") : transformResult.failureCode;
    if (transformResult.valid) {
        state.vtkToolTransform = transformResult.vtkToolTransform;
        state.toolVisible = true;
    }

    return state;
}

void NavigationRuntimeCoordinator::recomputeConfidence()
{
    if (!m_runtimeState || !m_runtimeState->hasTrackingQuality() || !m_runtimeState->hasRegistrationResult()) {
        return;
    }

    m_runtimeState->setConfidenceResult(m_confidenceEvaluator.evaluate(buildConfidenceInputs()));
    refreshDigitalTwinState();
}

void NavigationRuntimeCoordinator::persistEvaluationReportSnapshot(bool exportMetricsCsv)
{
    if (!m_runtimeState) {
        return;
    }

    const QString caseId = m_runtimeState->caseId();
    if (caseId.isEmpty()) {
        return;
    }

    const AnkleEvaluationSnapshot snapshot = m_persistenceActions.loadEvaluationSnapshot(caseId);
    const PointRegistrationResult registrationResult = m_runtimeState->registrationResult();
    const NavigationConfidenceResult confidenceResult = m_runtimeState->confidenceResult();
    const QVariantMap trackingQuality = m_runtimeState->trackingQuality();

    AnkleEvaluationReport report;
    report.caseId = caseId;
    report.translationErrorMm = registrationResult.success ? registrationResult.targetRegionTre : snapshot.translationErrorMm;
    report.rotationErrorDeg = snapshot.rotationErrorDeg;
    report.allowNavigation = confidenceResult.allowNavigation;
    report.confidenceScore = confidenceResult.score;
    report.gateReasons = confidenceResult.recommendations;
    report.calibrated = trackingQuality.value(QStringLiteral("calibrated")).toBool();
    report.calibrationAccuracyMm = trackingQuality.value(QStringLiteral("calibration_accuracy_mm")).toDouble();

    report.metrics = snapshot.evaluationMetrics;
    if (snapshot.hasNavigationRun) {
        report.metrics.unite(snapshot.navigationMetrics);
    }
    if (registrationResult.success) {
        report.metrics.unite(registrationResult.metrics);
        report.metrics.insert(QStringLiteral("registration_mode"), registrationResult.metrics.value(QStringLiteral("registration_mode")));
        report.metrics.insert(QStringLiteral("target_region_tre_mm"), registrationResult.targetRegionTre);
        report.metrics.insert(QStringLiteral("coverage_score"), registrationResult.coverageScore);
    }
    if (!trackingQuality.isEmpty()) {
        report.metrics.insert(QStringLiteral("tracking_jitter_mm"), trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble());
        report.metrics.insert(QStringLiteral("visible_frame_ratio"), trackingQuality.value(QStringLiteral("visible_frame_ratio")).toDouble());
        report.metrics.insert(QStringLiteral("tracking_profile"), trackingQuality.value(QStringLiteral("tracking_profile")).toString());
        report.metrics.insert(QStringLiteral("tracking_confidence_score"), trackingQuality.value(QStringLiteral("tracking_confidence_score")).toDouble());
    }
    if (m_runtimeState->hasTargetRegionNavigationStatus()) {
        const TargetRegionNavigationStatus& targetStatus = m_runtimeState->targetRegionNavigationStatus();
        report.metrics.insert(QStringLiteral("target_region_distance_mm"), targetStatus.distanceToTargetMm);
        report.metrics.insert(QStringLiteral("target_region_angle_error_deg"), targetStatus.angleErrorDeg);
        report.metrics.insert(QStringLiteral("target_hit_probability"), targetStatus.targetHitProbability);
        report.metrics.insert(QStringLiteral("target_local_confidence_score"), targetStatus.localConfidenceScore);
    }
    if (m_runtimeState->hasDigitalTwinRiskReport()) {
        const DigitalTwinRiskReport& riskReport = m_runtimeState->digitalTwinRiskReport();
        report.metrics.insert(QStringLiteral("dominant_risk_source"), riskReport.dominantRiskSource);
    }
    if (m_runtimeState->hasDigitalTwinState()) {
        const DigitalTwinState& twinState = m_runtimeState->digitalTwinState();
        report.metrics.insert(QStringLiteral("twin_confidence_score"), twinState.twinConfidenceScore);
        report.metrics.insert(QStringLiteral("local_risk_score"), twinState.localRiskScore);
        report.metrics.insert(QStringLiteral("re_register_recommended"), twinState.reRegisterRecommended);
        report.metrics.insert(QStringLiteral("tracking_degradation_detected"), twinState.trackingDegradationDetected);
    }

    report.metrics.insert(QStringLiteral("allow_navigation"), report.allowNavigation);
    report.metrics.insert(QStringLiteral("gate_reason_count"), report.gateReasons.size());
    report.metrics.insert(QStringLiteral("calibrated"), report.calibrated);
    report.metrics.insert(QStringLiteral("calibration_accuracy_mm"), report.calibrationAccuracyMm);

    m_persistenceActions.saveEvaluationReport(report);
    if (exportMetricsCsv) {
        m_persistenceActions.exportMetricsCsv(caseId);
    }
    m_persistenceActions.exportCaseSummary(caseId);
}

NavigationConfidenceInputs NavigationRuntimeCoordinator::buildConfidenceInputs() const
{
    const PointRegistrationResult registrationResult = m_runtimeState->registrationResult();
    const QVariantMap trackingQuality = m_runtimeState->trackingQuality();

    NavigationConfidenceInputs inputs;
    inputs.fre = registrationResult.rmsError;
    inputs.targetTre = registrationResult.targetRegionTre;
    inputs.coverageScore = registrationResult.coverageScore;
    inputs.surfaceResidual = registrationResult.metrics.value(QStringLiteral("refined_rms")).toDouble();
    inputs.trackingJitter = trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble();
    inputs.visibleFrameRatio = trackingQuality.value(QStringLiteral("visible_frame_ratio")).toDouble();
    inputs.toolCalibrated = trackingQuality.value(QStringLiteral("calibrated")).toBool();
    inputs.calibrationAccuracy = trackingQuality.value(QStringLiteral("calibration_accuracy_mm")).toDouble();
    return inputs;
}

void NavigationRuntimeCoordinator::refreshDigitalTwinState()
{
    if (!m_runtimeState || !m_runtimeState->hasTargetRegionDefinition()) {
        return;
    }

    const NavigationTransformResult transformResult = m_transformGraph.compute();
    const TargetRegionNavigationStatus targetStatus =
        buildTargetRegionNavigationStatus(m_runtimeState->targetRegionDefinition(), transformResult);
    const DigitalTwinRiskReport riskReport = buildDigitalTwinRiskReport(
        m_runtimeState->registrationResult(),
        m_runtimeState->trackingQuality(),
        m_runtimeState->confidenceResult(),
        targetStatus);
    const DigitalTwinState twinState = buildDigitalTwinState(
        m_runtimeState->registrationResult(),
        m_runtimeState->trackingQuality(),
        m_runtimeState->confidenceResult(),
        targetStatus,
        riskReport);

    m_runtimeState->setTargetRegionNavigationStatus(targetStatus);
    m_runtimeState->setDigitalTwinRiskReport(riskReport);
    m_runtimeState->setDigitalTwinState(twinState);

    if (m_transformGraph.hasLatestPoseFrame()) {
        const QString toolId = m_transformGraph.latestPoseFrame().toolId;
        if (!toolId.isEmpty()) {
            m_runtimeState->setActiveInstrumentPoseSummary(toolId, buildPoseSummary(transformResult));
        }
    }
}

NavigationRuntimeCoordinator::PersistenceActions NavigationRuntimeCoordinator::createDefaultPersistenceActions() const
{
    return {
        .loadEvaluationSnapshot = [this](const QString& caseId) {
            NavigationEvaluationService evaluationService(m_casesRoot);
            return evaluationService.loadEvaluationSnapshot(caseId);
        },
        .saveEvaluationReport = [this](const AnkleEvaluationReport& report) {
            NavigationEvaluationService evaluationService(m_casesRoot);
            return evaluationService.saveEvaluationReport(report);
        },
        .exportMetricsCsv = [this](const QString& caseId) {
            NavigationEvaluationService evaluationService(m_casesRoot);
            return evaluationService.exportMetricsCsv(caseId);
        },
        .exportCaseSummary = [this](const QString& caseId) {
            NavigationEvaluationService evaluationService(m_casesRoot);
            return evaluationService.exportCaseSummary(caseId);
        }
    };
}
