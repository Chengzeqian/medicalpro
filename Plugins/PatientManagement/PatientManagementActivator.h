#ifndef PATIENT_MANAGEMENT_ACTIVATOR_H
#define PATIENT_MANAGEMENT_ACTIVATOR_H

#include <ctkPluginActivator.h>
#include <ctkServiceRegistration.h>

class PatientDatabaseServiceImpl;

/**
 * @brief PatientManagement插件激活器
 * 
 * 负责插件的启动和停止，管理服务的注册和注销。
 * 遵循CTK插件框架的标准激活器模式。
 */
class PatientManagementActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.medicalpro.PatientManagement")
    Q_INTERFACES(ctkPluginActivator)

public:
    /**
     * @brief 构造函数
     */
    PatientManagementActivator();
    
    /**
     * @brief 析构函数
     */
    ~PatientManagementActivator() override;

    /**
     * @brief 插件启动
     * 
     * 在插件被加载时调用，负责：
     * 1. 初始化数据库服务
     * 2. 注册PatientDatabaseService服务
     * 3. 设置必要的配置
     * 
     * @param context 插件上下文
     */
    void start(ctkPluginContext* context) override;

    /**
     * @brief 插件停止
     * 
     * 在插件被卸载时调用，负责：
     * 1. 注销所有服务
     * 2. 清理资源
     * 3. 断开数据库连接
     * 
     * @param context 插件上下文
     */
    void stop(ctkPluginContext* context) override;

private slots:
    /**
     * @brief 处理数据库初始化完成
     */
    void onDatabaseInitialized();
    
    /**
     * @brief 处理数据库错误
     * @param error 错误信息
     */
    void onDatabaseError(const QString& error);

private:
    /**
     * @brief 初始化数据库服务
     * @return 成功返回true，失败返回false
     */
    bool initializeDatabaseService();
    
    /**
     * @brief 注册服务到CTK服务框架
     * @return 成功返回true，失败返回false
     */
    bool registerServices();
    
    /**
     * @brief 注销所有已注册的服务
     */
    void unregisterServices();
    
    /**
     * @brief 记录插件日志
     * @param level 日志级别 (INFO, WARN, ERROR)
     * @param message 日志消息
     */
    void logMessage(const QString& level, const QString& message) const;

private:
    ctkPluginContext* m_context;                                    // 插件上下文
    PatientDatabaseServiceImpl* m_databaseService;                  // 数据库服务实例
    ctkServiceRegistration m_serviceRegistration; // 服务注册句柄
    bool m_servicesRegistered;                                      // 服务注册状态标志
};

#endif // PATIENT_MANAGEMENT_ACTIVATOR_H
