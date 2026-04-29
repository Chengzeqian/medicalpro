#include "registration_core_module.h"

#include "RegistrationServiceImpl.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"

RegistrationCoreModule::~RegistrationCoreModule() = default;

QString RegistrationCoreModule::pluginId() const
{
    return QStringLiteral("RegistrationCore");
}

bool RegistrationCoreModule::start(PlatformModuleContext& context)
{
    if (!context.serviceRegistry) return false;

    m_service = std::make_unique<RegistrationServiceImpl>();
    m_service->setServiceRegistry(context.serviceRegistry);
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("RegistrationService"), m_service.get());
    return true;
}

void RegistrationCoreModule::stop(PlatformModuleContext& context)
{
    if (context.serviceRegistry) {
        context.serviceRegistry->unregisterPlugin(pluginId());
    }

    m_service.reset();
}
