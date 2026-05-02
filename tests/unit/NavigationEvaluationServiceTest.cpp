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
    void service_loads_evaluation_snapshot_with_metrics_evidence();
    void service_exports_case_summary_json_and_batch_summary_csv();
    void service_discovers_exportable_case_ids_from_cases_root();
};

void NavigationEvaluationServiceTest::exporter_writes_registration_navigation_and_csv_reports()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationEvaluationService service(tempRoot.path());

    AnkleRegistrationRecord registration;
    registration.caseId = QStringLiteral("ankle-case-005");
    registration.registrationMode = QStringLiteral("ankle_constrained_two_stage");
    registration.fre = 0.7;
    registration.targetTre = 1.4;
    registration.coverageScore = 0.82;
    registration.metrics.insert(QStringLiteral("target_region_radius_mm"), 18.5);
    registration.metrics.insert(QStringLiteral("constraint_region_count"), 3);
    registration.metrics.insert(QStringLiteral("constraint_region_keys"), QStringLiteral("distal_tibia|medial_malleolus|talar_dome"));
    registration.metrics.insert(QStringLiteral("constraint_region_bones"), QStringLiteral("tibia|tibia|talus"));
    registration.metrics.insert(QStringLiteral("constraint_region_roles"), QStringLiteral("implant_window|support|articular"));
    registration.metrics.insert(QStringLiteral("constraint_region_source"), QStringLiteral("planning_defined"));
    registration.metrics.insert(QStringLiteral("constraint_region_version"), QStringLiteral("2026.04"));

    AnkleNavigationRunRecord run;
    run.caseId = QStringLiteral("ankle-case-005");
    run.navigationMode = QStringLiteral("replay");
    run.confidenceScore = 0.88;
    run.warnings = QStringList{
        QStringLiteral("collect_more_points"),
        QStringLiteral("check_tracking_visibility")
    };
    run.metrics.insert(QStringLiteral("tracking_jitter_mm"), 0.31);
    run.metrics.insert(QStringLiteral("visible_frame_ratio"), 0.97);
    run.metrics.insert(QStringLiteral("tracking_profile"), QStringLiteral("live_tracking"));

    AnkleEvaluationReport report;
    report.caseId = QStringLiteral("ankle-case-005");
    report.translationErrorMm = 1.1;
    report.rotationErrorDeg = 2.4;
    report.allowNavigation = true;
    report.confidenceScore = 0.88;
    report.gateReasons = run.warnings;
    report.calibrated = true;
    report.calibrationAccuracyMm = 0.42;
    report.metrics.insert(QStringLiteral("tracking_jitter_mm"), 0.31);
    report.metrics.insert(QStringLiteral("visible_frame_ratio"), 0.97);
    report.metrics.insert(QStringLiteral("tracking_profile"), QStringLiteral("live_tracking"));
    report.metrics.insert(QStringLiteral("constraint_region_count"), 3);
    report.metrics.insert(QStringLiteral("constraint_region_keys"), QStringLiteral("distal_tibia|medial_malleolus|talar_dome"));
    report.metrics.insert(QStringLiteral("target_region_radius_mm"), 18.5);

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
    const QJsonObject evaluationMetrics = evaluationObject.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(evaluationMetrics.value(QStringLiteral("tracking_jitter_mm")).toDouble(), 0.31);
    QCOMPARE(evaluationMetrics.value(QStringLiteral("visible_frame_ratio")).toDouble(), 0.97);
    QCOMPARE(evaluationMetrics.value(QStringLiteral("constraint_region_count")).toInt(), 3);
    QCOMPARE(evaluationMetrics.value(QStringLiteral("target_region_radius_mm")).toDouble(), 18.5);

    QFile csvFile(tempRoot.path() + QStringLiteral("/ankle-case-005/evaluation/evaluation_metrics.csv"));
    QVERIFY(csvFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString csvContent = QString::fromUtf8(csvFile.readAll());
    QVERIFY(csvContent.contains(QStringLiteral("evaluation_confidence_score,0.8800")));
    QVERIFY(csvContent.contains(QStringLiteral("gate_reasons,\"collect_more_points; check_tracking_visibility\"")));
    QVERIFY(csvContent.contains(QStringLiteral("calibrated,true")));
    QVERIFY(csvContent.contains(QStringLiteral("calibration_accuracy_mm,0.4200")));
    QVERIFY(csvContent.contains(QStringLiteral("registration_mode,ankle_constrained_two_stage")));
    QVERIFY(csvContent.contains(QStringLiteral("target_region_radius_mm,18.5000")));
    QVERIFY(csvContent.contains(QStringLiteral("constraint_region_count,3")));
    QVERIFY(csvContent.contains(QStringLiteral("constraint_region_keys,\"distal_tibia|medial_malleolus|talar_dome\"")));
    QVERIFY(csvContent.contains(QStringLiteral("tracking_jitter_mm,0.3100")));
    QVERIFY(csvContent.contains(QStringLiteral("visible_frame_ratio,0.9700")));
    QVERIFY(csvContent.contains(QStringLiteral("tracking_profile,\"live_tracking\"")));
}

