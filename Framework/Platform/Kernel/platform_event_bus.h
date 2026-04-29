#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/platform_runtime_host_ports.h"

#include <QVector>

struct PlatformEventBusMessage
{
    QString topic;
    QVariantMap payload;
};

class FRAMEWORK_EXPORT PlatformEventBus final : public IPlatformEventBusPort
{
public:
    void publish(const QString& topic, const QVariantMap& payload) override;

    QVector<PlatformEventBusMessage> publishedEvents() const;
    void clear();

private:
    QVector<PlatformEventBusMessage> m_events;
};
