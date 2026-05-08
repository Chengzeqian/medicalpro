#include "MainWindow.h"

#include "Framework/Platform/Facades/IdentityAppService.h"
#include "Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h"
#include "DashboardPage.h"
#include "LoginPage.h"
#include "ManagementPage.h"
#include "ModuleSelectionPage.h"
#include "NavigationPage.h"
#include "SystemSettingsPage.h"
#include "WelcomePage.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QMessageBox>

MainWindowNew::MainWindowNew(QWidget* parent)
    : QMainWindow(parent)
    , m_pageStack(nullptr)
    , m_welcomePage(nullptr)
    , m_loginPage(nullptr)
    , m_moduleSelectionPage(nullptr)
    , m_systemSettingsPage(nullptr)
    , m_managementPage(nullptr)
    , m_dashboardPage(nullptr)
    , m_navigationPage(nullptr)
    , m_identityAdapter(nullptr)
    , m_identityAppService(nullptr)
    , m_currentPatientId(-1)
{
    setupUI();
    createPages();
    setupConnections();
    navigateTo(PageIndex::Welcome);
}

MainWindowNew::~MainWindowNew()
{
    delete m_identityAppService;
    delete m_identityAdapter;
}

void MainWindowNew::setupUI()
{
    setWindowTitle(QStringLiteral("\u533b\u7597\u5bfc\u822a\u7cfb\u7edf"));
    setMinimumSize(1280, 800);
    setStyleSheet(
        "QMainWindow {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "        stop:0 #1a1a2e, stop:1 #16213e);"
        "}");

    m_pageStack = new QStackedWidget(this);
    setCentralWidget(m_pageStack);
}

void MainWindowNew::createPages()
{
    if (!m_identityAdapter) m_identityAdapter = new LegacyUserManagementAdapter();
    if (!m_identityAppService) m_identityAppService = new IdentityAppService(m_identityAdapter);

    m_welcomePage = new WelcomePageNew(this);
    m_loginPage = new LoginPageNew(this, [this](const QString& username, const QString& password) {
        if (!m_identityAppService) return UserInfo {};
        return m_identityAppService->authenticate(username, password);
    });
    m_moduleSelectionPage = new ModuleSelectionPageNew(this);
    m_systemSettingsPage = new SystemSettingsPageNew(this);
    m_managementPage = new ManagementPageNew(this, m_identityAppService);
    m_dashboardPage = new DashboardPageNew(this, m_identityAppService);
    m_dashboardPage->setCaseWorkspaceDataRoot(QCoreApplication::applicationDirPath());
    m_navigationPage = new NavigationPageNew(this);

    m_pageStack->addWidget(m_welcomePage);
    m_pageStack->addWidget(m_loginPage);
    m_pageStack->addWidget(m_moduleSelectionPage);
    m_pageStack->addWidget(m_systemSettingsPage);
    m_pageStack->addWidget(m_managementPage);
    m_pageStack->addWidget(m_dashboardPage);
    m_pageStack->addWidget(m_navigationPage);
}

void MainWindowNew::setupConnections()
{
    connect(m_welcomePage, &WelcomePageNew::navigateTo, this, &MainWindowNew::onNavigateTo);
    connect(m_welcomePage, &WelcomePageNew::exitRequested, this, &MainWindowNew::onExitRequested);

    connect(m_loginPage, &LoginPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);
    connect(m_loginPage, &LoginPageNew::loginSucceeded, this, &MainWindowNew::onLoginSucceeded);

    connect(m_moduleSelectionPage, &ModuleSelectionPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);
    connect(m_moduleSelectionPage, &ModuleSelectionPageNew::logoutRequested, this, &MainWindowNew::onLogoutRequested);

    connect(m_systemSettingsPage, &SystemSettingsPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);

    connect(m_managementPage, &ManagementPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);
    connect(m_managementPage, &ManagementPageNew::enterCaseWorkspaceRequested,
            this, [this](const QString& caseId, int patientId) {
                m_currentPatientId = patientId;
                if (m_dashboardPage) {
                    m_dashboardPage->setCurrentPatientId(patientId);
                    m_dashboardPage->setCurrentCaseId(caseId);
                }
            });

    connect(m_dashboardPage, &DashboardPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);
    connect(m_dashboardPage, &DashboardPageNew::logoutRequested, this, &MainWindowNew::onLogoutRequested);
    connect(m_dashboardPage, &DashboardPageNew::enterNavigationRequested, this, &MainWindowNew::onEnterNavigationRequested);

    connect(m_navigationPage, &NavigationPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);
}

void MainWindowNew::navigateTo(PageIndex page)
{
    navigateTo(toInt(page));
}

