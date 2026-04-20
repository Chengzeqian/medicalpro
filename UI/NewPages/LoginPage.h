#ifndef LOGINPAGE_NEW_H
#define LOGINPAGE_NEW_H

#include "BasePage.h"
#include "PageIndex.h"
#include "Plugins/UserManagement/UserDataStructures.h"

#include <functional>

namespace Ui {
class LoginPage;
}

/**
 * @brief 登录页面
 *
 * 功能：
 * - 用户名/密码输入
 * - 记住密码选项
 * - 登录验证
 */
class LoginPageNew : public BasePage
{
    Q_OBJECT

public:
    using LoginHandler = std::function<UserInfo(const QString&, const QString&)>;

    explicit LoginPageNew(QWidget* parent = nullptr, LoginHandler loginHandler = {});
    ~LoginPageNew();

    void onActivated() override;

    // 设置当前用户（登录成功后）
    QString getCurrentUser() const { return m_currentUser; }

signals:
    void loginSucceeded(const QString& username);
    void loginFailed(const QString& message);
    void backToWelcomeRequested();  // MainInterfaceWidget期望的信号

private slots:
    void on_loginButton_clicked();
    void on_backButton_clicked();

private:
    void loadSavedCredentials();
    void saveCredentials();
    UserInfo authenticate(const QString& username, const QString& password) const;

    Ui::LoginPage* ui;
    LoginHandler m_loginHandler;
    QString m_currentUser;
};

#endif // LOGINPAGE_NEW_H
