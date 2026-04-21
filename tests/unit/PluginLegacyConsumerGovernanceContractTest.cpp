#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class PluginLegacyConsumerGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_remains_forbidden_product_mainline_consumer();
    void legacy_consumer_inventory_classifies_current_consumers();
    void ctk_manager_internal_policy_helpers_are_marked_as_temporary_internal_compatibility_debt();

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
    QVERIFY2(inventory.contains(QStringLiteral("`CTKManager::policyForPlugin()` | `temporary_internal_compatibility_debt`")),
        "legacy consumer inventory does not classify CTKManager::policyForPlugin() as temporary_internal_compatibility_debt");
    QVERIFY2(inventory.contains(QStringLiteral("`CTKManager::applyPolicyForPlugin()` | `temporary_internal_compatibility_debt`")),
        "legacy consumer inventory does not classify CTKManager::applyPolicyForPlugin() as temporary_internal_compatibility_debt");
}

void PluginLegacyConsumerGovernanceContractTest::ctk_manager_internal_policy_helpers_are_marked_as_temporary_internal_compatibility_debt()
{
    const QString source = readSource(QStringLiteral("Framework/CTKManager.cpp"));

    QVERIFY2(source.contains(QStringLiteral("temporary_internal_compatibility_debt")),
        "CTKManager.cpp does not mark legacy policy internals as temporary_internal_compatibility_debt");
    QVERIFY2(source.contains(QStringLiteral("Product startup truth remains descriptor-driven")),
        "CTKManager.cpp does not explain that descriptor-driven startup remains the product truth source");
}

QTEST_APPLESS_MAIN(PluginLegacyConsumerGovernanceContractTest)
#include "PluginLegacyConsumerGovernanceContractTest.moc"
