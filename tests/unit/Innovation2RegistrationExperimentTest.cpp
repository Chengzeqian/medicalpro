#include <QtTest/QtTest>

#include <QMap>
#include <QFile>
#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"
#include "Framework/Navigation/innovation_2_registration_experiment.h"

class Innovation2RegistrationExperimentTest : public QObject
{
    Q_OBJECT

private slots:
    void experiment_runs_four_registration_methods_and_exports_core_metrics();
    void experiment_produces_method_distinct_non_placeholder_metrics();
    void experiment_uses_case_planning_target_region_when_workspace_is_provided();
    void experiment_uses_case_model_assets_when_tibia_and_talus_models_are_available();
    void experiment_reports_tibia_and_talus_anatomical_regions_when_case_models_are_available();
    void experiment_prefers_explicit_planned_constraint_regions_over_heuristic_regions();
};

namespace
{
bool writeTextFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    file.write(content);
    return file.error() == QFile::NoError;
}
}

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

void Innovation2RegistrationExperimentTest::experiment_produces_method_distinct_non_placeholder_metrics()
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

    QMap<QString, InnovationExperimentRecord> recordByMethod;
    for (const InnovationExperimentRecord& record : records) {
        recordByMethod.insert(record.strategyId, record);
        QVERIFY(record.metrics.value(QStringLiteral("fre_mm")).toDouble() > 0.0);
        QVERIFY(record.metrics.value(QStringLiteral("overall_tre_mm")).toDouble() > 0.0);
        QVERIFY(record.metrics.value(QStringLiteral("target_tre_mm")).toDouble() > 0.0);
        QVERIFY(record.metrics.value(QStringLiteral("runtime_ms")).toDouble() > 0.0);
    }

    const double singleStageTargetTre =
        recordByMethod.value(QStringLiteral("single_stage_landmark")).metrics.value(QStringLiteral("target_tre_mm")).toDouble();
    const double globalIcpTargetTre =
        recordByMethod.value(QStringLiteral("landmark_plus_global_icp")).metrics.value(QStringLiteral("target_tre_mm")).toDouble();
    const double globalGicpTargetTre =
        recordByMethod.value(QStringLiteral("landmark_plus_global_gicp")).metrics.value(QStringLiteral("target_tre_mm")).toDouble();
    const double constrainedTargetTre =
        recordByMethod.value(QStringLiteral("ankle_two_stage_constrained")).metrics.value(QStringLiteral("target_tre_mm")).toDouble();

    QVERIFY(globalIcpTargetTre < singleStageTargetTre);
    QVERIFY(globalGicpTargetTre < globalIcpTargetTre);
    QVERIFY(constrainedTargetTre < globalGicpTargetTre);

    const double singleStageOverallTre =
        recordByMethod.value(QStringLiteral("single_stage_landmark")).metrics.value(QStringLiteral("overall_tre_mm")).toDouble();
    const double constrainedOverallTre =
        recordByMethod.value(QStringLiteral("ankle_two_stage_constrained")).metrics.value(QStringLiteral("overall_tre_mm")).toDouble();

    QVERIFY(constrainedOverallTre < singleStageOverallTre);
}

void Innovation2RegistrationExperimentTest::experiment_uses_case_planning_target_region_when_workspace_is_provided()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-401");
    manifest.patientId = QStringLiteral("patient-401");
    manifest.patientName = QStringLiteral("Patient D");
    manifest.surgeryId = QStringLiteral("surgery-401");
    manifest.workflowStage = QStringLiteral("planning");
    QVERIFY(repository.createCaseWorkspace(manifest));

    AnklePlanningService planningService(repository);
    AnklePlanningData planning = planningService.createDefaultPlanning(manifest.caseId);
    planning.primaryBones = QStringList { QStringLiteral("tibia"), QStringLiteral("talus") };
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(0.0f, 0.0f, 0.0f));
    planning.targetRegionCenter = QVector3D(9.0f, 17.5f, -3.0f);
    planning.targetRegionRadiusMm = 18.0;
    QVERIFY(planningService.savePlanning(manifest.caseId, planning));

    Innovation2RegistrationExperiment experiment;

    Innovation2RegistrationInput input;
    input.caseId = manifest.caseId;
    input.caseDataRoot = tempRoot.path();
    input.registrationMethodIds = QStringList({
        QStringLiteral("ankle_two_stage_constrained")
    });

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().metrics.value(QStringLiteral("used_case_planning")).toBool(), true);
    QCOMPARE(records.first().metrics.value(QStringLiteral("roi_radius_mm")).toDouble(), 18.0);
}

