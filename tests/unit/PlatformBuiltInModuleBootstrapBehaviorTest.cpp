#include <QtTest/QtTest>

#include "Framework/Platform/Bootstrap/platform_built_in_module_bootstrap.h"
#include "Framework/Platform/Kernel/platform_plugin_host.h"

class PlatformBuiltInModuleBootstrapBehaviorTest : public QObject
{
    Q_OBJECT

private slots:
    void bootstrap_registers_user_management_platform_module_without_ctk_activator();
    void bootstrap_registers_dicom_viewer_platform_module_without_ctk_activator();
    void bootstrap_registers_four_view_display_platform_module_without_ctk_activator();
    void bootstrap_registers_registration_core_platform_module_without_ctk_activator();
    void bootstrap_registers_optical_tracking_platform_module_without_ctk_activator();
    void bootstrap_registers_registration_2d3d_platform_module_without_ctk_activator();
    void bootstrap_registers_optical_registration_platform_module_without_ctk_activator();
    void bootstrap_registers_point_registration_platform_module_without_ctk_activator();
    void bootstrap_registers_instrument_management_platform_module_without_ctk_activator();
    void bootstrap_registers_bone_segmentation_platform_module_without_ctk_activator();
};

void PlatformBuiltInModuleBootstrapBehaviorTest::bootstrap_registers_user_management_platform_module_without_ctk_activator()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    registerBuiltInPlatformModules();

    QVERIFY2(host.hasActivator(QStringLiteral("UserManagement")),
        "bootstrap should register UserManagement platform module before CTK activator startup");
    QVERIFY2(host.registeredPluginIds().contains(QStringLiteral("UserManagement")),
        "registered plugin list should expose UserManagement after bootstrap");
}

void PlatformBuiltInModuleBootstrapBehaviorTest::bootstrap_registers_dicom_viewer_platform_module_without_ctk_activator()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    registerBuiltInPlatformModules();

    QVERIFY2(host.hasActivator(QStringLiteral("DicomViewer")),
        "bootstrap should register DicomViewer platform module before CTK activator startup");
    QVERIFY2(host.registeredPluginIds().contains(QStringLiteral("DicomViewer")),
        "registered plugin list should expose DicomViewer after bootstrap");
}

void PlatformBuiltInModuleBootstrapBehaviorTest::bootstrap_registers_four_view_display_platform_module_without_ctk_activator()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    registerBuiltInPlatformModules();

    QVERIFY2(host.hasActivator(QStringLiteral("FourViewDisplay")),
        "bootstrap should register FourViewDisplay platform module before CTK activator startup");
    QVERIFY2(host.registeredPluginIds().contains(QStringLiteral("FourViewDisplay")),
        "registered plugin list should expose FourViewDisplay after bootstrap");
}

void PlatformBuiltInModuleBootstrapBehaviorTest::bootstrap_registers_registration_core_platform_module_without_ctk_activator()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    registerBuiltInPlatformModules();

    QVERIFY2(host.hasActivator(QStringLiteral("RegistrationCore")),
        "bootstrap should register RegistrationCore platform module before CTK activator startup");
    QVERIFY2(host.registeredPluginIds().contains(QStringLiteral("RegistrationCore")),
        "registered plugin list should expose RegistrationCore after bootstrap");
}

void PlatformBuiltInModuleBootstrapBehaviorTest::bootstrap_registers_optical_tracking_platform_module_without_ctk_activator()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    registerBuiltInPlatformModules();

    QVERIFY2(host.hasActivator(QStringLiteral("OpticalTracking")),
        "bootstrap should register OpticalTracking platform module before CTK activator startup");
    QVERIFY2(host.registeredPluginIds().contains(QStringLiteral("OpticalTracking")),
        "registered plugin list should expose OpticalTracking after bootstrap");
}

void PlatformBuiltInModuleBootstrapBehaviorTest::bootstrap_registers_registration_2d3d_platform_module_without_ctk_activator()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    registerBuiltInPlatformModules();

    QVERIFY2(host.hasActivator(QStringLiteral("Registration2D3D")),
        "bootstrap should register Registration2D3D platform module before CTK activator startup");
    QVERIFY2(host.registeredPluginIds().contains(QStringLiteral("Registration2D3D")),
        "registered plugin list should expose Registration2D3D after bootstrap");
}

void PlatformBuiltInModuleBootstrapBehaviorTest::bootstrap_registers_optical_registration_platform_module_without_ctk_activator()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    registerBuiltInPlatformModules();

    QVERIFY2(host.hasActivator(QStringLiteral("OpticalRegistration")),
        "bootstrap should register OpticalRegistration platform module before CTK activator startup");
    QVERIFY2(host.registeredPluginIds().contains(QStringLiteral("OpticalRegistration")),
        "registered plugin list should expose OpticalRegistration after bootstrap");
}

void PlatformBuiltInModuleBootstrapBehaviorTest::bootstrap_registers_point_registration_platform_module_without_ctk_activator()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    registerBuiltInPlatformModules();

    QVERIFY2(host.hasActivator(QStringLiteral("PointRegistration")),
        "bootstrap should register PointRegistration platform module before CTK activator startup");
    QVERIFY2(host.registeredPluginIds().contains(QStringLiteral("PointRegistration")),
        "registered plugin list should expose PointRegistration after bootstrap");
}

void PlatformBuiltInModuleBootstrapBehaviorTest::bootstrap_registers_instrument_management_platform_module_without_ctk_activator()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    registerBuiltInPlatformModules();

    QVERIFY2(host.hasActivator(QStringLiteral("InstrumentManagement")),
        "bootstrap should register InstrumentManagement platform module before CTK activator startup");
    QVERIFY2(host.registeredPluginIds().contains(QStringLiteral("InstrumentManagement")),
        "registered plugin list should expose InstrumentManagement after bootstrap");
}

void PlatformBuiltInModuleBootstrapBehaviorTest::bootstrap_registers_bone_segmentation_platform_module_without_ctk_activator()
{
    auto& host = PlatformPluginHost::sharedInstance();
    host.stopAll();

    registerBuiltInPlatformModules();

    QVERIFY2(host.hasActivator(QStringLiteral("BoneSegmentation")),
        "bootstrap should register BoneSegmentation platform module before CTK activator startup");
    QVERIFY2(host.registeredPluginIds().contains(QStringLiteral("BoneSegmentation")),
        "registered plugin list should expose BoneSegmentation after bootstrap");
}

QTEST_APPLESS_MAIN(PlatformBuiltInModuleBootstrapBehaviorTest)
#include "PlatformBuiltInModuleBootstrapBehaviorTest.moc"
