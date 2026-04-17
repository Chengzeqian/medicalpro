#ifndef DICOMVIEWERACTIVATOR_H
#define DICOMVIEWERACTIVATOR_H

#include <ctkPluginActivator.h>
#include <ctkServiceRegistration.h>

class DicomViewerServiceImpl;

/**
 * @brief DICOM查看器插件激活器
 * 
 * 负责插件的启动、停止和服务注册
 */
class DicomViewerActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "org.medicalpro.DicomViewer")

public:
    DicomViewerActivator();
    virtual ~DicomViewerActivator();

    void start(ctkPluginContext* context) override;
    void stop(ctkPluginContext* context) override;

private:
    DicomViewerServiceImpl* m_serviceImpl;
    ctkServiceRegistration m_serviceRegistration;
};

#endif // DICOMVIEWERACTIVATOR_H
