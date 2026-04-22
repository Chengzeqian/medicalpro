#include "CTKManager.h"
#include "Logger.h"
#include "PluginLoadPolicy.h"
#include "StartupOrchestrator.h"
#include "ErrorHandler.h"
#include "Framework/Platform/Kernel/PlatformCtkPolicyBridge.h"

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkException.h>
#endif

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QThread>
#include <QTextStream>
#include <QUrl>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <algorithm>
#include <QScopedValueRollback>

namespace
{
QString resolutionStatusToString(PlatformCtkPolicyResolutionStatus status)
{
    switch (status) {
    case PlatformCtkPolicyResolutionStatus::ResolvedFromDescriptor:
        return QStringLiteral("resolved_from_descriptor");
    case PlatformCtkPolicyResolutionStatus::DescriptorMissingFallback:
        return QStringLiteral("descriptor_missing_fallback");
    }

    return QStringLiteral("unknown");
}

QString loadBucketToString(PlatformCtkLoadBucket bucket)
{
    switch (bucket) {
    case PlatformCtkLoadBucket::Immediate:
        return QStringLiteral("immediate");
    case PlatformCtkLoadBucket::Deferred:
        return QStringLiteral("deferred");
    case PlatformCtkLoadBucket::OnDemand:
        return QStringLiteral("on_demand");
    }

    return QStringLiteral("unknown");
}
}

// 鍗曚緥鐢盨ingletonManager绠＄悊锛屼笉鍐嶉渶瑕侀潤鎬佹垚鍛樺拰鎵嬪姩instance()瀹炵幇

CTKManager::CTKManager()
    : QObject(nullptr)
#ifdef CTK_PLUGIN_FRAMEWORK
    , m_pluginContext(nullptr)
    , m_frameworkFactory(nullptr)
    , m_eventAdmin(nullptr)
#endif
    , m_initialized(false)
    , m_started(false)
    , m_safeMode(false)
{
    LOG_INFO("CTKManager", "CTK Manager created");
}

CTKManager::~CTKManager()
{
    stopFramework();
#ifdef CTK_PLUGIN_FRAMEWORK
    delete m_frameworkFactory;
    m_frameworkFactory = nullptr;
#endif
    LOG_INFO("CTKManager", "CTK Manager destroyed");
}

bool CTKManager::initializeFramework(QApplication* app)
{
    if (m_initialized) {
        LOG_INFO("CTKManager", "CTK Framework already initialized");
        return true;
    }

#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        // 璁剧疆搴旂敤绋嬪簭鍚嶇О锛圠inux涓嬪繀闇€锛?
        if (app && app->applicationName().isEmpty()) {
            app->setApplicationName("MedicalPro");
        }

        // 閰嶇疆妗嗘灦灞炴€?
        ctkProperties frameworkProps;

        // 璁剧疆瀛樺偍璺緞涓哄簲鐢ㄧ▼搴忕洰褰曚笅鐨?configuration 鏂囦欢澶?
        QString storagePath = QCoreApplication::applicationDirPath() + "/configuration";
        frameworkProps.insert(ctkPluginConstants::FRAMEWORK_STORAGE, storagePath);
        LOG_INFO_F("CTKManager", "CTK storage path: %1", storagePath);

        // 妫€鏌?plugins.db 鏄惁鎹熷潖锛屽鏋滄崯鍧忓垯娓呯悊瀛樺偍
        QString dbPath = storagePath + "/plugins.db";
        bool needClean = false;
        if (QFile::exists(dbPath)) {
            // 灏濊瘯鎵撳紑鏁版嵁搴撻獙璇佸畬鏁存€?
            QSqlDatabase testDb = QSqlDatabase::addDatabase("QSQLITE", "ctk_integrity_check");
            testDb.setDatabaseName(dbPath);
            if (testDb.open()) {
                QSqlQuery query(testDb);
                if (!query.exec("PRAGMA integrity_check;") || !query.next()
                    || query.value(0).toString() != "ok") {
                    LOG_WARNING("CTKManager", "plugins.db integrity check failed, will clean storage");
                    needClean = true;
                } else {
                    // 妫€鏌ヨ〃缁撴瀯鏄惁瀹屾暣
                    QStringList tables = testDb.tables();
                    if (!tables.contains("Plugins") || !tables.contains("PluginResources")) {
                        LOG_WARNING("CTKManager", "plugins.db missing required tables, will clean storage");
                        needClean = true;
                    }
                }
                testDb.close();
            } else {
                LOG_WARNING("CTKManager", "Cannot open plugins.db for integrity check, will clean storage");
                needClean = true;
            }
            QSqlDatabase::removeDatabase("ctk_integrity_check");
        }

        if (needClean) {
            // 鍒犻櫎鎹熷潖鐨勬暟鎹簱鏂囦欢锛岃 CTK 閲嶆柊鍒涘缓
            LOG_INFO("CTKManager", "Removing corrupted plugins.db...");
            QFile::remove(dbPath);
            // 鍚屾椂鍒犻櫎鍙兘瀛樺湪鐨?journal 鏂囦欢
            QFile::remove(dbPath + "-journal");
            QFile::remove(dbPath + "-wal");
            QFile::remove(dbPath + "-shm");
        }

        // 寤惰繜鍒涘缓 FrameworkFactory锛堟鏃?QApplication 宸插氨缁紝璺緞鍙敤锛?
        delete m_frameworkFactory;
        m_frameworkFactory = new ctkPluginFrameworkFactory(frameworkProps);

        // 鍒涘缓鎻掍欢妗嗘灦
        m_framework = m_frameworkFactory->getFramework();
        if (!m_framework) {
            LOG_ERROR("CTKManager", "Failed to create CTK framework");
            return false;
        }

        // 鍒濆鍖栨鏋?
        m_framework->init();
        LOG_INFO("CTKManager", "CTK Framework initialized successfully");

        m_initialized = true;
        emit frameworkInitialized();
        return true;

    } catch (const ctkPluginException& e) {
        QString error = QString("CTK Plugin Exception during initialization: %1").arg(e.what());
        LOG_ERROR("CTKManager", error);
        emit errorOccurred(error);
        return false;
    } catch (const std::exception& e) {
        QString error = QString("Standard Exception during initialization: %1").arg(e.what());
        LOG_ERROR("CTKManager", error);
        emit errorOccurred(error);
        return false;
    } catch (...) {
        QString error = "Unknown exception occurred during CTK initialization";
        LOG_ERROR("CTKManager", error);
        emit errorOccurred(error);
        return false;
    }
#else
    LOG_WARNING("CTKManager", "CTK Plugin Framework not available - compiled without CTK support");
    return false;
