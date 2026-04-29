#include <QtTest/QtTest>

#include <QFile>

class RuntimeHostDetachmentContractTest : public QObject
{
    Q_OBJECT

private slots:
    void product_entry_and_mainwindow_do_not_depend_on_ctk_bridge_types();
    void legacy_platform_adapters_and_snapshots_do_not_depend_on_ctk_manager();

private:
    QString readSource(const QString& relativePath) const;
};

QString RuntimeHostDetachmentContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

void RuntimeHostDetachmentContractTest::product_entry_and_mainwindow_do_not_depend_on_ctk_bridge_types()
{
    const QString mainSource = readSource(QStringLiteral("main.cpp"));
    const QString mainWindowHeader = readSource(QStringLiteral("mainwindow.h"));
    const QString mainWindowSource = readSource(QStringLiteral("mainwindow.cpp"));
    const QString mainInterfaceHeader = readSource(QStringLiteral("UI/MainInterfaceWidget.h"));
    const QString navigationPageSource = readSource(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
    const QString orchestratorSource = readSource(QStringLiteral("Framework/StartupOrchestrator.cpp"));
    const QString startupCoordinatorHeader =
        readSource(QStringLiteral("Framework/Platform/Kernel/PlatformStartupCoordinator.h"));
    const QString startupCoordinatorSource =
        readSource(QStringLiteral("Framework/Platform/Kernel/PlatformStartupCoordinator.cpp"));
    const QString snapshotContracts =
        readSource(QStringLiteral("Framework/Platform/Contracts/PlatformSnapshots.h"));
    const QString runtimeHostPortsHeader =
        readSource(QStringLiteral("Framework/Platform/Contracts/platform_runtime_host_ports.h"));
    const QString runtimeHostAdapterHeader =
        readSource(QStringLiteral("Framework/Platform/Kernel/platform_runtime_host_adapter.h"));
    const QString runtimeHostAdapterSource =
        readSource(QStringLiteral("Framework/Platform/Kernel/platform_runtime_host_adapter.cpp"));
    const int runtimeHostPortStart = runtimeHostPortsHeader.indexOf(QStringLiteral("class FRAMEWORK_EXPORT IPlatformRuntimeHostPort"));
    const int serviceAccessPortStart = runtimeHostPortsHeader.indexOf(QStringLiteral("class FRAMEWORK_EXPORT IPlatformServiceAccessPort"));

    QVERIFY2(mainSource.contains(QStringLiteral("platform_runtime_host_adapter.h")),
        "main.cpp must compose the runtime through platform_runtime_host_adapter");
    QVERIFY2(!mainSource.contains(QStringLiteral("ctk_runtime_host_adapter.h")),
        "main.cpp still composes the runtime through the old ctk_runtime_host_adapter include");
    QVERIFY2(!mainSource.contains(QStringLiteral("legacy_ctk_runtime_bridge.h")),
        "main.cpp must not include legacy_ctk_runtime_bridge.h after bridge cleanup");
    QVERIFY2(!mainSource.contains(QStringLiteral("CTKManager::instance()")),
        "main.cpp still directly pulls CTKManager");
    QVERIFY2(!mainSource.contains(QStringLiteral("CTKManager*")),
        "main.cpp still leaks CTKManager* through the startup flow");
    QVERIFY2(!mainSource.contains(QStringLiteral("runtimeHost->manager()")),
        "main.cpp still reaches through the runtime host to grab CTKManager");
    QVERIFY2(!mainSource.contains(QStringLiteral("platformPluginIdToCtkSymbolicName")),
        "main.cpp still carries platformPluginIdToCtkSymbolicName naming");
    QVERIFY2(!mainSource.contains(QStringLiteral("pluginIdByCtkSymbolicName")),
        "main.cpp still carries pluginIdByCtkSymbolicName naming");
    QVERIFY2(!mainSource.contains(QStringLiteral("resolveByCtkSymbolicName")),
        "main.cpp still exposes resolveByCtkSymbolicName");
    QVERIFY2(!mainSource.contains(QStringLiteral("resolveByCtkSymbolicOrPath")),
        "main.cpp still exposes resolveByCtkSymbolicOrPath");
    QVERIFY2(!mainSource.contains(QStringLiteral("QString ctkSymbolicName;")),
        "main.cpp still exposes PluginIdentity::ctkSymbolicName");
    QVERIFY2(!mainSource.contains(QStringLiteral("legacyRuntimePort")),
        "main.cpp still carries the legacy runtime bridge variable");
    QVERIFY2(!mainSource.contains(QStringLiteral("setDescriptorPolicyContext(")),
        "main.cpp still performs CTK descriptor policy handoff");
    QVERIFY2(!mainSource.contains(QStringLiteral("runtimeHostEventSource")),
        "main.cpp still tracks the legacy runtime event source");
    QVERIFY2(!mainSource.contains(QStringLiteral("connectRuntimeHostSignal(")),
        "main.cpp still binds startup recorder bridge signals");
    QVERIFY2(!mainSource.contains(QStringLiteral("&PlatformRuntimeHostAdapter::")),
        "main.cpp still binds startup flow to PlatformRuntimeHostAdapter-specific signals");
    QVERIFY2(!mainSource.contains(QStringLiteral("runtimeHost->installPlugin(")),
        "main.cpp still depends on the adapter-specific installPlugin() entry");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("ctkPluginContext")),
        "mainwindow.h still stores ctkPluginContext");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("ctkServiceReference")),
        "mainwindow.h still stores ctkServiceReference");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("CTKManager::instance()")),
        "mainwindow.cpp still directly reaches for CTKManager::instance()");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("getPluginContext()")),
        "mainwindow.cpp still reaches through CTK plugin context");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("getServiceReference")),
        "mainwindow.cpp still queries services through CTK");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("getService<")),
        "mainwindow.cpp still resolves services through CTK");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("Plugins/MedicalImageCore/MedicalImageCoreService.h")),
        "mainwindow.cpp still includes the removed MedicalImageCoreService header");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("Plugins/PatientManagement/PatientDatabaseService.h")),
        "mainwindow.cpp still includes the removed PatientDatabaseService header");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("Plugins/MedicalViewer/MedicalViewerService.h")),
        "mainwindow.cpp still includes the removed MedicalViewerService header");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("Plugins/MedicalProcessing/MedicalProcessingService.h")),
        "mainwindow.cpp still includes the removed MedicalProcessingService header");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("Plugins/MedicalImageCore/MedicalImageCoreService.h")),
        "platform_runtime_host_adapter.cpp still includes the removed MedicalImageCoreService header");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("Plugins/PatientManagement/PatientDatabaseService.h")),
        "platform_runtime_host_adapter.cpp still includes the removed PatientDatabaseService header");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("Plugins/MedicalViewer/MedicalViewerService.h")),
        "platform_runtime_host_adapter.cpp still includes the removed MedicalViewerService header");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("Plugins/MedicalProcessing/MedicalProcessingService.h")),
        "platform_runtime_host_adapter.cpp still includes the removed MedicalProcessingService header");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("CTKManager::instance()")),
        "platform_runtime_host_adapter.cpp still binds the default runtime host to CTKManager::instance()");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("Framework/CTKManager.h")),
        "platform_runtime_host_adapter.cpp should no longer include CTKManager.h");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("getService<PatientDatabaseService>()")),
        "platform_runtime_host_adapter.cpp still resolves the removed PatientDatabaseService");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("getService<MedicalImageCoreService>()")),
        "platform_runtime_host_adapter.cpp still resolves the removed MedicalImageCoreService");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("getService<MedicalViewerService>()")),
        "platform_runtime_host_adapter.cpp still resolves the removed MedicalViewerService");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("getService<MedicalProcessingService>()")),
        "platform_runtime_host_adapter.cpp still resolves the removed MedicalProcessingService");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("ImageInteractionService")),
        "platform_runtime_host_adapter.cpp still exposes the legacy ImageInteractionService bridge");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("getService<RegistrationService>()")),
        "platform_runtime_host_adapter.cpp still resolves RegistrationService through CTK");
    QVERIFY2(!runtimeHostAdapterSource.contains(QStringLiteral("getService<OpticalTrackingService>()")),
        "platform_runtime_host_adapter.cpp still resolves OpticalTrackingService through CTK");
    QVERIFY2(!runtimeHostPortsHeader.contains(QStringLiteral("PatientDatabaseService")),
        "platform_runtime_host_ports.h still exposes the removed PatientDatabaseService contract");
    QVERIFY2(!runtimeHostPortsHeader.contains(QStringLiteral("MedicalImageCoreService")),
        "platform_runtime_host_ports.h still exposes the removed MedicalImageCoreService contract");
    QVERIFY2(!runtimeHostPortsHeader.contains(QStringLiteral("MedicalViewerService")),
        "platform_runtime_host_ports.h still exposes the removed MedicalViewerService contract");
    QVERIFY2(!runtimeHostPortsHeader.contains(QStringLiteral("MedicalProcessingService")),
        "platform_runtime_host_ports.h still exposes the removed MedicalProcessingService contract");
    QVERIFY2(!runtimeHostPortsHeader.contains(QStringLiteral("ImageInteractionService")),
        "platform_runtime_host_ports.h still exposes the legacy ImageInteractionService contract");
    QVERIFY2(!runtimeHostPortsHeader.contains(QStringLiteral("IPlatformLegacyPluginRuntimePort")),
        "platform_runtime_host_ports.h must not keep the legacy runtime port");
    QVERIFY2(runtimeHostPortStart >= 0 && serviceAccessPortStart > runtimeHostPortStart,
        "platform runtime host port declarations must stay ordered");

    const QString runtimeHostPortSection =
        runtimeHostPortsHeader.mid(runtimeHostPortStart, serviceAccessPortStart - runtimeHostPortStart);

    QVERIFY2(!runtimeHostPortSection.contains(QStringLiteral("virtual void setSafeMode(bool enabled) = 0;")),
        "platform runtime host port should no longer carry CTK safe-mode policy handoff");
    QVERIFY2(!runtimeHostPortSection.contains(QStringLiteral("virtual void setDescriptorPolicyContext(")),
        "platform runtime host port should no longer carry CTK descriptor policy handoff");
    QVERIFY2(!runtimeHostPortSection.contains(QStringLiteral("virtual bool installPlugin(")),
        "platform runtime host port should no longer carry CTK plugin installation");
    QVERIFY2(!runtimeHostPortSection.contains(QStringLiteral("virtual QStringList deferredPluginIds() const = 0;")),
        "platform runtime host port should no longer carry CTK deferred plugin state");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("patientDatabaseService() const override")),
        "platform_runtime_host_adapter.h still declares patientDatabaseService()");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("medicalImageCoreService() const override")),
        "platform_runtime_host_adapter.h still declares medicalImageCoreService()");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("medicalViewerService() const override")),
        "platform_runtime_host_adapter.h still declares medicalViewerService()");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("medicalProcessingService() const override")),
        "platform_runtime_host_adapter.h still declares medicalProcessingService()");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("imageInteractionService() const override")),
        "platform_runtime_host_adapter.h still declares imageInteractionService()");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("CTKManager* manager() const")),
        "platform_runtime_host_adapter.h still exposes CTKManager through manager()");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("explicit PlatformRuntimeHostAdapter(CTKManager* manager, QObject* parent = nullptr);")),
        "platform_runtime_host_adapter.h should no longer expose CTKManager constructor injection");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("void setSafeMode(bool enabled) override;")),
        "platform_runtime_host_adapter.h should no longer expose CTK safe-mode handling");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("void setDescriptorPolicyContext(")),
        "platform_runtime_host_adapter.h should no longer expose descriptor policy handoff");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("bool installPlugin(const QString& pluginPath, bool autoStart = false, QString* outPluginName = nullptr) override;")),
        "platform_runtime_host_adapter.h should no longer expose CTK plugin installation");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("QStringList deferredPluginIds() const override;")),
        "platform_runtime_host_adapter.h should no longer expose CTK deferred plugin ids");
    QVERIFY2(!runtimeHostAdapterHeader.contains(QStringLiteral("QObject* pluginEventSource() const override;")),
        "platform_runtime_host_adapter.h should no longer expose the CTK event source");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/Platform/CtkBridge/legacy_ctk_runtime_bridge.h")).exists(),
        "legacy_ctk_runtime_bridge.h must be deleted");
    QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Framework/Platform/CtkBridge/legacy_ctk_runtime_bridge.cpp")).exists(),
        "legacy_ctk_runtime_bridge.cpp must be deleted");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("PatientDatabaseService*")),
        "mainwindow.h still stores PatientDatabaseService");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("MedicalImageCoreService*")),
        "mainwindow.h still stores MedicalImageCoreService");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("MedicalViewerService*")),
        "mainwindow.h still stores MedicalViewerService");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("MedicalProcessingService*")),
        "mainwindow.h still stores MedicalProcessingService");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("ImageInteractionService*")),
        "mainwindow.h still stores ImageInteractionService");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("initializeImageInteractionService")),
        "mainwindow.h still declares initializeImageInteractionService()");
    QVERIFY2(!mainWindowHeader.contains(QStringLiteral("onImageInteractionServiceAvailable")),
        "mainwindow.h still declares onImageInteractionServiceAvailable()");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("initializeImageInteractionService()")),
        "mainwindow.cpp still initializes the legacy ImageInteractionService");
    QVERIFY2(!mainWindowSource.contains(QStringLiteral("m_imageInteractionService")),
        "mainwindow.cpp still stores or uses ImageInteractionService");
    QVERIFY2(!mainInterfaceHeader.contains(QStringLiteral("Framework/Platform/CtkBridge/CoreUiRuntimeStatusProvider.h")),
        "MainInterfaceWidget.h still includes CoreUiRuntimeStatusProvider through CtkBridge");
    QVERIFY2(!navigationPageSource.contains(QStringLiteral("Framework/Platform/CtkBridge/NavigationPageServiceAccess.h")),
        "NavigationPage.cpp still includes NavigationPageServiceAccess through CtkBridge");
    QVERIFY2(!orchestratorSource.contains(QStringLiteral("CTK framework initialization")),
        "StartupOrchestrator still treats CTK as a first-class startup phase");
    QVERIFY2(!startupCoordinatorHeader.contains(QStringLiteral("platformPluginIdToCtkSymbolicName")),
        "PlatformStartupCoordinator.h still leaks platformPluginIdToCtkSymbolicName naming");
    QVERIFY2(!startupCoordinatorSource.contains(QStringLiteral("platformPluginIdToCtkSymbolicName")),
        "PlatformStartupCoordinator.cpp still leaks platformPluginIdToCtkSymbolicName naming");
    QVERIFY2(!startupCoordinatorHeader.contains(QStringLiteral("QString ctkSymbolicName;")),
        "PlatformStartupCoordinator.h still exposes ResolvedPluginTarget::ctkSymbolicName");
    QVERIFY2(!startupCoordinatorHeader.contains(QStringLiteral("resolveDeferredPluginTarget(const QString& ctkSymbolicName) const")),
        "PlatformStartupCoordinator.h still exposes resolveDeferredPluginTarget(const QString& ctkSymbolicName)");
    QVERIFY2(!startupCoordinatorSource.contains(QStringLiteral("ctk:%1")),
        "PlatformStartupCoordinator.cpp still synthesizes ctk:* fallback keys");
    QVERIFY2(!snapshotContracts.contains(QStringLiteral("ctkSymbolicName")),
        "PlatformSnapshots.h still exposes ctkSymbolicName runtime fields");
}

