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
};

struct ParallelSearchPlan
{
    int candidateCount = 64;
    int topKCount = 4;
    QString multiResolutionProfileId = QStringLiteral("ankle_roi_three_level");
};

QList<CandidateInitialTransform> buildCandidateInitialTransforms(
    const QMatrix4x4& coarseTransform,
    const QVector3D& targetRegionCenter,
    const ParallelSearchPlan& plan);

QList<CandidateEvaluationResult> selectTopKCandidates(
    const QList<CandidateEvaluationResult>& scores,
    int topKCount);

QList<double> resolveMultiResolutionCellSizes(const QString& profileId);
QStringList candidateIds(const QList<CandidateEvaluationResult>& scores);
QList<CandidateInitialTransform> filterCandidatesByIds(
    const QList<CandidateInitialTransform>& candidates,
    const QStringList& candidateIds);
QVariantMap candidateEvaluationToVariantMap(const CandidateEvaluationResult& result);
