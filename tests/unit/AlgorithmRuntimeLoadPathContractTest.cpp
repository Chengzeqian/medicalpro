#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class AlgorithmRuntimeLoadPathContractTest : public QObject
{
    Q_OBJECT

private slots:
    void host_adapters_load_algorithm_dlls_from_runtime_output_without_private_source_fallbacks();

private:
    QString readSource(const QString& relativePath) const
    {
        QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTest::qFail(qPrintable(relativePath), __FILE__, __LINE__);
            return {};
        }

        return QString::fromUtf8(file.readAll());
    }
};

void AlgorithmRuntimeLoadPathContractTest::host_adapters_load_algorithm_dlls_from_runtime_output_without_private_source_fallbacks()
{
    const QString registrationSource = readSource(QStringLiteral("Plugins/RegistrationCore/RegistrationServiceImpl.cpp"));
    const QString trackingSource = readSource(QStringLiteral("Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp"));

    QVERIFY2(registrationSource.contains(QStringLiteral("QCoreApplication::applicationDirPath() + \"/MeshGPULib.dll\"")),
        "RegistrationServiceImpl must load MeshGPULib from the runtime output directory");
    QVERIFY2(trackingSource.contains(QStringLiteral("QCoreApplication::applicationDirPath() + \"/ProbeCalibration.dll\"")),
        "OpticalTrackingServiceImpl must load ProbeCalibration from the runtime output directory");

    QVERIFY2(!registrationSource.contains(QStringLiteral("D:/Qtproject/medicalpro/ICPtry")),
        "RegistrationServiceImpl must not contain private source-tree fallback paths");
    QVERIFY2(!registrationSource.contains(QStringLiteral("E:/ICPtry")),
        "RegistrationServiceImpl must not contain external ICPtry fallback paths");
    QVERIFY2(!trackingSource.contains(QStringLiteral("D:/Qtproject/medicalpro/ICPtry")),
        "OpticalTrackingServiceImpl must not contain private source-tree fallback paths");
    QVERIFY2(!trackingSource.contains(QStringLiteral("E:/ICPtry")),
        "OpticalTrackingServiceImpl must not contain external ICPtry fallback paths");
}

QTEST_APPLESS_MAIN(AlgorithmRuntimeLoadPathContractTest)
#include "AlgorithmRuntimeLoadPathContractTest.moc"