#endif
}

bool CTKManager::startFramework()
{
    if (!m_initialized) {
        LOG_ERROR("CTKManager", "CTK Framework not initialized - cannot start");
        return false;
    }
    
    if (m_started) {
        LOG_INFO("CTKManager", "CTK Framework already started");
        return true;
    }
    
#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        // 鍚姩妗嗘灦
        m_framework->start();
        
        // 鑾峰彇鎻掍欢涓婁笅鏂?
        m_pluginContext = m_framework->getPluginContext();
        if (!m_pluginContext) {
            LOG_ERROR("CTKManager", "Failed to get plugin context");
            return false;
        }

        // 鍚姩EventAdmin鏈嶅姟
        LOG_DEBUG("CTKManager", "Attempting to start EventAdmin service...");
        if (!startEventAdmin()) {
            LOG_WARNING("CTKManager", "Warning: EventAdmin service failed to start");
            // 涓嶈繑鍥瀎alse锛屽洜涓篍ventAdmin涓嶆槸蹇呴渶鐨?
        } else {
            LOG_INFO("CTKManager", "EventAdmin service started successfully");
        }

        LOG_INFO("CTKManager", "CTK Framework started successfully");
        m_started = true;
        emit frameworkStarted();
        return true;
        
    } catch (const ctkPluginException& e) {
        QString error = QString("CTK Plugin Exception during start: %1").arg(e.what());
        LOG_ERROR("CTKManager", error);
        emit errorOccurred(error);
        return false;
    } catch (const std::exception& e) {
        QString error = QString("Standard Exception during start: %1").arg(e.what());
        LOG_ERROR("CTKManager", error);
        emit errorOccurred(error);
        return false;
    } catch (...) {
        QString error = "Unknown exception occurred during CTK start";
        LOG_ERROR("CTKManager", error);
        emit errorOccurred(error);
        return false;
    }
#else
    LOG_WARNING("CTKManager", "CTK Plugin Framework not available");
    return false;
#endif
}

void CTKManager::stopPlugins()
{
    if (!m_started || !m_initialized) {
        LOG_WARNING("CTKManager", "Cannot stop plugins: Framework not started");
        return;
    }
    
#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        if (!m_pluginContext) {
            LOG_WARNING("CTKManager", "Cannot stop plugins: Plugin context not available");
            return;
        }
        
        LOG_INFO("CTKManager", "Safely stopping all plugins before framework shutdown...");
        
        // 鑾峰彇鎵€鏈夋椿鍔ㄦ彃浠?
        QList<QSharedPointer<ctkPlugin>> plugins = m_pluginContext->getPlugins();
        int stoppedCount = 0;
        
        // 棣栧厛鍋滄鎵€鏈夐潪绯荤粺鎻掍欢锛堝寘鎷敤鎴锋彃浠讹級
        for (const auto& plugin : plugins) {
            if (!plugin) continue;
            
            QString name = plugin->getSymbolicName();
            int state = plugin->getState();
            
            // 璺宠繃绯荤粺鎻掍欢
            if (name == "system.plugin" || name.startsWith("org.commontk")) {
                continue;
            }
            
            // 鍙仠姝㈠浜嶢CTIVE鐘舵€佺殑鎻掍欢
            if (state == ctkPlugin::ACTIVE) {
                try {
                    LOG_INFO_F("CTKManager", "Stopping plugin: %1", name);
                    plugin->stop(ctkPlugin::STOP_TRANSIENT);
                    stoppedCount++;
                    m_startedPluginNames.remove(name);
                    
                    // 澶勭悊浜嬩欢锛岀‘淇濇彃浠舵湁鏃堕棿杩涜娓呯悊
                    QCoreApplication::processEvents();
                    
                } catch (const ctkPluginException& e) {
                    LOG_WARNING_F("CTKManager", "Failed to stop plugin %1: %2", name, e.what());
                } catch (...) {
                    LOG_WARNING_F("CTKManager", "Unknown error stopping plugin %1", name);
                }
            }
        }
        
        // 鐒跺悗鍋滄绯荤粺鎻掍欢锛堝EventAdmin锛?
        for (const auto& plugin : plugins) {
            if (!plugin) continue;
            
            QString name = plugin->getSymbolicName();
            int state = plugin->getState();
            
            // 鍙仠姝㈢郴缁熸彃浠?
            if (name == "system.plugin" || name.startsWith("org.commontk")) {
                if (state == ctkPlugin::ACTIVE) {
                    try {
                        LOG_INFO_F("CTKManager", "Stopping system plugin: %1", name);
                        plugin->stop(ctkPlugin::STOP_TRANSIENT);
                        stoppedCount++;
                        m_startedPluginNames.remove(name);
                        
                        // 澶勭悊浜嬩欢
                        QCoreApplication::processEvents();
                        
                    } catch (const ctkPluginException& e) {
                        LOG_WARNING_F("CTKManager", "Failed to stop system plugin %1: %2", name, e.what());
                    } catch (...) {
                        LOG_WARNING_F("CTKManager", "Unknown error stopping system plugin %1", name);
                    }
                }
            }
        }
        
        LOG_INFO_F("CTKManager", "Successfully stopped %1 plugins", stoppedCount);
        
    } catch (const std::exception& e) {
        QString error = QString("Exception during plugin shutdown: %1").arg(e.what());
        LOG_ERROR("CTKManager", error);
        emit errorOccurred(error);
    } catch (...) {
        LOG_ERROR("CTKManager", "Unknown exception during plugin shutdown");
    }
#endif
}

void CTKManager::stopFramework()
{
    if (!m_started) {
        return;
    }
    
#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        // First stop all plugins in a controlled manner
        stopPlugins();
        
        if (m_framework) {
            m_framework->stop();
            m_framework->waitForStop(5000); // 绛夊緟鏈€澶?绉?
        }
        m_pluginContext = nullptr;
        LOG_INFO("CTKManager", "CTK Framework stopped");
        
    } catch (const ctkPluginException& e) {
        QString error = QString("CTK Plugin Exception during stop: %1").arg(e.what());
        LOG_ERROR("CTKManager", error);
        emit errorOccurred(error);
    } catch (const std::exception& e) {
        QString error = QString("Standard Exception during stop: %1").arg(e.what());
        LOG_ERROR("CTKManager", error);
        emit errorOccurred(error);
    } catch (...) {
        LOG_ERROR("CTKManager", "Unknown exception occurred during CTK stop");
    }
#endif
    
    m_started = false;
    m_loadedPlugins.clear();
