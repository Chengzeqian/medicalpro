#include <QtTest>

#include <QApplication>
#include <QSignalSpy>
#include <QThread>

#include <atomic>
#include <memory>

#include "Framework/StartupOrchestrator.h"

class StartupOrchestratorLifecycleTest : public QObject
{
    Q_OBJECT

private slots:
    void waits_for_background_startup_before_releasing_context();
};

void StartupOrchestratorLifecycleTest::waits_for_background_startup_before_releasing_context()
{
    auto* orchestrator = StartupOrchestrator::instance();
    orchestrator->waitForCompletion();
    orchestrator->clearPhaseHandlers();
    orchestrator->setLifecycleRecorder(nullptr);

    auto context = std::make_shared<std::atomic<bool>>(false);
    std::weak_ptr<std::atomic<bool>> weakContext = context;
    std::atomic<bool> handlerEntered = false;

    orchestrator->registerPhaseHandler(
        StartupPhase::VTKInit,
        [context, &handlerEntered](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
            handlerEntered.store(true);
            QThread::msleep(30);
            return true;
        });

    QSignalSpy completedSpy(orchestrator, &StartupOrchestrator::startupCompleted);

    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app);
    orchestrator->start(app);

    while (!handlerEntered.load()) {
        QCoreApplication::processEvents();
        QThread::yieldCurrentThread();
    }

    context.reset();
    QVERIFY(!weakContext.expired());

    orchestrator->waitForCompletion();
    QTRY_COMPARE(completedSpy.count(), 1);
    orchestrator->clearPhaseHandlers();
    orchestrator->setLifecycleRecorder(nullptr);
    QTRY_VERIFY(weakContext.expired());
}

QTEST_MAIN(StartupOrchestratorLifecycleTest)
#include "StartupOrchestratorLifecycleTest.moc"
