#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "Framework/Platform/Kernel/PlatformDescriptorLoader.h"

class PlatformDescriptorLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void loadFromFile_reads_required_fields();
    void loadFromFile_rejects_legacy_ctk_symbolic_name();
    void loadFromFile_reads_diagnostics_block();
    void loadFromFile_rejects_malformed_diagnostics_block();
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
        "symbolic_name": "DicomViewer",
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
    QCOMPARE(descriptor.runtime.symbolicName, QStringLiteral("DicomViewer"));
    QCOMPARE(descriptor.runtime.startupPolicy, PlatformStartupPolicy::Eager);
    QCOMPARE(descriptor.runtime.bootstrapLevel, PlatformBootstrapLevel::Core);
}

void PlatformDescriptorLoaderTest::loadFromFile_rejects_legacy_ctk_symbolic_name()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath("plugin.json"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
      "id": "org.medicalpro.legacy_viewer",
      "version": "1.0.0",
      "display_name": "LegacyViewer",
      "domain": "imaging",
      "enabled": true,
      "runtime": {
        "ctk_symbolic_name": "LegacyViewer",
        "startup_policy": "eager",
        "bootstrap_level": "core",
        "entry_capability": "imaging.data"
      },
      "provides": {"services": [], "capabilities": ["imaging.data"]},
      "requires": {"services": [], "capabilities": [], "plugins": []},
      "optional": {"services": [], "capabilities": [], "plugins": []},
      "health_checks": ["service_registered"]
    })json");
    file.close();

    QString error;
    const auto descriptor = PlatformDescriptorLoader::loadFromFile(file.fileName(), &error);

    QVERIFY(!error.isEmpty());
    QVERIFY(error.contains(QStringLiteral("runtime.symbolic_name")));
    QVERIFY(descriptor.id.isEmpty());
}

void PlatformDescriptorLoaderTest::loadFromFile_reads_diagnostics_block()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath("plugin.json"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
      "id": "org.medicalpro.lifecycle_viewer",
      "version": "1.0.0",
      "display_name": "LifecycleViewer",
      "domain": "platform",
      "enabled": true,
      "runtime": {
        "symbolic_name": "LifecycleViewer",
        "startup_policy": "on_demand",
        "bootstrap_level": "deferred",
        "entry_capability": "platform.lifecycle"
      },
      "diagnostics": {
        "required_services": ["org.medicalpro.viewer.ReadyService", "org.medicalpro.viewer.CacheService"],
        "service_ready_timeout_ms": 12000,
        "warmup_tasks": ["prime_cache", "hydrate_index"],
        "warmup_timeout_ms": 45000,
        "warmup_impacts_ready": true,
        "degrade_on": ["service_timeout", "warmup_failed"]
      },
      "provides": {"services": [], "capabilities": ["platform.lifecycle"], "plugins": []},
      "requires": {"services": [], "capabilities": [], "plugins": []},
      "optional": {"services": [], "capabilities": [], "plugins": []},
      "health_checks": []
    })json");
    file.close();

    QString error;
    const auto descriptor = PlatformDescriptorLoader::loadFromFile(file.fileName(), &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    const auto expectedRequiredServices =
        QStringList()
        << QStringLiteral("org.medicalpro.viewer.ReadyService")
        << QStringLiteral("org.medicalpro.viewer.CacheService");
    QCOMPARE(
        descriptor.diagnostics.requiredServices,
        expectedRequiredServices);
    QCOMPARE(descriptor.diagnostics.serviceReadyTimeoutMs, 12000);
    const auto expectedWarmupTasks =
        QStringList()
        << QStringLiteral("prime_cache")
        << QStringLiteral("hydrate_index");
    QCOMPARE(
        descriptor.diagnostics.warmupTasks,
        expectedWarmupTasks);
    QCOMPARE(descriptor.diagnostics.warmupTimeoutMs, 45000);
    QVERIFY(descriptor.diagnostics.warmupImpactsReady);
    const auto expectedDegradeOn =
        QStringList()
        << QStringLiteral("service_timeout")
        << QStringLiteral("warmup_failed");
    QCOMPARE(
        descriptor.diagnostics.degradeOn,
        expectedDegradeOn);
}

void PlatformDescriptorLoaderTest::loadFromFile_rejects_malformed_diagnostics_block()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath("plugin.json"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
      "id": "org.medicalpro.bad_diagnostics",
      "version": "1.0.0",
      "display_name": "BadDiagnostics",
      "domain": "platform",
      "enabled": true,
      "runtime": {
        "symbolic_name": "BadDiagnostics",
        "startup_policy": "on_demand",
        "bootstrap_level": "deferred",
        "entry_capability": "platform.lifecycle"
      },
      "diagnostics": {
        "required_services": ["ReadyService", 42],
        "service_ready_timeout_ms": -1,
        "warmup_tasks": "prime_cache",
        "warmup_timeout_ms": "45000",
        "warmup_impacts_ready": "yes",
        "degrade_on": ["service_timeout"]
      },
      "provides": {"services": [], "capabilities": ["platform.lifecycle"], "plugins": []},
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
        "symbolic_name": "BrokenPlugin",
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
