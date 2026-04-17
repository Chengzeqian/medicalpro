#include "UI/MainInterfaceWidget.h"
#include "UI/AppTheme.h"

#include "Framework/CTKManager.h"
#include "Framework/ConsoleLogBridge.h"
#include "Framework/PluginLoadPolicy.h"
#include "Framework/StartupOrchestrator.h"
#include "Framework/VTKGlobalInitializer.h"
#include "Framework/VTKWidgetPool.h"
#ifdef CTK_PLUGIN_FRAMEWORK
#include "Plugins/Registration2D3D/Registration2D3DService.h"
#include "Plugins/FourViewDisplay/FourViewDisplayService.h"
#include "Plugins/UserManagement/UserManagementService.h"
#endif

#include <QApplication>
#include <QEvent>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QLocale>
#include <QMessageBox>
#include <QSemaphore>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <exception>
#include <cstdio>

#ifdef _WIN32
#  include <windows.h>
#endif

#ifdef VTK_FOUND
#include <QVTKOpenGLNativeWidget.h>
#include <vtkImageData.h>
#include <vtkObject.h>
#endif

using MedicalProBaseApplication = QApplication;

class SafeApplication : public MedicalProBaseApplication
{
public:
    using MedicalProBaseApplication::MedicalProBaseApplication;

protected:
    bool notify(QObject* receiver, QEvent* event) override
    {
        try {
            return MedicalProBaseApplication::notify(receiver, event);
        } catch (const std::exception& e) {
            reportException(QString::fromUtf8(e.what()), receiver, event);
        } catch (...) {
            reportException(QStringLiteral("Unknown exception"), receiver, event);
        }
        return false;
    }

private:
    void reportException(const QString& detail, QObject* receiver, QEvent* event)
    {
        if (m_handlingException) {
            qCritical() << "[SafeApplication] Re-entrant exception detected; duplicate dialog skipped";
            return;
        }

        m_handlingException = true;

        const QString receiverName = receiver ? receiver->objectName() : QStringLiteral("<null>");
        const int eventType = event ? static_cast<int>(event->type()) : -1;

        qCritical() << "[SafeApplication] Qt event exception captured"
                    << "receiver:" << receiver
                    << "name:" << receiverName
                    << "eventType:" << eventType
                    << "detail:" << detail;

        QMessageBox::critical(
            nullptr,
            QStringLiteral("运行异常"),
            QStringLiteral("处理界面事件时发生异常，操作已安全终止。\n\n详情：%1")
                .arg(detail));

        m_handlingException = false;
    }

    bool m_handlingException = false;
};

namespace
{

// 在应用启动早期配置第三方 DLL 搜索路径，避免插件在加载 VTK 相关模块时出现
// "找不到指定的模块"（Win32 错误 126）。
#ifdef _WIN32
static bool isRedirectedStdHandle(DWORD stdHandleId)
{
    HANDLE handle = GetStdHandle(stdHandleId);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD consoleMode = 0;
    if (GetConsoleMode(handle, &consoleMode) != 0) {
        return false;
    }

    const DWORD fileType = GetFileType(handle);
    return fileType == FILE_TYPE_DISK || fileType == FILE_TYPE_PIPE;
}

static void enableDebugConsole()
{
    const bool preserveRedirectedHandles =
        isRedirectedStdHandle(STD_OUTPUT_HANDLE) || isRedirectedStdHandle(STD_ERROR_HANDLE);

    if (preserveRedirectedHandles) {
        return;
    }

    if (GetConsoleWindow() == nullptr) {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            AllocConsole();
        }
    }

    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);

    const UINT consoleCodePage = GetACP();
    SetConsoleOutputCP(consoleCodePage);
    SetConsoleCP(consoleCodePage);
    SetConsoleTitleW(L"MedicalPro Debug Console");
}
#else
static void enableDebugConsole()
{
}
#endif

