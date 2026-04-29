#pragma once

#include "Framework/FrameworkExport.h"

#include <QObject>

class DicomViewerService;
class FourViewDisplayService;
class INavigationPageServicePort;
class InstrumentManagementService;
class OpticalTrackingService;
class PointRegistrationService;
class BoneSegmentationService;

class FRAMEWORK_EXPORT NavigationPageServiceAccess : public QObject
{
    Q_OBJECT

public:
    explicit NavigationPageServiceAccess(INavigationPageServicePort* port, QObject* parent = nullptr);

    InstrumentManagementService* instrumentManagementService() const;
    DicomViewerService* dicomViewerService() const;
    BoneSegmentationService* segmentationService() const;
    FourViewDisplayService* fourViewDisplayService() const;
    OpticalTrackingService* opticalTrackingService() const;
    bool isPointRegistrationFrameworkReady() const;
    QString pointRegistrationPluginState() const;
    PointRegistrationService* pointRegistrationService(bool tryStartPlugin) const;

signals:
    void pointRegistrationPluginAvailable();

private slots:
    void onPluginLoaded(const QString& pluginName);

private:
    INavigationPageServicePort* m_port;
};
