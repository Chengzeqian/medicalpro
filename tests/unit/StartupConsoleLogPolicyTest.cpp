#include <QtTest/QtTest>

#include <QFile>

class StartupConsoleLogPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void mainStartupLogs_useAsciiMessages();
    void orchestratorDiagnostics_useAsciiMessages();
    void vtkWidgetFactoryLogs_useAsciiMessages();
};

namespace
{

QString readSourceFile(const QString& relativePath)
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    return QString::fromUtf8(file.readAll());
}

} // namespace

void StartupConsoleLogPolicyTest::mainStartupLogs_useAsciiMessages()
{
    const QString source = readSourceFile(QStringLiteral("main.cpp"));

    QVERIFY2(!source.isEmpty(), "Failed to load main.cpp");
    QVERIFY(source.contains(QStringLiteral("Medical Pro application startup")));
    QVERIFY(source.contains(QStringLiteral("[Phase 1] QApplication + VTK initialization")));
    QVERIFY(source.contains(QStringLiteral("[main] Configuring Qt OpenGL backend...")));
    QVERIFY(source.contains(QStringLiteral("[StartupOrchestrator] Starting critical plugin activation (synchronous)...")));
}

void StartupConsoleLogPolicyTest::orchestratorDiagnostics_useAsciiMessages()
{
    const QString source = readSourceFile(QStringLiteral("Framework/StartupOrchestrator.cpp"));

    QVERIFY2(!source.isEmpty(), "Failed to load StartupOrchestrator.cpp");
    QVERIFY(source.contains(QStringLiteral("VTK initialization")));
    QVERIFY(source.contains(QStringLiteral("Startup duration: %1 ms\\n")));
    QVERIFY(source.contains(QStringLiteral("\\nPhase summary:\\n")));
    QVERIFY(source.contains(QStringLiteral("\\nDiagnostics:\\n")));
    QVERIFY(source.contains(QStringLiteral("\\nErrors:\\n")));
}

void StartupConsoleLogPolicyTest::vtkWidgetFactoryLogs_useAsciiMessages()
{
    const QString source = readSourceFile(QStringLiteral("Framework/VTKWidgetFactory.cpp"));

    QVERIFY2(!source.isEmpty(), "Failed to load VTKWidgetFactory.cpp");
    QVERIFY(source.contains(QStringLiteral("[VTKWidgetFactory] Starting VTK widget creation...")));
    QVERIFY(source.contains(QStringLiteral("[VTKWidgetFactory] Step 1: checking VTK object factory...")));
    QVERIFY(source.contains(QStringLiteral("[VTKWidgetFactory] Step 6: OpenGL context validation...")));
    QVERIFY(source.contains(QStringLiteral("[VTKWidgetFactory] VTK support is disabled, widget creation is unavailable")));
}

QTEST_APPLESS_MAIN(StartupConsoleLogPolicyTest)

#include "StartupConsoleLogPolicyTest.moc"
