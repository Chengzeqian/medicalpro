#ifndef PLUGINLOADPOLICY_H
#define PLUGINLOADPOLICY_H

#include "FrameworkExport.h"
#include "ResourceManagement/SingletonManager.h"

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>

enum class LoadPolicy {
    Immediate,
    Deferred,
    OnDemand
};

struct PluginConfig {
    QString pluginName;
    LoadPolicy loadPolicy{LoadPolicy::OnDemand};
    QStringList dependencies;
    bool isCritical{false};
};

/**
 * @brief 插件加载策略管理器
 *
 * 使用SingletonManager模式管理单例生命周期（需求6.1-6.5）
 */
class FRAMEWORK_EXPORT PluginLoadPolicy : public QObject, public SingletonManager<PluginLoadPolicy>
{
    Q_OBJECT
    friend class SingletonManager<PluginLoadPolicy>;

public:
    /**
     * @brief 获取单例实例指针（兼容性接口）
     * @return 单例实例指针
     */
    static PluginLoadPolicy* instance() { return &SingletonManager<PluginLoadPolicy>::instance(); }

    void loadConfig(const QString& configFilePath);

    LoadPolicy getLoadPolicy(const QString& pluginName) const;
    QStringList getDependencies(const QString& pluginName) const;
    bool isCriticalPlugin(const QString& pluginName) const;

    QString configPath() const;
    bool hasValidConfig() const;

    /**
     * @brief 获取指定策略的插件列表
     * @param policy 加载策略
     * @return 符合该策略的插件名称列表
     */
    QStringList getPluginsByPolicy(LoadPolicy policy) const;

    /**
     * @brief 获取所有关键插件列表
     * @return 关键插件名称列表
     */
    QStringList getCriticalPlugins() const;

    /**
     * @brief 获取所有已配置的插件列表
     * @return 所有插件名称列表
     */
    QStringList getAllConfiguredPlugins() const;

signals:
    void policyReloaded();

private:
    PluginLoadPolicy();

    QString normaliseKey(const QString& key) const;
    LoadPolicy parseLoadPolicy(const QString& value) const;

    void clearPolicies();

    mutable QMutex m_mutex;
    QHash<QString, PluginConfig> m_policies;
    QString m_configPath;
    bool m_hasValidConfig;
};

#endif // PLUGINLOADPOLICY_H
