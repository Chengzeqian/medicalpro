#include "UI/MainInterfaceWidget.h"
#include "UI/AppTheme.h"

#include "Framework/CTKManager.h"
#include "Framework/ConsoleLogBridge.h"
#include "Framework/Platform/Kernel/PlatformDescriptorLoader.h"
#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"
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
#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QHash>
#include <QLocale>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSemaphore>
#include <QSet>
#include <QThread>
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

    int serviceReadyTimeoutMs(const QString& pluginId) const
    {
        const auto descriptor = descriptorsByPluginId.value(pluginId);
        return descriptor.diagnostics.serviceReadyTimeoutMs > 0
            ? descriptor.diagnostics.serviceReadyTimeoutMs
            : 3000;
    }

    QVector<PluginIdentity> warmupPluginTargets() const
    {
        QVector<PluginIdentity> targets;
        for (auto it = descriptorsByPluginId.constBegin(); it != descriptorsByPluginId.constEnd(); ++it) {
            const auto& descriptor = it.value();
            if (descriptor.diagnostics.warmupTasks.isEmpty()) continue;
            targets.append(resolveByPlatformPluginId(it.key()));
        }
        return targets;
    }

    void appendUniqueList(QStringList& target, const QStringList& values) const
    {
        for (const auto& value : values) {
            if (value.isEmpty()) continue;
            if (!target.contains(value)) target.append(value);
        }
    }

    PlatformRuntimeConfig runtimeConfig;
    PlatformLifecycleTraceRecorder lifecycleRecorder;
    PlatformStartupCoordinator startupCoordinator;
    PlatformStateStore* stateStore = nullptr;
    QHash<QString, PlatformPluginDescriptor> descriptorsByPluginId;
    QHash<QString, QString> pluginIdByCtkSymbolicName;
    mutable QMutex bridgeMutex;
    QHash<QString, PluginIdentity> pendingInstallByPath;
    QHash<QString, PluginIdentity> pendingStartByCtkSymbolicName;
    QVector<QMetaObject::Connection> startupRecorderBridgeConnections;
    std::atomic_bool startupRecorderBridgeEnabled{true};
    std::atomic_bool shutdownRequested{false};
};

struct ServiceWarmupSyncState
{
    void releaseOnce()
    {
        if (!completionReleased.exchange(true)) {
            completionSemaphore.release();
        }
    }

    QSemaphore completionSemaphore;
    std::atomic_bool completionReleased{false};
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

        QStringList coreCtkPluginNames;
        QHash<QString, QString> platformPluginIdToCtkSymbolicName;
        platformPluginIdToCtkSymbolicName.reserve(descriptors.size());
        for (const auto& descriptor : descriptors) {
            const auto ctkSymbolicName = descriptor.runtime.ctkSymbolicName.trimmed();
            if (ctkSymbolicName.isEmpty()) continue;
            platformPluginIdToCtkSymbolicName.insert(descriptor.id, ctkSymbolicName);
        }

