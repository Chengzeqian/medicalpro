#include <QtTest/QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

class PluginLoadPolicyCompatibilityResidueContractTest : public QObject
{
    Q_OBJECT

private slots:
    void plugin_load_policy_surface_is_minimal();
    void plugin_load_policy_projection_contains_only_descriptor_governed_plugins();
    void compatibility_note_describes_projection_boundary();
    void compatibility_artifacts_are_explicitly_deployed();

private:
    QString readSource(const QString& relativePath) const;
    QJsonDocument readJson(const QString& relativePath) const;
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

QJsonDocument PluginLoadPolicyCompatibilityResidueContractTest::readJson(const QString& relativePath) const
{
    return QJsonDocument::fromJson(readSource(relativePath).toUtf8());
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

void PluginLoadPolicyCompatibilityResidueContractTest::plugin_load_policy_projection_contains_only_descriptor_governed_plugins()
{
    const QJsonDocument document = readJson(QStringLiteral("config/plugin_load_policy.json"));
    const QJsonObject root = document.object();
    const QJsonArray plugins = root.value(QStringLiteral("plugins")).toArray();
    QStringList actualNames;
    actualNames.reserve(plugins.size());

    for (const QJsonValue& entry : plugins) {
        actualNames.append(entry.toObject().value(QStringLiteral("name")).toString());
    }

    const QStringList expectedNames{
        QStringLiteral("UserManagement"),
        QStringLiteral("DicomViewer"),
        QStringLiteral("FourViewDisplay"),
        QStringLiteral("RegistrationCore"),
        QStringLiteral("OpticalTracking")
    };

    QCOMPARE(actualNames, expectedNames);
    QCOMPARE(root.value(QStringLiteral("projection_scope")).toString(),
        QStringLiteral("descriptor_governed_ctk_plugin_set"));
    QVERIFY2(!actualNames.contains(QStringLiteral("BoneSegmentation")),
        "plugin_load_policy.json still contains BoneSegmentation");
    QVERIFY2(!actualNames.contains(QStringLiteral("InstrumentManagement")),
        "plugin_load_policy.json still contains InstrumentManagement");
    QVERIFY2(!actualNames.contains(QStringLiteral("Registration2D3D")),
        "plugin_load_policy.json still contains Registration2D3D");
    QVERIFY2(!actualNames.contains(QStringLiteral("PointRegistration")),
        "plugin_load_policy.json still contains PointRegistration");
    QVERIFY2(!actualNames.contains(QStringLiteral("OpticalRegistration")),
        "plugin_load_policy.json still contains OpticalRegistration");
}

void PluginLoadPolicyCompatibilityResidueContractTest::compatibility_note_describes_projection_boundary()
{
    const QString note = readSource(QStringLiteral("config/plugin_load_policy_compatibility.md"));

    QVERIFY2(note.contains(QStringLiteral("compatibility-only runtime projection")),
        "plugin_load_policy_compatibility.md does not describe the file as a projection");
    QVERIFY2(note.contains(QStringLiteral("descriptor-governed CTK plugin set")),
        "plugin_load_policy_compatibility.md does not describe the reduced projection scope");
    QVERIFY2(note.contains(QStringLiteral("must not define the product mainline")),
        "plugin_load_policy_compatibility.md lost the product-mainline boundary");
}

void PluginLoadPolicyCompatibilityResidueContractTest::compatibility_artifacts_are_explicitly_deployed()
{
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));

    QVERIFY2(rootCMake.contains(QStringLiteral("MEDICALPRO_COMPATIBILITY_CONFIG_FILES")),
        "CMakeLists.txt does not define explicit compatibility config artifacts");
    QVERIFY2(rootCMake.contains(QStringLiteral("plugin_load_policy.json")),
        "CMakeLists.txt does not explicitly name plugin_load_policy.json");
    QVERIFY2(rootCMake.contains(QStringLiteral("plugin_load_policy_compatibility.md")),
        "CMakeLists.txt does not explicitly name plugin_load_policy_compatibility.md");
    QVERIFY2(!rootCMake.contains(
                 QStringLiteral("COMMAND ${CMAKE_COMMAND} -E copy_directory\n            \"${CMAKE_SOURCE_DIR}/config\"")),
        "CMakeLists.txt still copies the whole config directory");
}

QTEST_APPLESS_MAIN(PluginLoadPolicyCompatibilityResidueContractTest)
#include "PluginLoadPolicyCompatibilityResidueContractTest.moc"
