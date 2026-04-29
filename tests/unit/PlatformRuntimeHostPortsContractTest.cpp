#include <QtTest/QtTest>

#include <QFile>

class PlatformRuntimeHostPortsContractTest : public QObject
{
    Q_OBJECT

private slots:
    void runtime_host_ports_define_runtime_service_and_event_bus_boundaries();

private:
    QString readSource(const QString& relativePath) const;
};

QString PlatformRuntimeHostPortsContractTest::readSource(const QString& relativePath) const
{
    QFile file(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

void PlatformRuntimeHostPortsContractTest::runtime_host_ports_define_runtime_service_and_event_bus_boundaries()
{
    const QString source = readSource(QStringLiteral("Framework/Platform/Contracts/platform_runtime_host_ports.h"));
    const int runtimeHostPortStart = source.indexOf(QStringLiteral("class FRAMEWORK_EXPORT IPlatformRuntimeHostPort"));
    const int serviceAccessPortStart = source.indexOf(QStringLiteral("class FRAMEWORK_EXPORT IPlatformServiceAccessPort"));
    const int eventBusPortStart = source.indexOf(QStringLiteral("class FRAMEWORK_EXPORT IPlatformEventBusPort"));

    QVERIFY2(source.contains(QStringLiteral("class FRAMEWORK_EXPORT IPlatformRuntimeHostPort")),
        "missing IPlatformRuntimeHostPort");
    QVERIFY2(source.contains(QStringLiteral("class FRAMEWORK_EXPORT IPlatformServiceAccessPort")),
        "missing IPlatformServiceAccessPort");
    QVERIFY2(source.contains(QStringLiteral("class FRAMEWORK_EXPORT IPlatformEventBusPort")),
        "missing IPlatformEventBusPort");
    QVERIFY2(!source.contains(QStringLiteral("class FRAMEWORK_EXPORT IPlatformLegacyPluginRuntimePort")),
        "legacy runtime port must be removed after bridge cleanup");
    QVERIFY2(runtimeHostPortStart >= 0 && serviceAccessPortStart > runtimeHostPortStart && eventBusPortStart > serviceAccessPortStart,
        "runtime host port declarations must stay ordered");

    const QString runtimeHostPortSection = source.mid(runtimeHostPortStart, serviceAccessPortStart - runtimeHostPortStart);
    const QString serviceAccessPortSection = source.mid(serviceAccessPortStart, eventBusPortStart - serviceAccessPortStart);

    QVERIFY2(runtimeHostPortSection.contains(QStringLiteral("virtual bool initialize(QApplication* app) = 0;")),
        "runtime host must own initialization");
    QVERIFY2(runtimeHostPortSection.contains(QStringLiteral("virtual bool start() = 0;")),
        "runtime host must own start");
    QVERIFY2(runtimeHostPortSection.contains(QStringLiteral("virtual bool stop() = 0;")),
        "runtime host must own stop");
    QVERIFY2(!runtimeHostPortSection.contains(QStringLiteral("virtual QObject* pluginEventSource() const = 0;")),
        "runtime host should no longer expose the legacy CTK event source directly");
    QVERIFY2(!runtimeHostPortSection.contains(QStringLiteral("setSafeMode(")),
        "runtime host must not retain CTK safe-mode handoff");
    QVERIFY2(!runtimeHostPortSection.contains(QStringLiteral("setDescriptorPolicyContext(")),
        "runtime host must not retain CTK descriptor policy handoff");
    QVERIFY2(!runtimeHostPortSection.contains(QStringLiteral("installPlugin(")),
        "runtime host must not retain CTK bundle installation");
    QVERIFY2(!runtimeHostPortSection.contains(QStringLiteral("deferredPluginIds(")),
        "runtime host must not retain CTK deferred plugin state");
    QVERIFY2(!serviceAccessPortSection.contains(QStringLiteral("ImageInteractionService")),
        "service access should not keep exposing the legacy ImageInteractionService");
    QVERIFY2(!serviceAccessPortSection.contains(QStringLiteral("PatientDatabaseService")),
        "service access should not keep exposing the removed PatientDatabaseService");
    QVERIFY2(serviceAccessPortSection.contains(QStringLiteral("virtual RegistrationService* registrationService() const = 0;")),
        "service access must expose RegistrationService");
    QVERIFY2(serviceAccessPortSection.contains(QStringLiteral("virtual OpticalTrackingService* opticalTrackingService() const = 0;")),
        "service access must expose OpticalTrackingService");
    QVERIFY2(source.contains(QStringLiteral("virtual void publish(")),
        "event bus must expose publish()");
}

QTEST_APPLESS_MAIN(PlatformRuntimeHostPortsContractTest)
#include "PlatformRuntimeHostPortsContractTest.moc"
