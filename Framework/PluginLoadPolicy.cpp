#include "PluginLoadPolicy.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutexLocker>

#include "Logger.h"

// 单例由SingletonManager管理，不再需要静态成员和手动instance()实现

PluginLoadPolicy::PluginLoadPolicy()
    : QObject(nullptr)
    , m_hasValidConfig(false)
{
}

void PluginLoadPolicy::loadConfig(const QString& configFilePath)
{
    QFile file(configFilePath);
    if (!file.exists()) {
        LOG_WARNING("PluginLoadPolicy", QString("Config file not found: %1").arg(configFilePath));
        QMutexLocker locker(&m_mutex);
        clearPolicies();
        m_configPath.clear();
        m_hasValidConfig = false;
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR("PluginLoadPolicy", QString("Failed to open config file: %1").arg(configFilePath));
        QMutexLocker locker(&m_mutex);
        clearPolicies();
        m_configPath.clear();
        m_hasValidConfig = false;
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR("PluginLoadPolicy", QString("Failed to parse config: %1").arg(parseError.errorString()));
        QMutexLocker locker(&m_mutex);
        clearPolicies();
        m_configPath.clear();
        m_hasValidConfig = false;
        return;
    }

    if (!document.isObject()) {
        LOG_ERROR("PluginLoadPolicy", "Invalid config format: root element must be an object");
        QMutexLocker locker(&m_mutex);
        clearPolicies();
        m_configPath.clear();
        m_hasValidConfig = false;
        return;
    }

    const QJsonObject root = document.object();
    const QJsonValue pluginsValue = root.value(QStringLiteral("plugins"));
    if (!pluginsValue.isArray()) {
        LOG_ERROR("PluginLoadPolicy", "Invalid config format: 'plugins' must be an array");
        QMutexLocker locker(&m_mutex);
        clearPolicies();
        m_configPath.clear();
        m_hasValidConfig = false;
        return;
    }

    QHash<QString, PluginConfig> policies;

    const QJsonArray pluginsArray = pluginsValue.toArray();
    for (const QJsonValue& entryValue : pluginsArray) {
        if (!entryValue.isObject()) {
            continue;
        }

        const QJsonObject entry = entryValue.toObject();
        const QString name = entry.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            continue;
        }

        PluginConfig config;
        config.pluginName = name;

        const QString policyStr = entry.value(QStringLiteral("loadPolicy")).toString();
        config.loadPolicy = parseLoadPolicy(policyStr);

        if (entry.contains(QStringLiteral("dependencies"))) {
            const QJsonValue depsValue = entry.value(QStringLiteral("dependencies"));
            if (depsValue.isArray()) {
                QStringList deps;
                for (const QJsonValue& depValue : depsValue.toArray()) {
                    if (depValue.isString()) {
                        deps.append(depValue.toString());
                    }
                }
                config.dependencies = deps;
            }
        }

        config.isCritical = entry.value(QStringLiteral("isCritical")).toBool(false);

        const QString key = normaliseKey(name);
        policies.insert(key, config);
    }

    {
        QMutexLocker locker(&m_mutex);
        m_policies = policies;
        m_configPath = configFilePath;
        m_hasValidConfig = !m_policies.isEmpty();
    }

    emit policyReloaded();

    LOG_INFO("PluginLoadPolicy", QString("Loaded plugin policy configuration from %1").arg(configFilePath));
}

LoadPolicy PluginLoadPolicy::getLoadPolicy(const QString& pluginName) const
{
    QMutexLocker locker(&m_mutex);
    const PluginConfig config = m_policies.value(normaliseKey(pluginName));
    return config.loadPolicy;
}

QStringList PluginLoadPolicy::getDependencies(const QString& pluginName) const
{
    QMutexLocker locker(&m_mutex);
    const PluginConfig config = m_policies.value(normaliseKey(pluginName));
    return config.dependencies;
}

bool PluginLoadPolicy::isCriticalPlugin(const QString& pluginName) const
{
    QMutexLocker locker(&m_mutex);
    const PluginConfig config = m_policies.value(normaliseKey(pluginName));
    return config.isCritical;
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

QString PluginLoadPolicy::normaliseKey(const QString& key) const
{
    QString normalised = key.trimmed().toLower();
    return normalised.replace(QLatin1Char('.'), QLatin1Char('_'));
}

LoadPolicy PluginLoadPolicy::parseLoadPolicy(const QString& value) const
{
    const QString normalised = value.trimmed().toLower();
    if (normalised == QStringLiteral("immediate")) {
        return LoadPolicy::Immediate;
    }
    if (normalised == QStringLiteral("deferred")) {
        return LoadPolicy::Deferred;
    }
    return LoadPolicy::OnDemand;
}

void PluginLoadPolicy::clearPolicies()
{
    m_policies.clear();
}

QStringList PluginLoadPolicy::getPluginsByPolicy(LoadPolicy policy) const
{
    QMutexLocker locker(&m_mutex);
    QStringList result;
    for (auto it = m_policies.constBegin(); it != m_policies.constEnd(); ++it) {
        if (it.value().loadPolicy == policy) {
            result.append(it.value().pluginName);
        }
    }
    return result;
}

QStringList PluginLoadPolicy::getCriticalPlugins() const
{
    QMutexLocker locker(&m_mutex);
    QStringList result;
    for (auto it = m_policies.constBegin(); it != m_policies.constEnd(); ++it) {
        if (it.value().isCritical) {
            result.append(it.value().pluginName);
        }
    }
    return result;
}

QStringList PluginLoadPolicy::getAllConfiguredPlugins() const
{
    QMutexLocker locker(&m_mutex);
    QStringList result;
    for (auto it = m_policies.constBegin(); it != m_policies.constEnd(); ++it) {
        result.append(it.value().pluginName);
    }
    return result;
}
