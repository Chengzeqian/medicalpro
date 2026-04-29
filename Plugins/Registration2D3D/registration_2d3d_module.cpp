#include "registration_2d3d_module.h"

#include "Registration2D3DServiceImpl.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"

Registration2D3DModule::~Registration2D3DModule() = default;

QString Registration2D3DModule::pluginId() const
{
    return QStringLiteral("Registration2D3D");
}

bool Registration2D3DModule::start(PlatformModuleContext& context)
{
    if (!context.serviceRegistry) return false;

    m_service = std::make_unique<Registration2D3DServiceImpl>();
    m_service->setServiceRegistry(context.serviceRegistry);
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("Registration2D3DService"), m_service.get());
    return true;
}

void Registration2D3DModule::stop(PlatformModuleContext& context)
{
    if (context.serviceRegistry) {
        context.serviceRegistry->unregisterPlugin(pluginId());
    }

    m_service.reset();
}
