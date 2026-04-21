#include "UI/StartupShell.h"

#include <QVBoxLayout>

#include "UI/NewPages/WelcomePage.h"

StartupShell::StartupShell(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_welcomePage = new WelcomePageNew(this);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_welcomePage, 1);

    connect(m_welcomePage, &WelcomePageNew::enterSystemRequested, this, &StartupShell::enterSystemRequested);
    connect(m_welcomePage, &WelcomePageNew::retryStartupRequested, this, &StartupShell::retryStartupRequested);
    connect(m_welcomePage, &WelcomePageNew::exitRequested, this, &StartupShell::exitRequested);
}

void StartupShell::applySnapshot(const StartupShellSnapshot& snapshot)
{
    m_welcomePage->applyStartupShellSnapshot(snapshot);
}
