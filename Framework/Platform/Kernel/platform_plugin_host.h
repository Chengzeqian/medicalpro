#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/platform_module_ports.h"

#include <QHash>
#include <QSet>
#include <QStringList>
#include <memory>
#include <vector>

class IPlatformEventBusPort;
class PlatformEventBus;
class PlatformServiceRegistry;

class FRAMEWORK_EXPORT PlatformPluginHost
{
public:
    static PlatformPluginHost& sharedInstance();

    PlatformPluginHost();
    ~PlatformPluginHost();

    void registerActivator(std::unique_ptr<IPlatformModuleActivator> activator);
    bool hasActivator(const QString& pluginId) const;
    bool startModule(const QString& pluginId);
    bool isModuleStarted(const QString& pluginId) const;
    void stopModule(const QString& pluginId);
    void stopAll();

    PlatformServiceRegistry* serviceRegistry() const;
    IPlatformEventBusPort* eventBus() const;
    QStringList registeredPluginIds() const;

private:
    PlatformModuleContext moduleContext() const;

    QHash<QString, IPlatformModuleActivator*> m_activators;
    QSet<QString> m_startedModules;
    std::vector<std::unique_ptr<IPlatformModuleActivator>> m_ownedActivators;
    std::unique_ptr<PlatformServiceRegistry> m_serviceRegistry;
    std::unique_ptr<PlatformEventBus> m_eventBus;
};
