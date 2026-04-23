#ifndef PLUGINLOADPOLICY_H
#define PLUGINLOADPOLICY_H

#include "FrameworkExport.h"
#include "ResourceManagement/SingletonManager.h"

#include <QObject>
#include <QMutex>
#include <QString>
#include <QStringList>

/**
 * @brief compatibility-only legacy plugin policy carrier.
 *
 * Product startup truth comes from platform_runtime.json + plugins/descriptors/*.json.
 * This type only tracks whether a compatibility projection was loaded for legacy paths.
 */
class FRAMEWORK_EXPORT PluginLoadPolicy : public QObject, public SingletonManager<PluginLoadPolicy>
{
    Q_OBJECT
    friend class SingletonManager<PluginLoadPolicy>;

public:
    static PluginLoadPolicy* instance() { return &SingletonManager<PluginLoadPolicy>::instance(); }

    void loadConfig(const QString& configFilePath);
    QString configPath() const;
    bool hasValidConfig() const;

signals:
    void policyReloaded();

private:
    PluginLoadPolicy();

    void clearState();

    mutable QMutex m_mutex;
    QStringList m_configuredPluginNames;
    QString m_configPath;
    bool m_hasValidConfig;
};

#endif // PLUGINLOADPOLICY_H
