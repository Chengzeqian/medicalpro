#include <QtTest/QtTest>

#include <QSignalSpy>

#include "Framework/Platform/Contracts/PlatformUiPorts.h"
#include "Framework/Platform/CtkBridge/CoreUiRuntimeStatusProvider.h"
#include "Framework/Platform/CtkBridge/NavigationPageServiceAccess.h"

class FakePluginEventSource : public QObject
{
    Q_OBJECT

signals:
    void pluginLoaded(const QString& pluginName);
};

class FakeCoreUiRuntimePort : public ICoreUiRuntimeStatusPort
{
public:
    bool frameworkReady() const override { return frameworkReadyValue; }
    QStringList installedPlugins() const override { return installedPluginsValue; }
    QStringList startedPlugins() const override { return startedPluginsValue; }
    QStringList loadedPlugins() const override { return loadedPluginsValue; }
    QStringList missingServices(const QStringList& requiredServices) const override
    {
        lastRequestedServices = requiredServices;
        return missingServicesValue;
    }
    bool directoryExists(const QString& directoryPath) const override
    {
        lastDirectoryPath = directoryPath;
        return directoryExistsValue;
    }
    bool directoryReadable(const QString& directoryPath) const override
    {
        lastReadableDirectoryPath = directoryPath;
        return directoryReadableValue;
    }

    bool frameworkReadyValue = false;
    QStringList installedPluginsValue;
    QStringList startedPluginsValue;
    QStringList loadedPluginsValue;
    QStringList missingServicesValue;
    bool directoryExistsValue = false;
    bool directoryReadableValue = false;
    mutable QStringList lastRequestedServices;
    mutable QString lastDirectoryPath;
    mutable QString lastReadableDirectoryPath;
};

class FakeNavigationPageServicePort : public INavigationPageServicePort
{
public:
    bool frameworkReady() const override { return frameworkReadyValue; }
    QObject* pluginEventSource() const override { return pluginSource; }
    bool isPluginStarted(const QString& pluginName) const override
    {
        lastIsPluginStartedName = pluginName;
        return pluginStartedValue;
    }
    bool startPlugin(const QString& pluginName) override
    {
        startPluginCalled = true;
        startedPluginName = pluginName;
        return startPluginResult;
    }
    QString pluginState(const QString& pluginName) const override
    {
        lastPluginStateName = pluginName;
        return pluginStateValue;
    }
    InstrumentManagementService* instrumentManagementService() const override { return instrumentServiceValue; }
    DicomViewerService* dicomViewerService() const override { return dicomServiceValue; }
    SegmentationService* segmentationService() const override { return segmentationServiceValue; }
    FourViewDisplayService* fourViewDisplayService() const override { return fourViewServiceValue; }
    PointRegistrationService* pointRegistrationService() const override { return pointRegistrationServiceValue; }

    bool frameworkReadyValue = false;
    bool pluginStartedValue = false;
    bool startPluginCalled = false;
    bool startPluginResult = true;
    QString pluginStateValue;
    QObject* pluginSource = nullptr;
    InstrumentManagementService* instrumentServiceValue = nullptr;
    DicomViewerService* dicomServiceValue = nullptr;
    SegmentationService* segmentationServiceValue = nullptr;
    FourViewDisplayService* fourViewServiceValue = nullptr;
    PointRegistrationService* pointRegistrationServiceValue = nullptr;
    mutable QString lastIsPluginStartedName;
    mutable QString lastPluginStateName;
    QString startedPluginName;
};

class PlatformUiBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    void coreRuntimeProvider_builds_welcome_snapshot_from_port();
    void coreRuntimeProvider_builds_system_settings_snapshot_from_loaded_plugins();
    void coreRuntimeProvider_system_settings_prefers_loaded_plugins_over_started_plugins();
    void navigationServiceAccess_forwards_service_pointers_from_port();
    void navigationServiceAccess_starts_point_registration_plugin_on_demand();
    void navigationServiceAccess_re_emits_point_registration_plugin_load();
};

void PlatformUiBridgeTest::coreRuntimeProvider_builds_welcome_snapshot_from_port()
{
    FakeCoreUiRuntimePort port;
    port.frameworkReadyValue = true;
    port.installedPluginsValue = QStringList { QStringLiteral("UserManagement") };
    port.startedPluginsValue = QStringList { QStringLiteral("UserManagement"), QStringLiteral("DicomViewer") };
    port.missingServicesValue = QStringList { QStringLiteral("DicomViewerService") };
    port.directoryExistsValue = true;
    port.directoryReadableValue = true;

    CoreUiRuntimeStatusProvider provider(&port, QStringLiteral("D:/runtime/data"));
    const auto snapshot = provider.welcomeSnapshot();

    QVERIFY(snapshot.frameworkReady);
    QCOMPARE(snapshot.pluginCount, 2);
    QCOMPARE(snapshot.totalServices, 3);
    QCOMPARE(snapshot.readyServices, 2);
    QVERIFY(snapshot.dataDirectoryExists);
    QVERIFY(snapshot.dataDirectoryReadable);
    QVERIFY(!snapshot.workflowReady);
    QCOMPARE(port.lastDirectoryPath, QStringLiteral("D:/runtime/data"));
    QCOMPARE(port.lastReadableDirectoryPath, QStringLiteral("D:/runtime/data"));
    QCOMPARE(port.lastRequestedServices.size(), 3);
}