static void configureThirdPartyDllSearchPaths()
{
#ifdef _WIN32
    QString appDir = QCoreApplication::applicationDirPath();

    // appDir: .../build/Desktop_.../Release
    // 项目根目录: 上两级
    QDir projectDir(appDir);
    projectDir.cdUp(); // Release -> build
    projectDir.cdUp(); // build   -> project root

    QStringList candidateDirs;
    candidateDirs << projectDir.absoluteFilePath("ThirdParty/VTK/VTK-install/bin");
    candidateDirs << projectDir.absoluteFilePath("ThirdParty/CTK/CTK_install/lib/ctk-0.1");
    candidateDirs << projectDir.absoluteFilePath("ThirdParty/ITK/ITK-install/bin");

    QStringList validDirs;
    for (const QString& dir : candidateDirs) {
        if (QDir(dir).exists()) {
            validDirs << QDir::toNativeSeparators(dir);
        }
    }

    if (validDirs.isEmpty()) {
        qDebug() << "[main] No third-party DLL directories found; PATH update skipped";
        return;
    }

    // 1) 扩展 PATH，兼容旧版 Windows 的搜索行为
    const QString currentPath = qEnvironmentVariable("PATH");
    const QString newPath = validDirs.join(";") + ";" + currentPath;
    qputenv("PATH", newPath.toUtf8());
    qDebug() << "[main] PATH extended with third-party DLL directories:" << validDirs;

    // 2) 可用时注册到 AddDllDirectory，兼容 Win8+ 在调用
    //    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) 之后的行为
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) {
        return;
    }

    typedef DLL_DIRECTORY_COOKIE (WINAPI *AddDllDirectoryFunc)(PCWSTR);
    AddDllDirectoryFunc addDllDirectory =
        reinterpret_cast<AddDllDirectoryFunc>(GetProcAddress(kernel32, "AddDllDirectory"));

    if (!addDllDirectory) {
        return; // 在老系统上静默退化为仅使用 PATH
    }

    for (const QString& dir : validDirs) {
        const std::wstring wPath = dir.toStdWString();
        DLL_DIRECTORY_COOKIE cookie = addDllDirectory(wPath.c_str());
        if (cookie) {
            qDebug() << "[main] AddDllDirectory succeeded:" << dir;
        } else {
            qWarning() << "[main] AddDllDirectory failed:" << dir
                       << "errorCode:" << GetLastError();
        }
    }
#else
    Q_UNUSED(configureThirdPartyDllSearchPaths);
#endif
}

} // namespace

int main(int argc, char* argv[])
{
    enableDebugConsole();
    ConsoleLogBridge::installMessageHandler();

    qDebug() << "========================================";
    qDebug() << "Medical Pro application startup";
    qDebug() << "========================================";

    // ========================================
    // 阶段1: 创建QApplication & 初始化VTK
    // ========================================
    qDebug() << "[Phase 1] QApplication + VTK initialization";
    qDebug() << "----------------------------------------";

    try {
        // 先选择 Qt 使用的 OpenGL 后端，再进行 VTK 全局初始化，
        // 确保 QVTKOpenGLNativeWidget::defaultFormat() 能看到正确的后端配置。
        qDebug() << "[main] Configuring Qt OpenGL backend...";
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

#if MEDICALPRO_HAVE_QMRMLWIDGET
        qMRMLWidget::preInitializeApplication();
#endif

        qDebug() << "[main] Starting VTK global initialization...";
        if (!VTKGlobalInitializer::instance()->initialize()) {
            QString error = VTKGlobalInitializer::instance()->getLastError();
            qCritical() << "[main] VTK global initialization failed:" << error;
            qCritical() << "[main] Application may be unstable, continuing startup";
        } else {
            qDebug() << "[main] VTK global initialization succeeded";
            qDebug() << "[main]   - OpenGL surface format configured";
            qDebug() << "[main]   - VTK object factory initialized";
        }

        qDebug() << "[main] Creating QApplication instance...";
        SafeApplication app(argc, argv);
        qDebug() << "[main] QApplication instance created";

        // 在任何插件和 CTK 框架初始化之前，先配置好第三方 DLL 搜索路径，
        // 这样像 PointRegistration 这类依赖额外 VTK 模块的插件在加载时
        // 不会因为找不到对应的 VTK *.dll 而直接启动失败。
        configureThirdPartyDllSearchPaths();

        // Setup application information
        qDebug() << "[main] Configuring application metadata...";
        app.setApplicationName("Medical Pro");
        app.setApplicationVersion("1.0");
        app.setOrganizationName("Medical Solutions");
        qDebug() << "[main] Application metadata configured";

        // 🔥🔥🔥 关键修复：全局禁用透明窗口，防止VTK Widget透明问题
        qDebug() << "[main] Applying application attributes...";
        QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, false);
        qDebug() << "[main] Transparent window attributes disabled";

        // Global font: Microsoft YaHei UI 9pt (closer to ANSN Chinese rendering)
        qDebug() << "[main] Applying global font...";
        QFont f("Microsoft YaHei UI", 9);
        app.setFont(f);
        qDebug() << "[main] Global font applied";

        // Setup internationalization
        qDebug() << "[main] Configuring localization...";
        QTranslator translator;
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = "medicalpro_" + QLocale(locale).name();
            if (translator.load(":/i18n/" + baseName)) {
                app.installTranslator(&translator);
                qDebug() << "[main] Translation loaded:" << baseName;
                break;
            }
        }
        qDebug() << "[main] Localization configured";

        AppTheme::applyThreePageTheme(app);

        const QStringList arguments = QCoreApplication::arguments();
        const bool safeMode = arguments.contains(QStringLiteral("--safe-mode"));
        CTKManager::instance()->setSafeMode(safeMode);

