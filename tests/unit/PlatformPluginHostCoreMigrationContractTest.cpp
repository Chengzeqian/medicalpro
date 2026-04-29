#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>

class PlatformPluginHostCoreMigrationContractTest : public QObject
{
    Q_OBJECT

private slots:
    void core_plugins_expose_platform_host_entry_points_and_registry_access();

private:
    QString readSource(const QString& relativePath) const;
};

QString PlatformPluginHostCoreMigrationContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

void PlatformPluginHostCoreMigrationContractTest::core_plugins_expose_platform_host_entry_points_and_registry_access()
{
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/RegistrationCore/registration_core_module.cpp")).exists(),
        "registration_core_module.cpp must exist");
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/Registration2D3D/registration_2d3d_module.cpp")).exists(),
        "registration_2d3d_module.cpp must exist");
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/OpticalTracking/optical_tracking_module.cpp")).exists(),
        "optical_tracking_module.cpp must exist");
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/OpticalRegistration/optical_registration_module.cpp")).exists(),
        "optical_registration_module.cpp must exist");
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/PointRegistration/point_registration_module.cpp")).exists(),
        "point_registration_module.cpp must exist");
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/InstrumentManagement/instrument_management_module.cpp")).exists(),
        "instrument_management_module.cpp must exist");
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/BoneSegmentation/bone_segmentation_module.cpp")).exists(),
        "bone_segmentation_module.cpp must exist");

    const QString registrationHeader = readSource(QStringLiteral("Plugins/RegistrationCore/RegistrationServiceImpl.h"));
    const QString registrationSource = readSource(QStringLiteral("Plugins/RegistrationCore/RegistrationServiceImpl.cpp"));
    const QString registration2d3dHeader = readSource(QStringLiteral("Plugins/Registration2D3D/Registration2D3DServiceImpl.h"));
    const QString registration2d3dSource = readSource(QStringLiteral("Plugins/Registration2D3D/Registration2D3DServiceImpl.cpp"));
    const QString opticalTrackingHeader = readSource(QStringLiteral("Plugins/OpticalTracking/OpticalTrackingServiceImpl.h"));
    const QString opticalTrackingSource = readSource(QStringLiteral("Plugins/OpticalTracking/OpticalTrackingServiceImpl.cpp"));
    const QString opticalRegistrationHeader = readSource(QStringLiteral("Plugins/OpticalRegistration/OpticalRegistrationServiceImpl.h"));
    const QString opticalRegistrationSource = readSource(QStringLiteral("Plugins/OpticalRegistration/OpticalRegistrationServiceImpl.cpp"));
    const QString platformPluginHostHeader =
        readSource(QStringLiteral("Framework/Platform/Kernel/platform_plugin_host.h"));
    const QString runtimeHostAdapterSource =
        readSource(QStringLiteral("Framework/Platform/Kernel/platform_runtime_host_adapter.cpp"));

    const QStringList removedActivatorFiles = {
        QStringLiteral("Plugins/RegistrationCore/RegistrationActivator.h"),
        QStringLiteral("Plugins/RegistrationCore/RegistrationActivator.cpp"),
        QStringLiteral("Plugins/Registration2D3D/Registration2D3DActivator.h"),
        QStringLiteral("Plugins/Registration2D3D/Registration2D3DActivator.cpp"),
        QStringLiteral("Plugins/OpticalTracking/OpticalTrackingActivator.h"),
        QStringLiteral("Plugins/OpticalTracking/OpticalTrackingActivator.cpp"),
        QStringLiteral("Plugins/OpticalRegistration/OpticalRegistrationActivator.h"),
        QStringLiteral("Plugins/OpticalRegistration/OpticalRegistrationActivator.cpp"),
        QStringLiteral("Plugins/PointRegistration/PointRegistrationActivator.h"),
        QStringLiteral("Plugins/PointRegistration/PointRegistrationActivator.cpp"),
        QStringLiteral("Plugins/InstrumentManagement/InstrumentManagementActivator.h"),
        QStringLiteral("Plugins/InstrumentManagement/InstrumentManagementActivator.cpp"),
        QStringLiteral("Plugins/BoneSegmentation/SegmentationActivator.h"),
        QStringLiteral("Plugins/BoneSegmentation/SegmentationActivator.cpp")
    };
    for (const auto& removedActivatorFile : removedActivatorFiles) {
        QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + removedActivatorFile).exists(),
            qPrintable(QStringLiteral("%1 must be deleted after CTK runtime exit").arg(removedActivatorFile)));
    }

    QVERIFY2(registrationHeader.contains(QStringLiteral("void setServiceRegistry(PlatformServiceRegistry* serviceRegistry);")),
        "RegistrationServiceImpl must expose setServiceRegistry()");
    QVERIFY2(registrationHeader.contains(QStringLiteral("PlatformServiceRegistry* m_serviceRegistry;")),
        "RegistrationServiceImpl must store PlatformServiceRegistry");
    QVERIFY2(!registrationHeader.contains(QStringLiteral("setPluginContext")),
        "RegistrationServiceImpl.h should not expose CTK plugin context injection");
    QVERIFY2(!registrationHeader.contains(QStringLiteral("ctkPluginContext")),
        "RegistrationServiceImpl.h should not depend on ctkPluginContext");
    QVERIFY2(registrationSource.contains(QStringLiteral("m_serviceRegistry->service(QStringLiteral(\"Registration2D3DService\"))")),
        "RegistrationServiceImpl must resolve Registration2D3DService through PlatformServiceRegistry");
    QVERIFY2(!registrationSource.contains(QStringLiteral("ctkServiceReference")),
        "RegistrationServiceImpl.cpp should stop depending on ctkServiceReference");
    QVERIFY2(!registrationSource.contains(QStringLiteral("m_context")),
        "RegistrationServiceImpl.cpp should not retain CTK plugin context state");
    QVERIFY2(registration2d3dHeader.contains(QStringLiteral("void setServiceRegistry(PlatformServiceRegistry* serviceRegistry)")),
        "Registration2D3DServiceImpl must expose setServiceRegistry()");
    QVERIFY2(registration2d3dHeader.contains(QStringLiteral("PlatformServiceRegistry* m_serviceRegistry;")),
        "Registration2D3DServiceImpl must store PlatformServiceRegistry");
    QVERIFY2(!registration2d3dHeader.contains(QStringLiteral("setContext")),
        "Registration2D3DServiceImpl.h should not expose CTK context injection");
    QVERIFY2(!registration2d3dHeader.contains(QStringLiteral("ctkPluginContext")),
        "Registration2D3DServiceImpl.h should not depend on ctkPluginContext");
    QVERIFY2(!registration2d3dSource.contains(QStringLiteral("m_context")),
        "Registration2D3DServiceImpl.cpp should not retain CTK plugin context state");
    QVERIFY2(opticalTrackingHeader.contains(QStringLiteral("void setServiceRegistry(PlatformServiceRegistry* serviceRegistry);")),
        "OpticalTrackingServiceImpl must expose setServiceRegistry()");
    QVERIFY2(opticalTrackingHeader.contains(QStringLiteral("PlatformServiceRegistry* m_serviceRegistry;")),
        "OpticalTrackingServiceImpl must store PlatformServiceRegistry");
    QVERIFY2(!opticalTrackingHeader.contains(QStringLiteral("setPluginContext")),
        "OpticalTrackingServiceImpl.h should not expose CTK plugin context injection");
    QVERIFY2(!opticalTrackingHeader.contains(QStringLiteral("ctkPluginContext")),
        "OpticalTrackingServiceImpl.h should not depend on ctkPluginContext");
    QVERIFY2(!opticalTrackingHeader.contains(QStringLiteral("ctkServiceReference")),
        "OpticalTrackingServiceImpl.h should not depend on ctkServiceReference");
    QVERIFY2(!opticalTrackingSource.contains(QStringLiteral("m_pluginContext")),
        "OpticalTrackingServiceImpl.cpp should not retain CTK plugin context state");
    QVERIFY2(opticalRegistrationHeader.contains(QStringLiteral("void setServiceRegistry(PlatformServiceRegistry* serviceRegistry);")),
        "OpticalRegistrationServiceImpl must expose setServiceRegistry()");
    QVERIFY2(opticalRegistrationHeader.contains(QStringLiteral("PlatformServiceRegistry* m_serviceRegistry;")),
        "OpticalRegistrationServiceImpl must store PlatformServiceRegistry");
    QVERIFY2(!opticalRegistrationHeader.contains(QStringLiteral("ctkPluginContext")),
        "OpticalRegistrationServiceImpl.h should not depend on ctkPluginContext");
    QVERIFY2(!opticalRegistrationHeader.contains(QStringLiteral("ctkServiceReference")),
        "OpticalRegistrationServiceImpl.h should not depend on ctkServiceReference");
    QVERIFY2(opticalRegistrationSource.contains(QStringLiteral("m_serviceRegistry->service(QStringLiteral(\"OpticalTrackingService\"))")),
        "OpticalRegistrationServiceImpl must resolve OpticalTrackingService through PlatformServiceRegistry");
    QVERIFY2(!opticalRegistrationSource.contains(QStringLiteral("m_pluginContext")),
        "OpticalRegistrationServiceImpl.cpp should not retain CTK plugin context state");
    QVERIFY2(platformPluginHostHeader.contains(QStringLiteral("static PlatformPluginHost& sharedInstance();")),
        "PlatformPluginHost must expose sharedInstance()");
    QVERIFY2(runtimeHostAdapterSource.contains(QStringLiteral("PlatformPluginHost::sharedInstance()")),
        "platform_runtime_host_adapter.cpp must consume PlatformPluginHost shared services");
}

QTEST_APPLESS_MAIN(PlatformPluginHostCoreMigrationContractTest)
#include "PlatformPluginHostCoreMigrationContractTest.moc"
