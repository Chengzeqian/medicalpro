#include <QtTest>

#include "Framework/Platform/Diagnostics/PlatformDiagnosticsService.h"
#include "Framework/Platform/Kernel/PlatformStateStore.h"

class PlatformDiagnosticsServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void buildSnapshot_includes_mode_trace_and_recovery_hint();
};

void PlatformDiagnosticsServiceTest::buildSnapshot_includes_mode_trace_and_recovery_hint()
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = QStringLiteral("org.medicalpro.dicom_viewer");
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = QStringLiteral("DicomViewer");
    descriptor.domain = QStringLiteral("imaging");
    descriptor.runtime.ctkSymbolicName = QStringLiteral("DicomViewer");
    descriptor.provides.capabilities = QStringList{QStringLiteral("imaging.data")};

    PlatformStateStore store;
    store.replaceDescriptors({descriptor});
    store.setRuntimeMode(PlatformRuntimeMode::ObserveOnly);

    PlatformRuntimeObservation observation;
    observation.frameworkReady = true;
    observation.installedPlugins = QStringList{QStringLiteral("DicomViewer")};
    observation.startupTrace = {
        {QStringLiteral("plugin_install"), QStringLiteral("Install plugins"), true, 210, QStringLiteral("installed 5 plugins")},
        {QStringLiteral("critical_start"), QStringLiteral("Start core plugins"), false, 820, QStringLiteral("DicomViewerService missing")}
    };

    PlatformDiagnosticsService service(&store);
    const auto snapshot = service.buildSnapshot(observation);

    QCOMPARE(snapshot.runtimeMode, PlatformRuntimeMode::ObserveOnly);
    QVERIFY(snapshot.frameworkReady);
    QCOMPARE(snapshot.startupTrace.size(), 2);
    QVERIFY(snapshot.recoveryHints.join(QStringLiteral(" | ")).contains(QStringLiteral("DicomViewerService")));
}

QTEST_APPLESS_MAIN(PlatformDiagnosticsServiceTest)
#include "PlatformDiagnosticsServiceTest.moc"
