#include "Plugins/OpticalTracking/OpticalTrackingServiceImpl.h"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QFileInfo>

class OpticalTrackingProbeCalibrationToolGeometrySelectionTest : public QObject
{
    Q_OBJECT

private slots:
    void session_tool_geometry_configuration_drives_probe_calibration_geometry_resolution();
    void probe_calibration_geometry_resolution_requires_explicit_tool_geometry();
};

void OpticalTrackingProbeCalibrationToolGeometrySelectionTest::session_tool_geometry_configuration_drives_probe_calibration_geometry_resolution()
{
    OpticalTrackingServiceImpl service;

    const QString sessionId = service.createTrackingSession(QStringLiteral("simulated_fusiontrack_001"),
        QStringLiteral("geometry-selection-test"));

    QVariantMap toolConfig;
    toolConfig[QStringLiteral("name")] = QStringLiteral("Probe-074");
    toolConfig[QStringLiteral("type")] = QStringLiteral("probe");
    toolConfig[QStringLiteral("geometryId")] = QStringLiteral("074");
    toolConfig[QStringLiteral("geometryFile")] = QStringLiteral("geometry074.ini");

    const QString toolId = service.addTrackingTool(sessionId, QStringLiteral("Probe-074"), toolConfig);
    QVERIFY(!toolId.isEmpty());

    const QVariantMap& storedConfig = service.m_sessions[sessionId].toolConfigurations[toolId];
    QCOMPARE(storedConfig.value(QStringLiteral("geometryId")).toString(), QStringLiteral("074"));
    QCOMPARE(storedConfig.value(QStringLiteral("geometryFile")).toString(), QStringLiteral("geometry074.ini"));

    const QString geometryPath = service.resolveProbeCalibrationGeometry(sessionId, toolId);
    QVERIFY2(!geometryPath.isEmpty(), qPrintable(service.getLastError()));
    QVERIFY2(QFileInfo::exists(geometryPath), qPrintable(geometryPath));

    const QVariantMap geometryInfo = service.parseGeometryInfo(geometryPath);
    QCOMPARE(geometryInfo.value(QStringLiteral("geometryId")).toString(), QStringLiteral("074"));
}

void OpticalTrackingProbeCalibrationToolGeometrySelectionTest::probe_calibration_geometry_resolution_requires_explicit_tool_geometry()
{
    OpticalTrackingServiceImpl service;

    const QString sessionId = service.createTrackingSession(QStringLiteral("simulated_fusiontrack_001"),
        QStringLiteral("geometry-required-test"));

    QVariantMap toolConfig;
    toolConfig[QStringLiteral("name")] = QStringLiteral("Probe-No-Geometry");
    toolConfig[QStringLiteral("type")] = QStringLiteral("probe");

    const QString toolId = service.addTrackingTool(sessionId, QStringLiteral("Probe-No-Geometry"), toolConfig);
    QVERIFY(!toolId.isEmpty());

    const QString geometryPath = service.resolveProbeCalibrationGeometry(sessionId, toolId);
    QVERIFY(geometryPath.isEmpty());
    QCOMPARE(service.getLastError(),
        QStringLiteral("No probe calibration geometry resolved for tool: %1").arg(toolId));
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    OpticalTrackingProbeCalibrationToolGeometrySelectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "OpticalTrackingProbeCalibrationToolGeometrySelectionTest.moc"
