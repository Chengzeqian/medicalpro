#include <QtTest/QtTest>

#include <QFile>

class PlatformPluginBuildGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void legacy_ctk_plugin_build_macros_are_removed();
    void platform_module_builds_no_longer_reference_legacy_activator_targets();
    void plugin_and_ui_configuration_no_longer_depend_on_ctk_found();

private:
    QString readSource(const QString& relativePath) const;
};

QString PlatformPluginBuildGovernanceContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

void PlatformPluginBuildGovernanceContractTest::legacy_ctk_plugin_build_macros_are_removed()
{
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));
    const QString pluginMacros = readSource(QStringLiteral("cmake/PluginMacros.cmake"));

    QVERIFY2(!rootCMake.contains(QStringLiteral("MEDICALPRO_BUILD_LEGACY_CTK_PLUGINS")),
        "CMakeLists.txt must not expose MEDICALPRO_BUILD_LEGACY_CTK_PLUGINS after CTK removal");
    QVERIFY2(!pluginMacros.contains(QStringLiteral("add_medical_plugin(")),
        "PluginMacros.cmake must not keep the legacy add_medical_plugin helper");
    QVERIFY2(!pluginMacros.contains(QStringLiteral("if(NOT MEDICALPRO_BUILD_LEGACY_CTK_PLUGINS)")),
        "PluginMacros.cmake must not retain legacy CTK plugin build switch logic");
    QVERIFY2(!pluginMacros.contains(QStringLiteral("if(NOT CTK_FOUND)")),
        "PluginMacros.cmake must not retain CTK_FOUND gating");
    QVERIFY2(pluginMacros.contains(QStringLiteral("function(register_platform_descriptor plugin_name descriptor_source)")),
        "PluginMacros.cmake must keep the platform descriptor registration helper");
}

void PlatformPluginBuildGovernanceContractTest::platform_module_builds_no_longer_reference_legacy_activator_targets()
{
    const QString pluginsRootCMake = readSource(QStringLiteral("Plugins/CMakeLists.txt"));

    QVERIFY2(!pluginsRootCMake.contains(QStringLiteral("CTK not found - plugins will not be built")),
        "Plugins/CMakeLists.txt should keep configuring platform modules even when CTK is unavailable");

    const struct PluginContract
    {
        const char* path;
        const char* target;
        const char* pluginName;
    } contracts[] = {
        { "Plugins/UserManagement/CMakeLists.txt", "UserManagementPlatformModuleLib", "UserManagement" },
        { "Plugins/DicomViewer/CMakeLists.txt", "DicomViewerPlatformModuleLib", "DicomViewer" },
        { "Plugins/FourViewDisplay/CMakeLists.txt", "FourViewDisplayPlatformModuleLib", "FourViewDisplay" },
        { "Plugins/RegistrationCore/CMakeLists.txt", "RegistrationCorePlatformModuleLib", "RegistrationCore" },
        { "Plugins/Registration2D3D/CMakeLists.txt", "Registration2D3DPlatformModuleLib", "Registration2D3D" },
        { "Plugins/OpticalTracking/CMakeLists.txt", "OpticalTrackingPlatformModuleLib", "OpticalTracking" },
        { "Plugins/OpticalRegistration/CMakeLists.txt", "OpticalRegistrationPlatformModuleLib", "OpticalRegistration" },
        { "Plugins/PointRegistration/CMakeLists.txt", "PointRegistrationPlatformModuleLib", "PointRegistration" },
        { "Plugins/InstrumentManagement/CMakeLists.txt", "InstrumentManagementPlatformModuleLib", "InstrumentManagement" },
        { "Plugins/BoneSegmentation/CMakeLists.txt", "BoneSegmentationPlatformModuleLib", "BoneSegmentation" }
    };

    for (const auto& contract : contracts) {
        const QString source = readSource(QString::fromUtf8(contract.path));
        QVERIFY2(source.contains(QStringLiteral("add_library(") + QString::fromUtf8(contract.target) + QStringLiteral(" STATIC")),
            qPrintable(QStringLiteral("%1 must keep building its platform module without a CTK gate").arg(QString::fromUtf8(contract.path))));
        QVERIFY2(source.contains(QStringLiteral("register_platform_descriptor(") + QString::fromUtf8(contract.pluginName)),
            qPrintable(QStringLiteral("%1 must register its platform descriptor from the plugin CMakeLists").arg(QString::fromUtf8(contract.path))));
        QVERIFY2(!source.contains(QStringLiteral("add_medical_plugin(")),
            qPrintable(QStringLiteral("%1 must not create a legacy CTK plugin target").arg(QString::fromUtf8(contract.path))));
        QVERIFY2(!source.contains(QStringLiteral("Activator.cpp")),
            qPrintable(QStringLiteral("%1 must not compile legacy CTK activator sources").arg(QString::fromUtf8(contract.path))));
        QVERIFY2(!source.contains(QStringLiteral("optional legacy CTK bridge")),
            qPrintable(QStringLiteral("%1 must not advertise an optional legacy CTK bridge").arg(QString::fromUtf8(contract.path))));
    }
}

void PlatformPluginBuildGovernanceContractTest::plugin_and_ui_configuration_no_longer_depend_on_ctk_found()
{
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));
    const QString newPagesCMake = readSource(QStringLiteral("UI/NewPages/CMakeLists.txt"));

    QVERIFY2(rootCMake.contains(QStringLiteral("if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/Plugins\")")),
        "Plugins/CMakeLists.txt must be added even when CTK is unavailable");
    QVERIFY2(!rootCMake.contains(QStringLiteral("if(CTK_FOUND AND EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/Plugins\")")),
        "platform plugin configuration must no longer be gated by CTK_FOUND");
    QVERIFY2(!newPagesCMake.contains(QStringLiteral("if(CTK_FOUND)")),
        "UI/NewPages/CMakeLists.txt must not gate UI sources behind CTK_FOUND");
    QVERIFY2(!newPagesCMake.contains(QStringLiteral("CTK_PLUGIN_FRAMEWORK")),
        "UI/NewPages/CMakeLists.txt must not re-enable CTK compile definitions");
}

QTEST_APPLESS_MAIN(PlatformPluginBuildGovernanceContractTest)
#include "PlatformPluginBuildGovernanceContractTest.moc"
