#ifndef OPTICAL_REGISTRATION_ACTIVATOR_H
#define OPTICAL_REGISTRATION_ACTIVATOR_H

#include <QObject>

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkPluginActivator.h>
#include <ctkPluginContext.h>
#include <ctkServiceRegistration.h>
#include <QScopedPointer>
#endif

#include "OpticalRegistrationServiceImpl.h"

/**
 * @brief 光学配准插件激活器
 * 
 * 负责插件的启动和停止，管理光学配准服务的生命周期。
 * 遵循CTK插件架构设计模式。
 */
class OpticalRegistrationActivator : public QObject
#ifdef CTK_PLUGIN_FRAMEWORK
    , public ctkPluginActivator
#endif
{
    Q_OBJECT
    
#ifdef CTK_PLUGIN_FRAMEWORK
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "org.medicalpro.OpticalRegistration")
#endif

public:
    OpticalRegistrationActivator();
    ~OpticalRegistrationActivator() override;

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
    /**
     * @brief 日志输出
     */
    void logMessage(const QString& message);
    
    QScopedPointer<OpticalRegistrationServiceImpl> m_serviceImpl;
    
#ifdef CTK_PLUGIN_FRAMEWORK
    ctkServiceRegistration m_serviceRegistration;
#endif
};

#endif // OPTICAL_REGISTRATION_ACTIVATOR_H

