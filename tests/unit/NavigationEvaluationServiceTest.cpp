#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
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
    run.warnings = QStringList{
        QStringLiteral("collect_more_points"),
        QStringLiteral("check_tracking_visibility")
    };

    AnkleEvaluationReport report;
    report.caseId = QStringLiteral("ankle-case-005");
    report.translationErrorMm = 1.1;
    report.rotationErrorDeg = 2.4;
    report.allowNavigation = true;
    report.confidenceScore = 0.88;
    report.gateReasons = run.warnings;
    report.calibrated = true;
    report.calibrationAccuracyMm = 0.42;

    QVERIFY(service.saveRegistrationRecord(registration));
    QVERIFY(service.saveNavigationRun(run));
    QVERIFY(service.saveEvaluationReport(report));
    QVERIFY(service.exportMetricsCsv(report.caseId));

    QVERIFY(QFileInfo::exists(tempRoot.path() + QStringLiteral("/ankle-case-005/registration/registration_result.json")));
    QVERIFY(QFileInfo::exists(tempRoot.path() + QStringLiteral("/ankle-case-005/navigation/navigation_run.json")));
    QVERIFY(QFileInfo::exists(tempRoot.path() + QStringLiteral("/ankle-case-005/evaluation/evaluation_report.json")));
    QVERIFY(QFileInfo::exists(tempRoot.path() + QStringLiteral("/ankle-case-005/evaluation/evaluation_metrics.csv")));

    QFile evaluationFile(tempRoot.path() + QStringLiteral("/ankle-case-005/evaluation/evaluation_report.json"));
    QVERIFY(evaluationFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonObject evaluationObject = QJsonDocument::fromJson(evaluationFile.readAll()).object();
    QCOMPARE(evaluationObject.value(QStringLiteral("confidence_score")).toDouble(), 0.88);
    QCOMPARE(evaluationObject.value(QStringLiteral("gate_reasons")).toArray().size(), 2);
    QCOMPARE(evaluationObject.value(QStringLiteral("calibrated")).toBool(), true);
    QCOMPARE(evaluationObject.value(QStringLiteral("calibration_accuracy_mm")).toDouble(), 0.42);

    QFile csvFile(tempRoot.path() + QStringLiteral("/ankle-case-005/evaluation/evaluation_metrics.csv"));
    QVERIFY(csvFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString csvContent = QString::fromUtf8(csvFile.readAll());
    QVERIFY(csvContent.contains(QStringLiteral("evaluation_confidence_score,0.8800")));
    QVERIFY(csvContent.contains(QStringLiteral("gate_reasons,\"collect_more_points; check_tracking_visibility\"")));
    QVERIFY(csvContent.contains(QStringLiteral("calibrated,true")));
    QVERIFY(csvContent.contains(QStringLiteral("calibration_accuracy_mm,0.4200")));
}

QTEST_APPLESS_MAIN(NavigationEvaluationServiceTest)
#include "NavigationEvaluationServiceTest.moc"
