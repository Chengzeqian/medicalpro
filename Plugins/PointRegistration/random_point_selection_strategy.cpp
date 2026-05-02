#include "random_point_selection_strategy.h"

#include <algorithm>
#include <numeric>
#include <random>

QString RandomPointSelectionStrategy::id() const
{
    return QStringLiteral("random");
}

QList<RecommendedRegistrationPoint> RandomPointSelectionStrategy::select(
    const TargetRegistrationRegion&,
    const QList<CandidateRegistrationPoint>& candidates,
    const QList<QVector3D>&) const
{
    QList<int> indices;
    indices.reserve(candidates.size());
    for (int index = 0; index < candidates.size(); ++index) {
        indices.append(index);
    }

    std::mt19937 generator(20260429u);
    std::shuffle(indices.begin(), indices.end(), generator);

    QList<RecommendedRegistrationPoint> ordered;
    ordered.reserve(candidates.size());
    for (int index : indices) {
        RecommendedRegistrationPoint point;
        point.pointId = candidates[index].pointId;
        point.position = candidates[index].position;
        point.score = 0.0;
        point.reason = QStringLiteral("random_order");
        ordered.append(point);
    }

    return ordered;
}