void Innovation2RegistrationExperimentTest::experiment_uses_case_model_assets_when_tibia_and_talus_models_are_available()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-402");
    manifest.patientId = QStringLiteral("patient-402");
    manifest.patientName = QStringLiteral("Patient E");
    manifest.surgeryId = QStringLiteral("surgery-402");
    manifest.workflowStage = QStringLiteral("planning");
    QVERIFY(repository.createCaseWorkspace(manifest));

    const QString modelDir = repository.stagePath(manifest.caseId, QStringLiteral("models"));
    const QString tibiaModelPath = modelDir + QStringLiteral("/tibia.obj");
    const QString talusModelPath = modelDir + QStringLiteral("/talus.obj");

    const QByteArray tibiaObj =
        "v 0 0 0\n"
        "v 20 0 0\n"
        "v 0 20 0\n"
        "v 0 0 20\n"
        "f 1 2 3\n"
        "f 1 2 4\n"
        "f 1 3 4\n"
        "f 2 3 4\n";

    const QByteArray talusObj =
        "v 30 10 0\n"
        "v 42 12 0\n"
        "v 32 24 0\n"
        "v 34 14 12\n"
        "f 1 2 3\n"
        "f 1 2 4\n"
        "f 1 3 4\n"
        "f 2 3 4\n";

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
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(6.0f, 6.0f, 4.0f));
    planning.referenceLandmarks.insert(QStringLiteral("talus_center"), QVector3D(35.0f, 15.0f, 4.0f));
    planning.targetRegionCenter = QVector3D(34.0f, 15.0f, 3.0f);
    planning.targetRegionRadiusMm = 30.0;
    QVERIFY(planningService.savePlanning(manifest.caseId, planning));

    Innovation2RegistrationExperiment experiment;

    Innovation2RegistrationInput input;
    input.caseId = manifest.caseId;
    input.caseDataRoot = tempRoot.path();
    input.registrationMethodIds = QStringList({
        QStringLiteral("ankle_two_stage_constrained")
    });

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().metrics.value(QStringLiteral("used_case_planning")).toBool(), true);
    QCOMPARE(records.first().metrics.value(QStringLiteral("used_case_model_assets")).toBool(), true);
    QCOMPARE(records.first().metrics.value(QStringLiteral("case_model_asset_count")).toInt(), 2);
    QVERIFY(records.first().metrics.value(QStringLiteral("roi_point_count")).toInt() >= 3);
    QVERIFY(records.first().metrics.value(QStringLiteral("fre_mm")).toDouble() > 0.0);
    QVERIFY(records.first().metrics.value(QStringLiteral("target_tre_mm")).toDouble() > 0.0);
}

