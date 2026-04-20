#include "Framework/Platform/Diagnostics/PlatformLifecycleTraceRecorder.h"

#include <QMutexLocker>
#include <QtGlobal>

#include <atomic>

namespace
{
std::atomic<qint64> g_sessionCounter = 0;

bool isModeSkipReason(const QString& reasonCode)
{
    return reasonCode == QStringLiteral("runtime_mode")
        || reasonCode == QStringLiteral("skipped_by_mode");
}
}

QString PlatformLifecycleTraceRecorder::stepKey(PlatformLifecycleStep step)
{
    switch (step) {
    case PlatformLifecycleStep::None:
        return QStringLiteral("none");
    case PlatformLifecycleStep::Install:
        return QStringLiteral("install");
    case PlatformLifecycleStep::Start:
        return QStringLiteral("start");
    case PlatformLifecycleStep::ServiceReady:
        return QStringLiteral("service_ready");
    case PlatformLifecycleStep::Warmup:
        return QStringLiteral("warmup");
    }

    return QStringLiteral("unknown");
}

QString PlatformLifecycleTraceRecorder::pluginSpanKey(
    const QString& pluginId,
    const QString& ctkSymbolicName,
    PlatformLifecycleStep step)
{
    QString identity = pluginId.trimmed();
    if (identity.isEmpty()) identity = ctkSymbolicName.trimmed();
    if (identity.isEmpty()) identity = QStringLiteral("unknown");
    return QStringLiteral("%1|%2").arg(identity.toLower(), stepKey(step));
}

qint64 PlatformLifecycleTraceRecorder::currentTimestampMs() const
{
    return m_sessionTimer.isValid() ? m_sessionTimer.elapsed() : 0;
}

void PlatformLifecycleTraceRecorder::beginSession(PlatformRuntimeMode runtimeMode)
{
    QMutexLocker locker(&m_mutex);
    beginSessionLocked(runtimeMode);
}

void PlatformLifecycleTraceRecorder::beginSessionLocked(PlatformRuntimeMode runtimeMode)
{
    m_runtimeMode = runtimeMode;
    m_sessionActive = true;
    m_sessionId = QStringLiteral("startup_session_%1").arg(++g_sessionCounter);
    m_phaseSpans.clear();
    m_pluginSpans.clear();
    m_lifecycleEvents.clear();
    m_startupTrace.clear();
    m_sessionTimer.start();

    appendEvent(
        PlatformLifecycleEventKind::StartupSessionStarted,
        {},
        PlatformLifecycleResult::Running,
        QString(),
        QString(),
        0);
}

void PlatformLifecycleTraceRecorder::finishSession(
    PlatformLifecycleResult result,
    const QString& reasonCode,
    const QString& detail)
{
    QMutexLocker locker(&m_mutex);
    if (!m_sessionActive) return;

    appendEvent(
        PlatformLifecycleEventKind::StartupSessionFinished,
        {},
        result,
        reasonCode,
        detail,
        currentTimestampMs());
    m_sessionActive = false;
    m_phaseSpans.clear();
    m_pluginSpans.clear();
}

void PlatformLifecycleTraceRecorder::recordPhaseStarted(const QString& phaseKey, const QString& phaseLabel, bool blockingStartup)
{
    QMutexLocker locker(&m_mutex);
    if (!m_sessionActive) beginSessionLocked(m_runtimeMode);

    ActiveSpan span;
    span.spanId = QStringLiteral("phase:%1").arg(phaseKey);
    span.parentSpanId = QStringLiteral("startup_session");
    span.phaseKey = phaseKey;
    span.phaseLabel = phaseLabel;
    span.blockingStartup = blockingStartup;
    span.startedAtMs = currentTimestampMs();
    m_phaseSpans.insert(phaseKey, span);

    appendEvent(
        PlatformLifecycleEventKind::PhaseStarted,
        span,
        PlatformLifecycleResult::Running,
        QString(),
        QString(),
        0);
}

