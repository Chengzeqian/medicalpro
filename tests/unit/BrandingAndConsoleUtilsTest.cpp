#include <QtTest/QtTest>

#include <QFile>
#include <QTemporaryDir>

#include "Framework/ConsoleLogBridge.h"
#include "UI/NewPages/WelcomeBrandingUtils.h"

class BrandingAndConsoleUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void buildLogoCandidatePaths_prefersRuntimeOverrideOrder();
    void resolveLogoPath_fallsBackToBundledResource();
    void buildFallbackBrandText_reportsFormalWordmark();
    void resolveOutputMode_prefersConsoleWhenStdHandleIsConsole();
    void resolveOutputMode_prefersRedirectedWhenStdHandleIsDisk();
    void resolveOutputMode_treatsAttachedConsolePipeAsConsole();
    void resolveOutputMode_fallsBackToRedirectedWithoutAttachedConsole();
    void formatLogLine_includesLevelMessageAndContext();
    void buildRedirectedOutputBytes_usesUtf8WithLineFeed();
    void buildConsolePipeOutputBytes_usesConsoleCodePageEncoding();
};

void BrandingAndConsoleUtilsTest::buildLogoCandidatePaths_prefersRuntimeOverrideOrder()
{
    const QString appDir = QStringLiteral("D:/Qtproject/medicalpro/build_x64/Release");
    const QStringList candidatePaths = WelcomeBrandingUtils::buildLogoCandidatePaths(appDir);

    QCOMPARE(candidatePaths.size(), 3);
    QCOMPARE(candidatePaths.at(0), appDir + QStringLiteral("/data/branding/welcome_logo.png"));
    QCOMPARE(candidatePaths.at(1), appDir + QStringLiteral("/data/logo.png"));
    QCOMPARE(candidatePaths.at(2), QStringLiteral(":/resoucce/logo.png"));
}

void BrandingAndConsoleUtilsTest::resolveLogoPath_fallsBackToBundledResource()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(QFile::exists(QStringLiteral(":/resoucce/logo.png")));

    QCOMPARE(
        WelcomeBrandingUtils::resolveLogoPath(tempDir.path()),
        QStringLiteral(":/resoucce/logo.png"));
}

void BrandingAndConsoleUtilsTest::buildFallbackBrandText_reportsFormalWordmark()
{
    QCOMPARE(
        WelcomeBrandingUtils::buildFallbackBrandText(),
        QStringLiteral("MedicalPro"));
}

void BrandingAndConsoleUtilsTest::resolveOutputMode_prefersConsoleWhenStdHandleIsConsole()
{
    QCOMPARE(
        ConsoleLogBridge::resolveOutputMode(true, false, false),
        ConsoleLogBridge::OutputMode::Console);
}

void BrandingAndConsoleUtilsTest::resolveOutputMode_prefersRedirectedWhenStdHandleIsDisk()
{
    QCOMPARE(
        ConsoleLogBridge::resolveOutputMode(false, true, true),
        ConsoleLogBridge::OutputMode::Redirected);
}

void BrandingAndConsoleUtilsTest::resolveOutputMode_treatsAttachedConsolePipeAsConsole()
{
    QCOMPARE(
        ConsoleLogBridge::resolveOutputMode(false, false, true),
        ConsoleLogBridge::OutputMode::Console);
}

void BrandingAndConsoleUtilsTest::resolveOutputMode_fallsBackToRedirectedWithoutAttachedConsole()
{
    QCOMPARE(
        ConsoleLogBridge::resolveOutputMode(false, false, false),
        ConsoleLogBridge::OutputMode::Redirected);
}

void BrandingAndConsoleUtilsTest::formatLogLine_includesLevelMessageAndContext()
{
    const QMessageLogContext context("main.cpp", 42, "main", "default");
    const QString formattedLine = ConsoleLogBridge::formatLogLine(
        QtWarningMsg,
        context,
        QStringLiteral("startup log"));

    QVERIFY(formattedLine.contains(QStringLiteral("[WARNING]")));
    QVERIFY(formattedLine.contains(QStringLiteral("startup log")));
    QVERIFY(formattedLine.contains(QStringLiteral("main.cpp:42")));
}

void BrandingAndConsoleUtilsTest::buildRedirectedOutputBytes_usesUtf8WithLineFeed()
{
    const QByteArray bytes = ConsoleLogBridge::buildRedirectedOutputBytes(
        QStringLiteral("[INFO] startup log"));

    QCOMPARE(bytes, QStringLiteral("[INFO] startup log\n").toUtf8());
}

void BrandingAndConsoleUtilsTest::buildConsolePipeOutputBytes_usesConsoleCodePageEncoding()
{
    const QByteArray bytes = ConsoleLogBridge::buildConsolePipeOutputBytes(
        QStringLiteral("[DEBUG] 应用程序启动"),
        936);

    QCOMPARE(
        bytes.toHex().toUpper(),
        QByteArray("5B44454255475D20D3A6D3C3B3CCD0F2C6F4B6AF0A"));
}

QTEST_APPLESS_MAIN(BrandingAndConsoleUtilsTest)

#include "BrandingAndConsoleUtilsTest.moc"
