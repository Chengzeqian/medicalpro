#pragma once

#include <QList>
#include <QMatrix4x4>
#include <QString>
#include <QVector3D>

struct ProbeTipFrameSample
{
    QVector3D tipPositionMm;
    double trackingErrorMm = 0.0;
    double timestampMs = 0.0;
    bool valid = false;
};

struct StableProbePointOptions
{
    int minimumAcceptedFrames = 5;
    double maxTrackingErrorMm = 0.35;
    double maxJitterRmsMm = 0.35;
};

struct StableProbePointResult
{
    bool accepted = false;
    QVector3D pointMm;
    int acceptedFrameCount = 0;
    int rejectedFrameCount = 0;
    double jitterRmsMm = 0.0;
    double meanTrackingErrorMm = 0.0;
    double confidence = 0.0;
    QString reason;
};

struct InitialValueQualityOptions
{
    double minBoundingDiagonalMm = 15.0;
    double minTriangleAreaMm2 = 25.0;
    double minNonCollinearityScore = 0.03;
};

struct InitialValueQuality
{
    bool accepted = false;
    int pointCount = 0;
    double boundingDiagonalMm = 0.0;
    double bestTriangleAreaMm2 = 0.0;
    double nonCollinearityScore = 0.0;
    double confidence = 0.0;
    QString reason;
};

struct RobustInitialTransformOptions
{
    double inlierResidualThresholdMm = 2.0;
    int minimumInlierCount = 3;
    InitialValueQualityOptions quality;
};

struct RobustInitialTransformResult
{
    bool success = false;
    QMatrix4x4 transform;
    int inlierCount = 0;
    int rejectedOutlierCount = 0;
    double inlierRmsMm = 0.0;
    double confidence = 0.0;
    QList<int> inlierIndices;
    InitialValueQuality sourceQuality;
    InitialValueQuality targetQuality;
    QString reason;
};

StableProbePointResult collectStableProbePoint(
    const QList<ProbeTipFrameSample>& samples,
    const StableProbePointOptions& options = StableProbePointOptions());

InitialValueQuality evaluateInitialPointSetQuality(
    const QList<QVector3D>& points,
    const InitialValueQualityOptions& options = InitialValueQualityOptions());

RobustInitialTransformResult estimateRobustInitialTransform(
    const QList<QVector3D>& sourcePoints,
    const QList<QVector3D>& targetPoints,
    const RobustInitialTransformOptions& options = RobustInitialTransformOptions());
