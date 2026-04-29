#include "instrument_management_module.h"

#include "Framework/Platform/Kernel/platform_service_registry.h"

InstrumentManagementModule::~InstrumentManagementModule() = default;

QString InstrumentManagementModule::pluginId() const
{
    return QStringLiteral("InstrumentManagement");
}

bool InstrumentManagementModule::start(PlatformModuleContext& context)
{
    if (!context.serviceRegistry) return false;

    m_service = std::make_unique<InstrumentManagementServiceImpl>();
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("InstrumentManagementService"), m_service.get());
    return true;
}

void InstrumentManagementModule::stop(PlatformModuleContext& context)
{
    if (context.serviceRegistry) {
        context.serviceRegistry->unregisterPlugin(pluginId());
    }

    m_service.reset();
}
