#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class PluginLegacyConsumerGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_remains_forbidden_product_mainline_consumer();
    void legacy_consumer_inventory_classifies_current_consumers();
    void legacy_consumer_inventory_retires_internal_policy_debt();
    void ctk_manager_uses_descriptor_policy_bridge_for_runtime_classification();
    void main_cpp_hands_descriptor_policy_context_into_ctk_manager();
    void governance_docs_record_descriptor_policy_bridge_ownership();
    void runtime_acceptance_wiring_separates_product_and_compatibility_artifacts();

private:
    QString readSource(const QString& relativePath) const;
};

QString PluginLegacyConsumerGovernanceContractTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read source file: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void PluginLegacyConsumerGovernanceContractTest::main_cpp_remains_forbidden_product_mainline_consumer()
{
    const QString source = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(!source.contains(QStringLiteral("loadPluginPolicy(")),
        "main.cpp still calls loadPluginPolicy() in the product startup mainline");
    QVERIFY2(!source.contains(QStringLiteral("installPluginsFromDirectory(")),
        "main.cpp still calls installPluginsFromDirectory() in the product startup mainline");
}

void PluginLegacyConsumerGovernanceContractTest::legacy_consumer_inventory_classifies_current_consumers()
{
    const QString inventory = readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md"));

    QVERIFY2(inventory.contains(QStringLiteral("`main.cpp` | `forbidden_product_mainline`")),
        "legacy consumer inventory does not classify main.cpp as forbidden_product_mainline");
    QVERIFY2(inventory.contains(QStringLiteral("`config/plugin_load_policy.json` | `allowed_compatibility_surface`")),
        "legacy consumer inventory does not classify plugin_load_policy.json as allowed_compatibility_surface");
    QVERIFY2(inventory.contains(QStringLiteral("`CTKManager::loadPluginPolicy()` | `allowed_compatibility_surface`")),
        "legacy consumer inventory does not classify CTKManager::loadPluginPolicy() as allowed_compatibility_surface");
    QVERIFY2(inventory.contains(QStringLiteral("`CTKManager::installPluginsFromDirectory()` | `allowed_compatibility_surface`")),
        "legacy consumer inventory does not classify CTKManager::installPluginsFromDirectory() as allowed_compatibility_surface");
}

void PluginLegacyConsumerGovernanceContractTest::legacy_consumer_inventory_retires_internal_policy_debt()
{
    const QString inventory = readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-legacy-consumer-inventory.md"));

    QVERIFY2(!inventory.contains(QStringLiteral("`CTKManager::policyForPlugin()` | `temporary_internal_compatibility_debt`")),
        "legacy consumer inventory still classifies CTKManager::policyForPlugin() as temporary_internal_compatibility_debt");
    QVERIFY2(!inventory.contains(QStringLiteral("`CTKManager::applyPolicyForPlugin()` | `temporary_internal_compatibility_debt`")),
        "legacy consumer inventory still classifies CTKManager::applyPolicyForPlugin() as temporary_internal_compatibility_debt");
    QVERIFY2(inventory.contains(QStringLiteral("PlatformCtkPolicyBridge")),
        "legacy consumer inventory does not record PlatformCtkPolicyBridge ownership for runtime classification");
}

