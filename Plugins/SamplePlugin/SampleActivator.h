#ifndef SAMPLEACTIVATOR_H
#define SAMPLEACTIVATOR_H

#include <QObject>

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkPluginActivator.h>
#include <ctkPluginContext.h>
#include <QScopedPointer>
#endif

#include "SampleServiceImpl.h"

/**
 * @brief 示例插件激活器
 * 负责插件的启动和停止，管理插件内部服务的生命周期
 */
class SampleActivator : public QObject
#ifdef CTK_PLUGIN_FRAMEWORK
    , public ctkPluginActivator
#endif
{
    Q_OBJECT
    
#ifdef CTK_PLUGIN_FRAMEWORK
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "org.medicalpro.SamplePlugin")
#endif

public:
    SampleActivator();
    ~SampleActivator() override;

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
    QScopedPointer<SampleServiceImpl> m_serviceImpl;
    
    void logMessage(const QString& message);
};

#endif // SAMPLEACTIVATOR_H
