#include <QtTest/QtTest>

#include <algorithm>
#include <QFile>
#include <QHash>
#include <QTemporaryDir>

#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"
#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"
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
    void startCorePlugin_records_blocking_start_with_platform_plugin_id();
    void startCorePlugin_deduplicates_followup_ensureReady();
    void observe_only_records_skipped_start_event();
    void facade_mode_records_successful_on_demand_start();
    void facade_mode_direct_deferred_start_reports_skip_without_failure();
    void orchestrate_core_mixed_governance_deferred_does_not_fail_for_unmanaged_ctk_plugin();
    void orchestrate_core_records_deferred_start_path();
    void installManagedPlugins_installs_only_planned_entries();
    void waitForServiceReady_returns_timeout_with_missing_dependencies();
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
    QCOMPARE(events.constLast().ctkSymbolicName, QStringLiteral("RegistrationCore"));
    QCOMPARE(events.constLast().reasonCode, QStringLiteral("core"));
    QCOMPARE(trace.size(), 1);
    QVERIFY(trace.constFirst().blockingStartup);
    QCOMPARE(trace.constFirst().pluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(trace.constFirst().ctkSymbolicName, QStringLiteral("RegistrationCore"));
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
    QCOMPARE(events.constLast().ctkSymbolicName, QStringLiteral("OpticalTracking"));
    QCOMPARE(events.constLast().runtimeMode, PlatformRuntimeMode::ObserveOnly);
    QCOMPARE(events.constLast().reasonCode, QStringLiteral("skipped_by_mode"));
    QCOMPARE(events.constLast().detail, QStringLiteral("On-demand plugin start skipped in observe_only mode"));
    QVERIFY(!events.constLast().blockingStartup);
    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().result, PlatformLifecycleResult::Skipped);
    QCOMPARE(trace.constFirst().ctkSymbolicName, QStringLiteral("OpticalTracking"));
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
    QCOMPARE(events.constLast().ctkSymbolicName, QStringLiteral("RegistrationCore"));
    QCOMPARE(events.constLast().runtimeMode, PlatformRuntimeMode::FacadeMode);
    QCOMPARE(events.constLast().reasonCode, QStringLiteral("on_demand"));
    QCOMPARE(events.constLast().detail, QStringLiteral("On-demand plugin start completed"));
    QVERIFY(!events.at(events.size() - 2).blockingStartup);
    QVERIFY(!events.constLast().blockingStartup);
    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().pluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(trace.constFirst().ctkSymbolicName, QStringLiteral("RegistrationCore"));
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
    QCOMPARE(events.constLast().ctkSymbolicName, QStringLiteral("DeferredNavigation"));
    QCOMPARE(events.constLast().reasonCode, QStringLiteral("skipped_by_mode"));
    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().result, PlatformLifecycleResult::Skipped);
    QCOMPARE(trace.constFirst().pluginId, QStringLiteral("org.medicalpro.deferred_navigation"));
    QCOMPARE(trace.constFirst().ctkSymbolicName, QStringLiteral("DeferredNavigation"));
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
    QCOMPARE(trace.constFirst().ctkSymbolicName, QStringLiteral("RegistrationCore"));
    QCOMPARE(trace.constFirst().reasonCode, QStringLiteral("deferred"));
    QVERIFY(std::none_of(events.cbegin(), events.cend(), [](const PlatformLifecycleEvent& event) {
        return event.ctkSymbolicName == QStringLiteral("PointRegistration");
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
    QCOMPARE(events.constLast().ctkSymbolicName, QStringLiteral("DeferredNavigation"));
    QCOMPARE(events.constLast().runtimeMode, PlatformRuntimeMode::OrchestrateCore);
    QCOMPARE(events.constLast().reasonCode, QStringLiteral("deferred"));
    QCOMPARE(events.constLast().detail, QStringLiteral("Deferred plugin start completed"));
    QVERIFY(!events.at(events.size() - 2).blockingStartup);
    QVERIFY(!events.constLast().blockingStartup);
    QCOMPARE(trace.size(), 1);
    QCOMPARE(trace.constFirst().pluginId, QStringLiteral("org.medicalpro.deferred_navigation"));
    QCOMPARE(trace.constFirst().ctkSymbolicName, QStringLiteral("DeferredNavigation"));
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
    user.ctkSymbolicName = QStringLiteral("UserManagement");
    user.bundleFilePath = QStringLiteral("C:/runtime/plugins/UserManagement.dll");

    PlatformManagedPluginPlanEntry dicom;
    dicom.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    dicom.ctkSymbolicName = QStringLiteral("DicomViewer");
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
    entry.ctkSymbolicName = QStringLiteral("FourViewDisplay");
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

QTEST_APPLESS_MAIN(PlatformStartupCoordinatorTest)
#include "PlatformStartupCoordinatorTest.moc"
