#include "UI/MainInterfaceWidget.h"
#include "UI/AppTheme.h"

#include "Framework/CTKManager.h"
#include "Framework/ConsoleLogBridge.h"
#include "Framework/Platform/Kernel/PlatformDescriptorLoader.h"
#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"
#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"
#include "Framework/Platform/Kernel/PlatformOnDemandActivationService.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"
#include "Framework/Platform/Kernel/PlatformStartupCoordinator.h"
#include "Framework/Platform/Kernel/PlatformWarmupCoordinator.h"
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
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QTranslator>
#include <atomic>
#include <exception>
#include <cstdio>
#include <memory>
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

struct StartupRuntimeContext
{
    struct PluginIdentity
    {
        QString pluginId;
        QString ctkSymbolicName;
    };

    StartupRuntimeContext(
        const PlatformRuntimeConfig& config,
        CTKManager* ctkManager,
        const QHash<QString, QString>& platformPluginIdToCtkSymbolicName,
        const QVector<PlatformPluginDescriptor>& descriptors)
        : runtimeConfig(config)
        , startupCoordinator(
              config.runtimeMode,
              [ctkManager](const QString& pluginName) {
                  return ctkManager->startPlugin(pluginName);
              },
              platformPluginIdToCtkSymbolicName,
              &lifecycleRecorder)
    {
        for (const auto& descriptor : descriptors) {
            descriptorsByPluginId.insert(descriptor.id, descriptor);
            const auto ctkSymbolicName = descriptor.runtime.ctkSymbolicName.trimmed();
            if (!ctkSymbolicName.isEmpty()) {
                pluginIdByCtkSymbolicName.insert(ctkSymbolicName.toLower(), descriptor.id);
            }
        }
    }

    PluginIdentity resolveByPlatformPluginId(const QString& pluginId) const
    {
        PluginIdentity identity;
        identity.pluginId = pluginId.trimmed();
        if (!identity.pluginId.isEmpty() && descriptorsByPluginId.contains(identity.pluginId)) {
            identity.ctkSymbolicName = descriptorsByPluginId.value(identity.pluginId).runtime.ctkSymbolicName.trimmed();
        }
        return identity;
    }

    PluginIdentity resolveByCtkSymbolicName(const QString& ctkSymbolicName) const
    {
        PluginIdentity identity;
        identity.ctkSymbolicName = ctkSymbolicName.trimmed();
        if (identity.ctkSymbolicName.isEmpty()) return identity;

        const auto exactPluginId = pluginIdByCtkSymbolicName.value(identity.ctkSymbolicName.toLower()).trimmed();
        if (!exactPluginId.isEmpty()) {
            identity.pluginId = exactPluginId;
            return identity;
        }

        QString normalized = identity.ctkSymbolicName;
        if (normalized.startsWith(QStringLiteral("lib"), Qt::CaseInsensitive)) {
            normalized = normalized.mid(3);
            const auto normalizedPluginId = pluginIdByCtkSymbolicName.value(normalized.toLower()).trimmed();
            if (!normalizedPluginId.isEmpty()) {
                identity.pluginId = normalizedPluginId;
                identity.ctkSymbolicName = normalized;
            }
        }
        return identity;
    }

