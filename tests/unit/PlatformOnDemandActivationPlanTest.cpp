#include <QtTest/QtTest>

#include <QFile>
#include <QTemporaryDir>

#include "Framework/Platform/Kernel/PlatformOnDemandActivationPlan.h"

namespace
{
PlatformPluginDescriptor makeDescriptor(
    const QString& pluginId,
    const QString& displayName,
    const QString& ctkSymbolicName,
    PlatformStartupPolicy startupPolicy = PlatformStartupPolicy::OnDemand,
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred,
    const QStringList& providedCapabilities = {},
    const QStringList& requiredPlugins = {},
    const QStringList& requiredCapabilities = {})
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = pluginId;
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = displayName;
    descriptor.domain = QStringLiteral("navigation");
    descriptor.runtime.symbolicName = ctkSymbolicName;
    descriptor.runtime.startupPolicy = startupPolicy;
    descriptor.runtime.bootstrapLevel = bootstrapLevel;
    descriptor.provides.capabilities = providedCapabilities;
    descriptor.required.plugins = requiredPlugins;
    descriptor.required.capabilities = requiredCapabilities;
    descriptor.diagnostics.requiredServices = QStringList{QStringLiteral("%1.service").arg(pluginId)};
    descriptor.diagnostics.serviceReadyTimeoutMs = 5000;
    descriptor.healthChecks = QStringList{QStringLiteral("service_registered")};
    return descriptor;
}
}

class PlatformOnDemandActivationPlanTest : public QObject
{
    Q_OBJECT

private slots:
    void build_returns_target_activation_entry();
    void build_marks_platform_hosted_target_as_not_requiring_bundle_install();
    void build_adds_required_plugin_before_target();
    void build_rejects_missing_diagnostics_contract();
    void build_rejects_missing_bundle_path();
};

void PlatformOnDemandActivationPlanTest::build_returns_target_activation_entry()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());

    QFile bundleFile(pluginDir.filePath(QStringLiteral("RegistrationCore.dll")));
    QVERIFY(bundleFile.open(QIODevice::WriteOnly));
    bundleFile.close();

    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        QStringLiteral("org.medicalpro.registration_core"),
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"),
                PlatformStartupPolicy::OnDemand,
                PlatformBootstrapLevel::Deferred,
                {QStringLiteral("navigation.registration")},
                {},
                {QStringLiteral("imaging.data")})
        },
        pluginDir.path(),
        &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.targetPluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(plan.activationEntries.size(), 1);
    QVERIFY(plan.activationEntries.constFirst().target);
    QVERIFY(plan.activationEntries.constFirst().bundleFilePath.endsWith(QStringLiteral("RegistrationCore.dll")));
}

void PlatformOnDemandActivationPlanTest::build_marks_platform_hosted_target_as_not_requiring_bundle_install()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());

    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        QStringLiteral("org.medicalpro.registration_core"),
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"))
        },
        pluginDir.path(),
        [](const QString& symbolicName) {
            return symbolicName == QStringLiteral("RegistrationCore");
        },
        &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.activationEntries.size(), 1);
    QVERIFY(plan.activationEntries.constFirst().bundleFilePath.isEmpty());
    QVERIFY(!plan.activationEntries.constFirst().requiresBundleInstall);
}

void PlatformOnDemandActivationPlanTest::build_adds_required_plugin_before_target()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());

    QFile supportBundle(pluginDir.filePath(QStringLiteral("NavigationSupport.dll")));
    QVERIFY(supportBundle.open(QIODevice::WriteOnly));
    supportBundle.close();

    QFile targetBundle(pluginDir.filePath(QStringLiteral("RegistrationCore.dll")));
    QVERIFY(targetBundle.open(QIODevice::WriteOnly));
    targetBundle.close();

    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        QStringLiteral("org.medicalpro.registration_core"),
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.navigation_support"),
                QStringLiteral("NavigationSupport"),
                QStringLiteral("NavigationSupport")),
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"),
                PlatformStartupPolicy::OnDemand,
                PlatformBootstrapLevel::Deferred,
                {QStringLiteral("navigation.registration")},
                {QStringLiteral("org.medicalpro.navigation_support")})
        },
        pluginDir.path(),
        &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.activationEntries.size(), 2);
    QCOMPARE(plan.activationEntries.at(0).pluginId, QStringLiteral("org.medicalpro.navigation_support"));
    QCOMPARE(plan.activationEntries.at(1).pluginId, QStringLiteral("org.medicalpro.registration_core"));
    QVERIFY(plan.activationEntries.at(1).target);
}

void PlatformOnDemandActivationPlanTest::build_rejects_missing_diagnostics_contract()
{
    QTemporaryDir pluginDir;
    QVERIFY(pluginDir.isValid());

    QFile bundleFile(pluginDir.filePath(QStringLiteral("OpticalTracking.dll")));
    QVERIFY(bundleFile.open(QIODevice::WriteOnly));
    bundleFile.close();

    auto descriptor = makeDescriptor(
        QStringLiteral("org.medicalpro.optical_tracking"),
        QStringLiteral("OpticalTracking"),
        QStringLiteral("OpticalTracking"));
    descriptor.diagnostics.requiredServices.clear();
    descriptor.diagnostics.serviceReadyTimeoutMs = 0;

    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        QStringLiteral("org.medicalpro.optical_tracking"),
        {descriptor},
        pluginDir.path(),
        &error);

    QVERIFY(plan.activationEntries.isEmpty());
    QVERIFY(error.contains(QStringLiteral("diagnostics")));
}

void PlatformOnDemandActivationPlanTest::build_rejects_missing_bundle_path()
{
    QString error;
    const auto plan = PlatformOnDemandActivationPlanBuilder::build(
        QStringLiteral("org.medicalpro.registration_core"),
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"))
        },
        QStringLiteral("C:/missing/plugins"),
        &error);

    QVERIFY(plan.activationEntries.isEmpty());
    QVERIFY(error.contains(QStringLiteral("bundle")));
}

QTEST_APPLESS_MAIN(PlatformOnDemandActivationPlanTest)
#include "PlatformOnDemandActivationPlanTest.moc"
