#include "Framework/Platform/LegacyAdapters/LegacyCoreUiRuntimeAdapter.h"

#include "Framework/Platform/Kernel/platform_plugin_host.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"

#include <QDir>
#include <QFileInfo>

bool LegacyCoreUiRuntimeAdapter::frameworkReady() const
{
    return !PlatformPluginHost::sharedInstance().registeredPluginIds().isEmpty();
}

QStringList LegacyCoreUiRuntimeAdapter::installedPlugins() const
{
    return PlatformPluginHost::sharedInstance().registeredPluginIds();
}

QStringList LegacyCoreUiRuntimeAdapter::startedPlugins() const
{
    QStringList startedPlugins;
    auto& host = PlatformPluginHost::sharedInstance();
    const QStringList pluginIds = host.registeredPluginIds();
    for (const QString& pluginId : pluginIds) {
        if (host.isModuleStarted(pluginId)) startedPlugins.append(pluginId);
    }
    return startedPlugins;
}

QStringList LegacyCoreUiRuntimeAdapter::loadedPlugins() const
{
    return PlatformPluginHost::sharedInstance().registeredPluginIds();
}

QStringList LegacyCoreUiRuntimeAdapter::missingServices(const QStringList& requiredServices) const
{
    QStringList missingServices;
    auto* registry = PlatformPluginHost::sharedInstance().serviceRegistry();
    for (const QString& serviceId : requiredServices) {
        if (!registry || !registry->hasService(serviceId)) missingServices.append(serviceId);
    }
    return missingServices;
}

bool LegacyCoreUiRuntimeAdapter::directoryExists(const QString& directoryPath) const
{
    return QDir(directoryPath).exists();
}

bool LegacyCoreUiRuntimeAdapter::directoryReadable(const QString& directoryPath) const
{
    const QFileInfo info(QDir(directoryPath).absolutePath());
    return info.isReadable();
}