#ifdef CTK_PLUGIN_FRAMEWORK
    m_installedPluginHandles.clear();
    m_installedPluginNames.clear();
    m_startedPluginNames.clear();
    m_deferredPlugins.clear();
    m_onDemandPlugins.clear();
    m_pluginSourceMap.clear();
#endif
    emit frameworkStopped();
}

int CTKManager::loadPluginsFromDirectory(const QString& pluginDir)
{
    if (!m_started) {
        LOG_ERROR("CTKManager", "CTK Framework not started - cannot load plugins");
        return 0;
    }

    LOG_INFO("CTKManager", QString("Loading plugins from directory: %1").arg(pluginDir));

#ifdef CTK_PLUGIN_FRAMEWORK
    int processedCount = 0;
    QDirIterator iterator(pluginDir, QStringList() << "*.dll" << "*.so" << "*.dylib", QDir::Files);
    while (iterator.hasNext()) {
        const QString pluginPath = iterator.next();
        if (installPlugin(pluginPath, true)) {
            ++processedCount;
        }
    }

    LOG_INFO_F("CTKManager", "Processed %1 plugins from directory", processedCount);
    return processedCount;
#else
    Q_UNUSED(pluginDir);
    return 0;
#endif
}

bool CTKManager::loadPlugin(const QString& pluginPath, bool autoStart)
{
    if (!m_started) {
        LOG_ERROR("CTKManager", "CTK Framework not started - cannot load plugin");
        return false;
    }

    if (pluginPath.isEmpty()) {
        LOG_ERROR("CTKManager", "Plugin path is empty");
        return false;
    }

    LOG_INFO("CTKManager", QString("Loading plugin: %1").arg(pluginPath));

#ifdef CTK_PLUGIN_FRAMEWORK
    if (autoStart) {
        return installPlugin(pluginPath, true);
    } else {
        return installPlugin(pluginPath, false);
    }
#else
    Q_UNUSED(pluginPath);
    Q_UNUSED(autoStart);
    LOG_WARNING("CTKManager", "CTK Plugin Framework not available - cannot load plugin");
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

void CTKManager::setSafeMode(bool enabled)
{
    m_safeMode = enabled;
    LOG_INFO_F("CTKManager", "Safe mode %1", enabled ? "enabled" : "disabled");
}

bool CTKManager::isSafeMode() const
{
    return m_safeMode;
}

int CTKManager::installPluginsFromDirectory(const QString& pluginDir)
{
    if (!m_started) {
        LOG_ERROR("CTKManager", "CTK Framework not started - cannot install plugins");
        return 0;
    }

    LOG_INFO(
        "CTKManager",
        QString("Installing plugins from compatibility-only directory scan (no auto start): %1").arg(pluginDir));

#ifdef CTK_PLUGIN_FRAMEWORK
    QHash<QString, QString> pluginCandidates;
    QDirIterator iterator(pluginDir, QStringList() << "*.dll" << "*.so" << "*.dylib", QDir::Files);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        QFileInfo info(path);
        QString key = info.completeBaseName();
        if (key.startsWith(QStringLiteral("lib"), Qt::CaseInsensitive)) {
            key.remove(0, 3);
        }
        pluginCandidates.insert(key.toLower(), path);
    }

    int installedCount = 0;

    auto installByKey = [this, &pluginCandidates, &installedCount](const QString& key) {
        const QString normalized = key.toLower();
        auto it = pluginCandidates.find(normalized);
        if (it != pluginCandidates.end()) {
            if (installPlugin(it.value(), false)) {
                ++installedCount;
            }
            pluginCandidates.erase(it);
        }
    };

    for (const QString& orderedName : m_pluginLoadOrder) {
        installByKey(orderedName);
    }

    auto toInstallKeys = pluginCandidates.keys();
    for (const QString& key : toInstallKeys) {
        installByKey(key);
    }

    LOG_INFO_F("CTKManager", "Installed %1 plugins (without auto start)", installedCount);
    return installedCount;
#else
    Q_UNUSED(pluginDir);
    return 0;
#endif
}

bool CTKManager::installPlugin(const QString& pluginPath, bool autoStart, QString* outPluginName)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_started || !m_pluginContext) {
        LOG_ERROR("CTKManager", "CTK Framework not started - cannot install plugin");
        emit pluginInstallFailedDetailed(QString(), pluginPath, QStringLiteral("framework_not_started"));
        return false;
    }

    QFileInfo fileInfo(pluginPath);
    if (!fileInfo.exists()) {
        QString error = QString("Plugin file does not exist: %1").arg(pluginPath);
        LOG_ERROR("CTKManager", error);
        emit pluginInstallFailedDetailed(fileInfo.completeBaseName(), pluginPath, error);
        emit pluginLoadFailed(pluginPath, error);
        return false;
    }

    const QString canonicalPath = fileInfo.canonicalFilePath();
    emit pluginInstallStartedDetailed(fileInfo.completeBaseName(), canonicalPath);

    try {
        QSharedPointer<ctkPlugin> pluginHandle = m_pluginContext->installPlugin(QUrl::fromLocalFile(canonicalPath));
        if (!pluginHandle) {
            QString error = QStringLiteral("Failed to install plugin (null handle)");
            LOG_ERROR("CTKManager", error);
            emit pluginInstallFailedDetailed(fileInfo.completeBaseName(), canonicalPath, error);
            emit pluginLoadFailed(pluginPath, error);
            return false;
        }

        const QString pluginName = pluginHandle->getSymbolicName();
        m_installedPluginHandles.insert(pluginName, pluginHandle);
        m_installedPluginNames.insert(pluginName);
        m_pluginSourceMap.insert(pluginName, canonicalPath);

        if (outPluginName) {
            *outPluginName = pluginName;
        }

        LOG_INFO_F("CTKManager", "Installed plugin: %1", pluginName);
        emit pluginInstalled(pluginName, canonicalPath);

        if (!applyPolicyForPlugin(pluginName, autoStart, false)) {
            LOG_WARNING_F("CTKManager", "Policy application failed for plugin %1", pluginName);
        }

        if (autoStart && !m_startedPluginNames.contains(pluginName)) {
            QString error = QString("Plugin installed but failed to start: %1").arg(pluginName);
            LOG_WARNING("CTKManager", error);
            emit pluginStartFailedDetailed(pluginName, error);
            emit pluginLoadFailed(pluginPath, error);
            return false;
        }

        return true;

    } catch (const ctkPluginException& e) {
        QString error = QString("CTK Plugin Exception: %1 (path: %2)").arg(e.what(), canonicalPath);
        LOG_ERROR("CTKManager", error);
        // 灏濊瘯鑾峰彇鏇村閿欒淇℃伅
        const ctkException* cause = e.cause();
        if (cause) {
            LOG_ERROR_F("CTKManager", "  Cause: %1", cause->message());
        }
        emit pluginInstallFailedDetailed(fileInfo.completeBaseName(), canonicalPath, error);
        emit pluginLoadFailed(pluginPath, error);
        return false;
    } catch (const std::exception& e) {
        QString error = QString("Standard Exception: %1 (path: %2)").arg(e.what(), canonicalPath);
        LOG_ERROR("CTKManager", error);
        emit pluginInstallFailedDetailed(fileInfo.completeBaseName(), canonicalPath, error);
        emit pluginLoadFailed(pluginPath, error);
        return false;
    } catch (...) {
        QString error = QString("Unknown exception during plugin installation (path: %1)").arg(canonicalPath);
        LOG_ERROR("CTKManager", error);
        emit pluginInstallFailedDetailed(fileInfo.completeBaseName(), canonicalPath, error);
        emit pluginLoadFailed(pluginPath, error);
        return false;
    }