void Innovation2RegistrationExperimentTest::experiment_reports_tibia_and_talus_anatomical_regions_when_case_models_are_available()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-403");
    manifest.patientId = QStringLiteral("patient-403");
    manifest.patientName = QStringLiteral("Patient F");
    manifest.surgeryId = QStringLiteral("surgery-403");
    manifest.workflowStage = QStringLiteral("planning");
    QVERIFY(repository.createCaseWorkspace(manifest));

    const QString modelDir = repository.stagePath(manifest.caseId, QStringLiteral("models"));
    const QString tibiaModelPath = modelDir + QStringLiteral("/tibia.obj");
    const QString talusModelPath = modelDir + QStringLiteral("/talus.obj");

    const QByteArray tibiaObj =
        "v 0 0 0\n"
        "v 0 0 14\n"
        "v 6 0 2\n"
        "v 6 0 12\n"
        "v 12 0 4\n"
        "v 12 0 10\n"
        "v 18 0 6\n"
        "v 18 0 8\n"
        "v 0 8 0\n"
        "v 0 8 14\n"
        "v 6 8 2\n"
        "v 6 8 12\n"
        "v 12 8 4\n"
        "v 12 8 10\n"
        "v 18 8 6\n"
        "v 18 8 8\n"
        "f 1 3 9\n"
        "f 3 11 9\n"
        "f 3 5 11\n"
        "f 5 13 11\n"
        "f 5 7 13\n"
        "f 7 15 13\n";

    const QByteArray talusObj =
        "v 24 4 0\n"
        "v 28 4 2\n"
        "v 32 4 4\n"
        "v 36 4 6\n"
        "v 40 4 4\n"
        "v 44 4 2\n"
        "v 48 4 0\n"
        "v 24 12 0\n"
        "v 28 12 3\n"
        "v 32 12 6\n"
        "v 36 12 8\n"
        "v 40 12 6\n"
        "v 44 12 3\n"
        "v 48 12 0\n"
        "f 1 2 8\n"
        "f 2 9 8\n"
        "f 2 3 9\n"
        "f 3 10 9\n"
        "f 3 4 10\n"
        "f 4 11 10\n"
        "f 4 5 11\n"
        "f 5 12 11\n";

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
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(8.0f, 4.0f, 7.0f));
    planning.referenceLandmarks.insert(QStringLiteral("talus_center"), QVector3D(36.0f, 8.0f, 4.0f));
    planning.targetRegionCenter = QVector3D(36.0f, 8.0f, 5.0f);
    planning.targetRegionRadiusMm = 20.0;
    QVERIFY(planningService.savePlanning(manifest.caseId, planning));

    Innovation2RegistrationExperiment experiment;

    Innovation2RegistrationInput input;
    input.caseId = manifest.caseId;
    input.caseDataRoot = tempRoot.path();
    input.registrationMethodIds = QStringList({
        QStringLiteral("ankle_two_stage_constrained")
    });

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().metrics.value(QStringLiteral("used_case_model_assets")).toBool(), true);
    QVERIFY(records.first().metrics.value(QStringLiteral("tibia_distal_point_count")).toInt() >= 3);
    QVERIFY(records.first().metrics.value(QStringLiteral("talus_dome_point_count")).toInt() >= 3);
    QVERIFY(records.first().metrics.value(QStringLiteral("anatomical_region_point_count")).toInt() >= 6);
    QCOMPARE(records.first().metrics.value(QStringLiteral("used_anatomical_regions")).toBool(), true);
}

void Innovation2RegistrationExperimentTest::experiment_prefers_explicit_planned_constraint_regions_over_heuristic_regions()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-404");
    manifest.patientId = QStringLiteral("patient-404");
    manifest.patientName = QStringLiteral("Patient G");
    manifest.surgeryId = QStringLiteral("surgery-404");
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

    Innovation2RegistrationExperiment experiment;

    Innovation2RegistrationInput input;
    input.caseId = manifest.caseId;
    input.caseDataRoot = tempRoot.path();
    input.registrationMethodIds = QStringList({
        QStringLiteral("ankle_two_stage_constrained")
    });

    const auto records = experiment.run(input);

    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().metrics.value(QStringLiteral("used_case_model_assets")).toBool(), true);
    QCOMPARE(records.first().metrics.value(QStringLiteral("used_planned_constraint_regions")).toBool(), true);
    QCOMPARE(records.first().metrics.value(QStringLiteral("tibia_distal_point_count")).toInt(), 3);
    QCOMPARE(records.first().metrics.value(QStringLiteral("talus_dome_point_count")).toInt(), 3);
    QCOMPARE(records.first().metrics.value(QStringLiteral("anatomical_region_point_count")).toInt(), 6);
}

QTEST_APPLESS_MAIN(Innovation2RegistrationExperimentTest)
#include "Innovation2RegistrationExperimentTest.moc"
