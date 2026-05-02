#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/ankle_planning_service.h"

class AnklePlanningServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void save_planning_writes_target_pose_landmarks_and_dashboard_readiness();
    void save_planning_persists_target_region_definition();
    void save_planning_persists_explicit_anatomical_constraint_regions();
    void save_planning_persists_anatomical_constraint_region_metadata();
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

void AnklePlanningServiceTest::save_planning_persists_target_region_definition()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-003");
    manifest.patientId = QStringLiteral("patient-003");
    manifest.patientName = QStringLiteral("Patient C");
    manifest.surgeryId = QStringLiteral("surgery-003");
    manifest.workflowStage = QStringLiteral("planning");

    AnkleCaseWorkspaceRepository repo(tempRoot.path());
    QVERIFY(repo.createCaseWorkspace(manifest));

    AnklePlanningService service(repo);
    AnklePlanningData planning = service.createDefaultPlanning(manifest.caseId);
    planning.primaryBones = QStringList { QStringLiteral("tibia"), QStringLiteral("talus") };
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(1.0f, 2.0f, 3.0f));
    planning.targetRegionCenter = QVector3D(8.5f, 18.0f, -3.5f);
    planning.targetRegionRadiusMm = 22.5;

    QVERIFY(service.savePlanning(manifest.caseId, planning));

    const AnklePlanningData loaded = service.loadPlanning(manifest.caseId);
    QCOMPARE(loaded.targetRegionCenter, QVector3D(8.5f, 18.0f, -3.5f));
    QCOMPARE(loaded.targetRegionRadiusMm, 22.5);
}

void AnklePlanningServiceTest::save_planning_persists_explicit_anatomical_constraint_regions()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-004");
    manifest.patientId = QStringLiteral("patient-004");
    manifest.patientName = QStringLiteral("Patient D");
    manifest.surgeryId = QStringLiteral("surgery-004");
    manifest.workflowStage = QStringLiteral("planning");

    AnkleCaseWorkspaceRepository repo(tempRoot.path());
    QVERIFY(repo.createCaseWorkspace(manifest));

    AnklePlanningService service(repo);
    AnklePlanningData planning = service.createDefaultPlanning(manifest.caseId);
    planning.primaryBones = QStringList { QStringLiteral("tibia"), QStringLiteral("talus") };
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(1.0f, 2.0f, 3.0f));

    planning.anatomicalConstraintRegions.insert(
        QStringLiteral("tibia_distal_region"),
        QList<QVector3D> {
            QVector3D(1.0f, 1.0f, 0.0f),
            QVector3D(2.0f, 1.0f, 0.5f),
            QVector3D(1.5f, 2.0f, 0.2f)
        });
    planning.anatomicalConstraintRegions.insert(
        QStringLiteral("talus_dome_region"),
        QList<QVector3D> {
            QVector3D(10.0f, 5.0f, 4.0f),
            QVector3D(11.0f, 5.5f, 4.5f),
            QVector3D(10.5f, 6.0f, 4.2f)
        });

    QVERIFY(service.savePlanning(manifest.caseId, planning));

    const AnklePlanningData loaded = service.loadPlanning(manifest.caseId);
    QVERIFY(loaded.anatomicalConstraintRegions.contains(QStringLiteral("tibia_distal_region")));
    QVERIFY(loaded.anatomicalConstraintRegions.contains(QStringLiteral("talus_dome_region")));
    QCOMPARE(loaded.anatomicalConstraintRegions.value(QStringLiteral("tibia_distal_region")).size(), 3);
    QCOMPARE(loaded.anatomicalConstraintRegions.value(QStringLiteral("talus_dome_region")).size(), 3);
}

void AnklePlanningServiceTest::save_planning_persists_anatomical_constraint_region_metadata()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-005");
    manifest.patientId = QStringLiteral("patient-005");
    manifest.patientName = QStringLiteral("Patient E");
    manifest.surgeryId = QStringLiteral("surgery-005");
    manifest.workflowStage = QStringLiteral("planning");

    AnkleCaseWorkspaceRepository repo(tempRoot.path());
    QVERIFY(repo.createCaseWorkspace(manifest));

    AnklePlanningService service(repo);
    AnklePlanningData planning = service.createDefaultPlanning(manifest.caseId);
    planning.primaryBones = QStringList { QStringLiteral("tibia"), QStringLiteral("talus") };
    planning.referenceLandmarks.insert(QStringLiteral("tibia_center"), QVector3D(1.0f, 2.0f, 3.0f));
    planning.anatomicalConstraintRegions.insert(
        QStringLiteral("tibia_distal_region"),
        QList<QVector3D> {
            QVector3D(1.0f, 1.0f, 0.0f),
            QVector3D(2.0f, 1.0f, 0.5f),
            QVector3D(1.5f, 2.0f, 0.2f)
        });
    planning.anatomicalConstraintRegions.insert(
        QStringLiteral("talus_dome_region"),
        QList<QVector3D> {
            QVector3D(10.0f, 5.0f, 4.0f),
            QVector3D(11.0f, 5.5f, 4.5f),
            QVector3D(10.5f, 6.0f, 4.2f)
        });

    planning.anatomicalConstraintRegionMetadata.insert(
        QStringLiteral("tibia_distal_region"),
        AnkleConstraintRegionMetadata {
            QStringLiteral("tibia"),
            QStringLiteral("distal_region"),
            QStringLiteral("planning_json"),
            QStringLiteral("1.1")
        });
    planning.anatomicalConstraintRegionMetadata.insert(
        QStringLiteral("talus_dome_region"),
        AnkleConstraintRegionMetadata {
            QStringLiteral("talus"),
            QStringLiteral("dome_region"),
            QStringLiteral("planning_json"),
            QStringLiteral("1.1")
        });

    QVERIFY(service.savePlanning(manifest.caseId, planning));

    const AnklePlanningData loaded = service.loadPlanning(manifest.caseId);
    QVERIFY(loaded.anatomicalConstraintRegionMetadata.contains(QStringLiteral("tibia_distal_region")));
    QVERIFY(loaded.anatomicalConstraintRegionMetadata.contains(QStringLiteral("talus_dome_region")));
    QCOMPARE(
        loaded.anatomicalConstraintRegionMetadata.value(QStringLiteral("tibia_distal_region")).boneName,
        QStringLiteral("tibia"));
    QCOMPARE(
        loaded.anatomicalConstraintRegionMetadata.value(QStringLiteral("tibia_distal_region")).regionRole,
        QStringLiteral("distal_region"));
    QCOMPARE(
        loaded.anatomicalConstraintRegionMetadata.value(QStringLiteral("talus_dome_region")).source,
        QStringLiteral("planning_json"));
    QCOMPARE(
        loaded.anatomicalConstraintRegionMetadata.value(QStringLiteral("talus_dome_region")).version,
        QStringLiteral("1.1"));
}

QTEST_APPLESS_MAIN(AnklePlanningServiceTest)
#include "AnklePlanningServiceTest.moc"
