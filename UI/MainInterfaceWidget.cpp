#include "MainInterfaceWidget.h"

#include "Framework/Platform/Facades/IdentityAppService.h"
#include "Framework/Platform/Facades/ImagingAppService.h"
#include "Framework/Platform/Facades/NavigationAppService.h"
#include "Framework/Platform/LegacyAdapters/LegacyCoreUiRuntimeAdapter.h"
#include "Framework/Platform/LegacyAdapters/LegacyImagingAdapter.h"
#include "Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h"
#include "Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h"
#include "NewPages/BasePage.h"
#include "NewPages/DashboardPage.h"
#include "NewPages/LoginPage.h"
#include "NewPages/ManagementPage.h"
#include "NewPages/ModuleSelectionPage.h"
#include "NewPages/PlatformDiagnosticsPage.h"
#include "NewPages/NavigationPage.h"
#include "NewPages/SystemSettingsPage.h"
#include "NewPages/WelcomePage.h"

#include <QCloseEvent>
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Plugins/UserManagement/UserDataStructures.h"
#endif

MainInterfaceWidget::MainInterfaceWidget(QWidget* parent)
    : QWidget(parent)
    , m_stackedWidget(nullptr)
    , m_welcomePage(nullptr)
    , m_loginPage(nullptr)
    , m_moduleSelectionPage(nullptr)
    , m_systemSettingsPage(nullptr)
    , m_managementPage(nullptr)
    , m_surgicalNavigationPage(nullptr)
    , m_dashboardPage(nullptr)
    , m_platformDiagnosticsPage(nullptr)
    , m_platformDiagnosticsService(&m_platformStateStore)
    , m_coreUiRuntimeAdapter(nullptr)
    , m_coreUiRuntimeStatusProvider(nullptr)
    , m_identityAdapter(nullptr)
    , m_imagingAdapter(nullptr)
    , m_navigationAdapter(nullptr)
    , m_identityAppService(nullptr)
    , m_imagingAppService(nullptr)
    , m_navigationAppService(nullptr)
    , m_currentPatientId(-1)
    , m_isLoggedIn(false)
{
    qDebug() << "[MainInterfaceWidget] create";

    setWindowTitle(QStringLiteral("MedicalPro - \u533b\u7597\u5bfc\u822a\u7cfb\u7edf"));
    setMinimumSize(1200, 800);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(true);

    setupUI();
    setupConnections();
    navigateToPage(PAGE_WELCOME);

    qDebug() << "[MainInterfaceWidget] initialized";
}

MainInterfaceWidget::~MainInterfaceWidget()
{
    delete m_navigationAppService;
    delete m_imagingAppService;
    delete m_identityAppService;
    delete m_navigationAdapter;
    delete m_imagingAdapter;
    delete m_identityAdapter;
    delete m_coreUiRuntimeStatusProvider;
    delete m_coreUiRuntimeAdapter;
    qDebug() << "[MainInterfaceWidget] destroy";
}

PlatformStateStore* MainInterfaceWidget::platformStateStore()
{
    return &m_platformStateStore;
}

PlatformLifecycleTraceRecorder* MainInterfaceWidget::lifecycleTraceRecorder()
{
    return &m_lifecycleTraceRecorder;
}

void MainInterfaceWidget::onWelcomeEnterSystem()
{
    qDebug() << "[MainInterfaceWidget] welcome -> module selection";
    m_isLoggedIn = true;
    m_currentUser = QStringLiteral("\u8bbf\u5ba2");
    if (m_moduleSelectionPage) {
        m_moduleSelectionPage->setCurrentUser(m_currentUser);
    }
    navigateToPage(PAGE_MODULE_SELECTION);
}

void MainInterfaceWidget::onLoginSuccess(const QString& username)
{
    qDebug() << "[MainInterfaceWidget] login success" << username;
    m_isLoggedIn = true;
    m_currentUser = username.isEmpty() ? QStringLiteral("\u8bbf\u5ba2") : username;
    if (m_moduleSelectionPage) {
        m_moduleSelectionPage->setCurrentUser(m_currentUser);
    }
    navigateToPage(PAGE_MODULE_SELECTION);
}

void MainInterfaceWidget::onLoginFailed(const QString& message)
{
    qWarning() << "[MainInterfaceWidget] login failed:" << message;
}

void MainInterfaceWidget::onLoginBackToWelcome()
{
    qDebug() << "[MainInterfaceWidget] login -> welcome";
    navigateToPage(PAGE_WELCOME);
}

void MainInterfaceWidget::onModuleSelectionSystemSettings()
{
    qDebug() << "[MainInterfaceWidget] module selection -> settings";
    navigateToPage(PAGE_SYSTEM_SETTINGS);
}

