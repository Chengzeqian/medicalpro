#include <QtTest/QtTest>

#include <algorithm>
#include <QFile>
#include <QHash>
#include <QTemporaryDir>

#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"
#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"
#include "Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"
#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"

class PlatformStartupCoordinatorTest : public QObject
{
    Q_OBJECT

private slots:
    void loadFromFile_reads_runtime_mode_and_core_plugin_ids();
    void resolveCorePluginIds_maps_to_symbolic_names();
    void resolveCorePluginIds_rejects_missing_descriptor();
    void facade_mode_runs_only_managed_core_startup_phases();
    void facade_mode_skips_framework_when_no_ctk_runtime_is_required();
    void facade_mode_initializes_framework_when_ctk_runtime_is_required();
    void orchestrate_core_keeps_framework_initialization_even_without_ctk_runtime_requirement();
    void orchestrate_core_runs_all_platform_managed_phases();
    void ensureReady_starts_target_plugin_once();
    void observe_only_does_not_start_any_plugin();
    void startCorePlugin_records_blocking_start_with_platform_plugin_id();
    void startCorePlugin_deduplicates_followup_ensureReady();
    void observe_only_records_skipped_start_event();
    void facade_mode_records_successful_on_demand_start();
    void facade_mode_direct_deferred_start_reports_skip_without_failure();
    void orchestrate_core_mixed_governance_deferred_does_not_fail_for_unmanaged_ctk_plugin();
    void orchestrate_core_records_deferred_start_path();
    void installManagedPlugins_installs_only_planned_entries();
    void installManagedPlugins_skips_platform_hosted_entries_without_bundle_install();
    void waitForServiceReady_returns_timeout_with_missing_dependencies();
    void observe_only_on_demand_activation_reports_skip();
    void facade_mode_on_demand_activation_runs_install_start_ready_and_health_checks();
    void on_demand_activation_skips_bundle_install_for_platform_hosted_entries();
    void orchestrate_core_on_demand_activation_reuses_same_governed_path();
    void on_demand_activation_short_circuits_when_target_is_already_ready();
    void on_demand_activation_fails_when_health_check_fails();
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

void PlatformStartupCoordinatorTest::resolveCorePluginIds_maps_to_symbolic_names()
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
            "symbolic_name": "UserManagement",
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
            "symbolic_name": "DicomViewer",
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
    const auto symbolicNames = config.resolveCoreSymbolicNames(dir.path(), &error);

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
    const auto symbolicNames = config.resolveCoreSymbolicNames(dir.path(), &error);

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

void PlatformStartupCoordinatorTest::facade_mode_skips_framework_when_no_ctk_runtime_is_required()
{
    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::FacadeMode, {});

    QVERIFY(!coordinator.shouldInitializeFramework(false));
}

void PlatformStartupCoordinatorTest::facade_mode_initializes_framework_when_ctk_runtime_is_required()
{
    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::FacadeMode, {});

    QVERIFY(coordinator.shouldInitializeFramework(true));
}

void PlatformStartupCoordinatorTest::orchestrate_core_keeps_framework_initialization_even_without_ctk_runtime_requirement()
{
    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::OrchestrateCore, {});

    QVERIFY(coordinator.shouldInitializeFramework(false));
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
    const QHash<QString, QString> platformPluginIdToCtkSymbolicName{
        {QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore")}
    };
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::FacadeMode,
        [&startedPlugins](const QString& pluginId) {
            startedPlugins.append(pluginId);
            return true;
        },
        platformPluginIdToCtkSymbolicName);

    QVERIFY(coordinator.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QVERIFY(coordinator.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QCOMPARE(startedPlugins, (QStringList{QStringLiteral("RegistrationCore")}));
}

void PlatformStartupCoordinatorTest::observe_only_does_not_start_any_plugin()
{
    QStringList startedPlugins;
    const QHash<QString, QString> platformPluginIdToCtkSymbolicName{
        {QStringLiteral("org.medicalpro.optical_tracking"), QStringLiteral("OpticalTracking")}
    };
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::ObserveOnly,
        [&startedPlugins](const QString& pluginId) {
            startedPlugins.append(pluginId);
            return true;
        },
        platformPluginIdToCtkSymbolicName);

    QVERIFY(!coordinator.ensureReady(QStringLiteral("org.medicalpro.optical_tracking")));
    QVERIFY(startedPlugins.isEmpty());
}

