#pragma once

#include <QList>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

struct WeightedRigidRegistrationResult
{
    bool success = false;
    QMatrix4x4 transform;
    QVector3D translation;
    QQuaternion rotation;
    double weightedRmsError = 0.0;
};

enum class AnkleRegistrationMethod
{
    SingleStageLandmark,
    LandmarkPlusGlobalIcp,
    LandmarkPlusGlobalGicp,
    AnkleTwoStageConstrained
};

class AnkleRegistrationUtils
{
public:
    static WeightedRigidRegistrationResult solveWeightedRigid(
        const QList<QVector3D>& source,
        const QList<QVector3D>& target,
        const QList<double>& weights);

    static QList<int> selectRoiPointIndices(
        const QList<QVector3D>& modelPoints,
        const QVector3D& roiCenter,
        double roiRadiusMm);
};
