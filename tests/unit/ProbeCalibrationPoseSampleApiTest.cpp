#include <QtTest/QtTest>

#include <cmath>

#include <QFile>
#include <QTemporaryDir>

#include "algorithms/probe_calibration/include/probe_calibration_c_api.h"
#include "algorithms/probe_calibration/include/realtime_transform.h"

namespace {

constexpr float kPi = 3.14159265358979323846f;

ProbeCalib::PoseData makePose(uint32_t geometryId, float rotationDeg, float tx, uint64_t timestampUs)
{
    ProbeCalib::PoseData pose;
    const Eigen::AngleAxisf rotation(rotationDeg * kPi / 180.0f, ProbeCalib::Vector3f::UnitY());
    pose.setFromRT(rotation.toRotationMatrix(), ProbeCalib::Vector3f(tx, 0.0f, 0.0f));
    pose.geometry_id = geometryId;
    pose.tracking_id = 1;
    pose.registration_error = 0.1f;
    pose.timestamp_us = timestampUs;
    pose.is_valid = true;
    return pose;
}

PC_PoseSample makePoseSample(uint32_t geometryId,
                             float rotationDeg,
                             float tx,
                             uint64_t timestampUs,
                             float registrationError = 0.1f,
                             int isValid = 1)
{
    const ProbeCalib::PoseData pose = makePose(geometryId, rotationDeg, tx, timestampUs);

    PC_PoseSample sample{};
    sample.geometry_id = geometryId;
    sample.timestamp_us = timestampUs;
    sample.registration_error = registrationError;
    sample.is_valid = isValid;

    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            sample.transform.m[row * 4 + column] = pose.transform(row, column);
        }
    }

    return sample;
}

QString createGeometryFile(const QTemporaryDir& dir)
{
    const QString geometryPath = dir.filePath(QStringLiteral("probe_geometry.ini"));
    QFile geometryFile(geometryPath);
    if (geometryFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        geometryFile.write("geometry_id=42\n");
        geometryFile.close();
    }
    return geometryPath;
}

QString createCalibrationFile(const QTemporaryDir& dir)
{
    const QString calibrationPath = dir.filePath(QStringLiteral("probe_calibration.txt"));
    QFile calibrationFile(calibrationPath);
    if (calibrationFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        calibrationFile.write("# Probe Tip Calibration File\n");
        calibrationFile.write("geometry_id=42\n");
        calibrationFile.write("tip_offset_x=1.0\n");
        calibrationFile.write("tip_offset_y=2.0\n");
        calibrationFile.write("tip_offset_z=3.0\n");
        calibrationFile.write("residual_error=0.2\n");
        calibrationFile.write("num_poses_used=25\n");
        calibrationFile.close();
    }
    return calibrationPath;
}

} // namespace

class ProbeCalibrationPoseSampleApiTest : public QObject
{
    Q_OBJECT

private slots:
    void calibration_session_runs_from_configured_geometry_without_tracker_bootstrap();
    void reset_calibration_session_clears_loaded_calibration();
    void c_api_reports_structured_stats_for_external_pose_samples();
};

void ProbeCalibrationPoseSampleApiTest::calibration_session_runs_from_configured_geometry_without_tracker_bootstrap()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString geometryPath = createGeometryFile(dir);
    QVERIFY(QFile::exists(geometryPath));

    ProbeCalib::ProbeTrackingPipeline pipeline;
    QVERIFY(!pipeline.hasGeometryConfigured());
    QVERIFY(!pipeline.addPoseSample(makePose(42u, 0.0f, 0.0f, 1000u)));

    QVERIFY(pipeline.configureGeometry(geometryPath.toStdString(), 42u));
    QVERIFY(pipeline.hasGeometryConfigured());
    QCOMPARE(pipeline.geometryId(), 42u);
    QCOMPARE(QString::fromStdString(pipeline.geometryPath()), geometryPath);

    QVERIFY(!pipeline.addPoseSample(makePose(42u, 0.0f, 0.0f, 2000u)));

    pipeline.resetCalibrationSession();
    QCOMPARE(pipeline.getCalibrationState(), ProbeCalib::CalibrationState::Idle);

    pipeline.startCalibration();
    QCOMPARE(pipeline.getCalibrationState(), ProbeCalib::CalibrationState::Recording);
    QVERIFY(pipeline.isTracking());

    QVERIFY(pipeline.addPoseSample(makePose(42u, 0.0f, 0.0f, 3000u)));
    QVERIFY(pipeline.addPoseSample(makePose(42u, 12.0f, 5.0f, 4000u)));
    QVERIFY(!pipeline.addPoseSample(makePose(99u, 24.0f, 10.0f, 5000u)));

    pipeline.resetCalibrationSession();
    QCOMPARE(pipeline.getCalibrationState(), ProbeCalib::CalibrationState::Idle);
    QVERIFY(!pipeline.isTracking());
    QVERIFY(!pipeline.addPoseSample(makePose(42u, 36.0f, 15.0f, 6000u)));
}

