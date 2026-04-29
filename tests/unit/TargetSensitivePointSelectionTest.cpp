#include <QtTest/QtTest>

#include "Plugins/PointRegistration/target_sensitive_point_selector.h"

class TargetSensitivePointSelectionTest : public QObject
{
    Q_OBJECT

private slots:
    void selector_prioritizes_points_near_target_axis_with_better_spread();
};

void TargetSensitivePointSelectionTest::selector_prioritizes_points_near_target_axis_with_better_spread()
{
    TargetSensitivePointSelector selector;

    TargetRegistrationRegion region;
    region.origin = QVector3D(0.0f, 0.0f, 0.0f);
    region.primaryAxis = QVector3D(0.0f, 0.0f, 1.0f);
    region.radiusMm = 12.0f;

    QList<CandidateRegistrationPoint> candidates = {
        { QStringLiteral("tibia_medial"), QVector3D(0.0f, 2.0f, 8.0f) },
        { QStringLiteral("tibia_lateral"), QVector3D(0.0f, -2.0f, 8.0f) },
        { QStringLiteral("far_shaft"), QVector3D(30.0f, 0.0f, 50.0f) }
    };

    const QList<RecommendedRegistrationPoint> ranked = selector.rankCandidates(region, candidates, {});

    QCOMPARE(ranked.size(), 3);
    QCOMPARE(ranked.first().pointId, QStringLiteral("tibia_medial"));
    QVERIFY(ranked.first().score > ranked.last().score);
}

QTEST_APPLESS_MAIN(TargetSensitivePointSelectionTest)
#include "TargetSensitivePointSelectionTest.moc"
