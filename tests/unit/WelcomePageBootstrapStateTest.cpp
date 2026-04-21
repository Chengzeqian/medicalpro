#include <QtTest/QtTest>

#include <QLabel>
#include <QPushButton>

#include "Framework/Platform/Contracts/StartupShellSnapshot.h"
#include "UI/NewPages/WelcomePage.h"

class WelcomePageBootstrapStateTest : public QObject
{
    Q_OBJECT

private slots:
    void applyStartupShellSnapshot_disables_enter_while_booting();
    void applyStartupShellSnapshot_enables_enter_when_ready();
    void applyStartupShellSnapshot_surfaces_failure_reason();
};

void WelcomePageBootstrapStateTest::applyStartupShellSnapshot_disables_enter_while_booting()
{
    WelcomePageNew page;
    page.onActivated();

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Booting;
    snapshot.canEnterSystem = false;
    snapshot.frameworkReady = false;
    snapshot.managedScopeReady = false;
    snapshot.stageLabel = QStringLiteral("CTK framework initialization");
    snapshot.statusText = QStringLiteral("系统初始化中");

    page.applyStartupShellSnapshot(snapshot);

    auto* enterButton = page.findChild<QPushButton*>(QStringLiteral("enterButton"));
    auto* runtimeDecisionBadge = page.findChild<QLabel*>(QStringLiteral("runtimeDecisionBadge"));

    QVERIFY2(enterButton != nullptr, "enterButton not found");
    QVERIFY2(runtimeDecisionBadge != nullptr, "runtimeDecisionBadge not found");
    QVERIFY(!enterButton->isEnabled());
    QCOMPARE(runtimeDecisionBadge->text(), QStringLiteral("系统初始化中"));
}

void WelcomePageBootstrapStateTest::applyStartupShellSnapshot_enables_enter_when_ready()
{
    WelcomePageNew page;
    page.onActivated();

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Ready;
    snapshot.canEnterSystem = true;
    snapshot.frameworkReady = true;
    snapshot.managedScopeReady = true;
    snapshot.statusText = QStringLiteral("主流程可进入");

    page.applyStartupShellSnapshot(snapshot);

    auto* enterButton = page.findChild<QPushButton*>(QStringLiteral("enterButton"));
    QVERIFY2(enterButton != nullptr, "enterButton not found");
    QVERIFY(enterButton->isEnabled());
}

void WelcomePageBootstrapStateTest::applyStartupShellSnapshot_surfaces_failure_reason()
{
    WelcomePageNew page;
    page.onActivated();

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Failed;
    snapshot.canEnterSystem = false;
    snapshot.failureReason = QStringLiteral("Critical plugin activation failed");
    snapshot.recoveryHints = QStringList{QStringLiteral("Retry startup or inspect diagnostics.")};
    snapshot.statusText = QStringLiteral("初始化失败");

    page.applyStartupShellSnapshot(snapshot);

    auto* runtimeSummaryTextLabel = page.findChild<QLabel*>(QStringLiteral("runtimeSummaryTextLabel"));
    QVERIFY2(runtimeSummaryTextLabel != nullptr, "runtimeSummaryTextLabel not found");
    QVERIFY(runtimeSummaryTextLabel->text().contains(QStringLiteral("Critical plugin activation failed")));
}

QTEST_MAIN(WelcomePageBootstrapStateTest)

#include "WelcomePageBootstrapStateTest.moc"
