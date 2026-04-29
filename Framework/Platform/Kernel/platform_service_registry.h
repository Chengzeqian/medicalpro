#pragma once

#include "Framework/FrameworkExport.h"

#include <QHash>
#include <QObject>
#include <QString>

class FRAMEWORK_EXPORT PlatformServiceRegistry
{
public:
    void registerService(const QString& pluginId, const QString& serviceId, QObject* service);
    QObject* service(const QString& serviceId) const;
    QObject* service(const QString& pluginId, const QString& serviceId) const;
    QString pluginForService(const QString& serviceId) const;
    bool hasService(const QString& serviceId) const;
    void unregisterPlugin(const QString& pluginId);

private:
    struct ServiceEntry
    {
        QString pluginId;
        QObject* instance = nullptr;
    };

    QHash<QString, ServiceEntry> m_services;
};
