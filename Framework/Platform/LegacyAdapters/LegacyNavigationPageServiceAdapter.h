#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformUiPorts.h"

class FRAMEWORK_EXPORT LegacyNavigationPageServiceAdapter : public INavigationPageServicePort
{
public:
    bool frameworkReady() const override;
    QObject* pluginEventSource() const override;
    bool isPluginStarted(const QString& pluginName) const override;
    bool startPlugin(const QString& pluginName) override;
    QString pluginState(const QString& pluginName) const override;
    InstrumentManagementService* instrumentManagementService() const override;
    DicomViewerService* dicomViewerService() const override;
    BoneSegmentationService* segmentationService() const override;
    FourViewDisplayService* fourViewDisplayService() const override;
    PointRegistrationService* pointRegistrationService() const override;
};
