#include "UI/StartupShell.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "UI/NewPages/WelcomePage.h"

StartupShell::StartupShell(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_welcomePage = new WelcomePageNew(this);
    m_failureLabel = new QLabel(this);
    m_retryButton = new QPushButton(QStringLiteral("重试启动"), this);
    m_viewDiagnosticsButton = new QPushButton(QStringLiteral("查看诊断"), this);

    m_failureLabel->setWordWrap(true);
    m_failureLabel->setVisible(false);
    m_retryButton->setVisible(false);
    m_viewDiagnosticsButton->setVisible(false);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(m_welcomePage, 1);
    layout->addWidget(m_failureLabel);
    layout->addWidget(m_retryButton);
    layout->addWidget(m_viewDiagnosticsButton);

    connect(m_welcomePage, &WelcomePageNew::enterSystemRequested, this, &StartupShell::enterSystemRequested);
    connect(m_welcomePage, &WelcomePageNew::exitRequested, this, &StartupShell::exitRequested);
    connect(m_retryButton, &QPushButton::clicked, this, &StartupShell::retryStartupRequested);
    connect(m_viewDiagnosticsButton, &QPushButton::clicked, this, &StartupShell::viewDiagnosticsRequested);
}

void StartupShell::applySnapshot(const StartupShellSnapshot& snapshot)
{
    m_welcomePage->applyStartupShellSnapshot(snapshot);
    m_failureLabel->setVisible(snapshot.state == StartupShellState::Failed);
    m_failureLabel->setText(snapshot.failureReason);
    m_retryButton->setVisible(snapshot.state == StartupShellState::Failed);
    m_viewDiagnosticsButton->setVisible(
        snapshot.state == StartupShellState::Failed || snapshot.state == StartupShellState::Booting);
}
