#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class MainInterfaceEntryApiContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_interface_api_does_not_expose_external_welcome_shell_mode();

private:
    QString readSource(const QString& relativePath) const;
};

QString MainInterfaceEntryApiContractTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("无法读取源文件: %1").arg(relativePath)), __FILE__, __LINE__);
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
        "MainInterfaceFactory.h 不应再暴露 external welcome shell 开关");
    QVERIFY2(!widgetHeader.contains(QStringLiteral("useExternalWelcomeShell")),
        "MainInterfaceWidget.h 不应再暴露 external welcome shell 开关");
    QVERIFY2(!factorySource.contains(QStringLiteral("useExternalWelcomeShell")),
        "MainInterfaceFactory.cpp 不应再保留 external welcome shell 分支");
    QVERIFY2(!widgetSource.contains(QStringLiteral("m_useExternalWelcomeShell")),
        "MainInterfaceWidget.cpp 不应再保留 external welcome shell 运行分支");
}

QTEST_APPLESS_MAIN(MainInterfaceEntryApiContractTest)
#include "MainInterfaceEntryApiContractTest.moc"