void MainInterfaceWidget::onModuleSelectionAnkleSurgery()
{
    qDebug() << "[MainInterfaceWidget] module selection -> management";
    navigateToPage(PAGE_MANAGEMENT);
}

void MainInterfaceWidget::onModuleSelectionBack()
{
    qDebug() << "[MainInterfaceWidget] module selection -> welcome";
    onReturnToWelcomeRequested();
}

void MainInterfaceWidget::onSystemSettingsBack()
{
    qDebug() << "[MainInterfaceWidget] settings -> module selection";
    navigateToPage(PAGE_MODULE_SELECTION);
}

void MainInterfaceWidget::onSystemSettingsDiagnostics()
{
    qDebug() << "[MainInterfaceWidget] settings -> diagnostics";
    navigateToPage(PAGE_DIAGNOSTICS);
}

void MainInterfaceWidget::onManagementBack()
{
    qDebug() << "[MainInterfaceWidget] management -> module selection";
    navigateToPage(PAGE_MODULE_SELECTION);
}

void MainInterfaceWidget::onManagementEnterMainSystem()
{
    qDebug() << "[MainInterfaceWidget] management -> dashboard";
    navigateToPage(PAGE_MAIN);
}

void MainInterfaceWidget::onDashboardBackToManagement()
{
    qDebug() << "[MainInterfaceWidget] dashboard -> management";
    navigateToPage(PAGE_MANAGEMENT);
}

void MainInterfaceWidget::onDiagnosticsBack()
{
    qDebug() << "[MainInterfaceWidget] diagnostics -> settings";
    navigateToPage(PAGE_SYSTEM_SETTINGS);
}

void MainInterfaceWidget::onReturnToWelcomeRequested()
{
    qDebug() << "[MainInterfaceWidget] return to welcome";
    m_isLoggedIn = false;
    m_currentUser.clear();
    m_currentPatientId = -1;
    emit logoutRequested();
    navigateToPage(PAGE_WELCOME);
}

void MainInterfaceWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    if (m_cachedBackground.isNull() || m_lastSize != size()) {
        const QPixmap bgImage(QStringLiteral(":/resoucce/widget_bg.jpg"));
        if (!bgImage.isNull()) {
            m_cachedBackground = bgImage.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            m_lastSize = size();
        }
    }

    if (!m_cachedBackground.isNull()) {
        const int x = (width() - m_cachedBackground.width()) / 2;
        const int y = (height() - m_cachedBackground.height()) / 2;
        painter.drawPixmap(x, y, m_cachedBackground);
        return;
    }

    painter.fillRect(rect(), QColor(15, 23, 42));
}

