#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>

class PlatformPluginHostUiMigrationContractTest : public QObject
{
    Q_OBJECT

private slots:
    void ui_plugins_expose_platform_host_bridge_and_event_bus_migration();

private:
    QString readSource(const QString& relativePath) const;
};

QString PlatformPluginHostUiMigrationContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

void PlatformPluginHostUiMigrationContractTest::ui_plugins_expose_platform_host_bridge_and_event_bus_migration()
{
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/DicomViewer/dicom_viewer_module.cpp")).exists(),
        "dicom_viewer_module.cpp must exist");
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/FourViewDisplay/four_view_display_module.cpp")).exists(),
        "four_view_display_module.cpp must exist");
    QVERIFY2(QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/Plugins/UserManagement/user_management_module.cpp")).exists(),
        "user_management_module.cpp must exist");

    const QString dicomServiceHeader = readSource(QStringLiteral("Plugins/DicomViewer/DicomViewerServiceImpl.h"));
    const QString dicomWidgetHeader = readSource(QStringLiteral("Plugins/DicomViewer/DicomViewerWidget.h"));
    const QString dicomWidgetSource = readSource(QStringLiteral("Plugins/DicomViewer/DicomViewerWidget.cpp"));
    const QString fourViewServiceHeader = readSource(QStringLiteral("Plugins/FourViewDisplay/FourViewDisplayServiceImpl.h"));
    const QString fourViewServiceSource = readSource(QStringLiteral("Plugins/FourViewDisplay/FourViewDisplayServiceImpl.cpp"));
    const QString userManagementServiceHeader = readSource(QStringLiteral("Plugins/UserManagement/UserManagementServiceImpl.h"));
    const QString userManagementServiceSource = readSource(QStringLiteral("Plugins/UserManagement/UserManagementServiceImpl.cpp"));
    const QStringList removedActivatorFiles = {
        QStringLiteral("Plugins/DicomViewer/DicomViewerActivator.h"),
        QStringLiteral("Plugins/DicomViewer/DicomViewerActivator.cpp"),
        QStringLiteral("Plugins/FourViewDisplay/FourViewDisplayActivator.h"),
        QStringLiteral("Plugins/FourViewDisplay/FourViewDisplayActivator.cpp"),
        QStringLiteral("Plugins/UserManagement/UserManagementActivator.h"),
        QStringLiteral("Plugins/UserManagement/UserManagementActivator.cpp")
    };
    for (const auto& removedActivatorFile : removedActivatorFiles) {
        QVERIFY2(!QFileInfo(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + removedActivatorFile).exists(),
            qPrintable(QStringLiteral("%1 must be deleted after CTK runtime exit").arg(removedActivatorFile)));
    }

    QVERIFY2(!dicomServiceHeader.contains(QStringLiteral("ctkPluginContext")),
        "DicomViewerServiceImpl.h should not depend on ctkPluginContext");
    QVERIFY2(!dicomServiceHeader.contains(QStringLiteral("setPluginContext")),
        "DicomViewerServiceImpl.h should not expose setPluginContext()");
    QVERIFY2(!dicomWidgetHeader.contains(QStringLiteral("ctkPluginContext")),
        "DicomViewerWidget.h should not depend on ctkPluginContext");
    QVERIFY2(!dicomWidgetHeader.contains(QStringLiteral("ctkEventAdmin")),
        "DicomViewerWidget.h should not depend on ctkEventAdmin");
    QVERIFY2(!dicomWidgetHeader.contains(QStringLiteral("initializeCTKService")),
        "DicomViewerWidget.h should not expose initializeCTKService()");
    QVERIFY2(!dicomWidgetSource.contains(QStringLiteral("ctkEventAdmin")),
        "DicomViewerWidget.cpp should not query ctkEventAdmin");
    QVERIFY2(!dicomWidgetSource.contains(QStringLiteral("getServiceReference<ctkEventAdmin>()")),
        "DicomViewerWidget.cpp should not request EventAdmin through CTK");
    QVERIFY2(!dicomWidgetSource.contains(QStringLiteral("m_ctkContext")),
        "DicomViewerWidget.cpp should not retain CTK context state");
    QVERIFY2(!dicomWidgetSource.contains(QStringLiteral("m_eventAdmin")),
        "DicomViewerWidget.cpp should not retain EventAdmin state");

    QVERIFY2(!fourViewServiceHeader.contains(QStringLiteral("ctkPluginContext")),
        "FourViewDisplayServiceImpl.h should not depend on ctkPluginContext");
    QVERIFY2(!fourViewServiceHeader.contains(QStringLiteral("setPluginContext")),
        "FourViewDisplayServiceImpl.h should not expose setPluginContext()");
    QVERIFY2(!fourViewServiceSource.contains(QStringLiteral("setPluginContext")),
        "FourViewDisplayServiceImpl.cpp should not define setPluginContext()");
    QVERIFY2(userManagementServiceHeader.contains(QStringLiteral("void setEventBus(IPlatformEventBusPort* eventBus);")),
        "UserManagementServiceImpl must expose setEventBus()");
    QVERIFY2(userManagementServiceHeader.contains(QStringLiteral("IPlatformEventBusPort* m_eventBus;")),
        "UserManagementServiceImpl must store IPlatformEventBusPort");
    QVERIFY2(!userManagementServiceHeader.contains(QStringLiteral("ctkEventAdmin")),
        "UserManagementServiceImpl.h should not depend on ctkEventAdmin");
    QVERIFY2(!userManagementServiceHeader.contains(QStringLiteral("sendCTKEvent")),
        "UserManagementServiceImpl.h should stop exposing CTK-named event helpers");
    QVERIFY2(userManagementServiceSource.contains(QStringLiteral("m_eventBus->publish(topic, properties);")),
        "UserManagementServiceImpl must publish through platform event bus");
    QVERIFY2(!userManagementServiceSource.contains(QStringLiteral("ctkEventAdmin")),
        "UserManagementServiceImpl.cpp should stop depending on ctkEventAdmin");
    QVERIFY2(!userManagementServiceSource.contains(QStringLiteral("sendCTKEvent")),
        "UserManagementServiceImpl.cpp should stop using CTK-named event helpers");
}

QTEST_APPLESS_MAIN(PlatformPluginHostUiMigrationContractTest)
#include "PlatformPluginHostUiMigrationContractTest.moc"
