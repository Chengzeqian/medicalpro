#ifndef MAININTERFACEWIDGET_H
#define MAININTERFACEWIDGET_H

#include <QPixmap>
#include <QSize>
#include <QString>
#include <QWidget>

#include "Framework/Platform/CtkBridge/CoreUiRuntimeStatusProvider.h"
#include "Framework/Platform/CtkBridge/CtkRuntimeSnapshotCollector.h"
#include "Framework/Platform/Diagnostics/PlatformDiagnosticsService.h"
#include "Framework/Platform/Kernel/PlatformStateStore.h"

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
class PlatformDiagnosticsPage;
class LegacyCoreUiRuntimeAdapter;
class LegacyUserManagementAdapter;
class LegacyImagingAdapter;
class LegacyNavigationAdapter;
class IdentityAppService;
class ImagingAppService;
class NavigationAppService;

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
    void onSystemSettingsDiagnostics();
    void onManagementBack();
    void onManagementEnterMainSystem();
    void onDashboardBackToManagement();
    void onDiagnosticsBack();
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
        PAGE_SURGICAL_NAVIGATION = 6,
        PAGE_DIAGNOSTICS = 7
    };

    void setupUI();
    void setupConnections();
    void navigateToPage(int pageIndex);
    void enterSurgicalNavigationSystem(int patientId);
    void exitSurgicalNavigationSystem();
    QString getProjectPath() const;

    QStackedWidget* m_stackedWidget;
    WelcomePageNew* m_welcomePage;
    LoginPageNew* m_loginPage;
    ModuleSelectionPageNew* m_moduleSelectionPage;
    SystemSettingsPageNew* m_systemSettingsPage;
    ManagementPageNew* m_managementPage;
    NavigationPageNew* m_surgicalNavigationPage;
    DashboardPageNew* m_dashboardPage;
    PlatformDiagnosticsPage* m_platformDiagnosticsPage;
    PlatformStateStore m_platformStateStore;
    PlatformDiagnosticsService m_platformDiagnosticsService;
    CtkRuntimeSnapshotCollector m_runtimeCollector;
    LegacyCoreUiRuntimeAdapter* m_coreUiRuntimeAdapter;
    CoreUiRuntimeStatusProvider* m_coreUiRuntimeStatusProvider;
    LegacyUserManagementAdapter* m_identityAdapter;
    LegacyImagingAdapter* m_imagingAdapter;
    LegacyNavigationAdapter* m_navigationAdapter;
    IdentityAppService* m_identityAppService;
    ImagingAppService* m_imagingAppService;
    NavigationAppService* m_navigationAppService;

    int m_currentPatientId;
    bool m_isLoggedIn;
    QString m_currentUser;
    QPixmap m_cachedBackground;
    QSize m_lastSize;
};

#endif // MAININTERFACEWIDGET_H
