#include <QtTest/QtTest>

#include "Framework/Navigation/navigation_confidence_evaluator.h"

class NavigationConfidenceEvaluatorTest : public QObject
{
    Q_OBJECT

private slots:
    void evaluator_blocks_navigation_when_tre_or_tracking_quality_is_poor();
    void evaluator_blocks_navigation_when_probe_is_not_calibrated_or_accuracy_is_poor();
};

void NavigationConfidenceEvaluatorTest::evaluator_blocks_navigation_when_tre_or_tracking_quality_is_poor()
{
    NavigationConfidenceInputs inputs;
    inputs.fre = 0.8;
    inputs.targetTre = 3.5;
    inputs.coverageScore = 0.35;
    inputs.surfaceResidual = 1.9;
    inputs.trackingJitter = 1.8;
    inputs.visibleFrameRatio = 0.70;

    const NavigationConfidenceResult result = NavigationConfidenceEvaluator().evaluate(inputs);

    QVERIFY(!result.allowNavigation);
    QVERIFY(result.score < 0.6);
    QVERIFY(result.recommendations.contains(QString::fromUtf8("\u8865\u91c7\u70b9")));
}

void NavigationConfidenceEvaluatorTest::evaluator_blocks_navigation_when_probe_is_not_calibrated_or_accuracy_is_poor()
{
    NavigationConfidenceInputs uncalibratedInputs;
    uncalibratedInputs.fre = 0.6;
    uncalibratedInputs.targetTre = 1.2;
    uncalibratedInputs.coverageScore = 0.92;
    uncalibratedInputs.surfaceResidual = 0.8;
    uncalibratedInputs.trackingJitter = 0.2;
    uncalibratedInputs.visibleFrameRatio = 0.99;
    uncalibratedInputs.toolCalibrated = false;
    uncalibratedInputs.calibrationAccuracy = 0.0;

    const NavigationConfidenceResult uncalibratedResult =
        NavigationConfidenceEvaluator().evaluate(uncalibratedInputs);

    QVERIFY(!uncalibratedResult.allowNavigation);
    QVERIFY(uncalibratedResult.recommendations.contains(QString::fromUtf8("\u5148\u5b8c\u6210\u63a2\u9488\u6807\u5b9a")));

    NavigationConfidenceInputs lowAccuracyInputs = uncalibratedInputs;
    lowAccuracyInputs.toolCalibrated = true;
    lowAccuracyInputs.calibrationAccuracy = 2.4;

    const NavigationConfidenceResult lowAccuracyResult =
        NavigationConfidenceEvaluator().evaluate(lowAccuracyInputs);

    QVERIFY(!lowAccuracyResult.allowNavigation);
    QVERIFY(lowAccuracyResult.recommendations.contains(QString::fromUtf8("\u91cd\u65b0\u6267\u884c\u63a2\u9488\u6807\u5b9a")));
}

QTEST_APPLESS_MAIN(NavigationConfidenceEvaluatorTest)
#include "NavigationConfidenceEvaluatorTest.moc"
