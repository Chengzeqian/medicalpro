#include "Framework/Navigation/navigation_digital_twin_state_builder.h"

#include <QtMath>

namespace
{
double clamp01(const double value)
{
    return qBound(0.0, value, 1.0);
}

double inverseScore(const double value, const double threshold)
{
    if (threshold <= 0.0) {
        return 0.0;
    }

    return clamp01(1.0 - (value / threshold));
}

QVector3D transformTranslation(const QMatrix4x4& matrix)
{
    return matrix.column(3).toVector3D();
}

QVector3D transformAxisZ(const QMatrix4x4& matrix)
{
    return matrix.mapVector(QVector3D(0.0f, 0.0f, 1.0f)).normalized();
}

double angleBetweenDeg(const QVector3D& lhs, const QVector3D& rhs)
{
    if (lhs.lengthSquared() <= 0.0f || rhs.lengthSquared() <= 0.0f) {
        return 0.0;
    }

    const double dot = clamp01((QVector3D::dotProduct(lhs.normalized(), rhs.normalized()) + 1.0) * 0.5);
    return qRadiansToDegrees(qAcos((dot * 2.0) - 1.0));
}
}

TargetRegionNavigationStatus buildTargetRegionNavigationStatus(
    const DigitalTwinTargetRegionDefinition& targetRegion,
    const NavigationTransformResult& transformResult)
{
    TargetRegionNavigationStatus status;
    status.targetRegionAvailable = targetRegion.available;
    if (!targetRegion.available || !transformResult.valid) {
        return status;
    }

    const QVector3D toolTipPatient = transformTranslation(transformResult.vtkToolTransform);
    const QVector3D toolAxisPatient = transformAxisZ(transformResult.vtkToolTransform);
    status.distanceToTargetMm = (toolTipPatient - targetRegion.centerPatient).length();
    status.angleErrorDeg = angleBetweenDeg(toolAxisPatient, targetRegion.plannedAxisPatient);

    const double radius = qMax(targetRegion.radiusMm, 1.0);
    const double distanceScore = inverseScore(status.distanceToTargetMm, radius * 2.0);
    const double angleScore = inverseScore(status.angleErrorDeg, 30.0);
    status.targetHitProbability = clamp01((distanceScore * 0.7) + (angleScore * 0.3));
    status.localConfidenceScore = clamp01((distanceScore * 0.5) + (angleScore * 0.2) + (status.targetHitProbability * 0.3));
    return status;
}

DigitalTwinRiskReport buildDigitalTwinRiskReport(
    const PointRegistrationResult& registrationResult,
    const QVariantMap& trackingQuality,
    const NavigationConfidenceResult& confidenceResult,
    const TargetRegionNavigationStatus& targetStatus)
{
    DigitalTwinRiskReport report;

    const double trackingJitter = trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble();
    const double visibleFrameRatio = trackingQuality.value(QStringLiteral("visible_frame_ratio"), 1.0).toDouble();
    const bool calibrated = trackingQuality.value(QStringLiteral("calibrated")).toBool();
    const double calibrationAccuracy = trackingQuality.value(QStringLiteral("calibration_accuracy_mm")).toDouble();

    double registrationRisk = 0.0;
    if (registrationResult.targetRegionTre > 2.5) {
        report.riskReasons.append(QStringLiteral("target_tre_high"));
        registrationRisk = qMax(registrationRisk, 0.9);
    }
    if (registrationResult.coverageScore < 0.6) {
        report.riskReasons.append(QStringLiteral("coverage_low"));
        registrationRisk = qMax(registrationRisk, 0.7);
    }

    double trackingRisk = 0.0;
    if (trackingJitter > 1.0) {
        report.riskReasons.append(QStringLiteral("tracking_jitter_high"));
        trackingRisk = qMax(trackingRisk, 0.8);
    }
    if (visibleFrameRatio < 0.85) {
        report.riskReasons.append(QStringLiteral("tracking_visibility_low"));
        trackingRisk = qMax(trackingRisk, 0.75);
    }
    if (!calibrated || calibrationAccuracy > 1.5) {
        report.riskReasons.append(QStringLiteral("calibration_unstable"));
        trackingRisk = qMax(trackingRisk, 0.7);
    }

    double targetRisk = 0.0;
    if (targetStatus.targetRegionAvailable && targetStatus.distanceToTargetMm > 2.5) {
        report.riskReasons.append(QStringLiteral("target_distance_high"));
        targetRisk = qMax(targetRisk, 0.75);
    }
    if (targetStatus.targetRegionAvailable && targetStatus.localConfidenceScore < 0.5) {
        report.riskReasons.append(QStringLiteral("target_local_confidence_low"));
        targetRisk = qMax(targetRisk, 0.7);
    }

    double navigationRisk = 0.0;
    if (confidenceResult.score < 0.5) {
        report.riskReasons.append(QStringLiteral("navigation_confidence_low"));
        navigationRisk = qMax(navigationRisk, 0.65);
    }
    if (!confidenceResult.allowNavigation) {
        report.riskReasons.append(QStringLiteral("navigation_gate_blocked"));
        navigationRisk = qMax(navigationRisk, 0.55);
    }

    report.rawMetrics.insert(QStringLiteral("registration_risk"), registrationRisk);
    report.rawMetrics.insert(QStringLiteral("tracking_risk"), trackingRisk);
    report.rawMetrics.insert(QStringLiteral("target_risk"), targetRisk);
    report.rawMetrics.insert(QStringLiteral("navigation_risk"), navigationRisk);

    if (registrationRisk >= trackingRisk
        && registrationRisk >= targetRisk
        && registrationRisk >= navigationRisk) {
        report.dominantRiskSource = QStringLiteral("registration");
    } else if (trackingRisk >= targetRisk && trackingRisk >= navigationRisk) {
        report.dominantRiskSource = QStringLiteral("tracking");
    } else if (targetRisk >= navigationRisk) {
        report.dominantRiskSource = QStringLiteral("target_region");
    } else {
        report.dominantRiskSource = QStringLiteral("navigation_gate");
    }

    return report;
}

