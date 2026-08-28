#include "ankle_registration_parallel_search.h"

#include <QChar>
#include <QtMath>

#include <algorithm>

namespace
{

struct PosePerturbation
{
    QVector3D rotationDeltaDeg;
    QVector3D translationDeltaMm;
    int activeComponentCount = 0;
    float translationNorm = 0.0f;
    float rotationNorm = 0.0f;
    int translationRank = 0;
    int rotationRank = 0;
};

const bool isZeroVector(const QVector3D& vector)
{
    return qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y()) && qFuzzyIsNull(vector.z());
}

QList<QVector3D> signedAxisDirections()
{
    return {
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(-1.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
        QVector3D(0.0f, -1.0f, 0.0f),
        QVector3D(0.0f, 0.0f, 1.0f),
        QVector3D(0.0f, 0.0f, -1.0f)
    };
}

QList<float> translationMagnitudesMm()
{
    return { 0.8f, 1.6f, 2.4f };
}

QList<float> rotationMagnitudesDeg()
{
    return { 1.5f, 3.0f, 4.5f };
}

void appendPosePerturbation(
    QList<PosePerturbation>& perturbations,
    const QVector3D& rotationDeltaDeg,
    const QVector3D& translationDeltaMm,
    int translationRank,
    int rotationRank)
{
    PosePerturbation perturbation;
    perturbation.rotationDeltaDeg = rotationDeltaDeg;
    perturbation.translationDeltaMm = translationDeltaMm;
    perturbation.activeComponentCount =
        (isZeroVector(translationDeltaMm) ? 0 : 1)
        + (isZeroVector(rotationDeltaDeg) ? 0 : 1);
    perturbation.translationNorm = translationDeltaMm.length();
    perturbation.rotationNorm = rotationDeltaDeg.length();
    perturbation.translationRank = translationRank;
    perturbation.rotationRank = rotationRank;
    perturbations.append(perturbation);
}

QList<PosePerturbation> buildOrderedPosePerturbations()
{
    const QList<QVector3D> axisDirections = signedAxisDirections();
    const QList<float> translationMagnitudes = translationMagnitudesMm();
    const QList<float> rotationMagnitudes = rotationMagnitudesDeg();

    QList<PosePerturbation> perturbations;
    perturbations.reserve(144);

    int translationRank = 0;
    for (float translationMagnitudeMm : translationMagnitudes) {
        for (const QVector3D& axisDirection : axisDirections) {
            appendPosePerturbation(
                perturbations,
                QVector3D(),
                axisDirection * translationMagnitudeMm,
                translationRank++,
                -1);
        }
    }

    int rotationRank = 0;
    for (float rotationMagnitudeDeg : rotationMagnitudes) {
        for (const QVector3D& axisDirection : axisDirections) {
            appendPosePerturbation(
                perturbations,
                axisDirection * rotationMagnitudeDeg,
                QVector3D(),
                -1,
                rotationRank++);
        }
    }

    const QList<QPair<float, float>> coupledMagnitudePairs = {
        qMakePair(0.8f, 1.5f),
        qMakePair(1.6f, 3.0f),
        qMakePair(2.4f, 4.5f),
        qMakePair(0.8f, 3.0f),
        qMakePair(1.6f, 1.5f),
        qMakePair(2.4f, 3.0f),
        qMakePair(1.6f, 4.5f),
        qMakePair(0.8f, 4.5f),
        qMakePair(2.4f, 1.5f)
    };

    for (const auto& coupledMagnitudePair : coupledMagnitudePairs) {
        const float translationMagnitudeMm = coupledMagnitudePair.first;
        const float rotationMagnitudeDeg = coupledMagnitudePair.second;
        for (const QVector3D& translationDirection : axisDirections) {
            const QVector3D rotationAxis(
                qAbs(translationDirection.x()),
                qAbs(translationDirection.y()),
                qAbs(translationDirection.z()));
            appendPosePerturbation(
                perturbations,
                rotationAxis * rotationMagnitudeDeg,
                translationDirection * translationMagnitudeMm,
                translationRank++,
                rotationRank++);
            appendPosePerturbation(
                perturbations,
                -rotationAxis * rotationMagnitudeDeg,
                translationDirection * translationMagnitudeMm,
                translationRank++,
                rotationRank++);
        }
    }

    return perturbations;
}

double coarseScoreTieWindowMm()
{
    return 0.05;
}

double strongRefineReductionScoreMm()
{
    return 2.0;
}

double baselineRefineReductionScoreWindowMm()
{
    return 0.35;
}

double candidateRegionQualityScore(const CandidateEvaluationResult& result)
{
    return result.targetRegionHitRatio * 0.7 + result.coverageScore * 0.3;
}

bool candidateHasMeaningfulRegionMetrics(const CandidateEvaluationResult& result)
{
    return result.targetRegionHitRatio > 0.0 || result.coverageScore > 0.0;
}

double candidateRegionQualityBonusMm()
{
    return 0.35;
}

double candidateNormalConsistencyBonusMm()
{
    return 0.25;
}

double candidateCurvatureBonusMm()
{
    return 0.15;
}

double candidateNonConvergedPenaltyMm()
{
    return 0.50;
}

double candidateMultiResolutionLevelBonusMm()
{
    return 0.02;
}

double candidateSelectionScore(const CandidateEvaluationResult& result)
{
    const double regionQuality =
        candidateHasMeaningfulRegionMetrics(result)
        ? candidateRegionQualityScore(result)
        : 0.0;
    const double convergencePenalty =
        result.converged ? 0.0 : candidateNonConvergedPenaltyMm();
    const double multiResolutionBonus =
        qMax(result.multiResolutionLevel, 0) * candidateMultiResolutionLevelBonusMm();
    const double normalConsistencyScore =
        qBound(0.0, result.normalConsistencyScore, 1.0);
    const double curvatureScore =
        qBound(0.0, result.curvatureScore, 1.0);
    return result.coarseScore
        - candidateRegionQualityBonusMm() * regionQuality
        - candidateNormalConsistencyBonusMm() * normalConsistencyScore
        - candidateCurvatureBonusMm() * curvatureScore
        + convergencePenalty
        - multiResolutionBonus;
}

}

