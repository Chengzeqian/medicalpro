#include "Framework/Platform/UiBridge/CoreUiRuntimeStatusProvider.h"

#include "Framework/Platform/Contracts/PlatformUiPorts.h"

#include <QtGlobal>

namespace
{
const QStringList kRequiredCoreUiServices = {
    QStringLiteral("UserManagementService"),
    QStringLiteral("DicomViewerService"),
    QStringLiteral("FourViewDisplayService")
};
}

CoreUiRuntimeStatusProvider::CoreUiRuntimeStatusProvider(ICoreUiRuntimeStatusPort* port, QString dataDirectoryPath)
    : m_port(port)
    , m_dataDirectoryPath(std::move(dataDirectoryPath))
{
    Q_ASSERT(m_port);
    Q_ASSERT(!m_dataDirectoryPath.isEmpty());
}

CoreUiRuntimeStatusSnapshot CoreUiRuntimeStatusProvider::welcomeSnapshot() const
{
    return buildSnapshot(qMax(m_port->installedPlugins().size(), m_port->startedPlugins().size()));
}

CoreUiRuntimeStatusSnapshot CoreUiRuntimeStatusProvider::moduleSelectionSnapshot() const
{
    return buildSnapshot(0);
}

CoreUiRuntimeStatusSnapshot CoreUiRuntimeStatusProvider::systemSettingsSnapshot() const
{
    return buildSnapshot(m_port->loadedPlugins().size());
}

CoreUiRuntimeStatusSnapshot CoreUiRuntimeStatusProvider::buildSnapshot(int pluginCount) const
{
    CoreUiRuntimeStatusSnapshot snapshot;
    snapshot.frameworkReady = m_port->frameworkReady();
    snapshot.pluginCount = pluginCount;
    snapshot.totalServices = kRequiredCoreUiServices.size();
    snapshot.missingServices = m_port->missingServices(kRequiredCoreUiServices);
    snapshot.readyServices = snapshot.totalServices - snapshot.missingServices.size();
    if (snapshot.readyServices < 0) snapshot.readyServices = 0;
    if (snapshot.readyServices > snapshot.totalServices) snapshot.readyServices = snapshot.totalServices;
    snapshot.workflowReady = snapshot.frameworkReady && snapshot.readyServices == snapshot.totalServices;
    snapshot.dataDirectoryExists = m_port->directoryExists(m_dataDirectoryPath);
    snapshot.dataDirectoryReadable = snapshot.dataDirectoryExists && m_port->directoryReadable(m_dataDirectoryPath);
    return snapshot;
}