#ifdef VTK_FOUND
        const int defaultPoolSize = safeMode ? 2 : 6;
        VTKWidgetPool::instance()->initialize(defaultPoolSize);
#endif

        auto orchestrator = StartupOrchestrator::instance();

        qDebug() << "[Phase 1] Completed\n";

        // ========================================
        // 阶段3: 显示启动界面 / 基础UI准备
        // ========================================
        qDebug() << "[Phase 3] Startup surface preparation";
        qDebug() << "----------------------------------------";

        CTKManager* ctkManager = CTKManager::instance();
        qDebug() << "[Phase 3] Completed\n";

        // ========================================
        // 阶段4: 创建主界面（不做耗时初始化）
        // ========================================
        qDebug() << "[Phase 4] Creating main interface";
        qDebug() << "----------------------------------------";
        auto mainInterface = new MainInterfaceWidget(nullptr);
        qDebug() << "[main] Creating main interface window...";
        mainInterface->setAttribute(Qt::WA_DeleteOnClose, true);
        mainInterface->show();
        mainInterface->raise();
        mainInterface->activateWindow();
        mainInterface->showFullScreen();
        mainInterface->raise();

        qDebug() << "[Phase 4] Completed\n";

        // ========================================
        qDebug() << "[Phase 5] Launching background startup tasks";
        qDebug() << "----------------------------------------";

        // 注册 CTK 框架初始化阶段处理器
