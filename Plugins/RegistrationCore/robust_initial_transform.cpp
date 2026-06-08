#include "robust_initial_transform.h"

#include "ankle_registration_utils.h"

#include <QtMath>

#include <algorithm>
#include <limits>

namespace
{
double clamp01(double value)
{
    return std::max(0.0, std::min(1.0, value));
}

double median(QList<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    const int middle = values.size() / 2;
    if (values.size() % 2 == 1) {
        return values.at(middle);
    }

    return (values.at(middle - 1) + values.at(middle)) * 0.5;
}

QVector3D medianPoint(const QList<QVector3D>& points)
{
    QList<double> xs;
    QList<double> ys;
    QList<double> zs;
    xs.reserve(points.size());
    ys.reserve(points.size());
    zs.reserve(points.size());

    for (const QVector3D& point : points) {
        xs.append(point.x());
        ys.append(point.y());
        zs.append(point.z());
    }

    return QVector3D(
        static_cast<float>(median(xs)),
        static_cast<float>(median(ys)),
        static_cast<float>(median(zs)));
}

double rmsDistanceToPoint(const QList<QVector3D>& points, const QVector3D& center)
{
    if (points.isEmpty()) {
        return 0.0;
    }

    double squaredSum = 0.0;
    for (const QVector3D& point : points) {
        const QVector3D delta = point - center;
        squaredSum += QVector3D::dotProduct(delta, delta);
    }

    return qSqrt(squaredSum / points.size());
}

double distanceMm(const QVector3D& left, const QVector3D& right)
{
    return static_cast<double>((left - right).length());
}

double triangleAreaMm2(const QVector3D& a, const QVector3D& b, const QVector3D& c)
{
    return 0.5 * static_cast<double>(QVector3D::crossProduct(b - a, c - a).length());
}

QVector3D mapPoint(const QMatrix4x4& transform, const QVector3D& point)
{
    return transform.map(point);
}

struct CandidateFit
{
    bool valid = false;
    QMatrix4x4 transform;
    QList<int> inlierIndices;
    double inlierRmsMm = std::numeric_limits<double>::max();
};

CandidateFit evaluateTransform(
    const QMatrix4x4& transform,
    const QList<QVector3D>& sourcePoints,
    const QList<QVector3D>& targetPoints,
    double thresholdMm)
{
    CandidateFit fit;
    fit.valid = true;
    fit.transform = transform;

    double squaredSum = 0.0;
    for (int index = 0; index < sourcePoints.size(); ++index) {
        const double residual = distanceMm(mapPoint(transform, sourcePoints.at(index)), targetPoints.at(index));
        if (residual <= thresholdMm) {
            fit.inlierIndices.append(index);
            squaredSum += residual * residual;
        }
    }

    if (!fit.inlierIndices.isEmpty()) {
        fit.inlierRmsMm = qSqrt(squaredSum / fit.inlierIndices.size());
    }

    return fit;
}

bool betterFit(const CandidateFit& candidate, const CandidateFit& current)
{
    if (!current.valid) {
        return true;
    }

    if (candidate.inlierIndices.size() != current.inlierIndices.size()) {
        return candidate.inlierIndices.size() > current.inlierIndices.size();
    }

    return candidate.inlierRmsMm < current.inlierRmsMm;
}

QList<double> weightsForInliers(int pointCount, const QList<int>& inlierIndices)
{
    QList<double> weights;
    weights.reserve(pointCount);
    for (int index = 0; index < pointCount; ++index) {
        weights.append(inlierIndices.contains(index) ? 1.0 : 0.0);
    }

    return weights;
}
}

StableProbePointResult collectStableProbePoint(
    const QList<ProbeTipFrameSample>& samples,
    const StableProbePointOptions& options)
{
    StableProbePointResult result;

    QList<QVector3D> acceptedPoints;
    acceptedPoints.reserve(samples.size());
    double trackingErrorSum = 0.0;

    for (const ProbeTipFrameSample& sample : samples) {
        if (!sample.valid || sample.trackingErrorMm > options.maxTrackingErrorMm) {
            ++result.rejectedFrameCount;
            continue;
        }

        acceptedPoints.append(sample.tipPositionMm);
        trackingErrorSum += sample.trackingErrorMm;
    }

    result.acceptedFrameCount = acceptedPoints.size();
    if (result.acceptedFrameCount > 0) {
        result.meanTrackingErrorMm = trackingErrorSum / result.acceptedFrameCount;
        result.pointMm = medianPoint(acceptedPoints);
        result.jitterRmsMm = rmsDistanceToPoint(acceptedPoints, result.pointMm);
    }

    if (result.acceptedFrameCount < options.minimumAcceptedFrames) {
        result.reason = QStringLiteral("not_enough_valid_frames");
        return result;
    }

    if (result.jitterRmsMm > options.maxJitterRmsMm) {
        result.reason = QStringLiteral("jitter_too_high");
        return result;
    }

    const double jitterRatio = result.jitterRmsMm / options.maxJitterRmsMm;
    const double trackingRatio = result.meanTrackingErrorMm / options.maxTrackingErrorMm;
    const double frameScore = clamp01(static_cast<double>(result.acceptedFrameCount) / options.minimumAcceptedFrames);
    result.confidence = clamp01(1.0 - 0.35 * jitterRatio - 0.15 * trackingRatio) * frameScore;
    result.accepted = true;
    result.reason = QStringLiteral("accepted");
    return result;
}

