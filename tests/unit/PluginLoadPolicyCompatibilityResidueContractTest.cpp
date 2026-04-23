#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class PluginLoadPolicyCompatibilityResidueContractTest : public QObject
{
    Q_OBJECT

private slots:
    void plugin_load_policy_surface_is_minimal();

private:
    QString readSource(const QString& relativePath) const;
};

QString PluginLoadPolicyCompatibilityResidueContractTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read source file: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void PluginLoadPolicyCompatibilityResidueContractTest::plugin_load_policy_surface_is_minimal()
{
    const QString header = readSource(QStringLiteral("Framework/PluginLoadPolicy.h"));
    const QString source = readSource(QStringLiteral("Framework/PluginLoadPolicy.cpp"));

    QVERIFY2(header.contains(QStringLiteral("void loadConfig(const QString& configFilePath);")),
        "PluginLoadPolicy.h no longer exposes loadConfig()");
    QVERIFY2(header.contains(QStringLiteral("QString configPath() const;")),
        "PluginLoadPolicy.h no longer exposes configPath()");
    QVERIFY2(header.contains(QStringLiteral("bool hasValidConfig() const;")),
        "PluginLoadPolicy.h no longer exposes hasValidConfig()");
    QVERIFY2(!header.contains(QStringLiteral("LoadPolicy getLoadPolicy(")),
        "PluginLoadPolicy.h still exposes getLoadPolicy()");
    QVERIFY2(!header.contains(QStringLiteral("QStringList getDependencies(")),
        "PluginLoadPolicy.h still exposes getDependencies()");
    QVERIFY2(!header.contains(QStringLiteral("bool isCriticalPlugin(")),
        "PluginLoadPolicy.h still exposes isCriticalPlugin()");
    QVERIFY2(!header.contains(QStringLiteral("QStringList getPluginsByPolicy(")),
        "PluginLoadPolicy.h still exposes getPluginsByPolicy()");
    QVERIFY2(!header.contains(QStringLiteral("QStringList getCriticalPlugins()")),
        "PluginLoadPolicy.h still exposes getCriticalPlugins()");
    QVERIFY2(!header.contains(QStringLiteral("QStringList getAllConfiguredPlugins()")),
        "PluginLoadPolicy.h still exposes getAllConfiguredPlugins()");
    QVERIFY2(!source.contains(QStringLiteral("LoadPolicy PluginLoadPolicy::getLoadPolicy(")),
        "PluginLoadPolicy.cpp still implements getLoadPolicy()");
    QVERIFY2(!source.contains(QStringLiteral("PluginLoadPolicy::isCriticalPlugin(")),
        "PluginLoadPolicy.cpp still implements isCriticalPlugin()");
    QVERIFY2(source.contains(QStringLiteral("compatibility-only")),
        "PluginLoadPolicy.cpp no longer logs compatibility-only language");
}

QTEST_APPLESS_MAIN(PluginLoadPolicyCompatibilityResidueContractTest)
#include "PluginLoadPolicyCompatibilityResidueContractTest.moc"
