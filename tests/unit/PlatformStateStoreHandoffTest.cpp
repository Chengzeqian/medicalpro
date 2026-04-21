#include <QtTest/QtTest>

#include "Framework/Platform/Bootstrap/PlatformStateStoreHandoff.h"
#include "Framework/Platform/Kernel/PlatformStateStore.h"

namespace
{
PlatformPluginDescriptor makeDescriptor(
    const QString& pluginId,
    const QString& symbolicName,
    const QStringList& providedCapabilities = {},
    const QStringList& requiredCapabilities = {})
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = pluginId;
    descriptor.displayName = pluginId;
    descriptor.runtime.ctkSymbolicName = symbolicName;
    descriptor.runtime.startupPolicy = PlatformStartupPolicy::Eager;
    descriptor.runtime.bootstrapLevel = PlatformBootstrapLevel::Core;
    descriptor.provides.capabilities = providedCapabilities;
    descriptor.required.capabilities = requiredCapabilities;
    return descriptor;
}

PlatformPluginRuntimeSnapshot findSnapshot(
    const QVector<PlatformPluginRuntimeSnapshot>& snapshots,
    const QString& pluginId)
{
    for (const auto& snapshot : snapshots) {
        if (snapshot.pluginId == pluginId) {
            return snapshot;
        }
    }

    return {};
}
}

class PlatformStateStoreHandoffTest : public QObject
{
    Q_OBJECT

private slots:
    void copyPlatformStateStore_preserves_scope_runtime_mode_and_plugin_states();
};

void PlatformStateStoreHandoffTest::copyPlatformStateStore_preserves_scope_runtime_mode_and_plugin_states()
{
    PlatformStateStore source;
    source.replaceDescriptors({
        makeDescriptor(
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("org.medicalpro.registration_core"),
            {QStringLiteral("registration")}),
        makeDescriptor(
            QStringLiteral("org.medicalpro.optical_tracking"),
            QStringLiteral("org.medicalpro.optical_tracking"),
            {QStringLiteral("tracking")},
            {QStringLiteral("registration")})
    });
    source.setRuntimeMode(PlatformRuntimeMode::OrchestrateCore);
    source.setStartupScopePluginIds({
        QStringLiteral("org.medicalpro.registration_core"),
        QStringLiteral("org.medicalpro.optical_tracking")
    });
    source.setGovernedPluginIds({
        QStringLiteral("org.medicalpro.registration_core"),
        QStringLiteral("org.medicalpro.optical_tracking")
    });
    source.setPluginState(QStringLiteral("org.medicalpro.registration_core"), PlatformPluginState::Ready);
    source.setPluginState(QStringLiteral("org.medicalpro.optical_tracking"), PlatformPluginState::Starting);

    PlatformStateStore target;
    copyPlatformStateStore(&target, &source);

    const auto capabilitySnapshot = target.capabilitySnapshot();
    QCOMPARE(capabilitySnapshot.runtimeMode, PlatformRuntimeMode::OrchestrateCore);
    QCOMPARE(
        capabilitySnapshot.startupScopePluginIds,
        QStringList({
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("org.medicalpro.optical_tracking")
        }));
    QCOMPARE(
        capabilitySnapshot.governedPluginIds,
        QStringList({
            QStringLiteral("org.medicalpro.registration_core"),
            QStringLiteral("org.medicalpro.optical_tracking")
        }));
    QVERIFY(!capabilitySnapshot.platformReady);

    const auto snapshots = target.pluginSnapshots();
    QCOMPARE(
        findSnapshot(snapshots, QStringLiteral("org.medicalpro.registration_core")).state,
        PlatformPluginState::Ready);
    QCOMPARE(
        findSnapshot(snapshots, QStringLiteral("org.medicalpro.optical_tracking")).state,
        PlatformPluginState::Starting);
}

QTEST_APPLESS_MAIN(PlatformStateStoreHandoffTest)

#include "PlatformStateStoreHandoffTest.moc"
