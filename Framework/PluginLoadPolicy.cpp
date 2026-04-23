#include "PluginLoadPolicy.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutexLocker>

#include "Logger.h"

PluginLoadPolicy::PluginLoadPolicy()
    : QObject(nullptr)
    , m_hasValidConfig(false)
{
}

void PluginLoadPolicy::loadConfig(const QString& configFilePath)
{
    QFile file(configFilePath);
    if (!file.exists()) {
        LOG_WARNING(
            "PluginLoadPolicy",
            QString("Compatibility-only projection file not found: %1").arg(configFilePath));
        QMutexLocker locker(&m_mutex);
        clearState();
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR("PluginLoadPolicy", QString("Failed to open config file: %1").arg(configFilePath));
        QMutexLocker locker(&m_mutex);
        clearState();
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR("PluginLoadPolicy", QString("Failed to parse config: %1").arg(parseError.errorString()));
        QMutexLocker locker(&m_mutex);
        clearState();
        return;
    }

    if (!document.isObject()) {
        LOG_ERROR("PluginLoadPolicy", "Invalid config format: root element must be an object");
        QMutexLocker locker(&m_mutex);
        clearState();
        return;
    }

    const QJsonArray pluginsArray = document.object().value(QStringLiteral("plugins")).toArray();
    QStringList pluginNames;
    pluginNames.reserve(pluginsArray.size());

    for (const QJsonValue& entryValue : pluginsArray) {
        if (!entryValue.isObject()) {
            continue;
        }

        const QString pluginName = entryValue.toObject().value(QStringLiteral("name")).toString().trimmed();
        if (!pluginName.isEmpty()) {
            pluginNames.append(pluginName);
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        m_configuredPluginNames = pluginNames;
        m_configPath = configFilePath;
        m_hasValidConfig = !m_configuredPluginNames.isEmpty();
    }

    emit policyReloaded();

    LOG_INFO(
        "PluginLoadPolicy",
        QString("Loaded compatibility-only plugin policy projection from %1 with %2 entries")
            .arg(configFilePath)
            .arg(pluginNames.size()));
}

QString PluginLoadPolicy::configPath() const
{
    QMutexLocker locker(&m_mutex);
    return m_configPath;
}

bool PluginLoadPolicy::hasValidConfig() const
{
    QMutexLocker locker(&m_mutex);
    return m_hasValidConfig;
}

void PluginLoadPolicy::clearState()
{
    m_configuredPluginNames.clear();
    m_configPath.clear();
    m_hasValidConfig = false;
}
