#ifndef STARTUPORCHESTRATOR_H
#define STARTUPORCHESTRATOR_H

#include "FrameworkExport.h"
#include "ResourceManagement/SingletonManager.h"

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "ErrorHandler.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

class QApplication;

enum class StartupPhase {
    VTKInit,
    QApplicationInit,
    SplashScreen,
    MainUICreation,
    CTKFrameworkInit,
    PluginInstallation,
    CriticalPluginStart,
    DeferredPluginStart,
    ServiceWarmup,        // 新增：服务预热阶段（Python初始化、VTK预热等）
    Completed
};

// qHash function for StartupPhase to use with QHash
inline uint qHash(StartupPhase phase, uint seed = 0) noexcept
{
    return qHash(static_cast<uint>(phase), seed);
}

struct PhaseInfo {
    StartupPhase phase;
    QString name;
    QString description;
    bool isCritical{true};
    int estimatedTimeMs{0};
};

struct DiagnosticEntry {
    QDateTime timestamp;
    ErrorHandler::ErrorLevel level;
    QString message;
    QVariantMap context;
};

/**
 * @brief 启动编排器
 *
 * 使用SingletonManager模式管理单例生命周期（需求6.1-6.5）
 */
class FRAMEWORK_EXPORT StartupOrchestrator : public QObject, public SingletonManager<StartupOrchestrator>
{
    Q_OBJECT
    friend class SingletonManager<StartupOrchestrator>;

public:
    using PhaseHandler = std::function<bool(QApplication*)>;

    /**
     * @brief 获取单例实例指针（兼容性接口）
     * @return 单例实例指针
     */
    static StartupOrchestrator* instance() { return &SingletonManager<StartupOrchestrator>::instance(); }

    void start(QApplication* app);

    int getProgress() const;
    QString getCurrentPhase() const;
    QString getDiagnosticReport() const;
    bool hasErrors() const;
    bool hasWarnings() const;
    QVector<PlatformStartupTraceEntry> getStartupTraceEntries() const;

    void logDiagnostic(ErrorHandler::ErrorLevel level,
                       const QString& message,
                       const QVariantMap& context = {});

    void registerPhaseHandler(StartupPhase phase, const PhaseHandler& handler);
    void clearPhaseHandlers();

signals:
    void progressChanged(int progress, const QString& message);
    void phaseCompleted(const QString& phaseName, bool success);
    void startupCompleted(bool success);
    void errorOccurred(const QString& errorMessage);
    void diagnosticEntryLogged(ErrorHandler::ErrorLevel level, const QString& message);
    void diagnosticReportUpdated(const QString& report);

private:
    StartupOrchestrator();

    bool executePhase(const PhaseInfo& info, QApplication* app, qint64& elapsedMs);
    void updateProgress(int completedMs, const QString& message);
    void recordError(const QString& phaseName, const QString& message, bool critical, const QVariantMap& context = {});
    void resetState();
    QString buildDiagnosticReportUnsafe() const;

    QVector<PhaseInfo> m_phases;
    int m_totalEstimatedMs;

    mutable QMutex m_mutex;
    int m_progress;
    QString m_currentPhase;
    QStringList m_errors;
    QStringList m_warnings;
    QHash<StartupPhase, qint64> m_phaseDurations;
    QHash<StartupPhase, bool> m_phaseResults;
    QHash<StartupPhase, PhaseHandler> m_phaseHandlers;
    qint64 m_totalElapsedMs;
    QVector<DiagnosticEntry> m_diagnostics;
    QVector<PlatformStartupTraceEntry> m_startupTraceEntries;
};

#endif // STARTUPORCHESTRATOR_H
