#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class MainInterfaceEntryApiContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_interface_api_does_not_expose_external_welcome_shell_mode();
    void main_interface_api_threads_real_case_workspace_seed_into_navigation_entry();

private:
    QString readSource(const QString& relativePath) const;
};

QString MainInterfaceEntryApiContractTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("Cannot read source file: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void MainInterfaceEntryApiContractTest::main_interface_api_does_not_expose_external_welcome_shell_mode()
{
    const QString factoryHeader = readSource(QStringLiteral("UI/MainInterfaceFactory.h"));
    const QString widgetHeader = readSource(QStringLiteral("UI/MainInterfaceWidget.h"));
    const QString factorySource = readSource(QStringLiteral("UI/MainInterfaceFactory.cpp"));
    const QString widgetSource = readSource(QStringLiteral("UI/MainInterfaceWidget.cpp"));

    QVERIFY2(!factoryHeader.contains(QStringLiteral("useExternalWelcomeShell")),
        "MainInterfaceFactory.h must not expose external welcome shell mode");
    QVERIFY2(!widgetHeader.contains(QStringLiteral("useExternalWelcomeShell")),
        "MainInterfaceWidget.h must not expose external welcome shell mode");
    QVERIFY2(!factorySource.contains(QStringLiteral("useExternalWelcomeShell")),
        "MainInterfaceFactory.cpp must not keep external welcome shell branches");
    QVERIFY2(!widgetSource.contains(QStringLiteral("m_useExternalWelcomeShell")),
        "MainInterfaceWidget.cpp must not keep external welcome shell state");
}

void MainInterfaceEntryApiContractTest::main_interface_api_threads_real_case_workspace_seed_into_navigation_entry()
{
    const QString factoryHeader = readSource(QStringLiteral("UI/MainInterfaceFactory.h"));
    const QString widgetHeader = readSource(QStringLiteral("UI/MainInterfaceWidget.h"));
    const QString factorySource = readSource(QStringLiteral("UI/MainInterfaceFactory.cpp"));
    const QString widgetSource = readSource(QStringLiteral("UI/MainInterfaceWidget.cpp"));
    const QString mainSource = readSource(QStringLiteral("main.cpp"));

    QVERIFY2(factoryHeader.contains(QStringLiteral("const RealCaseWorkspaceSeed& realCaseWorkspaceSeed")),
        "MainInterfaceFactory.h must expose the real case workspace seed parameter");
    QVERIFY2(widgetHeader.contains(QStringLiteral("const RealCaseWorkspaceSeed& realCaseWorkspaceSeed")),
        "MainInterfaceWidget.h must expose the real case workspace seed parameter");
    QVERIFY2(factorySource.contains(QStringLiteral("realCaseWorkspaceSeed")),
        "MainInterfaceFactory.cpp must thread the real case workspace seed into MainInterfaceWidget");
    QVERIFY2(widgetSource.contains(QStringLiteral("RealCaseWorkspaceSeedCoordinator")),
        "MainInterfaceWidget.cpp must depend on RealCaseWorkspaceSeedCoordinator");
    QVERIFY2(widgetSource.contains(QStringLiteral("ensureRealCaseWorkspaceSeeded()")),
        "MainInterfaceWidget.cpp must provide workspace seed bootstrapping");
    QVERIFY2(widgetSource.contains(QStringLiteral("m_realCaseWorkspaceSeed.caseId")),
        "MainInterfaceWidget.cpp must prefer the configured real case id");
    QVERIFY2(mainSource.contains(QStringLiteral("runtimeConfig.realCaseWorkspaceSeed")),
        "main.cpp must pass runtimeConfig.realCaseWorkspaceSeed into main interface creation");
}

QTEST_APPLESS_MAIN(MainInterfaceEntryApiContractTest)
#include "MainInterfaceEntryApiContractTest.moc"
