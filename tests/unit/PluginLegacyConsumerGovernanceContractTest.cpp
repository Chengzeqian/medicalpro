#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
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
    QVERIFY2(inventory.contains(QStringLiteral("## Deleted Compatibility Shell")),
        "legacy consumer inventory does not record the deleted compatibility shell section");
    QVERIFY2(inventory.contains(QStringLiteral("`PluginLoadPolicy`")),
        "legacy consumer inventory does not record deleted PluginLoadPolicy");
    QVERIFY2(inventory.contains(QStringLiteral("forbidden_reintroduction")),
        "legacy consumer inventory does not mark shell reintroduction as forbidden");
    QVERIFY2(!inventory.contains(QStringLiteral("`config/plugin_load_policy.json` | `allowed_compatibility_surface`")),
        "legacy consumer inventory still classifies plugin_load_policy.json as allowed_compatibility_surface");
    QVERIFY2(!inventory.contains(QStringLiteral("`CTKManager::loadPluginPolicy()` | `allowed_compatibility_surface`")),
        "legacy consumer inventory still classifies CTKManager::loadPluginPolicy() as allowed_compatibility_surface");
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

    QVERIFY2(governanceMatrix.contains(QStringLiteral("PlatformCtkPolicyBridge")),
        "governance matrix does not record PlatformCtkPolicyBridge ownership");
    QVERIFY2(governanceMatrix.contains(QStringLiteral("compatibility shell deletion is complete"), Qt::CaseInsensitive),
        "governance matrix does not record completed compatibility shell deletion");
    QVERIFY2(governanceMatrix.contains(QStringLiteral("no repository-recognized legacy load-policy entry point"), Qt::CaseInsensitive),
        "governance matrix does not record absence of legacy load-policy entry points");
    QVERIFY2(decisionLog.contains(QStringLiteral("delete the final `plugin_load_policy` compatibility shell")),
        "decision log does not record the shell deletion decision");
    QVERIFY2(decisionLog.contains(QStringLiteral("delete adjacent dead legacy helpers")),
        "decision log does not record adjacent helper deletion");
    QVERIFY2(!decisionLog.contains(QStringLiteral("keep `plugin_load_policy.json` as a minimal compatibility projection")),
        "decision log still records plugin_load_policy.json retention");
}

void PluginLegacyConsumerGovernanceContractTest::runtime_acceptance_wiring_separates_product_and_compatibility_artifacts()
{
    const QString testsCMake = readSource(QStringLiteral("tests/CMakeLists.txt"));
    const QString unitTestsCMake = readSource(QStringLiteral("tests/unit/CMakeLists.txt"));
    const QString runtimeScript = readSource(QStringLiteral("tests/runtime/verify_runtime_artifacts.cmake"));

    QVERIFY2(!testsCMake.contains(QStringLiteral("NAME plugin_legacy_compatibility_runtime_contract_test")),
        "tests/CMakeLists.txt still registers plugin_legacy_compatibility_runtime_contract_test");
    QVERIFY2(!runtimeScript.contains(QStringLiteral("verify_plugin_legacy_compatibility_runtime_contract")),
        "verify_runtime_artifacts.cmake still exposes verify_plugin_legacy_compatibility_runtime_contract");
    QVERIFY2(!unitTestsCMake.contains(QStringLiteral("plugin_load_policy_compatibility_residue_contract_test")),
        "tests/unit/CMakeLists.txt still registers plugin_load_policy_compatibility_residue_contract_test");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp")).exists(),
        "tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp still exists");
}

QTEST_APPLESS_MAIN(PluginLegacyConsumerGovernanceContractTest)
#include "PluginLegacyConsumerGovernanceContractTest.moc"
