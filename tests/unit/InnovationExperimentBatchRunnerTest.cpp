#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"
#include "Framework/Navigation/innovation_experiment_batch_runner.h"
#include "Framework/Navigation/innovation_2_registration_experiment.h"
#include "Framework/Navigation/navigation_evaluation_service.h"

namespace
{
class ScopedCurrentDir
{
public:
    explicit ScopedCurrentDir(const QString& path)
        : m_previous(QDir::currentPath())
    {
        QDir::setCurrent(path);
    }

    ~ScopedCurrentDir()
    {
        QDir::setCurrent(m_previous);
    }

private:
    QString m_previous;
};

bool writeTextFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    file.write(content);
    return file.error() == QFile::NoError;
}

QMap<QString, QString> summaryRowForMethod(
    const QString& summaryCsv,
    const QString& methodId)
{
    const QStringList rows = summaryCsv.split('\n', Qt::SkipEmptyParts);
    if (rows.size() < 2) {
        return {};
    }

    const QStringList headers = rows.first().split(',');
    for (int rowIndex = 1; rowIndex < rows.size(); ++rowIndex) {
        const QStringList columns = rows[rowIndex].split(',');
        if (columns.size() != headers.size() || columns.value(2) != methodId) {
            continue;
        }

        QMap<QString, QString> row;
        for (int columnIndex = 0; columnIndex < headers.size(); ++columnIndex) {
            row.insert(headers[columnIndex], columns[columnIndex]);
        }
        return row;
    }

    return {};
}

double targetTreForMethodFromSummary(
    const QString& summaryCsv,
    const QString& methodId)
{
    return summaryRowForMethod(summaryCsv, methodId).value(QStringLiteral("target_tre_mm"), QStringLiteral("-1")).toDouble();
}
}

class InnovationExperimentBatchRunnerTest : public QObject
{
    Q_OBJECT

private slots:
    void batch_runner_exports_three_summary_csv_files_for_a_case_set();
    void batch_runner_passes_case_planning_context_to_innovation_2_summary();
    void batch_runner_exports_innovation_2_anatomical_constraint_metrics();
    void batch_runner_exports_parallel_search_columns_for_innovation_2_summary();
    void batch_runner_exports_innovation_3_digital_twin_columns_from_case_metrics();
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

void InnovationExperimentBatchRunnerTest::batch_runner_passes_case_planning_context_to_innovation_2_summary()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());
    ScopedCurrentDir scopedCurrentDir(tempRoot.path());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-501");
    manifest.patientId = QStringLiteral("patient-501");
    manifest.patientName = QStringLiteral("Patient E");
    manifest.surgeryId = QStringLiteral("surgery-501");
    manifest.workflowStage = QStringLiteral("planning");
    QVERIFY(repository.createCaseWorkspace(manifest));

    AnklePlanningService planningService(repository);
    AnklePlanningData planning = planningService.createDefaultPlanning(manifest.caseId);
    planning.targetRegionCenter = QVector3D(9.0f, 17.5f, -3.0f);
    planning.targetRegionRadiusMm = 18.0;
    QVERIFY(planningService.savePlanning(manifest.caseId, planning));

    Innovation2RegistrationExperiment directExperiment;
    Innovation2RegistrationInput directInput;
    directInput.caseId = manifest.caseId;
    directInput.caseDataRoot = tempRoot.path();
    directInput.registrationMethodIds = QStringList({
        QStringLiteral("ankle_two_stage_constrained")
    });
    const auto directRecords = directExperiment.run(directInput);
    QCOMPARE(directRecords.size(), 1);

    InnovationExperimentBatchRunner runner;
    InnovationBatchInput batchInput;
    batchInput.caseIds = QStringList({ manifest.caseId });
    batchInput.caseDataRoot = tempRoot.path();

    const InnovationBatchOutput output = runner.run(batchInput);

    QVERIFY(output.summaryFiles.contains(QStringLiteral("innovation_2_summary.csv")));

    QFile innovation2Summary(QDir::currentPath() + QStringLiteral("/summaries/innovation_2_summary.csv"));
    QVERIFY(innovation2Summary.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString innovation2Csv = QString::fromUtf8(innovation2Summary.readAll());

    const double summaryTargetTre =
        targetTreForMethodFromSummary(innovation2Csv, QStringLiteral("ankle_two_stage_constrained"));
    QVERIFY(summaryTargetTre >= 0.0);

    const double expectedTargetTre =
        directRecords.first().metrics.value(QStringLiteral("target_tre_mm")).toDouble();
    QVERIFY(qAbs(summaryTargetTre - expectedTargetTre) < 0.0001);
}

