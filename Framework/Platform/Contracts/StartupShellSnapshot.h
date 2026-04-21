#ifndef STARTUPSHELLSNAPSHOT_H
#define STARTUPSHELLSNAPSHOT_H

#include <QString>
#include <QStringList>

enum class StartupShellState
{
    Booting,
    Ready,
    Failed
};

struct StartupShellSnapshot
{
    StartupShellState state = StartupShellState::Booting;
    bool canEnterSystem = false;
    bool frameworkReady = false;
    bool managedScopeReady = false;
    bool dataDirectoryReadable = false;
    QString stageKey;
    QString stageLabel;
    QString statusText;
    QString failureReason;
    QStringList recoveryHints;
};

#endif // STARTUPSHELLSNAPSHOT_H
