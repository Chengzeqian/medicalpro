#include <QtTest/QtTest>

#include <QFile>
#include <QRegularExpression>
#include <QString>

class StartupWelcomeEntrySourceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void main_cpp_does_not_route_visible_entry_through_external_shell();

private:
    QString readSource(const QString& relativePath) const;
};

QString StartupWelcomeEntrySourceContractTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("无法读取源文件: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void StartupWelcomeEntrySourceContractTest::main_cpp_does_not_route_visible_entry_through_external_shell()
{
    const QString source = readSource(QStringLiteral("main.cpp"));

    const QRegularExpression externalShellFactoryCall(
        QStringLiteral(R"(createMainInterface\s*\([\s\S]*?,\s*&bootstrapStateStore,\s*nullptr,\s*true\s*\))"));
    QVERIFY2(!source.contains(externalShellFactoryCall),
        "main.cpp 仍在通过 createMainInterface(..., true) 走外部欢迎壳入口");

    QVERIFY2(!source.contains(QStringLiteral("startupShell->showFullScreen();")),
        "main.cpp 仍在把 StartupShell 作为用户可见欢迎入口显示");

    QVERIFY2(!source.contains(QStringLiteral("returning to startup welcome shell")),
        "main.cpp 仍在把 logout 链路回退到 StartupShell");
}

QTEST_APPLESS_MAIN(StartupWelcomeEntrySourceContractTest)
#include "StartupWelcomeEntrySourceContractTest.moc"