QList<CandidateInitialTransform> buildCandidateInitialTransforms(
    const QMatrix4x4& coarseTransform,
    const QVector3D& targetRegionCenter,
    const ParallelSearchPlan& plan)
{
    QList<CandidateInitialTransform> candidates;
    const int requestedCount = qMax(plan.candidateCount, 1);
    candidates.reserve(requestedCount);
    const QList<PosePerturbation> orderedPerturbations = buildOrderedPosePerturbations();

    for (int index = 0; index < requestedCount; ++index) {
        CandidateInitialTransform candidate;
        candidate.candidateId = QStringLiteral("candidate_%1").arg(index, 3, 10, QChar('0'));
        candidate.seedType = index == 0
            ? QStringLiteral("landmark_identity_seed")
            : QStringLiteral("axis_perturbation_seed");
        candidate.rankHint = index;

        if (index > 0 && !orderedPerturbations.isEmpty()) {
            const PosePerturbation& perturbation = orderedPerturbations.at(index - 1);
            candidate.rotationDeltaDeg = perturbation.rotationDeltaDeg;
            candidate.translationDeltaMm = perturbation.translationDeltaMm;
        }

        QMatrix4x4 transform = coarseTransform;
        transform.translate(targetRegionCenter);
        transform.rotate(candidate.rotationDeltaDeg.y(), 0.0f, 1.0f, 0.0f);
        transform.rotate(candidate.rotationDeltaDeg.x(), 1.0f, 0.0f, 0.0f);
        transform.rotate(candidate.rotationDeltaDeg.z(), 0.0f, 0.0f, 1.0f);
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
            const double leftSelectionScore = candidateSelectionScore(left);
            const double rightSelectionScore = candidateSelectionScore(right);
            if (!qFuzzyCompare(1.0 + leftSelectionScore, 1.0 + rightSelectionScore)) {
                return leftSelectionScore < rightSelectionScore;
            }

            const double coarseDelta = qAbs(left.coarseScore - right.coarseScore);
            const bool bothHaveRegionMetrics =
                candidateHasMeaningfulRegionMetrics(left) && candidateHasMeaningfulRegionMetrics(right);
            if (bothHaveRegionMetrics && coarseDelta <= coarseScoreTieWindowMm()) {
                const double leftQuality = candidateRegionQualityScore(left);
                const double rightQuality = candidateRegionQualityScore(right);
                if (!qFuzzyCompare(1.0 + leftQuality, 1.0 + rightQuality)) {
                    return leftQuality > rightQuality;
                }
                if (!qFuzzyCompare(1.0 + left.targetRegionHitRatio, 1.0 + right.targetRegionHitRatio)) {
                    return left.targetRegionHitRatio > right.targetRegionHitRatio;
                }
                if (!qFuzzyCompare(1.0 + left.coverageScore, 1.0 + right.coverageScore)) {
                    return left.coverageScore > right.coverageScore;
                }
            }
            if (!qFuzzyCompare(1.0 + left.coarseScore, 1.0 + right.coarseScore)) {
                return left.coarseScore < right.coarseScore;
            }
            if (!qFuzzyCompare(1.0 + left.targetRegionHitRatio, 1.0 + right.targetRegionHitRatio)) {
                return left.targetRegionHitRatio > right.targetRegionHitRatio;
            }
            if (!qFuzzyCompare(1.0 + left.coverageScore, 1.0 + right.coverageScore)) {
                return left.coverageScore > right.coverageScore;
            }
            if (left.converged != right.converged) {
                return left.converged && !right.converged;
            }
            if (left.multiResolutionLevel != right.multiResolutionLevel) {
                return left.multiResolutionLevel > right.multiResolutionLevel;
            }
            if (left.candidateId != right.candidateId) {
                return left.candidateId < right.candidateId;
            }
            return left.coarseScore < right.coarseScore;
        });
    while (sorted.size() > topKCount) {
        sorted.removeLast();
    }
    return sorted;
}

