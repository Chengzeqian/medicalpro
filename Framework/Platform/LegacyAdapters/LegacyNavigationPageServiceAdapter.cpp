#include "Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.h"

#include "Framework/Platform/Kernel/platform_plugin_host.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"

#include "Plugins/BoneSegmentation/SegmentationService.h"
#include "Plugins/DicomViewer/DicomViewerService.h"
#include "Plugins/FourViewDisplay/FourViewDisplayService.h"
#include "Plugins/InstrumentManagement/InstrumentManagementService.h"
#include "Plugins/OpticalTracking/OpticalTrackingService.h"
#include "Plugins/PointRegistration/PointRegistrationService.h"

namespace
{
PlatformPluginHost& pluginHost()
{
    return PlatformPluginHost::sharedInstance();
}

PlatformServiceRegistry* serviceRegistry()
{
    return pluginHost().serviceRegistry();
}
}

bool LegacyNavigationPageServiceAdapter::frameworkReady() const
{
    return !pluginHost().registeredPluginIds().isEmpty();
}

QObject* LegacyNavigationPageServiceAdapter::pluginEventSource() const
{
    return nullptr;
}

bool LegacyNavigationPageServiceAdapter::isPluginStarted(const QString& pluginName) const
{
    return pluginHost().isModuleStarted(pluginName);
}

bool LegacyNavigationPageServiceAdapter::startPlugin(const QString& pluginName)
{
    return pluginHost().hasActivator(pluginName) && pluginHost().startModule(pluginName);
}

QString LegacyNavigationPageServiceAdapter::pluginState(const QString& pluginName) const
{
    if (pluginHost().isModuleStarted(pluginName)) return QStringLiteral("ACTIVE");
    if (pluginHost().hasActivator(pluginName)) return QStringLiteral("RESOLVED");
    return QStringLiteral("UNKNOWN");
}

InstrumentManagementService* LegacyNavigationPageServiceAdapter::instrumentManagementService() const
{
    return serviceRegistry()
        ? qobject_cast<InstrumentManagementService*>(serviceRegistry()->service(QStringLiteral("InstrumentManagementService")))
        : nullptr;
}

DicomViewerService* LegacyNavigationPageServiceAdapter::dicomViewerService() const
{
    return serviceRegistry()
        ? qobject_cast<DicomViewerService*>(serviceRegistry()->service(QStringLiteral("DicomViewerService")))
        : nullptr;
}

BoneSegmentationService* LegacyNavigationPageServiceAdapter::segmentationService() const
{
    return serviceRegistry()
        ? qobject_cast<BoneSegmentationService*>(serviceRegistry()->service(QStringLiteral("SegmentationService")))
        : nullptr;
}

FourViewDisplayService* LegacyNavigationPageServiceAdapter::fourViewDisplayService() const
{
    return serviceRegistry()
        ? qobject_cast<FourViewDisplayService*>(serviceRegistry()->service(QStringLiteral("FourViewDisplayService")))
        : nullptr;
}

OpticalTrackingService* LegacyNavigationPageServiceAdapter::opticalTrackingService() const
{
    return serviceRegistry()
        ? qobject_cast<OpticalTrackingService*>(serviceRegistry()->service(QStringLiteral("OpticalTrackingService")))
        : nullptr;
}

PointRegistrationService* LegacyNavigationPageServiceAdapter::pointRegistrationService() const
{
    return serviceRegistry()
        ? qobject_cast<PointRegistrationService*>(serviceRegistry()->service(QStringLiteral("PointRegistrationService")))
        : nullptr;
}
