#include <QtTest/QtTest>

#include "Framework/Navigation/navigation_digital_twin_state_builder.h"

class NavigationDigitalTwinStateBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    void builder_reports_target_region_distance_angle_and_hit_probability();
    void builder_marks_registration_as_dominant_risk_when_target_tre_is_high();
    void builder_recommends_reregister_when_twin_confidence_drops_below_threshold();
};

void NavigationDigitalTwinStateBuilderTest::builder_reports_target_region_distance_angle_and_hit_probability()
{
    DigitalTwinTargetRegionDefinition targetRegion;
    targetRegion.available = true;
    targetRegion.centerPatient = QVector3D(10.0f, 0.0f, 0.0f);
    targetRegion.plannedAxisPatient = QVector3D(0.0f, 0.0f, 1.0f);
    targetRegion.radiusMm = 5.0;

    NavigationTransformResult transformResult;
    transformResult.valid = true;
    transformResult.vtkToolTransform.translate(12.0f, 0.0f, 0.0f);

    const TargetRegionNavigationStatus status =
        buildTargetRegionNavigationStatus(targetRegion, transformResult);

    QVERIFY(status.targetRegionAvailable);
    QCOMPARE(status.distanceToTargetMm, 2.0);
    QVERIFY(status.targetHitProbability > 0.5);
}

void NavigationDigitalTwinStateBuilderTest::builder_marks_registration_as_dominant_risk_when_target_tre_is_high()
{
    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 2.8;
    registrationResult.coverageScore = 0.74;

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.28);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.99);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.35);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = false;
    confidenceResult.score = 0.42;

    TargetRegionNavigationStatus targetStatus;
    targetStatus.targetRegionAvailable = true;
    targetStatus.distanceToTargetMm = 1.6;
    targetStatus.localConfidenceScore = 0.48;

    const DigitalTwinRiskReport riskReport = buildDigitalTwinRiskReport(
        registrationResult,
        trackingQuality,
        confidenceResult,
        targetStatus);

    QCOMPARE(riskReport.dominantRiskSource, QStringLiteral("registration"));
    QVERIFY(riskReport.riskReasons.contains(QStringLiteral("target_tre_high")));
}

void NavigationDigitalTwinStateBuilderTest::builder_recommends_reregister_when_twin_confidence_drops_below_threshold()
{
    PointRegistrationResult registrationResult;
    registrationResult.success = true;
    registrationResult.targetRegionTre = 2.9;
    registrationResult.coverageScore = 0.52;

    QVariantMap trackingQuality;
    trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 1.10);
    trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 0.82);
    trackingQuality.insert(QStringLiteral("calibrated"), true);
    trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.90);

    NavigationConfidenceResult confidenceResult;
    confidenceResult.allowNavigation = false;
    confidenceResult.score = 0.38;

    TargetRegionNavigationStatus targetStatus;
    targetStatus.targetRegionAvailable = true;
    targetStatus.distanceToTargetMm = 3.4;
    targetStatus.localConfidenceScore = 0.36;

    const DigitalTwinRiskReport riskReport = buildDigitalTwinRiskReport(
        registrationResult,
        trackingQuality,
        confidenceResult,
        targetStatus);

    const DigitalTwinState twinState = buildDigitalTwinState(
        registrationResult,
        trackingQuality,
        confidenceResult,
        targetStatus,
        riskReport);

    QVERIFY(twinState.valid);
    QVERIFY(twinState.reRegisterRecommended);
    QVERIFY(!twinState.allowNavigation);
    QVERIFY(twinState.twinConfidenceScore < 0.5);
}

QTEST_APPLESS_MAIN(NavigationDigitalTwinStateBuilderTest)
#include "NavigationDigitalTwinStateBuilderTest.moc"
