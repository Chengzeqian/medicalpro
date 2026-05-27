#include <QtTest/QtTest>

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"

class AnkleCaseWorkspaceRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void create_case_workspace_writes_manifest_and_stage_directories();
    void real_case_workspace_persists_tibia_talus_model_assets_and_planning_bones();
    void case_workspace_persists_case_asset_bindings_for_multiple_bones_and_instruments();
    void case_workspace_manifest_persists_primary_instrument_assets();
};

void AnkleCaseWorkspaceRepositoryTest::create_case_workspace_writes_manifest_and_stage_directories()
{
    QTemporaryDir tempRoot;
    QVERIFY2(tempRoot.isValid(), "temporary root must exist");

    AnkleCaseWorkspaceRepository repo(tempRoot.path());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-001");
    manifest.patientId = QStringLiteral("patient-001");
    manifest.patientName = QStringLiteral("Patient A");
    manifest.surgeryId = QStringLiteral("surgery-001");
    manifest.workflowStage = QStringLiteral("preparation");

    QVERIFY(repo.createCaseWorkspace(manifest));

    const QString caseRoot = tempRoot.path() + QStringLiteral("/cases/ankle-case-001");
    QVERIFY(QDir(caseRoot + QStringLiteral("/dicom")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/segmentation")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/models")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/planning")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/registration")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/navigation")).exists());
    QVERIFY(QDir(caseRoot + QStringLiteral("/evaluation")).exists());

    const AnkleCaseManifest loaded = repo.loadManifest(QStringLiteral("ankle-case-001"));
    QCOMPARE(loaded.caseId, QStringLiteral("ankle-case-001"));
    QCOMPARE(loaded.patientName, QStringLiteral("Patient A"));
    QCOMPARE(loaded.workflowStage, QStringLiteral("preparation"));
}

void AnkleCaseWorkspaceRepositoryTest::real_case_workspace_persists_tibia_talus_model_assets_and_planning_bones()
{
    QTemporaryDir tempRoot;
    QVERIFY2(tempRoot.isValid(), "temporary root must exist");

    AnkleCaseWorkspaceRepository repo(tempRoot.path());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-real-001");
    manifest.patientId = QStringLiteral("45971129749");
    manifest.patientName = QStringLiteral("Real Case 45971129749");
    manifest.surgeryId = QStringLiteral("ankle-navigation-real-001");
    manifest.workflowStage = QStringLiteral("planning");
    QVERIFY(repo.createCaseWorkspace(manifest));

    const QString modelDir = repo.stagePath(manifest.caseId, QStringLiteral("models"));
    const QString tibiaModelPath = modelDir + QStringLiteral("/tibia.stl");
    const QString talusModelPath = modelDir + QStringLiteral("/talus.stl");
    QVERIFY(QFile(tibiaModelPath).open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(QFile(talusModelPath).open(QIODevice::WriteOnly | QIODevice::Truncate));

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
    QVERIFY(repo.saveManifest(manifest));

    AnklePlanningService planningService(repo);
    AnklePlanningData planning = planningService.createDefaultPlanning(manifest.caseId);
    planning.primaryBones = QStringList { QStringLiteral("tibia"), QStringLiteral("talus") };
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(5.0f, 5.0f, 10.0f));
    planning.referenceLandmarks.insert(QStringLiteral("talus_center"), QVector3D(35.0f, 5.0f, 5.0f));
    planning.targetRegionCenter = QVector3D(35.0f, 5.0f, 5.0f);
    planning.targetRegionRadiusMm = 15.0;
    QVERIFY(planningService.savePlanning(manifest.caseId, planning));

    const AnkleCaseManifest loadedManifest = repo.loadManifest(manifest.caseId);
    QCOMPARE(loadedManifest.modelAssets.size(), 2);
    QCOMPARE(loadedManifest.modelAssets.first().boneName, QStringLiteral("tibia"));
    QCOMPARE(loadedManifest.modelAssets.first().normalizedPath, QStringLiteral("models/tibia.stl"));
    QCOMPARE(loadedManifest.modelAssets.last().boneName, QStringLiteral("talus"));
    QCOMPARE(loadedManifest.modelAssets.last().normalizedPath, QStringLiteral("models/talus.stl"));

    const AnklePlanningData loadedPlanning = planningService.loadPlanning(manifest.caseId);
    QCOMPARE(loadedPlanning.primaryBones, QStringList({ QStringLiteral("tibia"), QStringLiteral("talus") }));
    QCOMPARE(loadedPlanning.targetRegionCenter, QVector3D(35.0f, 5.0f, 5.0f));
    QCOMPARE(loadedPlanning.targetRegionRadiusMm, 15.0);

    QFile manifestFile(repo.manifestPath(manifest.caseId));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonObject manifestObject = QJsonDocument::fromJson(manifestFile.readAll()).object();
    const QJsonArray modelAssets = manifestObject.value(QStringLiteral("model_assets")).toArray();
    QCOMPARE(modelAssets.size(), 2);
    QCOMPARE(modelAssets.at(0).toObject().value(QStringLiteral("bone_name")).toString(), QStringLiteral("tibia"));
    QCOMPARE(modelAssets.at(1).toObject().value(QStringLiteral("bone_name")).toString(), QStringLiteral("talus"));
}

