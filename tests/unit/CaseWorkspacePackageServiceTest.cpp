#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "Framework/Navigation/case_workspace_package_service.h"

class CaseWorkspacePackageServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void service_loads_case_workspace_package_with_bound_bones_and_instruments();
};

void CaseWorkspacePackageServiceTest::service_loads_case_workspace_package_with_bound_bones_and_instruments()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-package-001");
    QVERIFY(repository.createCaseWorkspace(manifest));

    AnkleCaseAssetBindings bindings;
    bindings.caseId = manifest.caseId;
    bindings.boundBoneAssetIds = QStringList { QStringLiteral("bone:tibia"), QStringLiteral("bone:talus") };
    bindings.activeBoneAssetIds = bindings.boundBoneAssetIds;
    bindings.boundInstrumentAssetIds = QStringList { QStringLiteral("instrument:probe-main") };
    bindings.activeInstrumentAssetIds = bindings.boundInstrumentAssetIds;
    bindings.instrumentGeometryBindings = {
        AnkleInstrumentGeometryBinding {
            QStringLiteral("instrument:probe-main"),
            QStringLiteral("geometry:probe-main"),
            QStringLiteral("geometry/probe-main.ini")
        }
    };
    QVERIFY(repository.saveCaseAssetBindings(bindings));

    CaseWorkspacePackageService service(tempRoot.path());
    const CaseWorkspacePackageSummary summary = service.loadSummary(manifest.caseId);
    QCOMPARE(summary.boundBoneCount, 2);
    QCOMPARE(summary.activeBoneCount, 2);
    QCOMPARE(summary.boundInstrumentCount, 1);
    QCOMPARE(summary.geometryBindingCount, 1);
    QCOMPARE(summary.readyForNavigation, true);
}

QTEST_APPLESS_MAIN(CaseWorkspacePackageServiceTest)
#include "CaseWorkspacePackageServiceTest.moc"