QList<CandidateEvaluationResult> selectRefineCandidates(
    const QList<CandidateEvaluationResult>& topKCandidateScores,
    int requestedRefineCount,
    bool adaptiveRefineEnabled)
{
    if (requestedRefineCount <= 0 || topKCandidateScores.isEmpty()) {
        return {};
    }

    QList<CandidateEvaluationResult> cappedCandidates = topKCandidateScores;
    while (cappedCandidates.size() > requestedRefineCount) {
        cappedCandidates.removeLast();
    }

    if (!adaptiveRefineEnabled
        || cappedCandidates.size() <= 2
        || cappedCandidates.first().coarseScore > strongRefineReductionScoreMm()) {
        return cappedCandidates;
    }

    const QString identityCandidateId = QStringLiteral("candidate_000");
    int identityCandidateIndex = -1;
    for (int candidateIndex = 0; candidateIndex < cappedCandidates.size(); ++candidateIndex) {
        if (cappedCandidates.at(candidateIndex).candidateId == identityCandidateId) {
            identityCandidateIndex = candidateIndex;
            break;
        }
    }
    if (identityCandidateIndex < 0) {
        return cappedCandidates;
    }

    const double bestCoarseScore = cappedCandidates.first().coarseScore;
    const double identityCoarseScore = cappedCandidates.at(identityCandidateIndex).coarseScore;
    if (identityCoarseScore > bestCoarseScore + baselineRefineReductionScoreWindowMm()) {
        return cappedCandidates;
    }

    QList<CandidateEvaluationResult> selectedCandidates;
    selectedCandidates.append(cappedCandidates.first());
    if (identityCandidateIndex == 0) {
        selectedCandidates.append(cappedCandidates.at(1));
    } else {
        selectedCandidates.append(cappedCandidates.at(identityCandidateIndex));
    }

    return selectedCandidates;
}

