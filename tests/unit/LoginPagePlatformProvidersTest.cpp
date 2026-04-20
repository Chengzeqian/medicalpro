#include <QtTest/QtTest>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>

#include "Plugins/UserManagement/UserDataStructures.h"
#include "UI/NewPages/LoginPage.h"

class LoginPagePlatformProvidersTest : public QObject
{
    Q_OBJECT

private slots:
    void loginPage_uses_injected_identity_authenticator();
};

void LoginPagePlatformProvidersTest::loginPage_uses_injected_identity_authenticator()
{
    int callCount = 0;
    QString lastUsername;
    QString lastPassword;

    LoginPageNew page(nullptr, [&callCount, &lastUsername, &lastPassword](const QString& username, const QString& password) {
        ++callCount;
        lastUsername = username;
        lastPassword = password;

        UserInfo user;
        user.id = 12;
        user.username = QStringLiteral("doctor.li");
        user.realName = QStringLiteral("李医生");
        return user;
    });

    auto* usernameEdit = page.findChild<QLineEdit*>(QStringLiteral("usernameEdit"));
    auto* passwordEdit = page.findChild<QLineEdit*>(QStringLiteral("passwordEdit"));
    auto* loginButton = page.findChild<QPushButton*>(QStringLiteral("loginButton"));
    auto* statusLabel = page.findChild<QLabel*>(QStringLiteral("statusLabel"));

    QVERIFY(usernameEdit != nullptr);
    QVERIFY(passwordEdit != nullptr);
    QVERIFY(loginButton != nullptr);
    QVERIFY(statusLabel != nullptr);

    QSignalSpy successSpy(&page, &LoginPageNew::loginSucceeded);
    QSignalSpy failedSpy(&page, &LoginPageNew::loginFailed);

    usernameEdit->setText(QStringLiteral("doctor.li"));
    passwordEdit->setText(QStringLiteral("secret"));

    QTest::mouseClick(loginButton, Qt::LeftButton);

    QCOMPARE(callCount, 1);
    QCOMPARE(lastUsername, QStringLiteral("doctor.li"));
    QCOMPARE(lastPassword, QStringLiteral("secret"));
    QCOMPARE(successSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(successSpy.takeFirst().at(0).toString(), QStringLiteral("doctor.li"));
    QCOMPARE(page.getCurrentUser(), QStringLiteral("doctor.li"));
    QVERIFY(!statusLabel->text().isEmpty());
}

QTEST_MAIN(LoginPagePlatformProvidersTest)
#include "LoginPagePlatformProvidersTest.moc"
