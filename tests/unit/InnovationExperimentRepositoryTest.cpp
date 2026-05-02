#include <QtTest/QtTest>

#include <QFileInfo>
#include <QTemporaryDir>

#include "Framework/Navigation/innovation_experiment_repository.h"

class InnovationExperimentRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void repository_writes_experiment_json_under_case_evaluation_tree();
};

void InnovationExperimentRepositoryTest::repository_writes_experiment_json_under_case_evaluation_tree()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    InnovationExperimentRepository repository(tempRoot.path());

    InnovationExperimentRecord record;
    record.caseId = QStringLiteral("ankle-case-201");
    record.innovationId = QStringLiteral("innovation_1");
    record.strategyId = QStringLiteral("target_sensitive");
    record.runIndex = 0;
    record.metrics.insert(QStringLiteral("target_tre_mm"), 1.25);

    QVERIFY(repository.saveRecord(record));

    QVERIFY(QFileInfo::exists(
        tempRoot.path() + QStringLiteral("/ankle-case-201/evaluation/experiments/innovation_1/target_sensitive_run_000.json")));
}

QTEST_APPLESS_MAIN(InnovationExperimentRepositoryTest)
#include "InnovationExperimentRepositoryTest.moc"
