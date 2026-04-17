#include "MainWindow.h"

#include "WelcomePage.h"
#include "LoginPage.h"
#include "ModuleSelectionPage.h"
#include "SystemSettingsPage.h"
#include "ManagementPage.h"
#include "DashboardPage.h"
#include "NavigationPage.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <QDebug>

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Framework/CTKManager.h"
#include "Plugins/UserManagement/UserManagementService.h"
#endif

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
    , m_currentPatientId(-1)
{
    setupUI();
    createPages();
    setupConnections();

    // 从欢迎页开始
    navigateTo(PageIndex::Welcome);
}

MainWindowNew::~MainWindowNew()
{
    // 页面由 QStackedWidget 管理，自动删除
}

void MainWindowNew::setupUI()
{
    setWindowTitle("医疗导航系统");
    setMinimumSize(1280, 800);

    // 设置窗口样式
    setStyleSheet(
        "QMainWindow {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "        stop:0 #1a1a2e, stop:1 #16213e);"
        "}"
    );

    // 创建中央页面栈
    m_pageStack = new QStackedWidget(this);
    setCentralWidget(m_pageStack);
}

void MainWindowNew::createPages()
{
    // 创建所有页面
    m_welcomePage = new WelcomePageNew(this);
    m_loginPage = new LoginPageNew(this);
    m_moduleSelectionPage = new ModuleSelectionPageNew(this);
    m_systemSettingsPage = new SystemSettingsPageNew(this);
    m_managementPage = new ManagementPageNew(this);
    m_dashboardPage = new DashboardPageNew(this);
    m_navigationPage = new NavigationPageNew(this);

    // 按照 PageIndex 顺序添加到栈中
    m_pageStack->addWidget(m_welcomePage);        // 0 - Welcome
    m_pageStack->addWidget(m_loginPage);          // 1 - Login
    m_pageStack->addWidget(m_moduleSelectionPage);// 2 - ModuleSelection
    m_pageStack->addWidget(m_systemSettingsPage); // 3 - SystemSettings
    m_pageStack->addWidget(m_managementPage);     // 4 - Management
    m_pageStack->addWidget(m_dashboardPage);      // 5 - Dashboard
    m_pageStack->addWidget(m_navigationPage);     // 6 - Navigation
}

void MainWindowNew::setupConnections()
{
    // 连接所有页面的导航信号

    // WelcomePage
    connect(m_welcomePage, &WelcomePageNew::navigateTo, this, &MainWindowNew::onNavigateTo);
    connect(m_welcomePage, &WelcomePageNew::exitRequested, this, &MainWindowNew::onExitRequested);

    // LoginPage
    connect(m_loginPage, &LoginPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);
    connect(m_loginPage, &LoginPageNew::loginSucceeded, this, &MainWindowNew::onLoginSucceeded);

    // ModuleSelectionPage
    connect(m_moduleSelectionPage, &ModuleSelectionPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);
    connect(m_moduleSelectionPage, &ModuleSelectionPageNew::logoutRequested, this, &MainWindowNew::onLogoutRequested);

    // SystemSettingsPage
    connect(m_systemSettingsPage, &SystemSettingsPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);

    // ManagementPage
    connect(m_managementPage, &ManagementPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);

    // DashboardPage
    connect(m_dashboardPage, &DashboardPageNew::navigateTo, this, &MainWindowNew::onNavigateTo);
    connect(m_dashboardPage, &DashboardPageNew::logoutRequested, this, &MainWindowNew::onLogoutRequested);
    connect(m_dashboardPage, &DashboardPageNew::enterNavigationRequested, this, &MainWindowNew::onEnterNavigationRequested);

    // NavigationPage
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

    int currentIndex = m_pageStack->currentIndex();

    // 如果是同一个页面，不做任何事
    if (currentIndex == pageIndex) {
        return;
    }

    qDebug() << "[MainWindow] Navigating from" << currentIndex << "to" << pageIndex;

    // 失活当前页面
    deactivatePage(currentIndex);

    // 记录导航历史（除了登录和欢迎页）
    if (currentIndex != toInt(PageIndex::Welcome) &&
        currentIndex != toInt(PageIndex::Login)) {
        m_navigationHistory.push(currentIndex);
    }

    // 切换页面
    m_pageStack->setCurrentIndex(pageIndex);

    // 激活新页面
    activatePage(pageIndex);
}

void MainWindowNew::goBack()
{
    if (m_navigationHistory.isEmpty()) {
        qDebug() << "[MainWindow] No navigation history, cannot go back";
        return;
    }

    int previousPage = m_navigationHistory.pop();
    int currentIndex = m_pageStack->currentIndex();

    qDebug() << "[MainWindow] Going back from" << currentIndex << "to" << previousPage;

    // 失活当前页面
    deactivatePage(currentIndex);

    // 切换页面
    m_pageStack->setCurrentIndex(previousPage);

    // 激活新页面
    activatePage(previousPage);
}

void MainWindowNew::activatePage(int pageIndex)
{
    QWidget* page = m_pageStack->widget(pageIndex);
    if (!page) return;

    // 调用页面的 onActivated 方法
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

    // 调用页面的 onDeactivated 方法
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
    if (QMessageBox::question(this, "退出", "确定要退出吗？",
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        emit exitRequested();
        close();
    }
}

void MainWindowNew::onLogoutRequested()
{
    m_currentUser.clear();
    m_currentPatientId = -1;
    m_navigationHistory.clear();

    qDebug() << "[MainWindow] User logged out";
}

void MainWindowNew::onLoginSucceeded(const QString& username)
{
    m_currentUser = username;

    // 更新模块选择页面的用户信息
    m_moduleSelectionPage->setCurrentUser(username);

    qDebug() << "[MainWindow] User logged in:" << username;
}

void MainWindowNew::onEnterNavigationRequested(int patientId)
{
    m_currentPatientId = patientId;

    // 设置导航页面的患者信息
    m_navigationPage->setPatientId(patientId);

    // 获取患者名称（简化处理）
    QString patientName = QString("患者 %1").arg(patientId);

#ifdef CTK_PLUGIN_FRAMEWORK
    auto* userService = CTKManager::instance()->getService<UserManagementService>();
    if (userService) {
        const auto patient = userService->getPatient(patientId);
        if (patient.isValid()) {
            patientName = patient.name;
        }
    }
#endif

    m_navigationPage->setPatientName(patientName);

    qDebug() << "[MainWindow] Entering navigation for patient:" << patientId;
}

void MainWindowNew::closeEvent(QCloseEvent* event)
{
    // 确认退出
    if (QMessageBox::question(this, "退出", "确定要退出吗？",
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
}
