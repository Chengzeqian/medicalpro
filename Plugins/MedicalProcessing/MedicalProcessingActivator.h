#ifndef MEDICAL_PROCESSING_ACTIVATOR_H
#define MEDICAL_PROCESSING_ACTIVATOR_H

#include <QObject>
#include <ctkPluginActivator.h>
#include <ctkServiceRegistration.h>

class MedicalProcessingServiceImpl;

/**
 * @brief Medical Processing Plugin Activator
 * 
 * 负责 MedicalProcessing 插件的生命周期管理：
 * - 插件启动时创建并注册服务
 * - 插件停止时清理资源
 */
class MedicalProcessingActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "medical.processing.MedicalProcessingActivator")

public:
    /**
     * @brief 插件启动
     * @param context CTK插件上下文
     */
    void start(ctkPluginContext* context) override;

    /**
     * @brief 插件停止
     * @param context CTK插件上下文
     */
    void stop(ctkPluginContext* context) override;

private:
    /**
     * @brief 初始化图像处理服务
     * @param context CTK插件上下文
     * @return 成功返回true
     */
    bool initializeProcessingService(ctkPluginContext* context);

    /**
     * @brief 清理资源
     */
    void cleanup();

    /**
     * @brief 验证依赖插件
     * @param context CTK插件上下文
     * @return 依赖满足返回true
     */
    bool validateDependencies(ctkPluginContext* context);

private:
    /// CTK插件上下文
    ctkPluginContext* m_context;
    
    /// 图像处理服务实现
    MedicalProcessingServiceImpl* m_processingService;
    
    /// 服务注册
    ctkServiceRegistration m_serviceRegistration;
    
    /// 初始化状态
    bool m_initialized;
};

#endif // MEDICAL_PROCESSING_ACTIVATOR_H