void MainInterfaceWidget::setupUI()
{
    qDebug() << "[MainInterfaceWidget] setup ui";

    if (!m_identityAdapter) m_identityAdapter = new LegacyUserManagementAdapter();
    if (!m_imagingAdapter) m_imagingAdapter = new LegacyImagingAdapter();
    if (!m_navigationAdapter) m_navigationAdapter = new LegacyNavigationAdapter();
    if (!m_coreUiRuntimeAdapter) m_coreUiRuntimeAdapter = new LegacyCoreUiRuntimeAdapter();
    if (!m_coreUiRuntimeStatusProvider) {
        m_coreUiRuntimeStatusProvider = new CoreUiRuntimeStatusProvider(
            m_coreUiRuntimeAdapter,
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data")));
    }
    if (!m_identityAppService) m_identityAppService = new IdentityAppService(m_identityAdapter);
    if (!m_imagingAppService) m_imagingAppService = new ImagingAppService(m_imagingAdapter);
    if (!m_navigationAppService) m_navigationAppService = new NavigationAppService(m_navigationAdapter);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setObjectName("mainInterfaceStack");
    m_stackedWidget->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));
    mainLayout->addWidget(m_stackedWidget);

    m_welcomePage = new WelcomePageNew(this, [this]() {
        const auto runtimeSnapshot = m_coreUiRuntimeStatusProvider
            ? m_coreUiRuntimeStatusProvider->welcomeSnapshot()
            : CoreUiRuntimeStatusSnapshot {};
        WelcomePageNew::RuntimeStatusSnapshot snapshot;
        snapshot.frameworkReady = runtimeSnapshot.frameworkReady;
        snapshot.pluginCount = runtimeSnapshot.pluginCount;
        snapshot.readyServices = runtimeSnapshot.readyServices;
        snapshot.totalServices = runtimeSnapshot.totalServices;
        snapshot.dataDirectoryExists = runtimeSnapshot.dataDirectoryExists;
        snapshot.dataDirectoryReadable = runtimeSnapshot.dataDirectoryReadable;
        snapshot.missingServices = runtimeSnapshot.missingServices;
        return snapshot;
    });
    m_loginPage = new LoginPageNew(this, [this](const QString& username, const QString& password) {
        if (!m_identityAppService) return UserInfo {};
        return m_identityAppService->authenticate(username, password);
    });
    m_moduleSelectionPage = new ModuleSelectionPageNew(this, [this]() {
        const auto runtimeSnapshot = m_coreUiRuntimeStatusProvider
            ? m_coreUiRuntimeStatusProvider->moduleSelectionSnapshot()
            : CoreUiRuntimeStatusSnapshot {};
        ModuleSelectionPageNew::ModuleRuntimeStatus status;
        status.frameworkReady = runtimeSnapshot.frameworkReady;
        status.workflowReady = runtimeSnapshot.workflowReady;
        status.readyServices = runtimeSnapshot.readyServices;
        status.totalServices = runtimeSnapshot.totalServices;
        status.missingServices = runtimeSnapshot.missingServices;
        return status;
    });
    m_systemSettingsPage = new SystemSettingsPageNew(this, [this]() {
        const auto runtimeSnapshot = m_coreUiRuntimeStatusProvider
            ? m_coreUiRuntimeStatusProvider->systemSettingsSnapshot()
            : CoreUiRuntimeStatusSnapshot {};
        SystemSettingsPageNew::RuntimeStatusSnapshot snapshot;
        snapshot.frameworkReady = runtimeSnapshot.frameworkReady;
        snapshot.pluginCount = runtimeSnapshot.pluginCount;
        snapshot.readyServices = runtimeSnapshot.readyServices;
        snapshot.totalServices = runtimeSnapshot.totalServices;
        snapshot.dataDirectoryReadable = runtimeSnapshot.dataDirectoryReadable;
        return snapshot;
    });
    m_managementPage = new ManagementPageNew(this, m_identityAppService);
    m_dashboardPage = new DashboardPageNew(
        this,
        m_identityAppService,
        m_imagingAppService,
        m_navigationAppService);
    m_surgicalNavigationPage = new NavigationPageNew(this);
    m_platformDiagnosticsPage = new PlatformDiagnosticsPage(this, [this]() {
        return m_platformDiagnosticsService.buildSnapshot(m_runtimeCollector.collect());
    });

    m_stackedWidget->addWidget(m_welcomePage);
    m_stackedWidget->addWidget(m_loginPage);
    m_stackedWidget->addWidget(m_moduleSelectionPage);
    m_stackedWidget->addWidget(m_systemSettingsPage);
    m_stackedWidget->addWidget(m_managementPage);
    m_stackedWidget->addWidget(m_dashboardPage);
    m_stackedWidget->addWidget(m_surgicalNavigationPage);
    m_stackedWidget->addWidget(m_platformDiagnosticsPage);
}

void MainInterfaceWidget::enterSurgicalNavigationSystem(int patientId)
{
    qDebug() << "[MainInterfaceWidget] enter navigation for patient" << patientId;

    if (!m_stackedWidget || !m_surgicalNavigationPage) {
        qWarning() << "[MainInterfaceWidget] navigation page is not ready";
        return;
    }

    m_currentPatientId = patientId;
    m_surgicalNavigationPage->resetPage();
    m_surgicalNavigationPage->setPatientId(patientId);
    navigateToPage(PAGE_SURGICAL_NAVIGATION);
}

void MainInterfaceWidget::exitSurgicalNavigationSystem()
{
    qDebug() << "[MainInterfaceWidget] leave navigation";
    navigateToPage(PAGE_MAIN);
}

