#include <QtTest/QtTest>

#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>

#include "Framework/Platform/Contracts/StartupShellSnapshot.h"
#include "UI/NewPages/WelcomePage.h"

class WelcomePageBootstrapStateTest : public QObject
{
    Q_OBJECT

private slots:
    void applyStartupShellSnapshot_disablesEnterWhileBooting();
    void applyStartupShellSnapshot_enablesEnterWhenReady();
    void applyStartupShellSnapshot_surfacesFailureReason();
    void applyStartupShellSnapshot_failedStateReusesPrimaryActionForRetry();
};

void WelcomePageBootstrapStateTest::applyStartupShellSnapshot_disablesEnterWhileBooting()
{
    WelcomePageNew page;
    page.onActivated();

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Booting;
    snapshot.canEnterSystem = false;
    snapshot.frameworkReady = false;
    snapshot.managedScopeReady = false;
    snapshot.stageLabel = QStringLiteral("CTK framework initialization");
    snapshot.statusText = QStringLiteral("Booting managed startup scope");

    page.applyStartupShellSnapshot(snapshot);

    auto* enterButton = page.findChild<QPushButton*>(QStringLiteral("enterButton"));
    auto* runtimeDecisionBadge = page.findChild<QLabel*>(QStringLiteral("runtimeDecisionBadge"));

    QVERIFY2(enterButton != nullptr, "enterButton not found");
    QVERIFY2(runtimeDecisionBadge != nullptr, "runtimeDecisionBadge not found");
    QVERIFY(!enterButton->isEnabled());
    QCOMPARE(enterButton->text(), QStringLiteral("系统初始化中"));
    QCOMPARE(runtimeDecisionBadge->text(), QStringLiteral("Booting managed startup scope"));
}

void WelcomePageBootstrapStateTest::applyStartupShellSnapshot_enablesEnterWhenReady()
{
    WelcomePageNew page;
    page.onActivated();

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Ready;
    snapshot.canEnterSystem = true;
    snapshot.frameworkReady = true;
    snapshot.managedScopeReady = true;
    snapshot.statusText = QStringLiteral("Primary workflow ready");

    page.applyStartupShellSnapshot(snapshot);

    auto* enterButton = page.findChild<QPushButton*>(QStringLiteral("enterButton"));
    QVERIFY2(enterButton != nullptr, "enterButton not found");
    QVERIFY(enterButton->isEnabled());
    QCOMPARE(enterButton->text(), QStringLiteral("进入系统"));
}

void WelcomePageBootstrapStateTest::applyStartupShellSnapshot_surfacesFailureReason()
{
    WelcomePageNew page;
    page.onActivated();

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Failed;
    snapshot.canEnterSystem = false;
    snapshot.failureReason = QStringLiteral("Critical plugin activation failed");
    snapshot.recoveryHints = QStringList{QStringLiteral("Retry startup or inspect diagnostics.")};
    snapshot.statusText = QStringLiteral("Startup failed");

    page.applyStartupShellSnapshot(snapshot);

    auto* runtimeSummaryTextLabel = page.findChild<QLabel*>(QStringLiteral("runtimeSummaryTextLabel"));
    QVERIFY2(runtimeSummaryTextLabel != nullptr, "runtimeSummaryTextLabel not found");
    QVERIFY(runtimeSummaryTextLabel->text().contains(QStringLiteral("Critical plugin activation failed")));
}

void WelcomePageBootstrapStateTest::applyStartupShellSnapshot_failedStateReusesPrimaryActionForRetry()
{
    WelcomePageNew page;
    page.onActivated();

    StartupShellSnapshot snapshot;
    snapshot.state = StartupShellState::Failed;
    snapshot.canEnterSystem = false;
    snapshot.failureReason = QStringLiteral("Managed startup scope failed");
    snapshot.recoveryHints = QStringList{QStringLiteral("Retry startup after checking diagnostics.")};
    snapshot.statusText = QStringLiteral("Startup failed");

    page.applyStartupShellSnapshot(snapshot);

    auto* enterButton = page.findChild<QPushButton*>(QStringLiteral("enterButton"));
    QVERIFY2(enterButton != nullptr, "enterButton not found");
    QVERIFY(enterButton->isEnabled());
    QCOMPARE(enterButton->text(), QStringLiteral("重试启动"));

    QSignalSpy retrySpy(&page, &WelcomePageNew::retryStartupRequested);
    QSignalSpy enterSpy(&page, &WelcomePageNew::enterSystemRequested);
    QTest::mouseClick(enterButton, Qt::LeftButton);

    QCOMPARE(retrySpy.count(), 1);
    QCOMPARE(enterSpy.count(), 0);
}

QTEST_MAIN(WelcomePageBootstrapStateTest)

#include "WelcomePageBootstrapStateTest.moc"
