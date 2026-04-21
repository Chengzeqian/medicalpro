#include "Framework/Platform/Bootstrap/StartupBootstrapController.h"

#include <QMetaType>

StartupBootstrapController::StartupBootstrapController(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<StartupShellSnapshot>("StartupShellSnapshot");
    resetForRetry();
}

const StartupShellSnapshot& StartupBootstrapController::snapshot() const
{
    return m_snapshot;
}

void StartupBootstrapController::beginBoot(const QString& stageLabel, const QString& statusText)
{
    m_snapshot.state = StartupShellState::Booting;
    m_snapshot.canEnterSystem = false;
    m_snapshot.frameworkReady = false;
    m_snapshot.managedScopeReady = false;
    m_snapshot.stageLabel = stageLabel;
    m_snapshot.statusText = statusText;
    m_snapshot.failureReason.clear();
    m_snapshot.recoveryHints.clear();
    publishSnapshot();
}

void StartupBootstrapController::updateBootStage(const QString& stageLabel, const QString& statusText)
{
    m_snapshot.stageLabel = stageLabel;
    m_snapshot.statusText = statusText;
    publishSnapshot();
}

void StartupBootstrapController::markReady()
{
    m_snapshot.state = StartupShellState::Ready;
    m_snapshot.canEnterSystem = true;
    m_snapshot.frameworkReady = true;
    m_snapshot.managedScopeReady = true;
    m_snapshot.statusText = QStringLiteral("主流程可进入");
    publishSnapshot();
    emit readyToEnter();
}

void StartupBootstrapController::markFailed(const QString& failureReason, const QStringList& recoveryHints)
{
    m_snapshot.state = StartupShellState::Failed;
    m_snapshot.canEnterSystem = false;
    m_snapshot.frameworkReady = false;
    m_snapshot.managedScopeReady = false;
    m_snapshot.statusText = QStringLiteral("初始化失败");
    m_snapshot.failureReason = failureReason;
    m_snapshot.recoveryHints = recoveryHints;
    publishSnapshot();
}

void StartupBootstrapController::resetForRetry()
{
    m_snapshot = StartupShellSnapshot{};
    m_snapshot.state = StartupShellState::Booting;
    m_snapshot.statusText = QStringLiteral("系统初始化中");
    m_snapshot.canEnterSystem = false;
    publishSnapshot();
}

void StartupBootstrapController::publishSnapshot()
{
    emit snapshotChanged(m_snapshot);
}
