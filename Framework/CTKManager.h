#ifndef CTKMANAGER_H
#define CTKMANAGER_H

#include <QObject>
#include <QApplication>
#include <QString>
#include <QStringList>
#include <QDebug>

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkPluginFrameworkFactory.h>
#include <ctkPluginFramework.h>
#include <ctkPluginContext.h>
#include <ctkPlugin.h>
#include <ctkPluginException.h>
#include <service/event/ctkEventAdmin.h>
#include <QSharedPointer>
#include <QUrl>
#include <QDirIterator>
#include <QCoreApplication>
#endif

/**
 * @brief CTK插件框架管理器
 * 负责CTK插件框架的初始化、启动、插件加载等功能
 */
class CTKManager : public QObject
{
    Q_OBJECT

public:
    static CTKManager* instance();
    
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
    bool loadPlugin(const QString& pluginPath);
    
    /**
     * @brief 检查CTK框架是否可用
     * @return 是否可用
     */
    bool isCTKAvailable() const;
    
    /**
     * @brief 获取已加载插件列表
     * @return 插件名称列表
     */
    QStringList getLoadedPlugins() const;

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
    void pluginLoaded(const QString& pluginName);
    void pluginLoadFailed(const QString& pluginPath, const QString& error);
    void errorOccurred(const QString& error);

private:
    explicit CTKManager(QObject *parent = nullptr);
    ~CTKManager() override;
    
    static CTKManager* m_instance;
    
#ifdef CTK_PLUGIN_FRAMEWORK
    QSharedPointer<ctkPluginFramework> m_framework;
    ctkPluginContext* m_pluginContext;
    ctkPluginFrameworkFactory m_frameworkFactory;
    ctkEventAdmin* m_eventAdmin;
#endif
    
    bool m_initialized;
    bool m_started;
    QStringList m_loadedPlugins;
    
    void logMessage(const QString& message);
};

#endif // CTKMANAGER_H