void PlatformStartupCoordinatorTest::startCorePlugin_records_blocking_start_with_platform_plugin_id()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::OrchestrateCore);

    QStringList startedPlugins;
    const QHash<QString, QString> platformPluginIdToCtkSymbolicName{
        {QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore")}
    };
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::OrchestrateCore,
        [&startedPlugins](const QString& ctkSymbolicName) {
            startedPlugins.append(ctkSymbolicName);
            return true;
        },
        platformPluginIdToCtkSymbolicName,
        &recorder);

    QVERIFY(coordinator.startCorePlugin(QStringLiteral("org.medicalpro.registration_core")));

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();
    QCOMPARE(startedPlugins, (QStringList{QStringLiteral("RegistrationCore")}));
    QVERIFY(events.size() >= 3);
    QCOMPARE(events.at(events.size() - 2).kind, PlatformLifecycleEventKind::PluginStartStarted);
    QVERIFY(events.at(events.size() - 2).blockingStartup);
    QCOMPARE(events.constLast().kind, PlatformLifecycleEventKind::PluginStartFinished);
    QVERIFY(events.constLast().blockingStartup);
    QCOMPARE(events.constLast().pluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(events.constLast().symbolicName, QStringLiteral("RegistrationCore"));
    QCOMPARE(events.constLast().reasonCode, QStringLiteral("core"));
    QCOMPARE(trace.size(), 1);
    QVERIFY(trace.constFirst().blockingStartup);
    QCOMPARE(trace.constFirst().pluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(trace.constFirst().symbolicName, QStringLiteral("RegistrationCore"));
}

void PlatformStartupCoordinatorTest::startCorePlugin_deduplicates_followup_ensureReady()
{
    QStringList startedPlugins;
    const QHash<QString, QString> platformPluginIdToCtkSymbolicName{
        {QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore")}
    };
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::OrchestrateCore,
        [&startedPlugins](const QString& ctkSymbolicName) {
            startedPlugins.append(ctkSymbolicName);
            return true;
        },
        platformPluginIdToCtkSymbolicName);

    QVERIFY(coordinator.startCorePlugin(QStringLiteral("org.medicalpro.registration_core")));
    QVERIFY(coordinator.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QCOMPARE(startedPlugins, (QStringList{QStringLiteral("RegistrationCore")}));
}

void PlatformStartupCoordinatorTest::observe_only_records_skipped_start_event()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::ObserveOnly);
    const QHash<QString, QString> platformPluginIdToCtkSymbolicName{
        {QStringLiteral("org.medicalpro.optical_tracking"), QStringLiteral("OpticalTracking")}
    };

    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::ObserveOnly,
        [](const QString&) { return true; },
        platformPluginIdToCtkSymbolicName,
        &recorder);

    QVERIFY(!coordinator.ensureReady(QStringLiteral("org.medicalpro.optical_tracking")));

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();
    QVERIFY(events.size() >= 3);
    QCOMPARE(events.at(events.size() - 2).kind, PlatformLifecycleEventKind::PluginStartStarted);
    QCOMPARE(events.constLast().kind, PlatformLifecycleEventKind::PluginSkippedByMode);
    QCOMPARE(events.constLast().result, PlatformLifecycleResult::Skipped);
    QCOMPARE(events.constLast().step, PlatformLifecycleStep::Start);
    QCOMPARE(events.constLast().pluginId, QStringLiteral("org.medicalpro.optical_tracking"));
    QCOMPARE(events.constLast().symbolicName, QStringLiteral("OpticalTracking"));
    QCOMPARE(events.constLast().runtimeMode, PlatformRuntimeMode::ObserveOnly);
    QCOMPARE(events.constLast().reasonCode, QStringLiteral("skipped_by_mode"));
    QCOMPARE(events.constLast().detail, QStringLiteral("On-demand plugin start skipped in observe_only mode"));
    QVERIFY(!events.constLast().blockingStartup);
    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().result, PlatformLifecycleResult::Skipped);
    QCOMPARE(trace.constFirst().symbolicName, QStringLiteral("OpticalTracking"));
    QCOMPARE(trace.constFirst().reasonCode, QStringLiteral("skipped_by_mode"));
    QCOMPARE(trace.constFirst().detail, QStringLiteral("On-demand plugin start skipped in observe_only mode"));
    QVERIFY(!trace.constFirst().blockingStartup);
}

