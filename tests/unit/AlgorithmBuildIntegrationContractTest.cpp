#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class AlgorithmBuildIntegrationContractTest : public QObject
{
    Q_OBJECT

private slots:
    void root_and_runtime_checks_include_algorithm_subsystems();

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

void AlgorithmBuildIntegrationContractTest::root_and_runtime_checks_include_algorithm_subsystems()
{
    const QString rootCmake = readSource(QStringLiteral("CMakeLists.txt"));
    QVERIFY2(rootCmake.contains(QStringLiteral("add_subdirectory(algorithms/meshgpu)")),
        "Root CMakeLists.txt must include add_subdirectory(algorithms/meshgpu)");
    QVERIFY2(rootCmake.contains(QStringLiteral("add_subdirectory(algorithms/probe_calibration)")),
        "Root CMakeLists.txt must include add_subdirectory(algorithms/probe_calibration)");
    QVERIFY2(rootCmake.contains(QStringLiteral("$<TARGET_FILE:MeshGPULib>")),
        "Root CMakeLists.txt must copy or reference $<TARGET_FILE:MeshGPULib>");
    QVERIFY2(rootCmake.contains(QStringLiteral("$<TARGET_FILE:ProbeCalibrationDLL>")),
        "Root CMakeLists.txt must copy or reference $<TARGET_FILE:ProbeCalibrationDLL>");

    const QString testsCmake = readSource(QStringLiteral("tests/CMakeLists.txt"));
    QVERIFY2(testsCmake.contains(QStringLiteral("probe_calibration_runtime_dll")),
        "tests/CMakeLists.txt must pass probe_calibration_runtime_dll to runtime artifact verification");

    const QString runtimeVerifyScript = readSource(QStringLiteral("tests/runtime/verify_runtime_artifacts.cmake"));
    QVERIFY2(runtimeVerifyScript.contains(QStringLiteral("probe_calibration_runtime_dll")),
        "tests/runtime/verify_runtime_artifacts.cmake must validate probe_calibration_runtime_dll");
}

QTEST_APPLESS_MAIN(AlgorithmBuildIntegrationContractTest)
#include "AlgorithmBuildIntegrationContractTest.moc"