void PluginLegacyConsumerGovernanceContractTest::ctk_manager_uses_descriptor_policy_bridge_for_runtime_classification()
{
    const QString ctkManagerSource = readSource(QStringLiteral("Framework/CTKManager.cpp"));
    const QString ctkManagerHeader = readSource(QStringLiteral("Framework/CTKManager.h"));

    QVERIFY2(ctkManagerSource.contains(QStringLiteral("PlatformCtkPolicyBridge::resolve")),
        "CTKManager.cpp does not resolve runtime classification via PlatformCtkPolicyBridge::resolve");
    QVERIFY2(!ctkManagerSource.contains(QStringLiteral("PluginLoadPolicy::instance()->isCriticalPlugin(")),
        "CTKManager.cpp still reads runtime criticality from PluginLoadPolicy::isCriticalPlugin");
    QVERIFY2(!ctkManagerSource.contains(QStringLiteral("getLoadPolicy(")),
        "CTKManager.cpp still reads runtime load bucket from PluginLoadPolicy::getLoadPolicy");
    QVERIFY2(!ctkManagerHeader.contains(QStringLiteral("policyForPlugin(")),
        "CTKManager.h still exposes the internal policyForPlugin helper");
    QVERIFY2(ctkManagerHeader.contains(QStringLiteral("setDescriptorPolicyContext(")),
        "CTKManager.h does not expose setDescriptorPolicyContext");
    QVERIFY2(ctkManagerHeader.contains(QStringLiteral("m_descriptorPolicyContextInitialized")),
        "CTKManager.h does not persist explicit descriptor policy context initialization state");
    QVERIFY2(ctkManagerSource.contains(QStringLiteral("descriptor_policy_context_missing_for_ctk_manager")),
        "CTKManager.cpp does not emit descriptor_policy_context_missing_for_ctk_manager when context is absent");
    QVERIFY2(ctkManagerSource.contains(QStringLiteral("descriptor_policy_context_missing")),
        "CTKManager.cpp does not use dedicated resolution_status for missing descriptor policy context");
}

void PluginLegacyConsumerGovernanceContractTest::main_cpp_hands_descriptor_policy_context_into_ctk_manager()
{
    const QString mainSource = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(mainSource.contains(QStringLiteral("setDescriptorPolicyContext(runtimeConfig, descriptors)")),
        "main.cpp does not hand descriptor policy context into CTKManager");
}

void PluginLegacyConsumerGovernanceContractTest::governance_docs_record_descriptor_policy_bridge_ownership()
{
    const QString governanceMatrix =
        readSource(QStringLiteral("docs/superpowers/tracking/platform-plugin-governance-matrix.md"));
    const QString decisionLog =
        readSource(QStringLiteral("docs/superpowers/tracking/platform-migration-decision-log.md"));
    const QString currentStatus = readSource(QStringLiteral("docs/current_status_and_project_overview.md"));

    QVERIFY2(governanceMatrix.contains(QStringLiteral("PlatformCtkPolicyBridge")),
        "governance matrix does not record PlatformCtkPolicyBridge ownership");
    QVERIFY2(governanceMatrix.contains(QStringLiteral("setDescriptorPolicyContext()")),
        "governance matrix does not record explicit setDescriptorPolicyContext() handoff");
    QVERIFY2(decisionLog.contains(QStringLiteral("safe mode criticality follows platform_runtime.json.core_plugin_ids")),
        "migration decision log does not record safe mode criticality truth on core_plugin_ids");
    QVERIFY2(currentStatus.contains(
                 QStringLiteral("CTKManager runtime bucket classification now resolves through PlatformCtkPolicyBridge")),
        "current status does not record CTK descriptor policy bridge runtime classification acceptance");
}

void PluginLegacyConsumerGovernanceContractTest::runtime_acceptance_wiring_separates_product_and_compatibility_artifacts()
{
    const QString testsCMake = readSource(QStringLiteral("tests/CMakeLists.txt"));
    const QString runtimeScript = readSource(QStringLiteral("tests/runtime/verify_runtime_artifacts.cmake"));

    QVERIFY2(!testsCMake.contains(QStringLiteral("-Dplugin_policy_file=$<TARGET_FILE_DIR:medicalpro>/config/plugin_load_policy.json")),
        "tests/CMakeLists.txt still wires plugin_policy_file into the default product runtime layout test");
    QVERIFY2(testsCMake.contains(QStringLiteral("NAME plugin_legacy_compatibility_runtime_contract_test")),
        "tests/CMakeLists.txt has not registered plugin_legacy_compatibility_runtime_contract_test");
    QVERIFY2(runtimeScript.contains(QStringLiteral("verify_plugin_legacy_compatibility_runtime_contract")),
        "verify_runtime_artifacts.cmake has no dedicated compatibility runtime contract mode");
}

QTEST_APPLESS_MAIN(PluginLegacyConsumerGovernanceContractTest)
#include "PluginLegacyConsumerGovernanceContractTest.moc"
