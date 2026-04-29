#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QString>

class PluginTruthSourceGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_uses_runtime_config_and_descriptor_loader_for_product_mainline();
    void main_cpp_does_not_call_legacy_policy_helpers_for_product_mainline();
    void plugin_load_policy_shell_and_ctk_bridge_runtime_are_deleted();

private:
    QString readSource(const QString& relativePath) const;
};

QString PluginTruthSourceGovernanceContractTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read source file: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void PluginTruthSourceGovernanceContractTest::main_cpp_uses_runtime_config_and_descriptor_loader_for_product_mainline()
{
    const QString source = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(source.contains(QStringLiteral("config/platform_runtime.json")),
        "main.cpp does not read product runtime config from platform_runtime.json");
    QVERIFY2(source.contains(QStringLiteral("PlatformDescriptorLoader::loadFromDirectory")),
        "main.cpp does not load governed plugin descriptors through PlatformDescriptorLoader");
    QVERIFY2(source.contains(QStringLiteral("PlatformManagedPluginPlanBuilder::build")),
        "main.cpp does not build the managed startup plan from descriptor facts");
}

void PluginTruthSourceGovernanceContractTest::main_cpp_does_not_call_legacy_policy_helpers_for_product_mainline()
{
    const QString source = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(!source.contains(QStringLiteral("loadPluginPolicy(")),
        "main.cpp still calls loadPluginPolicy() in the product startup mainline");
    QVERIFY2(!source.contains(QStringLiteral("installPluginsFromDirectory(")),
        "main.cpp still calls installPluginsFromDirectory() in the product startup mainline");
}

void PluginTruthSourceGovernanceContractTest::plugin_load_policy_shell_and_ctk_bridge_runtime_are_deleted()
{
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));

    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/PluginLoadPolicy.h")).exists(),
        "Framework/PluginLoadPolicy.h still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/PluginLoadPolicy.cpp")).exists(),
        "Framework/PluginLoadPolicy.cpp still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/config/plugin_load_policy.json")).exists(),
        "config/plugin_load_policy.json still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/config/plugin_load_policy_compatibility.md")).exists(),
        "config/plugin_load_policy_compatibility.md still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/CTKManager.h")).exists(),
        "Framework/CTKManager.h still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/CTKManager.cpp")).exists(),
        "Framework/CTKManager.cpp still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/Platform/CtkBridge/legacy_ctk_runtime_bridge.h")).exists(),
        "legacy_ctk_runtime_bridge.h still exists");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/Platform/CtkBridge/legacy_ctk_runtime_bridge.cpp")).exists(),
        "legacy_ctk_runtime_bridge.cpp still exists");
    QVERIFY2(!rootCMake.contains(QStringLiteral("Framework/PluginLoadPolicy.h")),
        "CMakeLists.txt still compiles Framework/PluginLoadPolicy.h");
    QVERIFY2(!rootCMake.contains(QStringLiteral("Framework/PluginLoadPolicy.cpp")),
        "CMakeLists.txt still compiles Framework/PluginLoadPolicy.cpp");
    QVERIFY2(!rootCMake.contains(QStringLiteral("Framework/CTKManager.h")),
        "CMakeLists.txt still compiles Framework/CTKManager.h");
    QVERIFY2(!rootCMake.contains(QStringLiteral("Framework/CTKManager.cpp")),
        "CMakeLists.txt still compiles Framework/CTKManager.cpp");
    QVERIFY2(!rootCMake.contains(QStringLiteral("Framework/Platform/CtkBridge/legacy_ctk_runtime_bridge.h")),
        "CMakeLists.txt still compiles legacy_ctk_runtime_bridge.h");
    QVERIFY2(!rootCMake.contains(QStringLiteral("Framework/Platform/CtkBridge/legacy_ctk_runtime_bridge.cpp")),
        "CMakeLists.txt still compiles legacy_ctk_runtime_bridge.cpp");
    QVERIFY2(!rootCMake.contains(QStringLiteral("plugin_load_policy.json")),
        "CMakeLists.txt still deploys plugin_load_policy.json");
    QVERIFY2(!rootCMake.contains(QStringLiteral("plugin_load_policy_compatibility.md")),
        "CMakeLists.txt still deploys plugin_load_policy_compatibility.md");
}

QTEST_APPLESS_MAIN(PluginTruthSourceGovernanceContractTest)
#include "PluginTruthSourceGovernanceContractTest.moc"
