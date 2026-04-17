#ifndef REGISTRATIONACTIVATOR_H
#define REGISTRATIONACTIVATOR_H

#include <ctkPluginActivator.h>
#include <ctkServiceRegistration.h>

class RegistrationServiceImpl;

/**
 * @brief 配准核心插件激活器
 *
 * 负责插件的启动、停止和服务注册
 */
class RegistrationActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "org.medicalpro.RegistrationCore")

public:
    RegistrationActivator();
    virtual ~RegistrationActivator();

    void start(ctkPluginContext* context) override;
    void stop(ctkPluginContext* context) override;

private:
    RegistrationServiceImpl* m_serviceImpl;
    ctkServiceRegistration m_serviceRegistration;
};

#endif // REGISTRATIONACTIVATOR_H