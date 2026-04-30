#include "UI/MainInterfaceWidget.h"
#include "UI/MainInterfaceFactory.h"
#include "UI/AppTheme.h"

#include "Framework/ConsoleLogBridge.h"
#include "Framework/Platform/Bootstrap/StartupBootstrapController.h"
#include "Framework/Platform/Bootstrap/startup_ui_coordinator.h"
#include "Framework/Platform/Bootstrap/startup_phase_registrar.h"
#include "Framework/Platform/Bootstrap/platform_built_in_module_bootstrap.h"
#include "Framework/Platform/Kernel/PlatformDescriptorLoader.h"
#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"
#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"
#include "Framework/Platform/Kernel/PlatformOnDemandActivationService.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"
#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"
#include "Framework/Platform/Kernel/PlatformWarmupCoordinator.h"
#include "Framework/Platform/Kernel/platform_plugin_host.h"
#include "Framework/Platform/Kernel/platform_runtime_host_adapter.h"
#include "Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h"
#include "Framework/Registration/RegistrationService.h"
#include "Framework/StartupOrchestrator.h"
#include "Framework/VTKGlobalInitializer.h"
#include "Framework/VTKWidgetPool.h"
#include "Plugins/OpticalTracking/OpticalTrackingService.h"

#include <QApplication>
#include <QEvent>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QHash>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QTranslator>
#include <atomic>
#include <exception>
#include <cstdio>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

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

struct StartupRuntimeContext
{
    struct PluginIdentity
    {
        QString pluginId;
        QString symbolicName;
    };

    StartupRuntimeContext(
        const PlatformRuntimeConfig& config,
        PlatformStartupCoordinator::StartPluginFn startPluginFn,
        const QHash<QString, QString>& platformPluginIdToSymbolicName,
        const QVector<PlatformPluginDescriptor>& descriptors)
        : runtimeConfig(config)
        , startupCoordinator(
              config.runtimeMode,
              std::move(startPluginFn),
              platformPluginIdToSymbolicName,
              &lifecycleRecorder)
    {
        for (const auto& descriptor : descriptors) {
            descriptorsByPluginId.insert(descriptor.id, descriptor);
            const auto symbolicName = descriptor.runtime.symbolicName.trimmed();
            if (!symbolicName.isEmpty()) {
                pluginIdBySymbolicName.insert(symbolicName.toLower(), descriptor.id);
            }
        }
    }

    PluginIdentity resolveByPlatformPluginId(const QString& pluginId) const
    {
        PluginIdentity identity;
        identity.pluginId = pluginId.trimmed();
        if (!identity.pluginId.isEmpty() && descriptorsByPluginId.contains(identity.pluginId)) {
            identity.symbolicName = descriptorsByPluginId.value(identity.pluginId).runtime.symbolicName.trimmed();
        }
        return identity;
    }

    PluginIdentity resolveBySymbolicName(const QString& symbolicName) const
    {
        PluginIdentity identity;
        identity.symbolicName = symbolicName.trimmed();
        if (identity.symbolicName.isEmpty()) return identity;

        const auto exactPluginId = pluginIdBySymbolicName.value(identity.symbolicName.toLower()).trimmed();
        if (!exactPluginId.isEmpty()) {
            identity.pluginId = exactPluginId;
            return identity;
        }

        QString normalized = identity.symbolicName;
        if (normalized.startsWith(QStringLiteral("lib"), Qt::CaseInsensitive)) {
            normalized = normalized.mid(3);
            const auto normalizedPluginId = pluginIdBySymbolicName.value(normalized.toLower()).trimmed();
            if (!normalizedPluginId.isEmpty()) {
                identity.pluginId = normalizedPluginId;
                identity.symbolicName = normalized;
            }
        }
        return identity;
    }

    PluginIdentity resolveBySymbolicOrPath(const QString& symbolicName, const QString& pluginPath) const
    {
        auto identity = resolveBySymbolicName(symbolicName);
        if (!identity.pluginId.isEmpty()) return identity;

        const auto fileBaseName = QFileInfo(pluginPath).completeBaseName();
        if (fileBaseName.isEmpty()) return identity;

        auto byFile = resolveBySymbolicName(fileBaseName);
        if (!byFile.pluginId.isEmpty()) return byFile;

        if (byFile.symbolicName.isEmpty()) byFile.symbolicName = fileBaseName;
        return byFile;
    }

