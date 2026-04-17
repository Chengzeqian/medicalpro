#ifndef POINT_REGISTRATION_ACTIVATOR_H
#define POINT_REGISTRATION_ACTIVATOR_H

/**
 * @file PointRegistrationActivator.h
 * @brief 点配准插件激活器
 *
 * CTK插件框架的标准激活器
 */

#include <ctkPluginActivator.h>
#include <ctkServiceRegistration.h>

class PointRegistrationServiceImpl;

/**
 * @brief 点配准插件激活器
 *
 * 负责:
 * 1. 插件启动时创建服务实例并注册到框架
 * 2. 插件停止时注销服务并清理资源
 */
class PointRegistrationActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.medicalpro.PointRegistration")
    Q_INTERFACES(ctkPluginActivator)

public:
    PointRegistrationActivator();
    ~PointRegistrationActivator() override;

    /**
     * @brief 启动插件
     * @param context CTK插件上下文
     */
    void start(ctkPluginContext* context) override;

    /**
     * @brief 停止插件
     * @param context CTK插件上下文
     */
    void stop(ctkPluginContext* context) override;

private:
    /**
     * @brief 初始化服务
     * @return 成功返回true
     */
    bool initializeService();

    /**
     * @brief 注册服务
     * @return 成功返回true
     */
    bool registerService();

    /**
     * @brief 注销服务
     */
    void unregisterService();

    /**
     * @brief 日志输出
     */
    void logMessage(const QString& level, const QString& message) const;

private:
    ctkPluginContext* m_context;                    ///< 插件上下文
    PointRegistrationServiceImpl* m_service;        ///< 服务实例
    ctkServiceRegistration m_serviceRegistration;   ///< 服务注册对象
    bool m_serviceRegistered;                       ///< 服务是否已注册
};

#endif // POINT_REGISTRATION_ACTIVATOR_H

