#pragma once

#include <QList>
#include <QString>
#include <QVector3D>

struct TargetRegistrationRegion
{
    QVector3D origin;
    QVector3D primaryAxis;
    double radiusMm = 0.0;
};

struct CandidateRegistrationPoint
{
    QString pointId;
    QVector3D position;
};

struct RecommendedRegistrationPoint
{
    QString pointId;
    QVector3D position;
    double score = 0.0;
    QString reason;
};

class TargetSensitivePointSelector
{
public:
    QList<RecommendedRegistrationPoint> rankCandidates(
        const TargetRegistrationRegion& region,
        const QList<CandidateRegistrationPoint>& candidates,
        const QList<QVector3D>& alreadySelected) const;
};
