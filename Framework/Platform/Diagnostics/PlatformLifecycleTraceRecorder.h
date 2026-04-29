#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>

class FRAMEWORK_EXPORT PlatformLifecycleTraceRecorder
{
public:
    void beginSession(PlatformRuntimeMode runtimeMode);
    void finishSession(
        PlatformLifecycleResult result = PlatformLifecycleResult::Succeeded,
        const QString& reasonCode = {},
        const QString& detail = {});
    void recordPhaseStarted(const QString& phaseKey, const QString& phaseLabel, bool blockingStartup);
    void recordPhaseFinished(
        const QString& phaseKey,
        PlatformLifecycleResult result,
        const QString& reasonCode = {},
        const QString& detail = {});
    void recordPluginStepStarted(
        const QString& pluginId,
        const QString& symbolicName,
        PlatformLifecycleStep step,
        bool blockingStartup);
    void recordPluginStepFinished(
        const QString& pluginId,
        const QString& symbolicName,
        PlatformLifecycleStep step,
        PlatformLifecycleResult result,
        const QString& reasonCode = {},
        const QString& detail = {});

    QVector<PlatformLifecycleEvent> lifecycleEvents() const;
    QVector<PlatformStartupTraceEntry> startupTrace() const;

private:
    struct ActiveSpan
    {
        QString spanId;
        QString parentSpanId;
        QString pluginId;
        QString symbolicName;
        QString phaseKey;
        QString phaseLabel;
        PlatformLifecycleStep step = PlatformLifecycleStep::None;
        bool blockingStartup = false;
        qint64 startedAtMs = 0;
    };

    void beginSessionLocked(PlatformRuntimeMode runtimeMode);
    qint64 currentTimestampMs() const;
    void appendEvent(
        PlatformLifecycleEventKind kind,
        const ActiveSpan& span,
        PlatformLifecycleResult result,
        const QString& reasonCode,
        const QString& detail,
        qint64 elapsedMs);
    void appendTrace(
        const ActiveSpan& span,
        PlatformLifecycleResult result,
        const QString& reasonCode,
        const QString& detail,
        qint64 elapsedMs,
        qint64 endOffsetMs);
    ActiveSpan takePhaseSpan(const QString& phaseKey);
    ActiveSpan takePluginSpan(
        const QString& pluginId,
        const QString& symbolicName,
        PlatformLifecycleStep step);
    static QString stepKey(PlatformLifecycleStep step);
    static QString pluginSpanKey(
        const QString& pluginId,
        const QString& symbolicName,
        PlatformLifecycleStep step);

    QString m_sessionId = QStringLiteral("startup_session");
    mutable QMutex m_mutex;
    PlatformRuntimeMode m_runtimeMode = PlatformRuntimeMode::ObserveOnly;
    QElapsedTimer m_sessionTimer;
    bool m_sessionActive = false;
    QHash<QString, ActiveSpan> m_phaseSpans;
    QHash<QString, ActiveSpan> m_pluginSpans;
    QVector<PlatformLifecycleEvent> m_lifecycleEvents;
    QVector<PlatformStartupTraceEntry> m_startupTrace;
};
