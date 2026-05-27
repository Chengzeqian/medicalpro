#include "ankle_registration_parallel_search.h"

#include <QChar>

#include <algorithm>

QList<CandidateInitialTransform> buildCandidateInitialTransforms(
    const QMatrix4x4& coarseTransform,
    const QVector3D& targetRegionCenter,
    const ParallelSearchPlan& plan)
{
    QList<CandidateInitialTransform> candidates;
    const int requestedCount = qMax(plan.candidateCount, 1);
    candidates.reserve(requestedCount);

    for (int index = 0; index < requestedCount; ++index) {
        CandidateInitialTransform candidate;
        candidate.candidateId = QStringLiteral("candidate_%1").arg(index, 3, 10, QChar('0'));
        candidate.seedType = index == 0
            ? QStringLiteral("landmark_identity_seed")
            : QStringLiteral("axis_perturbation_seed");
        candidate.rankHint = index;

        const float yawDelta = index == 0 ? 0.0f : static_cast<float>((index % 5) - 2) * 1.5f;
        const float pitchDelta = index == 0 ? 0.0f : static_cast<float>(((index / 5) % 5) - 2) * 1.0f;
        const float radialDelta = index == 0 ? 0.0f : static_cast<float>((index / 25) + 1) * 0.6f;

        candidate.rotationDeltaDeg = QVector3D(pitchDelta, yawDelta, 0.0f);
        candidate.translationDeltaMm = index == 0 ? QVector3D() : QVector3D(radialDelta, 0.0f, 0.0f);

        QMatrix4x4 transform = coarseTransform;
        transform.translate(targetRegionCenter);
        transform.rotate(candidate.rotationDeltaDeg.y(), 0.0f, 1.0f, 0.0f);
        transform.rotate(candidate.rotationDeltaDeg.x(), 1.0f, 0.0f, 0.0f);
        transform.translate(candidate.translationDeltaMm);
        transform.translate(-targetRegionCenter);
        candidate.transformMatrix = transform;
        candidates.append(candidate);
    }

    return candidates;
}

QList<CandidateEvaluationResult> selectTopKCandidates(
    const QList<CandidateEvaluationResult>& scores,
    int topKCount)
{
    if (topKCount <= 0) {
        return {};
    }

    QList<CandidateEvaluationResult> sorted = scores;
    std::sort(sorted.begin(), sorted.end(),
        [](const CandidateEvaluationResult& left, const CandidateEvaluationResult& right) {
            return left.coarseScore < right.coarseScore;
        });
    while (sorted.size() > topKCount) {
        sorted.removeLast();
    }
    return sorted;
}

QList<double> resolveMultiResolutionCellSizes(const QString& profileId)
{
    if (profileId == QStringLiteral("ankle_roi_three_level")) {
        return { 3.0, 1.5, 0.75 };
    }
    if (profileId == QStringLiteral("ankle_roi_two_level")) {
        return { 2.5, 1.0 };
    }
    // 未知 profile 回退到单层搜索，保持 helper 的默认兜底行为。
    return { 1.0 };
}

QStringList candidateIds(const QList<CandidateEvaluationResult>& scores)
{
    QStringList ids;
    ids.reserve(scores.size());
    for (const CandidateEvaluationResult& score : scores) {
        ids.append(score.candidateId);
    }
    return ids;
}

QList<CandidateInitialTransform> filterCandidatesByIds(
    const QList<CandidateInitialTransform>& candidates,
    const QStringList& candidateIds)
{
    QList<CandidateInitialTransform> filtered;
    for (const CandidateInitialTransform& candidate : candidates) {
        if (candidateIds.contains(candidate.candidateId)) {
            filtered.append(candidate);
        }
    }
    return filtered;
}

QVariantMap candidateEvaluationToVariantMap(const CandidateEvaluationResult& result)
{
    QVariantMap map;
    map.insert(QStringLiteral("candidate_id"), result.candidateId);
    map.insert(QStringLiteral("coarse_score"), result.coarseScore);
    map.insert(QStringLiteral("target_region_hit_ratio"), result.targetRegionHitRatio);
    map.insert(QStringLiteral("coverage_score"), result.coverageScore);
    map.insert(QStringLiteral("converged"), result.converged);
    map.insert(QStringLiteral("multi_resolution_level"), result.multiResolutionLevel);
    return map;
}
