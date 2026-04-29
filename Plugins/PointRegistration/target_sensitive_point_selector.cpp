#include "target_sensitive_point_selector.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
QVector3D normalizedAxis(const QVector3D& axis)
{
    return axis.lengthSquared() > 0.0f ? axis.normalized() : QVector3D(0.0f, 0.0f, 1.0f);
}
}

QList<RecommendedRegistrationPoint> TargetSensitivePointSelector::rankCandidates(
    const TargetRegistrationRegion& region,
    const QList<CandidateRegistrationPoint>& candidates,
    const QList<QVector3D>& alreadySelected) const
{
    QList<RecommendedRegistrationPoint> ranked;
    ranked.reserve(candidates.size());

    const QVector3D axis = normalizedAxis(region.primaryAxis);
    const double radius = region.radiusMm > 0.0 ? region.radiusMm : 1.0;

    for (const CandidateRegistrationPoint& candidate : candidates) {
        const QVector3D offset = candidate.position - region.origin;
        const double axisDistance = QVector3D::dotProduct(offset, axis);
        const QVector3D radialOffset = offset - axis * static_cast<float>(axisDistance);
        const double radialDistance = radialOffset.length();

        double spreadBonus = 0.0;
        if (!alreadySelected.isEmpty()) {
            double minDistance = std::numeric_limits<double>::max();
            for (const QVector3D& selected : alreadySelected) {
                minDistance = std::min(minDistance, static_cast<double>((candidate.position - selected).length()));
            }
            spreadBonus = std::min(minDistance, radius) * 0.1;
        }

        RecommendedRegistrationPoint recommendation;
        recommendation.pointId = candidate.pointId;
        recommendation.position = candidate.position;
        recommendation.score = (radius - radialDistance) - std::abs(axisDistance) * 0.05 + spreadBonus;
        recommendation.reason = QStringLiteral("radial=%1 axis=%2 spread=%3")
                                    .arg(radialDistance, 0, 'f', 2)
                                    .arg(axisDistance, 0, 'f', 2)
                                    .arg(spreadBonus, 0, 'f', 2);
        ranked.append(recommendation);
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const RecommendedRegistrationPoint& left, const RecommendedRegistrationPoint& right) {
        return left.score > right.score;
    });

    return ranked;
}
