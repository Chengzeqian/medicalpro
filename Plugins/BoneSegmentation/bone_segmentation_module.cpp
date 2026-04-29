#include "bone_segmentation_module.h"

#include "Framework/Platform/Kernel/platform_service_registry.h"

BoneSegmentationModule::~BoneSegmentationModule() = default;

QString BoneSegmentationModule::pluginId() const
{
    return QStringLiteral("BoneSegmentation");
}

bool BoneSegmentationModule::start(PlatformModuleContext& context)
{
    if (!context.serviceRegistry) return false;

    m_service = std::make_unique<SegmentationServiceImpl>();
    context.serviceRegistry->registerService(pluginId(), QStringLiteral("SegmentationService"), m_service.get());
    return true;
}

void BoneSegmentationModule::stop(PlatformModuleContext& context)
{
    if (m_service) {
        const QStringList activeTasks = m_service->getActiveTasks();
        for (const QString& taskId : activeTasks) {
            m_service->cancelTask(taskId);
        }
        m_service->cleanupTempFiles();
    }

    if (context.serviceRegistry) {
        context.serviceRegistry->unregisterPlugin(pluginId());
    }

    m_service.reset();
}