void NavigationEvaluationServiceTest::service_loads_evaluation_snapshot_with_metrics_evidence()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationEvaluationService service(tempRoot.path());

    AnkleRegistrationRecord registration;
    registration.caseId = QStringLiteral("ankle-case-021");
    registration.registrationMode = QStringLiteral("ankle_two_stage_constrained");
    registration.fre = 0.82;
    registration.targetTre = 1.36;
    registration.coverageScore = 0.91;
    registration.metrics.insert(QStringLiteral("target_region_radius_mm"), 18.5);
    registration.metrics.insert(QStringLiteral("constraint_region_count"), 3);
    registration.metrics.insert(QStringLiteral("constraint_region_keys"), QStringLiteral("distal_tibia|medial_malleolus|talar_dome"));
    registration.metrics.insert(QStringLiteral("constraint_region_bones"), QStringLiteral("tibia|tibia|talus"));
    registration.metrics.insert(QStringLiteral("constraint_region_roles"), QStringLiteral("implant_window|support|articular"));

    AnkleNavigationRunRecord run;
    run.caseId = registration.caseId;
    run.navigationMode = QStringLiteral("live_tracking");
    run.confidenceScore = 0.87;
    run.warnings = QStringList{
        QStringLiteral("collect_more_points"),
        QStringLiteral("check_tracking_visibility")
    };
    run.metrics.insert(QStringLiteral("tracking_jitter_mm"), 0.31);
    run.metrics.insert(QStringLiteral("visible_frame_ratio"), 0.97);
    run.metrics.insert(QStringLiteral("tracking_profile"), QStringLiteral("live_tracking"));
    run.metrics.insert(QStringLiteral("tracking_confidence_score"), 0.89);

    AnkleEvaluationReport report;
    report.caseId = registration.caseId;
    report.translationErrorMm = 1.08;
    report.rotationErrorDeg = 2.2;
    report.allowNavigation = true;
    report.confidenceScore = 0.87;
    report.gateReasons = run.warnings;
    report.calibrated = true;
    report.calibrationAccuracyMm = 0.42;
    report.metrics = run.metrics;
    report.metrics.insert(QStringLiteral("allow_navigation"), true);
    report.metrics.insert(QStringLiteral("gate_reason_count"), 2);

    QVERIFY(service.saveRegistrationRecord(registration));
    QVERIFY(service.saveNavigationRun(run));
    QVERIFY(service.saveEvaluationReport(report));

    const AnkleEvaluationSnapshot snapshot = service.loadEvaluationSnapshot(registration.caseId);

    QCOMPARE(snapshot.caseId, registration.caseId);
    QCOMPARE(snapshot.hasRegistration, true);
    QCOMPARE(snapshot.hasNavigationRun, true);
    QCOMPARE(snapshot.hasEvaluationReport, true);
    QCOMPARE(snapshot.registrationMode, QStringLiteral("ankle_two_stage_constrained"));
    QCOMPARE(snapshot.fre, 0.82);
    QCOMPARE(snapshot.targetTre, 1.36);
    QCOMPARE(snapshot.coverageScore, 0.91);
    QCOMPARE(snapshot.navigationMode, QStringLiteral("live_tracking"));
    QCOMPARE(snapshot.navigationConfidenceScore, 0.87);
    QCOMPARE(snapshot.allowNavigation, true);
    QCOMPARE(snapshot.evaluationConfidenceScore, 0.87);
    QCOMPARE(snapshot.calibrated, true);
    QCOMPARE(snapshot.calibrationAccuracyMm, 0.42);
    QCOMPARE(snapshot.registrationMetrics.value(QStringLiteral("constraint_region_count")).toInt(), 3);
    QCOMPARE(snapshot.registrationMetrics.value(QStringLiteral("target_region_radius_mm")).toDouble(), 18.5);
    QCOMPARE(snapshot.navigationMetrics.value(QStringLiteral("tracking_jitter_mm")).toDouble(), 0.31);
    QCOMPARE(snapshot.navigationMetrics.value(QStringLiteral("visible_frame_ratio")).toDouble(), 0.97);
    QCOMPARE(snapshot.navigationMetrics.value(QStringLiteral("tracking_confidence_score")).toDouble(), 0.89);
    QCOMPARE(snapshot.evaluationMetrics.value(QStringLiteral("gate_reason_count")).toInt(), 2);
    QCOMPARE(snapshot.gateReasons.size(), 2);
}