void RuntimeHostDetachmentContractTest::legacy_platform_adapters_and_snapshots_do_not_depend_on_ctk_manager()
{
    const QString navigationAdapterSource =
        readSource(QStringLiteral("Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.cpp"));
    const QString coreRuntimeAdapterSource =
        readSource(QStringLiteral("Framework/Platform/LegacyAdapters/LegacyCoreUiRuntimeAdapter.cpp"));
    const QString userAdapterSource =
        readSource(QStringLiteral("Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.cpp"));
    const QString imagingAdapterSource =
        readSource(QStringLiteral("Framework/Platform/LegacyAdapters/LegacyImagingAdapter.cpp"));
    const QString runtimeSnapshotSource =
        readSource(QStringLiteral("Framework/Platform/Diagnostics/PlatformRuntimeSnapshotCollector.cpp"));

    QVERIFY2(!navigationAdapterSource.contains(QStringLiteral("Framework/CTKManager.h")),
        "LegacyNavigationPageServiceAdapter.cpp still includes CTKManager.h");
    QVERIFY2(!navigationAdapterSource.contains(QStringLiteral("CTKManager::instance()")),
        "LegacyNavigationPageServiceAdapter.cpp still reaches for CTKManager::instance()");
    QVERIFY2(!coreRuntimeAdapterSource.contains(QStringLiteral("Framework/CTKManager.h")),
        "LegacyCoreUiRuntimeAdapter.cpp still includes CTKManager.h");
    QVERIFY2(!coreRuntimeAdapterSource.contains(QStringLiteral("CTKManager::instance()")),
        "LegacyCoreUiRuntimeAdapter.cpp still reaches for CTKManager::instance()");
    QVERIFY2(!userAdapterSource.contains(QStringLiteral("Framework/CTKManager.h")),
        "LegacyUserManagementAdapter.cpp still includes CTKManager.h");
    QVERIFY2(!userAdapterSource.contains(QStringLiteral("CTKManager::instance()")),
        "LegacyUserManagementAdapter.cpp still reaches for CTKManager::instance()");
    QVERIFY2(!imagingAdapterSource.contains(QStringLiteral("Framework/CTKManager.h")),
        "LegacyImagingAdapter.cpp still includes CTKManager.h");
    QVERIFY2(!imagingAdapterSource.contains(QStringLiteral("CTKManager::instance()")),
        "LegacyImagingAdapter.cpp still reaches for CTKManager::instance()");
    QVERIFY2(!runtimeSnapshotSource.contains(QStringLiteral("Framework/CTKManager.h")),
        "PlatformRuntimeSnapshotCollector.cpp still includes CTKManager.h");
    QVERIFY2(!runtimeSnapshotSource.contains(QStringLiteral("CTKManager::instance()")),
        "PlatformRuntimeSnapshotCollector.cpp still reaches for CTKManager::instance()");
}

QTEST_APPLESS_MAIN(RuntimeHostDetachmentContractTest)
#include "RuntimeHostDetachmentContractTest.moc"