void PlatformStartupCoordinatorTest::facade_mode_records_successful_on_demand_start()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::FacadeMode);

    QStringList startedPlugins;
    const QHash<QString, QString> platformPluginIdToCtkSymbolicName{
        {QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore")}
    };
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::FacadeMode,
        [&startedPlugins](const QString& ctkSymbolicName) {
            startedPlugins.append(ctkSymbolicName);
            return true;
        },
        platformPluginIdToCtkSymbolicName,
        &recorder);

    QVERIFY(coordinator.ensureReady(QStringLiteral("org.medicalpro.registration_core")));

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();
    QCOMPARE(startedPlugins, (QStringList{QStringLiteral("RegistrationCore")}));
    QVERIFY(events.size() >= 3);
    QCOMPARE(events.at(events.size() - 2).kind, PlatformLifecycleEventKind::PluginStartStarted);
    QCOMPARE(events.constLast().kind, PlatformLifecycleEventKind::PluginStartFinished);
    QCOMPARE(events.constLast().result, PlatformLifecycleResult::Succeeded);
    QCOMPARE(events.constLast().pluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(events.constLast().symbolicName, QStringLiteral("RegistrationCore"));
    QCOMPARE(events.constLast().runtimeMode, PlatformRuntimeMode::FacadeMode);
    QCOMPARE(events.constLast().reasonCode, QStringLiteral("on_demand"));
    QCOMPARE(events.constLast().detail, QStringLiteral("On-demand plugin start completed"));
    QVERIFY(!events.at(events.size() - 2).blockingStartup);
    QVERIFY(!events.constLast().blockingStartup);
    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().pluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(trace.constFirst().symbolicName, QStringLiteral("RegistrationCore"));
    QCOMPARE(trace.constFirst().result, PlatformLifecycleResult::Succeeded);
    QCOMPARE(trace.constFirst().reasonCode, QStringLiteral("on_demand"));
    QCOMPARE(trace.constFirst().detail, QStringLiteral("On-demand plugin start completed"));
    QVERIFY(!trace.constFirst().blockingStartup);
}

void PlatformStartupCoordinatorTest::facade_mode_direct_deferred_start_reports_skip_without_failure()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::FacadeMode);

    QStringList startedPlugins;
    const QHash<QString, QString> platformPluginIdToCtkSymbolicName{
        {QStringLiteral("org.medicalpro.deferred_navigation"), QStringLiteral("DeferredNavigation")}
    };
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::FacadeMode,
        [&startedPlugins](const QString& ctkSymbolicName) {
            startedPlugins.append(ctkSymbolicName);
            return true;
        },
        platformPluginIdToCtkSymbolicName,
        &recorder);

    QVERIFY(coordinator.startDeferredPlugins(QStringList{
        QStringLiteral("DeferredNavigation")
    }));

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();
    QVERIFY(startedPlugins.isEmpty());
    QVERIFY(events.size() >= 3);
    QCOMPARE(events.constLast().kind, PlatformLifecycleEventKind::PluginSkippedByMode);
    QCOMPARE(events.constLast().result, PlatformLifecycleResult::Skipped);
    QCOMPARE(events.constLast().pluginId, QStringLiteral("org.medicalpro.deferred_navigation"));
    QCOMPARE(events.constLast().symbolicName, QStringLiteral("DeferredNavigation"));
    QCOMPARE(events.constLast().reasonCode, QStringLiteral("skipped_by_mode"));
    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().result, PlatformLifecycleResult::Skipped);
    QCOMPARE(trace.constFirst().pluginId, QStringLiteral("org.medicalpro.deferred_navigation"));
    QCOMPARE(trace.constFirst().symbolicName, QStringLiteral("DeferredNavigation"));
}

