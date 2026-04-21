#ifndef CTKMANAGER_H
#define CTKMANAGER_H

#include <QObject>
#include <QApplication>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QMap>
#include <QSet>
#include <QHash>
#include <QVector>
#include <QtGlobal>
#include "PluginLoadPolicy.h"
#include "FrameworkExport.h"
#include "ResourceManagement/SingletonManager.h"

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkPluginFrameworkFactory.h>
#include <ctkPluginFramework.h>
#include <ctkPluginContext.h>
#include <ctkPlugin.h>
#include <ctkPluginException.h>
#include <ctkServiceReference.h>
#include <service/event/ctkEventAdmin.h>
#include <QSharedPointer>
#include <QUrl>
#include <QDirIterator>
#include <QCoreApplication>
#endif

/**
 * @brief CTK插件框架管理器
 * 负责CTK插件框架的初始化、启动、插件加载等功能
 *
 * 使用SingletonManager模式管理单例生命周期（需求6.1-6.5）
 */
class FRAMEWORK_EXPORT CTKManager : public QObject, public SingletonManager<CTKManager>
{
    Q_OBJECT
    friend class SingletonManager<CTKManager>;

public:
    /**
     * @brief 获取单例实例指针（兼容性接口）
     * @return 单例实例指针
     */
    static CTKManager* instance() { return &SingletonManager<CTKManager>::instance(); }
    
    /**
     * @brief 初始化CTK插件框架
     * @param app QApplication实例
     * @return 是否初始化成功
     */
    bool initializeFramework(QApplication* app);
    
    /**
     * @brief 启动CTK插件框架
     * @return 是否启动成功
     */
    bool startFramework();
    
    /**
     * @brief 停止CTK插件框架
     */
    void stopFramework();
    
    /**
     * @brief 安全停止所有加载的插件
     * 在完全停止框架之前调用，确保插件的资源被正确释放
     */
    void stopPlugins();
    
    /**
     * @brief 加载指定目录下的所有插件
     * @param pluginDir 插件目录路径
     * @return 成功加载的插件数量
     */
    int loadPluginsFromDirectory(const QString& pluginDir);
    
    /**
     * @brief 加载单个插件
     * @param pluginPath 插件文件路径
     * @return 是否加载成功
     */
    bool loadPlugin(const QString& pluginPath, bool autoStart = true);
    
    /**
     * @brief compatibility-only helper that installs plugins from a directory without starting them.
     * @param pluginDir 插件目录
     * @return 成功安装的插件数量
     * @note Product startup mainline must use platform_runtime.json + descriptors instead.
     */
    int installPluginsFromDirectory(const QString& pluginDir);

    /**
     * @brief 安装单个插件
     * @param pluginPath 插件路径
     * @param autoStart 是否在安装完成后立即启动
     * @return 是否安装成功
     */
    bool installPlugin(const QString& pluginPath, bool autoStart = false, QString* outPluginName = nullptr);

    /**
     * @brief 启动指定插件
     * @param pluginName 插件符号名称
     * @return 是否启动成功
     */
    bool startPlugin(const QString& pluginName);

    /**
     * @brief 批量启动插件
     * @param pluginNames 插件名称列表
     * @param stopOnFailure 若为 true，则任一插件启动失败时终止后续启动
     * @return 是否全部启动成功
     */
    bool startPlugins(const QStringList& pluginNames, bool stopOnFailure = false);

    /**
     * @brief 启动所有延迟加载策略的插件
     * @param stopOnFailure 若为 true，一旦有插件启动失败则停止后续启动
     * @return 是否全部启动成功
     */
    bool startDeferredPlugins(bool stopOnFailure = false);
    
    /**
     * @brief compatibility-only helper that loads legacy plugin policy metadata.
     * @param configPath 配置文件路径
     * @note Product startup mainline must not use this API as a truth source.
     */
    void loadPluginPolicy(const QString& configPath);
    
    /**
     * @brief 检查CTK框架是否可用
     * @return 是否可用
     */
    bool isCTKAvailable() const;

    /**
     * @brief 设置安全模式
     * @param enabled 是否启用安全模式
     * @note 安全模式下跳过可选插件的加载
     */
    void setSafeMode(bool enabled);

    /**
     * @brief 检查是否处于安全模式
     * @return 是否处于安全模式
     */
    bool isSafeMode() const;
    
    /**
     * @brief 获取已加载插件列表
     * @return 插件名称列表
     */
    QStringList getLoadedPlugins() const;

    /**
     * @brief 获取已安装但未启动的插件列表
     */
    QStringList getInstalledPlugins() const;

    /**
     * @brief 获取延迟启动策略的插件列表
     */
    QStringList getDeferredPlugins() const;

    /**
     * @brief 获取按需启动策略的插件列表
     */
    QStringList getOnDemandPlugins() const;

    /**
     * @brief 获取已启动的插件列表（去重）
     */
    QStringList getStartedPlugins() const;

    /**
     * @brief 判断插件是否已启动
     */
    bool isPluginStarted(const QString& pluginName) const;

