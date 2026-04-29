#pragma once

#include "Framework/FrameworkExport.h"

#include <QString>
#include <QStringList>

class QObject;
class DicomViewerService;
class FourViewDisplayService;
class InstrumentManagementService;
class PointRegistrationService;
class BoneSegmentationService;
class OpticalTrackingService;

class FRAMEWORK_EXPORT ICoreUiRuntimeStatusPort
{
public:
    virtual ~ICoreUiRuntimeStatusPort() = default;

    virtual bool frameworkReady() const = 0;
    virtual QStringList installedPlugins() const = 0;
    virtual QStringList startedPlugins() const = 0;
    virtual QStringList loadedPlugins() const = 0;
    virtual QStringList missingServices(const QStringList& requiredServices) const = 0;
    virtual bool directoryExists(const QString& directoryPath) const = 0;
    virtual bool directoryReadable(const QString& directoryPath) const = 0;
};

class FRAMEWORK_EXPORT INavigationPageServicePort
{
public:
    virtual ~INavigationPageServicePort() = default;

    virtual bool frameworkReady() const = 0;
    virtual QObject* pluginEventSource() const = 0;
    virtual bool isPluginStarted(const QString& pluginName) const = 0;
    virtual bool startPlugin(const QString& pluginName) = 0;
    virtual QString pluginState(const QString& pluginName) const = 0;
    virtual InstrumentManagementService* instrumentManagementService() const = 0;
    virtual DicomViewerService* dicomViewerService() const = 0;
    virtual BoneSegmentationService* segmentationService() const = 0;
    virtual FourViewDisplayService* fourViewDisplayService() const = 0;
    virtual OpticalTrackingService* opticalTrackingService() const = 0;
    virtual PointRegistrationService* pointRegistrationService() const = 0;
};
