#include <QtTest/QtTest>

#include <QApplication>
#include <QMetaObject>
#include <QThread>

#include "Framework/Platform/Bootstrap/startup_ui_coordinator.h"
#include "Framework/StartupOrchestrator.h"

namespace
{

class StartupCompletedEmitterThread : public QThread
{
public:
    StartupCompletedEmitterThread(StartupOrchestrator* orchestrator, bool success)
        : m_orchestrator(orchestrator)
        , m_success(success)
    {
    }

protected:
    void run() override
    {
        QVERIFY(QMetaObject::invokeMethod(
            m_orchestrator,
            "startupCompleted",
            Qt::DirectConnection,
            Q_ARG(bool, m_success)));
    }

private:
    StartupOrchestrator* m_orchestrator = nullptr;
    bool m_success = false;
};

QString uniqueDiagnosticMarker(const QString& scenario)
{
    return QStringLiteral("startup-ui-coordinator-marker:%1:%2")
        .arg(scenario)
        .arg(QDateTime::currentMSecsSinceEpoch());
}

void emitStartupCompletedFromBackgroundThread(StartupOrchestrator* orchestrator, bool success)
{
    StartupCompletedEmitterThread emitter(orchestrator, success);
    emitter.start();
    QVERIFY(emitter.wait(3000));
}

} // namespace

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
    QString receivedReportText;
    QThread* handlerThread = nullptr;
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app != nullptr);
    const QString diagnosticMarker = uniqueDiagnosticMarker(QStringLiteral("failure"));

    auto* orchestrator = StartupOrchestrator::instance();
    orchestrator->logDiagnostic(ErrorHandler::ErrorLevel::Warning, diagnosticMarker);

    StartupUiCoordinator coordinator(
        [&failureCount, &receivedReportText, &handlerThread](const QString& reportText) {
            ++failureCount;
            receivedReportText = reportText;
            handlerThread = QThread::currentThread();
        },
        [&safeModeCount](const QString&) { ++safeModeCount; },
        true);

    coordinator.bindToStartupCompletion(app);

    emitStartupCompletedFromBackgroundThread(orchestrator, false);

    QTRY_COMPARE(failureCount, 1);
    QCOMPARE(safeModeCount, 0);
    QVERIFY(receivedReportText.contains(diagnosticMarker));
    QCOMPARE(handlerThread, app->thread());
}

void StartupUiCoordinatorTest::startup_success_in_safe_mode_invokes_safe_mode_handler()
{
    int failureCount = 0;
    int safeModeCount = 0;
    QString receivedReportText;
    QThread* handlerThread = nullptr;
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app != nullptr);
    const QString diagnosticMarker = uniqueDiagnosticMarker(QStringLiteral("safe_mode_success"));

    auto* orchestrator = StartupOrchestrator::instance();
    orchestrator->logDiagnostic(ErrorHandler::ErrorLevel::Info, diagnosticMarker);

    StartupUiCoordinator coordinator(
        [&failureCount](const QString&) { ++failureCount; },
        [&safeModeCount, &receivedReportText, &handlerThread](const QString& reportText) {
            ++safeModeCount;
            receivedReportText = reportText;
            handlerThread = QThread::currentThread();
        },
        true);

    coordinator.bindToStartupCompletion(app);

    emitStartupCompletedFromBackgroundThread(orchestrator, true);

    QCOMPARE(failureCount, 0);
    QTRY_COMPARE(safeModeCount, 1);
    QVERIFY(receivedReportText.contains(diagnosticMarker));
    QCOMPARE(handlerThread, app->thread());
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
