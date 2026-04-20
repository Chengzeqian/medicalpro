#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class UiCtkDecouplingAcceptanceTest : public QObject
{
    Q_OBJECT

private slots:
    void mainInterfaceWidget_runtime_status_is_not_ctk_coupled();
    void navigationPage_service_loading_is_not_ctk_coupled();

private:
    QString readSource(const QString& relativePath) const;
    void verifyNoDirectCtkCoupling(const QString& relativePath) const;
};

QString UiCtkDecouplingAcceptanceTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("无法读取源文件: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void UiCtkDecouplingAcceptanceTest::verifyNoDirectCtkCoupling(const QString& relativePath) const
{
    const QString source = readSource(relativePath);
    QVERIFY2(!source.contains(QStringLiteral("CTKManager::instance(")),
             qPrintable(QStringLiteral("%1 仍然直接调用 CTKManager::instance()").arg(relativePath)));
    QVERIFY2(!source.contains(QStringLiteral("getService<")),
             qPrintable(QStringLiteral("%1 仍然直接调用 getService<...>()").arg(relativePath)));
}

void UiCtkDecouplingAcceptanceTest::mainInterfaceWidget_runtime_status_is_not_ctk_coupled()
{
    verifyNoDirectCtkCoupling(QStringLiteral("UI/MainInterfaceWidget.cpp"));
}

void UiCtkDecouplingAcceptanceTest::navigationPage_service_loading_is_not_ctk_coupled()
{
    verifyNoDirectCtkCoupling(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
}

QTEST_APPLESS_MAIN(UiCtkDecouplingAcceptanceTest)
#include "UiCtkDecouplingAcceptanceTest.moc"