#else
    Q_UNUSED(autoStart);
    emit pluginInstallFailedDetailed(QString(), pluginPath, QStringLiteral("ctk_not_available"));
    return false;
#endif
}

bool CTKManager::startPlugin(const QString& pluginName)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_started || !m_pluginContext) {
        LOG_ERROR("CTKManager", "CTK Framework not started - cannot start plugin");
        emit pluginStartFailedDetailed(pluginName, QStringLiteral("framework_not_started"));
        return false;
    }

    QSet<QString> visiting;
    const auto normalizedPluginName = pluginName.trimmed();
    const bool started = startPluginInternal(normalizedPluginName, visiting);
    if (!started) {
        emit pluginStartFailedDetailed(
            normalizedPluginName,
            QStringLiteral("start_plugin_internal_failed"));
    }
    return started;
#else
    emit pluginStartFailedDetailed(pluginName, QStringLiteral("ctk_not_available"));
    return false;
#endif
}

bool CTKManager::startPlugins(const QStringList& pluginNames, bool stopOnFailure)
{
    bool success = true;
    for (const QString& name : pluginNames) {
        if (!startPlugin(name)) {
            success = false;
            if (stopOnFailure) {
                break;
            }
        }
    }
    return success;
}

bool CTKManager::startDeferredPlugins(bool stopOnFailure)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    QStringList deferredList = getDeferredPlugins();
    bool success = true;
    for (const QString& pluginName : deferredList) {
        if (!startPlugin(pluginName)) {
            success = false;
            if (stopOnFailure) {
                break;
            }
        }
    }
    return success;
#else
    Q_UNUSED(stopOnFailure);
    return false;
#endif
}

void CTKManager::loadPluginPolicy(const QString& configPath)
{
    if (configPath.isEmpty()) {
        return;
    }
    LOG_INFO(
        "CTKManager",
        QString("Loading compatibility-only plugin policy metadata from: %1").arg(configPath));
    PluginLoadPolicy::instance()->loadConfig(configPath);
}

void CTKManager::setDescriptorPolicyContext(
    const PlatformRuntimeConfig& runtimeConfig,
    const QVector<PlatformPluginDescriptor>& descriptors)
{
    m_descriptorPolicyRuntimeConfig = runtimeConfig;
    m_descriptorPolicyDescriptors = descriptors;
    m_descriptorPolicyContextInitialized = true;
    LOG_INFO_F(
        "CTKManager",
        "Descriptor policy context updated (runtimeMode=%1, descriptors=%2, corePlugins=%3)",
        static_cast<int>(runtimeConfig.runtimeMode),
        descriptors.size(),
        runtimeConfig.corePluginIds.size());
}

QStringList CTKManager::getLoadedPlugins() const
{
    return m_loadedPlugins;
}

QStringList CTKManager::getInstalledPlugins() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    QStringList list = m_installedPluginNames.values();
    std::sort(list.begin(), list.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return list;
#else
    return {};
#endif
}

QStringList CTKManager::getDeferredPlugins() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    QStringList list = m_deferredPlugins.values();
    std::sort(list.begin(), list.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return list;
#else
    return {};
#endif
}

QStringList CTKManager::getOnDemandPlugins() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    QStringList list = m_onDemandPlugins.values();
    std::sort(list.begin(), list.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return list;
#else
    return {};
#endif
}

QStringList CTKManager::getStartedPlugins() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    QStringList list = m_startedPluginNames.values();
    std::sort(list.begin(), list.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return list;
#else
    return {};
#endif
}

bool CTKManager::isPluginStarted(const QString& pluginName) const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    return m_startedPluginNames.contains(pluginName);
#else
    Q_UNUSED(pluginName);
    return false;
#endif
}

QString CTKManager::getPluginState(const QString& pluginName) const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_pluginContext) {
        return QStringLiteral("UNKNOWN");
    }

    QList<QSharedPointer<ctkPlugin>> plugins = m_pluginContext->getPlugins();
    for (const auto& plugin : plugins) {
        if (plugin && !plugin->getSymbolicName().compare(pluginName, Qt::CaseInsensitive)) {
            return getPluginStateString(plugin->getState());
        }
    }

    return QStringLiteral("NOT_INSTALLED");
#else
    Q_UNUSED(pluginName);
    return QStringLiteral("N/A");
#endif
}

