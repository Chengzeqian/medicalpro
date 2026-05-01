#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

class ProbeCalibrationSubprojectContractTest : public QObject
{
    Q_OBJECT

private slots:
    void probe_calibration_subproject_uses_in_tree_cmake_contract();

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

void ProbeCalibrationSubprojectContractTest::probe_calibration_subproject_uses_in_tree_cmake_contract()
{
    const QFileInfo probeCalibrationCmake(QStringLiteral(MEDICALPRO_SOURCE_DIR "/algorithms/probe_calibration/CMakeLists.txt"));
    QVERIFY2(probeCalibrationCmake.exists(), "algorithms/probe_calibration/CMakeLists.txt must exist");
    QVERIFY2(probeCalibrationCmake.isReadable(), "algorithms/probe_calibration/CMakeLists.txt must be readable");

    const QString cmakeSource = readSource(QStringLiteral("algorithms/probe_calibration/CMakeLists.txt"));
    QVERIFY2(cmakeSource.contains(QStringLiteral("project(ProbeCalibration")),
        "ProbeCalibration subproject must declare project(ProbeCalibration");
    QVERIFY2(cmakeSource.contains(QStringLiteral("MEDICALPRO_ATRACSYS_SDK_DIR")),
        "ProbeCalibration subproject must consume MEDICALPRO_ATRACSYS_SDK_DIR");
    QVERIFY2(cmakeSource.contains(QStringLiteral("MEDICALPRO_EIGEN_ROOT")),
        "ProbeCalibration subproject must consume MEDICALPRO_EIGEN_ROOT");
    QVERIFY2(cmakeSource.contains(QStringLiteral("option(PROBECALIB_BUILD_TOOLS \"Build ProbeCalibration standalone tools\" OFF)")),
        "ProbeCalibration tools must stay OFF by default");
    QVERIFY2(cmakeSource.contains(QStringLiteral("if(PROBECALIB_BUILD_TOOLS)")),
        "ProbeCalibration standalone tools must stay behind the PROBECALIB_BUILD_TOOLS guard");
    QVERIFY2(cmakeSource.contains(QStringLiteral("find_library(PROBE_CALIBRATION_FUSIONTRACK_LIBRARY")),
        "ProbeCalibration must resolve the Atracsys SDK library with target-local find_library");
    QVERIFY2(!cmakeSource.contains(QStringLiteral("../eigen")),
        "ProbeCalibration subproject must not reference ../eigen");
    QVERIFY2(!cmakeSource.contains(QStringLiteral("../Atracsys")),
        "ProbeCalibration subproject must not reference ../Atracsys");
    QVERIFY2(!cmakeSource.contains(QStringLiteral("E:/ICPtry")),
        "ProbeCalibration subproject must not reference E:/ICPtry");
    QVERIFY2(!cmakeSource.contains(QStringLiteral("link_directories(")),
        "ProbeCalibration subproject must not use global link_directories");

    const QStringList forbiddenDirectories = {
        QStringLiteral("algorithms/probe_calibration/build"),
        QStringLiteral("algorithms/probe_calibration/geometry")
    };

    for (const QString& relativePath : forbiddenDirectories) {
        const QDir forbiddenDirectory(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
        QVERIFY2(!forbiddenDirectory.exists(), qPrintable(relativePath + QStringLiteral(" must not exist")));
    }

    const QFileInfo forbiddenBatchFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/algorithms/probe_calibration/run_calibration.bat"));
    QVERIFY2(!forbiddenBatchFile.exists(), "algorithms/probe_calibration/run_calibration.bat must not exist");
}

QTEST_APPLESS_MAIN(ProbeCalibrationSubprojectContractTest)
#include "ProbeCalibrationSubprojectContractTest.moc"
