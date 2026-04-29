#include <QtTest/QtTest>

#include <QFile>
#include <QString>

class UiPlatformDecouplingAcceptanceTest : public QObject
{
    Q_OBJECT

private slots:
    void mainInterfaceWidget_runtime_status_is_not_ctk_coupled();
    void navigationPage_service_loading_is_not_ctk_coupled();

private:
    QString readSource(const QString& relativePath) const;
    void verifyNoDirectCtkCoupling(const QString& relativePath) const;
};

QString UiPlatformDecouplingAcceptanceTest::readSource(const QString& relativePath) const
{
    QFile sourceFile(QStringLiteral(MEDICALPRO_SOURCE_DIR "/") + relativePath);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("failed to read source file: %1").arg(relativePath)), __FILE__, __LINE__);
        return {};
    }
    return QString::fromUtf8(sourceFile.readAll());
}

void UiPlatformDecouplingAcceptanceTest::verifyNoDirectCtkCoupling(const QString& relativePath) const
{
    const QString source = readSource(relativePath);
    QVERIFY2(!source.contains(QStringLiteral("CTKManager::instance(")),
        qPrintable(QStringLiteral("%1 still directly calls CTKManager::instance()").arg(relativePath)));
    QVERIFY2(!source.contains(QStringLiteral("getService<")),
        qPrintable(QStringLiteral("%1 still directly calls getService<...>()").arg(relativePath)));
    QVERIFY2(!source.contains(QStringLiteral("CTK_PLUGIN_FRAMEWORK")),
        qPrintable(QStringLiteral("%1 still uses CTK_PLUGIN_FRAMEWORK conditional compilation").arg(relativePath)));
}

void UiPlatformDecouplingAcceptanceTest::mainInterfaceWidget_runtime_status_is_not_ctk_coupled()
{
    verifyNoDirectCtkCoupling(QStringLiteral("UI/MainInterfaceWidget.cpp"));
}

void UiPlatformDecouplingAcceptanceTest::navigationPage_service_loading_is_not_ctk_coupled()
{
    verifyNoDirectCtkCoupling(QStringLiteral("UI/NewPages/NavigationPage.h"));
    verifyNoDirectCtkCoupling(QStringLiteral("UI/NewPages/NavigationPage.cpp"));
}

QTEST_APPLESS_MAIN(UiPlatformDecouplingAcceptanceTest)
#include "UiPlatformDecouplingAcceptanceTest.moc"
