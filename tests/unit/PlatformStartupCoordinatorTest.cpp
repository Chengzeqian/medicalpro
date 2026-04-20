#include <QtTest/QtTest>

#include <QFile>
#include <QTemporaryDir>

#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"
#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"

class PlatformStartupCoordinatorTest : public QObject
{
    Q_OBJECT

private slots:
    void loadFromFile_reads_runtime_mode_and_core_plugin_ids();
    void resolveCorePluginIds_maps_to_ctk_symbolic_names();
    void resolveCorePluginIds_rejects_missing_descriptor();
    void facade_mode_runs_only_managed_core_startup_phases();
    void orchestrate_core_runs_all_platform_managed_phases();
    void ensureReady_starts_target_plugin_once();
    void observe_only_does_not_start_any_plugin();
};

void PlatformStartupCoordinatorTest::loadFromFile_reads_runtime_mode_and_core_plugin_ids()
{
    QTemporaryDir dir;
    QFile file(dir.filePath(QStringLiteral("platform_runtime.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
      "runtime_mode": "facade_mode",
      "descriptor_directory": "plugins/descriptors",
      "core_plugin_ids": [
        "org.medicalpro.user_management",
        "org.medicalpro.dicom_viewer",
        "org.medicalpro.four_view_display"
      ]
    })json");
    file.close();

    QString error;
    const auto config = PlatformRuntimeConfig::loadFromFile(file.fileName(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(config.runtimeMode, PlatformRuntimeMode::FacadeMode);
    QCOMPARE(config.corePluginIds.size(), 3);
}

void PlatformStartupCoordinatorTest::resolveCorePluginIds_maps_to_ctk_symbolic_names()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto writeDescriptor = [&dir](const QString& fileName, const QByteArray& content) {
        QFile file(dir.filePath(fileName));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QCOMPARE(file.write(content), content.size());
        file.close();
    };

    writeDescriptor(
        QStringLiteral("user_management.json"),
        R"json({
          "id": "org.medicalpro.user_management",
          "version": "1.0.0",
          "display_name": "UserManagement",
          "domain": "identity",
          "enabled": true,
          "runtime": {
            "ctk_symbolic_name": "UserManagement",
            "startup_policy": "eager",
            "bootstrap_level": "core",
            "entry_capability": "identity.core"
          },
          "provides": {"services": [], "capabilities": [], "plugins": []},
          "requires": {"services": [], "capabilities": [], "plugins": []},
          "optional": {"services": [], "capabilities": [], "plugins": []},
          "health_checks": []
        })json");

    writeDescriptor(
        QStringLiteral("dicom_viewer.json"),
        R"json({
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
          "provides": {"services": [], "capabilities": [], "plugins": []},
          "requires": {"services": [], "capabilities": [], "plugins": []},
          "optional": {"services": [], "capabilities": [], "plugins": []},
          "health_checks": []
        })json");

    PlatformRuntimeConfig config;
    config.corePluginIds = QStringList{
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("org.medicalpro.dicom_viewer")
    };

    QString error;
    const auto symbolicNames = config.resolveCoreCtkPluginNames(dir.path(), &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(
        symbolicNames,
        (QStringList{
            QStringLiteral("UserManagement"),
            QStringLiteral("DicomViewer")
        }));
}

void PlatformStartupCoordinatorTest::resolveCorePluginIds_rejects_missing_descriptor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PlatformRuntimeConfig config;
    config.corePluginIds = QStringList{
        QStringLiteral("org.medicalpro.user_management")
    };

    QString error;
    const auto symbolicNames = config.resolveCoreCtkPluginNames(dir.path(), &error);

    QVERIFY(symbolicNames.isEmpty());
    QVERIFY(error.contains(QStringLiteral("org.medicalpro.user_management")));
}

void PlatformStartupCoordinatorTest::facade_mode_runs_only_managed_core_startup_phases()
{
    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::FacadeMode, {});

    QVERIFY(coordinator.shouldInitializeFramework());
    QVERIFY(coordinator.shouldInstallPlugins());
    QVERIFY(coordinator.shouldStartCorePlugins());
    QVERIFY(!coordinator.shouldStartDeferredPlugins());
    QVERIFY(!coordinator.shouldWarmupServices());
}

void PlatformStartupCoordinatorTest::orchestrate_core_runs_all_platform_managed_phases()
{
    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::OrchestrateCore, {});

    QVERIFY(coordinator.shouldInitializeFramework());
    QVERIFY(coordinator.shouldInstallPlugins());
    QVERIFY(coordinator.shouldStartCorePlugins());
    QVERIFY(coordinator.shouldStartDeferredPlugins());
    QVERIFY(coordinator.shouldWarmupServices());
}

void PlatformStartupCoordinatorTest::ensureReady_starts_target_plugin_once()
{
    QStringList startedPlugins;
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::FacadeMode,
        [&startedPlugins](const QString& pluginId) {
            startedPlugins.append(pluginId);
            return true;
        });

    QVERIFY(coordinator.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QVERIFY(coordinator.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QCOMPARE(startedPlugins, (QStringList{QStringLiteral("org.medicalpro.registration_core")}));
}

void PlatformStartupCoordinatorTest::observe_only_does_not_start_any_plugin()
{
    QStringList startedPlugins;
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::ObserveOnly,
        [&startedPlugins](const QString& pluginId) {
            startedPlugins.append(pluginId);
            return true;
        });

    QVERIFY(!coordinator.ensureReady(QStringLiteral("org.medicalpro.optical_tracking")));
    QVERIFY(startedPlugins.isEmpty());
}

QTEST_APPLESS_MAIN(PlatformStartupCoordinatorTest)
#include "PlatformStartupCoordinatorTest.moc"
