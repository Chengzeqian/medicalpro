#include "OpticalRegistrationActivator.h"
#include "OpticalRegistrationServiceImpl.h"
#include "OpticalRegistrationService.h"
#include <QDebug>
#include <QPointer>
#include <QTimer>

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkDictionary.h>
#include <ctkPluginConstants.h>
#endif

//-----------------------------------------------------------------------------
OpticalRegistrationActivator::OpticalRegistrationActivator()
{
    logMessage("OpticalRegistrationActivator created");
}

//-----------------------------------------------------------------------------
OpticalRegistrationActivator::~OpticalRegistrationActivator()
{
    logMessage("OpticalRegistrationActivator destroyed");
}

#ifdef CTK_PLUGIN_FRAMEWORK
//-----------------------------------------------------------------------------
void OpticalRegistrationActivator::start(ctkPluginContext* context)
{
    logMessage("OpticalRegistration plugin starting...");

    if (!context) {
        logMessage("Plugin context is null, cannot start");
        return;
    }

    try {
        // 同步创建服务实现，避免 ServiceManager 获取服务时为 nullptr
        m_serviceImpl.reset(new OpticalRegistrationServiceImpl(context));
        m_serviceImpl->startService();

        // 准备服务属性
        ctkDictionary props;
        props.insert("service.description", "Optical Registration Service for medical image registration");
        props.insert("service.vendor", "Medical Pro");
        props.insert("service.version", "1.0.0");
        props.insert("name", "OpticalRegistration");
        props.insert(ctkPluginConstants::SERVICE_RANKING, 1);

        // 注册服务
        m_serviceRegistration = context->registerService<OpticalRegistrationService>(
            m_serviceImpl.data(), props);

        if (m_serviceRegistration) {
            logMessage("OpticalRegistrationService registered successfully");
        } else {
            logMessage("Failed to register OpticalRegistrationService");
        }

        logMessage("OpticalRegistration plugin started successfully");

        const QString serviceName = m_serviceImpl->getServiceName();
        const QString serviceVersion = m_serviceImpl->getServiceVersion();
        logMessage(QString("Service initialized: %1 v%2").arg(serviceName).arg(serviceVersion));

    } catch (const std::exception& e) {
        const QString error = QString("Failed to start OpticalRegistration plugin: %1").arg(e.what());
        logMessage(error);
        m_serviceImpl.reset();
    } catch (...) {
        const QString error = "Unknown error occurred while starting OpticalRegistration plugin";
        logMessage(error);
        m_serviceImpl.reset();
    }
}

//-----------------------------------------------------------------------------
void OpticalRegistrationActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context)
    
    logMessage("OpticalRegistration plugin stopping...");
    
    try {
        // 注销服务
        if (m_serviceRegistration) {
            m_serviceRegistration.unregister();
            logMessage("OpticalRegistrationService unregistered");
        }
        
        // 停止服务
        if (m_serviceImpl) {
            m_serviceImpl->stopService();
            m_serviceImpl.reset();
        }
        
        logMessage("OpticalRegistration plugin stopped successfully");
        
    } catch (const std::exception& e) {
        logMessage(QString("Error while stopping OpticalRegistration plugin: %1").arg(e.what()));
    } catch (...) {
        logMessage("Unknown error occurred while stopping OpticalRegistration plugin");
    }
}
#endif

//-----------------------------------------------------------------------------
void OpticalRegistrationActivator::logMessage(const QString& message)
{
    qDebug() << "[OpticalRegistrationActivator]" << message;
}