    /**
     * @brief 获取指定插件的状态字符串
     */
    QString getPluginState(const QString& pluginName) const;
    
    /**
     * @brief 设置插件加载顺序
     * @param order 插件名称列表，按加载顺序排列
     */
    void setPluginLoadOrder(const QStringList& order);
    
    /**
     * @brief 获取推荐的插件加载顺序
     * @return 推荐的插件加载顺序列表
     */
    QStringList getRecommendedLoadOrder() const;
    
    /**
     * @brief 验证必需的服务是否已注册
     * @param serviceNames 必需的服务名称列表
     * @return 是否所有必需服务都已注册
     */
    bool verifyRequiredServices(const QStringList& serviceNames);
    
    /**
     * @brief 获取框架诊断信息
     * @return 包含框架状态、插件状态等信息的字符串
     */
    QString getFrameworkDiagnostics() const;
    
    /**
     * @brief 获取缺失的服务列表
     * @param required 必需的服务名称列表
     * @return 缺失的服务名称列表
     */
    QStringList getMissingServices(const QStringList& required) const;
    
    /**
     * @brief 获取所有插件的状态
     * @return 插件名称到状态字符串的映射
     */
    QMap<QString, QString> getPluginStatus() const;
    
    /**
     * @brief 验证插件加载后的服务依赖
     * @return 包含验证结果的字符串报告
     */
    QString verifyPluginServices();

#ifdef CTK_PLUGIN_FRAMEWORK
    /**
     * @brief 获取插件上下文
     * @return 插件上下文指针
     */
    ctkPluginContext* getPluginContext() const;

    /**
     * @brief 获取插件框架
     * @return 插件框架共享指针
     */
    QSharedPointer<ctkPluginFramework> getFramework() const;

    /**
     * @brief 获取 CTK 服务（便捷方法）
     * @tparam T 服务接口类型（需要有 Q_DECLARE_INTERFACE 声明）
     * @return 服务指针，未找到返回 nullptr
     *
     * 使用示例：
     * @code
     * auto* segService = CTKManager::instance()->getService<SegmentationService>();
     * @endcode
     */
    template<typename T>
    T* getService() {
        if (!m_pluginContext) {
            qWarning() << "[CTKManager] Plugin context not initialized";
            return nullptr;
        }
        ctkServiceReference ref = m_pluginContext->getServiceReference<T>();
        if (!ref) {
            return nullptr;
        }
        return m_pluginContext->getService<T>(ref);
    }

    /**
     * @brief 启动EventAdmin服务
     * @return 是否启动成功
     */
    bool startEventAdmin();

    /**
     * @brief 获取EventAdmin服务
     * @return EventAdmin服务指针
     */
    ctkEventAdmin* getEventAdmin() const;
#endif

signals:
    void frameworkInitialized();
    void frameworkStarted();
    void frameworkStopped();
    void pluginInstallStartedDetailed(const QString& pluginName, const QString& pluginPath);
    void pluginInstalled(const QString& pluginName, const QString& pluginPath);
    void pluginInstallFailedDetailed(const QString& pluginName, const QString& pluginPath, const QString& reason);
    void pluginStartRequestedDetailed(const QString& pluginName);
    void pluginStartedDetailed(const QString& pluginName);
    void pluginStartFailedDetailed(const QString& pluginName, const QString& reason);
    void pluginLoaded(const QString& pluginName);
    void pluginLoadFailed(const QString& pluginPath, const QString& error);
    void errorOccurred(const QString& error);

private:
    CTKManager();
    ~CTKManager() override;
    
#ifdef CTK_PLUGIN_FRAMEWORK
    QSharedPointer<ctkPluginFramework> m_framework;
    ctkPluginContext* m_pluginContext;
    ctkPluginFrameworkFactory* m_frameworkFactory;
    ctkEventAdmin* m_eventAdmin;
    QMap<QString, QSharedPointer<ctkPlugin>> m_installedPluginHandles;
    QSet<QString> m_installedPluginNames;
    QSet<QString> m_startedPluginNames;
    QSet<QString> m_deferredPlugins;
    QSet<QString> m_onDemandPlugins;
    QHash<QString, QString> m_pluginSourceMap;
    bool startPluginInternal(const QString& pluginName, QSet<QString>& visiting);
    bool activatePlugin(const QString& pluginName);
    bool stopPluginInternal(const QString& pluginName);
    QStringList manifestDependenciesForPlugin(const QString& pluginName) const;
    QString locateManifestForPlugin(const QString& pluginName) const;
    QStringList parseManifestDependencies(const QString& manifestPath) const;
    LoadPolicy policyForPlugin(const QString& pluginName);
    bool applyPolicyForPlugin(const QString& pluginName, bool allowStart, bool forceStart = false);
#endif
    
    bool m_initialized;
    bool m_started;
    bool m_safeMode;
    QStringList m_loadedPlugins;
    QStringList m_pluginLoadOrder;
    QString m_pluginPolicyPath;
    
    void logMessage(const QString& message);
    QString getPluginStateString(int state) const;
};

#endif // CTKMANAGER_H