DigitalTwinState buildDigitalTwinState(
    const PointRegistrationResult& registrationResult,
    const QVariantMap& trackingQuality,
    const NavigationConfidenceResult& confidenceResult,
    const TargetRegionNavigationStatus& targetStatus,
    const DigitalTwinRiskReport& riskReport)
{
    DigitalTwinState state;
    state.valid = registrationResult.success && targetStatus.targetRegionAvailable;

    const double trackingJitter = trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble();
    const double visibleFrameRatio = trackingQuality.value(QStringLiteral("visible_frame_ratio"), 1.0).toDouble();
    const bool calibrated = trackingQuality.value(QStringLiteral("calibrated")).toBool();
    const double calibrationAccuracy = trackingQuality.value(QStringLiteral("calibration_accuracy_mm")).toDouble();

    const double registrationScore =
        clamp01((inverseScore(registrationResult.targetRegionTre, 3.0) * 0.7)
        + (clamp01(registrationResult.coverageScore) * 0.3));
    const double trackingScore =
        clamp01((inverseScore(trackingJitter, 1.5) * 0.4)
        + (clamp01(visibleFrameRatio) * 0.3)
        + ((calibrated ? inverseScore(calibrationAccuracy, 2.0) : 0.0) * 0.3));
    const double targetScore = clamp01(targetStatus.localConfidenceScore);
    const double navigationScore = clamp01(confidenceResult.score);

    state.twinConfidenceScore = clamp01(
        (registrationScore * 0.35)
        + (trackingScore * 0.25)
        + (targetScore * 0.20)
        + (navigationScore * 0.20));

    const double registrationRisk = riskReport.rawMetrics.value(QStringLiteral("registration_risk")).toDouble();
    const double trackingRisk = riskReport.rawMetrics.value(QStringLiteral("tracking_risk")).toDouble();
    const double targetRisk = riskReport.rawMetrics.value(QStringLiteral("target_risk")).toDouble();
    const double navigationRisk = riskReport.rawMetrics.value(QStringLiteral("navigation_risk")).toDouble();
    state.localRiskScore = clamp01(qMax(qMax(registrationRisk, trackingRisk), qMax(targetRisk, navigationRisk)));

    state.allowNavigation = confidenceResult.allowNavigation && state.twinConfidenceScore >= 0.6 && state.localRiskScore < 0.5;
    state.reRegisterRecommended =
        registrationResult.targetRegionTre > 2.5
        || state.twinConfidenceScore < 0.5
        || riskReport.dominantRiskSource == QStringLiteral("registration");
    state.trackingDegradationDetected = trackingJitter > 1.0 || visibleFrameRatio < 0.85 || !calibrated;
    state.statusCode = state.allowNavigation ? QStringLiteral("ready") : QStringLiteral("risk_review_required");
    state.statusText = state.allowNavigation
        ? QStringLiteral("数字孪生状态稳定，可继续导航")
        : QStringLiteral("数字孪生检测到高风险，建议复核");

    state.evidence.insert(QStringLiteral("target_tre_mm"), registrationResult.targetRegionTre);
    state.evidence.insert(QStringLiteral("coverage_score"), registrationResult.coverageScore);
    state.evidence.insert(QStringLiteral("tracking_jitter_mm"), trackingJitter);
    state.evidence.insert(QStringLiteral("visible_frame_ratio"), visibleFrameRatio);
    state.evidence.insert(QStringLiteral("calibration_accuracy_mm"), calibrationAccuracy);
    state.evidence.insert(QStringLiteral("target_region_distance_mm"), targetStatus.distanceToTargetMm);
    state.evidence.insert(QStringLiteral("target_region_angle_error_deg"), targetStatus.angleErrorDeg);
    state.evidence.insert(QStringLiteral("dominant_risk_source"), riskReport.dominantRiskSource);

    return state;
}
