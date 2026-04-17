#ifndef MAINWINDOW_NEW_H
#define MAINWINDOW_NEW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QStack>

#include "PageIndex.h"

// 前向声明
class WelcomePageNew;
class LoginPageNew;
class ModuleSelectionPageNew;
class SystemSettingsPageNew;
class ManagementPageNew;
class DashboardPageNew;
class NavigationPageNew;

/**
 * @brief 简化的主窗口类
 *
 * 设计理念：
 * - 使用 QStackedWidget 管理所有页面
 * - 统一的页面导航机制
 * - 支持导航历史（返回功能）
 * - 最小化的职责：只负责页面切换和生命周期管理
 */
class MainWindowNew : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindowNew(QWidget* parent = nullptr);
    ~MainWindowNew();

    // 导航到指定页面
    void navigateTo(PageIndex page);
    void navigateTo(int pageIndex);

    // 返回上一页
    void goBack();

signals:
    void exitRequested();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // 页面信号处理
    void onNavigateTo(int pageIndex);
    void onGoBack();
    void onExitRequested();
    void onLogoutRequested();

    // 登录相关
    void onLoginSucceeded(const QString& username);

    // Dashboard相关
    void onEnterNavigationRequested(int patientId);

private:
    void setupUI();
    void createPages();
    void setupConnections();

    // 页面生命周期管理
    void activatePage(int pageIndex);
    void deactivatePage(int pageIndex);

    // UI组件
    QStackedWidget* m_pageStack;

    // 页面实例
    WelcomePageNew* m_welcomePage;
    LoginPageNew* m_loginPage;
    ModuleSelectionPageNew* m_moduleSelectionPage;
    SystemSettingsPageNew* m_systemSettingsPage;
    ManagementPageNew* m_managementPage;
    DashboardPageNew* m_dashboardPage;
    NavigationPageNew* m_navigationPage;

    // 导航历史
    QStack<int> m_navigationHistory;

    // 当前用户
    QString m_currentUser;

    // 当前患者ID
    int m_currentPatientId;
};

#endif // MAINWINDOW_NEW_H