#include <QtTest/QtTest>

#include <QFileInfo>
#include <QTemporaryDir>

#include "Framework/Navigation/navigation_evaluation_service.h"

class NavigationEvaluationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void exporter_writes_registration_navigation_and_csv_reports();
};

void NavigationEvaluationServiceTest::exporter_writes_registration_navigation_and_csv_reports()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationEvaluationService service(tempRoot.path());

    AnkleRegistrationRecord registration;
    registration.caseId = QStringLiteral("ankle-case-005");
    registration.fre = 0.7;
    registration.targetTre = 1.4;
    registration.coverageScore = 0.82;

    AnkleNavigationRunRecord run;
    run.caseId = QStringLiteral("ankle-case-005");
    run.navigationMode = QStringLiteral("replay");
    run.confidenceScore = 0.88;

    AnkleEvaluationReport report;
    report.caseId = QStringLiteral("ankle-case-005");
    report.translationErrorMm = 1.1;
    report.rotationErrorDeg = 2.4;
    report.allowNavigation = true;

    QVERIFY(service.saveRegistrationRecord(registration));
    QVERIFY(service.saveNavigationRun(run));
    QVERIFY(service.saveEvaluationReport(report));
    QVERIFY(service.exportMetricsCsv(report.caseId));

    QVERIFY(QFileInfo::exists(tempRoot.path() + QStringLiteral("/ankle-case-005/registration/registration_result.json")));
    QVERIFY(QFileInfo::exists(tempRoot.path() + QStringLiteral("/ankle-case-005/navigation/navigation_run.json")));
    QVERIFY(QFileInfo::exists(tempRoot.path() + QStringLiteral("/ankle-case-005/evaluation/evaluation_metrics.csv")));
}

QTEST_APPLESS_MAIN(NavigationEvaluationServiceTest)
#include "NavigationEvaluationServiceTest.moc"