void MainWindowNew::navigateTo(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_pageStack->count()) {
        qWarning() << "[MainWindow] Invalid page index:" << pageIndex;
        return;
    }

    const int currentIndex = m_pageStack->currentIndex();
    if (currentIndex == pageIndex) {
        return;
    }

    qDebug() << "[MainWindow] Navigating from" << currentIndex << "to" << pageIndex;
    deactivatePage(currentIndex);

    if (currentIndex != toInt(PageIndex::Welcome)
        && currentIndex != toInt(PageIndex::Login)) {
        m_navigationHistory.push(currentIndex);
    }

    m_pageStack->setCurrentIndex(pageIndex);
    activatePage(pageIndex);
}

void MainWindowNew::goBack()
{
    if (m_navigationHistory.isEmpty()) {
        qDebug() << "[MainWindow] No navigation history, cannot go back";
        return;
    }

    const int previousPage = m_navigationHistory.pop();
    const int currentIndex = m_pageStack->currentIndex();

    qDebug() << "[MainWindow] Going back from" << currentIndex << "to" << previousPage;
    deactivatePage(currentIndex);
    m_pageStack->setCurrentIndex(previousPage);
    activatePage(previousPage);
}

void MainWindowNew::activatePage(int pageIndex)
{
    QWidget* page = m_pageStack->widget(pageIndex);
    if (!page) return;

    if (auto* welcomePage = qobject_cast<WelcomePageNew*>(page)) {
        welcomePage->onActivated();
    } else if (auto* loginPage = qobject_cast<LoginPageNew*>(page)) {
        loginPage->onActivated();
    } else if (auto* moduleSelectionPage = qobject_cast<ModuleSelectionPageNew*>(page)) {
        moduleSelectionPage->onActivated();
    } else if (auto* systemSettingsPage = qobject_cast<SystemSettingsPageNew*>(page)) {
        systemSettingsPage->onActivated();
    } else if (auto* managementPage = qobject_cast<ManagementPageNew*>(page)) {
        managementPage->onActivated();
    } else if (auto* dashboardPage = qobject_cast<DashboardPageNew*>(page)) {
        dashboardPage->onActivated();
    } else if (auto* navigationPage = qobject_cast<NavigationPageNew*>(page)) {
        navigationPage->onActivated();
    }
}

void MainWindowNew::deactivatePage(int pageIndex)
{
    QWidget* page = m_pageStack->widget(pageIndex);
    if (!page) return;

    if (auto* welcomePage = qobject_cast<WelcomePageNew*>(page)) {
        welcomePage->onDeactivated();
    } else if (auto* loginPage = qobject_cast<LoginPageNew*>(page)) {
        loginPage->onDeactivated();
    } else if (auto* moduleSelectionPage = qobject_cast<ModuleSelectionPageNew*>(page)) {
        moduleSelectionPage->onDeactivated();
    } else if (auto* systemSettingsPage = qobject_cast<SystemSettingsPageNew*>(page)) {
        systemSettingsPage->onDeactivated();
    } else if (auto* managementPage = qobject_cast<ManagementPageNew*>(page)) {
        managementPage->onDeactivated();
    } else if (auto* dashboardPage = qobject_cast<DashboardPageNew*>(page)) {
        dashboardPage->onDeactivated();
    } else if (auto* navigationPage = qobject_cast<NavigationPageNew*>(page)) {
        navigationPage->onDeactivated();
    }
}

void MainWindowNew::onNavigateTo(int pageIndex)
{
    navigateTo(pageIndex);
}

void MainWindowNew::onGoBack()
{
    goBack();
}

void MainWindowNew::onExitRequested()
{
    if (QMessageBox::question(
            this,
            QStringLiteral("\u9000\u51fa"),
            QStringLiteral("\u786e\u5b9a\u8981\u9000\u51fa\u5417\uff1f"),
            QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
        emit exitRequested();
        close();
    }
}

void MainWindowNew::onLogoutRequested()
{
    if (m_identityAppService) {
        m_identityAppService->logoutCurrentUser();
    }

    m_currentUser.clear();
    m_currentPatientId = -1;
    m_navigationHistory.clear();

    qDebug() << "[MainWindow] User logged out";
}

void MainWindowNew::onLoginSucceeded(const QString& username)
{
    m_currentUser = username;
    m_moduleSelectionPage->setCurrentUser(username);

    qDebug() << "[MainWindow] User logged in:" << username;
}

void MainWindowNew::onEnterNavigationRequested(int patientId)
{
    m_currentPatientId = patientId;
    m_navigationPage->setPatientId(patientId);

    QString patientName = QStringLiteral("\u60a3\u8005%1").arg(patientId);
    if (m_identityAppService) {
        const auto patient = m_identityAppService->patientById(patientId);
        if (patient.isValid()) {
            patientName = patient.name;
        }
    }

    m_navigationPage->setPatientName(patientName);
    qDebug() << "[MainWindow] Entering navigation for patient:" << patientId;
}

void MainWindowNew::closeEvent(QCloseEvent* event)
{
    if (QMessageBox::question(
            this,
            QStringLiteral("\u9000\u51fa"),
            QStringLiteral("\u786e\u5b9a\u8981\u9000\u51fa\u5417\uff1f"),
            QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
        event->accept();
        return;
    }

    event->ignore();
}
