#include <QtTest/QtTest>

#include <QFile>
#include <QTemporaryDir>

#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformManagedPluginPlan.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

namespace
{
PlatformPluginDescriptor makeDescriptor(
    const QString& pluginId,
    const QString& displayName,
    const QString& ctkSymbolicName,
    PlatformBootstrapLevel bootstrapLevel,
    PlatformStartupPolicy startupPolicy,
    const QStringList& providesCapabilities = {},
    const QStringList& requiredCapabilities = {})
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = pluginId;
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = displayName;
    descriptor.domain = QStringLiteral("test");
    descriptor.runtime.ctkSymbolicName = ctkSymbolicName;
    descriptor.runtime.bootstrapLevel = bootstrapLevel;
    descriptor.runtime.startupPolicy = startupPolicy;
    descriptor.provides.capabilities = providesCapabilities;
    descriptor.required.capabilities = requiredCapabilities;
    descriptor.diagnostics.requiredServices = QStringList{QStringLiteral("%1.service").arg(pluginId)};
    descriptor.diagnostics.serviceReadyTimeoutMs = 5000;
    descriptor.healthChecks = QStringList{QStringLiteral("service_registered")};
    return descriptor;
}
}

class PlatformManagedPluginPlanTest : public QObject
{
    Q_OBJECT

private slots:
    void build_returns_phase1_managed_core_install_plan();
    void build_adds_required_capability_provider_before_dependent_plugin();
    void build_rejects_managed_plugin_missing_phase1_diagnostics_contract();
};

void PlatformManagedPluginPlanTest::build_returns_phase1_managed_core_install_plan()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());

    QFile userBundle(pluginDir.filePath(QStringLiteral("UserManagement.dll")));
    QVERIFY(userBundle.open(QIODevice::WriteOnly));
    userBundle.close();

    QFile dicomBundle(pluginDir.filePath(QStringLiteral("DicomViewer.dll")));
    QVERIFY(dicomBundle.open(QIODevice::WriteOnly));
    dicomBundle.close();

    QFile fourViewBundle(pluginDir.filePath(QStringLiteral("FourViewDisplay.dll")));
    QVERIFY(fourViewBundle.open(QIODevice::WriteOnly));
    fourViewBundle.close();

    PlatformRuntimeConfig config;
    config.runtimeMode = PlatformRuntimeMode::FacadeMode;
    config.corePluginIds = QStringList{
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("org.medicalpro.dicom_viewer"),
        QStringLiteral("org.medicalpro.four_view_display")
    };

    const auto plan = PlatformManagedPluginPlanBuilder::build(
        config,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.user_management"),
                QStringLiteral("UserManagement"),
                QStringLiteral("UserManagement"),
                PlatformBootstrapLevel::Core,
                PlatformStartupPolicy::Eager,
                {QStringLiteral("identity.core")}),
            makeDescriptor(
                QStringLiteral("org.medicalpro.dicom_viewer"),
                QStringLiteral("DicomViewer"),
                QStringLiteral("DicomViewer"),
                PlatformBootstrapLevel::Core,
                PlatformStartupPolicy::Eager,
                {QStringLiteral("imaging.data")}),
            makeDescriptor(
                QStringLiteral("org.medicalpro.four_view_display"),
                QStringLiteral("FourViewDisplay"),
                QStringLiteral("FourViewDisplay"),
                PlatformBootstrapLevel::Core,
                PlatformStartupPolicy::Eager,
                {QStringLiteral("imaging.viewport")},
                {QStringLiteral("imaging.data")})
        },
        pluginDir.path());

    QCOMPARE(
        plan.managedPluginIds,
        (QStringList{
            QStringLiteral("org.medicalpro.user_management"),
            QStringLiteral("org.medicalpro.dicom_viewer"),
            QStringLiteral("org.medicalpro.four_view_display")
        }));
    QCOMPARE(plan.corePluginIds, plan.managedPluginIds);
    QCOMPARE(plan.installEntries.size(), 3);
    QVERIFY(plan.installEntries.constFirst().bundleFilePath.endsWith(QStringLiteral("UserManagement.dll")));
}

void PlatformManagedPluginPlanTest::build_adds_required_capability_provider_before_dependent_plugin()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());

    QFile dicomBundle(pluginDir.filePath(QStringLiteral("DicomViewer.dll")));
    QVERIFY(dicomBundle.open(QIODevice::WriteOnly));
    dicomBundle.close();

    QFile fourViewBundle(pluginDir.filePath(QStringLiteral("FourViewDisplay.dll")));
    QVERIFY(fourViewBundle.open(QIODevice::WriteOnly));
    fourViewBundle.close();

    PlatformRuntimeConfig config;
    config.runtimeMode = PlatformRuntimeMode::FacadeMode;
    config.corePluginIds = QStringList{QStringLiteral("org.medicalpro.four_view_display")};

    const auto plan = PlatformManagedPluginPlanBuilder::build(
        config,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.dicom_viewer"),
                QStringLiteral("DicomViewer"),
                QStringLiteral("DicomViewer"),
                PlatformBootstrapLevel::Core,
                PlatformStartupPolicy::Eager,
                {QStringLiteral("imaging.data")}),
            makeDescriptor(
                QStringLiteral("org.medicalpro.four_view_display"),
                QStringLiteral("FourViewDisplay"),
                QStringLiteral("FourViewDisplay"),
                PlatformBootstrapLevel::Core,
                PlatformStartupPolicy::Eager,
                {QStringLiteral("imaging.viewport")},
                {QStringLiteral("imaging.data")})
        },
        pluginDir.path());

    QCOMPARE(plan.installEntries.size(), 2);
    QCOMPARE(plan.installEntries.at(0).pluginId, QStringLiteral("org.medicalpro.dicom_viewer"));
    QCOMPARE(plan.installEntries.at(1).pluginId, QStringLiteral("org.medicalpro.four_view_display"));
}

void PlatformManagedPluginPlanTest::build_rejects_managed_plugin_missing_phase1_diagnostics_contract()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());

    QFile userBundle(pluginDir.filePath(QStringLiteral("UserManagement.dll")));
    QVERIFY(userBundle.open(QIODevice::WriteOnly));
    userBundle.close();

    auto descriptor = makeDescriptor(
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("UserManagement"),
        QStringLiteral("UserManagement"),
        PlatformBootstrapLevel::Core,
        PlatformStartupPolicy::Eager,
        {QStringLiteral("identity.core")});
    descriptor.diagnostics.requiredServices.clear();
    descriptor.healthChecks.clear();

    PlatformRuntimeConfig config;
    config.runtimeMode = PlatformRuntimeMode::FacadeMode;
    config.corePluginIds = QStringList{QStringLiteral("org.medicalpro.user_management")};

    QString error;
    const auto plan = PlatformManagedPluginPlanBuilder::build(config, {descriptor}, pluginDir.path(), &error);

    QVERIFY(plan.installEntries.isEmpty());
    QVERIFY(error.contains(QStringLiteral("health_checks")));
}

QTEST_APPLESS_MAIN(PlatformManagedPluginPlanTest)
#include "PlatformManagedPluginPlanTest.moc"