    PluginIdentity resolveByCtkSymbolicOrPath(const QString& ctkSymbolicName, const QString& pluginPath) const
    {
        auto identity = resolveByCtkSymbolicName(ctkSymbolicName);
        if (!identity.pluginId.isEmpty()) return identity;

        const auto fileBaseName = QFileInfo(pluginPath).completeBaseName();
        if (fileBaseName.isEmpty()) return identity;

        auto byFile = resolveByCtkSymbolicName(fileBaseName);
        if (!byFile.pluginId.isEmpty()) return byFile;

        if (byFile.ctkSymbolicName.isEmpty()) byFile.ctkSymbolicName = fileBaseName;
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

    QStringList missingRequiredPlugins(const QString& pluginId, CTKManager* ctkManager) const
    {
        QStringList missing;
        if (!ctkManager) return missing;

        const auto descriptor = descriptorsByPluginId.value(pluginId);
        for (const auto& requiredPluginId : descriptor.required.plugins) {
            const auto requiredIdentity = resolveByPlatformPluginId(requiredPluginId);
            if (requiredIdentity.ctkSymbolicName.isEmpty()) {
                missing.append(requiredPluginId);
                continue;
            }
            if (!ctkManager->isPluginStarted(requiredIdentity.ctkSymbolicName)) {
                missing.append(requiredPluginId);
            }
        }
        return missing;
    }

    QStringList missingRequiredCapabilities(const QString& pluginId, CTKManager* ctkManager) const
    {
        QStringList missing;
        if (!ctkManager) return missing;

        const auto descriptor = descriptorsByPluginId.value(pluginId);
        for (const auto& requiredCapability : descriptor.required.capabilities) {
            bool capabilityReady = false;
            for (auto it = descriptorsByPluginId.constBegin(); it != descriptorsByPluginId.constEnd(); ++it) {
                const auto& candidate = it.value();
                if (!candidate.provides.capabilities.contains(requiredCapability)) continue;
                const auto ctkSymbolicName = candidate.runtime.ctkSymbolicName.trimmed();
                if (ctkSymbolicName.isEmpty()) continue;
                if (!ctkManager->isPluginStarted(ctkSymbolicName)) continue;
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
    QHash<QString, QString> pluginIdByCtkSymbolicName;
    mutable QMutex bridgeMutex;
    QHash<QString, PluginIdentity> pendingInstallByPath;
    QHash<QString, PluginIdentity> pendingStartByCtkSymbolicName;
    QVector<QMetaObject::Connection> startupRecorderBridgeConnections;
    std::atomic_bool startupRecorderBridgeEnabled{true};
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

        QHash<QString, QString> platformPluginIdToCtkSymbolicName;
        platformPluginIdToCtkSymbolicName.reserve(descriptors.size());
        for (const auto& descriptor : descriptors) {
            const auto ctkSymbolicName = descriptor.runtime.ctkSymbolicName.trimmed();
            if (ctkSymbolicName.isEmpty()) continue;
            platformPluginIdToCtkSymbolicName.insert(descriptor.id, ctkSymbolicName);
        }

        const QString pluginsPath =
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));
        QString managedPlanError;
        const auto managedPlan = PlatformManagedPluginPlanBuilder::build(
            runtimeConfig,
            descriptors,
            pluginsPath,
            &managedPlanError);
        if (!managedPlanError.isEmpty()) {
            throw std::runtime_error(
                QStringLiteral("Failed to build managed startup plan: %1").arg(managedPlanError).toStdString());
        }

        qDebug() << "[main] Platform runtime mode:" << runtimeModeToString(runtimeConfig.runtimeMode);
        qDebug() << "[main] Platform descriptor directory:" << runtimeConfig.descriptorDirectory;
        qDebug() << "[main] Platform core plugin ids:" << runtimeConfig.corePluginIds;
        qDebug() << "[main] Managed startup plugin ids:" << managedPlan.managedPluginIds;
        auto orchestrator = StartupOrchestrator::instance();
        orchestrator->setRuntimeMode(runtimeConfig.runtimeMode);
        auto startupContext = std::make_shared<StartupRuntimeContext>(
            runtimeConfig,
            ctkManager,
            platformPluginIdToCtkSymbolicName,
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
        // Phase 4: create the main interface without expensive work
        // ========================================
        qDebug() << "[Phase 4] Creating main interface";
        qDebug() << "----------------------------------------";
        const PlatformOnDemandProbeSet onDemandProbes {
            [startupContext](const QString& pluginId) {
                if (!startupContext->stateStore) return PlatformPluginState::Discovered;

                for (const auto& snapshot : startupContext->stateStore->pluginSnapshots()) {
                    if (snapshot.pluginId == pluginId) return snapshot.state;
                }

                return PlatformPluginState::Discovered;
            },
            [ctkManager](const QStringList& requiredServices) {
                return ctkManager->getMissingServices(requiredServices);
            },
            [startupContext, ctkManager](const QString& pluginId) {
                return startupContext->missingRequiredPlugins(pluginId, ctkManager);
            },
            [startupContext, ctkManager](const QString& pluginId) {
                return startupContext->missingRequiredCapabilities(pluginId, ctkManager);
            },
            [ctkManager](const QString&, const QStringList& healthChecks) {
                QVector<PlatformHealthCheckResult> results;

                for (const auto& healthCheck : healthChecks) {
                    PlatformHealthCheckResult result;
                    result.name = healthCheck;

                    if (healthCheck == QStringLiteral("service_registered")) {
                        result.passed = true;
                        result.detail = QStringLiteral("Required services were registered");
                    } else if (healthCheck == QStringLiteral("core_binary_accessible")) {
                        result.passed = ctkManager->getService<RegistrationService>() != nullptr;
                        result.detail = result.passed
                            ? QStringLiteral("RegistrationService is available")
                            : QStringLiteral("RegistrationService is not available");
                    } else if (healthCheck == QStringLiteral("tracking_adapter_accessible")) {
                        result.passed = ctkManager->getService<OpticalTrackingService>() != nullptr;
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
            [ctkManager](const PlatformOnDemandActivationPlanEntry& entry) {
                return ctkManager->installPlugin(entry.bundleFilePath, false, nullptr);
            },
            onDemandProbes);
        startupContext->navigationAdapter = std::make_unique<LegacyNavigationAdapter>(
            [service = startupContext->onDemandActivationService](const QString& pluginId) {
                return service->ensureReady(pluginId);
            });
        QPointer<MainInterfaceWidget> mainInterface = new MainInterfaceWidget(startupContext->navigationAdapter.get(), nullptr);
        mainInterface->platformStateStore()->replaceDescriptors(descriptors);
        mainInterface->platformStateStore()->setRuntimeMode(runtimeConfig.runtimeMode);
        mainInterface->platformStateStore()->setStartupScopePluginIds(managedPlan.managedPluginIds);
        QStringList governedPluginIds = managedPlan.managedPluginIds;
        if (!governedPluginIds.contains(QStringLiteral("org.medicalpro.registration_core"))) {
            governedPluginIds.append(QStringLiteral("org.medicalpro.registration_core"));
        }
        if (!governedPluginIds.contains(QStringLiteral("org.medicalpro.optical_tracking"))) {
            governedPluginIds.append(QStringLiteral("org.medicalpro.optical_tracking"));
        }
        mainInterface->platformStateStore()->setGovernedPluginIds(governedPluginIds);
        qDebug() << "[main] Creating main interface window...";
        mainInterface->setAttribute(Qt::WA_DeleteOnClose, true);
        startupContext->stateStore = mainInterface->platformStateStore();
        startupContext->onDemandActivationService->setStateStore(startupContext->stateStore);
        mainInterface->show();
        mainInterface->raise();
        mainInterface->activateWindow();
        mainInterface->showFullScreen();
        mainInterface->raise();

        qDebug() << "[Phase 4] Completed\n";

        // ========================================
        qDebug() << "[Phase 5] Launching background startup tasks";
        qDebug() << "----------------------------------------";
        orchestrator->setLifecycleRecorder(&startupContext->lifecycleRecorder);
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [startupContext]() {
            startupContext->shutdownRequested.store(true);
        });

        const auto normalizedIdentity = [startupContext](
                                            const QString& ctkSymbolicName,
                                            const QString& pluginPath) {
            auto identity = startupContext->resolveByCtkSymbolicOrPath(ctkSymbolicName, pluginPath);
            if (identity.ctkSymbolicName.isEmpty()) {
                identity.ctkSymbolicName = ctkSymbolicName.trimmed();
            }
            if (identity.pluginId.isEmpty()) {
                identity.pluginId = identity.ctkSymbolicName.isEmpty()
                    ? QStringLiteral("unknown_plugin")
                    : QStringLiteral("ctk:%1").arg(identity.ctkSymbolicName);
            }
            return identity;
        };

        const auto applyPluginState = [startupContext](const StartupRuntimeContext::PluginIdentity& identity, PlatformPluginState state) {
            if (!startupContext->stateStore) return;
            const auto pluginId = identity.pluginId.trimmed();
            if (pluginId.isEmpty()) return;
            if (!startupContext->descriptorsByPluginId.contains(pluginId)) return;
            startupContext->stateStore->setPluginState(pluginId, state);
        };

        // State write-back bridge (kept active during runtime).
        QObject::connect(
            ctkManager,
            &CTKManager::pluginInstalled,
            &app,
            [startupContext, normalizedIdentity, applyPluginState](const QString& pluginName, const QString& pluginPath) {
                applyPluginState(normalizedIdentity(pluginName, pluginPath), PlatformPluginState::Installed);
            },
            Qt::QueuedConnection);

        QObject::connect(
            ctkManager,
            &CTKManager::pluginInstallFailedDetailed,
            &app,
            [startupContext, normalizedIdentity, applyPluginState](const QString& pluginName, const QString& pluginPath, const QString&) {
                applyPluginState(normalizedIdentity(pluginName, pluginPath), PlatformPluginState::Failed);
            },
            Qt::QueuedConnection);

        QObject::connect(
            ctkManager,
            &CTKManager::pluginStartRequestedDetailed,
            &app,
            [normalizedIdentity, applyPluginState](const QString& pluginName) {
                applyPluginState(normalizedIdentity(pluginName, QString()), PlatformPluginState::Starting);
            },
            Qt::QueuedConnection);

        QObject::connect(
            ctkManager,
            &CTKManager::pluginStartedDetailed,
            &app,
            [normalizedIdentity, applyPluginState](const QString& pluginName) {
                applyPluginState(normalizedIdentity(pluginName, QString()), PlatformPluginState::Starting);
            },
            Qt::QueuedConnection);

        QObject::connect(
            ctkManager,
            &CTKManager::pluginStartFailedDetailed,
            &app,
            [normalizedIdentity, applyPluginState](const QString& pluginName, const QString&) {
                applyPluginState(normalizedIdentity(pluginName, QString()), PlatformPluginState::Failed);
            },
            Qt::QueuedConnection);

        // Startup recorder bridge lifecycle guard.
        QObject::connect(
            orchestrator,
            &StartupOrchestrator::startupCompleted,
            orchestrator,
            [startupContext](bool) {
                startupContext->startupRecorderBridgeEnabled.store(false);
                QMutexLocker locker(&startupContext->bridgeMutex);
                for (const auto& connection : startupContext->startupRecorderBridgeConnections) {
                    QObject::disconnect(connection);
                }
                startupContext->startupRecorderBridgeConnections.clear();
            },
            Qt::DirectConnection);

        {
            QMutexLocker locker(&startupContext->bridgeMutex);
            startupContext->startupRecorderBridgeConnections.append(QObject::connect(
            ctkManager,
            &CTKManager::pluginInstallStartedDetailed,
            [startupContext, normalizedIdentity](const QString& pluginName, const QString& pluginPath) {
                if (!startupContext->startupRecorderBridgeEnabled.load()) return;
                const auto identity = normalizedIdentity(pluginName, pluginPath);
                if (startupContext->managedPlan.managedPluginIds.contains(identity.pluginId)) return;
                {
                    QMutexLocker locker(&startupContext->bridgeMutex);
                    startupContext->pendingInstallByPath.insert(pluginPath, identity);
                }
                startupContext->lifecycleRecorder.recordPluginStepStarted(
                    identity.pluginId,
                    identity.ctkSymbolicName,
                    PlatformLifecycleStep::Install,
                    false);
            }));

            startupContext->startupRecorderBridgeConnections.append(QObject::connect(
            ctkManager,
            &CTKManager::pluginInstalled,
            [startupContext, normalizedIdentity](const QString& pluginName, const QString& pluginPath) {
                if (!startupContext->startupRecorderBridgeEnabled.load()) return;
                auto identity = normalizedIdentity(pluginName, pluginPath);
                if (startupContext->managedPlan.managedPluginIds.contains(identity.pluginId)) return;
                {
                    QMutexLocker locker(&startupContext->bridgeMutex);
                    if (startupContext->pendingInstallByPath.contains(pluginPath)) {
                        identity = startupContext->pendingInstallByPath.take(pluginPath);
                    }
                }
                startupContext->lifecycleRecorder.recordPluginStepFinished(
                    identity.pluginId,
                    identity.ctkSymbolicName,
                    PlatformLifecycleStep::Install,
                    PlatformLifecycleResult::Succeeded,
                    QStringLiteral("install_succeeded"),
                    QStringLiteral("CTK plugin install succeeded"));
            }));

            startupContext->startupRecorderBridgeConnections.append(QObject::connect(
            ctkManager,
            &CTKManager::pluginInstallFailedDetailed,
            [startupContext, normalizedIdentity](const QString& pluginName, const QString& pluginPath, const QString& reason) {
                if (!startupContext->startupRecorderBridgeEnabled.load()) return;
                auto identity = normalizedIdentity(pluginName, pluginPath);
                if (startupContext->managedPlan.managedPluginIds.contains(identity.pluginId)) return;
                {
                    QMutexLocker locker(&startupContext->bridgeMutex);
                    if (startupContext->pendingInstallByPath.contains(pluginPath)) {
                        identity = startupContext->pendingInstallByPath.take(pluginPath);
                    }
                }
                startupContext->lifecycleRecorder.recordPluginStepFinished(
                    identity.pluginId,
                    identity.ctkSymbolicName,
                    PlatformLifecycleStep::Install,
                    PlatformLifecycleResult::Failed,
                    QStringLiteral("install_failed"),
                    reason);
            }));

            startupContext->startupRecorderBridgeConnections.append(QObject::connect(
            ctkManager,
            &CTKManager::pluginStartRequestedDetailed,
            [startupContext, normalizedIdentity](const QString& pluginName) {
                if (!startupContext->startupRecorderBridgeEnabled.load()) return;
                auto identity = normalizedIdentity(pluginName, QString());
                if (!identity.pluginId.startsWith(QStringLiteral("ctk:"))
                    && startupContext->descriptorsByPluginId.contains(identity.pluginId)) {
                    return;
                }

                {
                    QMutexLocker locker(&startupContext->bridgeMutex);
                    startupContext->pendingStartByCtkSymbolicName.insert(identity.ctkSymbolicName.toLower(), identity);
                }
                startupContext->lifecycleRecorder.recordPluginStepStarted(
                    identity.pluginId,
                    identity.ctkSymbolicName,
                    PlatformLifecycleStep::Start,
                    false);
            }));

            startupContext->startupRecorderBridgeConnections.append(QObject::connect(
            ctkManager,
            &CTKManager::pluginStartedDetailed,
            [startupContext](const QString& pluginName) {
                if (!startupContext->startupRecorderBridgeEnabled.load()) return;
                StartupRuntimeContext::PluginIdentity identity;
                {
                    QMutexLocker locker(&startupContext->bridgeMutex);
                    identity = startupContext->pendingStartByCtkSymbolicName.take(pluginName.toLower());
                }
                if (identity.pluginId.isEmpty()) return;
                startupContext->lifecycleRecorder.recordPluginStepFinished(
                    identity.pluginId,
                    identity.ctkSymbolicName,
                    PlatformLifecycleStep::Start,
                    PlatformLifecycleResult::Succeeded,
                    QStringLiteral("start_succeeded"),
                    QStringLiteral("CTK plugin start succeeded"));
            }));

            startupContext->startupRecorderBridgeConnections.append(QObject::connect(
            ctkManager,
            &CTKManager::pluginStartFailedDetailed,
            [startupContext](const QString& pluginName, const QString& reason) {
                if (!startupContext->startupRecorderBridgeEnabled.load()) return;
                StartupRuntimeContext::PluginIdentity identity;
                {
                    QMutexLocker locker(&startupContext->bridgeMutex);
                    identity = startupContext->pendingStartByCtkSymbolicName.take(pluginName.toLower());
                }
                if (identity.pluginId.isEmpty()) return;
                startupContext->lifecycleRecorder.recordPluginStepFinished(
                    identity.pluginId,
                    identity.ctkSymbolicName,
                    PlatformLifecycleStep::Start,
                    PlatformLifecycleResult::Failed,
                    QStringLiteral("start_failed"),
                    reason);
            }));
        }

        // Register the CTK framework initialization handler
#ifdef CTK_PLUGIN_FRAMEWORK
        orchestrator->registerPhaseHandler(StartupPhase::CTKFrameworkInit, [ctkManager, startupContext](QApplication* app) -> StartupOrchestrator::PhaseExecutionResult {
            if (!startupContext->startupCoordinator.shouldInitializeFramework()) {
                qDebug() << "[StartupOrchestrator] Skipping CTK framework initialization in observe_only mode";
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("CTK framework initialization skipped in observe_only mode"));
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
        orchestrator->registerPhaseHandler(StartupPhase::PluginInstallation, [ctkManager, startupContext](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
            if (!startupContext->startupCoordinator.shouldInstallPlugins()) {
                qDebug() << "[StartupOrchestrator] Skipping plugin installation in observe_only mode";
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("Plugin installation skipped in observe_only mode"));
            }

            qDebug() << "[StartupOrchestrator] Running managed plugin installation...";
            const bool installed = startupContext->startupCoordinator.installManagedPlugins(
                startupContext->managedPlan,
                [ctkManager](const PlatformManagedPluginPlanEntry& entry) {
                    return ctkManager->installPlugin(entry.bundleFilePath, false, nullptr);
                });
            qDebug() << "[StartupOrchestrator] Managed plugin installation completed";
            return installed;
        });

        orchestrator->registerPhaseHandler(
            StartupPhase::CriticalPluginStart,
            [startupContext, ctkManager, applyPluginState](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
                if (!startupContext->startupCoordinator.shouldStartCorePlugins()) {
                    qDebug() << "[StartupOrchestrator] Skipping core plugin startup in observe_only mode";
                    return StartupOrchestrator::PhaseExecutionResult::skipped(
                        QStringLiteral("Core plugin activation skipped in observe_only mode"));
                }

                if (startupContext->managedPlan.installEntries.isEmpty()) {
                    qDebug() << "[StartupOrchestrator] No managed core plugins configured for startup";
                    return true;
                }

                qDebug() << "[StartupOrchestrator] Starting critical plugin activation (synchronous)...";

                for (const auto& entry : startupContext->managedPlan.installEntries) {
                    qDebug() << "[StartupOrchestrator] Starting managed core plugin:" << entry.pluginId;
                    if (!startupContext->startupCoordinator.startCorePlugin(entry.pluginId)) {
                        applyPluginState(startupContext->resolveByPlatformPluginId(entry.pluginId), PlatformPluginState::Failed);
                        qCritical() << "[StartupOrchestrator] Critical plugin start failed:" << entry.pluginId;
                        return false;
                    }

                    const auto identity = startupContext->resolveByPlatformPluginId(entry.pluginId);
                    const auto outcome = startupContext->startupCoordinator.waitForServiceReady(
                        entry,
                        {
                            [ctkManager](const QStringList& requiredServices) {
                                return ctkManager->getMissingServices(requiredServices);
                            },
                            [startupContext, ctkManager](const QString& pluginId) {
                                return startupContext->missingRequiredPlugins(pluginId, ctkManager);
                            },
                            [startupContext, ctkManager](const QString& pluginId) {
                                return startupContext->missingRequiredCapabilities(pluginId, ctkManager);
                            }
                        });

                    applyPluginState(identity, outcome.finalState);
                    startupContext->lifecycleRecorder.recordPluginStepStarted(
                        identity.pluginId,
                        identity.ctkSymbolicName,
                        PlatformLifecycleStep::ServiceReady,
                        true);
                    startupContext->lifecycleRecorder.recordPluginStepFinished(
                        identity.pluginId,
                        identity.ctkSymbolicName,
                        PlatformLifecycleStep::ServiceReady,
                        outcome.success ? PlatformLifecycleResult::Succeeded : PlatformLifecycleResult::Timeout,
                        outcome.reasonCode,
                        outcome.detail);
                    if (!outcome.success) {
                        qCritical() << "[StartupOrchestrator] Service ready timeout:" << entry.pluginId
                                    << outcome.missingServices << outcome.missingPlugins << outcome.missingCapabilities;
                        return false;
                    }
                }

                qDebug() << "[StartupOrchestrator] Critical plugin activation completed";
                return true;
        });

        orchestrator->registerPhaseHandler(StartupPhase::DeferredPluginStart, [ctkManager, startupContext](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
            if (!startupContext->startupCoordinator.shouldStartDeferredPlugins()) {
                qDebug() << "[StartupOrchestrator] Skipping deferred plugin startup in current runtime mode";
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("Deferred plugin activation skipped in current runtime mode"));
            }

            qDebug() << "[StartupOrchestrator] Running deferred plugin activation...";

            const auto deferredPlugins = ctkManager->getDeferredPlugins();
            if (deferredPlugins.isEmpty()) {
                qDebug() << "[StartupOrchestrator] No deferred plugins configured for startup";
                return true;
            }

            const bool success = startupContext->startupCoordinator.startDeferredPlugins(deferredPlugins);
            qDebug() << "[StartupOrchestrator] Deferred plugin activation completed";
            return success;
        });

        orchestrator->registerPhaseHandler(StartupPhase::ServiceWarmup, [startupContext](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
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
        });

#else
        qDebug() << "[Phase 5] CTK plugin framework disabled, skipping CTK startup handlers";
#endif

        QPointer<MainInterfaceWidget> mainInterfaceGuard(mainInterface);
        QObject::connect(orchestrator, &StartupOrchestrator::startupCompleted,
                             mainInterface, [mainInterfaceGuard, safeMode](bool success) {
                                 if (!success) {
                                     qWarning() << "[Startup] Background startup reported failures; continuing runtime";
                                     qWarning() << StartupOrchestrator::instance()->getDiagnosticReport();
                                 } else if (safeMode && mainInterfaceGuard) {
                                     QMessageBox::information(mainInterfaceGuard,
                                                              QObject::tr("安全模式"),
                                                              QObject::tr("应用正在安全模式下运行，部分可选插件已被跳过。\n\n诊断摘要：\n%1")
                                                                  .arg(StartupOrchestrator::instance()->getDiagnosticReport()));
                                 }
                             });

            QTimer::singleShot(0, orchestrator, [orchestrator, app = &app]() {
                orchestrator->start(app);
            });

            QObject::connect(mainInterface, &MainInterfaceWidget::exitRequested, &app, [mainInterface, startupContext, app = &app]() mutable {
                startupContext->shutdownRequested.store(true);
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
