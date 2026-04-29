#include "dicom_viewer_module.h"

#include "Framework/Platform/Kernel/platform_service_registry.h"

DicomViewerModule::~DicomViewerModule() = default;

QString DicomViewerModule::pluginId() const
{
    return QStringLiteral("DicomViewer");
}

bool DicomViewerModule::start(PlatformModuleContext& context)
{
    if (!context.serviceRegistry) return false;

    m_service = std::make_unique<DicomViewerServiceImpl>();
    if (!m_service->initialize()) {
        m_service.reset();
        return false;
    }

    context.serviceRegistry->registerService(pluginId(), QStringLiteral("DicomViewerService"), m_service.get());
    return true;
}

void DicomViewerModule::stop(PlatformModuleContext& context)
{
    if (m_service) {
        m_service->shutdown();
    }
    if (context.serviceRegistry) {
        context.serviceRegistry->unregisterPlugin(pluginId());
    }

    m_service.reset();
}
