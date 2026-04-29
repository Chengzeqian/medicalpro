#include "Framework/Platform/Kernel/platform_event_bus.h"

void PlatformEventBus::publish(const QString& topic, const QVariantMap& payload)
{
    PlatformEventBusMessage message;
    message.topic = topic;
    message.payload = payload;
    m_events.append(message);
}

QVector<PlatformEventBusMessage> PlatformEventBus::publishedEvents() const
{
    return m_events;
}

void PlatformEventBus::clear()
{
    m_events.clear();
}
