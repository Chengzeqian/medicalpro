#include "Framework/Platform/Kernel/platform_service_registry.h"

void PlatformServiceRegistry::registerService(const QString& pluginId, const QString& serviceId, QObject* service)
{
    if (serviceId.trimmed().isEmpty()) return;

    ServiceEntry entry;
    entry.pluginId = pluginId.trimmed();
    entry.instance = service;
    m_services.insert(serviceId.trimmed(), entry);
}

QObject* PlatformServiceRegistry::service(const QString& serviceId) const
{
    return m_services.value(serviceId.trimmed()).instance;
}

QObject* PlatformServiceRegistry::service(const QString& pluginId, const QString& serviceId) const
{
    const auto entry = m_services.value(serviceId.trimmed());
    if (entry.pluginId != pluginId.trimmed()) return nullptr;
    return entry.instance;
}

QString PlatformServiceRegistry::pluginForService(const QString& serviceId) const
{
    return m_services.value(serviceId.trimmed()).pluginId;
}

bool PlatformServiceRegistry::hasService(const QString& serviceId) const
{
    return service(serviceId) != nullptr;
}

void PlatformServiceRegistry::unregisterPlugin(const QString& pluginId)
{
    const auto normalizedPluginId = pluginId.trimmed();
    auto it = m_services.begin();
    while (it != m_services.end()) {
        if (it.value().pluginId == normalizedPluginId) {
            it = m_services.erase(it);
            continue;
        }

        ++it;
    }
}
