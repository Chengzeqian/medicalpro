#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"

class AnklePlanningServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void save_planning_writes_target_pose_landmarks_and_dashboard_readiness();
};

void AnklePlanningServiceTest::save_planning_writes_target_pose_landmarks_and_dashboard_readiness()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-002");
    manifest.patientId = QStringLiteral("patient-002");
    manifest.patientName = QStringLiteral("Patient B");
    manifest.surgeryId = QStringLiteral("surgery-002");
    manifest.workflowStage = QStringLiteral("planning");

    AnkleCaseWorkspaceRepository repo(tempRoot.path());
    QVERIFY(repo.createCaseWorkspace(manifest));

    AnklePlanningService service(repo);
    AnklePlanningData planning = service.createDefaultPlanning(manifest.caseId);
    planning.primaryBones = QStringList { QStringLiteral("tibia"), QStringLiteral("talus") };
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(1.0f, 2.0f, 3.0f));
    planning.referenceLandmarks.insert(QStringLiteral("talus_dome"), QVector3D(4.0f, 5.0f, 6.0f));
    planning.targetTranslation = QVector3D(10.0f, 0.0f, 5.0f);
    planning.targetOrientation = QQuaternion::fromEulerAngles(0.0f, 5.0f, 0.0f);

    QVERIFY(service.savePlanning(manifest.caseId, planning));

    const AnklePlanningData loaded = service.loadPlanning(manifest.caseId);
    QCOMPARE(loaded.primaryBones, QStringList({ QStringLiteral("tibia"), QStringLiteral("talus") }));
    QVERIFY(loaded.referenceLandmarks.contains(QStringLiteral("tibia_center")));

    const QVariantMap readiness = service.buildDashboardReadiness(manifest.caseId);
    QCOMPARE(readiness.value(QStringLiteral("planning_ready")).toBool(), true);
    QCOMPARE(readiness.value(QStringLiteral("registration_ready")).toBool(), false);
}

QTEST_APPLESS_MAIN(AnklePlanningServiceTest)
#include "AnklePlanningServiceTest.moc"
