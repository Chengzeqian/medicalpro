#pragma once

#include <QList>
#include <QMatrix4x4>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector3D>

struct CandidateInitialTransform
{
    QString candidateId;
    QString seedType;
    QMatrix4x4 transformMatrix;
    QVector3D rotationDeltaDeg;
    QVector3D translationDeltaMm;
    int rankHint = -1;
};

struct CandidateEvaluationResult
{
    QString candidateId;
    double coarseScore = 0.0;
    double targetRegionHitRatio = 0.0;
    double coverageScore = 0.0;
    bool converged = false;
    int multiResolutionLevel = 0;
    double normalConsistencyScore = 0.0;
    double curvatureScore = 0.0;
    bool geometryScoreAvailable = false;
};

struct ParallelSearchPlan
{
    int candidateCount = 64;
    int topKCount = 4;
    QString multiResolutionProfileId = QStringLiteral("ankle_roi_two_level");
};

struct InitialAdmissionPolicy
{
    double fastPathMaxCoarseScoreMm = 1.0;
    double fastPathMinRegionHitRatio = 0.80;
    double fastPathMinCoverageScore = 0.70;
    double refineMaxCoarseScoreMm = 5.0;
    double refineMinRegionHitRatio = 0.45;
    double refineMinCoverageScore = 0.35;
    double recoveryMaxCoarseScoreMm = 8.0;
    double robustResidualFastPathMaxMm = 3.0;
    double robustResidualRecoveryMaxMm = 8.0;
};

struct InitialAdmissionEvidence
{
    bool hasRobustInitialMetrics = false;
    double robustInitialRmsMm = 0.0;
    double robustInitialConfidence = 0.0;
    int robustInitialInlierCount = 0;
};

struct InitialAdmissionDecision
{
    bool accepted = false;
    QString action;
    QString reason;
    QString recoveryAction;
    double identityCoarseScoreMm = 0.0;
    double bestCoarseScoreMm = 0.0;
    double identityRegionHitRatio = 0.0;
    double identityCoverageScore = 0.0;
    bool robustInitialMetricsUsed = false;
    double robustInitialRmsMm = 0.0;
    double robustInitialConfidence = 0.0;
    int robustInitialInlierCount = 0;
};

QList<CandidateInitialTransform> buildCandidateInitialTransforms(
    const QMatrix4x4& coarseTransform,
    const QVector3D& targetRegionCenter,
    const ParallelSearchPlan& plan);

QList<CandidateEvaluationResult> selectTopKCandidates(
    const QList<CandidateEvaluationResult>& scores,
    int topKCount);
QList<CandidateEvaluationResult> selectRefineCandidates(
    const QList<CandidateEvaluationResult>& topKCandidateScores,
    int requestedRefineCount,
    bool adaptiveRefineEnabled);

QList<double> resolveMultiResolutionCellSizes(const QString& profileId);
QStringList candidateIds(const QList<CandidateEvaluationResult>& scores);
QList<CandidateInitialTransform> filterCandidatesByIds(
    const QList<CandidateInitialTransform>& candidates,
    const QStringList& candidateIds);
QVariantMap candidateEvaluationToVariantMap(const CandidateEvaluationResult& result);
InitialAdmissionDecision assessInitialAdmission(
    const CandidateEvaluationResult& identityCandidate,
    const QList<CandidateEvaluationResult>& topKCandidateScores,
    const InitialAdmissionPolicy& policy);
InitialAdmissionDecision assessInitialAdmission(
    const CandidateEvaluationResult& identityCandidate,
    const QList<CandidateEvaluationResult>& topKCandidateScores,
    const InitialAdmissionPolicy& policy,
    const InitialAdmissionEvidence& evidence);
QVariantMap initialAdmissionDecisionToVariantMap(const InitialAdmissionDecision& decision);
