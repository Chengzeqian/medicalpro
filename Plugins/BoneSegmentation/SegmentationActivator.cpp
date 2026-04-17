#include "SegmentationActivator.h"
#include "SegmentationServiceImpl.h"
#include "SegmentationService.h"
#include <ctkPluginContext.h>
#include <QDebug>

SegmentationActivator::SegmentationActivator()
    : m_serviceImpl(nullptr)
{
}

SegmentationActivator::~SegmentationActivator()
{
}

void SegmentationActivator::start(ctkPluginContext* context)
{
    qDebug() << "[BoneSegmentation] Plugin starting...";

    try {
        // 创建服务实现
        m_serviceImpl = new SegmentationServiceImpl();

        // 注册服务（添加服务属性，与 DicomViewer 保持一致）
        ctkDictionary properties;
        properties.insert("service.description", "AI-powered bone segmentation service");
        properties.insert("service.vendor", "MedicalPro");
        properties.insert("service.version", "1.0.0");
        properties.insert("plugin.category", "Medical");

        m_serviceRegistration = context->registerService<SegmentationService>(m_serviceImpl, properties);

        qDebug() << "[BoneSegmentation] Service registered successfully";

        // 检查 Python 环境
        if (m_serviceImpl->checkPythonEnvironment()) {
            qInfo() << "[BoneSegmentation] Python environment available";
        } else {
            qWarning() << "[BoneSegmentation] Python environment not configured";
        }

    } catch (const std::exception& ex) {
        qCritical() << "[BoneSegmentation] Failed to start plugin:" << ex.what();
        throw;
    }
}

void SegmentationActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context);

    qDebug() << "[BoneSegmentation] Plugin stopping...";

    // 清理资源
    if (m_serviceImpl) {
        // 取消所有正在运行的任务
        QStringList activeTasks = m_serviceImpl->getActiveTasks();
        for (const QString& taskId : activeTasks) {
            m_serviceImpl->cancelTask(taskId);
        }

        // 清理临时文件
        m_serviceImpl->cleanupTempFiles();
    }

    // 注销服务
    m_serviceRegistration.unregister();

    // 删除服务实现
    if (m_serviceImpl) {
        delete m_serviceImpl;
        m_serviceImpl = nullptr;
    }

    qDebug() << "[BoneSegmentation] Plugin stopped";
}