void PlatformUiBridgeTest::coreRuntimeProvider_builds_system_settings_snapshot_from_loaded_plugins()
{
    FakeCoreUiRuntimePort port;
    port.frameworkReadyValue = true;
    port.loadedPluginsValue = QStringList {
        QStringLiteral("UserManagement"),
        QStringLiteral("DicomViewer"),
        QStringLiteral("FourViewDisplay")
    };
    port.directoryExistsValue = true;
    port.directoryReadableValue = false;

    CoreUiRuntimeStatusProvider provider(&port, QStringLiteral("D:/runtime/data"));
    const auto snapshot = provider.systemSettingsSnapshot();

    QVERIFY(snapshot.frameworkReady);
    QCOMPARE(snapshot.pluginCount, 3);
    QCOMPARE(snapshot.readyServices, 3);
    QCOMPARE(snapshot.totalServices, 3);
    QVERIFY(snapshot.dataDirectoryExists);
    QVERIFY(!snapshot.dataDirectoryReadable);
    QVERIFY(snapshot.workflowReady);
}

void PlatformUiBridgeTest::coreRuntimeProvider_system_settings_prefers_loaded_plugins_over_started_plugins()
{
    FakeCoreUiRuntimePort port;
    port.frameworkReadyValue = true;
    port.startedPluginsValue = QStringList {
        QStringLiteral("UserManagement"),
        QStringLiteral("DicomViewer"),
        QStringLiteral("FourViewDisplay"),
        QStringLiteral("RegistrationCore")
    };
    port.loadedPluginsValue = QStringList {
        QStringLiteral("UserManagement"),
        QStringLiteral("DicomViewer")
    };
    port.directoryExistsValue = true;
    port.directoryReadableValue = true;

    CoreUiRuntimeStatusProvider provider(&port, QStringLiteral("D:/runtime/data"));
    const auto snapshot = provider.systemSettingsSnapshot();

    QCOMPARE(snapshot.pluginCount, 2);
}

void PlatformUiBridgeTest::navigationServiceAccess_forwards_service_pointers_from_port()
{
    FakeNavigationPageServicePort port;
    port.instrumentServiceValue = reinterpret_cast<InstrumentManagementService*>(quintptr(0x11));
    port.dicomServiceValue = reinterpret_cast<DicomViewerService*>(quintptr(0x22));
    port.segmentationServiceValue = reinterpret_cast<SegmentationService*>(quintptr(0x33));
    port.fourViewServiceValue = reinterpret_cast<FourViewDisplayService*>(quintptr(0x44));

    NavigationPageServiceAccess access(&port);

    QCOMPARE(access.instrumentManagementService(), port.instrumentServiceValue);
    QCOMPARE(access.dicomViewerService(), port.dicomServiceValue);
    QCOMPARE(access.segmentationService(), port.segmentationServiceValue);
    QCOMPARE(access.fourViewDisplayService(), port.fourViewServiceValue);
}

void PlatformUiBridgeTest::navigationServiceAccess_starts_point_registration_plugin_on_demand()
{
    FakeNavigationPageServicePort port;
    port.frameworkReadyValue = true;
    port.pluginStartedValue = false;
    port.pluginStateValue = QStringLiteral("RESOLVED");
    port.pointRegistrationServiceValue = reinterpret_cast<PointRegistrationService*>(quintptr(0x55));

    NavigationPageServiceAccess access(&port);
    const auto* service = access.pointRegistrationService(true);

    QCOMPARE(service, port.pointRegistrationServiceValue);
    QVERIFY(port.startPluginCalled);
    QCOMPARE(port.lastIsPluginStartedName, QStringLiteral("PointRegistration"));
    QCOMPARE(port.startedPluginName, QStringLiteral("PointRegistration"));
    QCOMPARE(access.pointRegistrationPluginState(), QStringLiteral("RESOLVED"));
    QCOMPARE(port.lastPluginStateName, QStringLiteral("PointRegistration"));
}

void PlatformUiBridgeTest::navigationServiceAccess_re_emits_point_registration_plugin_load()
{
    FakePluginEventSource source;
    FakeNavigationPageServicePort port;
    port.pluginSource = &source;

    NavigationPageServiceAccess access(&port);
    QSignalSpy spy(&access, &NavigationPageServiceAccess::pointRegistrationPluginAvailable);

    emit source.pluginLoaded(QStringLiteral("InstrumentManagement"));
    emit source.pluginLoaded(QStringLiteral("PointRegistration"));

    QCOMPARE(spy.count(), 1);
}

QTEST_APPLESS_MAIN(PlatformUiBridgeTest)
#include "PlatformUiBridgeTest.moc"
