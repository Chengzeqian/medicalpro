#include <QtTest/QtTest>

#include "Plugins/RegistrationCore/robust_initial_transform.h"

namespace
{
bool fuzzyVectorEquals(const QVector3D& actual, const QVector3D& expected, float toleranceMm)
{
    return (actual - expected).length() <= toleranceMm;
}

QVector3D transformPoint(const QMatrix4x4& transform, const QVector3D& point)
{
    return transform.map(point);
}
}

class RobustInitialTransformTest : public QObject
{
    Q_OBJECT

private slots:
    void stable_probe_point_accepts_low_jitter_samples();
    void stable_probe_point_rejects_high_tracking_error_and_jitter();
    void initial_point_set_quality_rejects_nearly_collinear_points();
    void robust_initial_transform_rejects_one_outlier_correspondence();
};

void RobustInitialTransformTest::stable_probe_point_accepts_low_jitter_samples()
{
    QList<ProbeTipFrameSample> samples = {
        { QVector3D(10.00f, 20.00f, 30.00f), 0.12, 0.0, true },
        { QVector3D(10.08f, 19.96f, 30.03f), 0.10, 8.0, true },
        { QVector3D(9.94f, 20.04f, 29.99f), 0.13, 16.0, true },
        { QVector3D(10.03f, 20.02f, 30.06f), 0.09, 24.0, true }
    };

    StableProbePointOptions options;
    options.minimumAcceptedFrames = 4;
    options.maxTrackingErrorMm = 0.35;
    options.maxJitterRmsMm = 0.25;

    const StableProbePointResult result = collectStableProbePoint(samples, options);

    QVERIFY(result.accepted);
    QCOMPARE(result.acceptedFrameCount, 4);
    QVERIFY(result.jitterRmsMm < 0.12);
    QVERIFY(result.confidence > 0.85);
    QVERIFY(fuzzyVectorEquals(result.pointMm, QVector3D(10.02f, 20.01f, 30.02f), 0.08f));
}

void RobustInitialTransformTest::stable_probe_point_rejects_high_tracking_error_and_jitter()
{
    QList<ProbeTipFrameSample> samples = {
        { QVector3D(5.0f, 5.0f, 5.0f), 0.12, 0.0, true },
        { QVector3D(8.5f, 5.0f, 5.0f), 0.14, 8.0, true },
        { QVector3D(5.1f, 8.4f, 5.0f), 0.90, 16.0, true },
        { QVector3D(5.2f, 5.1f, 8.6f), 0.11, 24.0, true }
    };

    StableProbePointOptions options;
    options.minimumAcceptedFrames = 3;
    options.maxTrackingErrorMm = 0.35;
    options.maxJitterRmsMm = 0.50;

    const StableProbePointResult result = collectStableProbePoint(samples, options);

    QVERIFY(!result.accepted);
    QVERIFY(result.rejectedFrameCount >= 1);
    QVERIFY(result.jitterRmsMm > options.maxJitterRmsMm);
    QCOMPARE(result.reason, QStringLiteral("jitter_too_high"));
}

void RobustInitialTransformTest::initial_point_set_quality_rejects_nearly_collinear_points()
{
    const QList<QVector3D> points = {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(10.0f, 0.02f, 0.0f),
        QVector3D(20.0f, 0.04f, 0.0f),
        QVector3D(30.0f, 0.05f, 0.0f)
    };

    InitialValueQualityOptions options;
    options.minBoundingDiagonalMm = 20.0;
    options.minTriangleAreaMm2 = 15.0;
    options.minNonCollinearityScore = 0.05;

    const InitialValueQuality quality = evaluateInitialPointSetQuality(points, options);

    QVERIFY(!quality.accepted);
    QVERIFY(quality.boundingDiagonalMm > 29.0);
    QVERIFY(quality.bestTriangleAreaMm2 < 1.0);
    QCOMPARE(quality.reason, QStringLiteral("points_nearly_collinear"));
}

void RobustInitialTransformTest::robust_initial_transform_rejects_one_outlier_correspondence()
{
    const QList<QVector3D> source = {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(40.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 35.0f, 0.0f),
        QVector3D(0.0f, 0.0f, 30.0f)
    };

    QMatrix4x4 groundTruth;
    groundTruth.setToIdentity();
    groundTruth.translate(12.0f, -7.0f, 4.0f);
    groundTruth.rotate(18.0f, 0.0f, 0.0f, 1.0f);

    QList<QVector3D> target;
    for (const QVector3D& point : source) {
        target.append(transformPoint(groundTruth, point));
    }
    target[3] += QVector3D(18.0f, -22.0f, 11.0f);

    RobustInitialTransformOptions options;
    options.inlierResidualThresholdMm = 1.0;
    options.minimumInlierCount = 3;
    options.quality.minBoundingDiagonalMm = 25.0;
    options.quality.minTriangleAreaMm2 = 100.0;

    const RobustInitialTransformResult result =
        estimateRobustInitialTransform(source, target, options);

    QVERIFY(result.success);
    QCOMPARE(result.inlierCount, 3);
    QCOMPARE(result.rejectedOutlierCount, 1);
    QVERIFY(result.inlierRmsMm < 0.05);
    QVERIFY(result.confidence > 0.70);
    QVERIFY(fuzzyVectorEquals(transformPoint(result.transform, source[0]), target[0], 0.05f));
    QVERIFY(fuzzyVectorEquals(transformPoint(result.transform, source[1]), target[1], 0.05f));
    QVERIFY(fuzzyVectorEquals(transformPoint(result.transform, source[2]), target[2], 0.05f));
}

QTEST_APPLESS_MAIN(RobustInitialTransformTest)
#include "RobustInitialTransformTest.moc"
