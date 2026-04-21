#ifndef STARTUPBOOTSTRAPCONTROLLER_H
#define STARTUPBOOTSTRAPCONTROLLER_H

#include "FrameworkExport.h"
#include "Framework/Platform/Contracts/StartupShellSnapshot.h"

#include <QObject>

class FRAMEWORK_EXPORT StartupBootstrapController : public QObject
{
    Q_OBJECT

public:
    explicit StartupBootstrapController(QObject* parent = nullptr);

    const StartupShellSnapshot& snapshot() const;

    void beginBoot(
        const QString& stageLabel,
        const QString& statusText = QStringLiteral("系统初始化中"));
    void updateBootStage(const QString& stageLabel, const QString& statusText);
    void markReady();
    void markFailed(const QString& failureReason, const QStringList& recoveryHints);
    void resetForRetry();

signals:
    void snapshotChanged(const StartupShellSnapshot& snapshot);
    void readyToEnter();

private:
    void publishSnapshot();

    StartupShellSnapshot m_snapshot;
};

#endif // STARTUPBOOTSTRAPCONTROLLER_H