void PlatformStartupCoordinatorTest::orchestrate_core_mixed_governance_deferred_does_not_fail_for_unmanaged_ctk_plugin()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::OrchestrateCore);

    QStringList startedPlugins;
    const QHash<QString, QString> platformPluginIdToCtkSymbolicName{
        {QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore")}
    };
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::OrchestrateCore,
        [&startedPlugins](const QString& ctkSymbolicName) {
            startedPlugins.append(ctkSymbolicName);
            return true;
        },
        platformPluginIdToCtkSymbolicName,
        &recorder);

    QVERIFY(coordinator.startDeferredPlugins(QStringList{
        QStringLiteral("RegistrationCore"),
        QStringLiteral("PointRegistration")
    }));

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();
    QCOMPARE(startedPlugins, (QStringList{
        QStringLiteral("RegistrationCore"),
        QStringLiteral("PointRegistration")
    }));
    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().pluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(trace.constFirst().symbolicName, QStringLiteral("RegistrationCore"));
    QCOMPARE(trace.constFirst().reasonCode, QStringLiteral("deferred"));
    QVERIFY(std::none_of(events.cbegin(), events.cend(), [](const PlatformLifecycleEvent& event) {
        return event.symbolicName == QStringLiteral("PointRegistration");
    }));
}

void PlatformStartupCoordinatorTest::orchestrate_core_records_deferred_start_path()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::OrchestrateCore);

    QStringList startedPlugins;
    const QHash<QString, QString> platformPluginIdToCtkSymbolicName{
        {QStringLiteral("org.medicalpro.deferred_navigation"), QStringLiteral("DeferredNavigation")},
        {QStringLiteral("org.medicalpro.registration_core"), QStringLiteral("RegistrationCore")}
    };
    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::OrchestrateCore,
        [&startedPlugins](const QString& ctkSymbolicName) {
            startedPlugins.append(ctkSymbolicName);
            return true;
        },
        platformPluginIdToCtkSymbolicName,
        &recorder);

    QVERIFY(coordinator.startDeferredPlugins(QStringList{
        QStringLiteral("DeferredNavigation")
    }));

    const auto events = recorder.lifecycleEvents();
    const auto trace = recorder.startupTrace();
    QCOMPARE(startedPlugins, (QStringList{QStringLiteral("DeferredNavigation")}));
    QVERIFY(events.size() >= 3);
    QCOMPARE(events.at(events.size() - 2).kind, PlatformLifecycleEventKind::PluginStartStarted);
    QCOMPARE(events.constLast().kind, PlatformLifecycleEventKind::PluginStartFinished);
    QCOMPARE(events.constLast().result, PlatformLifecycleResult::Succeeded);
    QCOMPARE(events.constLast().pluginId, QStringLiteral("org.medicalpro.deferred_navigation"));
    QCOMPARE(events.constLast().symbolicName, QStringLiteral("DeferredNavigation"));
    QCOMPARE(events.constLast().runtimeMode, PlatformRuntimeMode::OrchestrateCore);
    QCOMPARE(events.constLast().reasonCode, QStringLiteral("deferred"));
    QCOMPARE(events.constLast().detail, QStringLiteral("Deferred plugin start completed"));
    QVERIFY(!events.at(events.size() - 2).blockingStartup);
    QVERIFY(!events.constLast().blockingStartup);
    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().pluginId, QStringLiteral("org.medicalpro.deferred_navigation"));
    QCOMPARE(trace.constFirst().symbolicName, QStringLiteral("DeferredNavigation"));
    QCOMPARE(trace.constFirst().result, PlatformLifecycleResult::Succeeded);
    QCOMPARE(trace.constFirst().reasonCode, QStringLiteral("deferred"));
    QCOMPARE(trace.constFirst().detail, QStringLiteral("Deferred plugin start completed"));
    QVERIFY(!trace.constFirst().blockingStartup);
}

void PlatformStartupCoordinatorTest::installManagedPlugins_installs_only_planned_entries()
{
    QStringList installedBundles;
    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::FacadeMode, {});

    PlatformManagedPluginPlan plan;
    PlatformManagedPluginPlanEntry user;
    user.pluginId = QStringLiteral("org.medicalpro.user_management");
    user.symbolicName = QStringLiteral("UserManagement");
    user.bundleFilePath = QStringLiteral("C:/runtime/plugins/UserManagement.dll");

    PlatformManagedPluginPlanEntry dicom;
    dicom.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    dicom.symbolicName = QStringLiteral("DicomViewer");
    dicom.bundleFilePath = QStringLiteral("C:/runtime/plugins/DicomViewer.dll");

    plan.installEntries = {user, dicom};

    QVERIFY(coordinator.installManagedPlugins(plan, [&installedBundles](const PlatformManagedPluginPlanEntry& entry) {
        installedBundles.append(entry.bundleFilePath);
        return true;
    }));

    QCOMPARE(installedBundles, (QStringList{
        QStringLiteral("C:/runtime/plugins/UserManagement.dll"),
        QStringLiteral("C:/runtime/plugins/DicomViewer.dll")
    }));
}

