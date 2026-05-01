#include "Plugins/OpticalTracking/OpticalTrackingServiceImpl.h"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

class OpticalTrackingProbeCalibrationInitializationTest : public QObject
{
    Q_OBJECT

private slots:
    void runtime_output_contains_default_probe_geometry_when_sdk_is_available();
    void optical_tracking_service_exposes_geometry_resolution_and_initialization_state();
};

void OpticalTrackingProbeCalibrationInitializationTest::runtime_output_contains_default_probe_geometry_when_sdk_is_available()
{
    const QString runtimeDir = QCoreApplication::applicationDirPath();
    const QString defaultGeometryPath = runtimeDir + QStringLiteral("/atracsys_geometry/geometry072.ini");

    QVERIFY2(QFileInfo::exists(defaultGeometryPath), qPrintable(defaultGeometryPath));
}

void OpticalTrackingProbeCalibrationInitializationTest::optical_tracking_service_exposes_geometry_resolution_and_initialization_state()
{
    OpticalTrackingServiceImpl service;

    const QString geometryPath = service.findGeometryFile(QStringLiteral("072"));
    QVERIFY2(!geometryPath.isEmpty(), qPrintable(service.getLastError()));
    QVERIFY2(QFileInfo::exists(geometryPath), qPrintable(geometryPath));
    QVERIFY2(service.validateGeometryFile(geometryPath), qPrintable(service.getLastError()));

    const QVariantMap geometryInfo = service.parseGeometryInfo(geometryPath);
    QCOMPARE(geometryInfo.value(QStringLiteral("geometryId")).toString(), QStringLiteral("072"));
    QVERIFY(geometryInfo.value(QStringLiteral("fiducialCount")).toInt() > 0);

    QVERIFY2(service.loadProbeCalibrationDLL(), qPrintable(service.getLastError()));
    QVERIFY(service.m_pcLoaded);
    QVERIFY(service.m_pcPipeline != nullptr);
    QVERIFY(service.m_pcInitialize != nullptr);
    QVERIFY(service.m_pcIsInitialized != nullptr);
    QVERIFY(service.m_pcGetLastError != nullptr);
    QCOMPARE(service.m_pcIsInitialized(service.m_pcPipeline), 0);

    const bool initializeResult = service.initializeProbeCalibrationPipeline(QStringLiteral("072"));
    if (initializeResult) {
        QCOMPARE(service.m_pcIsInitialized(service.m_pcPipeline), 1);
        QCOMPARE(service.getLastError(), QString());
    } else {
        QVERIFY2(service.m_pcIsInitialized(service.m_pcPipeline) == 0,
            "Pipeline should remain uninitialized after a failed initialization attempt");
        QVERIFY2(!service.getLastError().isEmpty() || service.m_pcGetLastError(service.m_pcPipeline)[0] != '\0',
            "Initialization failure must expose an actionable error");
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    OpticalTrackingProbeCalibrationInitializationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "OpticalTrackingProbeCalibrationInitializationTest.moc"
