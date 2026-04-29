#include <QtTest/QtTest>

#include "Framework/Navigation/navigation_confidence_evaluator.h"

class NavigationConfidenceEvaluatorTest : public QObject
{
    Q_OBJECT

private slots:
    void evaluator_blocks_navigation_when_tre_or_tracking_quality_is_poor();
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

QTEST_APPLESS_MAIN(NavigationConfidenceEvaluatorTest)
#include "NavigationConfidenceEvaluatorTest.moc"
