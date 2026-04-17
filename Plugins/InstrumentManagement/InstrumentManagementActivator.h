#ifndef INSTRUMENT_MANAGEMENT_ACTIVATOR_H
#define INSTRUMENT_MANAGEMENT_ACTIVATOR_H

#include <ctkPluginActivator.h>
#include <QObject>

class InstrumentManagementService;

/**
 * @brief 器械管理插件激活器
 */
class InstrumentManagementActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "com.medicalpro.InstrumentManagement")
    
public:
    InstrumentManagementActivator();
    ~InstrumentManagementActivator() override;
    
    void start(ctkPluginContext* context) override;
    void stop(ctkPluginContext* context) override;
    
private:
    ctkPluginContext* m_context;
    InstrumentManagementService* m_service;
};

#endif // INSTRUMENT_MANAGEMENT_ACTIVATOR_H

