#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/platform_runtime_host_ports.h"

#include <QObject>

class FRAMEWORK_EXPORT PlatformRuntimeHostAdapter final
    : public QObject
    , public IPlatformRuntimeHostPort
    , public IPlatformServiceAccessPort
    , public IPlatformEventBusPort
{
    Q_OBJECT

public:
    explicit PlatformRuntimeHostAdapter(QObject* parent = nullptr);

    bool initialize(QApplication* app) override;
    bool start() override;
    bool stop() override;
    bool activatePlugin(const QString& pluginId) override;
    bool isPluginStarted(const QString& pluginId) const override;
    QString pluginState(const QString& pluginId) const override;
    QStringList missingServices(const QStringList& requiredServices) const override;

    registration_core::RegistrationService* registrationService() const override;
    OpticalTrackingService* opticalTrackingService() const override;

    void publish(const QString& topic, const QVariantMap& payload) override;
};
