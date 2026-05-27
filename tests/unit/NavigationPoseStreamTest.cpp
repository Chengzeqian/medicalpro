#include <QtTest/QtTest>

#include "Framework/Navigation/navigation_pose_frame.h"
#include "Framework/Navigation/navigation_pose_stream.h"

class NavigationPoseStreamTest : public QObject
{
    Q_OBJECT

private slots:
    void stream_starts_empty_without_latest_frame();
    void stream_keeps_latest_frame_and_bounded_recent_window();
    void stream_clears_all_cached_frames();
};

void NavigationPoseStreamTest::stream_starts_empty_without_latest_frame()
{
    NavigationPoseStream stream(5);

    QVERIFY(!stream.hasLatestFrame());
    QCOMPARE(stream.sampleWindow().recentFrames.size(), 0);
    QCOMPARE(stream.maxFrameCount(), 5);
}

void NavigationPoseStreamTest::stream_keeps_latest_frame_and_bounded_recent_window()
{
    NavigationPoseStream stream(3);

    NavigationPoseFrame frameA;
    frameA.sourceId = QStringLiteral("simulator");
    frameA.toolId = QStringLiteral("instrument:probe-main");
    frameA.timestamp = QDateTime::fromString(QStringLiteral("2026-05-08T10:00:00Z"), Qt::ISODate);
    frameA.trackingVisible = true;
    frameA.trackingConfidence = 0.91;
    frameA.trackingToMarker.translate(1.0f, 0.0f, 0.0f);

    NavigationPoseFrame frameB = frameA;
    frameB.timestamp = QDateTime::fromString(QStringLiteral("2026-05-08T10:00:01Z"), Qt::ISODate);
    frameB.trackingToMarker.translate(0.0f, 2.0f, 0.0f);

    NavigationPoseFrame frameC = frameA;
    frameC.timestamp = QDateTime::fromString(QStringLiteral("2026-05-08T10:00:02Z"), Qt::ISODate);

    NavigationPoseFrame frameD = frameA;
    frameD.timestamp = QDateTime::fromString(QStringLiteral("2026-05-08T10:00:03Z"), Qt::ISODate);

    stream.pushFrame(frameA);
    stream.pushFrame(frameB);
    stream.pushFrame(frameC);
    stream.pushFrame(frameD);

    QVERIFY(stream.hasLatestFrame());
    QCOMPARE(stream.latestFrame().timestamp, frameD.timestamp);
    QCOMPARE(stream.sampleWindow().recentFrames.size(), 3);
    QCOMPARE(stream.sampleWindow().recentFrames.first().timestamp, frameB.timestamp);
    QCOMPARE(stream.sampleWindow().recentFrames.last().timestamp, frameD.timestamp);
}

void NavigationPoseStreamTest::stream_clears_all_cached_frames()
{
    NavigationPoseStream stream(3);

    NavigationPoseFrame frame;
    frame.sourceId = QStringLiteral("simulator");
    frame.toolId = QStringLiteral("instrument:probe-main");
    frame.trackingVisible = true;
    stream.pushFrame(frame);
    QVERIFY(stream.hasLatestFrame());

    stream.clear();

    QVERIFY(!stream.hasLatestFrame());
    QCOMPARE(stream.sampleWindow().recentFrames.size(), 0);
}

QTEST_APPLESS_MAIN(NavigationPoseStreamTest)
#include "NavigationPoseStreamTest.moc"