void NavigationEvaluationServiceTest::service_exports_case_summary_json_and_batch_summary_csv()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationEvaluationService service(tempRoot.path());

    AnkleRegistrationRecord registrationA;
    registrationA.caseId = QStringLiteral("ankle-case-101");
    registrationA.registrationMode = QStringLiteral("ankle_two_stage_constrained");
    registrationA.fre = 0.82;
    registrationA.targetTre = 1.36;
    registrationA.coverageScore = 0.91;
    registrationA.metrics.insert(QStringLiteral("target_region_radius_mm"), 18.5);
    registrationA.metrics.insert(QStringLiteral("constraint_region_count"), 3);

    AnkleNavigationRunRecord runA;
    runA.caseId = registrationA.caseId;
    runA.navigationMode = QStringLiteral("live_tracking");
    runA.confidenceScore = 0.87;
    runA.warnings = QStringList{
        QStringLiteral("collect_more_points"),
        QStringLiteral("check_tracking_visibility")
    };
    runA.metrics.insert(QStringLiteral("tracking_jitter_mm"), 0.31);
    runA.metrics.insert(QStringLiteral("visible_frame_ratio"), 0.97);
    runA.metrics.insert(QStringLiteral("tracking_profile"), QStringLiteral("live_tracking"));
    runA.metrics.insert(QStringLiteral("tracking_confidence_score"), 0.89);

    AnkleEvaluationReport reportA;
    reportA.caseId = registrationA.caseId;
    reportA.translationErrorMm = 1.08;
    reportA.rotationErrorDeg = 2.2;
    reportA.allowNavigation = true;
    reportA.confidenceScore = 0.87;
    reportA.gateReasons = runA.warnings;
    reportA.calibrated = true;
    reportA.calibrationAccuracyMm = 0.42;
    reportA.metrics = runA.metrics;
    reportA.metrics.insert(QStringLiteral("allow_navigation"), true);
    reportA.metrics.insert(QStringLiteral("gate_reason_count"), 2);

    AnkleRegistrationRecord registrationB;
    registrationB.caseId = QStringLiteral("ankle-case-102");
    registrationB.registrationMode = QStringLiteral("single_stage_landmark");
    registrationB.fre = 1.42;
    registrationB.targetTre = 2.81;
    registrationB.coverageScore = 0.63;
    registrationB.metrics.insert(QStringLiteral("target_region_radius_mm"), 15.0);
    registrationB.metrics.insert(QStringLiteral("constraint_region_count"), 1);

    AnkleNavigationRunRecord runB;
    runB.caseId = registrationB.caseId;
    runB.navigationMode = QStringLiteral("replay");
    runB.confidenceScore = 0.43;
    runB.warnings = QStringList{
        QStringLiteral("improve_tracking_quality")
    };
    runB.metrics.insert(QStringLiteral("tracking_jitter_mm"), 1.64);
    runB.metrics.insert(QStringLiteral("visible_frame_ratio"), 0.72);
    runB.metrics.insert(QStringLiteral("tracking_profile"), QStringLiteral("replay"));
    runB.metrics.insert(QStringLiteral("tracking_confidence_score"), 0.41);

    AnkleEvaluationReport reportB;
    reportB.caseId = registrationB.caseId;
    reportB.translationErrorMm = 2.84;
    reportB.rotationErrorDeg = 4.7;
    reportB.allowNavigation = false;
    reportB.confidenceScore = 0.43;
    reportB.gateReasons = runB.warnings;
    reportB.calibrated = false;
    reportB.calibrationAccuracyMm = 1.85;
    reportB.metrics = runB.metrics;
    reportB.metrics.insert(QStringLiteral("allow_navigation"), false);
    reportB.metrics.insert(QStringLiteral("gate_reason_count"), 1);

    QVERIFY(service.saveRegistrationRecord(registrationA));
    QVERIFY(service.saveNavigationRun(runA));
    QVERIFY(service.saveEvaluationReport(reportA));
    QVERIFY(service.saveRegistrationRecord(registrationB));
    QVERIFY(service.saveNavigationRun(runB));
    QVERIFY(service.saveEvaluationReport(reportB));

    QVERIFY(service.exportCaseSummary(registrationA.caseId));
    QVERIFY(service.exportCaseSummary(registrationB.caseId));
    QVERIFY(service.exportBatchSummaryCsv(QStringList{ registrationA.caseId, registrationB.caseId }));

    const QString summaryAPath =
        tempRoot.path() + QStringLiteral("/ankle-case-101/evaluation/case_evaluation_summary.json");
    const QString summaryCsvPath =
        tempRoot.path() + QStringLiteral("/summaries/evaluation_case_summaries.csv");
    QVERIFY(QFileInfo::exists(summaryAPath));
    QVERIFY(QFileInfo::exists(summaryCsvPath));

    QFile summaryAFile(summaryAPath);
    QVERIFY(summaryAFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonObject summaryAObject = QJsonDocument::fromJson(summaryAFile.readAll()).object();
    QCOMPARE(summaryAObject.value(QStringLiteral("case_id")).toString(), QStringLiteral("ankle-case-101"));
    QCOMPARE(summaryAObject.value(QStringLiteral("allow_navigation")).toBool(), true);
    QCOMPARE(summaryAObject.value(QStringLiteral("constraint_region_count")).toInt(), 3);
    QCOMPARE(summaryAObject.value(QStringLiteral("tracking_jitter_mm")).toDouble(), 0.31);
    QCOMPARE(summaryAObject.value(QStringLiteral("gate_reason_count")).toInt(), 2);

    QFile summaryCsvFile(summaryCsvPath);
    QVERIFY(summaryCsvFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString summaryCsvContent = QString::fromUtf8(summaryCsvFile.readAll());
    QVERIFY(summaryCsvContent.contains(QStringLiteral("case_id,registration_mode,navigation_mode,allow_navigation")));
    QVERIFY(summaryCsvContent.contains(QStringLiteral("ankle-case-101,ankle_two_stage_constrained,live_tracking,true")));
    QVERIFY(summaryCsvContent.contains(QStringLiteral("ankle-case-102,single_stage_landmark,replay,false")));
    QVERIFY(summaryCsvContent.contains(QStringLiteral(",0.8200,1.3600,0.9100,")));
    QVERIFY(summaryCsvContent.contains(QStringLiteral(",1.6400,0.7200,0.4100,1,false,1.8500")));
}

void NavigationEvaluationServiceTest::service_discovers_exportable_case_ids_from_cases_root()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    NavigationEvaluationService service(tempRoot.path());

    AnkleRegistrationRecord registration;
    registration.caseId = QStringLiteral("ankle-case-201");
    registration.registrationMode = QStringLiteral("ankle_two_stage_constrained");
    registration.fre = 0.82;
    registration.targetTre = 1.36;
    registration.coverageScore = 0.91;

    AnkleNavigationRunRecord run;
    run.caseId = registration.caseId;
    run.navigationMode = QStringLiteral("live_tracking");
    run.confidenceScore = 0.87;

    AnkleEvaluationReport report;
    report.caseId = registration.caseId;
    report.translationErrorMm = 1.08;
    report.rotationErrorDeg = 2.2;
    report.allowNavigation = true;
    report.confidenceScore = 0.87;
    report.calibrated = true;
    report.calibrationAccuracyMm = 0.42;

    QVERIFY(service.saveRegistrationRecord(registration));
    QVERIFY(service.saveNavigationRun(run));
    QVERIFY(service.saveEvaluationReport(report));

    QDir(tempRoot.path()).mkpath(QStringLiteral("ankle-case-empty"));
    QDir(tempRoot.path()).mkpath(QStringLiteral("summaries"));

    const QStringList caseIds = service.discoverExportableCaseIds();

    QCOMPARE(caseIds, QStringList{ QStringLiteral("ankle-case-201") });
}

QTEST_APPLESS_MAIN(NavigationEvaluationServiceTest)
#include "NavigationEvaluationServiceTest.moc"
