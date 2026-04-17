#ifndef REGISTRATION2D3D_ACTIVATOR_H
#define REGISTRATION2D3D_ACTIVATOR_H

#include <ctkPluginActivator.h>
#include <QObject>

class Registration2D3DService;
class Registration2D3DServiceImpl;

/**
 * @brief 2D3D配准插件激活器
 * 
 * CTK插件框架的入口点，负责插件的启动和停止
 */
class Registration2D3DActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "com.medicalpro.Registration2D3D")
    
public:
    Registration2D3DActivator();
    ~Registration2D3DActivator() override;
    
    /**
     * @brief 插件启动
     * @param context 插件上下文
     */
    void start(ctkPluginContext* context) override;
    
    /**
     * @brief 插件停止
     * @param context 插件上下文
     */
    void stop(ctkPluginContext* context) override;
    
private:
    Registration2D3DServiceImpl* m_serviceImpl;  // 实现类指针，用于调用实现细节
    Registration2D3DService* m_service;          // 服务接口指针，用于注册服务
};

#endif // REGISTRATION2D3D_ACTIVATOR_H

