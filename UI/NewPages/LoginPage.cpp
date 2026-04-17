#include "LoginPage.h"
#include "ui_LoginPage.h"

#include <QSettings>
#include <QDebug>

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Plugins/UserManagement/UserManagementService.h"
#include "Framework/CTKManager.h"
#endif

LoginPageNew::LoginPageNew(QWidget* parent)
    : BasePage(parent)
    , ui(new Ui::LoginPage)
{
    ui->setupUi(this);
    setObjectName("LoginPage");

    // 加载保存的凭据
    loadSavedCredentials();
}

LoginPageNew::~LoginPageNew()
{
    delete ui;
}

void LoginPageNew::onActivated()
{
    BasePage::onActivated();

    // 清空状态消息
    ui->statusLabel->clear();

    // 如果没有记住密码，清空密码框
    if (!ui->rememberCheckBox->isChecked()) {
        ui->passwordEdit->clear();
    }

    // 聚焦到用户名输入框
    if (ui->usernameEdit->text().isEmpty()) {
        ui->usernameEdit->setFocus();
    } else {
        ui->passwordEdit->setFocus();
    }
}

void LoginPageNew::on_loginButton_clicked()
{
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();

    if (username.isEmpty()) {
        ui->statusLabel->setText("请输入用户名");
        ui->usernameEdit->setFocus();
        return;
    }

    if (password.isEmpty()) {
        ui->statusLabel->setText("请输入密码");
        ui->passwordEdit->setFocus();
        return;
    }

    ui->statusLabel->setText("正在登录...");
    ui->statusLabel->setStyleSheet("color: #a0a0a0;");

    // 验证登录
    if (validateLogin(username, password)) {
        m_currentUser = username;

        // 保存凭据（如果选择了记住密码）
        if (ui->rememberCheckBox->isChecked()) {
            saveCredentials();
        }

        ui->statusLabel->setText("登录成功！");
        ui->statusLabel->setStyleSheet("color: #27ae60;");

        emit loginSucceeded(username);
        emit navigateTo(toInt(PageIndex::ModuleSelection));
    } else {
        ui->statusLabel->setText("用户名或密码错误");
        ui->statusLabel->setStyleSheet("color: #e94560;");
        ui->passwordEdit->clear();
        ui->passwordEdit->setFocus();

        emit loginFailed("用户名或密码错误");
    }
}

void LoginPageNew::on_backButton_clicked()
{
    emit backToWelcomeRequested();  // MainInterfaceWidget期望的信号
    emit navigateTo(toInt(PageIndex::Welcome));
}

void LoginPageNew::loadSavedCredentials()
{
    QSettings settings("MedicalPro", "NavigationSystem");
    bool rememberMe = settings.value("login/rememberMe", false).toBool();

    ui->rememberCheckBox->setChecked(rememberMe);

    if (rememberMe) {
        ui->usernameEdit->setText(settings.value("login/username", "").toString());
        // 注意：实际应用中不应该保存明文密码
        // 这里仅作演示
    }
}

void LoginPageNew::saveCredentials()
{
    QSettings settings("MedicalPro", "NavigationSystem");
    settings.setValue("login/rememberMe", ui->rememberCheckBox->isChecked());
    settings.setValue("login/username", ui->usernameEdit->text());
}

bool LoginPageNew::validateLogin(const QString& username, const QString& password)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    // 使用CTK服务验证
    auto* userService = CTKManager::instance()->getService<UserManagementService>();
    if (userService) {
        const auto user = userService->loginUser(username, password);
        return user.isValid();
    }
#endif

    // 后备验证：简单的用户名密码检查（仅用于测试）
    // 实际生产中应该使用数据库验证
    if (username == "admin" && password == "admin") {
        return true;
    }
    if (username == "doctor" && password == "doctor") {
        return true;
    }

    return false;
}
