#include <QtTest/QtTest>

#include <type_traits>

#include <QFileInfo>
#include <QLibrary>

#include "algorithms/probe_calibration/include/probe_calibration_c_api.h"

static_assert(std::is_standard_layout_v<PC_Matrix4x4f>, "PC_Matrix4x4f must remain standard layout");
static_assert(std::is_standard_layout_v<PC_PoseSample>, "PC_PoseSample must remain standard layout");
static_assert(std::is_standard_layout_v<PC_CalibrationResult>, "PC_CalibrationResult must remain standard layout");
static_assert(std::is_standard_layout_v<PC_CalibrationStats>, "PC_CalibrationStats must remain standard layout");
static_assert(sizeof(PC_Matrix4x4f) == sizeof(float) * 16, "PC_Matrix4x4f must contain 16 floats");
static_assert(std::is_same_v<decltype(((PC_Matrix4x4f*)nullptr)->m[0]), float&>,
    "PC_Matrix4x4f.m element type mismatch");
static_assert(std::is_same_v<decltype(((PC_PoseSample*)nullptr)->geometry_id), uint32_t>,
    "PC_PoseSample.geometry_id type mismatch");
static_assert(std::is_same_v<decltype(((PC_PoseSample*)nullptr)->timestamp_us), uint64_t>,
    "PC_PoseSample.timestamp_us type mismatch");
static_assert(std::is_same_v<decltype(((PC_PoseSample*)nullptr)->registration_error), float>,
    "PC_PoseSample.registration_error type mismatch");
static_assert(std::is_same_v<decltype(((PC_PoseSample*)nullptr)->is_valid), int>,
    "PC_PoseSample.is_valid type mismatch");
static_assert(std::is_same_v<decltype(((PC_PoseSample*)nullptr)->transform), PC_Matrix4x4f>,
    "PC_PoseSample.transform type mismatch");
static_assert(std::is_same_v<decltype(((PC_CalibrationResult*)nullptr)->tip_offset), PC_Vector3f>,
    "PC_CalibrationResult.tip_offset type mismatch");
static_assert(std::is_same_v<decltype(((PC_CalibrationResult*)nullptr)->residual_error), float>,
    "PC_CalibrationResult.residual_error type mismatch");
static_assert(std::is_same_v<decltype(((PC_CalibrationResult*)nullptr)->geometry_id), uint32_t>,
    "PC_CalibrationResult.geometry_id type mismatch");
static_assert(std::is_same_v<decltype(((PC_CalibrationResult*)nullptr)->num_poses_used), uint32_t>,
    "PC_CalibrationResult.num_poses_used type mismatch");
static_assert(std::is_same_v<decltype(((PC_CalibrationResult*)nullptr)->is_valid), int>,
    "PC_CalibrationResult.is_valid type mismatch");

class ProbeCalibrationRuntimeSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void runtime_output_contains_probe_calibration_and_sdk_dlls();
    void probe_calibration_runtime_exports_core_c_api_and_collector_path();
    void probe_calibration_runtime_exports_unified_tracking_calibration_contract();
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

void ProbeCalibrationRuntimeSmokeTest::probe_calibration_runtime_exports_unified_tracking_calibration_contract()
{
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/ProbeCalibration.dll");
    QLibrary library(runtimeDllPath);
    QVERIFY2(library.load(), qPrintable(library.errorString()));

    auto configureGeometry = reinterpret_cast<int (*)(PC_PipelineHandle, const char*, uint32_t)>(
        library.resolve("PC_ConfigureGeometry"));
    auto resetCalibrationSession = reinterpret_cast<int (*)(PC_PipelineHandle)>(
        library.resolve("PC_ResetCalibrationSession"));
    auto addPoseSample = reinterpret_cast<int (*)(PC_PipelineHandle, const PC_PoseSample*)>(
        library.resolve("PC_AddPoseSample"));
    auto getCalibrationResult = reinterpret_cast<int (*)(PC_PipelineHandle, PC_CalibrationResult*)>(
        library.resolve("PC_GetCalibrationResult"));
    auto getCalibrationStats = reinterpret_cast<int (*)(PC_PipelineHandle, PC_CalibrationStats*)>(
        library.resolve("PC_GetCalibrationStats"));

    QVERIFY(configureGeometry);
    QVERIFY(resetCalibrationSession);
    QVERIFY(addPoseSample);
    QVERIFY(getCalibrationResult);
    QVERIFY(getCalibrationStats);

    library.unload();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ProbeCalibrationRuntimeSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProbeCalibrationRuntimeSmokeTest.moc"