void PlatformStartupCoordinatorTest::installManagedPlugins_skips_platform_hosted_entries_without_bundle_install()
{
    QStringList installedBundles;
    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::FacadeMode, {});

    PlatformManagedPluginPlanEntry user;
    user.pluginId = QStringLiteral("org.medicalpro.user_management");
    user.symbolicName = QStringLiteral("UserManagement");
    user.requiresBundleInstall = false;

    PlatformManagedPluginPlanEntry dicom;
    dicom.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    dicom.symbolicName = QStringLiteral("DicomViewer");
    dicom.bundleFilePath = QStringLiteral("C:/runtime/plugins/DicomViewer.dll");

    PlatformManagedPluginPlan plan;
    plan.installEntries = {user, dicom};

    QVERIFY(coordinator.installManagedPlugins(plan, [&installedBundles](const PlatformManagedPluginPlanEntry& entry) {
        installedBundles.append(entry.bundleFilePath);
        return true;
    }));

    QCOMPARE(installedBundles, QStringList{QStringLiteral("C:/runtime/plugins/DicomViewer.dll")});
}

void PlatformStartupCoordinatorTest::waitForServiceReady_returns_timeout_with_missing_dependencies()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::FacadeMode);

    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::FacadeMode,
        [](const QString&) { return true; },
        {},
        &recorder);

    PlatformManagedPluginPlanEntry entry;
    entry.pluginId = QStringLiteral("org.medicalpro.four_view_display");
    entry.symbolicName = QStringLiteral("FourViewDisplay");
    entry.requiredServices = QStringList{QStringLiteral("imaging.viewport")};
    entry.serviceReadyTimeoutMs = 100;

    const auto outcome = coordinator.waitForServiceReady(
        entry,
        {
            [](const QStringList&) { return QStringList{QStringLiteral("imaging.viewport")}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; }
        },
        10);

    QVERIFY(!outcome.success);
    QCOMPARE(outcome.reasonCode, QStringLiteral("service_ready_timeout"));
}

void PlatformStartupCoordinatorTest::observe_only_on_demand_activation_reports_skip()
{
    PlatformLifecycleTraceRecorder recorder;
    recorder.beginSession(PlatformRuntimeMode::ObserveOnly);

    PlatformOnDemandActivationPlan plan;
    plan.targetPluginId = QStringLiteral("org.medicalpro.registration_core");
    plan.activationEntries = {
        {
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("C:/runtime/plugins/RegistrationCore.dll"),
            {},
            {QStringLiteral("imaging.data")},
            {QStringLiteral("RegistrationService")},
            {QStringLiteral("service_registered"), QStringLiteral("core_binary_accessible")},
            5000,
            true
        }
    };

    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::ObserveOnly, {}, {}, &recorder);
    const auto outcome = coordinator.activateOnDemand(
        plan,
        {},
        {
            [](const QString&) { return PlatformPluginState::Discovered; },
            [](const QStringList&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&, const QStringList&) { return QVector<PlatformHealthCheckResult>{}; }
        });

    QVERIFY(!outcome.success);
    QCOMPARE(outcome.reasonCode, QStringLiteral("skipped_by_mode"));
}

