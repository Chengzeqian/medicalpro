#ifndef MAININTERFACEWIDGET_H
#define MAININTERFACEWIDGET_H

#include <QPixmap>
#include <QSize>
#include <QString>
#include <QWidget>

#include "Framework/CTKManager.h"

class QCloseEvent;
class QPaintEvent;
class QStackedWidget;
class WelcomePageNew;
class LoginPageNew;
class ModuleSelectionPageNew;
class SystemSettingsPageNew;
class ManagementPageNew;
class NavigationPageNew;
class DashboardPageNew;

#ifdef CTK_PLUGIN_FRAMEWORK
class UserManagementService;
class DicomViewerService;
class InstrumentManagementService;
class FourViewDisplayService;
class SegmentationService;
class ctkEventAdmin;
#endif

enum class MeasurementTool
{
    Distance,
    Angle,
    Position,
    Line,
    Area
};

class MainInterfaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainInterfaceWidget(QWidget* parent = nullptr);
    ~MainInterfaceWidget() override;

signals:
    void exitRequested();
    void logoutRequested();

protected:
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onWelcomeEnterSystem();
    void onLoginSuccess(const QString& username);
    void onLoginFailed(const QString& message);
    void onLoginBackToWelcome();
    void onModuleSelectionSystemSettings();
    void onModuleSelectionAnkleSurgery();
    void onModuleSelectionBack();
    void onSystemSettingsBack();
    void onManagementBack();
    void onManagementEnterMainSystem();
    void onDashboardBackToManagement();
    void onReturnToWelcomeRequested();
    void onLogoutButtonClicked();
    void onExitButtonClicked();

private:
    enum PageIndex {
        PAGE_WELCOME = 0,
        PAGE_LOGIN = 1,
        PAGE_MODULE_SELECTION = 2,
        PAGE_SYSTEM_SETTINGS = 3,
        PAGE_MANAGEMENT = 4,
        PAGE_MAIN = 5,
        PAGE_SURGICAL_NAVIGATION = 6
    };

    void setupUI();
    void setupConnections();
    void navigateToPage(int pageIndex);
    void enterSurgicalNavigationSystem(int patientId);
    void exitSurgicalNavigationSystem();
    QString getProjectPath() const;

#ifdef CTK_PLUGIN_FRAMEWORK
    UserManagementService* getUserService();
    DicomViewerService* getDicomService();
    InstrumentManagementService* getInstrumentService();
    FourViewDisplayService* getFourViewService();
    SegmentationService* getSegmentationService();
    ctkEventAdmin* getEventAdmin();
#endif

    QStackedWidget* m_stackedWidget;
    WelcomePageNew* m_welcomePage;
    LoginPageNew* m_loginPage;
    ModuleSelectionPageNew* m_moduleSelectionPage;
    SystemSettingsPageNew* m_systemSettingsPage;
    ManagementPageNew* m_managementPage;
    NavigationPageNew* m_surgicalNavigationPage;
    DashboardPageNew* m_dashboardPage;
    CTKManager* m_ctkManager;

#ifdef CTK_PLUGIN_FRAMEWORK
    UserManagementService* m_userService;
    DicomViewerService* m_dicomService;
    InstrumentManagementService* m_instrumentService;
    FourViewDisplayService* m_fourViewService;
    SegmentationService* m_segmentationService;
    ctkEventAdmin* m_eventAdmin;
#endif

    int m_currentPatientId;
    bool m_isLoggedIn;
    QString m_currentUser;
    QPixmap m_cachedBackground;
    QSize m_lastSize;
};

#endif // MAININTERFACEWIDGET_H
