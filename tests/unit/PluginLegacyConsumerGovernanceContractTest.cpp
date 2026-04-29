#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QString>

class PluginLegacyConsumerGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_remains_forbidden_product_mainline_consumer();
    void legacy_ctk_bridge_runtime_is_deleted();
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

void PluginLegacyConsumerGovernanceContractTest::legacy_ctk_bridge_runtime_is_deleted()
{
    const QString mainSource = readSource(QStringLiteral("main.cpp"));
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));

    QVERIFY2(!mainSource.contains(QStringLiteral("setDescriptorPolicyContext(")),
        "main.cpp still hands descriptor policy context into CTKManager");
    QVERIFY2(!mainSource.contains(QStringLiteral("legacy_ctk_runtime_bridge.h")),
        "main.cpp still composes the legacy CTK bridge");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/CTKManager.h")).exists(),
        "Framework/CTKManager.h still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/CTKManager.cpp")).exists(),
        "Framework/CTKManager.cpp still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/Platform/CtkBridge/legacy_ctk_runtime_bridge.h")).exists(),
        "legacy_ctk_runtime_bridge.h still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/Platform/CtkBridge/legacy_ctk_runtime_bridge.cpp")).exists(),
        "legacy_ctk_runtime_bridge.cpp still exists");
    QVERIFY2(!rootCMake.contains(QStringLiteral("Framework/CTKManager.cpp")),
        "CMakeLists.txt still compiles CTKManager.cpp");
    QVERIFY2(!rootCMake.contains(QStringLiteral("legacy_ctk_runtime_bridge.cpp")),
        "CMakeLists.txt still compiles legacy_ctk_runtime_bridge.cpp");
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
    QVERIFY2(!unitTestsCMake.contains(QStringLiteral("ctk_manager_descriptor_policy_context_test")),
        "tests/unit/CMakeLists.txt still registers ctk_manager_descriptor_policy_context_test");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp")).exists(),
        "tests/unit/PluginLoadPolicyCompatibilityResidueContractTest.cpp still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/tests/unit/CtkManagerDescriptorPolicyContextTest.cpp")).exists(),
        "tests/unit/CtkManagerDescriptorPolicyContextTest.cpp still exists");
}

QTEST_APPLESS_MAIN(PluginLegacyConsumerGovernanceContractTest)
#include "PluginLegacyConsumerGovernanceContractTest.moc"