#ifdef CTK_PLUGIN_FRAMEWORK
bool CTKManager::startPluginInternal(const QString& pluginName, QSet<QString>& visiting)
{
    if (pluginName.isEmpty()) {
        LOG_WARNING("CTKManager", "Cannot start plugin with empty name");
        emit pluginStartFailedDetailed(pluginName, QStringLiteral("plugin_name_empty"));
        return false;
    }
    emit pluginStartRequestedDetailed(pluginName);

    QString resolvedName = pluginName;
    if (m_startedPluginNames.contains(resolvedName)) {
        return true;
    }

    auto resolveHandle = [this](const QString& name) -> QSharedPointer<ctkPlugin> {
        QSharedPointer<ctkPlugin> handle = m_installedPluginHandles.value(name);
        if (handle) {
            return handle;
        }

        for (auto it = m_installedPluginHandles.constBegin(); it != m_installedPluginHandles.constEnd(); ++it) {
            if (!it.key().compare(name, Qt::CaseInsensitive)) {
                return it.value();
            }
        }

        if (m_pluginContext) {
            QList<QSharedPointer<ctkPlugin>> plugins = m_pluginContext->getPlugins();
            for (const auto& plugin : plugins) {
                if (plugin && !plugin->getSymbolicName().compare(name, Qt::CaseInsensitive)) {
                    return plugin;
                }
            }
        }

        return {};
    };

    QSharedPointer<ctkPlugin> pluginHandle = resolveHandle(resolvedName);
    if (!pluginHandle) {
        const QString sourcePath = m_pluginSourceMap.value(resolvedName);
        if (!sourcePath.isEmpty() && installPlugin(sourcePath, false)) {
            pluginHandle = resolveHandle(resolvedName);
        }
    }

    if (!pluginHandle) {
        LOG_WARNING_F("CTKManager", "Plugin handle not found for %1", resolvedName);
        emit pluginStartFailedDetailed(resolvedName, QStringLiteral("plugin_handle_not_found"));
        return false;
    }

    resolvedName = pluginHandle->getSymbolicName();

    if (m_startedPluginNames.contains(resolvedName)) {
        return true;
    }

    if (visiting.contains(resolvedName)) {
        LOG_ERROR_F("CTKManager", "Detected cyclic plugin dependency involving %1", resolvedName);
        emit pluginStartFailedDetailed(resolvedName, QStringLiteral("plugin_dependency_cycle"));
        return false;
    }

    visiting.insert(resolvedName);
    QScopedValueRollback<QSet<QString>> rollback(visiting);

    QStringList dependencies = manifestDependenciesForPlugin(resolvedName);
    QVector<QString> newlyStartedDependencies;
    newlyStartedDependencies.reserve(dependencies.size());

    for (const QString& dependency : dependencies) {
        const QString trimmed = dependency.trimmed();
        if (trimmed.isEmpty() || !trimmed.compare(resolvedName, Qt::CaseInsensitive)) {
            continue;
        }

        const bool alreadyRunning = m_startedPluginNames.contains(trimmed);
        if (!startPluginInternal(trimmed, visiting)) {
            LOG_WARNING_F("CTKManager", "Failed to start dependency %1 required by %2", trimmed, resolvedName);
            for (auto it = newlyStartedDependencies.crbegin(); it != newlyStartedDependencies.crend(); ++it) {
                stopPluginInternal(*it);
            }
            return false;
        }

        if (!alreadyRunning && m_startedPluginNames.contains(trimmed)) {
            newlyStartedDependencies.append(trimmed);
        }
    }

    if (!activatePlugin(resolvedName)) {
        LOG_WARNING_F("CTKManager", "Failed to activate plugin %1", resolvedName);
        for (auto it = newlyStartedDependencies.crbegin(); it != newlyStartedDependencies.crend(); ++it) {
            stopPluginInternal(*it);
        }
        return false;
    }

    m_deferredPlugins.remove(resolvedName);
    m_onDemandPlugins.remove(resolvedName);

    return true;
}

bool CTKManager::activatePlugin(const QString& pluginName)
{
    QSharedPointer<ctkPlugin> pluginHandle = m_installedPluginHandles.value(pluginName);
    if (!pluginHandle) {
        QList<QSharedPointer<ctkPlugin>> plugins = m_pluginContext->getPlugins();
        for (const auto& plugin : plugins) {
            if (plugin && plugin->getSymbolicName() == pluginName) {
                pluginHandle = plugin;
                break;
            }
        }
    }

    if (!pluginHandle) {
        LOG_WARNING_F("CTKManager", "Cannot activate plugin %1: handle missing", pluginName);
        emit pluginStartFailedDetailed(pluginName, QStringLiteral("plugin_handle_missing"));
        return false;
    }

    if (pluginHandle->getState() == ctkPlugin::ACTIVE) {
        if (!m_startedPluginNames.contains(pluginName)) {
            m_startedPluginNames.insert(pluginName);
        }
        if (!m_loadedPlugins.contains(pluginName)) {
            m_loadedPlugins.append(pluginName);
        }
        emit pluginStartedDetailed(pluginName);
        emit pluginLoaded(pluginName);
        return true;
    }

    try {
        pluginHandle->start(ctkPlugin::START_TRANSIENT);
    } catch (const ctkPluginException& ex) {
        LOG_ERROR_F("CTKManager", "Failed to start plugin %1: %2", pluginName, ex.what());
        emit pluginStartFailedDetailed(pluginName, QString::fromUtf8(ex.what()));
        return false;
    } catch (const std::exception& ex) {
        LOG_ERROR_F("CTKManager", "Exception while starting plugin %1: %2", pluginName, ex.what());
        emit pluginStartFailedDetailed(pluginName, QString::fromUtf8(ex.what()));
        return false;
    } catch (...) {
        LOG_ERROR_F("CTKManager", "Unknown exception while starting plugin %1", pluginName);
        emit pluginStartFailedDetailed(pluginName, QStringLiteral("unknown_start_exception"));
        return false;
    }

    if (pluginHandle->getState() == ctkPlugin::ACTIVE) {
        m_startedPluginNames.insert(pluginName);
        if (!m_loadedPlugins.contains(pluginName)) {
            m_loadedPlugins.append(pluginName);
        }
        m_deferredPlugins.remove(pluginName);
        m_onDemandPlugins.remove(pluginName);
        emit pluginStartedDetailed(pluginName);
        emit pluginLoaded(pluginName);
        LOG_INFO_F("CTKManager", "Plugin started successfully: %1", pluginName);
        return true;
    }

    QString stateStr = getPluginStateString(pluginHandle->getState());
    LOG_WARNING_F("CTKManager", "Plugin %1 failed to reach ACTIVE state (state: %2)", pluginName, stateStr);
    emit pluginStartFailedDetailed(pluginName, stateStr);
    return false;
}

bool CTKManager::stopPluginInternal(const QString& pluginName)
{
    if (!m_startedPluginNames.contains(pluginName)) {
        return true;
    }

    QSharedPointer<ctkPlugin> pluginHandle = m_installedPluginHandles.value(pluginName);
    if (!pluginHandle) {
        QList<QSharedPointer<ctkPlugin>> plugins = m_pluginContext->getPlugins();
        for (const auto& plugin : plugins) {
            if (plugin && plugin->getSymbolicName() == pluginName) {
                pluginHandle = plugin;
                break;
            }
        }
    }

    if (!pluginHandle) {
        return false;
    }

    try {
        if (pluginHandle->getState() == ctkPlugin::ACTIVE) {
            pluginHandle->stop(ctkPlugin::STOP_TRANSIENT);
        }
    } catch (const ctkPluginException& ex) {
        LOG_WARNING_F("CTKManager", "Failed to rollback plugin %1: %2", pluginName, ex.what());
    } catch (const std::exception& ex) {
        LOG_WARNING_F("CTKManager", "Exception while rolling back plugin %1: %2", pluginName, ex.what());
    } catch (...) {
        LOG_WARNING_F("CTKManager", "Unknown exception while rolling back plugin %1", pluginName);
    }

    m_startedPluginNames.remove(pluginName);
    m_loadedPlugins.removeAll(pluginName);
    return true;
}

