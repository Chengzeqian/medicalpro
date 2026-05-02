#include "Plugins/OpticalTracking/OpticalTrackingServiceImpl.h"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QFileInfo>

class OpticalTrackingProbeCalibrationLoadSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void runtime_output_contains_probe_calibration_runtime_dependency_chain();
    void optical_tracking_service_loads_probe_calibration_dll_from_runtime_output();
};

void OpticalTrackingProbeCalibrationLoadSmokeTest::runtime_output_contains_probe_calibration_runtime_dependency_chain()
{
    const QString runtimeDir = QCoreApplication::applicationDirPath();
    QVERIFY2(QFileInfo::exists(runtimeDir + QStringLiteral("/ProbeCalibration.dll")),
        qPrintable(runtimeDir + QStringLiteral("/ProbeCalibration.dll")));
    QVERIFY2(QFileInfo::exists(runtimeDir + QStringLiteral("/fusionTrack64.dll")),
        qPrintable(runtimeDir + QStringLiteral("/fusionTrack64.dll")));
    QVERIFY2(QFileInfo::exists(runtimeDir + QStringLiteral("/device64.dll")),
        qPrintable(runtimeDir + QStringLiteral("/device64.dll")));
}

void OpticalTrackingProbeCalibrationLoadSmokeTest::optical_tracking_service_loads_probe_calibration_dll_from_runtime_output()
{
    OpticalTrackingServiceImpl service;

    QVERIFY2(service.loadProbeCalibrationDLL(), qPrintable(service.getLastError()));
    QVERIFY(service.m_pcLoaded);
    QVERIFY(service.m_pcPipeline != nullptr);
    QVERIFY(service.m_pcCreate != nullptr);
    QVERIFY(service.m_pcDestroy != nullptr);
    QVERIFY(service.m_pcStartCalibration != nullptr);
    QVERIFY(service.m_pcFinishCalibration != nullptr);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    OpticalTrackingProbeCalibrationLoadSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "OpticalTrackingProbeCalibrationLoadSmokeTest.moc"
