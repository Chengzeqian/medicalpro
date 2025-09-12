#include "SampleActivator.h"
#include <QDebug>

SampleActivator::SampleActivator()
{
    logMessage("SampleActivator created");
}

SampleActivator::~SampleActivator()
{
    logMessage("SampleActivator destroyed");
}

#ifdef CTK_PLUGIN_FRAMEWORK
void SampleActivator::start(ctkPluginContext* context)
{
    logMessage("SamplePlugin starting...");
    
    try {
        // 创建服务实现实例
        m_serviceImpl.reset(new SampleServiceImpl(context));
        
        // 启动服务
        m_serviceImpl->startService();
        
        logMessage("SamplePlugin started successfully");
        
        // 测试服务功能
        QString testResult = m_serviceImpl->performOperation("Plugin initialization test");
        logMessage(QString("Plugin test result: %1").arg(testResult));
        
    } catch (const std::exception& e) {
        QString error = QString("Failed to start SamplePlugin: %1").arg(e.what());
        logMessage(error);
        throw;
    } catch (...) {
        QString error = "Unknown error occurred while starting SamplePlugin";
        logMessage(error);
        throw;
    }
}

void SampleActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context)
    
    logMessage("SamplePlugin stopping...");
    
    try {
        // 停止服务
        if (m_serviceImpl) {
            m_serviceImpl->stopService();
            m_serviceImpl.reset();
        }
        
        logMessage("SamplePlugin stopped successfully");
        
    } catch (const std::exception& e) {
        logMessage(QString("Error while stopping SamplePlugin: %1").arg(e.what()));
    } catch (...) {
        logMessage("Unknown error occurred while stopping SamplePlugin");
    }
}
#endif

void SampleActivator::logMessage(const QString& message)
{
    qDebug() << "[SampleActivator]" << message;
}
