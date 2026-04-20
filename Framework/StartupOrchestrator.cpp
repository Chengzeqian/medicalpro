#include "StartupOrchestrator.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QtConcurrent/QtConcurrent>
#include <QtGlobal>

#include "Logger.h"
#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"

StartupOrchestrator::StartupOrchestrator()
    : QObject(nullptr)
    , m_totalEstimatedMs(0)
    , m_progress(0)
{
    m_phases = {
        {StartupPhase::VTKInit, "VTK initialization", "Initialize the global VTK runtime", true, 500},
        {StartupPhase::QApplicationInit, "QApplication creation", "Create the Qt application instance", true, 100},
        {StartupPhase::SplashScreen, "Startup surface", "Prepare the startup surface", false, 50},
        {StartupPhase::MainUICreation, "Main interface creation", "Create and show the main interface", true, 300},
        {StartupPhase::CTKFrameworkInit, "CTK framework initialization", "Initialize the CTK plugin framework", true, 200},
        {StartupPhase::PluginInstallation, "Plugin installation", "Install available plugins", false, 500},
        {StartupPhase::CriticalPluginStart, "Critical plugin activation", "Start critical plugins", true, 300},
        {StartupPhase::DeferredPluginStart, "Deferred plugin activation", "Start non-critical plugins", false, 500},
        {StartupPhase::ServiceWarmup, "Service warmup", "Pre-initialize Python runtime and VTK components", false, 2000},
        {StartupPhase::Completed, "Startup complete", "All startup phases completed", true, 50}
    };

    for (const PhaseInfo& info : m_phases) {
        m_totalEstimatedMs += info.estimatedTimeMs;
    }
}

void StartupOrchestrator::start(QApplication* app)
{
    if (!app) {
        LOG_ERROR("StartupOrchestrator", "QApplication pointer is null");
        emit errorOccurred("Invalid QApplication instance");
        emit startupCompleted(false);
        return;
    }

    waitForCompletion();
    resetState();

    QFuture<void> startFuture = QtConcurrent::run([this, app]() {
        QElapsedTimer totalTimer;
        totalTimer.start();
        PlatformRuntimeMode runtimeMode = PlatformRuntimeMode::ObserveOnly;
        PlatformLifecycleTraceRecorder* lifecycleRecorder = nullptr;
        {
            QMutexLocker locker(&m_mutex);
            runtimeMode = m_runtimeMode;
            lifecycleRecorder = m_lifecycleRecorder;
        }
        if (lifecycleRecorder) {
            lifecycleRecorder->beginSession(runtimeMode);
        }
        int accumulatedMs = 0;
        for (const PhaseInfo& info : m_phases) {
            qint64 elapsedMs = 0;
            const bool success = executePhase(info, app, elapsedMs);

            if (!success) {
                QVariantMap context;
                context.insert(QStringLiteral("phase"), info.name);
                context.insert(QStringLiteral("description"), info.description);
                context.insert(QStringLiteral("durationMs"), elapsedMs);
                recordError(info.name, QStringLiteral("Phase execution failed"), info.isCritical, context);
                if (info.isCritical) {
                    {
                        QMutexLocker locker(&m_mutex);
                        m_totalElapsedMs = totalTimer.elapsed();
                    }
                    if (lifecycleRecorder) {
                        lifecycleRecorder->finishSession(
                            PlatformLifecycleResult::Failed,
                            QStringLiteral("critical_phase_failed"),
                            info.name);
                    }
                    emit startupCompleted(false);
                    return;
                }
            }

            accumulatedMs += info.estimatedTimeMs;
            updateProgress(accumulatedMs, info.name);
            emit phaseCompleted(info.name, m_phaseResults.value(info.phase, false));
        }

        {
            QMutexLocker locker(&m_mutex);
            m_totalElapsedMs = totalTimer.elapsed();
        }
        if (lifecycleRecorder) {
            lifecycleRecorder->finishSession(
                PlatformLifecycleResult::Succeeded,
                QString(),
                QStringLiteral("startup_completed"));
        }
        emit startupCompleted(true);
    });

    {
        QMutexLocker locker(&m_mutex);
        m_startFuture = startFuture;
    }
}

