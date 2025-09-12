#ifndef OPTICALTRACKINGACTIVATOR_H
#define OPTICALTRACKINGACTIVATOR_H

#include <QObject>

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkPluginActivator.h>
#include <ctkPluginContext.h>
#include <ctkServiceRegistration.h>
#include <QScopedPointer>
#endif

#include "OpticalTrackingServiceImpl.h"

/**
 * @brief 光学追踪插件激活器
 * 负责插件的启动和停止，管理光学追踪服务的生命周期
 * 遵循SamplePlugin的开发范式
 */
class OpticalTrackingActivator : public QObject
#ifdef CTK_PLUGIN_FRAMEWORK
    , public ctkPluginActivator
#endif
{
    Q_OBJECT
    
#ifdef CTK_PLUGIN_FRAMEWORK
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "org.medicalpro.OpticalTracking")
#endif

public:
    OpticalTrackingActivator();
    ~OpticalTrackingActivator() override;

#ifdef CTK_PLUGIN_FRAMEWORK
    /**
     * @brief 插件启动时调用
     * @param context 插件上下文
     */
    void start(ctkPluginContext* context) override;
    
    /**
     * @brief 插件停止时调用
     * @param context 插件上下文
     */
    void stop(ctkPluginContext* context) override;
#endif

private:
    QScopedPointer<OpticalTrackingServiceImpl> m_serviceImpl;
    
#ifdef CTK_PLUGIN_FRAMEWORK
    ctkServiceRegistration m_serviceRegistration;
#endif
    
    void logMessage(const QString& message);
};

#endif // OPTICALTRACKINGACTIVATOR_H