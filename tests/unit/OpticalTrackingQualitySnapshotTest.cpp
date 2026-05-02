#include <QtTest/QtTest>

#include "Plugins/OpticalTracking/OpticalTrackingServiceImpl.h"

class OpticalTrackingQualitySnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    void default_quality_snapshot_exposes_navigation_gate_inputs_and_replay_metrics();
    void default_quality_snapshot_exposes_probe_calibration_fields();
};

void OpticalTrackingQualitySnapshotTest::default_quality_snapshot_exposes_navigation_gate_inputs_and_replay_metrics()
{
    OpticalTrackingServiceImpl service;

    const QVariantMap snapshot = service.checkTrackingQuality(QString(), QString());

    QVERIFY(snapshot.value(QStringLiteral("valid")).toBool());
    QVERIFY(snapshot.contains(QStringLiteral("tracking_jitter_mm")));
    QVERIFY(snapshot.contains(QStringLiteral("visible_frame_ratio")));
    QVERIFY(snapshot.contains(QStringLiteral("tracking_confidence_score")));
    QVERIFY(snapshot.contains(QStringLiteral("frame_drop_rate")));
    QVERIFY(snapshot.contains(QStringLiteral("replay_ready")));
    QVERIFY(snapshot.contains(QStringLiteral("tracking_profile")));
}

void OpticalTrackingQualitySnapshotTest::default_quality_snapshot_exposes_probe_calibration_fields()
{
    OpticalTrackingServiceImpl service;

    const QVariantMap snapshot = service.checkTrackingQuality(QString(), QString());

    QVERIFY(snapshot.contains(QStringLiteral("calibrated")));
    QVERIFY(snapshot.contains(QStringLiteral("calibration_accuracy_mm")));
}

QTEST_APPLESS_MAIN(OpticalTrackingQualitySnapshotTest)
#include "OpticalTrackingQualitySnapshotTest.moc"
