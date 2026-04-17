#include <QtTest/QtTest>

#include <QLabel>
#include <QMetaObject>

#include "UI/NewPages/WelcomePage.h"

class WelcomePageRuntimeRefreshTest : public QObject
{
    Q_OBJECT

private slots:
    void refreshTrigger_updatesRuntimeSnapshot();
};

void WelcomePageRuntimeRefreshTest::refreshTrigger_updatesRuntimeSnapshot()
{
    WelcomePageNew::RuntimeStatusSnapshot snapshot{
        false,
        0,
        0,
        3,
        true,
        true,
        {
            QStringLiteral("UserManagementService"),
            QStringLiteral("DicomViewerService"),
            QStringLiteral("FourViewDisplayService")
        }
    };

    WelcomePageNew page(
        nullptr,
        [&snapshot]() {
            return snapshot;
        });

    page.onActivated();

    auto* frameworkQuickValueLabel = page.findChild<QLabel*>(QStringLiteral("frameworkQuickValueLabel"));
    auto* runtimeDecisionBadge = page.findChild<QLabel*>(QStringLiteral("runtimeDecisionBadge"));
    auto* pluginFrameworkStateLabel = page.findChild<QLabel*>(QStringLiteral("pluginFrameworkStateLabel"));

    QVERIFY2(frameworkQuickValueLabel != nullptr, "frameworkQuickValueLabel not found");
    QVERIFY2(runtimeDecisionBadge != nullptr, "runtimeDecisionBadge not found");
    QVERIFY2(pluginFrameworkStateLabel != nullptr, "pluginFrameworkStateLabel not found");

    QCOMPARE(frameworkQuickValueLabel->text(), QStringLiteral("已识别 0 个插件"));
    QCOMPARE(runtimeDecisionBadge->text(), QStringLiteral("暂不建议进入"));
    QCOMPARE(pluginFrameworkStateLabel->text(), QStringLiteral("框架未就绪"));

    snapshot.frameworkReady = true;
    snapshot.pluginCount = 6;
    snapshot.readyServices = 3;
    snapshot.totalServices = 3;
    snapshot.missingServices.clear();

    QVERIFY(QMetaObject::invokeMethod(&page, "scheduleRuntimeStatusRefresh", Qt::DirectConnection));

    QTRY_COMPARE(frameworkQuickValueLabel->text(), QStringLiteral("已识别 6 个插件"));
    QTRY_COMPARE(runtimeDecisionBadge->text(), QStringLiteral("主流程可进入"));
    QTRY_COMPARE(pluginFrameworkStateLabel->text(), QStringLiteral("框架已初始化"));
}

QTEST_MAIN(WelcomePageRuntimeRefreshTest)

#include "WelcomePageRuntimeRefreshTest.moc"
