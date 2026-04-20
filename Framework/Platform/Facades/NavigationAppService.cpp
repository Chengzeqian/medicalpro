#include "Framework/Platform/Facades/NavigationAppService.h"

#include <QtGlobal>

NavigationAppService::NavigationAppService(INavigationFacadePort* port)
    : m_port(port)
{
    Q_ASSERT(m_port);
}

bool NavigationAppService::ensureReady(const QString& pluginId)
{
    Q_ASSERT(m_port);
    return m_port->ensureReady(pluginId);
}
