#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformFacadePorts.h"

class FRAMEWORK_EXPORT NavigationAppService
{
public:
    explicit NavigationAppService(INavigationFacadePort* port);
    bool ensureReady(const QString& pluginId);

private:
    INavigationFacadePort* m_port;
};