void StartupOrchestrator::waitForCompletion()
{
    QFuture<void> startFuture;
    {
        QMutexLocker locker(&m_mutex);
        startFuture = m_startFuture;
    }
    if (startFuture.isStarted()) startFuture.waitForFinished();
}

int StartupOrchestrator::getProgress() const
{
    QMutexLocker locker(&m_mutex);
    return m_progress;
}

QString StartupOrchestrator::getCurrentPhase() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentPhase;
}

QString StartupOrchestrator::getDiagnosticReport() const
{
    QMutexLocker locker(&m_mutex);
    return buildDiagnosticReportUnsafe();
}

bool StartupOrchestrator::hasErrors() const
{
    QMutexLocker locker(&m_mutex);
    return !m_errors.isEmpty();
}

bool StartupOrchestrator::hasWarnings() const
{
    QMutexLocker locker(&m_mutex);
    return !m_warnings.isEmpty();
}

QVector<PlatformStartupTraceEntry> StartupOrchestrator::getStartupTraceEntries() const
{
    QMutexLocker locker(&m_mutex);
    if (m_lifecycleRecorder) return m_lifecycleRecorder->startupTrace();
    return m_startupTraceEntries;
}

QVector<PlatformLifecycleEvent> StartupOrchestrator::getLifecycleEvents() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_lifecycleRecorder) return {};
    return m_lifecycleRecorder->lifecycleEvents();
}

void StartupOrchestrator::setLifecycleRecorder(PlatformLifecycleTraceRecorder* recorder)
{
    QMutexLocker locker(&m_mutex);
    m_lifecycleRecorder = recorder;
}

void StartupOrchestrator::setRuntimeMode(PlatformRuntimeMode runtimeMode)
{
    QMutexLocker locker(&m_mutex);
    m_runtimeMode = runtimeMode;
}

void StartupOrchestrator::logDiagnostic(ErrorHandler::ErrorLevel level,
                                        const QString& message,
                                        const QVariantMap& context)
{
    DiagnosticEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.message = message;
    entry.context = context;

    QString report;
    {
        QMutexLocker locker(&m_mutex);
        m_diagnostics.append(entry);
        report = buildDiagnosticReportUnsafe();
    }

    emit diagnosticEntryLogged(level, message);
    emit diagnosticReportUpdated(report);
}

void StartupOrchestrator::registerPhaseHandler(StartupPhase phase, const PhaseHandler& handler)
{
    QMutexLocker locker(&m_mutex);
    if (handler) {
        m_phaseHandlers.insert(phase, handler);
    } else {
        m_phaseHandlers.remove(phase);
    }
}

void StartupOrchestrator::clearPhaseHandlers()
{
    QMutexLocker locker(&m_mutex);
    m_phaseHandlers.clear();
}

bool StartupOrchestrator::executePhase(const PhaseInfo& info, QApplication* app, qint64& elapsedMs)
{
    PhaseHandler handler;
    {
        QMutexLocker locker(&m_mutex);
        handler = m_phaseHandlers.value(info.phase);
        m_currentPhase = info.name;
    }

    LOG_INFO("StartupOrchestrator", QString("Executing phase: %1").arg(info.name));
    if (m_lifecycleRecorder) {
        m_lifecycleRecorder->recordPhaseStarted(info.name, info.description, info.isCritical);
    }

    QElapsedTimer phaseTimer;
    phaseTimer.start();

    auto phaseResult = PhaseExecutionResult {};
    if (handler) phaseResult = handler(app);

    elapsedMs = phaseTimer.elapsed();
    if (m_lifecycleRecorder) {
        m_lifecycleRecorder->recordPhaseFinished(
            info.name,
            phaseResult.lifecycleResult,
            phaseResult.reasonCode,
            phaseResult.detail);
    }

    {
        QMutexLocker locker(&m_mutex);
        m_phaseDurations.insert(info.phase, elapsedMs);
        m_phaseResults.insert(info.phase, phaseResult.success);

        PlatformStartupTraceEntry entry;
        entry.spanId = QStringLiteral("phase:%1").arg(info.name);
        entry.parentSpanId = QStringLiteral("startup_session");
        entry.phaseKey = info.name;
        entry.phaseLabel = info.description;
        entry.result = phaseResult.lifecycleResult;
        entry.success = phaseResult.success;
        entry.blockingStartup = info.isCritical;
        entry.elapsedMs = elapsedMs;
        entry.reasonCode = phaseResult.reasonCode;
        entry.detail = phaseResult.detail;
        m_startupTraceEntries.append(entry);
    }

    return phaseResult.success;
}

