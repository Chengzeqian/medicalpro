#include <QtTest/QtTest>

#include "Framework/Navigation/ankle_navigation_types.h"
#include "UI/NewPages/Navigation/navigation_evaluation_summary_formatter.h"

class NavigationEvaluationSummaryFormatterTest : public QObject
{
    Q_OBJECT

private slots:
    void formatter_exposes_registration_constraint_tracking_and_gate_sections();
};

void NavigationEvaluationSummaryFormatterTest::formatter_exposes_registration_constraint_tracking_and_gate_sections()
{
    AnkleEvaluationSnapshot snapshot;
    snapshot.caseId = QStringLiteral("ankle-case-099");
    snapshot.hasRegistration = true;
    snapshot.hasNavigationRun = true;
    snapshot.hasEvaluationReport = true;
    snapshot.registrationMode = QStringLiteral("ankle_two_stage_constrained");
    snapshot.fre = 0.82;
    snapshot.targetTre = 1.36;
    snapshot.coverageScore = 0.91;
    snapshot.navigationMode = QStringLiteral("live_tracking");
    snapshot.navigationConfidenceScore = 0.87;
    snapshot.allowNavigation = true;
    snapshot.evaluationConfidenceScore = 0.87;
    snapshot.gateReasons = QStringList{
        QStringLiteral("collect_more_points"),
        QStringLiteral("check_tracking_visibility")
    };
    snapshot.calibrated = true;
    snapshot.calibrationAccuracyMm = 0.42;
    snapshot.registrationMetrics.insert(QStringLiteral("target_region_radius_mm"), 18.5);
    snapshot.registrationMetrics.insert(QStringLiteral("constraint_region_count"), 3);
    snapshot.registrationMetrics.insert(QStringLiteral("constraint_region_keys"), QStringLiteral("distal_tibia|medial_malleolus|talar_dome"));
    snapshot.registrationMetrics.insert(QStringLiteral("constraint_region_bones"), QStringLiteral("tibia|tibia|talus"));
    snapshot.registrationMetrics.insert(QStringLiteral("constraint_region_roles"), QStringLiteral("implant_window|support|articular"));
    snapshot.navigationMetrics.insert(QStringLiteral("tracking_jitter_mm"), 0.31);
    snapshot.navigationMetrics.insert(QStringLiteral("visible_frame_ratio"), 0.97);
    snapshot.navigationMetrics.insert(QStringLiteral("tracking_profile"), QStringLiteral("live_tracking"));
    snapshot.navigationMetrics.insert(QStringLiteral("tracking_confidence_score"), 0.89);
    snapshot.evaluationMetrics.insert(QStringLiteral("gate_reason_count"), 2);

    const NavigationEvaluationSummary summary = buildNavigationEvaluationSummary(snapshot);

    QCOMPARE(summary.hasData, true);
    QVERIFY(summary.headerText.contains(QStringLiteral("ankle-case-099")));
    QVERIFY(summary.registrationText.contains(QStringLiteral("0.82")));
    QVERIFY(summary.registrationText.contains(QStringLiteral("1.36")));
    QVERIFY(summary.constraintText.contains(QStringLiteral("18.50")));
    QVERIFY(summary.constraintText.contains(QStringLiteral("distal_tibia|medial_malleolus|talar_dome")));
    QVERIFY(summary.trackingText.contains(QStringLiteral("0.31")));
    QVERIFY(summary.trackingText.contains(QStringLiteral("97.00%")));
    QVERIFY(summary.gateText.contains(QStringLiteral("0.87")));
    QVERIFY(summary.gateText.contains(QStringLiteral("0.42")));
    QVERIFY(summary.gateText.contains(QStringLiteral("collect_more_points")));
}

QTEST_APPLESS_MAIN(NavigationEvaluationSummaryFormatterTest)
#include "NavigationEvaluationSummaryFormatterTest.moc"
