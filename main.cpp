#include "UI/MainInterfaceWidget.h"
#include "UI/AppTheme.h"

#include "Framework/CTKManager.h"
#include "Framework/ConsoleLogBridge.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"
#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"
#include "Framework/StartupOrchestrator.h"
#include "Framework/VTKGlobalInitializer.h"
#include "Framework/VTKWidgetPool.h"
#ifdef CTK_PLUGIN_FRAMEWORK
#include "Plugins/Registration2D3D/Registration2D3DService.h"
#include "Plugins/FourViewDisplay/FourViewDisplayService.h"
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
#include <QTimer>
#include <QTranslator>
#include <exception>
#include <cstdio>
#include <stdexcept>

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
            QStringLiteral("Qt Event Exception"),
            QStringLiteral("A Qt event handler threw an exception.\n\n%1")
                .arg(detail));

        m_handlingException = false;
    }

    bool m_handlingException = false;
};

namespace
{

// Configure third-party DLL search paths early so VTK-dependent plugins do not fail with Win32 error 126.
// This keeps optional modules loadable before the CTK framework starts.
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
    // Project root directory: go up two levels
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

    // 1) Extend PATH for legacy Windows DLL search behavior
    const QString currentPath = qEnvironmentVariable("PATH");
    const QString newPath = validDirs.join(";") + ";" + currentPath;
    qputenv("PATH", newPath.toUtf8());
    qDebug() << "[main] PATH extended with third-party DLL directories:" << validDirs;

