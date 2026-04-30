#include <QtTest/QtTest>

#include <QApplication>
#include <QMetaObject>

#include "Framework/Platform/Bootstrap/startup_ui_coordinator.h"
#include "Framework/StartupOrchestrator.h"

class StartupUiCoordinatorTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void startup_failure_only_invokes_failure_handler();
    void startup_success_in_safe_mode_invokes_safe_mode_handler();
    void startup_success_without_safe_mode_skips_safe_mode_handler();
};

void StartupUiCoordinatorTest::init()
{
    auto* orchestrator = StartupOrchestrator::instance();
    orchestrator->waitForCompletion();
    orchestrator->clearPhaseHandlers();
    orchestrator->setLifecycleRecorder(nullptr);
    QObject::disconnect(orchestrator, &StartupOrchestrator::startupCompleted, qApp, nullptr);
}

void StartupUiCoordinatorTest::cleanup()
{
    auto* orchestrator = StartupOrchestrator::instance();
    QObject::disconnect(orchestrator, &StartupOrchestrator::startupCompleted, qApp, nullptr);
    orchestrator->waitForCompletion();
    orchestrator->clearPhaseHandlers();
    orchestrator->setLifecycleRecorder(nullptr);
}

void StartupUiCoordinatorTest::startup_failure_only_invokes_failure_handler()
{
    int failureCount = 0;
    int safeModeCount = 0;

    StartupUiCoordinator coordinator(
        [&failureCount](const QString&) { ++failureCount; },
        [&safeModeCount](const QString&) { ++safeModeCount; },
        true);

    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app != nullptr);

    coordinator.bindToStartupCompletion(app);

    QVERIFY(QMetaObject::invokeMethod(
        StartupOrchestrator::instance(),
        "startupCompleted",
        Qt::DirectConnection,
        Q_ARG(bool, false)));

    QCOMPARE(failureCount, 1);
    QCOMPARE(safeModeCount, 0);
}

void StartupUiCoordinatorTest::startup_success_in_safe_mode_invokes_safe_mode_handler()
{
    int failureCount = 0;
    int safeModeCount = 0;

    StartupUiCoordinator coordinator(
        [&failureCount](const QString&) { ++failureCount; },
        [&safeModeCount](const QString&) { ++safeModeCount; },
        true);

    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app != nullptr);

    coordinator.bindToStartupCompletion(app);

    QVERIFY(QMetaObject::invokeMethod(
        StartupOrchestrator::instance(),
        "startupCompleted",
        Qt::DirectConnection,
        Q_ARG(bool, true)));

    QCOMPARE(failureCount, 0);
    QCOMPARE(safeModeCount, 1);
}

void StartupUiCoordinatorTest::startup_success_without_safe_mode_skips_safe_mode_handler()
{
    int failureCount = 0;
    int safeModeCount = 0;

    StartupUiCoordinator coordinator(
        [&failureCount](const QString&) { ++failureCount; },
        [&safeModeCount](const QString&) { ++safeModeCount; },
        false);

    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app != nullptr);

    coordinator.bindToStartupCompletion(app);

    QVERIFY(QMetaObject::invokeMethod(
        StartupOrchestrator::instance(),
        "startupCompleted",
        Qt::DirectConnection,
        Q_ARG(bool, true)));

    QCOMPARE(failureCount, 0);
    QCOMPARE(safeModeCount, 0);
}

QTEST_MAIN(StartupUiCoordinatorTest)

#include "StartupUiCoordinatorTest.moc"
