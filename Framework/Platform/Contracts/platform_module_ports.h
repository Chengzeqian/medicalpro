#pragma once

#include "Framework/FrameworkExport.h"

#include <QString>

class PlatformServiceRegistry;
class IPlatformEventBusPort;

struct PlatformModuleContext
{
    PlatformServiceRegistry* serviceRegistry = nullptr;
    IPlatformEventBusPort* eventBus = nullptr;
};

class FRAMEWORK_EXPORT IPlatformModuleActivator
{
public:
    virtual ~IPlatformModuleActivator() = default;

    virtual QString pluginId() const = 0;
    virtual bool start(PlatformModuleContext& context) = 0;
    virtual void stop(PlatformModuleContext& context) = 0;
};