InitialValueQuality evaluateInitialPointSetQuality(
    const QList<QVector3D>& points,
    const InitialValueQualityOptions& options)
{
    InitialValueQuality quality;
    quality.pointCount = points.size();

    if (points.size() < 3) {
        quality.reason = QStringLiteral("not_enough_points");
        return quality;
    }

    for (int i = 0; i < points.size(); ++i) {
        for (int j = i + 1; j < points.size(); ++j) {
            quality.boundingDiagonalMm = std::max(
                quality.boundingDiagonalMm,
                distanceMm(points.at(i), points.at(j)));
        }
    }

    if (quality.boundingDiagonalMm < options.minBoundingDiagonalMm) {
        quality.reason = QStringLiteral("spread_too_small");
        return quality;
    }

    for (int i = 0; i < points.size(); ++i) {
        for (int j = i + 1; j < points.size(); ++j) {
            for (int k = j + 1; k < points.size(); ++k) {
                quality.bestTriangleAreaMm2 = std::max(
                    quality.bestTriangleAreaMm2,
                    triangleAreaMm2(points.at(i), points.at(j), points.at(k)));
            }
        }
    }

    const double diagonalSquared = quality.boundingDiagonalMm * quality.boundingDiagonalMm;
    if (diagonalSquared > 0.0) {
        quality.nonCollinearityScore = quality.bestTriangleAreaMm2 / diagonalSquared;
    }

    if (quality.bestTriangleAreaMm2 < options.minTriangleAreaMm2
        || quality.nonCollinearityScore < options.minNonCollinearityScore) {
        quality.reason = QStringLiteral("points_nearly_collinear");
        return quality;
    }

    const double spreadScore = clamp01(quality.boundingDiagonalMm / (options.minBoundingDiagonalMm * 2.0));
    const double areaScore = clamp01(quality.bestTriangleAreaMm2 / (options.minTriangleAreaMm2 * 2.0));
    const double shapeScore = clamp01(quality.nonCollinearityScore / (options.minNonCollinearityScore * 2.0));
    quality.confidence = clamp01(0.35 * spreadScore + 0.35 * areaScore + 0.30 * shapeScore);
    quality.accepted = true;
    quality.reason = QStringLiteral("accepted");
    return quality;
}

RobustInitialTransformResult estimateRobustInitialTransform(
    const QList<QVector3D>& sourcePoints,
    const QList<QVector3D>& targetPoints,
    const RobustInitialTransformOptions& options)
{
    RobustInitialTransformResult result;
    result.transform.setToIdentity();

    if (sourcePoints.size() != targetPoints.size() || sourcePoints.size() < 3) {
        result.reason = QStringLiteral("invalid_correspondence_count");
        return result;
    }

    result.sourceQuality = evaluateInitialPointSetQuality(sourcePoints, options.quality);
    if (!result.sourceQuality.accepted) {
        result.reason = QStringLiteral("source_quality_rejected");
        return result;
    }

    result.targetQuality = evaluateInitialPointSetQuality(targetPoints, options.quality);
    if (!result.targetQuality.accepted) {
        result.reason = QStringLiteral("target_quality_rejected");
        return result;
    }

    CandidateFit bestFit;
    const QList<double> tripletWeights = { 1.0, 1.0, 1.0 };

    for (int i = 0; i < sourcePoints.size(); ++i) {
        for (int j = i + 1; j < sourcePoints.size(); ++j) {
            for (int k = j + 1; k < sourcePoints.size(); ++k) {
                const QList<QVector3D> sourceTriplet = { sourcePoints.at(i), sourcePoints.at(j), sourcePoints.at(k) };
                const QList<QVector3D> targetTriplet = { targetPoints.at(i), targetPoints.at(j), targetPoints.at(k) };
                const InitialValueQuality tripletQuality = evaluateInitialPointSetQuality(sourceTriplet, options.quality);
                if (!tripletQuality.accepted) {
                    continue;
                }

                const WeightedRigidRegistrationResult rigid =
                    AnkleRegistrationUtils::solveWeightedRigid(sourceTriplet, targetTriplet, tripletWeights);
                if (!rigid.success) {
                    continue;
                }

                const CandidateFit fit =
                    evaluateTransform(rigid.transform, sourcePoints, targetPoints, options.inlierResidualThresholdMm);
                if (betterFit(fit, bestFit)) {
                    bestFit = fit;
                }
            }
        }
    }

    if (!bestFit.valid || bestFit.inlierIndices.size() < options.minimumInlierCount) {
        result.reason = QStringLiteral("not_enough_inliers");
        return result;
    }

    const QList<double> inlierWeights = weightsForInliers(sourcePoints.size(), bestFit.inlierIndices);
    const WeightedRigidRegistrationResult refined =
        AnkleRegistrationUtils::solveWeightedRigid(sourcePoints, targetPoints, inlierWeights);
    if (!refined.success) {
        result.reason = QStringLiteral("refit_failed");
        return result;
    }

    const CandidateFit refinedFit =
        evaluateTransform(refined.transform, sourcePoints, targetPoints, options.inlierResidualThresholdMm);
    if (refinedFit.inlierIndices.size() < options.minimumInlierCount) {
        result.reason = QStringLiteral("refit_lost_inliers");
        return result;
    }

    result.success = true;
    result.transform = refined.transform;
    result.inlierIndices = refinedFit.inlierIndices;
    result.inlierCount = refinedFit.inlierIndices.size();
    result.rejectedOutlierCount = sourcePoints.size() - result.inlierCount;
    result.inlierRmsMm = refinedFit.inlierRmsMm;

    const double inlierRatio = static_cast<double>(result.inlierCount) / sourcePoints.size();
    const double residualScore = clamp01(1.0 - result.inlierRmsMm / options.inlierResidualThresholdMm);
    const double geometryScore = 0.5 * result.sourceQuality.confidence + 0.5 * result.targetQuality.confidence;
    result.confidence = clamp01(0.60 * inlierRatio + 0.25 * residualScore + 0.15 * geometryScore);
    result.reason = QStringLiteral("accepted");
    return result;
}
