#ifndef MEDICAL_VIEWER_ACTIVATOR_H
#define MEDICAL_VIEWER_ACTIVATOR_H

#include <QObject>
#include <ctkPluginActivator.h>
#include <ctkServiceRegistration.h>

class MedicalViewerServiceImpl;

/**
 * @brief Medical Viewer Plugin Activator
 * 
 * 负责 MedicalViewer 插件的生命周期管理：
 * - 插件启动时创建并注册显示服务
 * - 插件停止时清理资源
 */
class MedicalViewerActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "medical.viewer.MedicalViewerActivator")

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
     * @brief 初始化图像显示服务
     * @param context CTK插件上下文
     * @return 成功返回true
     */
    bool initializeViewerService(ctkPluginContext* context);

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

    /**
     * @brief 检查VTK可用性
     * @return VTK可用返回true
     */
    bool checkVTKAvailability();

private:
    /// CTK插件上下文
    ctkPluginContext* m_context;
    
    /// 图像显示服务实现
    MedicalViewerServiceImpl* m_viewerService;
    
    /// 服务注册
    ctkServiceRegistration m_serviceRegistration;
    
    /// 初始化状态
    bool m_initialized;
};

#endif // MEDICAL_VIEWER_ACTIVATOR_H
