#include "Framework/Platform/Kernel/platform_plugin_host.h"

#include "Framework/Platform/Kernel/platform_event_bus.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"

PlatformPluginHost& PlatformPluginHost::sharedInstance()
{
    static PlatformPluginHost instance;
    return instance;
}

PlatformPluginHost::PlatformPluginHost()
    : m_serviceRegistry(std::make_unique<PlatformServiceRegistry>())
    , m_eventBus(std::make_unique<PlatformEventBus>())
{
}

PlatformPluginHost::~PlatformPluginHost()
{
    stopAll();
}

void PlatformPluginHost::registerActivator(std::unique_ptr<IPlatformModuleActivator> activator)
{
    if (!activator) return;

    auto* rawActivator = activator.get();
    const auto pluginId = rawActivator->pluginId().trimmed();
    if (pluginId.isEmpty()) return;
    if (m_activators.contains(pluginId)) return;

    m_activators.insert(pluginId, rawActivator);
    m_ownedActivators.push_back(std::move(activator));
}

bool PlatformPluginHost::hasActivator(const QString& pluginId) const
{
    return m_activators.contains(pluginId.trimmed());
}

bool PlatformPluginHost::startModule(const QString& pluginId)
{
    const auto normalizedPluginId = pluginId.trimmed();
    if (normalizedPluginId.isEmpty()) return false;
    if (m_startedModules.contains(normalizedPluginId)) return true;
    if (!m_activators.contains(normalizedPluginId)) return false;

    auto* activator = m_activators.value(normalizedPluginId);
    auto context = moduleContext();
    if (!activator->start(context)) return false;

    m_startedModules.insert(normalizedPluginId);
    if (m_eventBus) {
        QVariantMap payload;
        payload.insert(QStringLiteral("pluginId"), normalizedPluginId);
        m_eventBus->publish(QStringLiteral("platform/module_started"), payload);
    }
    return true;
}

bool PlatformPluginHost::isModuleStarted(const QString& pluginId) const
{
    return m_startedModules.contains(pluginId.trimmed());
}

void PlatformPluginHost::stopModule(const QString& pluginId)
{
    const auto normalizedPluginId = pluginId.trimmed();
    if (!m_startedModules.contains(normalizedPluginId)) return;
    if (!m_activators.contains(normalizedPluginId)) return;

    auto* activator = m_activators.value(normalizedPluginId);
    auto context = moduleContext();
    activator->stop(context);
    m_serviceRegistry->unregisterPlugin(normalizedPluginId);
    if (m_eventBus) {
        QVariantMap payload;
        payload.insert(QStringLiteral("pluginId"), normalizedPluginId);
        m_eventBus->publish(QStringLiteral("platform/module_stopped"), payload);
    }
    m_startedModules.remove(normalizedPluginId);
}

void PlatformPluginHost::stopAll()
{
    const auto startedPluginIds = m_startedModules.values();
    for (const auto& pluginId : startedPluginIds) {
        stopModule(pluginId);
    }
}

PlatformServiceRegistry* PlatformPluginHost::serviceRegistry() const
{
    return m_serviceRegistry.get();
}

IPlatformEventBusPort* PlatformPluginHost::eventBus() const
{
    return m_eventBus.get();
}

QStringList PlatformPluginHost::registeredPluginIds() const
{
    return m_activators.keys();
}

PlatformModuleContext PlatformPluginHost::moduleContext() const
{
    PlatformModuleContext context;
    context.serviceRegistry = m_serviceRegistry.get();
    context.eventBus = m_eventBus.get();
    return context;
}
