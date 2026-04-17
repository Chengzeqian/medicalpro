#include <QtTest/QtTest>

#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformDependencyGraph.h"
#include "Framework/Platform/Kernel/PlatformStateStore.h"

namespace
{
PlatformPluginDescriptor makeDescriptor(
    const QString& id,
    const QString& symbolicName,
    PlatformBootstrapLevel bootstrapLevel,
    PlatformStartupPolicy startupPolicy,
    const QStringList& providesCapabilities,
    const QStringList& requiredCapabilities = {})
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = id;
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = symbolicName;
    descriptor.domain = QStringLiteral("test");
    descriptor.runtime.ctkSymbolicName = symbolicName;
    descriptor.runtime.bootstrapLevel = bootstrapLevel;
    descriptor.runtime.startupPolicy = startupPolicy;
    descriptor.provides.capabilities = providesCapabilities;
    descriptor.required.capabilities = requiredCapabilities;
    return descriptor;
}
}

class PlatformDependencyGraphTest : public QObject
{
    Q_OBJECT

private slots:
    void build_rejects_core_dependency_on_on_demand_plugin();
    void build_returns_topological_core_order();
    void capabilitySnapshot_keeps_capabilities_locked_until_plugin_ready();
    void capabilitySnapshot_unlocks_ready_capabilities();
};

void PlatformDependencyGraphTest::build_rejects_core_dependency_on_on_demand_plugin()
{
    const auto coreDisplay = makeDescriptor(
        QStringLiteral("org.medicalpro.four_view_display"),
        QStringLiteral("FourViewDisplay"),
        PlatformBootstrapLevel::Core,
        PlatformStartupPolicy::Eager,
        {QStringLiteral("imaging.viewport")},
        {QStringLiteral("navigation.tracking")});

    const auto tracking = makeDescriptor(
        QStringLiteral("org.medicalpro.optical_tracking"),
        QStringLiteral("OpticalTracking"),
        PlatformBootstrapLevel::Deferred,
        PlatformStartupPolicy::OnDemand,
        {QStringLiteral("navigation.tracking")});

    const auto result = PlatformDependencyGraph::build({coreDisplay, tracking});
    QVERIFY(!result.errors.isEmpty());
    QVERIFY(result.errors.join(QStringLiteral(" | ")).contains(QStringLiteral("on_demand")));
}

void PlatformDependencyGraphTest::build_returns_topological_core_order()
{
    const auto user = makeDescriptor(
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("UserManagement"),
        PlatformBootstrapLevel::Core,
        PlatformStartupPolicy::Eager,
        {QStringLiteral("identity.core")});
    const auto dicom = makeDescriptor(
        QStringLiteral("org.medicalpro.dicom_viewer"),
        QStringLiteral("DicomViewer"),
        PlatformBootstrapLevel::Core,
        PlatformStartupPolicy::Eager,
        {QStringLiteral("imaging.data")});
    const auto display = makeDescriptor(
        QStringLiteral("org.medicalpro.four_view_display"),
        QStringLiteral("FourViewDisplay"),
        PlatformBootstrapLevel::Core,
        PlatformStartupPolicy::Eager,
        {QStringLiteral("imaging.viewport")},
        {QStringLiteral("imaging.data")});

    const auto result = PlatformDependencyGraph::build({user, dicom, display});
    QVERIFY(result.errors.isEmpty());
    QCOMPARE(result.coreStartupOrder, (QStringList{
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("org.medicalpro.dicom_viewer"),
        QStringLiteral("org.medicalpro.four_view_display")
    }));
}

void PlatformDependencyGraphTest::capabilitySnapshot_keeps_capabilities_locked_until_plugin_ready()
{
    PlatformStateStore store;
    store.setRuntimeMode(PlatformRuntimeMode::ObserveOnly);
    store.replaceDescriptors({
        makeDescriptor(
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformBootstrapLevel::Core,
            PlatformStartupPolicy::Eager,
            {QStringLiteral("imaging.data")})
    });

    const auto pluginSnapshots = store.pluginSnapshots();
    QCOMPARE(pluginSnapshots.size(), 1);
    QCOMPARE(pluginSnapshots.first().state, PlatformPluginState::Discovered);

    const auto snapshot = store.capabilitySnapshot();
    QVERIFY(!snapshot.platformReady);
    QCOMPARE(snapshot.runtimeMode, PlatformRuntimeMode::ObserveOnly);
    QVERIFY(snapshot.unlockedCapabilities.isEmpty());
    QCOMPARE(snapshot.lockedCapabilities, (QStringList{QStringLiteral("imaging.data")}));
}

void PlatformDependencyGraphTest::capabilitySnapshot_unlocks_ready_capabilities()
{
    PlatformStateStore store;
    store.setRuntimeMode(PlatformRuntimeMode::FacadeMode);
    store.replaceDescriptors({
        makeDescriptor(
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("UserManagement"),
            PlatformBootstrapLevel::Core,
            PlatformStartupPolicy::Eager,
            {QStringLiteral("identity.core")}),
        makeDescriptor(
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("DicomViewer"),
            PlatformBootstrapLevel::Core,
            PlatformStartupPolicy::Eager,
            {QStringLiteral("imaging.data")})
    });

    store.setPluginState(QStringLiteral("org.medicalpro.user_management"), PlatformPluginState::Ready);
    store.setPluginState(QStringLiteral("org.medicalpro.dicom_viewer"), PlatformPluginState::Ready);

    const auto snapshot = store.capabilitySnapshot();
    QVERIFY(snapshot.platformReady);
    QCOMPARE(snapshot.runtimeMode, PlatformRuntimeMode::FacadeMode);
    QVERIFY(snapshot.lockedCapabilities.isEmpty());
    QCOMPARE(snapshot.unlockedCapabilities, (QStringList{
        QStringLiteral("identity.core"),
        QStringLiteral("imaging.data")
    }));
}

QTEST_APPLESS_MAIN(PlatformDependencyGraphTest)
#include "PlatformDependencyGraphTest.moc"
