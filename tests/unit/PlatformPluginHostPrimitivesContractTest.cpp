#include <QtTest/QtTest>

#include <QFile>

class PlatformPluginHostPrimitivesContractTest : public QObject
{
    Q_OBJECT

private slots:
    void platform_plugin_host_primitives_define_module_registry_and_event_bus_boundaries();

private:
    QString readSource(const QString& relativePath) const;
};

QString PlatformPluginHostPrimitivesContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

void PlatformPluginHostPrimitivesContractTest::platform_plugin_host_primitives_define_module_registry_and_event_bus_boundaries()
{
    const QString modulePortsHeader =
        readSource(QStringLiteral("Framework/Platform/Contracts/platform_module_ports.h"));
    const QString serviceRegistryHeader =
        readSource(QStringLiteral("Framework/Platform/Kernel/platform_service_registry.h"));
    const QString eventBusHeader =
        readSource(QStringLiteral("Framework/Platform/Kernel/platform_event_bus.h"));
    const QString pluginHostHeader =
        readSource(QStringLiteral("Framework/Platform/Kernel/platform_plugin_host.h"));

    QVERIFY2(modulePortsHeader.contains(QStringLiteral("struct PlatformModuleContext")),
        "platform_module_ports.h must define PlatformModuleContext");
    QVERIFY2(modulePortsHeader.contains(QStringLiteral("PlatformServiceRegistry* serviceRegistry = nullptr;")),
        "PlatformModuleContext must expose PlatformServiceRegistry");
    QVERIFY2(modulePortsHeader.contains(QStringLiteral("IPlatformEventBusPort* eventBus = nullptr;")),
        "PlatformModuleContext must expose IPlatformEventBusPort");
    QVERIFY2(modulePortsHeader.contains(QStringLiteral("class FRAMEWORK_EXPORT IPlatformModuleActivator")),
        "platform_module_ports.h must define IPlatformModuleActivator");
    QVERIFY2(modulePortsHeader.contains(QStringLiteral("virtual QString pluginId() const = 0;")),
        "IPlatformModuleActivator must expose pluginId()");
    QVERIFY2(modulePortsHeader.contains(QStringLiteral("virtual bool start(PlatformModuleContext& context) = 0;")),
        "IPlatformModuleActivator must expose start()");
    QVERIFY2(modulePortsHeader.contains(QStringLiteral("virtual void stop(PlatformModuleContext& context) = 0;")),
        "IPlatformModuleActivator must expose stop()");

    QVERIFY2(serviceRegistryHeader.contains(QStringLiteral("class FRAMEWORK_EXPORT PlatformServiceRegistry")),
        "platform_service_registry.h must define PlatformServiceRegistry");
    QVERIFY2(serviceRegistryHeader.contains(QStringLiteral("void registerService(const QString& pluginId, const QString& serviceId, QObject* service);")),
        "PlatformServiceRegistry must expose registerService()");
    QVERIFY2(serviceRegistryHeader.contains(QStringLiteral("QObject* service(const QString& serviceId) const;")),
        "PlatformServiceRegistry must expose service(serviceId)");
    QVERIFY2(serviceRegistryHeader.contains(QStringLiteral("void unregisterPlugin(const QString& pluginId);")),
        "PlatformServiceRegistry must expose unregisterPlugin()");

    QVERIFY2(eventBusHeader.contains(QStringLiteral("class FRAMEWORK_EXPORT PlatformEventBus final")),
        "platform_event_bus.h must define PlatformEventBus");
    QVERIFY2(eventBusHeader.contains(QStringLiteral("void publish(const QString& topic, const QVariantMap& payload) override;")),
        "PlatformEventBus must implement publish()");

    QVERIFY2(pluginHostHeader.contains(QStringLiteral("class FRAMEWORK_EXPORT PlatformPluginHost")),
        "platform_plugin_host.h must define PlatformPluginHost");
    QVERIFY2(pluginHostHeader.contains(QStringLiteral("void registerActivator(std::unique_ptr<IPlatformModuleActivator> activator);")),
        "PlatformPluginHost must expose registerActivator()");
    QVERIFY2(pluginHostHeader.contains(QStringLiteral("bool startModule(const QString& pluginId);")),
        "PlatformPluginHost must expose startModule()");
    QVERIFY2(pluginHostHeader.contains(QStringLiteral("void stopModule(const QString& pluginId);")),
        "PlatformPluginHost must expose stopModule()");
    QVERIFY2(pluginHostHeader.contains(QStringLiteral("PlatformServiceRegistry* serviceRegistry() const;")),
        "PlatformPluginHost must expose serviceRegistry()");
    QVERIFY2(pluginHostHeader.contains(QStringLiteral("IPlatformEventBusPort* eventBus() const;")),
        "PlatformPluginHost must expose eventBus()");
}

QTEST_APPLESS_MAIN(PlatformPluginHostPrimitivesContractTest)
#include "PlatformPluginHostPrimitivesContractTest.moc"