void PlatformStartupCoordinatorTest::facade_mode_on_demand_activation_runs_install_start_ready_and_health_checks()
{
    QStringList installedPlugins;
    QStringList startedPlugins;
    QStringList healthCheckTargets;

    PlatformOnDemandActivationPlan plan;
    plan.targetPluginId = QStringLiteral("org.medicalpro.registration_core");
    plan.activationEntries = {
        {
            QStringLiteral("org.medicalpro.registration_support"),
            QStringLiteral("RegistrationSupport"),
            QStringLiteral("RegistrationSupport"),
            QStringLiteral("C:/runtime/plugins/RegistrationSupport.dll"),
            {},
            {},
            {QStringLiteral("RegistrationSupportService")},
            {QStringLiteral("service_registered")},
            5000,
            false
        },
        {
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("C:/runtime/plugins/RegistrationCore.dll"),
            {QStringLiteral("org.medicalpro.registration_support")},
            {QStringLiteral("imaging.data")},
            {QStringLiteral("RegistrationService")},
            {QStringLiteral("service_registered"), QStringLiteral("core_binary_accessible")},
            5000,
            true
        }
    };

    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::FacadeMode,
        [&startedPlugins](const QString& ctkSymbolicName) {
            startedPlugins.append(ctkSymbolicName);
            return true;
        });

    const auto outcome = coordinator.activateOnDemand(
        plan,
        [&installedPlugins](const PlatformOnDemandActivationPlanEntry& entry) {
            installedPlugins.append(entry.pluginId);
            return true;
        },
        {
            [](const QString&) { return PlatformPluginState::Discovered; },
            [](const QStringList&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [&healthCheckTargets](const QString& pluginId, const QStringList& healthChecks) {
                healthCheckTargets.append(pluginId);
                QVector<PlatformHealthCheckResult> results;
                for (const auto& healthCheck : healthChecks) {
                    PlatformHealthCheckResult result;
                    result.name = healthCheck;
                    result.passed = true;
                    results.append(result);
                }
                return results;
            }
        });

    QVERIFY(outcome.success);
    QCOMPARE(outcome.reasonCode, QStringLiteral("service_ready"));
    QCOMPARE(installedPlugins, (QStringList{
        QStringLiteral("org.medicalpro.registration_support"),
        QStringLiteral("org.medicalpro.registration_core")
    }));
    QCOMPARE(startedPlugins, (QStringList{
        QStringLiteral("RegistrationSupport"),
        QStringLiteral("RegistrationCore")
    }));
    QCOMPARE(healthCheckTargets, (QStringList{QStringLiteral("org.medicalpro.registration_core")}));
}

void PlatformStartupCoordinatorTest::on_demand_activation_skips_bundle_install_for_platform_hosted_entries()
{
    QStringList installedPlugins;
    QStringList startedPlugins;

    PlatformOnDemandActivationPlan plan;
    plan.targetPluginId = QStringLiteral("org.medicalpro.registration_core");
    plan.activationEntries = {
        {
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("RegistrationCore"),
            QString {},
            {},
            {},
            {QStringLiteral("RegistrationService")},
            {QStringLiteral("service_registered")},
            5000,
            true
        }
    };
    plan.activationEntries[0].requiresBundleInstall = false;

    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::FacadeMode,
        [&startedPlugins](const QString& ctkSymbolicName) {
            startedPlugins.append(ctkSymbolicName);
            return true;
        });

    const auto outcome = coordinator.activateOnDemand(
        plan,
        [&installedPlugins](const PlatformOnDemandActivationPlanEntry& entry) {
            installedPlugins.append(entry.pluginId);
            return true;
        },
        {
            [](const QString&) { return PlatformPluginState::Discovered; },
            [](const QStringList&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&, const QStringList& healthChecks) {
                QVector<PlatformHealthCheckResult> results;
                for (const auto& healthCheck : healthChecks) {
                    PlatformHealthCheckResult result;
                    result.name = healthCheck;
                    result.passed = true;
                    results.append(result);
                }
                return results;
            }
        });

    QVERIFY(outcome.success);
    QVERIFY(installedPlugins.isEmpty());
    QCOMPARE(startedPlugins, QStringList{QStringLiteral("RegistrationCore")});
}