void StartupOrchestrator::updateProgress(int completedMs, const QString& message)
{
    const int clamped = qBound(0, (m_totalEstimatedMs == 0 ? 0 : (completedMs * 100) / m_totalEstimatedMs), 100);

    {
        QMutexLocker locker(&m_mutex);
        m_progress = clamped;
        m_currentPhase = message;
    }

    emit progressChanged(clamped, message);
}

void StartupOrchestrator::recordError(const QString& phaseName, const QString& message, bool critical, const QVariantMap& context)
{
    const QString formatted = QString("[%1] %2").arg(phaseName, message);
    const ErrorHandler::ErrorLevel level = critical ? ErrorHandler::ErrorLevel::Critical : ErrorHandler::ErrorLevel::Warning;

    {
        QMutexLocker locker(&m_mutex);
        if (critical) {
            m_errors.append(formatted);
        } else {
            m_warnings.append(formatted);
        }
    }
    if (critical) {
        emit errorOccurred(formatted);
    }

    QVariantMap extendedContext = context;
    extendedContext.insert(QStringLiteral("phase"), phaseName);
    logDiagnostic(level, formatted, extendedContext);
}

void StartupOrchestrator::resetState()
{
    QMutexLocker locker(&m_mutex);
    m_progress = 0;
    m_currentPhase.clear();
    m_errors.clear();
    m_warnings.clear();
    m_phaseDurations.clear();
    m_phaseResults.clear();
    m_totalElapsedMs = 0;
    m_diagnostics.clear();
    m_startupTraceEntries.clear();
}

QString StartupOrchestrator::buildDiagnosticReportUnsafe() const
{
    QString report;
    report += QStringLiteral("Startup duration: %1 ms\n").arg(m_totalElapsedMs);

    report += QStringLiteral("\nPhase summary:\n");
    for (const PhaseInfo& info : m_phases) {
        const qint64 duration = m_phaseDurations.value(info.phase, 0);
        const bool success = m_phaseResults.value(info.phase, false);
        report += QStringLiteral(" - [%1] %2 | %3 ms | %4\n")
                      .arg(info.isCritical ? QStringLiteral("critical") : QStringLiteral("optional"))
                      .arg(info.name)
                      .arg(duration)
                      .arg(success ? QStringLiteral("success") : QStringLiteral("failed"));
    }

    if (!m_diagnostics.isEmpty()) {
        report += QStringLiteral("\nDiagnostics:\n");
        for (const DiagnosticEntry& entry : m_diagnostics) {
            report += QStringLiteral(" [%1] %2 - %3")
                          .arg(entry.timestamp.toString(Qt::ISODate))
                          .arg([&]() {
                              switch (entry.level) {
                              case ErrorHandler::ErrorLevel::Info:
                                  return QStringLiteral("info");
                              case ErrorHandler::ErrorLevel::Warning:
                                  return QStringLiteral("warning");
                              case ErrorHandler::ErrorLevel::Error:
                                  return QStringLiteral("error");
                              case ErrorHandler::ErrorLevel::Critical:
                                  return QStringLiteral("critical");
                              }
                              return QStringLiteral("unknown");
                          }())
                          .arg(entry.message);

            if (!entry.context.isEmpty()) {
                QStringList pairs;
                const auto keys = entry.context.keys();
                for (const QString& key : keys) {
                    pairs << QStringLiteral("%1=%2").arg(key, entry.context.value(key).toString());
                }
                report += QStringLiteral(" (%1)").arg(pairs.join(QStringLiteral(", ")));
            }
            report += QLatin1Char('\n');
        }
    }

    if (!m_errors.isEmpty()) {
        report += QStringLiteral("\nErrors:\n");
        for (const QString& error : m_errors) {
            report += QStringLiteral(" - %1\n").arg(error);
        }
    }

    if (!m_warnings.isEmpty()) {
        report += QStringLiteral("\nWarnings:\n");
        for (const QString& warning : m_warnings) {
            report += QStringLiteral(" - %1\n").arg(warning);
        }
    }

    return report;
}
