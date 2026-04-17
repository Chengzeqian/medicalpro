#include "OpticalTrackingActivator.h"
#include "OpticalTrackingServiceImpl.h"
#include "OpticalTrackingService.h"
#include <QDebug>
#include <QPointer>
#include <QTimer>

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkDictionary.h>
#include <ctkPluginConstants.h>
#endif

OpticalTrackingActivator::OpticalTrackingActivator()
{
    logMessage("OpticalTrackingActivator created");
}

OpticalTrackingActivator::~OpticalTrackingActivator()
{
    logMessage("OpticalTrackingActivator destroyed");
}

#ifdef CTK_PLUGIN_FRAMEWORK
void OpticalTrackingActivator::start(ctkPluginContext* context)
{
    logMessage("OpticalTracking plugin starting...");

    if (!context) {
        logMessage("Plugin context is null, cannot start");
        return;
    }

    ctkPluginContext* safeContext = context;

    QTimer::singleShot(0, this, [this, safeContext]() {
        if (!safeContext) {
            logMessage("Plugin context unavailable, skipping initialization");
            return;
        }

        try {
            m_serviceImpl.reset(new OpticalTrackingServiceImpl(safeContext));
            m_serviceImpl->startService();

            ctkDictionary props;
            props.insert("service.description", "Optical Tracking Service for Atracsys fusionTrack 500");
            props.insert("service.vendor", "Medical Pro");
            props.insert("service.version", "1.0");
            props.insert("name", "OpticalTracking");
            props.insert(ctkPluginConstants::SERVICE_RANKING, 1);

            m_serviceRegistration = safeContext->registerService<OpticalTrackingService>(
                m_serviceImpl.data(), props);

            if (m_serviceRegistration) {
                logMessage("TrackingService registered successfully");
            } else {
                logMessage("Failed to register TrackingService");
            }

            logMessage("OpticalTracking plugin started successfully");

            const QString serviceName = m_serviceImpl->getServiceName();
            const QString serviceVersion = m_serviceImpl->getServiceVersion();
            logMessage(QString("Service initialized: %1 v%2").arg(serviceName).arg(serviceVersion));

        } catch (const std::exception& e) {
            const QString error = QString("Failed to start OpticalTracking plugin: %1").arg(e.what());
            logMessage(error);
            m_serviceImpl.reset();
        } catch (...) {
            const QString error = "Unknown error occurred while starting OpticalTracking plugin";
            logMessage(error);
            m_serviceImpl.reset();
        }
    });
}

void OpticalTrackingActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context)
    
    logMessage("OpticalTracking plugin stopping...");
    
    try {
        // 注销服务
        if (m_serviceRegistration) {
            m_serviceRegistration.unregister();
            logMessage("TrackingService unregistered");
        }
        
        // 停止服务
        if (m_serviceImpl) {
            m_serviceImpl->stopService();
            m_serviceImpl.reset();
        }
        
        logMessage("OpticalTracking plugin stopped successfully");
        
    } catch (const std::exception& e) {
        logMessage(QString("Error while stopping OpticalTracking plugin: %1").arg(e.what()));
    } catch (...) {
        logMessage("Unknown error occurred while stopping OpticalTracking plugin");
    }
}
#endif

void OpticalTrackingActivator::logMessage(const QString& message)
{
    qDebug() << "[OpticalTrackingActivator]" << message;
}
