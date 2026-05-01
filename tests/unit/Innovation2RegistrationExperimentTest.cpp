#include <QtTest/QtTest>

#include "Framework/Navigation/innovation_2_registration_experiment.h"

class Innovation2RegistrationExperimentTest : public QObject
{
    Q_OBJECT

private slots:
    void experiment_runs_four_registration_methods_and_exports_core_metrics();
};

void Innovation2RegistrationExperimentTest::experiment_runs_four_registration_methods_and_exports_core_metrics()
{
    Innovation2RegistrationExperiment experiment;

    Innovation2RegistrationInput input;
    input.caseId = QStringLiteral("ankle-case-203");
    input.registrationMethodIds = QStringList({
        QStringLiteral("single_stage_landmark"),
        QStringLiteral("landmark_plus_global_icp"),
        QStringLiteral("landmark_plus_global_gicp"),
        QStringLiteral("ankle_two_stage_constrained")
    });

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 4);
    QVERIFY(records.first().metrics.contains(QStringLiteral("fre_mm")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("overall_tre_mm")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("target_tre_mm")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("convergence_success")));
    QVERIFY(records.first().metrics.contains(QStringLiteral("runtime_ms")));
}

QTEST_APPLESS_MAIN(Innovation2RegistrationExperimentTest)
#include "Innovation2RegistrationExperimentTest.moc"
