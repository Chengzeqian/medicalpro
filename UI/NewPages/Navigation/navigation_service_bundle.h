#pragma once

#include <QString>

class NavigationPageServiceAccess;
class BoneSegmentationService;
class DicomViewerService;
class FourViewDisplayService;
class InstrumentManagementService;
class OpticalTrackingService;
class PointRegistrationService;

class NavigationServiceBundle
{
public:
    explicit NavigationServiceBundle(NavigationPageServiceAccess* serviceAccess);

    InstrumentManagementService* instrumentManagementService() const;
    DicomViewerService* dicomViewerService() const;
    BoneSegmentationService* segmentationService() const;
    FourViewDisplayService* fourViewDisplayService() const;
    OpticalTrackingService* opticalTrackingService() const;
    bool isPointRegistrationFrameworkReady() const;
    QString pointRegistrationPluginState() const;
    PointRegistrationService* pointRegistrationService(bool tryStartPlugin) const;

private:
    NavigationPageServiceAccess* m_serviceAccess = nullptr;
    mutable FourViewDisplayService* m_fourViewService = nullptr;
    mutable OpticalTrackingService* m_trackingService = nullptr;
    mutable PointRegistrationService* m_pointRegistrationService = nullptr;
};
