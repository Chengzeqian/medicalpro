#include "ImageInteractionActivator.h"
#include <QDebug>
#include <stdexcept>

ImageInteractionActivator::ImageInteractionActivator()
{
    logMessage("ImageInteractionActivator created");
}

ImageInteractionActivator::~ImageInteractionActivator()
{
    logMessage("ImageInteractionActivator destroyed");
}

#ifdef CTK_PLUGIN_FRAMEWORK
void ImageInteractionActivator::start(ctkPluginContext* context)
{
    logMessage("ImageInteraction plugin starting...");
    
    try {
        // 创建交互服务实现实例
        m_serviceImpl.reset(new ImageInteractionServiceImpl(context));
        
        // 注册服务到CTK框架
        ctkDictionary serviceProps;
        serviceProps["service.description"] = "Medical Image Interaction Service";
        serviceProps["plugin.name"] = "ImageInteraction";
        serviceProps["interaction.types"] = "point_picking,measurement,annotation";
        serviceProps["ui.support"] = "true";
        
        m_serviceRegistration = context->registerService<ImageInteractionService>(
            m_serviceImpl.data(), serviceProps);
        
        if (!m_serviceRegistration) {
            throw std::runtime_error("图像交互服务注册失败");
        }
        
        // 设置服务连接
        setupServiceConnections();
        
        // 启动服务
        m_serviceImpl->startService();
        
        logMessage("ImageInteraction plugin started successfully");
        
        // 测试服务功能
        QString serviceName = m_serviceImpl->getServiceName();
        QString serviceVersion = m_serviceImpl->getServiceVersion();
        bool isActive = m_serviceImpl->isActive();
        
        logMessage(QString("Service initialized: %1 v%2 (Active: %3)")
                   .arg(serviceName)
                   .arg(serviceVersion)
                   .arg(isActive ? "Yes" : "No"));
        
        // 记录服务能力
        logMessage("Available interaction features:");
        logMessage("- 3D point picking");
        logMessage("- Point visualization");
        logMessage("- Distance measurement");
        logMessage("- CTK event communication");
        
    } catch (const std::exception& e) {
        QString error = QString("Failed to start ImageInteraction plugin: %1").arg(e.what());
        logMessage(error);
        throw;
    } catch (...) {
        QString error = "Unknown error occurred while starting ImageInteraction plugin";
        logMessage(error);
        throw;
    }
}

void ImageInteractionActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context)
    
    logMessage("ImageInteraction plugin stopping...");
    
    try {
        // 注销服务
        if (m_serviceRegistration) {
            m_serviceRegistration.unregister();
            logMessage("图像交互服务已从CTK框架注销");
        }
        
        // 停止服务
        if (m_serviceImpl) {
            m_serviceImpl->stopService();
            
            // 清理资源
            if (m_serviceImpl->isPointPickingEnabled()) {
                m_serviceImpl->enablePointPicking("global", false);
            }
            m_serviceImpl->clearPoints();
            
            // 记录服务统计
            int pointCount = m_serviceImpl->getPointCount();
            logMessage(QString("Service stopped with %1 points remaining").arg(pointCount));
            
            m_serviceImpl.reset();
        }
        
        logMessage("ImageInteraction plugin stopped successfully");
        
    } catch (const std::exception& e) {
        logMessage(QString("Error while stopping ImageInteraction plugin: %1").arg(e.what()));
    } catch (...) {
        logMessage("Unknown error occurred while stopping ImageInteraction plugin");
    }
}
#endif

void ImageInteractionActivator::setupServiceConnections()
{
    if (!m_serviceImpl) {
        return;
    }
    
    // 连接服务状态信号
    connect(m_serviceImpl.data(), &ImageInteractionServiceImpl::serviceStatusChanged,
            this, &ImageInteractionActivator::onServiceStatusChanged);
    
    // 连接交互事件信号
    connect(m_serviceImpl.data(), &ImageInteractionServiceImpl::interactionEvent,
            this, &ImageInteractionActivator::onInteractionEvent);
    
    // 连接具体的交互信号
    connect(m_serviceImpl.data(), &ImageInteractionServiceImpl::pointPicked,
            this, [this](const QVector3D& point, int index, int totalCount) {
                logMessage(QString("Point picked notification: Point %1 of %2 at (%3, %4, %5)")
                           .arg(index + 1)
                           .arg(totalCount)
                           .arg(point.x(), 0, 'f', 2)
                           .arg(point.y(), 0, 'f', 2)
                           .arg(point.z(), 0, 'f', 2));
            });
    
    connect(m_serviceImpl.data(), &ImageInteractionServiceImpl::pointRemoved,
            this, [this](int index, int totalCount) {
                logMessage(QString("Point removed notification: Index %1, %2 points remaining")
                           .arg(index).arg(totalCount));
            });
    
    connect(m_serviceImpl.data(), &ImageInteractionServiceImpl::allPointsCleared,
            this, [this]() {
                logMessage("All points cleared notification");
            });
    
    connect(m_serviceImpl.data(), &ImageInteractionServiceImpl::pointPickingModeChanged,
            this, [this](bool enabled) {
                logMessage(QString("Point picking mode changed: %1")
                           .arg(enabled ? "Enabled" : "Disabled"));
            });
    
    logMessage("Service signal connections established");
}

void ImageInteractionActivator::onServiceStatusChanged(bool active)
{
    logMessage(QString("Service status changed: %1").arg(active ? "Active" : "Inactive"));
    
    if (active) {
        logMessage("ImageInteraction service is ready for use");
        logMessage("Other plugins can now access interaction features via CTK service registry");
    } else {
        logMessage("ImageInteraction service has been deactivated");
    }
}

void ImageInteractionActivator::onInteractionEvent(const QString& eventType, const QVariantMap& data)
{
    QString summary = QString("Interaction event: %1").arg(eventType);
    
    // 添加事件特定的详细信息
    if (eventType == "point_picked") {
        int index = data.value("index", -1).toInt();
        int totalCount = data.value("total_count", 0).toInt();
        summary += QString(" (Point %1 of %2)").arg(index + 1).arg(totalCount);
    } else if (eventType == "point_removed") {
        int index = data.value("index", -1).toInt();
        int remainingCount = data.value("remaining_count", 0).toInt();
        summary += QString(" (Index %1, %2 remaining)").arg(index).arg(remainingCount);
    }
    
    logMessage(summary);
}

void ImageInteractionActivator::logMessage(const QString& message)
{
    qDebug() << "[ImageInteractionActivator]" << message;
}
