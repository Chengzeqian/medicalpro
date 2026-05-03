#include "UI/NewPages/Navigation/navigation_runtime_coordinator.h"

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

void NavigationRuntimeCoordinator::handleRegistrationResult(const PointRegistrationResult& registrationResult)
{
    if (!m_runtimeState) {
        return;
    }

    m_runtimeState->setRegistrationResult(registrationResult);
}

void NavigationRuntimeCoordinator::handleTrackingQuality(const QVariantMap& trackingQuality)
{
    if (!m_runtimeState) {
        return;
    }

    m_runtimeState->setTrackingQuality(trackingQuality);
}

void NavigationRuntimeCoordinator::handleCalibrationCompleted(const QVariantMap& calibrationResult)
{
    if (!m_runtimeState) {
        return;
    }

    const QVariantMap currentTrackingQuality = m_runtimeState->trackingQuality();
    m_runtimeState->setTrackingQuality(mergeTrackingQuality(currentTrackingQuality, calibrationResult));
}

void NavigationRuntimeCoordinator::recomputeConfidence()
{
    if (!m_runtimeState || !m_runtimeState->hasTrackingQuality() || !m_runtimeState->hasRegistrationResult()) {
        return;
    }

    m_runtimeState->setConfidenceResult(m_confidenceEvaluator.evaluate(buildConfidenceInputs()));
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
