#include "CTKManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QThread>

CTKManager* CTKManager::m_instance = nullptr;

CTKManager* CTKManager::instance()
{
    if (!m_instance) {
        m_instance = new CTKManager();
    }
    return m_instance;
}

CTKManager::CTKManager(QObject *parent)
    : QObject(parent)
#ifdef CTK_PLUGIN_FRAMEWORK
    , m_pluginContext(nullptr)
    , m_eventAdmin(nullptr)
#endif
    , m_initialized(false)
    , m_started(false)
{
    logMessage("CTK Manager created");
}

CTKManager::~CTKManager()
{
    stopFramework();
    logMessage("CTK Manager destroyed");
}

bool CTKManager::initializeFramework(QApplication* app)
{
    if (m_initialized) {
        logMessage("CTK Framework already initialized");
        return true;
    }
    
#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        // 设置应用程序名称（Linux下必需）
        if (app && app->applicationName().isEmpty()) {
            app->setApplicationName("MedicalPro");
        }
        
        // 创建插件框架
        m_framework = m_frameworkFactory.getFramework();
        if (!m_framework) {
            logMessage("Failed to create CTK framework");
            return false;
        }
        
        // 初始化框架
        m_framework->init();
        logMessage("CTK Framework initialized successfully");
        
        m_initialized = true;
        emit frameworkInitialized();
        return true;
        
    } catch (const ctkPluginException& e) {
        QString error = QString("CTK Plugin Exception during initialization: %1").arg(e.what());
        logMessage(error);
        emit errorOccurred(error);
        return false;
    } catch (const std::exception& e) {
        QString error = QString("Standard Exception during initialization: %1").arg(e.what());
        logMessage(error);
        emit errorOccurred(error);
        return false;
    } catch (...) {
        QString error = "Unknown exception occurred during CTK initialization";
        logMessage(error);
        emit errorOccurred(error);
        return false;
    }
#else
    logMessage("CTK Plugin Framework not available - compiled without CTK support");
    return false;
#endif
}

bool CTKManager::startFramework()
{
    if (!m_initialized) {
        logMessage("CTK Framework not initialized - cannot start");
        return false;
    }
    
    if (m_started) {
        logMessage("CTK Framework already started");
        return true;
    }
    
#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        // 启动框架
        m_framework->start();
        
        // 获取插件上下文
        m_pluginContext = m_framework->getPluginContext();
        if (!m_pluginContext) {
            logMessage("Failed to get plugin context");
            return false;
        }

        // 启动EventAdmin服务
        logMessage("Attempting to start EventAdmin service...");
        if (!startEventAdmin()) {
            logMessage("Warning: EventAdmin service failed to start");
            // 不返回false，因为EventAdmin不是必需的
        } else {
            logMessage("EventAdmin service started successfully");
        }

        logMessage("CTK Framework started successfully");
        m_started = true;
        emit frameworkStarted();
        return true;
        
    } catch (const ctkPluginException& e) {
        QString error = QString("CTK Plugin Exception during start: %1").arg(e.what());
        logMessage(error);
        emit errorOccurred(error);
        return false;
    } catch (const std::exception& e) {
        QString error = QString("Standard Exception during start: %1").arg(e.what());
        logMessage(error);
        emit errorOccurred(error);
        return false;
    } catch (...) {
        QString error = "Unknown exception occurred during CTK start";
        logMessage(error);
        emit errorOccurred(error);
        return false;
    }
#else
    logMessage("CTK Plugin Framework not available");
    return false;
#endif
}

void CTKManager::stopFramework()
{
    if (!m_started) {
        return;
    }
    
#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        if (m_framework) {
            m_framework->stop();
            m_framework->waitForStop(5000); // 等待最多5秒
        }
        m_pluginContext = nullptr;
        logMessage("CTK Framework stopped");
        
    } catch (const ctkPluginException& e) {
        QString error = QString("CTK Plugin Exception during stop: %1").arg(e.what());
        logMessage(error);
        emit errorOccurred(error);
    } catch (const std::exception& e) {
        QString error = QString("Standard Exception during stop: %1").arg(e.what());
        logMessage(error);
        emit errorOccurred(error);
    } catch (...) {
        logMessage("Unknown exception occurred during CTK stop");
    }
#endif
    
    m_started = false;
    m_loadedPlugins.clear();
    emit frameworkStopped();
}

int CTKManager::loadPluginsFromDirectory(const QString& pluginDir)
{
    if (!m_started) {
        logMessage("CTK Framework not started - cannot load plugins");
        return 0;
    }
    
    logMessage(QString("Loading plugins from directory: %1").arg(pluginDir));
    
    int loadedCount = 0;
    
#ifdef CTK_PLUGIN_FRAMEWORK
    // 遍历目录下的所有插件文件
    QDirIterator itPlugin(pluginDir, QStringList() << "*.dll" << "*.so" << "*.dylib", QDir::Files);
    while (itPlugin.hasNext()) {
        QString pluginPath = itPlugin.next();
        if (loadPlugin(pluginPath)) {
            loadedCount++;
        }
    }
#endif
    
    logMessage(QString("Loaded %1 plugins from directory").arg(loadedCount));
    return loadedCount;
}

