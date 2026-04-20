#include "Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.h"

#include "Framework/CTKManager.h"

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Plugins/BoneSegmentation/SegmentationService.h"
#include "Plugins/DicomViewer/DicomViewerService.h"
#include "Plugins/FourViewDisplay/FourViewDisplayService.h"
#include "Plugins/InstrumentManagement/InstrumentManagementService.h"
#include "Plugins/PointRegistration/PointRegistrationService.h"
#endif

bool LegacyNavigationPageServiceAdapter::frameworkReady() const
{
    auto* ctkManager = CTKManager::instance();
    return ctkManager && ctkManager->isCTKAvailable();
}

QObject* LegacyNavigationPageServiceAdapter::pluginEventSource() const
{
    return CTKManager::instance();
}

bool LegacyNavigationPageServiceAdapter::isPluginStarted(const QString& pluginName) const
{
    auto* ctkManager = CTKManager::instance();
    return ctkManager && ctkManager->isPluginStarted(pluginName);
}

bool LegacyNavigationPageServiceAdapter::startPlugin(const QString& pluginName)
{
    auto* ctkManager = CTKManager::instance();
    return ctkManager && ctkManager->startPlugin(pluginName);
}

QString LegacyNavigationPageServiceAdapter::pluginState(const QString& pluginName) const
{
    auto* ctkManager = CTKManager::instance();
    return ctkManager ? ctkManager->getPluginState(pluginName) : QString {};
}

InstrumentManagementService* LegacyNavigationPageServiceAdapter::instrumentManagementService() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* ctkManager = CTKManager::instance();
    return ctkManager ? ctkManager->getService<InstrumentManagementService>() : nullptr;
#else
    return nullptr;
#endif
}

DicomViewerService* LegacyNavigationPageServiceAdapter::dicomViewerService() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* ctkManager = CTKManager::instance();
    return ctkManager ? ctkManager->getService<DicomViewerService>() : nullptr;
#else
    return nullptr;
#endif
}

SegmentationService* LegacyNavigationPageServiceAdapter::segmentationService() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* ctkManager = CTKManager::instance();
    return ctkManager ? ctkManager->getService<SegmentationService>() : nullptr;
#else
    return nullptr;
#endif
}

FourViewDisplayService* LegacyNavigationPageServiceAdapter::fourViewDisplayService() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* ctkManager = CTKManager::instance();
    return ctkManager ? ctkManager->getService<FourViewDisplayService>() : nullptr;
#else
    return nullptr;
#endif
}

PointRegistrationService* LegacyNavigationPageServiceAdapter::pointRegistrationService() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* ctkManager = CTKManager::instance();
    return ctkManager ? ctkManager->getService<PointRegistrationService>() : nullptr;
#else
    return nullptr;
#endif
}