    QStringList requiredServices(const QString& pluginId) const
    {
        QStringList services;
        const auto descriptor = descriptorsByPluginId.value(pluginId);
        appendUniqueList(services, descriptor.required.services);
        appendUniqueList(services, descriptor.diagnostics.requiredServices);
        return services;
    }

    QStringList missingRequiredPlugins(
        const QString& pluginId,
        const std::function<bool(const QString&)>& isPluginStartedFn) const
    {
        QStringList missing;
        if (!isPluginStartedFn) return missing;

        const auto descriptor = descriptorsByPluginId.value(pluginId);
        for (const auto& requiredPluginId : descriptor.required.plugins) {
            const auto requiredIdentity = resolveByPlatformPluginId(requiredPluginId);
            if (requiredIdentity.symbolicName.isEmpty()) {
                missing.append(requiredPluginId);
                continue;
            }
            if (!isPluginStartedFn(requiredIdentity.symbolicName)) {
                missing.append(requiredPluginId);
            }
        }
        return missing;
    }

    QStringList missingRequiredCapabilities(
        const QString& pluginId,
        const std::function<bool(const QString&)>& isPluginStartedFn) const
    {
        QStringList missing;
        if (!isPluginStartedFn) return missing;

        const auto descriptor = descriptorsByPluginId.value(pluginId);
        for (const auto& requiredCapability : descriptor.required.capabilities) {
            bool capabilityReady = false;
            for (auto it = descriptorsByPluginId.constBegin(); it != descriptorsByPluginId.constEnd(); ++it) {
                const auto& candidate = it.value();
                if (!candidate.provides.capabilities.contains(requiredCapability)) continue;
                const auto symbolicName = candidate.runtime.symbolicName.trimmed();
                if (symbolicName.isEmpty()) continue;
                if (!isPluginStartedFn(symbolicName)) continue;
                capabilityReady = true;
                break;
            }
            if (!capabilityReady) missing.append(requiredCapability);
        }

        return missing;
    }

    void appendUniqueList(QStringList& target, const QStringList& values) const
    {
        for (const auto& value : values) {
            if (value.isEmpty()) continue;
            if (!target.contains(value)) target.append(value);
        }
    }

    PlatformRuntimeConfig runtimeConfig;
    PlatformManagedPluginPlan managedPlan;
    PlatformLifecycleTraceRecorder lifecycleRecorder;
    PlatformStartupCoordinator startupCoordinator;
    std::unique_ptr<PlatformWarmupCoordinator> warmupCoordinator;
    PlatformStateStore* stateStore = nullptr;
    std::shared_ptr<PlatformOnDemandActivationService> onDemandActivationService;
    std::unique_ptr<LegacyNavigationAdapter> navigationAdapter;
    QHash<QString, PlatformPluginDescriptor> descriptorsByPluginId;
    QHash<QString, QString> pluginIdBySymbolicName;
    std::atomic_bool shutdownRequested{false};
};

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

        // Configure third-party DLL search paths before platform module
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
        auto runtimeHost = std::make_shared<PlatformRuntimeHostAdapter>();
        auto* runtimeHostPort = static_cast<IPlatformRuntimeHostPort*>(runtimeHost.get());
        auto* serviceAccessPort = static_cast<IPlatformServiceAccessPort*>(runtimeHost.get());
        registerBuiltInPlatformModules();

#ifdef VTK_FOUND
        const int defaultPoolSize = safeMode ? 2 : 6;
        VTKWidgetPool::instance()->initialize(defaultPoolSize);