QStringList CTKManager::manifestDependenciesForPlugin(const QString& pluginName) const
{
    const QString manifestPath = locateManifestForPlugin(pluginName);
    if (manifestPath.isEmpty()) {
        return {};
    }
    return parseManifestDependencies(manifestPath);
}

QString CTKManager::locateManifestForPlugin(const QString& pluginName) const
{
    const QString sourcePath = m_pluginSourceMap.value(pluginName);
    if (!sourcePath.isEmpty()) {
        QFileInfo info(sourcePath);
        QDir dir(info.absolutePath());
        QString manifest = dir.filePath(QStringLiteral("MANIFEST.MF"));
        if (QFile::exists(manifest)) {
            return manifest;
        }
        manifest = dir.filePath(QStringLiteral("META-INF/MANIFEST.MF"));
        if (QFile::exists(manifest)) {
            return manifest;
        }
    }

    QSharedPointer<ctkPlugin> pluginHandle = m_installedPluginHandles.value(pluginName);
    if (pluginHandle) {
        const QString location = pluginHandle->getLocation();
        if (!location.isEmpty()) {
            QFileInfo info(location);
            QDir dir(info.absolutePath());
            QString manifest = dir.filePath(QStringLiteral("MANIFEST.MF"));
            if (QFile::exists(manifest)) {
                return manifest;
            }
            manifest = dir.filePath(QStringLiteral("META-INF/MANIFEST.MF"));
            if (QFile::exists(manifest)) {
                return manifest;
            }
        }
    }

    return {};
}

QStringList CTKManager::parseManifestDependencies(const QString& manifestPath) const
{
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_WARNING_F("CTKManager", "Failed to open manifest: %1", manifestPath);
        return {};
    }

    QTextStream stream(&file);
    QStringList lines;
    while (!stream.atEnd()) {
        lines.append(stream.readLine());
    }
    file.close();

    QString requireSection;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (line.startsWith(QStringLiteral("Require-Bundle"), Qt::CaseInsensitive)) {
            const int colonIndex = line.indexOf(QLatin1Char(':'));
            if (colonIndex > -1) {
                requireSection = line.mid(colonIndex + 1).trimmed();
            }
            int j = i + 1;
            while (j < lines.size() && lines[j].startsWith(QLatin1Char(' '))) {
                requireSection += lines[j].trimmed();
                ++j;
            }
            break;
        }
    }

    QStringList dependencies;
    for (const QString& token : requireSection.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        QString dep = token.trimmed();
        if (!dep.isEmpty()) {
            dependencies.append(dep);
        }
    }

    return dependencies;
}

