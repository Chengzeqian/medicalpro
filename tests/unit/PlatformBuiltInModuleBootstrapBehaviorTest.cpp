#include <QtTest/QtTest>

#include "Framework/Platform/Bootstrap/platform_built_in_module_bootstrap.h"
#include "Framework/Platform/Kernel/platform_plugin_host.h"

#include <QFile>

class PlatformBuiltInModuleBootstrapBehaviorTest : public QObject
{
    Q_OBJECT

private slots:
    void bootstrap_registers_user_management_platform_module_without_ctk_activator();
    void user_management_module_registers_governed_and_legacy_service_ids();
    void bootstrap_registers_dicom_viewer_platform_module_without_ctk_activator();
    void dicom_viewer_module_registers_governed_and_legacy_service_ids();
    void bootstrap_registers_four_view_display_platform_module_without_ctk_activator();
    void four_view_display_module_registers_governed_and_legacy_service_ids();
    void bootstrap_registers_registration_core_platform_module_without_ctk_activator();
    void bootstrap_registers_optical_tracking_platform_module_without_ctk_activator();
    void bootstrap_registers_registration_2d3d_platform_module_without_ctk_activator();
    void bootstrap_registers_optical_registration_platform_module_without_ctk_activator();
    void bootstrap_registers_point_registration_platform_module_without_ctk_activator();
    void bootstrap_registers_instrument_management_platform_module_without_ctk_activator();
    void bootstrap_registers_bone_segmentation_platform_module_without_ctk_activator();

private:
    QString readSource(const QString& relativePath) const;
};

QString PlatformBuiltInModuleBootstrapBehaviorTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

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

void PlatformBuiltInModuleBootstrapBehaviorTest::user_management_module_registers_governed_and_legacy_service_ids()
{
    const QString source = readSource(QStringLiteral("Plugins/UserManagement/user_management_module.cpp"));
    QVERIFY2(source.contains(QStringLiteral("medical.UserManagementService")),
        "UserManagement module must register the governed service id required by startup descriptors");
    QVERIFY2(source.contains(QStringLiteral("UserManagementService")),
        "UserManagement module must keep the legacy service id used by UI adapters");
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

void PlatformBuiltInModuleBootstrapBehaviorTest::dicom_viewer_module_registers_governed_and_legacy_service_ids()
{
    const QString source = readSource(QStringLiteral("Plugins/DicomViewer/dicom_viewer_module.cpp"));
    QVERIFY2(source.contains(QStringLiteral("org.medicalpro.DicomViewerService")),
        "DicomViewer module must register the governed service id required by startup descriptors");
    QVERIFY2(source.contains(QStringLiteral("DicomViewerService")),
        "DicomViewer module must keep the legacy service id used by UI adapters");
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

void PlatformBuiltInModuleBootstrapBehaviorTest::four_view_display_module_registers_governed_and_legacy_service_ids()
{
    const QString source = readSource(QStringLiteral("Plugins/FourViewDisplay/four_view_display_module.cpp"));
    QVERIFY2(source.contains(QStringLiteral("com.medicalpro.FourViewDisplayService")),
        "FourViewDisplay module must register the governed service id required by startup descriptors");
    QVERIFY2(source.contains(QStringLiteral("FourViewDisplayService")),
        "FourViewDisplay module must keep the legacy service id used by UI adapters");
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
