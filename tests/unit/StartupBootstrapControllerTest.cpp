#include <QtTest/QtTest>

#include <QSignalSpy>

#include "Framework/Platform/Bootstrap/StartupBootstrapController.h"

class StartupBootstrapControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void start_marks_shell_booting_before_ready();
    void publishFailure_keeps_enter_locked_and_emits_retry_reset();
};

void StartupBootstrapControllerTest::start_marks_shell_booting_before_ready()
{
    StartupBootstrapController controller;
    QSignalSpy snapshotSpy(&controller, &StartupBootstrapController::snapshotChanged);

    controller.beginBoot(QStringLiteral("CTK framework initialization"));
    QVERIFY(snapshotSpy.count() >= 1);

    const auto booting = controller.snapshot();
    QCOMPARE(booting.state, StartupShellState::Booting);
    QVERIFY(!booting.canEnterSystem);

    controller.markReady();
    const auto ready = controller.snapshot();
    QCOMPARE(ready.state, StartupShellState::Ready);
    QVERIFY(ready.canEnterSystem);
}

void StartupBootstrapControllerTest::publishFailure_keeps_enter_locked_and_emits_retry_reset()
{
    StartupBootstrapController controller;

    controller.beginBoot(QStringLiteral("Critical plugin activation"));
    controller.markFailed(
        QStringLiteral("Critical plugin activation failed"),
        QStringList{QStringLiteral("Retry startup or inspect diagnostics.")});

    const auto failed = controller.snapshot();
    QCOMPARE(failed.state, StartupShellState::Failed);
    QVERIFY(!failed.canEnterSystem);
    QVERIFY(failed.failureReason.contains(QStringLiteral("failed")));

    controller.resetForRetry();
    const auto retry = controller.snapshot();
    QCOMPARE(retry.state, StartupShellState::Booting);
    QVERIFY(!retry.canEnterSystem);
}

QTEST_APPLESS_MAIN(StartupBootstrapControllerTest)

#include "StartupBootstrapControllerTest.moc"
