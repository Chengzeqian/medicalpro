#include <QtTest/QtTest>

#include <QFile>

class RuntimeBuildDeploymentContractTest : public QObject
{
    Q_OBJECT

private slots:
    void ctk_runtime_exit_removes_legacy_build_switches_and_ctk_discovery();
    void ctk_runtime_artifact_deployment_has_been_removed();
    void runtime_artifact_contracts_validate_platform_layout_without_legacy_plugin_dlls();

private:
    QString readSource(const QString& relativePath) const;
};

QString RuntimeBuildDeploymentContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

void RuntimeBuildDeploymentContractTest::ctk_runtime_exit_removes_legacy_build_switches_and_ctk_discovery()
{
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));

    QVERIFY2(!rootCMake.contains(QStringLiteral("ENABLE_CTK_PLUGIN_FRAMEWORK")),
        "CMakeLists.txt must not expose ENABLE_CTK_PLUGIN_FRAMEWORK after build-time CTK removal");
    QVERIFY2(!rootCMake.contains(QStringLiteral("MEDICALPRO_DEPLOY_CTK_RUNTIME")),
        "CMakeLists.txt must not expose MEDICALPRO_DEPLOY_CTK_RUNTIME after build-time CTK removal");
    QVERIFY2(!rootCMake.contains(QStringLiteral("MEDICALPRO_BUILD_LEGACY_CTK_PLUGINS")),
        "CMakeLists.txt must not expose MEDICALPRO_BUILD_LEGACY_CTK_PLUGINS after build-time CTK removal");
    QVERIFY2(!rootCMake.contains(QStringLiteral("CTK_FOUND")),
        "CMakeLists.txt must not retain CTK_FOUND gates after build-time CTK removal");
    QVERIFY2(!rootCMake.contains(QStringLiteral("CTK_LIBRARIES")),
        "CMakeLists.txt must not retain CTK runtime linkage after build-time CTK removal");
    QVERIFY2(!rootCMake.contains(QStringLiteral("CTK_INCLUDE_DIRS")),
        "CMakeLists.txt must not retain CTK include propagation after build-time CTK removal");
}

void RuntimeBuildDeploymentContractTest::ctk_runtime_artifact_deployment_has_been_removed()
{
    const QString rootCMake = readSource(QStringLiteral("CMakeLists.txt"));

    QVERIFY2(!rootCMake.contains(QStringLiteral("liborg_commontk_eventadmin.dll")),
        "CMakeLists.txt still deploys EventAdmin");
    QVERIFY2(!rootCMake.contains(QStringLiteral("CTKPluginFramework.dll")),
        "CMakeLists.txt still deploys CTKPluginFramework.dll");
    QVERIFY2(!rootCMake.contains(QStringLiteral("CTK*.dll")),
        "CMakeLists.txt still deploys wildcard CTK runtime DLLs");
}

void RuntimeBuildDeploymentContractTest::runtime_artifact_contracts_validate_platform_layout_without_legacy_plugin_dlls()
{
    const QString testsCMake = readSource(QStringLiteral("tests/CMakeLists.txt"));
    const QString runtimeVerify = readSource(QStringLiteral("tests/runtime/verify_runtime_artifacts.cmake"));

    QVERIFY2(!testsCMake.contains(QStringLiteral("TARGET UserManagement AND TARGET DicomViewer AND TARGET FourViewDisplay")),
        "runtime_artifact_layout_test must not depend on legacy CTK plugin DLL targets");
    QVERIFY2(testsCMake.contains(QStringLiteral("-Drequire_platform_descriptors=ON")),
        "runtime_artifact_layout_test must validate platform descriptor layout");
    QVERIFY2(!runtimeVerify.contains(QStringLiteral("CTKPluginFramework.dll")),
        "runtime artifact verification must not expect CTKPluginFramework.dll");
    QVERIFY2(!runtimeVerify.contains(QStringLiteral("EventAdmin")),
        "runtime artifact verification must not expect CTK EventAdmin artifacts");
    QVERIFY2(!runtimeVerify.contains(QStringLiteral("UserManagement.dll")),
        "runtime artifact verification must not require UserManagement.dll");
    QVERIFY2(!runtimeVerify.contains(QStringLiteral("DicomViewer.dll")),
        "runtime artifact verification must not require DicomViewer.dll");
    QVERIFY2(!runtimeVerify.contains(QStringLiteral("FourViewDisplay.dll")),
        "runtime artifact verification must not require FourViewDisplay.dll");
    QVERIFY2(!runtimeVerify.contains(QStringLiteral("UserManagement.manifest")),
        "runtime artifact verification must not require CTK plugin manifests");
}

QTEST_APPLESS_MAIN(RuntimeBuildDeploymentContractTest)
#include "RuntimeBuildDeploymentContractTest.moc"
