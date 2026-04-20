#include <QtTest>

#include <QLabel>
#include <QTableWidget>

#include "UI/NewPages/PlatformDiagnosticsPage.h"

class PlatformDiagnosticsPageTest : public QObject
{
    Q_OBJECT

private slots:
    void refreshSnapshot_renders_mode_plugin_and_trace();
};

void PlatformDiagnosticsPageTest::refreshSnapshot_renders_mode_plugin_and_trace()
{
    PlatformDiagnosticSnapshot snapshot;
    snapshot.runtimeMode = PlatformRuntimeMode::ObserveOnly;
    snapshot.frameworkReady = true;
    snapshot.startupTrace = {
        {QStringLiteral("critical_start"), QStringLiteral("Start core plugins"), true, 540, QStringLiteral("ok")}
    };

    PlatformPluginRuntimeSnapshot pluginSnapshot;
    pluginSnapshot.pluginId = QStringLiteral("org.medicalpro.dicom_viewer");
    pluginSnapshot.ctkSymbolicName = QStringLiteral("DicomViewer");
    pluginSnapshot.state = PlatformPluginState::Ready;
    snapshot.plugins.append(pluginSnapshot);

    PlatformDiagnosticsPage page(nullptr, [snapshot]() { return snapshot; });
    page.refreshSnapshot();

    QCOMPARE(page.findChild<QLabel*>(QStringLiteral("runtimeModeValueLabel"))->text(), QStringLiteral("observe_only"));
    QCOMPARE(page.findChild<QTableWidget*>(QStringLiteral("pluginTableWidget"))->rowCount(), 1);
    QCOMPARE(page.findChild<QTableWidget*>(QStringLiteral("traceTableWidget"))->rowCount(), 1);
}

QTEST_MAIN(PlatformDiagnosticsPageTest)
#include "PlatformDiagnosticsPageTest.moc"