void AnkleCaseWorkspaceRepositoryTest::case_workspace_persists_case_asset_bindings_for_multiple_bones_and_instruments()
{
    QTemporaryDir tempRoot;
    QVERIFY2(tempRoot.isValid(), "temporary root must exist");

    AnkleCaseWorkspaceRepository repo(tempRoot.path());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-bindings-001");
    manifest.patientId = QStringLiteral("patient-001");
    manifest.patientName = QStringLiteral("Patient 001");
    QVERIFY(repo.createCaseWorkspace(manifest));

    AnkleCaseAssetBindings bindings;
    bindings.caseId = manifest.caseId;
    bindings.boundBoneAssetIds = QStringList {
        QStringLiteral("bone-tibia"),
        QStringLiteral("bone-talus")
    };
    bindings.activeBoneAssetIds = bindings.boundBoneAssetIds;
    bindings.boundInstrumentAssetIds = QStringList {
        QStringLiteral("probe-main"),
        QStringLiteral("tool-guide")
    };
    bindings.instrumentGeometryBindings = {
        AnkleInstrumentGeometryBinding {
            QStringLiteral("probe-main"),
            QStringLiteral("geometry-probe"),
            QStringLiteral("geometry/probe.ini")
        },
        AnkleInstrumentGeometryBinding {
            QStringLiteral("tool-guide"),
            QStringLiteral("geometry-guide"),
            QStringLiteral("geometry/guide.ini")
        }
    };

    QVERIFY(repo.saveCaseAssetBindings(bindings));

    const AnkleCaseAssetBindings restored = repo.loadCaseAssetBindings(manifest.caseId);
    QCOMPARE(restored.boundBoneAssetIds, bindings.boundBoneAssetIds);
    QCOMPARE(restored.boundInstrumentAssetIds, bindings.boundInstrumentAssetIds);
    QCOMPARE(restored.instrumentGeometryBindings.size(), 2);
}

void AnkleCaseWorkspaceRepositoryTest::case_workspace_manifest_persists_primary_instrument_assets()
{
    QTemporaryDir tempRoot;
    QVERIFY2(tempRoot.isValid(), "temporary root must exist");

    AnkleCaseWorkspaceRepository repo(tempRoot.path());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-instrument-001");
    manifest.patientId = QStringLiteral("patient-001");
    manifest.patientName = QStringLiteral("Patient 001");
    QVERIFY(repo.createCaseWorkspace(manifest));

    manifest.instrumentAssets = {
        AnkleInstrumentAsset {
            QStringLiteral("instrument:probe-main"),
            QStringLiteral("主探针"),
            QStringLiteral("instruments/5.stl"),
            QStringLiteral("instruments/5.stl"),
            QStringLiteral("stl"),
            QStringLiteral("40"),
            QStringLiteral("geometry/geometry40.ini"),
            QStringLiteral("geometry:probe-main")
        }
    };
    QVERIFY(repo.saveManifest(manifest));

    const AnkleCaseManifest restored = repo.loadManifest(manifest.caseId);
    QCOMPARE(restored.instrumentAssets.size(), 1);
    QCOMPARE(restored.instrumentAssets.first().instrumentAssetId, QStringLiteral("instrument:probe-main"));
    QCOMPARE(restored.instrumentAssets.first().normalizedPath, QStringLiteral("instruments/5.stl"));
    QCOMPARE(restored.instrumentAssets.first().geometryFilePath, QStringLiteral("geometry/geometry40.ini"));
    QCOMPARE(restored.instrumentAssets.first().trackingMarkerId, QStringLiteral("40"));
}

QTEST_APPLESS_MAIN(AnkleCaseWorkspaceRepositoryTest)
#include "AnkleCaseWorkspaceRepositoryTest.moc"