bool CTKManager::applyPolicyForPlugin(const QString& pluginName, bool allowStart, bool forceStart)
{
    m_deferredPlugins.remove(pluginName);
    m_onDemandPlugins.remove(pluginName);

    PlatformCtkPolicyBridgeResult resolved;
    QString resolutionStatus = QStringLiteral("descriptor_policy_context_missing");
    bool shouldLogFallbackWarning = false;

    if (!m_descriptorPolicyContextInitialized) {
        resolved.ctkSymbolicName = pluginName.trimmed();
        resolved.loadBucket = PlatformCtkLoadBucket::OnDemand;
        resolved.isCritical = false;
        resolved.diagnosticCode = QStringLiteral("descriptor_policy_context_missing_for_ctk_manager");
        QVariantMap context;
        context.insert(QStringLiteral("plugin"), pluginName);
        context.insert(QStringLiteral("diagnostic_code"), resolved.diagnosticCode);
        context.insert(QStringLiteral("resolution_status"), resolutionStatus);
        context.insert(QStringLiteral("load_bucket"), loadBucketToString(resolved.loadBucket));
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Warning,
            QStringLiteral("Descriptor policy context missing for CTKManager runtime classification: %1").arg(pluginName),
            context);
    } else {
        resolved = PlatformCtkPolicyBridge::resolve(
            m_descriptorPolicyRuntimeConfig,
            m_descriptorPolicyDescriptors,
            pluginName);
        resolutionStatus = resolutionStatusToString(resolved.resolutionStatus);
        shouldLogFallbackWarning =
            resolved.resolutionStatus == PlatformCtkPolicyResolutionStatus::DescriptorMissingFallback;
    }

    if (shouldLogFallbackWarning) {
        QVariantMap context;
        context.insert(QStringLiteral("plugin"), pluginName);
        context.insert(QStringLiteral("diagnostic_code"), resolved.diagnosticCode);
        context.insert(QStringLiteral("resolution_status"), resolutionStatus);
        context.insert(QStringLiteral("load_bucket"), loadBucketToString(resolved.loadBucket));
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Warning,
            QStringLiteral("Descriptor policy bridge fallback applied for CTK plugin: %1").arg(pluginName),
            context);
    }

    if (m_safeMode && !resolved.isCritical) {
        LOG_INFO_F("CTKManager", "Safe mode skipped non-core plugin: %1", pluginName);
        QVariantMap context;
        context.insert(QStringLiteral("plugin"), pluginName);
        context.insert(QStringLiteral("diagnostic_code"), QStringLiteral("safe_mode_skipped_non_core_plugin"));
        context.insert(QStringLiteral("resolution_status"), resolutionStatus);
        context.insert(QStringLiteral("load_bucket"), loadBucketToString(resolved.loadBucket));
        StartupOrchestrator::instance()->logDiagnostic(
            ErrorHandler::ErrorLevel::Info,
            QStringLiteral("Safe mode skipped non-core plugin: %1").arg(pluginName),
            context);
        return true;
    }

    switch (resolved.loadBucket) {
    case PlatformCtkLoadBucket::Immediate: {
        if (!m_startedPluginNames.contains(pluginName)) {
            if (forceStart) {
                allowStart = true;
            }
            if (allowStart) {
                QSet<QString> visiting;
                if (!startPluginInternal(pluginName, visiting)) {
                    return false;
                }
            }
        }
        return true;
    }
    case PlatformCtkLoadBucket::Deferred:
        if (!m_startedPluginNames.contains(pluginName)) {
            m_deferredPlugins.insert(pluginName);
        }
        return true;
    case PlatformCtkLoadBucket::OnDemand:
    default:
        if (!m_startedPluginNames.contains(pluginName)) {
            m_onDemandPlugins.insert(pluginName);
        }
        return true;
    }
}

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
        LOG_INFO("CTKManager", "Cannot start EventAdmin: Plugin context not available");
        return false;
    }

    LOG_INFO("CTKManager", "Starting EventAdmin service initialization...");

    try {
        // 棣栧厛灏濊瘯鍔犺浇EventAdmin鎻掍欢
        QString appDir = QCoreApplication::applicationDirPath();
        QString currentDir = QDir::currentPath();

        logMessage(QString("Application directory: %1").arg(appDir));
        logMessage(QString("Current directory: %1").arg(currentDir));

        QStringList possiblePaths;

        // 娣诲姞鍙兘鐨勮矾寰勶紙浼樺厛鍦ㄨ緭鍑虹洰褰曠殑plugins鏂囦欢澶逛腑鏌ユ壘锛?
        possiblePaths << appDir + "/plugins/liborg_commontk_eventadmin.dll"  // 杈撳嚭鐩綍鐨刾lugins鏂囦欢澶癸紙鏈€浼樺厛锛?
                     << currentDir + "/plugins/liborg_commontk_eventadmin.dll"  // 褰撳墠鐩綍鐨刾lugins鏂囦欢澶?
                     << appDir + "/../ThirdParty/CTK/CTK_install/lib/ctk-0.1/plugins/liborg_commontk_eventadmin.dll"
                     << "ThirdParty/CTK/CTK_install/lib/ctk-0.1/plugins/liborg_commontk_eventadmin.dll"
                     << appDir + "/../../ThirdParty/CTK/CTK_install/lib/ctk-0.1/plugins/liborg_commontk_eventadmin.dll"
                     << currentDir + "/ThirdParty/CTK/CTK_install/lib/ctk-0.1/plugins/liborg_commontk_eventadmin.dll";

        QString eventAdminPluginPath;
        bool found = false;

        LOG_INFO("CTKManager", "Searching for EventAdmin plugin...");
        for (const QString& path : possiblePaths) {
            LOG_INFO_F("CTKManager", "  Checking: %1", path);
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
            LOG_INFO("CTKManager", "EventAdmin plugin not found at any expected locations");
            return false;
        }

        // 瀹夎骞跺惎鍔‥ventAdmin鎻掍欢
        LOG_INFO("CTKManager", "Installing EventAdmin plugin...");
        QUrl pluginUrl = QUrl::fromLocalFile(QFileInfo(eventAdminPluginPath).absoluteFilePath());
        LOG_INFO_F("CTKManager", "Plugin URL: %1", pluginUrl.toString());

        QSharedPointer<ctkPlugin> eventAdminPlugin = m_pluginContext->installPlugin(pluginUrl);

        if (eventAdminPlugin) {
            LOG_INFO("CTKManager", "EventAdmin plugin installed successfully, starting...");
            eventAdminPlugin->start();
            LOG_INFO("CTKManager", "EventAdmin plugin started successfully");

            // 绛夊緟涓€灏忔鏃堕棿璁╂湇鍔℃敞鍐?
            QThread::msleep(100);

            // 鐜板湪灏濊瘯鑾峰彇EventAdmin鏈嶅姟
            LOG_INFO("CTKManager", "Looking for EventAdmin service...");
            ctkServiceReference eventAdminRef = m_pluginContext->getServiceReference<ctkEventAdmin>();
            if (eventAdminRef) {
                LOG_INFO("CTKManager", "EventAdmin service reference found");
                m_eventAdmin = m_pluginContext->getService<ctkEventAdmin>(eventAdminRef);
                if (m_eventAdmin) {
                    LOG_INFO("CTKManager", "EventAdmin service connected successfully");
                    return true;
                } else {
                    LOG_INFO("CTKManager", "Failed to get EventAdmin service instance");
                }
            } else {
                LOG_INFO("CTKManager", "EventAdmin service reference not found after plugin start");
            }
        } else {
            LOG_INFO("CTKManager", "Failed to install EventAdmin plugin");
        }

    } catch (const ctkPluginException& e) {
        LOG_INFO_F("CTKManager", "CTK Plugin Exception while starting EventAdmin: %1", e.what());
    } catch (const std::exception& e) {
        LOG_INFO_F("CTKManager", "Exception while starting EventAdmin: %1", e.what());
    } catch (...) {
        LOG_INFO("CTKManager", "Unknown exception while starting EventAdmin");
    }

    return false;
}

ctkEventAdmin* CTKManager::getEventAdmin() const
{
    return m_eventAdmin;
}
#endif

// Deprecated: Use LOG_* macros from Logger.h instead
void CTKManager::logMessage(const QString& message)
{
    LOG_INFO("CTKManager", message);
}

void CTKManager::setPluginLoadOrder(const QStringList& order)
{
    m_pluginLoadOrder = order;
    LOG_INFO_F("CTKManager", "Plugin load order set: %1", order.join(", "));
}

QStringList CTKManager::getRecommendedLoadOrder() const
{
    return {
        "UserManagement",      // 基础用户服务
        "DicomViewer",         // DICOM 影像查看
        "FourViewDisplay",     // 四视图显示
        "BoneSegmentation",    // AI 骨骼分割
        "PointRegistration",   // 点配准
        "RegistrationCore",    // 配准核心算法
        "InstrumentManagement",// 器械管理（按需）
        "OpticalTracking",     // 光学跟踪（按需）
        "Registration2D3D",    // 2D-3D 配准（按需）
        "OpticalRegistration"  // 光学配准（按需）
    };
}

bool CTKManager::verifyRequiredServices(const QStringList& serviceNames)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_pluginContext) {
        LOG_ERROR("CTKManager", "Cannot verify services: Plugin context not available");
        return false;
    }
    
    QStringList missing = getMissingServices(serviceNames);
    
    if (missing.isEmpty()) {
        LOG_INFO("CTKManager", "All required services are available");
        return true;
    } else {
        LOG_WARNING_F("CTKManager", "Missing required services: %1", missing.join(", "));
        return false;
    }
#else
    LOG_ERROR("CTKManager", "Cannot verify services: CTK Plugin Framework not available");
    return false;
#endif
}

