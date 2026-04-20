#ifndef MAINWINDOW_NEW_H
#define MAINWINDOW_NEW_H

#include <QMainWindow>
#include <QStack>
#include <QStackedWidget>

#include "PageIndex.h"

class QCloseEvent;
class WelcomePageNew;
class LoginPageNew;
class ModuleSelectionPageNew;
class SystemSettingsPageNew;
class ManagementPageNew;
class DashboardPageNew;
class NavigationPageNew;
class LegacyUserManagementAdapter;
class IdentityAppService;

class MainWindowNew : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindowNew(QWidget* parent = nullptr);
    ~MainWindowNew();

    void navigateTo(PageIndex page);
    void navigateTo(int pageIndex);
    void goBack();

signals:
    void exitRequested();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNavigateTo(int pageIndex);
    void onGoBack();
    void onExitRequested();
    void onLogoutRequested();
    void onLoginSucceeded(const QString& username);
    void onEnterNavigationRequested(int patientId);

private:
    void setupUI();
    void createPages();
    void setupConnections();
    void activatePage(int pageIndex);
    void deactivatePage(int pageIndex);

    QStackedWidget* m_pageStack;
    WelcomePageNew* m_welcomePage;
    LoginPageNew* m_loginPage;
    ModuleSelectionPageNew* m_moduleSelectionPage;
    SystemSettingsPageNew* m_systemSettingsPage;
    ManagementPageNew* m_managementPage;
    DashboardPageNew* m_dashboardPage;
    NavigationPageNew* m_navigationPage;
    LegacyUserManagementAdapter* m_identityAdapter;
    IdentityAppService* m_identityAppService;
    QStack<int> m_navigationHistory;
    QString m_currentUser;
    int m_currentPatientId;
};

#endif // MAINWINDOW_NEW_H
