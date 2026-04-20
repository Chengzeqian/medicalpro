#include "LoginPage.h"
#include "ui_LoginPage.h"

#include <QDebug>
#include <QSettings>

#include <utility>

LoginPageNew::LoginPageNew(QWidget* parent, LoginHandler loginHandler)
    : BasePage(parent)
    , ui(new Ui::LoginPage)
    , m_loginHandler(std::move(loginHandler))
{
    ui->setupUi(this);
    setObjectName("LoginPage");

    loadSavedCredentials();
}

LoginPageNew::~LoginPageNew()
{
    delete ui;
}

void LoginPageNew::onActivated()
{
    BasePage::onActivated();

    ui->statusLabel->clear();

    if (!ui->rememberCheckBox->isChecked()) {
        ui->passwordEdit->clear();
    }

    if (ui->usernameEdit->text().isEmpty()) {
        ui->usernameEdit->setFocus();
        return;
    }

    ui->passwordEdit->setFocus();
}

void LoginPageNew::on_loginButton_clicked()
{
    const QString username = ui->usernameEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();

    if (username.isEmpty()) {
        ui->statusLabel->setText(QStringLiteral("请输入用户名"));
        ui->usernameEdit->setFocus();
        return;
    }

    if (password.isEmpty()) {
        ui->statusLabel->setText(QStringLiteral("请输入密码"));
        ui->passwordEdit->setFocus();
        return;
    }

    ui->statusLabel->setText(QStringLiteral("正在登录..."));
    ui->statusLabel->setStyleSheet(QStringLiteral("color: #a0a0a0;"));

    const auto user = authenticate(username, password);
    if (user.isValid()) {
        m_currentUser = user.username;

        if (ui->rememberCheckBox->isChecked()) {
            saveCredentials();
        }

        ui->statusLabel->setText(QStringLiteral("登录成功"));
        ui->statusLabel->setStyleSheet(QStringLiteral("color: #27ae60;"));

        emit loginSucceeded(m_currentUser);
        emit navigateTo(toInt(PageIndex::ModuleSelection));
        return;
    }

    ui->statusLabel->setText(QStringLiteral("用户名或密码错误"));
    ui->statusLabel->setStyleSheet(QStringLiteral("color: #e94560;"));
    ui->passwordEdit->clear();
    ui->passwordEdit->setFocus();

    emit loginFailed(QStringLiteral("用户名或密码错误"));
}

void LoginPageNew::on_backButton_clicked()
{
    emit backToWelcomeRequested();
    emit navigateTo(toInt(PageIndex::Welcome));
}

void LoginPageNew::loadSavedCredentials()
{
    QSettings settings(QStringLiteral("MedicalPro"), QStringLiteral("NavigationSystem"));
    const bool rememberMe = settings.value(QStringLiteral("login/rememberMe"), false).toBool();

    ui->rememberCheckBox->setChecked(rememberMe);

    if (!rememberMe) return;

    ui->usernameEdit->setText(settings.value(QStringLiteral("login/username"), QString()).toString());
}

void LoginPageNew::saveCredentials()
{
    QSettings settings(QStringLiteral("MedicalPro"), QStringLiteral("NavigationSystem"));
    settings.setValue(QStringLiteral("login/rememberMe"), ui->rememberCheckBox->isChecked());
    settings.setValue(QStringLiteral("login/username"), ui->usernameEdit->text());
}

UserInfo LoginPageNew::authenticate(const QString& username, const QString& password) const
{
    if (!m_loginHandler) return {};
    return m_loginHandler(username, password);
}
