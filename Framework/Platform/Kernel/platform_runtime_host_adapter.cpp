#include "Framework/Platform/Kernel/platform_runtime_host_adapter.h"

#include "Framework/Platform/Kernel/platform_plugin_host.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"
#include "Framework/Registration/RegistrationService.h"
#include "Plugins/OpticalTracking/OpticalTrackingService.h"

PlatformRuntimeHostAdapter::PlatformRuntimeHostAdapter(QObject* parent)
    : QObject(parent)
{
}

bool PlatformRuntimeHostAdapter::initialize(QApplication* app)
{
    Q_UNUSED(app);
    return true;
}

bool PlatformRuntimeHostAdapter::start()
{
    return true;
}

bool PlatformRuntimeHostAdapter::stop()
{
    PlatformPluginHost::sharedInstance().stopAll();
    return true;
}

bool PlatformRuntimeHostAdapter::activatePlugin(const QString& pluginId)
{
    auto& platformHost = PlatformPluginHost::sharedInstance();
    if (!platformHost.hasActivator(pluginId)) return false;
    return platformHost.startModule(pluginId);
}

bool PlatformRuntimeHostAdapter::isPluginStarted(const QString& pluginId) const
{
    auto& platformHost = PlatformPluginHost::sharedInstance();
    return platformHost.hasActivator(pluginId) && platformHost.isModuleStarted(pluginId);
}

QString PlatformRuntimeHostAdapter::pluginState(const QString& pluginId) const
{
    auto& platformHost = PlatformPluginHost::sharedInstance();
    if (!platformHost.hasActivator(pluginId)) return QStringLiteral("UNKNOWN");
    return platformHost.isModuleStarted(pluginId)
        ? QStringLiteral("ACTIVE")
        : QStringLiteral("RESOLVED");
}

QStringList PlatformRuntimeHostAdapter::missingServices(const QStringList& requiredServices) const
{
    QStringList unresolvedServices;
    auto* registry = PlatformPluginHost::sharedInstance().serviceRegistry();
    for (const QString& serviceId : requiredServices) {
        if (registry && registry->hasService(serviceId)) continue;
        unresolvedServices.append(serviceId);
    }
    return unresolvedServices;
}

RegistrationService* PlatformRuntimeHostAdapter::registrationService() const
{
    auto* registry = PlatformPluginHost::sharedInstance().serviceRegistry();
    if (!registry) return nullptr;
    return qobject_cast<RegistrationService*>(registry->service(QStringLiteral("RegistrationService")));
}

OpticalTrackingService* PlatformRuntimeHostAdapter::opticalTrackingService() const
{
    auto* registry = PlatformPluginHost::sharedInstance().serviceRegistry();
    if (!registry) return nullptr;
    return qobject_cast<OpticalTrackingService*>(registry->service(QStringLiteral("OpticalTrackingService")));
}

void PlatformRuntimeHostAdapter::publish(const QString& topic, const QVariantMap& payload)
{
    auto* eventBus = PlatformPluginHost::sharedInstance().eventBus();
    if (!eventBus) return;
    eventBus->publish(topic, payload);
}
