#include "user_management_module.h"

#include "Framework/Platform/Kernel/platform_service_registry.h"

UserManagementModule::~UserManagementModule() = default;

QString UserManagementModule::pluginId() const
{
    return QStringLiteral("UserManagement");
}

bool UserManagementModule::start(PlatformModuleContext& context)
{
    if (!context.serviceRegistry) return false;

    m_service = std::make_unique<UserManagementServiceImpl>();
    m_service->setEventBus(context.eventBus);
    if (!m_service->initializeDatabase()) {
        m_service.reset();
        return false;
    }

    context.serviceRegistry->registerService(
        pluginId(),
        QStringLiteral("medical.UserManagementService"),
        m_service.get());
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("UserManagementService"), m_service.get());
    return true;
}

void UserManagementModule::stop(PlatformModuleContext& context)
{
    if (context.serviceRegistry) {
        context.serviceRegistry->unregisterPlugin(pluginId());
    }

    m_service.reset();
}