void InnovationExperimentBatchRunnerTest::batch_runner_exports_innovation_2_anatomical_constraint_metrics()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());
    ScopedCurrentDir scopedCurrentDir(tempRoot.path());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-502");
    manifest.patientId = QStringLiteral("patient-502");
    manifest.patientName = QStringLiteral("Patient F");
    manifest.surgeryId = QStringLiteral("surgery-502");
    manifest.workflowStage = QStringLiteral("planning");
    QVERIFY(repository.createCaseWorkspace(manifest));

    const QString modelDir = repository.stagePath(manifest.caseId, QStringLiteral("models"));
    const QString tibiaModelPath = modelDir + QStringLiteral("/tibia.obj");
    const QString talusModelPath = modelDir + QStringLiteral("/talus.obj");

    const QByteArray tibiaObj =
        "v 0 0 0\n"
        "v 0 0 20\n"
        "v 10 0 0\n"
        "v 10 0 20\n"
        "v 0 10 0\n"
        "v 0 10 20\n"
        "v 10 10 0\n"
        "v 10 10 20\n"
        "f 1 3 5\n"
        "f 3 7 5\n"
        "f 2 4 6\n"
        "f 4 8 6\n";

    const QByteArray talusObj =
        "v 30 0 0\n"
        "v 30 0 10\n"
        "v 40 0 0\n"
        "v 40 0 10\n"
        "v 30 10 0\n"
        "v 30 10 10\n"
        "v 40 10 0\n"
        "v 40 10 10\n"
        "f 1 3 5\n"
        "f 3 7 5\n"
        "f 2 4 6\n"
        "f 4 8 6\n";

    QVERIFY(writeTextFile(tibiaModelPath, tibiaObj));
    QVERIFY(writeTextFile(talusModelPath, talusObj));

    manifest.modelAssets = {
        AnkleModelAsset {
            QStringLiteral("tibia"),
            tibiaModelPath,
            QStringLiteral("models/tibia.obj"),
            QStringLiteral("obj")
        },
        AnkleModelAsset {
            QStringLiteral("talus"),
            talusModelPath,
            QStringLiteral("models/talus.obj"),
            QStringLiteral("obj")
        }
    };
    QVERIFY(repository.saveManifest(manifest));

    AnklePlanningService planningService(repository);
    AnklePlanningData planning = planningService.createDefaultPlanning(manifest.caseId);
    planning.primaryBones = QStringList { QStringLiteral("tibia"), QStringLiteral("talus") };
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(5.0f, 5.0f, 10.0f));
    planning.referenceLandmarks.insert(QStringLiteral("talus_center"), QVector3D(35.0f, 5.0f, 5.0f));
    planning.targetRegionCenter = QVector3D(35.0f, 5.0f, 5.0f);
    planning.targetRegionRadiusMm = 15.0;
    planning.anatomicalConstraintRegions.insert(
        QStringLiteral("tibia_distal_region"),
        QList<QVector3D> {
            QVector3D(100.0f, 100.0f, 100.0f),
            QVector3D(101.0f, 100.0f, 100.0f),
            QVector3D(100.0f, 101.0f, 100.0f)
        });
    planning.anatomicalConstraintRegions.insert(
        QStringLiteral("talus_dome_region"),
        QList<QVector3D> {
            QVector3D(200.0f, 200.0f, 200.0f),
            QVector3D(201.0f, 200.0f, 200.0f),
            QVector3D(200.0f, 201.0f, 200.0f)
        });
    QVERIFY(planningService.savePlanning(manifest.caseId, planning));

    InnovationExperimentBatchRunner runner;
    InnovationBatchInput batchInput;
    batchInput.caseIds = QStringList({ manifest.caseId });
    batchInput.caseDataRoot = tempRoot.path();

    const InnovationBatchOutput output = runner.run(batchInput);

    QVERIFY(output.summaryFiles.contains(QStringLiteral("innovation_2_summary.csv")));

    QFile innovation2Summary(QDir::currentPath() + QStringLiteral("/summaries/innovation_2_summary.csv"));
    QVERIFY(innovation2Summary.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString innovation2Csv = QString::fromUtf8(innovation2Summary.readAll());

    const QMap<QString, QString> summaryRow = summaryRowForMethod(
        innovation2Csv,
        QStringLiteral("ankle_two_stage_constrained"));
    QVERIFY(!summaryRow.isEmpty());
    QCOMPARE(summaryRow.value(QStringLiteral("used_planned_constraint_regions")), QStringLiteral("true"));
    QCOMPARE(summaryRow.value(QStringLiteral("used_anatomical_regions")), QStringLiteral("true"));
    QCOMPARE(summaryRow.value(QStringLiteral("tibia_distal_point_count")), QStringLiteral("3"));
    QCOMPARE(summaryRow.value(QStringLiteral("talus_dome_point_count")), QStringLiteral("3"));
    QCOMPARE(summaryRow.value(QStringLiteral("anatomical_region_point_count")), QStringLiteral("6"));
    QCOMPARE(summaryRow.value(QStringLiteral("case_loaded_bones")), QStringLiteral("tibia|talus"));
}

