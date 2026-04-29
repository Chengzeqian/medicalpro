#include <QtTest/QtTest>

#include <QFile>

class PlatformNamingGovernanceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void active_build_and_test_targets_use_platform_neutral_names();
    void active_product_sources_do_not_keep_ctk_placeholder_comments();
    void runtime_entry_and_user_management_headers_use_platform_neutral_readable_language();
    void current_build_docs_do_not_describe_ctk_as_a_required_runtime_dependency();

private:
    QString readSource(const QString& relativePath) const;
};

QString PlatformNamingGovernanceContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

void PlatformNamingGovernanceContractTest::active_build_and_test_targets_use_platform_neutral_names()
{
    const QString unitTestsCMake = readSource(QStringLiteral("tests/unit/CMakeLists.txt"));
    const QString runtimeArtifactVerify = readSource(QStringLiteral("tests/runtime/verify_runtime_artifacts.cmake"));
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));

    QVERIFY2(!unitTestsCMake.contains(QStringLiteral("ctk_runtime_exit_host_detachment_contract_test")),
        "tests/unit/CMakeLists.txt still exposes the old ctk_runtime_exit_host_detachment_contract_test target name");
    QVERIFY2(!unitTestsCMake.contains(QStringLiteral("ctk_runtime_build_deployment_contract_test")),
        "tests/unit/CMakeLists.txt still exposes the old ctk_runtime_build_deployment_contract_test target name");
    QVERIFY2(!unitTestsCMake.contains(QStringLiteral("ctk_legacy_plugin_build_contract_test")),
        "tests/unit/CMakeLists.txt still exposes the old ctk_legacy_plugin_build_contract_test target name");
    QVERIFY2(!unitTestsCMake.contains(QStringLiteral("ui_ctk_decoupling_acceptance_test")),
        "tests/unit/CMakeLists.txt still exposes the old ui_ctk_decoupling_acceptance_test target name");
    QVERIFY2(!runtimeArtifactVerify.contains(QStringLiteral("stale_ctk_runtime_artifacts")),
        "runtime artifact verification still uses stale_ctk_runtime_artifacts naming");
    QVERIFY2(!runtimeArtifactVerify.contains(QStringLiteral("legacy CTK plugin manifests")),
        "runtime artifact verification still reports legacy CTK plugin manifests");
    QVERIFY2(!rootCMake.contains(QStringLiteral("remove_ctk_runtime_host_artifacts.cmake")),
        "CMakeLists.txt still references remove_ctk_runtime_host_artifacts.cmake");
    QVERIFY2(!rootCMake.contains(QStringLiteral("remove_legacy_ctk_plugin_artifacts.cmake")),
        "CMakeLists.txt still references remove_legacy_ctk_plugin_artifacts.cmake");
}

void PlatformNamingGovernanceContractTest::active_product_sources_do_not_keep_ctk_placeholder_comments()
{
    const QString dicomViewerHeader = readSource(QStringLiteral("Plugins/DicomViewer/DicomViewerServiceImpl.h"));
    const QString fourViewHeader = readSource(QStringLiteral("Plugins/FourViewDisplay/FourViewDisplayServiceImpl.h"));
    const QString registration2d3dHeader = readSource(QStringLiteral("Plugins/Registration2D3D/Registration2D3DServiceImpl.h"));
    const QString registrationCoreHeader = readSource(QStringLiteral("Plugins/RegistrationCore/RegistrationServiceImpl.h"));
    const QString opticalTrackingCMake = readSource(QStringLiteral("Plugins/OpticalTracking/CMakeLists.txt"));
    const QString opticalRegistrationCMake = readSource(QStringLiteral("Plugins/OpticalRegistration/CMakeLists.txt"));
    const QString registration2d3dCMake = readSource(QStringLiteral("Plugins/Registration2D3D/CMakeLists.txt"));

    QVERIFY2(!dicomViewerHeader.contains(QStringLiteral("CTK Plugin Framework")),
        "DicomViewerServiceImpl.h still keeps the old CTK Plugin Framework placeholder comment");
    QVERIFY2(!dicomViewerHeader.contains(QStringLiteral("CTK Context")),
        "DicomViewerServiceImpl.h still keeps CTK Context comments");
    QVERIFY2(!fourViewHeader.contains(QStringLiteral("CTK Context")),
        "FourViewDisplayServiceImpl.h still keeps CTK Context comments");
    QVERIFY2(!registration2d3dHeader.contains(QStringLiteral("CTK Context")),
        "Registration2D3DServiceImpl.h still keeps CTK Context comments");
    QVERIFY2(!registrationCoreHeader.contains(QStringLiteral("CTK")),
        "RegistrationServiceImpl.h still keeps CTK wording in active service-registry comments");
    QVERIFY2(!opticalTrackingCMake.contains(QStringLiteral("CTK Plugin Framework")),
        "OpticalTracking/CMakeLists.txt still describes the module as CTK Plugin Framework configuration");
    QVERIFY2(!opticalRegistrationCMake.contains(QStringLiteral("CTK Plugin Framework")),
        "OpticalRegistration/CMakeLists.txt still describes the module as CTK Plugin Framework configuration");
    QVERIFY2(!registration2d3dCMake.contains(QStringLiteral("CTK Plugin Framework")),
        "Registration2D3D/CMakeLists.txt still describes the module as CTK Plugin Framework configuration");
}