    // 2) Also register AddDllDirectory on supported systems
    //    so SetDefaultDllDirectories() keeps these folders visible
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) {
        return;
    }

    typedef DLL_DIRECTORY_COOKIE (WINAPI *AddDllDirectoryFunc)(PCWSTR);
    AddDllDirectoryFunc addDllDirectory =
        reinterpret_cast<AddDllDirectoryFunc>(GetProcAddress(kernel32, "AddDllDirectory"));

    if (!addDllDirectory) {
        return; // Fall back silently to PATH-only behavior on older systems
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

static QString runtimeModeToString(PlatformRuntimeMode runtimeMode)
{
    switch (runtimeMode) {
    case PlatformRuntimeMode::ObserveOnly:
        return QStringLiteral("observe_only");
    case PlatformRuntimeMode::FacadeMode:
        return QStringLiteral("facade_mode");
    case PlatformRuntimeMode::OrchestrateCore:
        return QStringLiteral("orchestrate_core");
    }

    return QStringLiteral("unknown");
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
    // Phase 1: create QApplication and initialize VTK
    // ========================================
    qDebug() << "[Phase 1] QApplication + VTK initialization";
    qDebug() << "----------------------------------------";

    try {
        // Select the Qt OpenGL backend before VTK global initialization
        // so QVTKOpenGLNativeWidget sees the correct surface format.
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

        // Configure third-party DLL search paths before any plugin or CTK
        // initialization so VTK-dependent plugins can resolve their runtime
        // DLLs without failing during startup.
        configureThirdPartyDllSearchPaths();

        // Setup application information
        qDebug() << "[main] Configuring application metadata...";
        app.setApplicationName("Medical Pro");
        app.setApplicationVersion("1.0");
        app.setOrganizationName("Medical Solutions");
        qDebug() << "[main] Application metadata configured";

        // Key fix: disable transparent native sibling creation for VTK widgets
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

        CTKManager* ctkManager = CTKManager::instance();
        const QString runtimeConfigPath =
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/platform_runtime.json"));
        QString runtimeConfigError;
        const auto runtimeConfig = PlatformRuntimeConfig::loadFromFile(runtimeConfigPath, &runtimeConfigError);
        if (!runtimeConfigError.isEmpty()) {
            throw std::runtime_error(
                QStringLiteral("Failed to load platform runtime config: %1").arg(runtimeConfigError).toStdString());
        }

        QStringList coreCtkPluginNames;
        if (runtimeConfig.runtimeMode != PlatformRuntimeMode::ObserveOnly) {
            const QString descriptorDirectoryPath =
                QDir(QCoreApplication::applicationDirPath()).filePath(runtimeConfig.descriptorDirectory);
            coreCtkPluginNames = runtimeConfig.resolveCoreCtkPluginNames(descriptorDirectoryPath, &runtimeConfigError);
            if (!runtimeConfigError.isEmpty()) {
                throw std::runtime_error(
                    QStringLiteral("Failed to resolve core CTK plugin names: %1").arg(runtimeConfigError).toStdString());
            }
        }

        qDebug() << "[main] Platform runtime mode:" << runtimeModeToString(runtimeConfig.runtimeMode);
        qDebug() << "[main] Platform descriptor directory:" << runtimeConfig.descriptorDirectory;
        qDebug() << "[main] Platform core plugin ids:" << runtimeConfig.corePluginIds;
        qDebug() << "[main] Platform core CTK plugin names:" << coreCtkPluginNames;

        PlatformStartupCoordinator startupCoordinator(
            runtimeConfig.runtimeMode,
            [ctkManager](const QString& pluginName) {
                return ctkManager->startPlugin(pluginName);
            });
        auto orchestrator = StartupOrchestrator::instance();

        qDebug() << "[Phase 1] Completed\n";

        // ========================================
        // Phase 3: prepare the startup surface and base UI
        // ========================================
        qDebug() << "[Phase 3] Startup surface preparation";
        qDebug() << "----------------------------------------";

        qDebug() << "[Phase 3] Completed\n";

        // ========================================
        // Phase 4: create the main interface without expensive work
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

        // Register the CTK framework initialization handler
#ifdef CTK_PLUGIN_FRAMEWORK
        orchestrator->registerPhaseHandler(StartupPhase::CTKFrameworkInit, [ctkManager, &startupCoordinator](QApplication* app) {
            if (!startupCoordinator.shouldInitializeFramework()) {
                qDebug() << "[StartupOrchestrator] Skipping CTK framework initialization in observe_only mode";
                return true;
            }

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

        // Register the plugin installation handler
        orchestrator->registerPhaseHandler(StartupPhase::PluginInstallation, [ctkManager, &startupCoordinator](QApplication*) {
            if (!startupCoordinator.shouldInstallPlugins()) {
                qDebug() << "[StartupOrchestrator] Skipping plugin installation in observe_only mode";
                return true;
            }

            qDebug() << "[StartupOrchestrator] Running plugin installation...";

            // Load plugin policy configuration before installing bundles
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
            return true; // Installation failures should not block app startup
        });

        orchestrator->registerPhaseHandler(
            StartupPhase::CriticalPluginStart,
            [&startupCoordinator, coreCtkPluginNames](QApplication*) {
                if (!startupCoordinator.shouldStartCorePlugins()) {
                    qDebug() << "[StartupOrchestrator] Skipping core plugin startup in observe_only mode";
                    return true;
                }

                if (coreCtkPluginNames.isEmpty()) {
                    qDebug() << "[StartupOrchestrator] No core CTK plugins configured for startup";
                    return true;
                }

                qDebug() << "[StartupOrchestrator] Starting critical plugin activation (synchronous)...";

                for (const QString& pluginName : coreCtkPluginNames) {
                    qDebug() << "[StartupOrchestrator] Starting core plugin from platform config:" << pluginName;
                    if (!startupCoordinator.ensureReady(pluginName)) {
                        qCritical() << "[StartupOrchestrator] Critical plugin start failed:" << pluginName;
                        return false;
                    }
                }

                qDebug() << "[StartupOrchestrator] Critical plugin activation completed";
                return true;
        });

        orchestrator->registerPhaseHandler(StartupPhase::DeferredPluginStart, [ctkManager, &startupCoordinator](QApplication*) {
            if (!startupCoordinator.shouldStartDeferredPlugins()) {
                qDebug() << "[StartupOrchestrator] Skipping deferred plugin startup in current runtime mode";
                return true;
            }

            qDebug() << "[StartupOrchestrator] Running deferred plugin activation...";

            const bool success = ctkManager->startDeferredPlugins(false);
            qDebug() << "[StartupOrchestrator] Deferred plugin activation completed";
            return success;
        });

        // Register the service warmup handler
        // This phase runs off the UI thread and dispatches UI-bound work back
        orchestrator->registerPhaseHandler(StartupPhase::ServiceWarmup, [ctkManager, &startupCoordinator](QApplication* app) {
            if (!startupCoordinator.shouldWarmupServices()) {
                qDebug() << "[StartupOrchestrator] Skipping service warmup in current runtime mode";
                return true;
            }

            qDebug() << "[StartupOrchestrator] Running service warmup...";

            // Use a semaphore so background startup waits for the main-thread work
            QSemaphore semaphore(0);
            bool initSuccess = true;

            // Dispatch Python initialization onto the main thread
            QMetaObject::invokeMethod(app, [&semaphore, &initSuccess, ctkManager]() {
                qDebug() << "[ServiceWarmup] Initializing services on the main thread...";

                // 1. Warm up the Registration2D3D Python environment
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

                // 2. Warm up the FourViewDisplay service
                auto fourViewService = ctkManager->getService<FourViewDisplayService>();
                if (fourViewService) {
                    qDebug() << "[ServiceWarmup] Warming up FourViewDisplay service...";
                    qDebug() << "[ServiceWarmup] FourViewDisplay service warmup completed";
                }

                qDebug() << "[ServiceWarmup] Main-thread service initialization completed";
                semaphore.release();  // Release the background thread after UI warmup
            }, Qt::QueuedConnection);

            // Wait up to 30 seconds for main-thread initialization
            if (!semaphore.tryAcquire(1, 30000)) {
                qWarning() << "[ServiceWarmup] Service warmup timed out";
                return false;
            }

            qDebug() << "[StartupOrchestrator] Service warmup completed";
            return true;  // Python warmup failures should not block app startup
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
            QMessageBox::critical(
                nullptr,
                QStringLiteral("Application Startup Error"),
                QStringLiteral("Application initialization failed.\n\n%1\n\nPlease check the startup log for details.")
                    .arg(QString::fromUtf8(e.what())));
            return 1;
        } catch (...) {
            qCritical() << "[main] Application initialization hit an unknown exception";
            QMessageBox::critical(
                nullptr,
                QStringLiteral("Application Startup Error"),
                QStringLiteral("Application initialization hit an unknown exception.\n\nPlease check the startup log for details."));
            return 1;
        }

    return 0;
}