void PlatformLifecycleTraceRecorder::recordPhaseFinished(
    const QString& phaseKey,
    PlatformLifecycleResult result,
    const QString& reasonCode,
    const QString& detail)
{
    QMutexLocker locker(&m_mutex);
    const auto span = takePhaseSpan(phaseKey);
    const auto endOffsetMs = currentTimestampMs();
    const auto elapsedMs = qMax<qint64>(0, endOffsetMs - span.startedAtMs);

    appendEvent(
        PlatformLifecycleEventKind::PhaseFinished,
        span,
        result,
        reasonCode,
        detail,
        elapsedMs);
    appendTrace(span, result, reasonCode, detail, elapsedMs, endOffsetMs);
}

void PlatformLifecycleTraceRecorder::recordPluginStepStarted(
    const QString& pluginId,
    const QString& ctkSymbolicName,
    PlatformLifecycleStep step,
    bool blockingStartup)
{
    QMutexLocker locker(&m_mutex);
    if (!m_sessionActive) beginSessionLocked(m_runtimeMode);

    ActiveSpan span;
    span.spanId = QStringLiteral("plugin:%1:%2").arg(pluginId, stepKey(step));
    span.parentSpanId = QStringLiteral("startup_session");
    span.pluginId = pluginId;
    span.ctkSymbolicName = ctkSymbolicName;
    span.phaseKey = pluginId;
    span.phaseLabel = stepKey(step);
    span.step = step;
    span.blockingStartup = blockingStartup;
    span.startedAtMs = currentTimestampMs();
    m_pluginSpans.insert(pluginSpanKey(pluginId, ctkSymbolicName, step), span);

    auto eventKind = PlatformLifecycleEventKind::PluginStartStarted;
    if (step == PlatformLifecycleStep::Install) {
        eventKind = PlatformLifecycleEventKind::PluginInstallStarted;
    } else if (step == PlatformLifecycleStep::Warmup) {
        eventKind = PlatformLifecycleEventKind::PluginWarmupStarted;
    } else if (step == PlatformLifecycleStep::ServiceReady) {
        return;
    }

    appendEvent(
        eventKind,
        span,
        PlatformLifecycleResult::Running,
        QString(),
        QString(),
        0);
}

void PlatformLifecycleTraceRecorder::recordPluginStepFinished(
    const QString& pluginId,
    const QString& ctkSymbolicName,
    PlatformLifecycleStep step,
    PlatformLifecycleResult result,
    const QString& reasonCode,
    const QString& detail)
{
    QMutexLocker locker(&m_mutex);
    auto span = takePluginSpan(pluginId, ctkSymbolicName, step);
    if (!ctkSymbolicName.isEmpty()) span.ctkSymbolicName = ctkSymbolicName;
    const auto endOffsetMs = currentTimestampMs();
    const auto elapsedMs = qMax<qint64>(0, endOffsetMs - span.startedAtMs);

    auto eventKind = PlatformLifecycleEventKind::PluginStartFinished;
    if (result == PlatformLifecycleResult::Failed) {
        eventKind = PlatformLifecycleEventKind::PluginFailed;
    } else if (result == PlatformLifecycleResult::Degraded) {
        eventKind = PlatformLifecycleEventKind::PluginDegraded;
    } else if (result == PlatformLifecycleResult::Skipped && isModeSkipReason(reasonCode)) {
        eventKind = PlatformLifecycleEventKind::PluginSkippedByMode;
    } else if (step == PlatformLifecycleStep::Install) {
        eventKind = PlatformLifecycleEventKind::PluginInstallFinished;
    } else if (step == PlatformLifecycleStep::Warmup) {
        eventKind = PlatformLifecycleEventKind::PluginWarmupFinished;
    } else if (step == PlatformLifecycleStep::ServiceReady) {
        eventKind = PlatformLifecycleEventKind::PluginServiceReady;
    }

    appendEvent(
        eventKind,
        span,
        result,
        reasonCode,
        detail,
        elapsedMs);
    appendTrace(span, result, reasonCode, detail, elapsedMs, endOffsetMs);
}

QVector<PlatformLifecycleEvent> PlatformLifecycleTraceRecorder::lifecycleEvents() const
{
    QMutexLocker locker(&m_mutex);
    return m_lifecycleEvents;
}

QVector<PlatformStartupTraceEntry> PlatformLifecycleTraceRecorder::startupTrace() const
{
    QMutexLocker locker(&m_mutex);
    return m_startupTrace;
}

