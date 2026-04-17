#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "Framework/Platform/Kernel/PlatformDescriptorLoader.h"

class PlatformDescriptorLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void loadFromFile_reads_required_fields();
    void loadFromFile_rejects_missing_startup_policy();
};

void PlatformDescriptorLoaderTest::loadFromFile_reads_required_fields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath("plugin.json"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
      "id": "org.medicalpro.dicom_viewer",
      "version": "1.0.0",
      "display_name": "DicomViewer",
      "domain": "imaging",
      "enabled": true,
      "runtime": {
        "ctk_symbolic_name": "DicomViewer",
        "startup_policy": "eager",
        "bootstrap_level": "core",
        "entry_capability": "imaging.data"
      },
      "provides": {"services": ["imaging.study_query"], "capabilities": ["imaging.data"]},
      "requires": {"services": [], "capabilities": [], "plugins": []},
      "optional": {"services": [], "capabilities": [], "plugins": []},
      "health_checks": ["service_registered", "data_path_accessible"]
    })json");
    file.close();

    QString error;
    const auto descriptor = PlatformDescriptorLoader::loadFromFile(file.fileName(), &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(descriptor.id, QStringLiteral("org.medicalpro.dicom_viewer"));
    QCOMPARE(descriptor.runtime.ctkSymbolicName, QStringLiteral("DicomViewer"));
    QCOMPARE(descriptor.runtime.startupPolicy, PlatformStartupPolicy::Eager);
    QCOMPARE(descriptor.runtime.bootstrapLevel, PlatformBootstrapLevel::Core);
}

void PlatformDescriptorLoaderTest::loadFromFile_rejects_missing_startup_policy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath("plugin.json"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
      "id": "org.medicalpro.bad_plugin",
      "version": "1.0.0",
      "display_name": "BrokenPlugin",
      "domain": "core",
      "enabled": true,
      "runtime": {
        "ctk_symbolic_name": "BrokenPlugin",
        "bootstrap_level": "core",
        "entry_capability": "broken.capability"
      },
      "provides": {"services": [], "capabilities": []},
      "requires": {"services": [], "capabilities": [], "plugins": []},
      "optional": {"services": [], "capabilities": [], "plugins": []},
      "health_checks": []
    })json");
    file.close();

    QString error;
    const auto descriptor = PlatformDescriptorLoader::loadFromFile(file.fileName(), &error);

    QVERIFY(!error.isEmpty());
    QVERIFY(descriptor.id.isEmpty());
}

QTEST_APPLESS_MAIN(PlatformDescriptorLoaderTest)
#include "PlatformDescriptorLoaderTest.moc"
