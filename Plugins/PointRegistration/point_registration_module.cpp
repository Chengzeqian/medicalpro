#include "point_registration_module.h"

#include "Framework/Platform/Kernel/platform_service_registry.h"

PointRegistrationModule::~PointRegistrationModule() = default;

QString PointRegistrationModule::pluginId() const
{
    return QStringLiteral("PointRegistration");
}

bool PointRegistrationModule::start(PlatformModuleContext& context)
{
    if (!context.serviceRegistry) return false;

    m_service = std::make_unique<PointRegistrationServiceImpl>();
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("PointRegistrationService"), m_service.get());
    return true;
}

void PointRegistrationModule::stop(PlatformModuleContext& context)
{
    if (context.serviceRegistry) {
        context.serviceRegistry->unregisterPlugin(pluginId());
    }

    m_service.reset();
}
