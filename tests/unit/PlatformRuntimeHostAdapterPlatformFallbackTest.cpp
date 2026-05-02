#include <QtTest/QtTest>

#include "Framework/Platform/Kernel/platform_runtime_host_adapter.h"
#include "Framework/Platform/Contracts/platform_module_ports.h"
#include "Framework/Platform/Kernel/platform_plugin_host.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"
#include "Plugins/RegistrationCore/registration_core_module.h"

#include <QUuid>
#include <memory>

namespace
{
class DummyFallbackService final : public QObject
{
    Q_OBJECT
};

class FakePlatformOnlyModule final : public IPlatformModuleActivator
{
public:
    explicit FakePlatformOnlyModule(QString pluginId)
        : m_pluginId(std::move(pluginId))
    {
    }

    QString pluginId() const override
    {
        return m_pluginId;
    }

    bool start(PlatformModuleContext& context) override
    {
        if (!context.serviceRegistry) return false;
        context.serviceRegistry->registerService(m_pluginId, m_pluginId + QStringLiteral("Service"), &m_service);
        return true;
    }

    void stop(PlatformModuleContext&) override
    {
    }

private:
    QString m_pluginId;
    DummyFallbackService m_service;
};

class FakeServicePublishingModule final : public IPlatformModuleActivator
{
public:
    FakeServicePublishingModule(QString pluginId, QStringList serviceIds)
        : m_pluginId(std::move(pluginId))
        , m_serviceIds(std::move(serviceIds))
    {
    }

    QString pluginId() const override
    {
        return m_pluginId;
    }

    bool start(PlatformModuleContext& context) override
    {
        if (!context.serviceRegistry) return false;
        for (const QString& serviceId : m_serviceIds) {
            auto service = std::make_unique<DummyFallbackService>();
            context.serviceRegistry->registerService(m_pluginId, serviceId, service.get());
            m_services.push_back(std::move(service));
        }
        return true;
    }

    void stop(PlatformModuleContext&) override
    {
    }

private:
    QString m_pluginId;
    QStringList m_serviceIds;
    std::vector<std::unique_ptr<DummyFallbackService>> m_services;
};
}

class PlatformRuntimeHostAdapterPlatformFallbackTest : public QObject
{
    Q_OBJECT

private slots:
    void adapter_default_constructor_stays_platform_only();
    void adapter_can_activate_and_report_platform_host_modules();
    void adapter_can_initialize_start_and_stop_platform_host();
    void adapter_reports_platform_registry_services_as_ready();
    void adapter_returns_registration_core_service_for_platform_module();
};

void PlatformRuntimeHostAdapterPlatformFallbackTest::adapter_default_constructor_stays_platform_only()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    const QString pluginId = QStringLiteral("PlatformDefaultCtor_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    host.registerActivator(std::make_unique<FakePlatformOnlyModule>(pluginId));

    PlatformRuntimeHostAdapter adapter;

    QVERIFY2(adapter.initialize(nullptr),
        "default-constructed runtime host should still initialize in platform-only mode");
    QVERIFY2(adapter.start(),
        "default-constructed runtime host should still start in platform-only mode");
    QVERIFY2(adapter.activatePlugin(pluginId),
        "default-constructed runtime host should activate platform-host modules");
    QVERIFY2(host.serviceRegistry()->hasService(pluginId + QStringLiteral("Service")),
        "default-constructed runtime host should publish platform-host services");
    QVERIFY2(adapter.stop(),
        "default-constructed runtime host should stop platform-host modules");
}

void PlatformRuntimeHostAdapterPlatformFallbackTest::adapter_can_activate_and_report_platform_host_modules()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    const QString pluginId = QStringLiteral("PlatformOnly_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    host.registerActivator(std::make_unique<FakePlatformOnlyModule>(pluginId));

    PlatformRuntimeHostAdapter adapter;

    QVERIFY2(adapter.activatePlugin(pluginId), "adapter should start platform-host module");
    QVERIFY2(adapter.isPluginStarted(pluginId), "adapter should report platform-host module as started");
    QCOMPARE(adapter.pluginState(pluginId), QStringLiteral("ACTIVE"));
    QVERIFY2(host.serviceRegistry()->hasService(pluginId + QStringLiteral("Service")),
        "platform-host module service should be published");

    host.stopModule(pluginId);
    QVERIFY2(!adapter.isPluginStarted(pluginId), "adapter should report stopped platform-host module");
    QCOMPARE(adapter.pluginState(pluginId), QStringLiteral("RESOLVED"));
}

void PlatformRuntimeHostAdapterPlatformFallbackTest::adapter_can_initialize_start_and_stop_platform_host()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    const QString pluginId = QStringLiteral("PlatformLifecycle_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    host.registerActivator(std::make_unique<FakePlatformOnlyModule>(pluginId));

    PlatformRuntimeHostAdapter adapter;

    QVERIFY2(adapter.initialize(nullptr), "platform-only runtime should initialize");
    QVERIFY2(adapter.start(), "platform-only runtime should start");
    QVERIFY2(adapter.activatePlugin(pluginId), "platform-only runtime should still activate platform modules");
    QVERIFY2(host.serviceRegistry()->hasService(pluginId + QStringLiteral("Service")),
        "activated platform module should register its service");

    QVERIFY2(adapter.stop(), "platform-only runtime should stop without CTK manager");
    QVERIFY2(!adapter.isPluginStarted(pluginId), "stop() should stop all platform-host modules");
    QVERIFY2(!host.serviceRegistry()->hasService(pluginId + QStringLiteral("Service")),
        "stop() should clear services registered by platform modules");
    QCOMPARE(adapter.pluginState(pluginId), QStringLiteral("RESOLVED"));
}

void PlatformRuntimeHostAdapterPlatformFallbackTest::adapter_reports_platform_registry_services_as_ready()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    const QString pluginId = QStringLiteral("PlatformServices_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    host.registerActivator(std::make_unique<FakeServicePublishingModule>(
        pluginId,
        QStringList{
            QStringLiteral("UserManagementService"),
            QStringLiteral("DicomViewerService"),
            QStringLiteral("RegistrationService")
        }));

    PlatformRuntimeHostAdapter adapter;
    QVERIFY2(adapter.activatePlugin(pluginId), "adapter should activate service-publishing module");

    const QStringList missing = adapter.missingServices(QStringList{
        QStringLiteral("UserManagementService"),
        QStringLiteral("DicomViewerService"),
        QStringLiteral("RegistrationService"),
        QStringLiteral("OpticalTrackingService")
    });

    QCOMPARE(missing, QStringList{QStringLiteral("OpticalTrackingService")});
}

void PlatformRuntimeHostAdapterPlatformFallbackTest::adapter_returns_registration_core_service_for_platform_module()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    if (!host.hasActivator(QStringLiteral("RegistrationCore"))) {
        host.registerActivator(std::make_unique<RegistrationCoreModule>());
    }

    PlatformRuntimeHostAdapter adapter;
    QVERIFY2(adapter.activatePlugin(QStringLiteral("RegistrationCore")),
        "adapter should activate the RegistrationCore platform module");
    QVERIFY2(adapter.registrationService() != nullptr,
        "adapter should expose the started RegistrationCore service through the runtime access port");
}

QTEST_APPLESS_MAIN(PlatformRuntimeHostAdapterPlatformFallbackTest)
#include "PlatformRuntimeHostAdapterPlatformFallbackTest.moc"
