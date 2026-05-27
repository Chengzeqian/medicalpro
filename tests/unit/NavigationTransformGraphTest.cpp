#include <QtTest/QtTest>

#include "Framework/Navigation/navigation_pose_frame.h"
#include "Framework/Navigation/navigation_transform_graph.h"

class NavigationTransformGraphTest : public QObject
{
    Q_OBJECT

private slots:
    void graph_builds_vtk_tool_transform_from_pose_calibration_and_registration();
    void graph_reports_tracking_calibration_and_registration_failures();
    void graph_reports_tracking_unavailable_when_pose_not_visible();
};

void NavigationTransformGraphTest::graph_builds_vtk_tool_transform_from_pose_calibration_and_registration()
{
    NavigationPoseFrame frame;
    frame.sourceId = QStringLiteral("simulator");
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.timestamp = QDateTime::currentDateTimeUtc();
    frame.trackingVisible = true;
    frame.trackingToMarker.translate(10.0f, 0.0f, 0.0f);

    QMatrix4x4 markerToTool;
    markerToTool.translate(0.0f, 5.0f, 0.0f);

    QMatrix4x4 patientToVtkWorld;
    patientToVtkWorld.translate(0.0f, 0.0f, 7.0f);

    NavigationTransformGraph graph;
    graph.setLatestPoseFrame(frame);
    graph.setMarkerToToolTransform(markerToTool);
    graph.setPatientToVtkWorldTransform(patientToVtkWorld);

    const NavigationTransformResult result = graph.compute();

    QVERIFY(result.trackingAvailable);
    QVERIFY(result.calibrationAvailable);
    QVERIFY(result.registrationAvailable);
    QVERIFY(result.valid);
    QCOMPARE(result.failureCode, QStringLiteral("ok"));
    QCOMPARE(result.vtkToolTransform.column(3).toVector3D(), QVector3D(10.0f, 5.0f, 7.0f));
}

void NavigationTransformGraphTest::graph_reports_tracking_calibration_and_registration_failures()
{
    NavigationTransformGraph graph;

    QCOMPARE(graph.compute().failureCode, QStringLiteral("tracking_unavailable"));

    NavigationPoseFrame frame;
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.trackingVisible = true;
    graph.setLatestPoseFrame(frame);
    QCOMPARE(graph.compute().failureCode, QStringLiteral("calibration_missing"));

    QMatrix4x4 markerToTool;
    graph.setMarkerToToolTransform(markerToTool);
    QCOMPARE(graph.compute().failureCode, QStringLiteral("registration_missing"));

    QMatrix4x4 patientToVtkWorld;
    graph.setPatientToVtkWorldTransform(patientToVtkWorld);
    QVERIFY(graph.compute().valid);
}

void NavigationTransformGraphTest::graph_reports_tracking_unavailable_when_pose_not_visible()
{
    NavigationPoseFrame frame;
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.trackingVisible = false;

    QMatrix4x4 markerToTool;
    QMatrix4x4 patientToVtkWorld;

    NavigationTransformGraph graph;
    graph.setLatestPoseFrame(frame);
    graph.setMarkerToToolTransform(markerToTool);
    graph.setPatientToVtkWorldTransform(patientToVtkWorld);

    const NavigationTransformResult result = graph.compute();
    QVERIFY(!result.trackingAvailable);
    QVERIFY(!result.valid);
    QCOMPARE(result.failureCode, QStringLiteral("tracking_unavailable"));
}

QTEST_APPLESS_MAIN(NavigationTransformGraphTest)
#include "NavigationTransformGraphTest.moc"
