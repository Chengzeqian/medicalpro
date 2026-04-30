#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class StartupPhaseRegistrarContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_delegates_runtime_phase_registration_to_bootstrap_registrar();
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
}

QTEST_APPLESS_MAIN(StartupPhaseRegistrarContractTest)
#include "StartupPhaseRegistrarContractTest.moc"
