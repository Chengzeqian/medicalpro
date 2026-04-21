#ifndef STARTUPSHELL_H
#define STARTUPSHELL_H

#include "Framework/Platform/Contracts/StartupShellSnapshot.h"

#include <QWidget>

class WelcomePageNew;

class StartupShell : public QWidget
{
    Q_OBJECT

public:
    explicit StartupShell(QWidget* parent = nullptr);

    void applySnapshot(const StartupShellSnapshot& snapshot);

signals:
    void enterSystemRequested();
    void retryStartupRequested();
    void viewDiagnosticsRequested();
    void exitRequested();

private:
    WelcomePageNew* m_welcomePage = nullptr;
};

#endif // STARTUPSHELL_H
