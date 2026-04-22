#include <QtTest/QtTest>

#include <QSignalSpy>

#include "Framework/CTKManager.h"

#include "Framework/StartupOrchestrator.h"

class CtkManagerDescriptorPolicyContextTest : public QObject
{
    Q_OBJECT

private slots:
    void apply_policy_without_descriptor_context_emits_contract_break_diagnostic();
    void set_descriptor_policy_context_marks_initialized();
};

void CtkManagerDescriptorPolicyContextTest::apply_policy_without_descriptor_context_emits_contract_break_diagnostic()
{
#ifndef CTK_PLUGIN_FRAMEWORK
    QSKIP("CTK plugin framework not enabled");
#else
    auto* ctkManager = CTKManager::instance();
    auto* orchestrator = StartupOrchestrator::instance();

    const QString pluginName = QStringLiteral("ctk_missing_context_probe");
    ctkManager->setSafeMode(false);
    ctkManager->m_descriptorPolicyContextInitialized = false;
    ctkManager->m_descriptorPolicyRuntimeConfig = PlatformRuntimeConfig {};
    ctkManager->m_descriptorPolicyDescriptors.clear();
    ctkManager->m_startedPluginNames.remove(pluginName);
    ctkManager->m_deferredPlugins.remove(pluginName);
    ctkManager->m_onDemandPlugins.remove(pluginName);

    QSignalSpy reportSpy(orchestrator, &StartupOrchestrator::diagnosticReportUpdated);

    QVERIFY(ctkManager->applyPolicyForPlugin(pluginName, false, false));
    QVERIFY(ctkManager->m_onDemandPlugins.contains(pluginName));
    QVERIFY2(reportSpy.count() > 0, "applyPolicyForPlugin() did not emit diagnosticReportUpdated");

    const QString report = reportSpy.takeLast().value(0).toString();
    QVERIFY2(report.contains(QStringLiteral("diagnostic_code=descriptor_policy_context_missing_for_ctk_manager")),
        "Missing descriptor policy context did not emit the expected contract-break diagnostic code");
    QVERIFY2(report.contains(QStringLiteral("resolution_status=descriptor_policy_context_missing")),
        "Missing descriptor policy context did not emit the expected contract-break resolution status");
#endif
}

void CtkManagerDescriptorPolicyContextTest::set_descriptor_policy_context_marks_initialized()
{
#ifndef CTK_PLUGIN_FRAMEWORK
    QSKIP("CTK plugin framework not enabled");
#else
    auto* ctkManager = CTKManager::instance();
    ctkManager->m_descriptorPolicyContextInitialized = false;

    PlatformRuntimeConfig runtimeConfig;
    runtimeConfig.runtimeMode = PlatformRuntimeMode::FacadeMode;
    runtimeConfig.corePluginIds = QStringList {QStringLiteral("org.medicalpro.user_management")};

    PlatformPluginDescriptor descriptor;
    descriptor.id = QStringLiteral("org.medicalpro.user_management");
    descriptor.runtime.ctkSymbolicName = QStringLiteral("UserManagement");

    ctkManager->setDescriptorPolicyContext(runtimeConfig, {descriptor});

    QVERIFY(ctkManager->m_descriptorPolicyContextInitialized);
#endif
}

QTEST_APPLESS_MAIN(CtkManagerDescriptorPolicyContextTest)
#include "CtkManagerDescriptorPolicyContextTest.moc"
