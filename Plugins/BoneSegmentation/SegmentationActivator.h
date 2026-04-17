#ifndef SEGMENTATIONACTIVATOR_H
#define SEGMENTATIONACTIVATOR_H

#include <ctkPluginActivator.h>
#include <ctkServiceRegistration.h>

class SegmentationServiceImpl;

/**
 * @brief 骨骼分割插件激活器
 *
 * 负责插件的启动、停止和服务注册
 */
class SegmentationActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "org.medicalpro.BoneSegmentation")

public:
    SegmentationActivator();
    virtual ~SegmentationActivator();

    void start(ctkPluginContext* context) override;
    void stop(ctkPluginContext* context) override;

private:
    SegmentationServiceImpl* m_serviceImpl;
    ctkServiceRegistration m_serviceRegistration;
};

#endif // SEGMENTATIONACTIVATOR_H
