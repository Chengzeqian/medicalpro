#include <QtTest/QtTest>

#include "UI/NewPages/Navigation/navigation_registration_snapshot_utils.h"

class NavigationRegistrationSnapshotUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void utils_build_transform_summary_from_registration_result();
    void utils_rebuild_transform_matrix_from_workspace_registration_state();
};

void NavigationRegistrationSnapshotUtilsTest::utils_build_transform_summary_from_registration_result()
{
    PointRegistrationResult result;
    result.translationX = 1.1;
    result.translationY = 2.2;
    result.translationZ = 3.3;
    result.rotationX = 4.4;
    result.rotationY = 5.5;
    result.rotationZ = 6.6;

    QCOMPARE(
        summarizeRegistrationTransform(result),
        QStringLiteral("tx=1.100,ty=2.200,tz=3.300,rx=4.400,ry=5.500,rz=6.600"));
}

void NavigationRegistrationSnapshotUtilsTest::utils_rebuild_transform_matrix_from_workspace_registration_state()
{
    NavigationWorkspaceRegistrationState registrationState;
    registrationState.success = true;
    registrationState.translationX = 10.0;
    registrationState.translationY = 20.0;
    registrationState.translationZ = 30.0;
    registrationState.rotationZ = 90.0;

    const QMatrix4x4 transform = buildRegistrationTransformMatrix(registrationState);
    const QVector3D translatedOrigin = transform.map(QVector3D(0.0f, 0.0f, 0.0f));
    const QVector3D translatedXAxis = transform.map(QVector3D(1.0f, 0.0f, 0.0f));

    QVERIFY(qAbs(translatedOrigin.x() - 10.0f) < 0.001f);
    QVERIFY(qAbs(translatedOrigin.y() - 20.0f) < 0.001f);
    QVERIFY(qAbs(translatedOrigin.z() - 30.0f) < 0.001f);
    QVERIFY(qAbs(translatedXAxis.x() - 10.0f) < 0.01f);
    QVERIFY(qAbs(translatedXAxis.y() - 21.0f) < 0.01f);
    QVERIFY(qAbs(translatedXAxis.z() - 30.0f) < 0.01f);
}

QTEST_APPLESS_MAIN(NavigationRegistrationSnapshotUtilsTest)
#include "NavigationRegistrationSnapshotUtilsTest.moc"
