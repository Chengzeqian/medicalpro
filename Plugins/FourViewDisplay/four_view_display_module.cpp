#include "four_view_display_module.h"

#include "Framework/Platform/Kernel/platform_service_registry.h"

FourViewDisplayModule::~FourViewDisplayModule() = default;

QString FourViewDisplayModule::pluginId() const
{
    return QStringLiteral("FourViewDisplay");
}

bool FourViewDisplayModule::start(PlatformModuleContext& context)
{
    if (!context.serviceRegistry) return false;

    m_service = std::make_unique<FourViewDisplayServiceImpl>();
    context.serviceRegistry->registerService(
        pluginId(),
        QStringLiteral("com.medicalpro.FourViewDisplayService"),
        m_service.get());
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("FourViewDisplayService"), m_service.get());
    return true;
}

void FourViewDisplayModule::stop(PlatformModuleContext& context)
{
    if (context.serviceRegistry) {
        context.serviceRegistry->unregisterPlugin(pluginId());
    }

    m_service.reset();
}
