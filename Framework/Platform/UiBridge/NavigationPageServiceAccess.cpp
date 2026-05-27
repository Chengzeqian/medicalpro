#include "Framework/Platform/UiBridge/NavigationPageServiceAccess.h"

#include "Framework/Platform/Contracts/PlatformUiPorts.h"

#include <QtGlobal>

namespace
{
const QString kInstrumentManagementPluginName = QStringLiteral("InstrumentManagement");
const QString kPointRegistrationPluginName = QStringLiteral("PointRegistration");
}

NavigationPageServiceAccess::NavigationPageServiceAccess(INavigationPageServicePort* port, QObject* parent)
    : QObject(parent)
    , m_port(port)
{
    Q_ASSERT(m_port);

    if (auto* eventSource = m_port->pluginEventSource()) {
        connect(eventSource, SIGNAL(pluginLoaded(QString)), this, SLOT(onPluginLoaded(QString)));
    }
}

InstrumentManagementService* NavigationPageServiceAccess::instrumentManagementService() const
{
    if (m_port->frameworkReady() && !m_port->isPluginStarted(kInstrumentManagementPluginName)) {
        m_port->startPlugin(kInstrumentManagementPluginName);
    }

    return m_port->instrumentManagementService();
}

DicomViewerService* NavigationPageServiceAccess::dicomViewerService() const
{
    return m_port->dicomViewerService();
}

BoneSegmentationService* NavigationPageServiceAccess::segmentationService() const
{
    return m_port->segmentationService();
}

FourViewDisplayService* NavigationPageServiceAccess::fourViewDisplayService() const
{
    return m_port->fourViewDisplayService();
}

OpticalTrackingService* NavigationPageServiceAccess::opticalTrackingService() const
{
    return m_port->opticalTrackingService();
}

bool NavigationPageServiceAccess::isPointRegistrationFrameworkReady() const
{
    return m_port->frameworkReady();
}

QString NavigationPageServiceAccess::pointRegistrationPluginState() const
{
    return m_port->pluginState(kPointRegistrationPluginName);
}

PointRegistrationService* NavigationPageServiceAccess::pointRegistrationService(bool tryStartPlugin) const
{
    if (!m_port->frameworkReady()) {
        return nullptr;
    }

    if (tryStartPlugin && !m_port->isPluginStarted(kPointRegistrationPluginName)) {
        m_port->startPlugin(kPointRegistrationPluginName);
    }

    return m_port->pointRegistrationService();
}

void NavigationPageServiceAccess::onPluginLoaded(const QString& pluginName)
{
    if (!pluginName.compare(kPointRegistrationPluginName, Qt::CaseInsensitive)) {
        emit pointRegistrationPluginAvailable();
    }
}