QString CTKManager::getFrameworkDiagnostics() const
{
    QString diagnostics;
    QTextStream stream(&diagnostics);
    
    stream << "=== CTK Framework Diagnostics ===" << "\n";
    stream << "Initialized: " << (m_initialized ? "Yes" : "No") << "\n";
    stream << "Started: " << (m_started ? "Yes" : "No") << "\n";
    stream << "CTK Available: " << (isCTKAvailable() ? "Yes" : "No") << "\n";
    stream << "\n";
    
#ifdef CTK_PLUGIN_FRAMEWORK
    if (m_pluginContext) {
        stream << "Plugin Context: Available" << "\n";
        
        // 鑾峰彇鎵€鏈夋彃浠?
        QList<QSharedPointer<ctkPlugin>> plugins = m_pluginContext->getPlugins();
        stream << "Total Plugins: " << plugins.size() << "\n";
        stream << "\n";
        
        stream << "=== Plugin Status ===" << "\n";
        for (const auto& plugin : plugins) {
            if (plugin) {
                QString name = plugin->getSymbolicName();
                int state = plugin->getState();
                QString stateStr = getPluginStateString(state);
                stream << "  " << name << ": " << stateStr << "\n";
            }
        }
        stream << "\n";
        
        stream << "=== Loaded Plugins ===" << "\n";
        for (const QString& pluginName : m_loadedPlugins) {
            stream << "  - " << pluginName << "\n";
        }
        stream << "\n";
        
        stream << "=== EventAdmin Service ===" << "\n";
        stream << "EventAdmin: " << (m_eventAdmin ? "Available" : "Not Available") << "\n";
        
    } else {
        stream << "Plugin Context: Not Available" << "\n";
    }
#else
    stream << "CTK Plugin Framework: Not Compiled" << "\n";
#endif
    
    return diagnostics;
}

QStringList CTKManager::getMissingServices(const QStringList& required) const
{
    QStringList missing;
    
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_pluginContext) {
        LOG_INFO("CTKManager", "Cannot check services: Plugin context not available");
        return required; // 鎵€鏈夋湇鍔￠兘缂哄け
    }
    
    for (const QString& serviceName : required) {
        try {
            // 灏濊瘯鑾峰彇鏈嶅姟寮曠敤
            ctkServiceReference ref = m_pluginContext->getServiceReference(serviceName);
            if (!ref) {
                missing.append(serviceName);
            }
        } catch (...) {
            missing.append(serviceName);
        }
    }
#else
    missing = required; // CTK涓嶅彲鐢紝鎵€鏈夋湇鍔￠兘缂哄け
#endif
    
    return missing;
}

QMap<QString, QString> CTKManager::getPluginStatus() const
{
    QMap<QString, QString> statusMap;
    
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_pluginContext) {
        return statusMap;
    }
    
    QList<QSharedPointer<ctkPlugin>> plugins = m_pluginContext->getPlugins();
    for (const auto& plugin : plugins) {
        if (plugin) {
            QString name = plugin->getSymbolicName();
            int state = plugin->getState();
            QString stateStr = getPluginStateString(state);
            statusMap[name] = stateStr;
        }
    }
#endif
    
    return statusMap;
}

QString CTKManager::getPluginStateString(int state) const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    switch (state) {
        case ctkPlugin::UNINSTALLED:
            return "UNINSTALLED";
        case ctkPlugin::INSTALLED:
            return "INSTALLED";
        case ctkPlugin::RESOLVED:
            return "RESOLVED";
        case ctkPlugin::STARTING:
            return "STARTING";
        case ctkPlugin::STOPPING:
            return "STOPPING";
        case ctkPlugin::ACTIVE:
            return "ACTIVE";
        default:
            return QString("UNKNOWN(%1)").arg(state);
    }
#else
    return QString("N/A(%1)").arg(state);
#endif
}

QString CTKManager::verifyPluginServices()
{
    QString report;
    QTextStream stream(&report);
    
    stream << "=== Plugin Service Verification ===" << "\n";
    
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_pluginContext) {
        stream << "ERROR: Plugin context not available" << "\n";
        return report;
    }
    
    // 瀹氫箟鏍稿績鏈嶅姟鍙婂叾鎻愪緵鎻掍欢
    QMap<QString, QString> coreServices;
    coreServices["UserManagementService"] = "UserManagement";
    coreServices["DicomViewerService"] = "DicomViewer";
    coreServices["SegmentationService"] = "BoneSegmentation";
    coreServices["FourViewDisplayService"] = "FourViewDisplay";
    coreServices["PointRegistrationService"] = "PointRegistration";
    coreServices["RegistrationService"] = "RegistrationCore";
    
    int availableCount = 0;
    int missingCount = 0;
    
    stream << "\n=== Core Services Status ===" << "\n";
    
    for (auto it = coreServices.begin(); it != coreServices.end(); ++it) {
        QString serviceName = it.key();
        QString pluginName = it.value();
        
        try {
            ctkServiceReference ref = m_pluginContext->getServiceReference(serviceName);
            if (ref) {
                stream << "  [OK] " << serviceName << " (from " << pluginName << "): AVAILABLE" << "\\n";
                availableCount++;
            } else {
                stream << "  [MISSING] " << serviceName << " (from " << pluginName << "): MISSING" << "\\n";
                missingCount++;
            }
        } catch (const std::exception& e) {
            stream << "  [ERROR] " << serviceName << " (from " << pluginName << "): ERROR - " << e.what() << "\\n";
            missingCount++;
        } catch (...) {
            stream << "  [ERROR] " << serviceName << " (from " << pluginName << "): ERROR - Unknown exception" << "\\n";
            missingCount++;
        }
    }
    
    stream << "\n=== Summary ===" << "\n";
    stream << "Available Services: " << availableCount << "\n";
    stream << "Missing Services: " << missingCount << "\n";
    stream << "Total Core Services: " << coreServices.size() << "\n";
    
    if (missingCount > 0) {
        stream << "\nWARNING: Some core services are missing. Related functionality may not work." << "\n";
    } else {
        stream << "\nSUCCESS: All core services are available." << "\n";
    }
    
    // 鍒楀嚭鎵€鏈夊彲鐢ㄧ殑鏈嶅姟
    stream << "\n=== All Available Services ===" << "\n";
    try {
        QList<ctkServiceReference> allRefs = m_pluginContext->getServiceReferences(QString());
        if (allRefs.isEmpty()) {
            stream << "  No services registered" << "\n";
        } else {
            for (const ctkServiceReference& ref : allRefs) {
                if (ref) {
                    QStringList objectClass = ref.getProperty("objectClass").toStringList();
                    if (!objectClass.isEmpty()) {
                        stream << "  - " << objectClass.join(", ") << "\n";
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        stream << "  ERROR: Failed to list services - " << e.what() << "\n";
    }
    
#else
    stream << "ERROR: CTK Plugin Framework not compiled" << "\n";
#endif
    
    LOG_INFO("CTKManager", "Service verification completed");
    return report;
}