#endif

        const QString runtimeConfigPath =
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/platform_runtime.json"));
        QString runtimeConfigError;
        const auto runtimeConfig = PlatformRuntimeConfig::loadFromFile(runtimeConfigPath, &runtimeConfigError);
        if (!runtimeConfigError.isEmpty()) {
            throw std::runtime_error(
                QStringLiteral("Failed to load platform runtime config: %1").arg(runtimeConfigError).toStdString());
        }

        const QString descriptorDirectoryPath =
            QDir(QCoreApplication::applicationDirPath()).filePath(runtimeConfig.descriptorDirectory);
        QStringList descriptorErrors;
        const auto descriptors = PlatformDescriptorLoader::loadFromDirectory(descriptorDirectoryPath, &descriptorErrors);
        if (!descriptorErrors.isEmpty()) {
            throw std::runtime_error(
                QStringLiteral("Failed to load platform descriptors: %1")
                    .arg(descriptorErrors.join(QStringLiteral("; ")))
                    .toStdString());
        }

        QHash<QString, QString> platformPluginIdToSymbolicName;
        platformPluginIdToSymbolicName.reserve(descriptors.size());
        for (const auto& descriptor : descriptors) {
            const auto symbolicName = descriptor.runtime.symbolicName.trimmed();
            if (symbolicName.isEmpty()) continue;
            platformPluginIdToSymbolicName.insert(descriptor.id, symbolicName);
        }

        const auto isPlatformModuleAvailable = [](const QString& symbolicName) {
            return PlatformPluginHost::sharedInstance().hasActivator(symbolicName);
        };
        const auto activatePlugin = [runtimeHostPort](const QString& pluginName) {
            return runtimeHostPort && runtimeHostPort->activatePlugin(pluginName);
        };
        const auto isPluginStarted = [runtimeHostPort](const QString& pluginName) {
            return runtimeHostPort && runtimeHostPort->isPluginStarted(pluginName);
        };
        const auto missingServices = [runtimeHostPort](const QStringList& requiredServices) {
            return runtimeHostPort
                ? runtimeHostPort->missingServices(requiredServices)
                : requiredServices;
        };

        const QString pluginsPath =
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));
        QString managedPlanError;
        const auto managedPlan = PlatformManagedPluginPlanBuilder::build(
            runtimeConfig,
            descriptors,
            pluginsPath,
            isPlatformModuleAvailable,
            &managedPlanError);
        if (!managedPlanError.isEmpty()) {
            throw std::runtime_error(
                QStringLiteral("Failed to build managed startup plan: %1").arg(managedPlanError).toStdString());
        }

        bool missingPlatformModule = false;
        for (const auto& entry : managedPlan.installEntries) {
            if (entry.requiresBundleInstall) {
                missingPlatformModule = true;
                break;
            }
        }
        if (!missingPlatformModule) {
            for (const auto& descriptor : descriptors) {
                const auto symbolicName = descriptor.runtime.symbolicName.trimmed();
                if (symbolicName.isEmpty()) continue;
                if (!isPlatformModuleAvailable(symbolicName)) {
                    missingPlatformModule = true;
                    break;
                }
            }
        }
        if (missingPlatformModule) {
            throw std::runtime_error(
                QStringLiteral("Platform module coverage is incomplete after runtime host cleanup")
                    .toStdString());
        }

        qDebug() << "[main] Platform runtime mode:" << runtimeModeToString(runtimeConfig.runtimeMode);
        qDebug() << "[main] Platform descriptor directory:" << runtimeConfig.descriptorDirectory;
        qDebug() << "[main] Platform core plugin ids:" << runtimeConfig.corePluginIds;
        qDebug() << "[main] Managed startup plugin ids:" << managedPlan.managedPluginIds;
        qDebug() << "[main] Platform runtime host active:" << true;
        auto orchestrator = StartupOrchestrator::instance();
        orchestrator->setRuntimeMode(runtimeConfig.runtimeMode);
        auto startupContext = std::make_shared<StartupRuntimeContext>(
            runtimeConfig,
            activatePlugin,
            platformPluginIdToSymbolicName,
            descriptors);
        startupContext->managedPlan = managedPlan;
        startupContext->warmupCoordinator = std::make_unique<PlatformWarmupCoordinator>(&startupContext->lifecycleRecorder);

        qDebug() << "[Phase 1] Completed\n";

        // ========================================
        // Phase 3: prepare the startup surface and base UI
        // ========================================
        qDebug() << "[Phase 3] Startup surface preparation";
        qDebug() << "----------------------------------------";

        qDebug() << "[Phase 3] Completed\n";

        // ========================================
        // Phase 4: create the main interface early so the original welcome page
        // stays the only visible entry surface and page switching stays smooth.
        // ========================================
        qDebug() << "[Phase 4] Creating main interface";
        qDebug() << "----------------------------------------";
        QStringList governedPluginIds = managedPlan.managedPluginIds;
        if (!governedPluginIds.contains(QStringLiteral("org.medicalpro.registration_core"))) {
            governedPluginIds.append(QStringLiteral("org.medicalpro.registration_core"));
        }
        if (!governedPluginIds.contains(QStringLiteral("org.medicalpro.optical_tracking"))) {
            governedPluginIds.append(QStringLiteral("org.medicalpro.optical_tracking"));
        }

        PlatformStateStore bootstrapStateStore;
        const auto resetBootstrapStateStore = [&]() {
            bootstrapStateStore.replaceDescriptors(descriptors);
            bootstrapStateStore.setRuntimeMode(runtimeConfig.runtimeMode);
            bootstrapStateStore.setStartupScopePluginIds(managedPlan.managedPluginIds);
            bootstrapStateStore.setGovernedPluginIds(governedPluginIds);
        };

        resetBootstrapStateStore();
        startupContext->stateStore = &bootstrapStateStore;
        const PlatformOnDemandProbeSet onDemandProbes {
            [startupContext](const QString& pluginId) {
                if (!startupContext->stateStore) return PlatformPluginState::Discovered;

                for (const auto& snapshot : startupContext->stateStore->pluginSnapshots()) {
                    if (snapshot.pluginId == pluginId) return snapshot.state;
                }

                return PlatformPluginState::Discovered;
            },
            [missingServices](const QStringList& requiredServices) {
                return missingServices(requiredServices);
            },
            [startupContext, isPluginStarted](const QString& pluginId) {
                return startupContext->missingRequiredPlugins(pluginId, isPluginStarted);
            },
            [startupContext, isPluginStarted](const QString& pluginId) {
                return startupContext->missingRequiredCapabilities(pluginId, isPluginStarted);
            },
            [serviceAccessPort](const QString&, const QStringList& healthChecks) {
                QVector<PlatformHealthCheckResult> results;

                for (const auto& healthCheck : healthChecks) {
                    PlatformHealthCheckResult result;
                    result.name = healthCheck;

                    if (healthCheck == QStringLiteral("service_registered")) {
                        result.passed = true;
                        result.detail = QStringLiteral("Required services were registered");
                    } else if (healthCheck == QStringLiteral("core_binary_accessible")) {
                        result.passed = serviceAccessPort->registrationService() != nullptr;
                        result.detail = result.passed
                            ? QStringLiteral("RegistrationService is available")
                            : QStringLiteral("RegistrationService is not available");
                    } else if (healthCheck == QStringLiteral("tracking_adapter_accessible")) {
                        result.passed = serviceAccessPort->opticalTrackingService() != nullptr;
                        result.detail = result.passed
                            ? QStringLiteral("OpticalTrackingService is available")
                            : QStringLiteral("OpticalTrackingService is not available");
                    } else {
                        result.passed = false;
                        result.detail = QStringLiteral("Unknown health check");
                    }

                    results.append(result);
                }

                return results;
            }
        };
        startupContext->onDemandActivationService = std::make_shared<PlatformOnDemandActivationService>(
            descriptors,
            pluginsPath,
            &startupContext->startupCoordinator,
            nullptr,
            PlatformOnDemandActivationService::InstallPluginFn {},
            onDemandProbes,
            isPlatformModuleAvailable);
        startupContext->navigationAdapter = std::make_unique<LegacyNavigationAdapter>(
            [service = startupContext->onDemandActivationService](const QString& pluginId) {
                return service->ensureReady(pluginId);
            });
        startupContext->onDemandActivationService->setStateStore(startupContext->stateStore);

        auto bootstrapController = std::make_unique<StartupBootstrapController>();
        QPointer<MainInterfaceWidget> mainInterface;

        const auto publishBootStage = [bootstrapControllerPtr = bootstrapController.get()](
                                          const QString& stageLabel,
                                          const QString& statusText) {
            QMetaObject::invokeMethod(
                bootstrapControllerPtr,
                [bootstrapControllerPtr, stageLabel, statusText]() {
                    bootstrapControllerPtr->updateBootStage(stageLabel, statusText);
                },
                Qt::BlockingQueuedConnection);
        };

        const auto publishReady = [bootstrapControllerPtr = bootstrapController.get()]() {
            QMetaObject::invokeMethod(
                bootstrapControllerPtr,
                [bootstrapControllerPtr]() {
                    bootstrapControllerPtr->markReady();
                    qDebug() << "[Startup] welcome entry enabled";
                },
                Qt::BlockingQueuedConnection);
        };

        const auto publishFailure = [bootstrapControllerPtr = bootstrapController.get()](const QString& failureReason) {
            QMetaObject::invokeMethod(
                bootstrapControllerPtr,
                [bootstrapControllerPtr, failureReason]() {
                    bootstrapControllerPtr->markFailed(
                        failureReason,
                        QStringList{QStringLiteral("Retry startup or inspect diagnostics.")});
                },
                Qt::BlockingQueuedConnection);
        };

        auto mainInterfaceOwner = createMainInterface(
            startupContext->navigationAdapter.get(),
            &bootstrapStateStore,
            nullptr);
        mainInterface = mainInterfaceOwner.release();
        mainInterface->setAttribute(Qt::WA_DeleteOnClose, true);
        startupContext->stateStore = mainInterface->platformStateStore();
        startupContext->onDemandActivationService->setStateStore(startupContext->stateStore);

        QObject::connect(mainInterface, &MainInterfaceWidget::exitRequested, &app, [mainInterface, startupContext, app = &app]() mutable {
            startupContext->shutdownRequested.store(true);
            if (mainInterface) {
                mainInterface->close();
            }
            app->quit();
        });
        StartupUiCoordinator startupUiCoordinator(bootstrapController.get(), &mainInterface, safeMode);

        bootstrapController->beginBoot(
            QStringLiteral("Main interface welcome shown"),
            QStringLiteral("系统初始化中"));
        mainInterface->show();
        mainInterface->raise();
        mainInterface->activateWindow();
        mainInterface->showFullScreen();
        mainInterface->raise();
        qDebug() << "[MainInterfaceWidget] welcome shown";

        qDebug() << "[Phase 4] Completed\n";

        // ========================================
        qDebug() << "[Phase 5] Launching background startup tasks";
        qDebug() << "----------------------------------------";
        orchestrator->setLifecycleRecorder(&startupContext->lifecycleRecorder);
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [startupContext, runtimeHostPort]() {
            startupContext->shutdownRequested.store(true);
            if (runtimeHostPort) runtimeHostPort->stop();
        });

        const auto applyPluginState = [startupContext](const StartupRuntimeContext::PluginIdentity& identity, PlatformPluginState state) {
            if (!startupContext->stateStore) return;
            const auto pluginId = identity.pluginId.trimmed();
            if (pluginId.isEmpty()) return;
            if (!startupContext->descriptorsByPluginId.contains(pluginId)) return;
            startupContext->stateStore->setPluginState(pluginId, state);
        };

        // Register the runtime host initialization handler
        orchestrator->registerPhaseHandler(
            StartupPhase::MainUICreation,
            [publishBootStage](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
                publishBootStage(
                    QStringLiteral("Main interface deferred"),
                    QStringLiteral("欢迎页已显示，主界面将在进入系统后创建"));
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("Main interface deferred until enter"),
                    QStringLiteral("main_interface_deferred"));
            });

        const StartupOrchestrator::PhaseHandler platformRuntimeInitHandler =
            [runtimeHostPort, startupContext, publishBootStage, publishFailure](QApplication* app) -> StartupOrchestrator::PhaseExecutionResult {
            publishBootStage(
                QStringLiteral("Platform runtime initialization"),
                QStringLiteral("正在初始化插件框架"));
            if (!startupContext->startupCoordinator.shouldInitializeFramework()) {
                qDebug() << "[StartupOrchestrator] Skipping platform runtime initialization in observe_only mode";
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("Platform runtime initialization skipped in observe_only mode"),
                    QStringLiteral("observe_only_mode"));
            }

            qDebug() << "[StartupOrchestrator] Running platform runtime initialization...";
            if (!runtimeHostPort->initialize(app)) {
                qCritical() << "[StartupOrchestrator] Platform runtime initialization failed";
                publishFailure(QStringLiteral("Platform runtime initialization failed"));
                return false;
            }
            if (!runtimeHostPort->start()) {
                qCritical() << "[StartupOrchestrator] Platform runtime startup failed";
                publishFailure(QStringLiteral("Platform runtime startup failed"));
                return false;
            }
            qDebug() << "[StartupOrchestrator] Platform runtime initialization completed";

            return true;
        };

        const StartupOrchestrator::PhaseHandler pluginInstallationHandler =
            [startupContext, publishBootStage, publishFailure](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
            publishBootStage(
                QStringLiteral("Plugin installation"),
                QStringLiteral("正在安装平台插件"));
            if (!startupContext->startupCoordinator.shouldInstallPlugins()) {
                qDebug() << "[StartupOrchestrator] Skipping plugin installation in observe_only mode";
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("Plugin installation skipped in observe_only mode"));
            }

            qDebug() << "[StartupOrchestrator] Running managed plugin installation...";
            const bool installed = startupContext->startupCoordinator.installManagedPlugins(
                startupContext->managedPlan,
                [](const PlatformManagedPluginPlanEntry&) {
                    return true;
                });
            qDebug() << "[StartupOrchestrator] Managed plugin installation completed";
            if (!installed) {
                publishFailure(QStringLiteral("Managed plugin installation failed"));
            }
            return installed;
        };

        const StartupOrchestrator::PhaseHandler criticalPluginStartHandler =
            [startupContext, applyPluginState, publishBootStage, publishFailure, publishReady, missingServices, isPluginStarted](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
                publishBootStage(
                    QStringLiteral("Critical plugin activation"),
                    QStringLiteral("正在准备主流程插件"));
                if (!startupContext->startupCoordinator.shouldStartCorePlugins()) {
                    qDebug() << "[StartupOrchestrator] Skipping core plugin startup in observe_only mode";
                    publishReady();
                    return StartupOrchestrator::PhaseExecutionResult::skipped(
                        QStringLiteral("Core plugin activation skipped in observe_only mode"));
                }

                if (startupContext->managedPlan.installEntries.isEmpty()) {
                    qDebug() << "[StartupOrchestrator] No managed core plugins configured for startup";
                    publishReady();
                    return true;
                }

                qDebug() << "[StartupOrchestrator] Starting critical plugin activation (synchronous)...";

                for (const auto& entry : startupContext->managedPlan.installEntries) {
                    qDebug() << "[StartupOrchestrator] Starting managed core plugin:" << entry.pluginId;
                    if (!startupContext->startupCoordinator.startCorePlugin(entry.pluginId)) {
                        applyPluginState(startupContext->resolveByPlatformPluginId(entry.pluginId), PlatformPluginState::Failed);
                        qCritical() << "[StartupOrchestrator] Critical plugin start failed:" << entry.pluginId;
                        publishFailure(QStringLiteral("Critical plugin activation failed"));
                        return false;
                    }

                    const auto identity = startupContext->resolveByPlatformPluginId(entry.pluginId);
                    const auto outcome = startupContext->startupCoordinator.waitForServiceReady(
                        entry,
                        {
                            [missingServices](const QStringList& requiredServices) {
                                return missingServices(requiredServices);
                            },
                            [startupContext, isPluginStarted](const QString& pluginId) {
                                return startupContext->missingRequiredPlugins(pluginId, isPluginStarted);
                            },
                            [startupContext, isPluginStarted](const QString& pluginId) {
                                return startupContext->missingRequiredCapabilities(pluginId, isPluginStarted);
                            }
                        });

                    applyPluginState(identity, outcome.finalState);
                    startupContext->lifecycleRecorder.recordPluginStepStarted(
                        identity.pluginId,
                        identity.symbolicName,
                        PlatformLifecycleStep::ServiceReady,
                        true);
                    startupContext->lifecycleRecorder.recordPluginStepFinished(
                        identity.pluginId,
                        identity.symbolicName,
                        PlatformLifecycleStep::ServiceReady,
                        outcome.success ? PlatformLifecycleResult::Succeeded : PlatformLifecycleResult::Timeout,
                        outcome.reasonCode,
                        outcome.detail);
                    if (!outcome.success) {
                        qCritical() << "[StartupOrchestrator] Service ready timeout:" << entry.pluginId
                                    << outcome.missingServices << outcome.missingPlugins << outcome.missingCapabilities;
                        publishFailure(outcome.detail);
                        return false;
                    }
                }

                qDebug() << "[StartupOrchestrator] Critical plugin activation completed";
                publishReady();
                return true;
        };

        const StartupOrchestrator::PhaseHandler deferredPluginStartHandler =
            [startupContext, publishBootStage](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
            publishBootStage(
                QStringLiteral("Deferred plugin activation"),
                QStringLiteral("主流程已就绪，后台继续启动可选插件"));
            if (!startupContext->startupCoordinator.shouldStartDeferredPlugins()) {
                qDebug() << "[StartupOrchestrator] Skipping deferred plugin startup in current runtime mode";
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("Deferred plugin activation skipped in current runtime mode"));
            }

            qDebug() << "[StartupOrchestrator] Running deferred plugin activation...";

            qDebug() << "[StartupOrchestrator] Deferred plugin activation has no additional runtime work";
            return StartupOrchestrator::PhaseExecutionResult::skipped(
                QStringLiteral("Deferred plugin activation has no additional runtime work"),
                QStringLiteral("no_deferred_runtime_work"));
        };

        const StartupOrchestrator::PhaseHandler serviceWarmupHandler =
            [startupContext, publishBootStage](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
            publishBootStage(
                QStringLiteral("Service warmup"),
                QStringLiteral("主流程已就绪，后台继续预热服务"));
            if (startupContext->shutdownRequested.load()) {
                qDebug() << "[StartupOrchestrator] Aborting service warmup because shutdown was requested";
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("Service warmup aborted because application shutdown was requested"),
                    QStringLiteral("aborted_by_shutdown"));
            }

            const auto outcome = startupContext->warmupCoordinator->run(
                startupContext->managedPlan,
                startupContext->runtimeConfig.runtimeMode,
                {});

            if (!outcome.success) {
                return StartupOrchestrator::PhaseExecutionResult {
                    false,
                    outcome.result,
                    outcome.reasonCode,
                    outcome.detail
                };
            }

            if (outcome.result == PlatformLifecycleResult::Skipped) {
                return StartupOrchestrator::PhaseExecutionResult::skipped(outcome.detail, outcome.reasonCode);
            }

            if (outcome.result == PlatformLifecycleResult::Degraded) {
                return StartupOrchestrator::PhaseExecutionResult {
                    true,
                    outcome.result,
                    outcome.reasonCode,
                    outcome.detail
                };
            }

            return true;
        };

        const StartupPhaseRegistrar startupPhaseRegistrar;
        const StartupPhaseRegistrar::RuntimePhaseHandlers runtimePhaseHandlers(
            StartupPhaseRegistrar::PlatformRuntimeInitPhaseHandler { platformRuntimeInitHandler },
            StartupPhaseRegistrar::PluginInstallationPhaseHandler { pluginInstallationHandler },
            StartupPhaseRegistrar::CriticalPluginStartPhaseHandler { criticalPluginStartHandler },
            StartupPhaseRegistrar::DeferredPluginStartPhaseHandler { deferredPluginStartHandler },
            StartupPhaseRegistrar::ServiceWarmupPhaseHandler { serviceWarmupHandler });
        startupPhaseRegistrar.registerRuntimePhases(orchestrator, runtimePhaseHandlers);

        startupUiCoordinator.bindToStartupCompletion(&app);

            QTimer::singleShot(0, orchestrator, [orchestrator, app = &app]() {
                orchestrator->start(app);
            });

            qDebug() << "========================================";
            qDebug() << "Startup flow completed, entering event loop";
            qDebug() << "========================================\n";

            int result = app.exec();
            orchestrator->waitForCompletion();
            orchestrator->clearPhaseHandlers();
            orchestrator->setLifecycleRecorder(nullptr);
            startupContext.reset();

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

#include "main.moc"
