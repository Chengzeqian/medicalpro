#pragma once

#include "Framework/FrameworkExport.h"

#include <QApplication>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class RegistrationService;
class OpticalTrackingService;

class FRAMEWORK_EXPORT IPlatformRuntimeHostPort
{
public:
    virtual ~IPlatformRuntimeHostPort() = default;

    virtual bool initialize(QApplication* app) = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool activatePlugin(const QString& pluginId) = 0;
    virtual bool isPluginStarted(const QString& pluginId) const = 0;
    virtual QString pluginState(const QString& pluginId) const = 0;
    virtual QStringList missingServices(const QStringList& requiredServices) const = 0;
};

class FRAMEWORK_EXPORT IPlatformServiceAccessPort
{
public:
    virtual ~IPlatformServiceAccessPort() = default;

    virtual RegistrationService* registrationService() const = 0;
    virtual OpticalTrackingService* opticalTrackingService() const = 0;
};

class FRAMEWORK_EXPORT IPlatformEventBusPort
{
public:
    virtual ~IPlatformEventBusPort() = default;

    virtual void publish(const QString& topic, const QVariantMap& payload) = 0;
};
