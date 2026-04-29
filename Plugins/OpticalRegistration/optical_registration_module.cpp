#include "optical_registration_module.h"

#include "Framework/Platform/Kernel/platform_service_registry.h"

OpticalRegistrationModule::~OpticalRegistrationModule() = default;

QString OpticalRegistrationModule::pluginId() const
{
    return QStringLiteral("OpticalRegistration");
}

bool OpticalRegistrationModule::start(PlatformModuleContext& context)
{
    if (!context.serviceRegistry) return false;

    m_service = std::make_unique<OpticalRegistrationServiceImpl>();
    m_service->setServiceRegistry(context.serviceRegistry);
    m_service->startService();
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("OpticalRegistrationService"), m_service.get());
    return true;
}

void OpticalRegistrationModule::stop(PlatformModuleContext& context)
{
    if (m_service) {
        m_service->stopService();
    }
    if (context.serviceRegistry) {
        context.serviceRegistry->unregisterPlugin(pluginId());
    }

    m_service.reset();
}
