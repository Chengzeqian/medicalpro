#include <QtTest/QtTest>

#include "Framework/Navigation/innovation_1_point_selection_experiment.h"

class Innovation1PointSelectionExperimentTest : public QObject
{
    Q_OBJECT

private slots:
    void experiment_emits_metrics_for_all_point_selection_strategies();
};

void Innovation1PointSelectionExperimentTest::experiment_emits_metrics_for_all_point_selection_strategies()
{
    Innovation1PointSelectionExperiment experiment;

    Innovation1PointSelectionInput input;
    input.caseId = QStringLiteral("ankle-case-202");
    input.strategyIds = QStringList({
        QStringLiteral("target_sensitive"),
        QStringLiteral("random"),
        QStringLiteral("uniform"),
        QStringLiteral("expert_rule")
    });
    input.pointBudget = 5;

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 4);
    QVERIFY(records.first().metrics.contains(QStringLiteral("target_tre_mm")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("overall_tre_mm")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("point_count")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("picking_time_ms")));
}

QTEST_APPLESS_MAIN(Innovation1PointSelectionExperimentTest)
#include "Innovation1PointSelectionExperimentTest.moc"
