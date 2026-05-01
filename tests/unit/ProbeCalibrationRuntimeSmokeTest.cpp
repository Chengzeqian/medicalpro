#include <QtTest/QtTest>

#include <QFileInfo>
#include <QLibrary>

#include "algorithms/probe_calibration/include/probe_calibration_c_api.h"

class ProbeCalibrationRuntimeSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void runtime_output_contains_probe_calibration_and_sdk_dlls();
    void probe_calibration_runtime_exports_core_c_api_and_collector_path();
};

void ProbeCalibrationRuntimeSmokeTest::runtime_output_contains_probe_calibration_and_sdk_dlls()
{
    const QString runtimeDir = QCoreApplication::applicationDirPath();
    QVERIFY2(QFileInfo::exists(runtimeDir + QStringLiteral("/ProbeCalibration.dll")),
        qPrintable(runtimeDir + QStringLiteral("/ProbeCalibration.dll")));
    QVERIFY2(QFileInfo::exists(runtimeDir + QStringLiteral("/fusionTrack64.dll")),
        qPrintable(runtimeDir + QStringLiteral("/fusionTrack64.dll")));
    QVERIFY2(QFileInfo::exists(runtimeDir + QStringLiteral("/device64.dll")),
        qPrintable(runtimeDir + QStringLiteral("/device64.dll")));
}

void ProbeCalibrationRuntimeSmokeTest::probe_calibration_runtime_exports_core_c_api_and_collector_path()
{
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/ProbeCalibration.dll");
    QLibrary library(runtimeDllPath);
    QVERIFY2(library.load(), qPrintable(library.errorString()));

    auto createPipeline = reinterpret_cast<PC_PipelineHandle (*)()>(library.resolve("PC_CreatePipeline"));
    auto destroyPipeline = reinterpret_cast<void (*)(PC_PipelineHandle)>(library.resolve("PC_DestroyPipeline"));
    auto collectorReset = reinterpret_cast<int (*)(PC_PipelineHandle)>(library.resolve("PC_CollectorReset"));
    auto collectorAddPoint = reinterpret_cast<int (*)(PC_PipelineHandle, float, float, float, uint64_t)>(
        library.resolve("PC_CollectorAddPoint"));
    auto collectorSetMinSamplesPerVoxel = reinterpret_cast<int (*)(PC_PipelineHandle, uint32_t)>(
        library.resolve("PC_CollectorSetMinSamplesPerVoxel"));
    auto collectorGetSuperPointCount = reinterpret_cast<int (*)(PC_PipelineHandle, uint32_t*)>(
        library.resolve("PC_CollectorGetSuperPointCount"));
    auto getLastError = reinterpret_cast<const char* (*)(PC_PipelineHandle)>(library.resolve("PC_GetLastError"));

    QVERIFY(createPipeline);
    QVERIFY(destroyPipeline);
    QVERIFY(collectorReset);
    QVERIFY(collectorAddPoint);
    QVERIFY(collectorSetMinSamplesPerVoxel);
    QVERIFY(collectorGetSuperPointCount);
    QVERIFY(getLastError);

    const PC_PipelineHandle pipeline = createPipeline();
    QVERIFY(pipeline != nullptr);

    QVERIFY2(collectorSetMinSamplesPerVoxel(pipeline, 1u) == 1, getLastError(pipeline));
    QVERIFY2(collectorReset(pipeline) == 1, getLastError(pipeline));
    QVERIFY2(collectorAddPoint(pipeline, 10.0f, 20.0f, 30.0f, 1000u) == 1, getLastError(pipeline));
    QVERIFY2(collectorAddPoint(pipeline, 10.2f, 20.1f, 30.0f, 2000u) == 1, getLastError(pipeline));

    uint32_t superPointCount = 0;
    QVERIFY2(collectorGetSuperPointCount(pipeline, &superPointCount) == 1, getLastError(pipeline));
    QVERIFY2(superPointCount >= 1u, "ProbeCalibration collector path did not produce any fused points");

    destroyPipeline(pipeline);
    library.unload();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ProbeCalibrationRuntimeSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProbeCalibrationRuntimeSmokeTest.moc"