bool CTKManager::loadPlugin(const QString& pluginPath)
{
    if (!m_started) {
        logMessage("CTK Framework not started - cannot load plugin");
        return false;
    }
    
#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        logMessage(QString("Loading plugin: %1").arg(pluginPath));
        
        // 安装插件
        QSharedPointer<ctkPlugin> plugin = m_pluginContext->installPlugin(QUrl::fromLocalFile(pluginPath));
        if (!plugin) {
            QString error = QString("Failed to install plugin: %1").arg(pluginPath);
            logMessage(error);
            emit pluginLoadFailed(pluginPath, error);
            return false;
        }
        
        // 启动插件
        plugin->start(ctkPlugin::START_TRANSIENT);
        
        QString pluginName = plugin->getSymbolicName();
        m_loadedPlugins.append(pluginName);
        
        logMessage(QString("Plugin loaded successfully: %1").arg(pluginName));
        emit pluginLoaded(pluginName);
        return true;
        
    } catch (const ctkPluginException& e) {
        QString error = QString("CTK Plugin Exception loading %1: %2").arg(pluginPath, e.what());
        logMessage(error);
        emit pluginLoadFailed(pluginPath, error);
        return false;
    } catch (const std::exception& e) {
        QString error = QString("Standard Exception loading %1: %2").arg(pluginPath, e.what());
        logMessage(error);
        emit pluginLoadFailed(pluginPath, error);
        return false;
    } catch (...) {
        QString error = QString("Unknown exception loading plugin: %1").arg(pluginPath);
        logMessage(error);
        emit pluginLoadFailed(pluginPath, error);
        return false;
    }
#else
    QString error = "CTK Plugin Framework not available";
    emit pluginLoadFailed(pluginPath, error);
    return false;
#endif
}

bool CTKManager::isCTKAvailable() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    return m_initialized && m_started;
#else
    return false;
#endif
}

QStringList CTKManager::getLoadedPlugins() const
{
    return m_loadedPlugins;
}

#ifdef CTK_PLUGIN_FRAMEWORK
ctkPluginContext* CTKManager::getPluginContext() const
{
    return m_pluginContext;
}

QSharedPointer<ctkPluginFramework> CTKManager::getFramework() const
{
    return m_framework;
}

bool CTKManager::startEventAdmin()
{
    if (!m_pluginContext) {
        logMessage("Cannot start EventAdmin: Plugin context not available");
        return false;
    }

    logMessage("Starting EventAdmin service initialization...");

    try {
        // 首先尝试加载EventAdmin插件
        QString appDir = QCoreApplication::applicationDirPath();
        QString currentDir = QDir::currentPath();

        logMessage(QString("Application directory: %1").arg(appDir));
        logMessage(QString("Current directory: %1").arg(currentDir));

        QStringList possiblePaths;

        // 添加可能的路径（优先在输出目录的plugins文件夹中查找）
        possiblePaths << appDir + "/plugins/liborg_commontk_eventadmin.dll"  // 输出目录的plugins文件夹（最优先）
                     << currentDir + "/plugins/liborg_commontk_eventadmin.dll"  // 当前目录的plugins文件夹
                     << appDir + "/../ThirdParty/CTK/CTK_install/lib/ctk-0.1/plugins/liborg_commontk_eventadmin.dll"
                     << "ThirdParty/CTK/CTK_install/lib/ctk-0.1/plugins/liborg_commontk_eventadmin.dll"
                     << appDir + "/../../ThirdParty/CTK/CTK_install/lib/ctk-0.1/plugins/liborg_commontk_eventadmin.dll"
                     << currentDir + "/ThirdParty/CTK/CTK_install/lib/ctk-0.1/plugins/liborg_commontk_eventadmin.dll";

        QString eventAdminPluginPath;
        bool found = false;

        logMessage("Searching for EventAdmin plugin...");
        for (const QString& path : possiblePaths) {
            logMessage(QString("  Checking: %1").arg(path));
            if (QFile::exists(path)) {
                eventAdminPluginPath = path;
                found = true;
                logMessage(QString("Found EventAdmin plugin at: %1").arg(path));
                break;
            } else {
                logMessage(QString("  Not found at: %1").arg(path));
            }
        }

        if (!found) {
            logMessage("EventAdmin plugin not found at any expected locations");
            return false;
        }

        // 安装并启动EventAdmin插件
        logMessage("Installing EventAdmin plugin...");
        QUrl pluginUrl = QUrl::fromLocalFile(QFileInfo(eventAdminPluginPath).absoluteFilePath());
        logMessage(QString("Plugin URL: %1").arg(pluginUrl.toString()));

        QSharedPointer<ctkPlugin> eventAdminPlugin = m_pluginContext->installPlugin(pluginUrl);

        if (eventAdminPlugin) {
            logMessage("EventAdmin plugin installed successfully, starting...");
            eventAdminPlugin->start();
            logMessage("EventAdmin plugin started successfully");

            // 等待一小段时间让服务注册
            QThread::msleep(100);

            // 现在尝试获取EventAdmin服务
            logMessage("Looking for EventAdmin service...");
            ctkServiceReference eventAdminRef = m_pluginContext->getServiceReference<ctkEventAdmin>();
            if (eventAdminRef) {
                logMessage("EventAdmin service reference found");
                m_eventAdmin = m_pluginContext->getService<ctkEventAdmin>(eventAdminRef);
                if (m_eventAdmin) {
                    logMessage("EventAdmin service connected successfully");
                    return true;
                } else {
                    logMessage("Failed to get EventAdmin service instance");
                }
            } else {
                logMessage("EventAdmin service reference not found after plugin start");
            }
        } else {
            logMessage("Failed to install EventAdmin plugin");
        }

    } catch (const ctkPluginException& e) {
        logMessage(QString("CTK Plugin Exception while starting EventAdmin: %1").arg(e.what()));
    } catch (const std::exception& e) {
        logMessage(QString("Exception while starting EventAdmin: %1").arg(e.what()));
    } catch (...) {
        logMessage("Unknown exception while starting EventAdmin");
    }

    return false;
}

ctkEventAdmin* CTKManager::getEventAdmin() const
{
    return m_eventAdmin;
}
#endif

void CTKManager::logMessage(const QString& message)
{
    qDebug() << "[CTKManager]" << message;
}
