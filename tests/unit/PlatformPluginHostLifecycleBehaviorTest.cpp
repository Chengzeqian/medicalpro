#include <QtTest/QtTest>

#include "Framework/Platform/Contracts/platform_module_ports.h"
#include "Framework/Platform/Kernel/platform_event_bus.h"
#include "Framework/Platform/Kernel/platform_plugin_host.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"

#include <memory>

namespace
{
class DummyPlatformService final : public QObject
{
    Q_OBJECT
};

class FakeLifecycleModule final : public IPlatformModuleActivator
{
public:
    QString pluginId() const override
    {
        return QStringLiteral("FakeLifecycleModule");
    }

    bool start(PlatformModuleContext& context) override
    {
        m_startedWithRegistry = context.serviceRegistry != nullptr;
        m_startedWithEventBus = context.eventBus != nullptr;
        if (!context.serviceRegistry) return false;

        context.serviceRegistry->registerService(pluginId(), QStringLiteral("FakeLifecycleService"), &m_service);
        return true;
    }

    void stop(PlatformModuleContext& context) override
    {
        m_stoppedWithRegistry = context.serviceRegistry != nullptr;
        m_stoppedWithEventBus = context.eventBus != nullptr;
    }

    bool startedWithRegistry() const { return m_startedWithRegistry; }
    bool startedWithEventBus() const { return m_startedWithEventBus; }
    bool stoppedWithRegistry() const { return m_stoppedWithRegistry; }
    bool stoppedWithEventBus() const { return m_stoppedWithEventBus; }

private:
    DummyPlatformService m_service;
    bool m_startedWithRegistry = false;
    bool m_startedWithEventBus = false;
    bool m_stoppedWithRegistry = false;
    bool m_stoppedWithEventBus = false;
};
}

class PlatformPluginHostLifecycleBehaviorTest : public QObject
{
    Q_OBJECT

private slots:
    void host_provides_runtime_context_and_publishes_lifecycle_events();
};

void PlatformPluginHostLifecycleBehaviorTest::host_provides_runtime_context_and_publishes_lifecycle_events()
{
    PlatformPluginHost host;
    auto module = std::make_unique<FakeLifecycleModule>();
    auto* modulePtr = module.get();

    host.registerActivator(std::move(module));

    auto* eventBus = dynamic_cast<PlatformEventBus*>(host.eventBus());
    QVERIFY2(eventBus != nullptr, "PlatformPluginHost must expose PlatformEventBus implementation");
    eventBus->clear();

    QVERIFY2(host.startModule(QStringLiteral("FakeLifecycleModule")), "host should start registered module");
    QVERIFY2(modulePtr->startedWithRegistry(), "start() must receive PlatformServiceRegistry");
    QVERIFY2(modulePtr->startedWithEventBus(), "start() must receive IPlatformEventBusPort");
    QVERIFY2(host.serviceRegistry()->hasService(QStringLiteral("FakeLifecycleService")),
        "module service must be published through PlatformServiceRegistry");

    const auto startEvents = eventBus->publishedEvents();
    QCOMPARE(startEvents.size(), 1);
    QCOMPARE(startEvents.first().topic, QStringLiteral("platform/module_started"));
    QCOMPARE(startEvents.first().payload.value(QStringLiteral("pluginId")).toString(), QStringLiteral("FakeLifecycleModule"));

    host.stopModule(QStringLiteral("FakeLifecycleModule"));

    QVERIFY2(modulePtr->stoppedWithRegistry(), "stop() must receive PlatformServiceRegistry");
    QVERIFY2(modulePtr->stoppedWithEventBus(), "stop() must receive IPlatformEventBusPort");
    QVERIFY2(!host.serviceRegistry()->hasService(QStringLiteral("FakeLifecycleService")),
        "host stop should remove module services from PlatformServiceRegistry");

    const auto lifecycleEvents = eventBus->publishedEvents();
    QCOMPARE(lifecycleEvents.size(), 2);
    QCOMPARE(lifecycleEvents.last().topic, QStringLiteral("platform/module_stopped"));
    QCOMPARE(lifecycleEvents.last().payload.value(QStringLiteral("pluginId")).toString(), QStringLiteral("FakeLifecycleModule"));
}

QTEST_APPLESS_MAIN(PlatformPluginHostLifecycleBehaviorTest)
#include "PlatformPluginHostLifecycleBehaviorTest.moc"
