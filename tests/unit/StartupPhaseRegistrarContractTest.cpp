#include <QtTest/QtTest>

#include <QApplication>
#include <QFile>
#include <QSignalSpy>
#include <QString>

#include <atomic>

#include "Framework/Platform/Bootstrap/startup_phase_registrar.h"
#include "Framework/StartupOrchestrator.h"

class StartupPhaseRegistrarContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_delegates_runtime_phase_registration_to_bootstrap_registrar();
    void registrar_registers_all_runtime_phase_handlers_and_they_execute();
    void build_wiring_includes_registrar_sources_and_contract_test();

private:
    QString readSource(const QString& relativePath) const;
};

QString StartupPhaseRegistrarContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

void StartupPhaseRegistrarContractTest::main_cpp_delegates_runtime_phase_registration_to_bootstrap_registrar()
{
    const QString mainSource = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(mainSource.contains(QStringLiteral("Framework/Platform/Bootstrap/startup_phase_registrar.h")),
        "main.cpp must include startup_phase_registrar.h");
    QVERIFY2(mainSource.contains(QStringLiteral("StartupPhaseRegistrar")),
        "main.cpp must delegate runtime phase registration through StartupPhaseRegistrar");
    QVERIFY2(mainSource.contains(QStringLiteral("registerRuntimePhases(")),
        "main.cpp must call StartupPhaseRegistrar::registerRuntimePhases()");

    QVERIFY2(
        !mainSource.contains(QStringLiteral("orchestrator->registerPhaseHandler(StartupPhase::PlatformRuntimeInit")),
        "main.cpp must not inline PlatformRuntimeInit phase registration");
    QVERIFY2(
        !mainSource.contains(QStringLiteral("orchestrator->registerPhaseHandler(StartupPhase::PluginInstallation")),
        "main.cpp must not inline PluginInstallation phase registration");
    QVERIFY2(
        !mainSource.contains(QStringLiteral("orchestrator->registerPhaseHandler(StartupPhase::CriticalPluginStart")),
        "main.cpp must not inline CriticalPluginStart phase registration");
    QVERIFY2(
        !mainSource.contains(QStringLiteral("orchestrator->registerPhaseHandler(StartupPhase::DeferredPluginStart")),
        "main.cpp must not inline DeferredPluginStart phase registration");
    QVERIFY2(
        !mainSource.contains(QStringLiteral("orchestrator->registerPhaseHandler(StartupPhase::ServiceWarmup")),
        "main.cpp must not inline ServiceWarmup phase registration");
}

void StartupPhaseRegistrarContractTest::registrar_registers_all_runtime_phase_handlers_and_they_execute()
{
    auto* orchestrator = StartupOrchestrator::instance();
    orchestrator->waitForCompletion();
    orchestrator->clearPhaseHandlers();
    orchestrator->setLifecycleRecorder(nullptr);

    std::atomic_int platformRuntimeInitCalls = 0;
    std::atomic_int pluginInstallationCalls = 0;
    std::atomic_int criticalPluginStartCalls = 0;
    std::atomic_int deferredPluginStartCalls = 0;
    std::atomic_int serviceWarmupCalls = 0;

    StartupPhaseRegistrar registrar;
    StartupPhaseRegistrar::RuntimePhaseHandlers handlers;
    handlers.platformRuntimeInit = [&platformRuntimeInitCalls](QApplication*) {
        platformRuntimeInitCalls.fetch_add(1);
        return StartupOrchestrator::PhaseExecutionResult {};
    };
    handlers.pluginInstallation = [&pluginInstallationCalls](QApplication*) {
        pluginInstallationCalls.fetch_add(1);
        return StartupOrchestrator::PhaseExecutionResult {};
    };
    handlers.criticalPluginStart = [&criticalPluginStartCalls](QApplication*) {
        criticalPluginStartCalls.fetch_add(1);
        return StartupOrchestrator::PhaseExecutionResult {};
    };
    handlers.deferredPluginStart = [&deferredPluginStartCalls](QApplication*) {
        deferredPluginStartCalls.fetch_add(1);
        return StartupOrchestrator::PhaseExecutionResult {};
    };
    handlers.serviceWarmup = [&serviceWarmupCalls](QApplication*) {
        serviceWarmupCalls.fetch_add(1);
        return StartupOrchestrator::PhaseExecutionResult {};
    };

    registrar.registerRuntimePhases(orchestrator, handlers);

    QSignalSpy completedSpy(orchestrator, &StartupOrchestrator::startupCompleted);
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app);

    orchestrator->start(app);
    orchestrator->waitForCompletion();

    QTRY_COMPARE(completedSpy.count(), 1);
    QVERIFY(completedSpy.at(0).at(0).toBool());
    QCOMPARE(platformRuntimeInitCalls.load(), 1);
    QCOMPARE(pluginInstallationCalls.load(), 1);
    QCOMPARE(criticalPluginStartCalls.load(), 1);
    QCOMPARE(deferredPluginStartCalls.load(), 1);
    QCOMPARE(serviceWarmupCalls.load(), 1);

    orchestrator->clearPhaseHandlers();
    orchestrator->setLifecycleRecorder(nullptr);
}

void StartupPhaseRegistrarContractTest::build_wiring_includes_registrar_sources_and_contract_test()
{
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));
    const QString unitTestsCMake = readSource(QStringLiteral("tests/unit/CMakeLists.txt"));

    QVERIFY2(rootCMake.contains(QStringLiteral("Framework/Platform/Bootstrap/startup_phase_registrar.h")),
        "CMakeLists.txt must add startup_phase_registrar.h to Framework sources");
    QVERIFY2(rootCMake.contains(QStringLiteral("Framework/Platform/Bootstrap/startup_phase_registrar.cpp")),
        "CMakeLists.txt must add startup_phase_registrar.cpp to Framework sources");
    QVERIFY2(unitTestsCMake.contains(QStringLiteral("add_executable(startup_phase_registrar_contract_test")),
        "tests/unit/CMakeLists.txt must define startup_phase_registrar_contract_test");
    QVERIFY2(unitTestsCMake.contains(QStringLiteral("StartupPhaseRegistrarContractTest.cpp")),
        "tests/unit/CMakeLists.txt must compile StartupPhaseRegistrarContractTest.cpp");
    QVERIFY2(unitTestsCMake.contains(QStringLiteral("target_link_libraries(startup_phase_registrar_contract_test PRIVATE")),
        "tests/unit/CMakeLists.txt must define startup_phase_registrar_contract_test link dependencies");
    QVERIFY2(unitTestsCMake.contains(QStringLiteral("Framework")),
        "tests/unit/CMakeLists.txt must link startup_phase_registrar_contract_test against Framework");
    QVERIFY2(unitTestsCMake.contains(QStringLiteral("set_tests_properties(startup_phase_registrar_contract_test PROPERTIES")),
        "tests/unit/CMakeLists.txt must define runtime PATH for startup_phase_registrar_contract_test");
    QVERIFY2(unitTestsCMake.contains(QStringLiteral("MEDICALPRO_FRAMEWORK_UNIT_TEST_TARGETS")),
        "tests/unit/CMakeLists.txt must keep framework runtime sync wiring for startup_phase_registrar_contract_test");
}

QTEST_MAIN(StartupPhaseRegistrarContractTest)
#include "StartupPhaseRegistrarContractTest.moc"
