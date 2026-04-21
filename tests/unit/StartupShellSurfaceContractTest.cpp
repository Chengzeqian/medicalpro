#include <QtTest/QtTest>

#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>

#include "Framework/Platform/Contracts/StartupShellSnapshot.h"
#include "UI/NewPages/WelcomePage.h"
#include "UI/StartupShell.h"

class StartupShellSurfaceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void bootingSnapshot_keepsWelcomeAsOnlyDirectSurface();
    void failedSnapshot_routesPrimaryActionToRetry();
};

void StartupShellSurfaceContractTest::bootingSnapshot_keepsWelcomeAsOnlyDirectSurface()
{
    StartupShell shell;

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Booting;
    snapshot.canEnterSystem = false;
    snapshot.statusText = QStringLiteral("Booting managed startup scope");

    shell.applySnapshot(snapshot);

    auto* welcomePage = shell.findChild<WelcomePageNew*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY2(welcomePage != nullptr, "welcome page should remain the only direct startup surface");

    const auto directButtons = shell.findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
    const auto directLabels = shell.findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);

    QCOMPARE(directButtons.size(), 0);
    QCOMPARE(directLabels.size(), 0);
}

void StartupShellSurfaceContractTest::failedSnapshot_routesPrimaryActionToRetry()
{
    StartupShell shell;
    QSignalSpy retrySpy(&shell, &StartupShell::retryStartupRequested);

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Failed;
    snapshot.canEnterSystem = false;
    snapshot.failureReason = QStringLiteral("Managed startup scope failed");
    snapshot.recoveryHints = QStringList{QStringLiteral("Retry startup after checking diagnostics.")};
    snapshot.statusText = QStringLiteral("Startup failed");

    shell.applySnapshot(snapshot);

    auto* welcomePage = shell.findChild<WelcomePageNew*>();
    QVERIFY2(welcomePage != nullptr, "welcome page not found");

    auto* enterButton = welcomePage->findChild<QPushButton*>(QStringLiteral("enterButton"));
    QVERIFY2(enterButton != nullptr, "enterButton not found");
    QCOMPARE(enterButton->text(), QStringLiteral("重试启动"));

    QTest::mouseClick(enterButton, Qt::LeftButton);
    QCOMPARE(retrySpy.count(), 1);
}

QTEST_MAIN(StartupShellSurfaceContractTest)

#include "StartupShellSurfaceContractTest.moc"
