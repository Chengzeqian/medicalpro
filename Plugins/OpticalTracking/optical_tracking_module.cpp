#include "optical_tracking_module.h"

#include "OpticalTrackingServiceImpl.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"

OpticalTrackingModule::~OpticalTrackingModule() = default;

QString OpticalTrackingModule::pluginId() const
{
    return QStringLiteral("OpticalTracking");
}

bool OpticalTrackingModule::start(PlatformModuleContext& context)
{
    if (!context.serviceRegistry) return false;

    m_service = std::make_unique<OpticalTrackingServiceImpl>();
    m_service->setServiceRegistry(context.serviceRegistry);
    m_service->startService();
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("OpticalTrackingService"), m_service.get());
    return true;
}

void OpticalTrackingModule::stop(PlatformModuleContext& context)
{
    if (m_service) {
        m_service->stopService();
    }
    if (context.serviceRegistry) {
        context.serviceRegistry->unregisterPlugin(pluginId());
    }

    m_service.reset();
}
