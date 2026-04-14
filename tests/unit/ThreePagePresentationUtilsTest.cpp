#include <QtTest/QtTest>

#include "UI/NewPages/ThreePagePresentationUtils.h"

class ThreePagePresentationUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void buildFrameworkSummary_reportsReady();
    void buildFrameworkSummary_reportsNotReady();
    void buildServiceGroupSummary_reportsRatio();
    void buildDirectorySummary_reportsReadable();
    void buildDirectorySummary_reportsMissing();
    void buildDirectorySummary_reportsNotReadable();
    void buildToneName_reportsWarning();
    void buildToneName_reportsDangerWhenNoReady();
    void buildToneName_reportsOkWhenAllReady();
    void buildWelcomeRuntimeSummary_reportsReady();
    void buildWelcomeRuntimeSummary_reportsWarning();
    void buildWelcomeRuntimeSummary_reportsBlocked();
    void buildWelcomeDecisionLabel_reportsReady();
    void buildWelcomeDecisionLabel_reportsWarning();
    void buildWelcomeDecisionLabel_reportsBlocked();
    void buildWelcomeDecisionTone_reportsReady();
    void buildWelcomeDecisionTone_reportsWarning();
    void buildWelcomeDecisionTone_reportsBlocked();
    void buildModuleStatusSummary_reportsReady();
    void buildModuleStatusSummary_reportsPending();
    void buildDashboardDicomSummary_reportsCount();
    void formatModuleTimestamp_reportsReadableText();
};

void ThreePagePresentationUtilsTest::buildFrameworkSummary_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildFrameworkSummary(true, 6),
        QStringLiteral("插件框架已联通，当前识别到 6 个插件。"));
}

void ThreePagePresentationUtilsTest::buildFrameworkSummary_reportsNotReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildFrameworkSummary(false, 0),
        QStringLiteral("插件框架尚未就绪，请先检查启动日志。"));
}

void ThreePagePresentationUtilsTest::buildServiceGroupSummary_reportsRatio()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildServiceGroupSummary(2, 3),
        QStringLiteral("2/3 个关键服务已就绪。"));
}

void ThreePagePresentationUtilsTest::buildDirectorySummary_reportsReadable()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildDirectorySummary(true, true, QStringLiteral("data")),
        QStringLiteral("data 目录存在且可访问。"));
}

void ThreePagePresentationUtilsTest::buildDirectorySummary_reportsMissing()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildDirectorySummary(false, false, QStringLiteral("data")),
        QStringLiteral("data 目录缺失。"));
}

void ThreePagePresentationUtilsTest::buildDirectorySummary_reportsNotReadable()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildDirectorySummary(true, false, QStringLiteral("data")),
        QStringLiteral("data 目录存在但不可访问。"));
}

void ThreePagePresentationUtilsTest::buildToneName_reportsWarning()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildToneName(2, 3),
        QStringLiteral("warning"));
}

void ThreePagePresentationUtilsTest::buildToneName_reportsDangerWhenNoReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildToneName(0, 1),
        QStringLiteral("danger"));
}

void ThreePagePresentationUtilsTest::buildToneName_reportsOkWhenAllReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildToneName(3, 3),
        QStringLiteral("ok"));
}

void ThreePagePresentationUtilsTest::buildWelcomeRuntimeSummary_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeRuntimeSummary(true, 3, 3, true),
        QStringLiteral("插件框架已联通，关键服务持续自检中，系统当前处于可进入状态。"));
}

void ThreePagePresentationUtilsTest::buildWelcomeRuntimeSummary_reportsWarning()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeRuntimeSummary(true, 2, 3, true),
        QStringLiteral("插件框架已联通，部分关键服务仍在检查，系统当前可进入但建议先确认状态。"));
}

void ThreePagePresentationUtilsTest::buildWelcomeRuntimeSummary_reportsBlocked()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeRuntimeSummary(false, 0, 3, false),
        QStringLiteral("插件框架或关键依赖仍在检查，当前暂不建议进入主流程。"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionLabel_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionLabel(true, 3, 3, true),
        QStringLiteral("主流程可进入"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionLabel_reportsWarning()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionLabel(true, 2, 3, true),
        QStringLiteral("可进入，建议检查"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionLabel_reportsBlocked()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionLabel(false, 0, 3, false),
        QStringLiteral("暂不建议进入"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionTone_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionTone(true, 3, 3, true),
        QStringLiteral("ok"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionTone_reportsWarning()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionTone(true, 2, 3, true),
        QStringLiteral("warning"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionTone_reportsBlocked()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionTone(false, 0, 3, false),
        QStringLiteral("danger"));
}

void ThreePagePresentationUtilsTest::buildModuleStatusSummary_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildModuleStatusSummary(true),
        QStringLiteral("主流程可进入"));
}

void ThreePagePresentationUtilsTest::buildModuleStatusSummary_reportsPending()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildModuleStatusSummary(false),
        QStringLiteral("部分依赖待确认"));
}

void ThreePagePresentationUtilsTest::buildDashboardDicomSummary_reportsCount()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildDashboardDicomSummary(5),
        QStringLiteral("当前病例包含 5 组 DICOM 检查。"));
}

void ThreePagePresentationUtilsTest::formatModuleTimestamp_reportsReadableText()
{
    const QDateTime dateTime(QDate(2026, 4, 14), QTime(9, 30, 0));
    QCOMPARE(
        ThreePagePresentationUtils::formatModuleTimestamp(dateTime),
        QStringLiteral("2026-04-14 09:30"));
}

QTEST_APPLESS_MAIN(ThreePagePresentationUtilsTest)

#include "ThreePagePresentationUtilsTest.moc"