void PlatformLifecycleTraceRecorder::appendEvent(
    PlatformLifecycleEventKind kind,
    const ActiveSpan& span,
    PlatformLifecycleResult result,
    const QString& reasonCode,
    const QString& detail,
    qint64 elapsedMs)
{
    PlatformLifecycleEvent event;
    event.sessionId = m_sessionId;
    event.kind = kind;
    event.step = span.step;
    event.result = result;
    event.runtimeMode = m_runtimeMode;
    event.phaseKey = span.phaseKey;
    event.phaseLabel = span.phaseLabel;
    event.pluginId = span.pluginId;
    event.ctkSymbolicName = span.ctkSymbolicName;
    event.offsetMs = currentTimestampMs();
    event.durationMs = elapsedMs;
    event.blockingStartup = span.blockingStartup;
    event.critical = span.blockingStartup || kind == PlatformLifecycleEventKind::PluginFailed;
    event.reasonCode = reasonCode;
    event.detail = detail;
    m_lifecycleEvents.append(event);
}

void PlatformLifecycleTraceRecorder::appendTrace(
    const ActiveSpan& span,
    PlatformLifecycleResult result,
    const QString& reasonCode,
    const QString& detail,
    qint64 elapsedMs,
    qint64 endOffsetMs)
{
    PlatformStartupTraceEntry entry;
    entry.spanId = span.spanId;
    entry.parentSpanId = span.parentSpanId;
    entry.phaseKey = span.phaseKey;
    entry.phaseLabel = span.phaseLabel;
    entry.pluginId = span.pluginId;
    entry.ctkSymbolicName = span.ctkSymbolicName;
    entry.step = span.step;
    entry.result = result;
    entry.success = result != PlatformLifecycleResult::Failed
        && result != PlatformLifecycleResult::Timeout;
    entry.blockingStartup = span.blockingStartup;
    entry.startOffsetMs = span.startedAtMs;
    entry.endOffsetMs = endOffsetMs;
    entry.elapsedMs = elapsedMs;
    entry.reasonCode = reasonCode;
    entry.detail = detail;
    m_startupTrace.append(entry);
}

PlatformLifecycleTraceRecorder::ActiveSpan PlatformLifecycleTraceRecorder::takePhaseSpan(const QString& phaseKey)
{
    if (m_phaseSpans.contains(phaseKey)) return m_phaseSpans.take(phaseKey);

    ActiveSpan span;
    span.spanId = QStringLiteral("phase:%1").arg(phaseKey);
    span.parentSpanId = QStringLiteral("startup_session");
    span.phaseKey = phaseKey;
    span.phaseLabel = phaseKey;
    span.startedAtMs = currentTimestampMs();
    return span;
}

PlatformLifecycleTraceRecorder::ActiveSpan PlatformLifecycleTraceRecorder::takePluginSpan(
    const QString& pluginId,
    const QString& ctkSymbolicName,
    PlatformLifecycleStep step)
{
    const auto key = pluginSpanKey(pluginId, ctkSymbolicName, step);
    if (m_pluginSpans.contains(key)) return m_pluginSpans.take(key);

    const auto normalizedPluginId = pluginId.trimmed().toLower();
    const auto normalizedCtkName = ctkSymbolicName.trimmed().toLower();
    for (auto it = m_pluginSpans.begin(); it != m_pluginSpans.end(); ++it) {
        const auto& candidate = it.value();
        if (candidate.step != step) continue;
        const auto candidatePluginId = candidate.pluginId.trimmed().toLower();
        const auto candidateCtkName = candidate.ctkSymbolicName.trimmed().toLower();
        const bool pluginMatch = !normalizedPluginId.isEmpty() && candidatePluginId == normalizedPluginId;
        const bool ctkMatch = !normalizedCtkName.isEmpty() && candidateCtkName == normalizedCtkName;
        if (!pluginMatch && !ctkMatch) continue;

        const auto matchedSpan = it.value();
        m_pluginSpans.erase(it);
        return matchedSpan;
    }

    ActiveSpan span;
    span.spanId = QStringLiteral("plugin:%1:%2").arg(pluginId, stepKey(step));
    span.parentSpanId = QStringLiteral("startup_session");
    span.pluginId = pluginId;
    span.phaseKey = pluginId;
    span.phaseLabel = stepKey(step);
    span.step = step;
    span.startedAtMs = currentTimestampMs();
    return span;
}