#ifdef CTK_PLUGIN_FRAMEWORK
        orchestrator->registerPhaseHandler(StartupPhase::CTKFrameworkInit, [ctkManager](QApplication* app) {
            qDebug() << "[StartupOrchestrator] Running CTK framework initialization...";
            if (!ctkManager->initializeFramework(app)) {
                qCritical() << "[StartupOrchestrator] CTK framework initialization failed";
                return false;
            }
            if (!ctkManager->startFramework()) {
                qCritical() << "[StartupOrchestrator] CTK framework startup failed";
                return false;
            }
            qDebug() << "[StartupOrchestrator] CTK framework initialization completed";

            return true;
        });

        // 注册插件安装阶段处理器
        orchestrator->registerPhaseHandler(StartupPhase::PluginInstallation, [ctkManager](QApplication*) {
            qDebug() << "[StartupOrchestrator] Running plugin installation...";

            // 🔥 关键修复：在安装插件前先加载插件加载策略配置
            QString configPath = QCoreApplication::applicationDirPath() + "/config/plugin_load_policy.json";
            if (QFile::exists(configPath)) {
                qDebug() << "[StartupOrchestrator] Loading plugin policy configuration:" << configPath;
                ctkManager->loadPluginPolicy(configPath);
            } else {
                qWarning() << "[StartupOrchestrator] Plugin policy configuration file not found:" << configPath;
            }

            QString pluginsPath = QCoreApplication::applicationDirPath() + "/plugins";
            int installedCount = ctkManager->installPluginsFromDirectory(pluginsPath);
            qDebug() << "[StartupOrchestrator] Installed plugin count:" << installedCount;
            return true; // 插件安装失败不阻止启动
        });

        // 注册关键插件启动阶段处理器
        // 关键插件必须同步初始化，确保服务立即可用（如登录服务）
        orchestrator->registerPhaseHandler(StartupPhase::CriticalPluginStart, [ctkManager](QApplication*) {
            qDebug() << "[StartupOrchestrator] Starting critical plugin activation (synchronous)...";

            // 🔥 使用配置驱动获取关键插件列表
            PluginLoadPolicy* policy = PluginLoadPolicy::instance();
            QStringList criticalPlugins;
            if (policy->hasValidConfig()) {
                criticalPlugins = policy->getCriticalPlugins();
                qDebug() << "[StartupOrchestrator] Critical plugins from configuration:" << criticalPlugins;
            }
            // 如果配置为空，使用默认的关键插件
            if (criticalPlugins.isEmpty()) {
                criticalPlugins.clear();
                criticalPlugins << "UserManagement";
                qDebug() << "[StartupOrchestrator] Falling back to default critical plugins:" << criticalPlugins;
            }

            bool success = true;
            for (const QString& pluginName : criticalPlugins) {
                qDebug() << "[StartupOrchestrator] Starting critical plugin:" << pluginName;
                if (!ctkManager->startPlugin(pluginName)) {
                    qCritical() << "[StartupOrchestrator] Critical plugin start failed:" << pluginName;
                    success = false;
                    break;
                }

                // 等待服务注册完成（给异步初始化一点时间）
                QCoreApplication::processEvents();
                QThread::msleep(50);
            }

            // 验证关键服务是否可用
            if (success) {
                qDebug() << "[StartupOrchestrator] Verifying critical service availability...";
                // 直接使用 CTKManager 检查服务
                auto* userService = ctkManager->getService<UserManagementService>();
                if (!userService) {
                    qWarning() << "[StartupOrchestrator] UserManagementService not ready yet; waiting...";

                    // 最多等待 2 秒
                    for (int i = 0; i < 20; ++i) {
                        QCoreApplication::processEvents();
                        QThread::msleep(100);
                        userService = ctkManager->getService<UserManagementService>();
                        if (userService) {
                            qDebug() << "[StartupOrchestrator] UserManagementService is ready";
                            break;
                        }
                    }
                }
            }

            qDebug() << "[StartupOrchestrator] Critical plugin activation completed";
            return success;
        });

        // 注册延迟插件启动阶段处理器
        orchestrator->registerPhaseHandler(StartupPhase::DeferredPluginStart, [ctkManager](QApplication*) {
            qDebug() << "[StartupOrchestrator] Running deferred plugin activation...";

            // 🔥 使用配置驱动获取需要启动的插件列表
            PluginLoadPolicy* policy = PluginLoadPolicy::instance();
            QStringList deferredPlugins;
            QStringList immediatePlugins;

            if (policy->hasValidConfig()) {
                // 获取 deferred 策略的插件
                deferredPlugins = policy->getPluginsByPolicy(LoadPolicy::Deferred);
                // 获取 immediate 策略的插件（非关键的）
                immediatePlugins = policy->getPluginsByPolicy(LoadPolicy::Immediate);

                // 过滤掉已标记为 critical 的插件（它们在上一阶段已启动）
                QStringList criticalPlugins = policy->getCriticalPlugins();
                for (const QString& critical : criticalPlugins) {
                    immediatePlugins.removeAll(critical);
                }

                qDebug() << "[StartupOrchestrator] Policy-driven immediate plugins:" << immediatePlugins;
                qDebug() << "[StartupOrchestrator] Policy-driven deferred plugins:" << deferredPlugins;
            }

            // 合并列表：先启动 immediate，再启动 deferred
            QStringList allPlugins = immediatePlugins + deferredPlugins;

            // 如果配置为空，使用默认的插件列表
            if (allPlugins.isEmpty()) {
                allPlugins.clear();
                allPlugins << "DicomViewer"
                           << "InstrumentManagement"
                           << "FourViewDisplay"
                           << "OpticalTracking"
                           << "Registration2D3D";
                qDebug() << "[StartupOrchestrator] Falling back to default plugin list:" << allPlugins;
            }

            for (const QString& pluginName : allPlugins) {
                qDebug() << "[StartupOrchestrator] Starting plugin:" << pluginName;
                ctkManager->startPlugin(pluginName); // 失败不影响启动
            }
            qDebug() << "[StartupOrchestrator] Deferred plugin activation completed";
            return true;
        });

        // 注册服务预热阶段处理器
        // 注意：这个阶段在后台线程执行，需要将某些操作调度到主线程
        orchestrator->registerPhaseHandler(StartupPhase::ServiceWarmup, [ctkManager](QApplication* app) {
            qDebug() << "[StartupOrchestrator] Running service warmup...";

            // 使用信号量同步，确保主线程操作完成后再返回
            QSemaphore semaphore(0);
            bool initSuccess = true;

            // 将Python初始化调度到主线程执行
            QMetaObject::invokeMethod(app, [&semaphore, &initSuccess, ctkManager]() {
                qDebug() << "[ServiceWarmup] Initializing services on the main thread...";

                // 1. 预热Registration2D3D的Python环境
                auto reg2D3DService = ctkManager->getService<Registration2D3DService>();
                if (reg2D3DService) {
                    qDebug() << "[ServiceWarmup] Warming up Registration2D3D Python environment...";
                    bool pythonDeferred = reg2D3DService->getConfiguration("pythonInitDeferred", false).toBool();
                    if (pythonDeferred && !reg2D3DService->isPythonInitialized()) {
                        QString pythonHome = reg2D3DService->getConfiguration("pythonHome", QString()).toString();
                        QString scriptsPath = reg2D3DService->getConfiguration("scriptsPath", QString()).toString();
                        if (!scriptsPath.isEmpty()) {
                            initSuccess = reg2D3DService->initializePythonEnvironment(pythonHome, scriptsPath);
                            if (!initSuccess) {
                                qWarning() << "[ServiceWarmup] Python environment initialization failed:"
                                           << reg2D3DService->getLastError();
                            }
                        }
                    }
                    qDebug() << "[ServiceWarmup] Registration2D3D Python environment warmup completed";
                }

                // 2. 预热FourViewDisplay服务
                auto fourViewService = ctkManager->getService<FourViewDisplayService>();
                if (fourViewService) {
                    qDebug() << "[ServiceWarmup] Warming up FourViewDisplay service...";
                    qDebug() << "[ServiceWarmup] FourViewDisplay service warmup completed";
                }

                qDebug() << "[ServiceWarmup] Main-thread service initialization completed";
                semaphore.release();  // 释放信号量，让后台线程继续
            }, Qt::QueuedConnection);

            // 等待主线程完成初始化（最多等待30秒）
            if (!semaphore.tryAcquire(1, 30000)) {
                qWarning() << "[ServiceWarmup] Service warmup timed out";
                return false;
            }

            qDebug() << "[StartupOrchestrator] Service warmup completed";
            return true;  // 即使Python初始化失败也不阻止启动
        });

