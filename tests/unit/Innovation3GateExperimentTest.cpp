#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "Framework/Navigation/navigation_evaluation_service.h"
#include "Framework/Navigation/innovation_3_gate_experiment.h"

class Innovation3GateExperimentTest : public QObject
{
    Q_OBJECT

private slots:
    void experiment_runs_three_gate_strategies_and_exports_core_metrics();
    void experiment_uses_case_evaluation_twin_metrics_when_workspace_is_provided();
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

void Innovation3GateExperimentTest::experiment_uses_case_evaluation_twin_metrics_when_workspace_is_provided()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationEvaluationService evaluationService(tempRoot.path() + QStringLiteral("/cases"));
    AnkleEvaluationReport report;
    report.caseId = QStringLiteral("ankle-case-704");
    report.allowNavigation = true;
    report.confidenceScore = 0.86;
    report.gateReasons = QStringList {
        QStringLiteral("review_target_alignment")
    };
    report.calibrated = true;
    report.calibrationAccuracyMm = 0.29;
    report.metrics.insert(QStringLiteral("twin_confidence_score"), 0.41);
    report.metrics.insert(QStringLiteral("local_risk_score"), 0.78);
    report.metrics.insert(QStringLiteral("target_region_distance_mm"), 6.40);
    report.metrics.insert(QStringLiteral("target_region_angle_error_deg"), 9.50);
    report.metrics.insert(QStringLiteral("dominant_risk_source"), QStringLiteral("registration"));
    report.metrics.insert(QStringLiteral("re_register_recommended"), true);
    report.metrics.insert(QStringLiteral("tracking_degradation_detected"), true);
    QVERIFY(evaluationService.saveEvaluationReport(report));

    Innovation3GateExperiment experiment;

    Innovation3GateInput input;
    input.caseId = report.caseId;
    input.caseDataRoot = tempRoot.path();
    input.gateStrategyIds = QStringList({
        QStringLiteral("no_gate"),
        QStringLiteral("threshold_only"),
        QStringLiteral("joint_confidence")
    });

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 3);

    QMap<QString, InnovationExperimentRecord> recordsByStrategy;
    for (const InnovationExperimentRecord& record : records) {
        recordsByStrategy.insert(record.strategyId, record);
        QCOMPARE(record.metrics.value(QStringLiteral("twin_confidence_score")).toDouble(), 0.41);
        QCOMPARE(record.metrics.value(QStringLiteral("local_risk_score")).toDouble(), 0.78);
        QCOMPARE(record.metrics.value(QStringLiteral("target_region_distance_mm")).toDouble(), 6.40);
        QCOMPARE(record.metrics.value(QStringLiteral("dominant_risk_source")).toString(), QStringLiteral("registration"));
        QCOMPARE(record.metrics.value(QStringLiteral("re_register_recommended")).toBool(), true);
        QCOMPARE(record.metrics.value(QStringLiteral("tracking_degradation_detected")).toBool(), true);
    }

    QCOMPARE(recordsByStrategy.value(QStringLiteral("joint_confidence")).metrics.value(QStringLiteral("allow_navigation")).toBool(), false);
    QVERIFY(recordsByStrategy.value(QStringLiteral("joint_confidence")).metrics.value(QStringLiteral("gate_reasons")).toString().contains(QStringLiteral("re_register_recommended")));
    QVERIFY(recordsByStrategy.value(QStringLiteral("joint_confidence")).metrics.value(QStringLiteral("gate_reasons")).toString().contains(QStringLiteral("dominant_risk_source=registration")));
}

QTEST_APPLESS_MAIN(Innovation3GateExperimentTest)
#include "Innovation3GateExperimentTest.moc"
