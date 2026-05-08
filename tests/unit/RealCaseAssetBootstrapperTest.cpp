#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"
#include "Framework/Navigation/real_case_asset_bootstrapper.h"

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

class RealCaseAssetBootstrapperTest : public QObject
{
    Q_OBJECT

private slots:
    void bootstrap_copies_external_tibia_talus_models_into_case_workspace_and_writes_real_planning_metadata();
};

void RealCaseAssetBootstrapperTest::bootstrap_copies_external_tibia_talus_models_into_case_workspace_and_writes_real_planning_metadata()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    const QString externalDir = tempRoot.path() + QStringLiteral("/external");
    QVERIFY(QDir().mkpath(externalDir));

    const QString externalTibiaPath = externalDir + QStringLiteral("/jinggu_left.stl");
    const QString externalTalusPath = externalDir + QStringLiteral("/jugu_left.stl");

    const QByteArray tibiaStl =
        "solid tibia\n"
        "facet normal 0 0 1\n"
        "outer loop\n"
        "vertex 0 0 0\n"
        "vertex 20 0 0\n"
        "vertex 0 20 0\n"
        "endloop\n"
        "endfacet\n"
        "facet normal 0 0 -1\n"
        "outer loop\n"
        "vertex 0 0 30\n"
        "vertex 0 20 30\n"
        "vertex 20 0 30\n"
        "endloop\n"
        "endfacet\n"
        "endsolid tibia\n";

    const QByteArray talusStl =
        "solid talus\n"
        "facet normal 0 0 1\n"
        "outer loop\n"
        "vertex 30 0 0\n"
        "vertex 42 0 0\n"
        "vertex 30 12 0\n"
        "endloop\n"
        "endfacet\n"
        "facet normal 0 0 -1\n"
        "outer loop\n"
        "vertex 30 0 12\n"
        "vertex 30 12 12\n"
        "vertex 42 0 12\n"
        "endloop\n"
        "endfacet\n"
        "endsolid talus\n";

    QVERIFY(writeAsciiFile(externalTibiaPath, tibiaStl));
    QVERIFY(writeAsciiFile(externalTalusPath, talusStl));

    RealCaseAssetBootstrapRequest request;
    request.dataRoot = tempRoot.path();
    request.caseId = QStringLiteral("ankle-case-real-import-001");
    request.patientId = QStringLiteral("45971129749");
    request.patientName = QStringLiteral("Real Case 45971129749");
    request.surgeryId = QStringLiteral("ankle-navigation-real-import-001");
    request.tibiaModelPath = externalTibiaPath;
    request.talusModelPath = externalTalusPath;
    request.defaultInstrumentAssetIds = QStringList {
        QStringLiteral("instrument:probe-main"),
        QStringLiteral("instrument:guide-default")
    };
    request.defaultInstrumentGeometryBindings = {
        AnkleInstrumentGeometryBinding {
            QStringLiteral("instrument:probe-main"),
            QStringLiteral("geometry:probe-main"),
            QStringLiteral("geometry/probe-main.ini")
        },
        AnkleInstrumentGeometryBinding {
            QStringLiteral("instrument:guide-default"),
            QStringLiteral("geometry:guide-default"),
            QStringLiteral("geometry/guide-default.ini")
        }
    };
    request.targetRegionCenter = QVector3D(35.0f, 5.0f, 5.0f);
    request.targetRegionRadiusMm = 15.0;

    RealCaseAssetBootstrapper bootstrapper;
    QVERIFY(bootstrapper.bootstrap(request));

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    const AnkleCaseManifest manifest = repository.loadManifest(request.caseId);
    QCOMPARE(manifest.caseId, request.caseId);
    QCOMPARE(manifest.modelAssets.size(), 2);
    QCOMPARE(manifest.modelAssets.at(0).boneName, QStringLiteral("tibia"));
    QCOMPARE(manifest.modelAssets.at(0).normalizedPath, QStringLiteral("models/tibia.stl"));
    QCOMPARE(manifest.modelAssets.at(1).boneName, QStringLiteral("talus"));
    QCOMPARE(manifest.modelAssets.at(1).normalizedPath, QStringLiteral("models/talus.stl"));

    const AnkleCaseAssetBindings bindings = repository.loadCaseAssetBindings(request.caseId);
    QCOMPARE(bindings.boundBoneAssetIds, QStringList({ QStringLiteral("bone:tibia"), QStringLiteral("bone:talus") }));
    QCOMPARE(bindings.activeBoneAssetIds, bindings.boundBoneAssetIds);
    QCOMPARE(bindings.boundInstrumentAssetIds, request.defaultInstrumentAssetIds);
    QCOMPARE(bindings.activeInstrumentAssetIds, request.defaultInstrumentAssetIds);
    QCOMPARE(bindings.instrumentGeometryBindings.size(), 2);
    QCOMPARE(bindings.instrumentGeometryBindings.at(0).geometryFilePath, QStringLiteral("geometry/probe-main.ini"));
    QCOMPARE(bindings.instrumentGeometryBindings.at(1).geometryAssetId, QStringLiteral("geometry:guide-default"));

    const QString tibiaWorkspacePath = repository.stagePath(request.caseId, QStringLiteral("models")) + QStringLiteral("/tibia.stl");
    const QString talusWorkspacePath = repository.stagePath(request.caseId, QStringLiteral("models")) + QStringLiteral("/talus.stl");
    QVERIFY(QFileInfo::exists(tibiaWorkspacePath));
    QVERIFY(QFileInfo::exists(talusWorkspacePath));
    QVERIFY(QFileInfo(tibiaWorkspacePath).absoluteFilePath() != QFileInfo(externalTibiaPath).absoluteFilePath());
    QVERIFY(QFileInfo(talusWorkspacePath).absoluteFilePath() != QFileInfo(externalTalusPath).absoluteFilePath());

    AnklePlanningService planningService(repository);
    const AnklePlanningData planning = planningService.loadPlanning(request.caseId);
    QCOMPARE(planning.caseId, request.caseId);
    QCOMPARE(planning.primaryBones, QStringList({ QStringLiteral("tibia"), QStringLiteral("talus") }));
    QVERIFY(planning.referenceLandmarks.contains(QStringLiteral("tibia_center")));
    QVERIFY(planning.referenceLandmarks.contains(QStringLiteral("talus_center")));
    QCOMPARE(planning.referenceLandmarks.value(QStringLiteral("tibia_center")), QVector3D(6.6666665f, 6.6666665f, 15.0f));
    QCOMPARE(planning.referenceLandmarks.value(QStringLiteral("talus_center")), QVector3D(34.0f, 4.0f, 6.0f));
    QVERIFY(planning.anatomicalConstraintRegions.contains(QStringLiteral("tibia_distal_region")));
    QVERIFY(planning.anatomicalConstraintRegions.contains(QStringLiteral("talus_dome_region")));
    QCOMPARE(planning.anatomicalConstraintRegions.value(QStringLiteral("tibia_distal_region")).size(), 3);
    QCOMPARE(planning.anatomicalConstraintRegions.value(QStringLiteral("talus_dome_region")).size(), 3);
    QVERIFY(planning.anatomicalConstraintRegionMetadata.contains(QStringLiteral("tibia_distal_region")));
    QVERIFY(planning.anatomicalConstraintRegionMetadata.contains(QStringLiteral("talus_dome_region")));
    QCOMPARE(
        planning.anatomicalConstraintRegionMetadata.value(QStringLiteral("tibia_distal_region")).boneName,
        QStringLiteral("tibia"));
    QCOMPARE(
        planning.anatomicalConstraintRegionMetadata.value(QStringLiteral("tibia_distal_region")).regionRole,
        QStringLiteral("distal_region"));
    QCOMPARE(
        planning.anatomicalConstraintRegionMetadata.value(QStringLiteral("talus_dome_region")).boneName,
        QStringLiteral("talus"));
    QCOMPARE(
        planning.anatomicalConstraintRegionMetadata.value(QStringLiteral("talus_dome_region")).regionRole,
        QStringLiteral("dome_region"));
    QCOMPARE(planning.targetRegionCenter, QVector3D(35.0f, 5.0f, 5.0f));
    QCOMPARE(planning.targetRegionRadiusMm, 15.0);
}

QTEST_APPLESS_MAIN(RealCaseAssetBootstrapperTest)
#include "RealCaseAssetBootstrapperTest.moc"
