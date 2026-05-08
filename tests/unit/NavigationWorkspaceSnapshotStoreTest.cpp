#include <QtTest/QtTest>

#include <QFileInfo>
#include <QTemporaryDir>

#include "UI/NewPages/Navigation/navigation_workspace_snapshot_store.h"

class NavigationWorkspaceSnapshotStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void store_persists_latest_workspace_snapshot_for_stage_gate();
    void store_restores_workspace_snapshot_as_single_truth_source();
    void store_round_trips_multi_bone_multi_instrument_workspace_snapshot();
};

void NavigationWorkspaceSnapshotStoreTest::store_persists_latest_workspace_snapshot_for_stage_gate()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    NavigationWorkspaceSnapshotStore store(tempDir.path());
    NavigationWorkspaceSnapshot snapshot;
    snapshot.caseId = QStringLiteral("case-001");
    snapshot.caseContext.currentStage = AnkleWorkflowStage::Navigation;
    snapshot.stageGate.requestedStage = AnkleWorkflowStage::Navigation;
    snapshot.stageGate.allowed = false;
    snapshot.stageGate.reasonCode = QStringLiteral("calibration_missing");
    snapshot.stageGate.reasonText = QStringLiteral("\u6807\u5b9a\u672a\u5b8c\u6210");

    QVERIFY(store.persistSnapshot(snapshot));
    QVERIFY(QFileInfo(store.latestSnapshotPath()).exists());
}

void NavigationWorkspaceSnapshotStoreTest::store_restores_workspace_snapshot_as_single_truth_source()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    NavigationWorkspaceSnapshotStore store(tempDir.path());
    NavigationWorkspaceSnapshot snapshot;
    snapshot.caseId = QStringLiteral("case-001");
    snapshot.caseContext.currentStage = AnkleWorkflowStage::Navigation;
    snapshot.stageGate.requestedStage = AnkleWorkflowStage::Navigation;
    snapshot.stageGate.allowed = false;
    snapshot.stageGate.reasonCode = QStringLiteral("calibration_missing");
    snapshot.stageGate.reasonText = QStringLiteral("\u6807\u5b9a\u672a\u5b8c\u6210");
    snapshot.calibrationState.trackingReady = true;
    snapshot.calibrationState.started = true;
    snapshot.calibrationState.collectedPoints = 4;
    snapshot.calibrationState.requiredPoints = 8;
    snapshot.calibrationState.statusText = QStringLiteral("collecting");
    snapshot.registrationState.pointCount = 5;
    snapshot.registrationState.success = true;
    snapshot.registrationState.fre = 0.9;
    snapshot.registrationState.translationX = 1.1;
    snapshot.registrationState.translationY = 2.2;
    snapshot.registrationState.translationZ = 3.3;
    snapshot.registrationState.rotationX = 4.4;
    snapshot.registrationState.rotationY = 5.5;
    snapshot.registrationState.rotationZ = 6.6;
    snapshot.registrationState.transformMatrix = QStringLiteral("tx=1.100,ty=2.200,tz=3.300,rx=4.400,ry=5.500,rz=6.600");
    snapshot.navigationState.running = true;
    snapshot.navigationState.confidence = 0.82;
    snapshot.navigationState.summaryText = QStringLiteral("active");

    QVERIFY(store.persistSnapshot(snapshot));

    const NavigationWorkspaceSnapshot restored = store.loadSnapshot();

    QCOMPARE(restored.caseId, QStringLiteral("case-001"));
    QCOMPARE(restored.caseContext.currentStage, AnkleWorkflowStage::Navigation);
    QCOMPARE(restored.stageGate.reasonCode, QStringLiteral("calibration_missing"));
    QCOMPARE(restored.stageGate.reasonText, QStringLiteral("\u6807\u5b9a\u672a\u5b8c\u6210"));
    QCOMPARE(restored.calibrationState.trackingReady, true);
    QCOMPARE(restored.calibrationState.collectedPoints, 4);
    QCOMPARE(restored.calibrationState.requiredPoints, 8);
    QCOMPARE(restored.calibrationState.statusText, QStringLiteral("collecting"));
    QCOMPARE(restored.registrationState.pointCount, 5);
    QCOMPARE(restored.registrationState.success, true);
    QCOMPARE(restored.registrationState.fre, 0.9);
    QCOMPARE(restored.registrationState.translationX, 1.1);
    QCOMPARE(restored.registrationState.translationY, 2.2);
    QCOMPARE(restored.registrationState.translationZ, 3.3);
    QCOMPARE(restored.registrationState.rotationX, 4.4);
    QCOMPARE(restored.registrationState.rotationY, 5.5);
    QCOMPARE(restored.registrationState.rotationZ, 6.6);
    QCOMPARE(restored.registrationState.transformMatrix, QStringLiteral("tx=1.100,ty=2.200,tz=3.300,rx=4.400,ry=5.500,rz=6.600"));
    QCOMPARE(restored.navigationState.running, true);
    QCOMPARE(restored.navigationState.confidence, 0.82);
    QCOMPARE(restored.navigationState.summaryText, QStringLiteral("active"));
}