#else
        qDebug() << "[Phase 5] CTK plugin framework disabled, skipping CTK startup handlers";
#endif

        QObject::connect(orchestrator, &StartupOrchestrator::startupCompleted,
                             &app, [mainInterface, safeMode](bool success) {
                                 if (!success) {
                                     qWarning() << "[Startup] Background startup reported failures; continuing runtime";
                                     qWarning() << StartupOrchestrator::instance()->getDiagnosticReport();
                                 } else if (safeMode) {
                                     QMessageBox::information(mainInterface,
                                                              QObject::tr("安全模式"),
                                                              QObject::tr("应用正在安全模式下运行，部分可选插件已被跳过。\n\n诊断摘要：\n%1")
                                                                  .arg(StartupOrchestrator::instance()->getDiagnosticReport()));
                                 }
                             });

            QTimer::singleShot(0, orchestrator, [orchestrator, app = &app]() {
                orchestrator->start(app);
            });

            QObject::connect(mainInterface, &MainInterfaceWidget::exitRequested, &app, [mainInterface, app = &app]() mutable {
                mainInterface->close();
                app->quit();
            });
            QObject::connect(mainInterface, &MainInterfaceWidget::logoutRequested, &app, []() {
                qDebug() << "[main] Handling logout request and returning to Welcome page";
            });
            qDebug() << "========================================";
            qDebug() << "Startup flow completed, entering event loop";
            qDebug() << "========================================\n";

            int result = app.exec();

            if (mainInterface) {
                delete mainInterface;
                mainInterface = nullptr;
            }

            return result;

        } catch (const std::exception& e) {
            qCritical() << "[main] Application initialization exception:" << e.what();
            QMessageBox::critical(nullptr, "启动失败", 
                QString("应用程序初始化失败：\n\n%1\n\n应用程序将退出。").arg(e.what()));
            return 1;
        } catch (...) {
            qCritical() << "[main] Application initialization hit an unknown exception";
            QMessageBox::critical(nullptr, "启动失败",
                "应用程序初始化失败（未知异常）。\n\n应用程序将退出。");
            return 1;
        }

    return 0;
}