QList<double> resolveMultiResolutionCellSizes(const QString& profileId)
{
    if (profileId == QStringLiteral("ankle_roi_three_level")) {
        return { 3.0, 1.5, 1.0 };
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
    map.insert(QStringLiteral("normal_consistency_score"), result.normalConsistencyScore);
    map.insert(QStringLiteral("curvature_score"), result.curvatureScore);
    map.insert(QStringLiteral("geometry_score_available"), result.geometryScoreAvailable);
    map.insert(QStringLiteral("selection_score"), candidateSelectionScore(result));
    map.insert(QStringLiteral("converged"), result.converged);
    map.insert(QStringLiteral("multi_resolution_level"), result.multiResolutionLevel);
    return map;
}

InitialAdmissionDecision assessInitialAdmission(
    const CandidateEvaluationResult& identityCandidate,
    const QList<CandidateEvaluationResult>& topKCandidateScores,
    const InitialAdmissionPolicy& policy)
{
    return assessInitialAdmission(
        identityCandidate,
        topKCandidateScores,
        policy,
        InitialAdmissionEvidence());
}

InitialAdmissionDecision assessInitialAdmission(
    const CandidateEvaluationResult& identityCandidate,
    const QList<CandidateEvaluationResult>& topKCandidateScores,
    const InitialAdmissionPolicy& policy,
    const InitialAdmissionEvidence& evidence)
{
    InitialAdmissionDecision decision;
    decision.identityCoarseScoreMm = identityCandidate.coarseScore;
    decision.identityRegionHitRatio = identityCandidate.targetRegionHitRatio;
    decision.identityCoverageScore = identityCandidate.coverageScore;
    decision.bestCoarseScoreMm =
        topKCandidateScores.isEmpty()
            ? identityCandidate.coarseScore
            : topKCandidateScores.first().coarseScore;
    decision.robustInitialMetricsUsed = evidence.hasRobustInitialMetrics;
    decision.robustInitialRmsMm = evidence.robustInitialRmsMm;
    decision.robustInitialConfidence = evidence.robustInitialConfidence;
    decision.robustInitialInlierCount = evidence.robustInitialInlierCount;

    if (evidence.hasRobustInitialMetrics
        && evidence.robustInitialRmsMm > policy.robustResidualRecoveryMaxMm) {
        decision.action = QStringLiteral("reject");
        decision.reason = QStringLiteral("robust_initial_residual_exceeds_recovery_threshold");
        decision.recoveryAction =
            QStringLiteral("resample_probe_points_or_check_probe_calibration");
        return decision;
    }

    if (identityCandidate.coarseScore > policy.recoveryMaxCoarseScoreMm) {
        decision.action = QStringLiteral("reject");
        decision.reason = QStringLiteral("initial_score_exceeds_recovery_threshold");
        decision.recoveryAction =
            QStringLiteral("resample_probe_points_or_check_probe_calibration");
        return decision;
    }

    const bool robustResidualAllowsFastPath =
        !evidence.hasRobustInitialMetrics
        || evidence.robustInitialRmsMm <= policy.robustResidualFastPathMaxMm;
    const bool confidentIdentity =
        identityCandidate.coarseScore <= policy.fastPathMaxCoarseScoreMm
        && identityCandidate.targetRegionHitRatio >= policy.fastPathMinRegionHitRatio
        && identityCandidate.coverageScore >= policy.fastPathMinCoverageScore
        && identityCandidate.converged
        && robustResidualAllowsFastPath;
    if (confidentIdentity) {
        decision.accepted = true;
        decision.action = QStringLiteral("fast_path");
        decision.reason = QStringLiteral("confident_identity_initial");
        return decision;
    }

    if (!robustResidualAllowsFastPath
        && decision.bestCoarseScoreMm <= policy.refineMaxCoarseScoreMm
        && identityCandidate.coarseScore <= policy.refineMaxCoarseScoreMm
        && identityCandidate.targetRegionHitRatio >= policy.refineMinRegionHitRatio
        && identityCandidate.coverageScore >= policy.refineMinCoverageScore) {
        decision.accepted = true;
        decision.action = QStringLiteral("refine");
        decision.reason = QStringLiteral("robust_initial_residual_requires_refine");
        return decision;
    }

    const bool refineableInitial =
        decision.bestCoarseScoreMm <= policy.refineMaxCoarseScoreMm
        && identityCandidate.coarseScore <= policy.refineMaxCoarseScoreMm
        && identityCandidate.targetRegionHitRatio >= policy.refineMinRegionHitRatio
        && identityCandidate.coverageScore >= policy.refineMinCoverageScore;
    if (refineableInitial) {
        decision.accepted = true;
        decision.action = QStringLiteral("refine");
        decision.reason = QStringLiteral("medium_quality_initial_requires_refine");
        return decision;
    }

    decision.action = QStringLiteral("reject");
    decision.reason = QStringLiteral("initial_quality_below_refine_threshold");
    decision.recoveryAction = QStringLiteral("add_wider_spread_anchor_points");
    return decision;
}

QVariantMap initialAdmissionDecisionToVariantMap(const InitialAdmissionDecision& decision)
{
    QVariantMap map;
    map.insert(QStringLiteral("accepted"), decision.accepted);
    map.insert(QStringLiteral("action"), decision.action);
    map.insert(QStringLiteral("reason"), decision.reason);
    map.insert(QStringLiteral("recovery_action"), decision.recoveryAction);
    map.insert(QStringLiteral("identity_coarse_score_mm"), decision.identityCoarseScoreMm);
    map.insert(QStringLiteral("best_coarse_score_mm"), decision.bestCoarseScoreMm);
    map.insert(QStringLiteral("identity_region_hit_ratio"), decision.identityRegionHitRatio);
    map.insert(QStringLiteral("identity_coverage_score"), decision.identityCoverageScore);
    map.insert(QStringLiteral("robust_initial_metrics_used"), decision.robustInitialMetricsUsed);
    map.insert(QStringLiteral("robust_initial_rms_mm"), decision.robustInitialRmsMm);
    map.insert(QStringLiteral("robust_initial_confidence"), decision.robustInitialConfidence);
    map.insert(QStringLiteral("robust_initial_inlier_count"), decision.robustInitialInlierCount);
    return map;
}
