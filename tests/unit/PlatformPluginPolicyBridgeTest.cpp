#include <QtTest/QtTest>

#include "Framework/Platform/Contracts/PlatformPluginDescriptor.h"
#include "Framework/Platform/Kernel/PlatformPluginPolicyBridge.h"
#include "Framework/Platform/Kernel/PlatformRuntimeConfig.h"

namespace
{
PlatformPluginDescriptor makeDescriptor(
    const QString& pluginId,
    const QString& displayName,
    const QString& ctkSymbolicName,
    PlatformStartupPolicy startupPolicy,
    PlatformBootstrapLevel bootstrapLevel = PlatformBootstrapLevel::Deferred)
{
    PlatformPluginDescriptor descriptor;
    descriptor.id = pluginId;
    descriptor.version = QStringLiteral("1.0.0");
    descriptor.displayName = displayName;
    descriptor.domain = QStringLiteral("test");
    descriptor.runtime.symbolicName = ctkSymbolicName;
    descriptor.runtime.startupPolicy = startupPolicy;
    descriptor.runtime.bootstrapLevel = bootstrapLevel;
    descriptor.diagnostics.requiredServices = QStringList{QStringLiteral("%1.service").arg(pluginId)};
    descriptor.diagnostics.serviceReadyTimeoutMs = 5000;
    descriptor.healthChecks = QStringList{QStringLiteral("service_registered")};
    return descriptor;
}
}

class PlatformPluginPolicyBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    void resolve_returns_immediate_and_critical_for_core_eager_descriptor();
    void resolve_returns_deferred_and_non_critical_for_non_core_eager_descriptor();
    void resolve_returns_on_demand_and_non_critical_for_registration_core_descriptor();
    void resolve_returns_conservative_fallback_when_descriptor_is_missing();
    void resolve_normalizes_case_and_lib_prefix_before_matching();
};

void PlatformPluginPolicyBridgeTest::resolve_returns_immediate_and_critical_for_core_eager_descriptor()
{
    PlatformRuntimeConfig runtimeConfig;
    runtimeConfig.corePluginIds = QStringList{QStringLiteral("org.medicalpro.user_management")};

    const auto result = PlatformPluginPolicyBridge::resolve(
        runtimeConfig,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.user_management"),
                QStringLiteral("UserManagement"),
                QStringLiteral("UserManagement"),
                PlatformStartupPolicy::Eager,
                PlatformBootstrapLevel::Core)
        },
        QStringLiteral("UserManagement"));

    QCOMPARE(result.resolvedPluginId, QStringLiteral("org.medicalpro.user_management"));
    QCOMPARE(result.symbolicName, QStringLiteral("UserManagement"));
    QCOMPARE(result.loadBucket, PlatformPluginLoadBucket::Immediate);
    QVERIFY(result.isCritical);
    QCOMPARE(result.resolutionStatus, PlatformPluginPolicyResolutionStatus::ResolvedFromDescriptor);
    QVERIFY(result.diagnosticCode.isEmpty());
}

void PlatformPluginPolicyBridgeTest::resolve_returns_deferred_and_non_critical_for_non_core_eager_descriptor()
{
    PlatformRuntimeConfig runtimeConfig;
    runtimeConfig.corePluginIds = QStringList{QStringLiteral("org.medicalpro.user_management")};

    const auto result = PlatformPluginPolicyBridge::resolve(
        runtimeConfig,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.navigation_support"),
                QStringLiteral("NavigationSupport"),
                QStringLiteral("NavigationSupport"),
                PlatformStartupPolicy::Eager)
        },
        QStringLiteral("NavigationSupport"));

    QCOMPARE(result.resolvedPluginId, QStringLiteral("org.medicalpro.navigation_support"));
    QCOMPARE(result.loadBucket, PlatformPluginLoadBucket::Deferred);
    QVERIFY(!result.isCritical);
    QCOMPARE(result.resolutionStatus, PlatformPluginPolicyResolutionStatus::ResolvedFromDescriptor);
    QVERIFY(result.diagnosticCode.isEmpty());
}

void PlatformPluginPolicyBridgeTest::resolve_returns_on_demand_and_non_critical_for_registration_core_descriptor()
{
    PlatformRuntimeConfig runtimeConfig;
    runtimeConfig.corePluginIds = QStringList{
        QStringLiteral("org.medicalpro.user_management"),
        QStringLiteral("org.medicalpro.dicom_viewer"),
        QStringLiteral("org.medicalpro.four_view_display")
    };

    const auto result = PlatformPluginPolicyBridge::resolve(
        runtimeConfig,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"),
                PlatformStartupPolicy::OnDemand)
        },
        QStringLiteral("RegistrationCore"));

    QCOMPARE(result.resolvedPluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(result.loadBucket, PlatformPluginLoadBucket::OnDemand);
    QVERIFY(!result.isCritical);
    QCOMPARE(result.resolutionStatus, PlatformPluginPolicyResolutionStatus::ResolvedFromDescriptor);
    QVERIFY(result.diagnosticCode.isEmpty());
}

void PlatformPluginPolicyBridgeTest::resolve_returns_conservative_fallback_when_descriptor_is_missing()
{
    PlatformRuntimeConfig runtimeConfig;
    runtimeConfig.corePluginIds = QStringList{QStringLiteral("org.medicalpro.user_management")};

    const auto result = PlatformPluginPolicyBridge::resolve(
        runtimeConfig,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.user_management"),
                QStringLiteral("UserManagement"),
                QStringLiteral("UserManagement"),
                PlatformStartupPolicy::Eager,
                PlatformBootstrapLevel::Core)
        },
        QStringLiteral("LegacyOnlyPlugin"));

    QVERIFY(result.resolvedPluginId.isEmpty());
    QCOMPARE(result.symbolicName, QStringLiteral("LegacyOnlyPlugin"));
    QCOMPARE(result.loadBucket, PlatformPluginLoadBucket::OnDemand);
    QVERIFY(!result.isCritical);
    QCOMPARE(result.resolutionStatus, PlatformPluginPolicyResolutionStatus::DescriptorMissingFallback);
    QCOMPARE(result.diagnosticCode, QStringLiteral("descriptor_missing_for_plugin_policy_bridge"));
}

void PlatformPluginPolicyBridgeTest::resolve_normalizes_case_and_lib_prefix_before_matching()
{
    PlatformRuntimeConfig runtimeConfig;

    const auto result = PlatformPluginPolicyBridge::resolve(
        runtimeConfig,
        {
            makeDescriptor(
                QStringLiteral("org.medicalpro.registration_core"),
                QStringLiteral("RegistrationCore"),
                QStringLiteral("RegistrationCore"),
                PlatformStartupPolicy::OnDemand)
        },
        QStringLiteral("libregistrationcore"));

    QCOMPARE(result.resolvedPluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(result.symbolicName, QStringLiteral("RegistrationCore"));
    QCOMPARE(result.loadBucket, PlatformPluginLoadBucket::OnDemand);
    QCOMPARE(result.resolutionStatus, PlatformPluginPolicyResolutionStatus::ResolvedFromDescriptor);
}

QTEST_APPLESS_MAIN(PlatformPluginPolicyBridgeTest)
#include "PlatformPluginPolicyBridgeTest.moc"
