#include <QtTest/QtTest>

#include <QDir>
#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"

class AnkleCaseWorkspaceRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void create_case_workspace_writes_manifest_and_stage_directories();
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

QTEST_APPLESS_MAIN(AnkleCaseWorkspaceRepositoryTest)
#include "AnkleCaseWorkspaceRepositoryTest.moc"