void ProbeCalibrationPoseSampleApiTest::reset_calibration_session_clears_loaded_calibration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString calibrationPath = createCalibrationFile(dir);
    QVERIFY(QFile::exists(calibrationPath));

    ProbeCalib::ProbeTrackingPipeline pipeline;
    QVERIFY(pipeline.loadCalibration(calibrationPath.toStdString()));
    QVERIFY(pipeline.isCalibrated());
    QVERIFY(pipeline.hasGeometryConfigured());
    QCOMPARE(pipeline.geometryId(), 42u);
    QVERIFY(pipeline.getCalibrationResult().is_valid);

    pipeline.startCalibration();
    QCOMPARE(pipeline.getCalibrationState(), ProbeCalib::CalibrationState::Recording);
    QVERIFY(pipeline.addPoseSample(makePose(42u, 18.0f, 8.0f, 7000u)));

    pipeline.resetCalibrationSession();

    QVERIFY(!pipeline.isCalibrated());
    QVERIFY(!pipeline.getCalibrationResult().is_valid);
    QCOMPARE(pipeline.getCalibrationState(), ProbeCalib::CalibrationState::Idle);
}

void ProbeCalibrationPoseSampleApiTest::c_api_reports_structured_stats_for_external_pose_samples()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString geometryPath = createGeometryFile(dir);
    QVERIFY(QFile::exists(geometryPath));

    const PC_PipelineHandle handle = PC_CreatePipeline();
    QVERIFY(handle != nullptr);

    QVERIFY2(PC_ConfigureGeometry(handle, geometryPath.toUtf8().constData(), 42u) == 1, PC_GetLastError(handle));
    QVERIFY2(PC_ResetCalibrationSession(handle) == 1, PC_GetLastError(handle));
    QVERIFY2(PC_StartCalibration(handle) == 1, PC_GetLastError(handle));

    const PC_PoseSample accepted = makePoseSample(42u, 0.0f, 0.0f, 1000u);
    const PC_PoseSample invalid = makePoseSample(42u, 12.0f, 2.0f, 2000u, 0.1f, 0);
    const PC_PoseSample highError = makePoseSample(42u, 24.0f, 4.0f, 3000u, 0.6f, 1);
    const PC_PoseSample wrongGeometry = makePoseSample(99u, 36.0f, 6.0f, 4000u);
    const PC_PoseSample similar = makePoseSample(42u, 2.0f, 8.0f, 5000u);

    QVERIFY2(PC_AddPoseSample(handle, &accepted) == 1, PC_GetLastError(handle));
    QVERIFY(PC_AddPoseSample(handle, &invalid) == 0);
    QVERIFY(PC_AddPoseSample(handle, &highError) == 0);
    QVERIFY(PC_AddPoseSample(handle, &wrongGeometry) == 0);
    QVERIFY(PC_AddPoseSample(handle, &similar) == 0);

    PC_CalibrationStats stats{};
    QVERIFY2(PC_GetCalibrationStats(handle, &stats) == 1, PC_GetLastError(handle));
    QCOMPARE(stats.total_received, 5u);
    QCOMPARE(stats.total_accepted, 1u);
    QCOMPARE(stats.rejected_invalid, 1u);
    QCOMPARE(stats.rejected_high_error, 1u);
    QCOMPARE(stats.rejected_similar, 1u);
    QCOMPARE(stats.angular_coverage, 0.0f);
    QCOMPARE(stats.mean_registration_error, 0.1f);

    PC_CalibrationResult result{};
    QVERIFY2(PC_GetCalibrationResult(handle, &result) == 1, PC_GetLastError(handle));
    QCOMPARE(result.geometry_id, 42u);
    QCOMPARE(result.num_poses_used, 0u);
    QCOMPARE(result.is_valid, 0);

    QVERIFY2(PC_ResetCalibrationSession(handle) == 1, PC_GetLastError(handle));
    QVERIFY2(PC_GetCalibrationStats(handle, &stats) == 1, PC_GetLastError(handle));
    QCOMPARE(stats.total_received, 0u);
    QCOMPARE(stats.total_accepted, 0u);

    PC_DestroyPipeline(handle);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ProbeCalibrationPoseSampleApiTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProbeCalibrationPoseSampleApiTest.moc"
