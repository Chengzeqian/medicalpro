#include <QtTest/QtTest>

#include "Framework/Navigation/innovation_3_gate_experiment.h"

class Innovation3GateExperimentTest : public QObject
{
    Q_OBJECT

private slots:
    void experiment_runs_three_gate_strategies_and_exports_core_metrics();
};

void Innovation3GateExperimentTest::experiment_runs_three_gate_strategies_and_exports_core_metrics()
{
    Innovation3GateExperiment experiment;

    Innovation3GateInput input;
    input.caseId = QStringLiteral("ankle-case-204");
    input.gateStrategyIds = QStringList({
        QStringLiteral("no_gate"),
        QStringLiteral("threshold_only"),
        QStringLiteral("joint_confidence")
    });

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 3);
    QVERIFY(records.first().metrics.contains(QStringLiteral("error_intercept_rate")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("false_pass_rate")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("navigation_success_rate")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("interruption_count")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("confidence_score")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("allow_navigation")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("gate_reasons")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("calibrated")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("calibration_accuracy_mm")));
}

QTEST_APPLESS_MAIN(Innovation3GateExperimentTest)
#include "Innovation3GateExperimentTest.moc"