        if (runtimeConfig.runtimeMode != PlatformRuntimeMode::ObserveOnly) {
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
        auto orchestrator = StartupOrchestrator::instance();
        orchestrator->setRuntimeMode(runtimeConfig.runtimeMode);

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
        QPointer<MainInterfaceWidget> mainInterface = new MainInterfaceWidget(nullptr);
        mainInterface->platformStateStore()->replaceDescriptors(descriptors);
        mainInterface->platformStateStore()->setRuntimeMode(runtimeConfig.runtimeMode);
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

        auto startupContext = std::make_shared<StartupRuntimeContext>(
            runtimeConfig,
            ctkManager,
            platformPluginIdToCtkSymbolicName,
            descriptors);
        startupContext->stateStore = mainInterface->platformStateStore();
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
            [startupContext, ctkManager, applyPluginState](QApplication*) -> StartupOrchestrator::PhaseExecutionResult {
                if (!startupContext->startupCoordinator.shouldStartCorePlugins()) {
                    qDebug() << "[StartupOrchestrator] Skipping core plugin startup in observe_only mode";
                    return StartupOrchestrator::PhaseExecutionResult::skipped(
                        QStringLiteral("Core plugin activation skipped in observe_only mode"));
                }

                if (startupContext->runtimeConfig.corePluginIds.isEmpty()) {
                    qDebug() << "[StartupOrchestrator] No core CTK plugins configured for startup";
                    return true;
                }

                qDebug() << "[StartupOrchestrator] Starting critical plugin activation (synchronous)...";

                for (const QString& pluginId : startupContext->runtimeConfig.corePluginIds) {
                    qDebug() << "[StartupOrchestrator] Starting core plugin from platform config:" << pluginId;
                    if (!startupContext->startupCoordinator.startCorePlugin(pluginId)) {
                        qCritical() << "[StartupOrchestrator] Critical plugin start failed:" << pluginId;
                        return false;
                    }

                    const auto identity = startupContext->resolveByPlatformPluginId(pluginId);
                    startupContext->lifecycleRecorder.recordPluginStepStarted(
                        identity.pluginId,
                        identity.ctkSymbolicName,
                        PlatformLifecycleStep::ServiceReady,
                        true);

                    const auto timeoutMs = startupContext->serviceReadyTimeoutMs(pluginId);
                    QElapsedTimer serviceReadyTimer;
                    serviceReadyTimer.start();

                    QStringList missingServices;
                    QStringList missingPlugins;
                    QStringList missingCapabilities;
                    while (true) {
                        missingServices = ctkManager->getMissingServices(startupContext->requiredServices(pluginId));
                        missingPlugins = startupContext->missingRequiredPlugins(pluginId, ctkManager);
                        missingCapabilities = startupContext->missingRequiredCapabilities(pluginId, ctkManager);

                        if (missingServices.isEmpty() && missingPlugins.isEmpty() && missingCapabilities.isEmpty()) {
                            applyPluginState(identity, PlatformPluginState::Ready);
                            startupContext->lifecycleRecorder.recordPluginStepFinished(
                                identity.pluginId,
                                identity.ctkSymbolicName,
                                PlatformLifecycleStep::ServiceReady,
                                PlatformLifecycleResult::Succeeded,
                                QStringLiteral("service_ready"),
                                QStringLiteral("Required services and dependencies are ready"));
                            break;
                        }

                        if (serviceReadyTimer.elapsed() >= timeoutMs) {
                            applyPluginState(identity, PlatformPluginState::Failed);
                            QStringList details;
                            if (!missingServices.isEmpty()) {
                                details.append(
                                    QStringLiteral("missing_services=%1").arg(missingServices.join(QStringLiteral(","))));
                            }
                            if (!missingPlugins.isEmpty()) {
                                details.append(
                                    QStringLiteral("missing_plugins=%1").arg(missingPlugins.join(QStringLiteral(","))));
                            }
                            if (!missingCapabilities.isEmpty()) {
                                details.append(
                                    QStringLiteral("missing_capabilities=%1")
                                        .arg(missingCapabilities.join(QStringLiteral(","))));
                            }
                            startupContext->lifecycleRecorder.recordPluginStepFinished(
                                identity.pluginId,
                                identity.ctkSymbolicName,
                                PlatformLifecycleStep::ServiceReady,
                                PlatformLifecycleResult::Timeout,
                                QStringLiteral("service_ready_timeout"),
                                details.join(QStringLiteral("; ")));
                            qCritical() << "[StartupOrchestrator] Service ready timeout:" << pluginId << details;
                            return false;
                        }

                        QThread::msleep(50);
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

        // Register the service warmup handler
        // This phase runs off the UI thread and dispatches UI-bound work back
        orchestrator->registerPhaseHandler(StartupPhase::ServiceWarmup, [ctkManager, startupContext, applyPluginState](QApplication* app) -> StartupOrchestrator::PhaseExecutionResult {
            const auto warmupIdentityBySymbolicName = [startupContext](const QString& ctkSymbolicName) {
                auto identity = startupContext->resolveByCtkSymbolicName(ctkSymbolicName);
                if (identity.ctkSymbolicName.isEmpty()) identity.ctkSymbolicName = ctkSymbolicName;
                if (identity.pluginId.isEmpty()) {
                    identity.pluginId = QStringLiteral("ctk:%1").arg(identity.ctkSymbolicName);
                }
                return identity;
            };

            const auto warmupTargets = QVector<StartupRuntimeContext::PluginIdentity>{
                warmupIdentityBySymbolicName(QStringLiteral("Registration2D3D")),
                warmupIdentityBySymbolicName(QStringLiteral("FourViewDisplay"))
            };

            if (!startupContext->startupCoordinator.shouldWarmupServices()) {
                qDebug() << "[StartupOrchestrator] Skipping service warmup in current runtime mode";
                for (const auto& identity : warmupTargets) {
                    startupContext->lifecycleRecorder.recordPluginStepStarted(
                        identity.pluginId,
                        identity.ctkSymbolicName,
                        PlatformLifecycleStep::Warmup,
                        false);
                    startupContext->lifecycleRecorder.recordPluginStepFinished(
                        identity.pluginId,
                        identity.ctkSymbolicName,
                        PlatformLifecycleStep::Warmup,
                        PlatformLifecycleResult::Skipped,
                        QStringLiteral("skipped_by_mode"),
                        QStringLiteral("Service warmup skipped in current runtime mode"));
                }
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("Service warmup skipped in current runtime mode"));
            }
            if (startupContext->shutdownRequested.load()) {
                qDebug() << "[StartupOrchestrator] Aborting service warmup because shutdown was requested";
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("Service warmup aborted because application shutdown was requested"),
                    QStringLiteral("aborted_by_shutdown"));
            }

            qDebug() << "[StartupOrchestrator] Running service warmup...";

            // Use heap-owned sync state so queued work never touches destroyed stack objects.
            auto syncState = std::make_shared<ServiceWarmupSyncState>();
            auto warmupOutcome = std::make_shared<PlatformLifecycleResult>(PlatformLifecycleResult::Succeeded);

            // Dispatch Python initialization onto the main thread
            const bool invokeQueued = QMetaObject::invokeMethod(app, [ctkManager, startupContext, syncState, warmupOutcome, warmupTargets, applyPluginState]() {
                if (startupContext->shutdownRequested.load()) {
                    qDebug() << "[ServiceWarmup] Main-thread warmup skipped because shutdown was requested";
                    syncState->releaseOnce();
                    return;
                }

                qDebug() << "[ServiceWarmup] Initializing services on the main thread...";

                // 1. Warm up the Registration2D3D Python environment
                const auto& regIdentity = warmupTargets.at(0);
                startupContext->lifecycleRecorder.recordPluginStepStarted(
                    regIdentity.pluginId,
                    regIdentity.ctkSymbolicName,
                    PlatformLifecycleStep::Warmup,
                    false);
                auto reg2D3DService = ctkManager->getService<Registration2D3DService>();
                if (reg2D3DService) {
                    qDebug() << "[ServiceWarmup] Warming up Registration2D3D Python environment...";
                    bool warmupSuccess = true;
                    bool pythonDeferred = reg2D3DService->getConfiguration("pythonInitDeferred", false).toBool();
                    if (pythonDeferred && !reg2D3DService->isPythonInitialized()) {
                        QString pythonHome = reg2D3DService->getConfiguration("pythonHome", QString()).toString();
                        QString scriptsPath = reg2D3DService->getConfiguration("scriptsPath", QString()).toString();
                        if (!scriptsPath.isEmpty()) {
                            const bool pythonInitSuccess = reg2D3DService->initializePythonEnvironment(pythonHome, scriptsPath);
                            if (!pythonInitSuccess) {
                                warmupSuccess = false;
                                qWarning() << "[ServiceWarmup] Python environment initialization failed:"
                                           << reg2D3DService->getLastError();
                            }
                        }
                    }
                    if (warmupSuccess) {
                        startupContext->lifecycleRecorder.recordPluginStepFinished(
                            regIdentity.pluginId,
                            regIdentity.ctkSymbolicName,
                            PlatformLifecycleStep::Warmup,
                            PlatformLifecycleResult::Succeeded,
                            QStringLiteral("warmup_ready"),
                            QStringLiteral("Registration2D3D warmup completed"));
                    } else {
                        applyPluginState(regIdentity, PlatformPluginState::Degraded);
                        *warmupOutcome = PlatformLifecycleResult::Degraded;
                        startupContext->lifecycleRecorder.recordPluginStepFinished(
                            regIdentity.pluginId,
                            regIdentity.ctkSymbolicName,
                            PlatformLifecycleStep::Warmup,
                            PlatformLifecycleResult::Degraded,
                            QStringLiteral("warmup_failed"),
                            QStringLiteral("Registration2D3D Python environment warmup failed"));
                    }
                    qDebug() << "[ServiceWarmup] Registration2D3D Python environment warmup completed";
                } else {
                    startupContext->lifecycleRecorder.recordPluginStepFinished(
                        regIdentity.pluginId,
                        regIdentity.ctkSymbolicName,
                        PlatformLifecycleStep::Warmup,
                        PlatformLifecycleResult::Skipped,
                        QStringLiteral("service_not_available"),
                        QStringLiteral("Registration2D3D service not available for warmup"));
                }

                // 2. Warm up the FourViewDisplay service
                const auto& fourViewIdentity = warmupTargets.at(1);
                startupContext->lifecycleRecorder.recordPluginStepStarted(
                    fourViewIdentity.pluginId,
                    fourViewIdentity.ctkSymbolicName,
                    PlatformLifecycleStep::Warmup,
                    false);
                auto fourViewService = ctkManager->getService<FourViewDisplayService>();
                if (fourViewService) {
                    qDebug() << "[ServiceWarmup] Warming up FourViewDisplay service...";
                    startupContext->lifecycleRecorder.recordPluginStepFinished(
                        fourViewIdentity.pluginId,
                        fourViewIdentity.ctkSymbolicName,
                        PlatformLifecycleStep::Warmup,
                        PlatformLifecycleResult::Succeeded,
                        QStringLiteral("warmup_ready"),
                        QStringLiteral("FourViewDisplay warmup completed"));
                    qDebug() << "[ServiceWarmup] FourViewDisplay service warmup completed";
                } else {
                    startupContext->lifecycleRecorder.recordPluginStepFinished(
                        fourViewIdentity.pluginId,
                        fourViewIdentity.ctkSymbolicName,
                        PlatformLifecycleStep::Warmup,
                        PlatformLifecycleResult::Skipped,
                        QStringLiteral("service_not_available"),
                        QStringLiteral("FourViewDisplay service not available for warmup"));
                }

                qDebug() << "[ServiceWarmup] Main-thread service initialization completed";
                syncState->releaseOnce();
            }, Qt::QueuedConnection);
            if (!invokeQueued) {
                if (startupContext->shutdownRequested.load()) {
                    qDebug() << "[StartupOrchestrator] Skipping service warmup queue because shutdown was requested";
                    return StartupOrchestrator::PhaseExecutionResult::skipped(
                        QStringLiteral("Service warmup aborted because application shutdown was requested"),
                        QStringLiteral("aborted_by_shutdown"));
                }

                qWarning() << "[ServiceWarmup] Failed to queue main-thread warmup work";
                return false;
            }

            // Poll in short intervals so shutdown can stop the wait immediately.
            QElapsedTimer waitTimer;
            waitTimer.start();
            while (!syncState->completionSemaphore.tryAcquire(1, 50)) {
                if (startupContext->shutdownRequested.load()) {
                    qDebug() << "[StartupOrchestrator] Aborting service warmup wait because shutdown was requested";
                    syncState->releaseOnce();
                    return StartupOrchestrator::PhaseExecutionResult::skipped(
                        QStringLiteral("Service warmup aborted because application shutdown was requested"),
                        QStringLiteral("aborted_by_shutdown"));
                }
                if (waitTimer.elapsed() >= 30000) {
                    break;
                }
            }
            if (waitTimer.elapsed() >= 30000 && !startupContext->shutdownRequested.load()) {
                qWarning() << "[ServiceWarmup] Service warmup timed out";
                return false;
            }
            if (startupContext->shutdownRequested.load()) {
                qDebug() << "[StartupOrchestrator] Service warmup completed as shutdown abort";
                return StartupOrchestrator::PhaseExecutionResult::skipped(
                    QStringLiteral("Service warmup aborted because application shutdown was requested"),
                    QStringLiteral("aborted_by_shutdown"));
            }

            qDebug() << "[StartupOrchestrator] Service warmup completed";
            if (*warmupOutcome == PlatformLifecycleResult::Degraded) {
                return StartupOrchestrator::PhaseExecutionResult {
                    true,
                    PlatformLifecycleResult::Degraded,
                    QStringLiteral("warmup_degraded"),
                    QStringLiteral("Service warmup completed with degradation")
                };
            }

            return true;  // Warmup failures should not block app startup
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
