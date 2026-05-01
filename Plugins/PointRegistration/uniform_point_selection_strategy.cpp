#include "uniform_point_selection_strategy.h"

#include <limits>

namespace
{
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

QString UniformPointSelectionStrategy::id() const
{
    return QStringLiteral("uniform");
}

QList<RecommendedRegistrationPoint> UniformPointSelectionStrategy::select(
    const TargetRegistrationRegion& region,
    const QList<CandidateRegistrationPoint>& candidates,
    const QList<QVector3D>& alreadySelected) const
{
    QList<QVector3D> anchors = alreadySelected;
    QList<int> remaining;
    remaining.reserve(candidates.size());
    for (int index = 0; index < candidates.size(); ++index) {
        remaining.append(index);
    }

    QList<RecommendedRegistrationPoint> ordered;
    ordered.reserve(candidates.size());

    while (!remaining.isEmpty()) {
        int bestPosition = 0;
        double bestScore = -1.0;

        for (int position = 0; position < remaining.size(); ++position) {
            const CandidateRegistrationPoint& candidate = candidates[remaining[position]];
            const double score = anchors.isEmpty()
                ? static_cast<double>((candidate.position - region.origin).length())
                : minimumDistanceToSet(candidate.position, anchors);
            if (score > bestScore) {
                bestScore = score;
                bestPosition = position;
            }
        }

        const CandidateRegistrationPoint& selectedCandidate = candidates[remaining.takeAt(bestPosition)];
        anchors.append(selectedCandidate.position);

        RecommendedRegistrationPoint point;
        point.pointId = selectedCandidate.pointId;
        point.position = selectedCandidate.position;
        point.score = bestScore;
        point.reason = QStringLiteral("uniform_min_distance=%1").arg(bestScore, 0, 'f', 2);
        ordered.append(point);
    }

    return ordered;
}