void PlatformStartupCoordinatorTest::orchestrate_core_on_demand_activation_reuses_same_governed_path()
{
    QStringList installedPlugins;
    QStringList startedPlugins;

    PlatformOnDemandActivationPlan plan;
    plan.targetPluginId = QStringLiteral("org.medicalpro.optical_tracking");
    plan.activationEntries = {
        {
            QStringLiteral("org.medicalpro.optical_tracking"),
            QStringLiteral("OpticalTracking"),
            QStringLiteral("OpticalTracking"),
            QStringLiteral("C:/runtime/plugins/OpticalTracking.dll"),
            {},
            {},
            {QStringLiteral("OpticalTrackingService")},
            {QStringLiteral("service_registered"), QStringLiteral("tracking_adapter_accessible")},
            5000,
            true
        }
    };

    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::OrchestrateCore,
        [&startedPlugins](const QString& ctkSymbolicName) {
            startedPlugins.append(ctkSymbolicName);
            return true;
        });

    const auto outcome = coordinator.activateOnDemand(
        plan,
        [&installedPlugins](const PlatformOnDemandActivationPlanEntry& entry) {
            installedPlugins.append(entry.pluginId);
            return true;
        },
        {
            [](const QString&) { return PlatformPluginState::Discovered; },
            [](const QStringList&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&, const QStringList& healthChecks) {
                QVector<PlatformHealthCheckResult> results;
                for (const auto& healthCheck : healthChecks) {
                    PlatformHealthCheckResult result;
                    result.name = healthCheck;
                    result.passed = true;
                    results.append(result);
                }
                return results;
            }
        });

    QVERIFY(outcome.success);
    QCOMPARE(outcome.reasonCode, QStringLiteral("service_ready"));
    QCOMPARE(installedPlugins, (QStringList{QStringLiteral("org.medicalpro.optical_tracking")}));
    QCOMPARE(startedPlugins, (QStringList{QStringLiteral("OpticalTracking")}));
}

void PlatformStartupCoordinatorTest::on_demand_activation_short_circuits_when_target_is_already_ready()
{
    PlatformOnDemandActivationPlan plan;
    plan.targetPluginId = QStringLiteral("org.medicalpro.registration_core");
    plan.activationEntries = {
        {
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("C:/runtime/plugins/RegistrationCore.dll"),
            {},
            {},
            {QStringLiteral("RegistrationService")},
            {QStringLiteral("service_registered")},
            5000,
            true
        }
    };

    PlatformStartupCoordinator coordinator(PlatformRuntimeMode::FacadeMode, {}, {});
    const auto outcome = coordinator.activateOnDemand(
        plan,
        {},
        {
            [](const QString&) { return PlatformPluginState::Ready; },
            [](const QStringList&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&, const QStringList&) { return QVector<PlatformHealthCheckResult>{}; }
        });

    QVERIFY(outcome.success);
    QCOMPARE(outcome.reasonCode, QStringLiteral("already_ready"));
}

void PlatformStartupCoordinatorTest::on_demand_activation_fails_when_health_check_fails()
{
    PlatformOnDemandActivationPlan plan;
    plan.targetPluginId = QStringLiteral("org.medicalpro.registration_core");
    plan.activationEntries = {
        {
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("RegistrationCore"),
            QStringLiteral("C:/runtime/plugins/RegistrationCore.dll"),
            {},
            {},
            {QStringLiteral("RegistrationService")},
            {QStringLiteral("service_registered"), QStringLiteral("core_binary_accessible")},
            5000,
            true
        }
    };

    PlatformStartupCoordinator coordinator(
        PlatformRuntimeMode::FacadeMode,
        [](const QString&) { return true; },
        {});

    const auto outcome = coordinator.activateOnDemand(
        plan,
        [](const PlatformOnDemandActivationPlanEntry&) { return true; },
        {
            [](const QString&) { return PlatformPluginState::Discovered; },
            [](const QStringList&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&) { return QStringList{}; },
            [](const QString&, const QStringList& healthChecks) {
                QVector<PlatformHealthCheckResult> results;
                for (const auto& healthCheck : healthChecks) {
                    PlatformHealthCheckResult result;
                    result.name = healthCheck;
                    result.passed = healthCheck == QStringLiteral("service_registered");
                    result.detail = result.passed
                        ? QStringLiteral("ok")
                        : QStringLiteral("core binary is not accessible");
                    results.append(result);
                }
                return results;
            }
        });

    QVERIFY(!outcome.success);
    QCOMPARE(outcome.reasonCode, QStringLiteral("health_check_failed"));
    QCOMPARE(outcome.finalState, PlatformPluginState::Failed);
}

QTEST_APPLESS_MAIN(PlatformStartupCoordinatorTest)
#include "PlatformStartupCoordinatorTest.moc"
