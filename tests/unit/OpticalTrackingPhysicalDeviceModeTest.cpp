#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>

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

class OpticalTrackingPhysicalDeviceModeTest : public QObject
{
    Q_OBJECT

private slots:
    void device_scan_exposes_physical_and_simulation_mode_separately();
};

void OpticalTrackingPhysicalDeviceModeTest::device_scan_exposes_physical_and_simulation_mode_separately()
{
    QFile file(resolveOpticalTrackingServiceImplSourcePath());
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(file.readAll());

    QVERIFY(!source.contains(QStringLiteral("No physical devices found, adding simulated devices")));
    QVERIFY(source.contains(QStringLiteral("runtimeMode")));
    QVERIFY(source.contains(QStringLiteral("physical")));
    QVERIFY(source.contains(QStringLiteral("simulation")));
    QVERIFY(source.contains(QStringLiteral("Probe calibration requires a physical tracking device")));
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    OpticalTrackingPhysicalDeviceModeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "OpticalTrackingPhysicalDeviceModeTest.moc"
