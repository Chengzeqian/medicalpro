#include "expert_rule_point_selection_strategy.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
QVector3D normalizedAxis(const QVector3D& axis)
{
    return axis.lengthSquared() > 0.0f ? axis.normalized() : QVector3D(0.0f, 0.0f, 1.0f);
}

double minimumDistanceToSet(const QVector3D& point, const QList<QVector3D>& anchors)
{
    if (anchors.isEmpty()) {
        return 0.0;
    }

    double bestDistance = std::numeric_limits<double>::max();
    for (const QVector3D& anchor : anchors) {
        bestDistance = std::min(bestDistance, static_cast<double>((point - anchor).length()));
    }
    return bestDistance;
}
}

QString ExpertRulePointSelectionStrategy::id() const
{
    return QStringLiteral("expert_rule");
}

QList<RecommendedRegistrationPoint> ExpertRulePointSelectionStrategy::select(
    const TargetRegistrationRegion& region,
    const QList<CandidateRegistrationPoint>& candidates,
    const QList<QVector3D>& alreadySelected) const
{
    const QVector3D axis = normalizedAxis(region.primaryAxis);
    const double radius = region.radiusMm > 0.0 ? region.radiusMm : 1.0;
    const double preferredRadius = radius * 0.65;

    QList<RecommendedRegistrationPoint> ranked;
    ranked.reserve(candidates.size());

    for (const CandidateRegistrationPoint& candidate : candidates) {
        const QVector3D offset = candidate.position - region.origin;
        const double axisDistance = std::abs(QVector3D::dotProduct(offset, axis));
        const QVector3D radialOffset = offset - axis * static_cast<float>(QVector3D::dotProduct(offset, axis));
        const double radialDistance = radialOffset.length();
        const double radialPenalty = std::abs(radialDistance - preferredRadius);
        const double spreadBonus = minimumDistanceToSet(candidate.position, alreadySelected) * 0.05;

        RecommendedRegistrationPoint point;
        point.pointId = candidate.pointId;
        point.position = candidate.position;
        point.score = (radius - radialPenalty) - axisDistance * 0.08 + spreadBonus;
        point.reason = QStringLiteral("expert_radial=%1 axis=%2 spread=%3")
                           .arg(radialDistance, 0, 'f', 2)
                           .arg(axisDistance, 0, 'f', 2)
                           .arg(spreadBonus, 0, 'f', 2);
        ranked.append(point);
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const RecommendedRegistrationPoint& left, const RecommendedRegistrationPoint& right) {
        return left.score > right.score;
    });

    return ranked;
}
