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
    QVERIFY2(!mainSource.contains(QStringLiteral("StartupPhaseRegistrar::RuntimePhaseHandlers runtimePhaseHandlers;")),
        "main.cpp must not default-construct RuntimePhaseHandlers");
    QVERIFY2(mainSource.contains(QStringLiteral("StartupPhaseRegistrar::RuntimePhaseHandlers runtimePhaseHandlers(")),
        "main.cpp must construct RuntimePhaseHandlers with all runtime handlers at once");

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

    struct PhaseExpectation
    {
        QString phaseKey;
        QString reasonCode;
        QString detail;
        std::atomic_int calls = 0;
    };

    PhaseExpectation platformRuntimeInit {
        QStringLiteral("Platform runtime initialization"),
        QStringLiteral("runtime_init_reason"),
        QStringLiteral("runtime_init_detail")
    };
    PhaseExpectation pluginInstallation {
        QStringLiteral("Managed plugin preparation"),
        QStringLiteral("plugin_install_reason"),
        QStringLiteral("plugin_install_detail")
    };
    PhaseExpectation criticalPluginStart {
        QStringLiteral("Core service activation"),
        QStringLiteral("critical_start_reason"),
        QStringLiteral("critical_start_detail")
    };
    PhaseExpectation deferredPluginStart {
        QStringLiteral("Deferred service activation"),
        QStringLiteral("deferred_start_reason"),
        QStringLiteral("deferred_start_detail")
    };
    PhaseExpectation serviceWarmup {
        QStringLiteral("Service warmup"),
        QStringLiteral("service_warmup_reason"),
        QStringLiteral("service_warmup_detail")
    };

    StartupPhaseRegistrar registrar;
    const StartupPhaseRegistrar::RuntimePhaseHandlers handlers(
        [&platformRuntimeInit](QApplication*) {
            platformRuntimeInit.calls.fetch_add(1);
            return StartupOrchestrator::PhaseExecutionResult {
                true,
                PlatformLifecycleResult::Succeeded,
                platformRuntimeInit.reasonCode,
                platformRuntimeInit.detail
            };
        },
        [&pluginInstallation](QApplication*) {
            pluginInstallation.calls.fetch_add(1);
            return StartupOrchestrator::PhaseExecutionResult {
                true,
                PlatformLifecycleResult::Succeeded,
                pluginInstallation.reasonCode,
                pluginInstallation.detail
            };
        },
        [&criticalPluginStart](QApplication*) {
            criticalPluginStart.calls.fetch_add(1);
            return StartupOrchestrator::PhaseExecutionResult {
                true,
                PlatformLifecycleResult::Succeeded,
                criticalPluginStart.reasonCode,
                criticalPluginStart.detail
            };
        },
        [&deferredPluginStart](QApplication*) {
            deferredPluginStart.calls.fetch_add(1);
            return StartupOrchestrator::PhaseExecutionResult {
                true,
                PlatformLifecycleResult::Succeeded,
                deferredPluginStart.reasonCode,
                deferredPluginStart.detail
            };
        },
        [&serviceWarmup](QApplication*) {
            serviceWarmup.calls.fetch_add(1);
            return StartupOrchestrator::PhaseExecutionResult {
                true,
                PlatformLifecycleResult::Succeeded,
                serviceWarmup.reasonCode,
                serviceWarmup.detail
            };
        });

    registrar.registerRuntimePhases(orchestrator, handlers);

    QSignalSpy completedSpy(orchestrator, &StartupOrchestrator::startupCompleted);
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app);

    orchestrator->start(app);
    orchestrator->waitForCompletion();

    QTRY_COMPARE(completedSpy.count(), 1);
    QVERIFY(completedSpy.at(0).at(0).toBool());
    const auto startupTraceEntries = orchestrator->getStartupTraceEntries();

    const auto assertPhaseTrace = [&startupTraceEntries](const PhaseExpectation& expectation) {
        bool matched = false;
        for (const auto& entry : startupTraceEntries) {
            if (entry.phaseKey != expectation.phaseKey) continue;
            if (entry.reasonCode != expectation.reasonCode) continue;
            QCOMPARE(entry.detail, expectation.detail);
            matched = true;
            break;
        }
        QVERIFY2(matched, qPrintable(QStringLiteral("missing startup trace entry for phase %1")
            .arg(expectation.phaseKey)));
    };

    QCOMPARE(platformRuntimeInit.calls.load(), 1);
    QCOMPARE(pluginInstallation.calls.load(), 1);
    QCOMPARE(criticalPluginStart.calls.load(), 1);
    QCOMPARE(deferredPluginStart.calls.load(), 1);
    QCOMPARE(serviceWarmup.calls.load(), 1);
    assertPhaseTrace(platformRuntimeInit);
    assertPhaseTrace(pluginInstallation);
    assertPhaseTrace(criticalPluginStart);
    assertPhaseTrace(deferredPluginStart);
    assertPhaseTrace(serviceWarmup);

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
