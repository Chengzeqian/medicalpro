#include "UI/NewPages/Navigation/navigation_service_bundle.h"

#include "Framework/Platform/UiBridge/NavigationPageServiceAccess.h"

NavigationServiceBundle::NavigationServiceBundle(NavigationPageServiceAccess* serviceAccess)
    : m_serviceAccess(serviceAccess)
{
}

InstrumentManagementService* NavigationServiceBundle::instrumentManagementService() const
{
    return m_serviceAccess ? m_serviceAccess->instrumentManagementService() : nullptr;
}

DicomViewerService* NavigationServiceBundle::dicomViewerService() const
{
    return m_serviceAccess ? m_serviceAccess->dicomViewerService() : nullptr;
}

BoneSegmentationService* NavigationServiceBundle::segmentationService() const
{
    return m_serviceAccess ? m_serviceAccess->segmentationService() : nullptr;
}

FourViewDisplayService* NavigationServiceBundle::fourViewDisplayService() const
{
    if (!m_fourViewService && m_serviceAccess) {
        m_fourViewService = m_serviceAccess->fourViewDisplayService();
    }

    return m_fourViewService;
}

OpticalTrackingService* NavigationServiceBundle::opticalTrackingService() const
{
    if (!m_trackingService && m_serviceAccess) {
        m_trackingService = m_serviceAccess->opticalTrackingService();
    }

    return m_trackingService;
}

bool NavigationServiceBundle::isPointRegistrationFrameworkReady() const
{
    return m_serviceAccess && m_serviceAccess->isPointRegistrationFrameworkReady();
}

QString NavigationServiceBundle::pointRegistrationPluginState() const
{
    return m_serviceAccess ? m_serviceAccess->pointRegistrationPluginState() : QString();
}

PointRegistrationService* NavigationServiceBundle::pointRegistrationService(bool tryStartPlugin) const
{
    if (m_pointRegistrationService) {
        return m_pointRegistrationService;
    }

    if (!m_serviceAccess) {
        return nullptr;
    }

    m_pointRegistrationService = m_serviceAccess->pointRegistrationService(tryStartPlugin);
    return m_pointRegistrationService;
}
