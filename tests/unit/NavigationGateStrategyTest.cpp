#include <QtTest/QtTest>

#include "Plugins/PointRegistration/joint_confidence_gate_strategy.h"
#include "Plugins/PointRegistration/navigation_gate_strategy.h"
#include "Plugins/PointRegistration/no_gate_strategy.h"
#include "Plugins/PointRegistration/threshold_only_gate_strategy.h"

class NavigationGateStrategyTest : public QObject
{
    Q_OBJECT

private slots:
    void strategies_produce_distinct_navigation_gate_decisions();
};

void NavigationGateStrategyTest::strategies_produce_distinct_navigation_gate_decisions()
{
    NavigationConfidenceInputs inputs;
    inputs.fre = 1.2;
    inputs.targetTre = 3.6;
    inputs.coverageScore = 0.40;
    inputs.surfaceResidual = 2.2;
    inputs.trackingJitter = 1.5;
    inputs.visibleFrameRatio = 0.78;

    NoGateStrategy noGate;
    ThresholdOnlyGateStrategy thresholdOnly;
    JointConfidenceGateStrategy joint;

    QVERIFY(noGate.evaluate(inputs).allowNavigation);
    QVERIFY(!thresholdOnly.evaluate(inputs).allowNavigation);
    QVERIFY(!joint.evaluate(inputs).allowNavigation);
}

QTEST_APPLESS_MAIN(NavigationGateStrategyTest)
#include "NavigationGateStrategyTest.moc"
