#include <QtTest/QtTest>

#include <QDir>
#include <QFile>

#include "Framework/Navigation/innovation_experiment_batch_runner.h"

class InnovationExperimentBatchRunnerTest : public QObject
{
    Q_OBJECT

private slots:
    void batch_runner_exports_three_summary_csv_files_for_a_case_set();
};

void InnovationExperimentBatchRunnerTest::batch_runner_exports_three_summary_csv_files_for_a_case_set()
{
    InnovationExperimentBatchRunner runner;
    InnovationBatchInput input;
    input.caseIds = QStringList({
        QStringLiteral("ankle-case-301"),
        QStringLiteral("ankle-case-302")
    });

    const InnovationBatchOutput output = runner.run(input);

    QCOMPARE(output.summaryFiles.size(), 3);
    QVERIFY(output.summaryFiles.contains(QStringLiteral("innovation_1_summary.csv")));
    QVERIFY(output.summaryFiles.contains(QStringLiteral("innovation_2_summary.csv")));
    QVERIFY(output.summaryFiles.contains(QStringLiteral("innovation_3_summary.csv")));

    QFile innovation3Summary(QDir::currentPath() + QStringLiteral("/summaries/innovation_3_summary.csv"));
    QVERIFY(innovation3Summary.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString innovation3Csv = QString::fromUtf8(innovation3Summary.readAll());
    QVERIFY(innovation3Csv.contains(QStringLiteral("confidence_score")));
    QVERIFY(innovation3Csv.contains(QStringLiteral("allow_navigation")));
    QVERIFY(innovation3Csv.contains(QStringLiteral("gate_reasons")));
    QVERIFY(innovation3Csv.contains(QStringLiteral("calibrated")));
    QVERIFY(innovation3Csv.contains(QStringLiteral("calibration_accuracy_mm")));
}

QTEST_APPLESS_MAIN(InnovationExperimentBatchRunnerTest)
#include "InnovationExperimentBatchRunnerTest.moc"
