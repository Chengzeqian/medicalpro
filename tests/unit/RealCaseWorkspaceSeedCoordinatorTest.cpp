#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"
#include "Framework/Navigation/real_case_workspace_seed_coordinator.h"

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

class RealCaseWorkspaceSeedCoordinatorTest : public QObject
{
    Q_OBJECT

private slots:
    void ensureWorkspace_bootstraps_real_case_into_runtime_cases_directory();
};

void RealCaseWorkspaceSeedCoordinatorTest::ensureWorkspace_bootstraps_real_case_into_runtime_cases_directory()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    const QString externalDir = tempRoot.path() + QStringLiteral("/external");
    QVERIFY(QDir().mkpath(externalDir));

    const QString tibiaPath = externalDir + QStringLiteral("/jinggu_left.stl");
    const QString talusPath = externalDir + QStringLiteral("/jugu_left.stl");
    QVERIFY(writeAsciiFile(
        tibiaPath,
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
        "endsolid tibia\n"));
    QVERIFY(writeAsciiFile(
        talusPath,
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
        "endsolid talus\n"));

    RealCaseWorkspaceSeed seed;
    seed.enabled = true;
    seed.caseId = QStringLiteral("ankle-case-real-45971129749");
    seed.patientId = QStringLiteral("45971129749");
    seed.patientName = QStringLiteral("Real Case 45971129749");
    seed.surgeryId = QStringLiteral("ankle-navigation-real-45971129749");
    seed.tibiaModelPath = tibiaPath;
    seed.talusModelPath = talusPath;
    seed.targetRegionCenter = QVector3D(35.0f, 5.0f, 5.0f);
    seed.targetRegionRadiusMm = 15.0;

    RealCaseWorkspaceSeedCoordinator coordinator;
    const QString casesRoot = tempRoot.path() + QStringLiteral("/runtime/cases");
    QVERIFY(coordinator.ensureWorkspace(seed, casesRoot));

    const QString dataRoot = QFileInfo(casesRoot).dir().absolutePath();
    AnkleCaseWorkspaceRepository repository(dataRoot);
    const AnkleCaseManifest manifest = repository.loadManifest(seed.caseId);
    QCOMPARE(manifest.caseId, seed.caseId);
    QCOMPARE(manifest.modelAssets.size(), 2);
    QVERIFY(QFileInfo::exists(repository.stagePath(seed.caseId, QStringLiteral("models")) + QStringLiteral("/tibia.stl")));
    QVERIFY(QFileInfo::exists(repository.stagePath(seed.caseId, QStringLiteral("models")) + QStringLiteral("/talus.stl")));

    const AnkleCaseAssetBindings bindings = repository.loadCaseAssetBindings(seed.caseId);
    QCOMPARE(bindings.boundBoneAssetIds, QStringList({ QStringLiteral("bone:tibia"), QStringLiteral("bone:talus") }));
    QCOMPARE(bindings.boundInstrumentAssetIds, QStringList({
        QStringLiteral("instrument:probe-main"),
        QStringLiteral("instrument:guide-default")
    }));
    QCOMPARE(bindings.activeInstrumentAssetIds, bindings.boundInstrumentAssetIds);
    QCOMPARE(bindings.instrumentGeometryBindings.size(), 2);
    QCOMPARE(bindings.instrumentGeometryBindings.at(0).geometryAssetId, QStringLiteral("geometry:probe-main"));
    QCOMPARE(bindings.instrumentGeometryBindings.at(1).geometryFilePath, QStringLiteral("geometry/guide-default.ini"));

    AnklePlanningService planningService(repository);
    const AnklePlanningData planning = planningService.loadPlanning(seed.caseId);
    QCOMPARE(planning.primaryBones, QStringList({ QStringLiteral("tibia"), QStringLiteral("talus") }));
    QVERIFY(planning.referenceLandmarks.contains(QStringLiteral("tibia_center")));
    QVERIFY(planning.referenceLandmarks.contains(QStringLiteral("talus_center")));
}

QTEST_APPLESS_MAIN(RealCaseWorkspaceSeedCoordinatorTest)
#include "RealCaseWorkspaceSeedCoordinatorTest.moc"
