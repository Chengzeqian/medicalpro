#include "Plugins/OpticalTracking/OpticalTrackingServiceImpl.h"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>

class OpticalTrackingProbeCalibrationResultApplicationTest : public QObject
{
    Q_OBJECT

private slots:
    void pivot_dll_path_does_not_use_collector_api_as_pose_input();
    void apply_calibration_result_requires_explicit_tip_offset();
    void apply_calibration_result_requires_tip_offset_when_missing();
};

namespace {

QString resolveOpticalTrackingServiceImplSourcePath()
{
    const QString relativePath = QStringLiteral("Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp");
    const QStringList candidates = {
        relativePath,
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../") + relativePath)
    };

    for (const QString& candidate : candidates) {
        if (QFile::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }
    }

    return QString();
}

}

void OpticalTrackingProbeCalibrationResultApplicationTest::pivot_dll_path_does_not_use_collector_api_as_pose_input()
{
    QFile file(resolveOpticalTrackingServiceImplSourcePath());
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(file.readAll());

    QVERIFY(!source.contains(QStringLiteral("m_pcCollectorAddPoint(m_pcPipeline")));
    QVERIFY(source.contains(QStringLiteral("[OpticalTracking] Pivot calibration start")));
    QVERIFY(source.contains(QStringLiteral("geometryPath=")));
    QVERIFY(source.contains(QStringLiteral("runtimeMode=")));
    QVERIFY(source.contains(QStringLiteral("[OpticalTracking] Pivot calibration result")));
    QVERIFY(source.contains(QStringLiteral("geometryId=")));
}

void OpticalTrackingProbeCalibrationResultApplicationTest::apply_calibration_result_requires_explicit_tip_offset()
{
    OpticalTrackingServiceImpl service;
    const QString sessionId = service.createTrackingSession(
        QStringLiteral("simulated_fusiontrack_001"),
        QStringLiteral("apply-result-test"));

    QVariantMap toolConfig;
    toolConfig[QStringLiteral("name")] = QStringLiteral("Probe");
    toolConfig[QStringLiteral("type")] = QStringLiteral("probe");
    toolConfig[QStringLiteral("geometryFile")] = QStringLiteral("geometry074.ini");

    const QString toolId = service.addTrackingTool(sessionId, QStringLiteral("Probe"), toolConfig);
    QVERIFY(!toolId.isEmpty());

    QVariantMap calibrationResult;
    calibrationResult[QStringLiteral("success")] = true;
    calibrationResult[QStringLiteral("pivotPoint")] = QVariantList{1.0, 2.0, 3.0};

    QVERIFY(!service.applyCalibrationResult(sessionId, toolId, calibrationResult));
}

void OpticalTrackingProbeCalibrationResultApplicationTest::apply_calibration_result_requires_tip_offset_when_missing()
{
    OpticalTrackingServiceImpl service;
    const QString sessionId = service.createTrackingSession(
        QStringLiteral("simulated_fusiontrack_001"),
        QStringLiteral("apply-result-missing-tip-offset-test"));

    QVariantMap toolConfig;
    toolConfig[QStringLiteral("name")] = QStringLiteral("Probe");
    toolConfig[QStringLiteral("type")] = QStringLiteral("probe");
    toolConfig[QStringLiteral("geometryFile")] = QStringLiteral("geometry074.ini");

    const QString toolId = service.addTrackingTool(sessionId, QStringLiteral("Probe"), toolConfig);
    QVERIFY(!toolId.isEmpty());

    QVariantMap calibrationResult;
    calibrationResult[QStringLiteral("success")] = true;

    QVERIFY(!service.applyCalibrationResult(sessionId, toolId, calibrationResult));
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    OpticalTrackingProbeCalibrationResultApplicationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "OpticalTrackingProbeCalibrationResultApplicationTest.moc"
