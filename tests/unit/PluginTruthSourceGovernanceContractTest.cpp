#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class PluginTruthSourceGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_uses_runtime_config_and_descriptor_loader_for_product_mainline();
    void main_cpp_does_not_call_legacy_policy_helpers_for_product_mainline();
    void legacy_policy_surface_is_marked_as_compatibility_only();

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

void PluginTruthSourceGovernanceContractTest::legacy_policy_surface_is_marked_as_compatibility_only()
{
    const QString ctkManagerHeader = readSource(QStringLiteral("Framework/CTKManager.h"));
    const QString pluginLoadPolicyHeader = readSource(QStringLiteral("Framework/PluginLoadPolicy.h"));
    const QString pluginLoadPolicySource = readSource(QStringLiteral("Framework/PluginLoadPolicy.cpp"));

    QVERIFY2(ctkManagerHeader.contains(QStringLiteral("compatibility-only")),
        "CTKManager.h has not marked the legacy load-policy APIs as compatibility-only");
    QVERIFY2(pluginLoadPolicyHeader.contains(QStringLiteral("compatibility-only")),
        "PluginLoadPolicy.h has not marked PluginLoadPolicy as compatibility-only");
    QVERIFY2(pluginLoadPolicySource.contains(QStringLiteral("compatibility-only")),
        "PluginLoadPolicy.cpp does not emit compatibility-only log language");
}

QTEST_APPLESS_MAIN(PluginTruthSourceGovernanceContractTest)
#include "PluginTruthSourceGovernanceContractTest.moc"