void NavigationWorkspaceSnapshotStoreTest::store_round_trips_multi_bone_multi_instrument_workspace_snapshot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    NavigationWorkspaceSnapshotStore store(tempDir.path());
    NavigationWorkspaceSnapshot snapshot;
    snapshot.caseId = QStringLiteral("ankle-case-v2-001");
    snapshot.assetState.boundBoneAssets = QStringList {
        QStringLiteral("bone:tibia"),
        QStringLiteral("bone:talus")
    };
    snapshot.assetState.activeBoneAssets = snapshot.assetState.boundBoneAssets;
    snapshot.assetState.boundInstrumentIds = QStringList {
        QStringLiteral("instrument:probe-main"),
        QStringLiteral("instrument:guide-default")
    };
    snapshot.assetState.instrumentGeometryBindings = QList<NavigationInstrumentGeometryState> {
        NavigationInstrumentGeometryState {
            QStringLiteral("instrument:probe-main"),
            QStringLiteral("geometry:probe-main"),
            QStringLiteral("probe-main.rom"),
            true
        },
        NavigationInstrumentGeometryState {
            QStringLiteral("instrument:guide-default"),
            QStringLiteral("geometry:guide-default"),
            QStringLiteral("guide-default.rom"),
            true
        }
    };
    snapshot.preparationState.instrumentCalibrationStates = QList<NavigationInstrumentCalibrationState> {
        NavigationInstrumentCalibrationState {
            QStringLiteral("instrument:probe-main"),
            QStringLiteral("geometry:probe-main"),
            true,
            12,
            12,
            true,
            0.41
        },
        NavigationInstrumentCalibrationState {
            QStringLiteral("instrument:guide-default"),
            QStringLiteral("geometry:guide-default"),
            true,
            10,
            10,
            true,
            0.52
        }
    };
    snapshot.preparationState.allRequiredInstrumentsCalibrated = true;
    snapshot.registrationState.perBoneResults = QList<NavigationPerBoneRegistrationState> {
        NavigationPerBoneRegistrationState {
            QStringLiteral("bone:tibia"),
            QStringLiteral("distal"),
            6,
            true,
            0.71,
            1.03,
            0.92
        },
        NavigationPerBoneRegistrationState {
            QStringLiteral("bone:talus"),
            QStringLiteral("dome"),
            6,
            true,
            0.68,
            0.97,
            0.95
        }
    };
    snapshot.registrationState.fusedNavigationSpaceReady = true;
    snapshot.registrationState.fusedNavigationSpacePath =
        QStringLiteral("registration/fused_navigation_space.json");

    QVERIFY(store.persistSnapshot(snapshot));

    const NavigationWorkspaceSnapshot restored = store.loadSnapshot();
    QCOMPARE(restored.assetState.boundBoneAssets.size(), 2);
    QCOMPARE(restored.assetState.instrumentGeometryBindings.size(), 2);
    QCOMPARE(restored.preparationState.instrumentCalibrationStates.size(), 2);
    QCOMPARE(restored.preparationState.allRequiredInstrumentsCalibrated, true);
    QCOMPARE(restored.registrationState.perBoneResults.size(), 2);
    QCOMPARE(restored.registrationState.fusedNavigationSpaceReady, true);
    QCOMPARE(
        restored.registrationState.fusedNavigationSpacePath,
        QStringLiteral("registration/fused_navigation_space.json"));
}

QTEST_APPLESS_MAIN(NavigationWorkspaceSnapshotStoreTest)
#include "NavigationWorkspaceSnapshotStoreTest.moc"
