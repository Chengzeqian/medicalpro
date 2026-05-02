#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"
#include "Framework/Navigation/innovation_2_registration_experiment.h"

namespace
{
bool writeAsciiFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    file.write(content);
    return file.error() == QFile::NoError;
}
}

class RealCaseAssetBootstrapTest : public QObject
{
    Q_OBJECT

private slots:
    void real_case_workspace_with_tibia_talus_stl_is_consumable_by_innovation_2();
};

void RealCaseAssetBootstrapTest::real_case_workspace_with_tibia_talus_stl_is_consumable_by_innovation_2()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-real-45971129749");
    manifest.patientId = QStringLiteral("45971129749");
    manifest.patientName = QStringLiteral("Real Case 45971129749");
    manifest.surgeryId = QStringLiteral("ankle-navigation-real-45971129749");
    manifest.workflowStage = QStringLiteral("planning");
    QVERIFY(repository.createCaseWorkspace(manifest));

    const QString modelDir = repository.stagePath(manifest.caseId, QStringLiteral("models"));
    const QString tibiaModelPath = modelDir + QStringLiteral("/tibia.stl");
    const QString talusModelPath = modelDir + QStringLiteral("/talus.stl");

    const QByteArray tibiaStl =
        "solid tibia\n"
        "facet normal 0 0 1\n"
        "outer loop\n"
        "vertex 0 0 0\n"
        "vertex 10 0 0\n"
        "vertex 0 10 0\n"
        "endloop\n"
        "endfacet\n"
        "facet normal 0 0 1\n"
        "outer loop\n"
        "vertex 10 0 0\n"
        "vertex 10 10 0\n"
        "vertex 0 10 0\n"
        "endloop\n"
        "endfacet\n"
        "facet normal 0 0 -1\n"
        "outer loop\n"
        "vertex 0 0 30\n"
        "vertex 0 10 30\n"
        "vertex 10 0 30\n"
        "endloop\n"
        "endfacet\n"
        "facet normal 0 0 -1\n"
        "outer loop\n"
        "vertex 10 0 30\n"
        "vertex 0 10 30\n"
        "vertex 10 10 30\n"
        "endloop\n"
        "endfacet\n"
        "endsolid tibia\n";

    const QByteArray talusStl =
        "solid talus\n"
        "facet normal 0 0 1\n"
        "outer loop\n"
        "vertex 30 0 0\n"
        "vertex 40 0 0\n"
        "vertex 30 10 0\n"
        "endloop\n"
        "endfacet\n"
        "facet normal 0 0 1\n"
        "outer loop\n"
        "vertex 40 0 0\n"
        "vertex 40 10 0\n"
        "vertex 30 10 0\n"
        "endloop\n"
        "endfacet\n"
        "facet normal 0 0 -1\n"
        "outer loop\n"
        "vertex 30 0 10\n"
        "vertex 30 10 10\n"
        "vertex 40 0 10\n"
        "endloop\n"
        "endfacet\n"
        "facet normal 0 0 -1\n"
        "outer loop\n"
        "vertex 40 0 10\n"
        "vertex 30 10 10\n"
        "vertex 40 10 10\n"
        "endloop\n"
        "endfacet\n"
        "endsolid talus\n";

    QVERIFY(writeAsciiFile(tibiaModelPath, tibiaStl));
    QVERIFY(writeAsciiFile(talusModelPath, talusStl));

    manifest.modelAssets = {
        AnkleModelAsset {
            QStringLiteral("tibia"),
            tibiaModelPath,
            QStringLiteral("models/tibia.stl"),
            QStringLiteral("stl")
        },
        AnkleModelAsset {
            QStringLiteral("talus"),
            talusModelPath,
            QStringLiteral("models/talus.stl"),
            QStringLiteral("stl")
        }
    };
    QVERIFY(repository.saveManifest(manifest));

    AnklePlanningService planningService(repository);
    AnklePlanningData planning = planningService.createDefaultPlanning(manifest.caseId);
    planning.primaryBones = QStringList { QStringLiteral("tibia"), QStringLiteral("talus") };
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(5.0f, 5.0f, 15.0f));
    planning.referenceLandmarks.insert(QStringLiteral("talus_center"), QVector3D(35.0f, 5.0f, 5.0f));
    planning.targetRegionCenter = QVector3D(35.0f, 5.0f, 5.0f);
    planning.targetRegionRadiusMm = 15.0;
    planning.anatomicalConstraintRegions.insert(
        QStringLiteral("tibia_distal_region"),
        QList<QVector3D> {
            QVector3D(1.0f, 1.0f, 25.0f),
            QVector3D(2.0f, 1.0f, 26.0f),
            QVector3D(1.5f, 2.0f, 24.5f)
        });
    planning.anatomicalConstraintRegions.insert(
        QStringLiteral("talus_dome_region"),
        QList<QVector3D> {
            QVector3D(31.0f, 2.0f, 8.0f),
            QVector3D(35.0f, 4.0f, 9.0f),
            QVector3D(38.0f, 7.0f, 7.5f)
        });
    QVERIFY(planningService.savePlanning(manifest.caseId, planning));

    Innovation2RegistrationExperiment experiment;
    Innovation2RegistrationInput input;
    input.caseId = manifest.caseId;
    input.caseDataRoot = tempRoot.path();
    input.registrationMethodIds = QStringList { QStringLiteral("ankle_two_stage_constrained") };

    const QList<InnovationExperimentRecord> records = experiment.run(input);
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().metrics.value(QStringLiteral("used_case_model_assets")).toBool(), true);
    QCOMPARE(records.first().metrics.value(QStringLiteral("used_case_planning")).toBool(), true);
    QCOMPARE(records.first().metrics.value(QStringLiteral("used_planned_constraint_regions")).toBool(), true);
    QCOMPARE(records.first().metrics.value(QStringLiteral("case_loaded_bones")).toString(), QStringLiteral("tibia|talus"));
}

QTEST_APPLESS_MAIN(RealCaseAssetBootstrapTest)
#include "RealCaseAssetBootstrapTest.moc"