void InnovationExperimentBatchRunnerTest::batch_runner_exports_parallel_search_columns_for_innovation_2_summary()
{
    InnovationExperimentBatchRunner runner;
    InnovationBatchInput input;
    input.caseIds = QStringList({ QStringLiteral("ankle-case-601") });

    const InnovationBatchOutput output = runner.run(input);

    QVERIFY(output.summaryFiles.contains(QStringLiteral("innovation_2_summary.csv")));

    QFile innovation2Summary(QDir::currentPath() + QStringLiteral("/summaries/innovation_2_summary.csv"));
    QVERIFY(innovation2Summary.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString innovation2Csv = QString::fromUtf8(innovation2Summary.readAll());

    const QMap<QString, QString> summaryRow = summaryRowForMethod(
        innovation2Csv,
        QStringLiteral("ankle_two_stage_constrained"));
    QVERIFY(!summaryRow.isEmpty());
    QVERIFY(summaryRow.contains(QStringLiteral("candidate_count")));
    QVERIFY(summaryRow.contains(QStringLiteral("top_k_count")));
    QVERIFY(summaryRow.contains(QStringLiteral("coarse_search_ms")));
    QVERIFY(summaryRow.contains(QStringLiteral("best_candidate_rank")));
    QVERIFY(summaryRow.contains(QStringLiteral("parallel_search_enabled")));
    QVERIFY(summaryRow.contains(QStringLiteral("multi_resolution_profile")));
}

void InnovationExperimentBatchRunnerTest::batch_runner_exports_innovation_3_digital_twin_columns_from_case_metrics()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());
    ScopedCurrentDir scopedCurrentDir(tempRoot.path());

    NavigationEvaluationService evaluationService(tempRoot.path() + QStringLiteral("/cases"));
    AnkleEvaluationReport report;
    report.caseId = QStringLiteral("ankle-case-705");
    report.allowNavigation = true;
    report.confidenceScore = 0.83;
    report.gateReasons = QStringList {
        QStringLiteral("review_target_alignment")
    };
    report.calibrated = true;
    report.calibrationAccuracyMm = 0.31;
    report.metrics.insert(QStringLiteral("twin_confidence_score"), 0.44);
    report.metrics.insert(QStringLiteral("local_risk_score"), 0.73);
    report.metrics.insert(QStringLiteral("target_region_distance_mm"), 5.20);
    report.metrics.insert(QStringLiteral("target_region_angle_error_deg"), 7.80);
    report.metrics.insert(QStringLiteral("dominant_risk_source"), QStringLiteral("tracking"));
    report.metrics.insert(QStringLiteral("re_register_recommended"), true);
    report.metrics.insert(QStringLiteral("tracking_degradation_detected"), true);
    QVERIFY(evaluationService.saveEvaluationReport(report));

    InnovationExperimentBatchRunner runner;
    InnovationBatchInput input;
    input.caseIds = QStringList({ report.caseId });
    input.caseDataRoot = tempRoot.path();

    const InnovationBatchOutput output = runner.run(input);

    QVERIFY(output.summaryFiles.contains(QStringLiteral("innovation_3_summary.csv")));

    QFile innovation3Summary(QDir::currentPath() + QStringLiteral("/summaries/innovation_3_summary.csv"));
    QVERIFY(innovation3Summary.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString innovation3Csv = QString::fromUtf8(innovation3Summary.readAll());

    QVERIFY(innovation3Csv.contains(QStringLiteral("twin_confidence_score")));
    QVERIFY(innovation3Csv.contains(QStringLiteral("local_risk_score")));
    QVERIFY(innovation3Csv.contains(QStringLiteral("target_region_distance_mm")));
    QVERIFY(innovation3Csv.contains(QStringLiteral("dominant_risk_source")));
    QVERIFY(innovation3Csv.contains(QStringLiteral("re_register_recommended")));
    QVERIFY(innovation3Csv.contains(QStringLiteral("tracking_degradation_detected")));

    const QMap<QString, QString> summaryRow = summaryRowForMethod(
        innovation3Csv,
        QStringLiteral("joint_confidence"));
    QVERIFY(!summaryRow.isEmpty());
    QCOMPARE(summaryRow.value(QStringLiteral("twin_confidence_score")), QStringLiteral("0.4400"));
    QCOMPARE(summaryRow.value(QStringLiteral("local_risk_score")), QStringLiteral("0.7300"));
    QCOMPARE(summaryRow.value(QStringLiteral("target_region_distance_mm")), QStringLiteral("5.2000"));
    QCOMPARE(summaryRow.value(QStringLiteral("dominant_risk_source")), QStringLiteral("tracking"));
    QCOMPARE(summaryRow.value(QStringLiteral("re_register_recommended")), QStringLiteral("true"));
    QCOMPARE(summaryRow.value(QStringLiteral("tracking_degradation_detected")), QStringLiteral("true"));
}

QTEST_APPLESS_MAIN(InnovationExperimentBatchRunnerTest)
#include "InnovationExperimentBatchRunnerTest.moc"