void MainInterfaceWidget::setupConnections()
{
    qDebug() << "[MainInterfaceWidget] setup connections";

    connect(m_welcomePage, &WelcomePageNew::enterSystemRequested,
            this, &MainInterfaceWidget::onWelcomeEnterSystem);
    connect(m_welcomePage, &WelcomePageNew::exitRequested,
            this, &MainInterfaceWidget::onExitButtonClicked);

    connect(m_loginPage, &LoginPageNew::loginSucceeded,
            this, &MainInterfaceWidget::onLoginSuccess);
    connect(m_loginPage, &LoginPageNew::loginFailed,
            this, &MainInterfaceWidget::onLoginFailed);
    connect(m_loginPage, &LoginPageNew::backToWelcomeRequested,
            this, &MainInterfaceWidget::onLoginBackToWelcome);

    connect(m_moduleSelectionPage, &ModuleSelectionPageNew::systemSettingsRequested,
            this, &MainInterfaceWidget::onModuleSelectionSystemSettings);
    connect(m_moduleSelectionPage, &ModuleSelectionPageNew::ankleSurgeryRequested,
            this, &MainInterfaceWidget::onModuleSelectionAnkleSurgery);
    connect(m_moduleSelectionPage, &ModuleSelectionPageNew::logoutRequested,
            this, &MainInterfaceWidget::onModuleSelectionBack);

    connect(m_systemSettingsPage, &SystemSettingsPageNew::backRequested,
            this, &MainInterfaceWidget::onSystemSettingsBack);
    connect(m_systemSettingsPage, &SystemSettingsPageNew::diagnosticsRequested,
            this, &MainInterfaceWidget::onSystemSettingsDiagnostics);

    connect(m_managementPage, &ManagementPageNew::backRequested,
            this, &MainInterfaceWidget::onManagementBack);
    connect(m_managementPage, &ManagementPageNew::enterMainSystemRequested,
            this, &MainInterfaceWidget::onManagementEnterMainSystem);

    connect(m_dashboardPage, &DashboardPageNew::backToManagementRequested,
            this, &MainInterfaceWidget::onDashboardBackToManagement);
    connect(m_dashboardPage, &DashboardPageNew::enterNavigationRequested,
            this, &MainInterfaceWidget::enterSurgicalNavigationSystem);
    connect(m_dashboardPage, &DashboardPageNew::logoutRequested,
            this, &MainInterfaceWidget::onLogoutButtonClicked);

    connect(m_surgicalNavigationPage, &NavigationPageNew::backToMainRequested,
            this, &MainInterfaceWidget::exitSurgicalNavigationSystem);
    connect(m_platformDiagnosticsPage, &PlatformDiagnosticsPage::backRequested,
            this, &MainInterfaceWidget::onDiagnosticsBack);
}

void MainInterfaceWidget::onLogoutButtonClicked()
{
    const auto result = QMessageBox::question(
        this,
        QStringLiteral("\u8fd4\u56de\u6b22\u8fce\u9875"),
        QStringLiteral("\u786e\u5b9a\u8fd4\u56de\u6b22\u8fce\u9875\u5417\uff1f"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    if (m_identityAppService) {
        m_identityAppService->logoutCurrentUser();
    }

    onReturnToWelcomeRequested();
}

void MainInterfaceWidget::onExitButtonClicked()
{
    qDebug() << "[MainInterfaceWidget] exit requested";
    emit exitRequested();
}

void MainInterfaceWidget::navigateToPage(int pageIndex)
{
    if (!m_stackedWidget) {
        qWarning() << "[MainInterfaceWidget] stacked widget is null";
        return;
    }

    if (auto* currentPage = qobject_cast<BasePage*>(m_stackedWidget->currentWidget())) {
        currentPage->onPageDeactivated();
    }

    QWidget* targetPage = nullptr;
    switch (pageIndex) {
    case PAGE_WELCOME:
        targetPage = m_welcomePage;
        break;
    case PAGE_LOGIN:
        targetPage = m_loginPage;
        break;
    case PAGE_MODULE_SELECTION:
        targetPage = m_moduleSelectionPage;
        break;
    case PAGE_SYSTEM_SETTINGS:
        targetPage = m_systemSettingsPage;
        break;
    case PAGE_MANAGEMENT:
        targetPage = m_managementPage;
        break;
    case PAGE_MAIN:
        targetPage = m_dashboardPage;
        break;
    case PAGE_SURGICAL_NAVIGATION:
        targetPage = m_surgicalNavigationPage;
        break;
    case PAGE_DIAGNOSTICS:
        targetPage = m_platformDiagnosticsPage;
        break;
    default:
        qWarning() << "[MainInterfaceWidget] invalid page index" << pageIndex;
        return;
    }

    m_stackedWidget->setUpdatesEnabled(false);
    m_stackedWidget->setCurrentWidget(targetPage);
    if (auto* nextPage = qobject_cast<BasePage*>(targetPage)) {
        nextPage->onPageActivated();
    }
    m_stackedWidget->setUpdatesEnabled(true);
}

void MainInterfaceWidget::closeEvent(QCloseEvent* event)
{
    qDebug() << "[MainInterfaceWidget] close event";
    event->accept();
}

QString MainInterfaceWidget::getProjectPath() const
{
    QDir dir(QCoreApplication::applicationDirPath());
    while (dir.dirName() != QStringLiteral("medicalpro") && !dir.isRoot()) {
        const QString currentName = dir.dirName();
        if (currentName == QStringLiteral("Release")
            || currentName == QStringLiteral("Debug")
            || currentName.contains(QStringLiteral("build"), Qt::CaseInsensitive)
            || currentName.contains(QStringLiteral("Desktop_"), Qt::CaseInsensitive)
            || currentName.contains(QStringLiteral("MSVC"), Qt::CaseInsensitive)) {
            dir.cdUp();
            continue;
        }
        break;
    }
    return dir.absolutePath();
}