void PlatformNamingGovernanceContractTest::runtime_entry_and_user_management_headers_use_platform_neutral_readable_language()
{
    const QString mainSource = readSource(QStringLiteral("main.cpp"));
    const QString startupHeader = readSource(QStringLiteral("Framework/StartupOrchestrator.h"));
    const QString startupSource = readSource(QStringLiteral("Framework/StartupOrchestrator.cpp"));
    const QString userManagementService = readSource(QStringLiteral("Plugins/UserManagement/UserManagementService.h"));
    const QString userManagementImpl = readSource(QStringLiteral("Plugins/UserManagement/UserManagementServiceImpl.h"));
    const QString instrumentManagementService = readSource(QStringLiteral("Plugins/InstrumentManagement/InstrumentManagementService.h"));
    const QString pointRegistrationService = readSource(QStringLiteral("Plugins/PointRegistration/PointRegistrationService.h"));
    const QString fourViewService = readSource(QStringLiteral("Plugins/FourViewDisplay/FourViewDisplayService.h"));
    const QString registration2d3dService = readSource(QStringLiteral("Plugins/Registration2D3D/Registration2D3DService.h"));
    const QString opticalRegistrationService = readSource(QStringLiteral("Plugins/OpticalRegistration/OpticalRegistrationService.h"));

    QVERIFY2(!mainSource.contains(QStringLiteral("ThirdParty/CTK/CTK_install/lib/ctk-0.1")),
        "main.cpp still searches the legacy CTK runtime directory");
    QVERIFY2(!mainSource.contains(QStringLiteral("CTK runtime bridge removal")),
        "main.cpp still reports bridge-removal wording in active runtime diagnostics");
    QVERIFY2(!mainSource.contains(QStringLiteral("legacy CTK runtime work")),
        "main.cpp still reports legacy CTK runtime work in active runtime diagnostics");
    QVERIFY2(!startupHeader.contains(QStringLiteral("CTKFrameworkInit")),
        "StartupOrchestrator.h still exposes CTKFrameworkInit");
    QVERIFY2(!startupSource.contains(QStringLiteral("CTKFrameworkInit")),
        "StartupOrchestrator.cpp still uses CTKFrameworkInit");

    QVERIFY2(!userManagementService.contains(QStringLiteral("CTK服务架构设计原则")),
        "UserManagementService.h still describes the interface as CTK-based");
    QVERIFY2(!userManagementImpl.contains(QStringLiteral("CTK事件")),
        "UserManagementServiceImpl.h still exposes CTK event wording");
    QVERIFY2(!instrumentManagementService.contains(QStringLiteral("CTK服务架构设计原则")),
        "InstrumentManagementService.h still describes the interface as CTK-based");
    QVERIFY2(!pointRegistrationService.contains(QStringLiteral("CTK服务架构设计原则")),
        "PointRegistrationService.h still describes the interface as CTK-based");
    QVERIFY2(!fourViewService.contains(QStringLiteral("CTK服务架构设计原则")),
        "FourViewDisplayService.h still describes the interface as CTK-based");
    QVERIFY2(!registration2d3dService.contains(QStringLiteral("CTK服务架构设计原则")),
        "Registration2D3DService.h still describes the interface as CTK-based");
    QVERIFY2(!opticalRegistrationService.contains(QStringLiteral("CTK服务接口声明")),
        "OpticalRegistrationService.h still keeps a CTK service declaration comment");

    QVERIFY2(userManagementService.contains(QStringLiteral("User management service interface.")),
        "UserManagementService.h should keep a readable platform-neutral summary comment");
    QVERIFY2(userManagementImpl.contains(QStringLiteral("Concrete implementation of the user management service contract.")),
        "UserManagementServiceImpl.h should keep a readable platform-neutral summary comment");
    QVERIFY2(!userManagementService.contains(QStringLiteral("鐢ㄦ埛")),
        "UserManagementService.h still contains mojibake comments");
    QVERIFY2(!userManagementImpl.contains(QStringLiteral("鐢ㄦ埛")),
        "UserManagementServiceImpl.h still contains mojibake comments");
}

void PlatformNamingGovernanceContractTest::current_build_docs_do_not_describe_ctk_as_a_required_runtime_dependency()
{
    const QString buildDoc = readSource(QStringLiteral("docs/build_x64.md"));
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));

    QVERIFY2(!buildDoc.contains(QStringLiteral("Qt、CTK、VTK、ITK")),
        "docs/build_x64.md still describes CTK as a required current dependency");
    QVERIFY2(!buildDoc.contains(QStringLiteral("CTK 框架初始化")),
        "docs/build_x64.md still describes startup around CTK framework initialization");
    QVERIFY2(!rootCMake.contains(QStringLiteral("Qt/CTK/VTK/ITK")),
        "CMakeLists.txt still tells users that CTK is a required architecture dependency");
}

QTEST_APPLESS_MAIN(PlatformNamingGovernanceContractTest)
#include "PlatformNamingGovernanceContractTest.moc"
