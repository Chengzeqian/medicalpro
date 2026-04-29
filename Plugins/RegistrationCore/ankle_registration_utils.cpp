#include "ankle_registration_utils.h"

#include <QtMath>

WeightedRigidRegistrationResult AnkleRegistrationUtils::solveWeightedRigid(
    const QList<QVector3D>& source,
    const QList<QVector3D>& target,
    const QList<double>& weights)
{
    WeightedRigidRegistrationResult result;
    result.transform.setToIdentity();

    if (source.isEmpty() || source.size() != target.size() || source.size() != weights.size()) {
        return result;
    }

    double totalWeight = 0.0;
    QVector3D weightedSourceCentroid;
    QVector3D weightedTargetCentroid;

    for (int i = 0; i < source.size(); ++i) {
        const double weight = weights[i];
        if (weight <= 0.0) {
            continue;
        }

        totalWeight += weight;
        weightedSourceCentroid += static_cast<float>(weight) * source[i];
        weightedTargetCentroid += static_cast<float>(weight) * target[i];
    }

    if (totalWeight <= 0.0) {
        return result;
    }

    weightedSourceCentroid /= static_cast<float>(totalWeight);
    weightedTargetCentroid /= static_cast<float>(totalWeight);

    result.translation = weightedTargetCentroid - weightedSourceCentroid;
    result.transform.translate(result.translation);

    double weightedSquaredError = 0.0;
    for (int i = 0; i < source.size(); ++i) {
        const double weight = weights[i];
        if (weight <= 0.0) {
            continue;
        }

        const QVector3D delta = source[i] + result.translation - target[i];
        weightedSquaredError += weight * static_cast<double>(QVector3D::dotProduct(delta, delta));
    }

    result.weightedRmsError = qSqrt(weightedSquaredError / totalWeight);
    result.success = true;
    return result;
}

QList<int> AnkleRegistrationUtils::selectRoiPointIndices(
    const QList<QVector3D>& modelPoints,
    const QVector3D& roiCenter,
    double roiRadiusMm)
{
    QList<int> indices;
    if (roiRadiusMm <= 0.0) {
        return indices;
    }

    const float radiusSquared = static_cast<float>(roiRadiusMm * roiRadiusMm);
    for (int i = 0; i < modelPoints.size(); ++i) {
        const QVector3D delta = modelPoints[i] - roiCenter;
        if (QVector3D::dotProduct(delta, delta) <= radiusSquared) {
            indices.append(i);
        }
    }

    return indices;
}
