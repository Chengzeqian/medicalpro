#ifndef IMAGEINTERACTIONACTIVATOR_H
#define IMAGEINTERACTIONACTIVATOR_H

#include <QObject>

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkPluginActivator.h>
#include <ctkPluginContext.h>
#include <service/event/ctkEventAdmin.h>
#include <service/event/ctkEvent.h>
#include <QScopedPointer>
#include <QVector3D>
#endif

#include "ImageInteractionServiceImpl.h"

/**
 * @brief 图像交互插件激活器
 * 负责插件的启动和停止，管理交互服务的生命周期
 */
class ImageInteractionActivator : public QObject
#ifdef CTK_PLUGIN_FRAMEWORK
    , public ctkPluginActivator
#endif
{
    Q_OBJECT
    
#ifdef CTK_PLUGIN_FRAMEWORK
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "org.medicalpro.ImageInteraction")
#endif

public:
    ImageInteractionActivator();
    ~ImageInteractionActivator() override;

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

private slots:
    void onServiceStatusChanged(bool active);
    void onInteractionEvent(const QString& eventType, const QVariantMap& data);

private:
    QScopedPointer<ImageInteractionServiceImpl> m_serviceImpl;
    
#ifdef CTK_PLUGIN_FRAMEWORK
    ctkServiceRegistration m_serviceRegistration;
#endif
    
    void logMessage(const QString& message);
    void setupServiceConnections();
};

#endif // IMAGEINTERACTIONACTIVATOR_H
