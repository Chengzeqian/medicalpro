# UI Phase 2 Three Pages Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `WelcomePage`、`ModuleSelectionPage`、`DashboardPage` 从 Phase 1 的基础主题层升级为正式产品化三页，并保留欢迎页入口、免登录流程、真实运行状态摘要与现有页面跳转主链。

**Architecture:** 保持现有 `Qt Widgets + .ui + NewPages` 架构不变，把可测试的状态文案与时间格式化逻辑下沉到一个纯 `QtCore` 辅助文件，再分别改造三页 `.ui` 与页面类，最后通过公共 QSS 和少量主题令牌把视觉层统一收口。业务主链继续依赖 `CTKManager`、`UserManagementService`、`DicomViewerService` 的现有调用路径，不引入新的 UI 框架。

**Tech Stack:** Qt Widgets、Qt Designer `.ui`、Qt Style Sheet、Qt Test、CMake、CTK 服务查询

## Progress Update

更新时间：2026-04-15

- 当前 UI Phase 2 主链已按真实运行页面修正为 `Welcome -> ModuleSelection -> Management -> Dashboard`。
- WelcomePage Phase 2 已完成并保留欢迎页入口、免登录流程与真实运行摘要。
- `ThreePagePresentationUtils` 已继续扩展，新增门厅访问提示与 Dashboard 导航 CTA 提示 / tone 逻辑。
- `tests/unit/ThreePagePresentationUtilsTest.cpp` 已覆盖新增展示层逻辑，验证通过。
- `ModuleSelectionPage` 已完成门厅化改造：
  - 顶部状态条
  - Hero 文案区
  - 两张正式主卡
  - 动态状态标签与服务提示
- `DashboardPage` 已完成工作台化改造：
  - 顶部工作台头部区
  - 四张概览卡
  - 三个详情分区
  - 底部导航 CTA
- `ManagementPage` 已完成数据管理中台化改造：
  - 头部框架
  - 三张概览卡
  - 当前工作视图上下文条
  - 底部流程收口区
- `UI/styles/three_pages_theme.qss` 已追加 ModuleSelection / Management / Dashboard 的 Phase 2 样式覆盖。
- 2026-04-15 已完成本轮代码级验证：
  - `cmake --build build_x64 --config Release --target medicalpro`
  - `ctest -C Release -R three_page_presentation_utils_test --output-on-failure`
- 当前剩余工作重点：
  - 真实运行下的人工视觉验收
  - 基于实际画面继续微调 Welcome / ModuleSelection / Management / Dashboard
  - 暂不处理插件链问题

---

## File Structure

- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/ThreePagePresentationUtilsTest.cpp`
- Create: `UI/NewPages/ThreePagePresentationUtils.h`
- Create: `UI/NewPages/ThreePagePresentationUtils.cpp`
- Modify: `UI/Forms/WelcomePage.ui`
- Modify: `UI/NewPages/WelcomePage.h`
- Modify: `UI/NewPages/WelcomePage.cpp`
- Modify: `UI/Forms/ModuleSelectionPage.ui`
- Modify: `UI/NewPages/ModuleSelectionPage.h`
- Modify: `UI/NewPages/ModuleSelectionPage.cpp`
- Modify: `UI/Forms/DashboardPage.ui`
- Modify: `UI/NewPages/DashboardPage.h`
- Modify: `UI/NewPages/DashboardPage.cpp`
- Modify: `UI/AppTheme.h`
- Modify: `UI/styles/three_pages_theme.qss`
- Modify: `docs/current_status_and_project_overview.md`

责任划分：

- `UI/NewPages/ThreePagePresentationUtils.*` 只负责三页共用的展示层文案、时间格式化、状态摘要和 tone 名称映射，避免把可测试逻辑散落到页面类中。
- 三个 `.ui` 文件只负责结构和对象命名，不直接承载运行时状态判断。
- 三个页面类只负责把真实服务状态、目录状态和患者数据映射到界面控件。
- `three_pages_theme.qss` 只负责 Phase 2 视觉层与动态状态样式，`AppTheme.h` 只维护需要新增的主题令牌。
- `tests/unit/ThreePagePresentationUtilsTest.cpp` 只覆盖纯展示逻辑，不碰 GUI 事件驱动测试。

### Task 1: 搭建可测试的三页表示层基础

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/ThreePagePresentationUtilsTest.cpp`
- Create: `UI/NewPages/ThreePagePresentationUtils.h`
- Create: `UI/NewPages/ThreePagePresentationUtils.cpp`

- [ ] **Step 1: 先写失败的单元测试和测试入口**

```cmake
# tests/CMakeLists.txt
add_subdirectory(unit)
```

```cmake
# tests/unit/CMakeLists.txt
add_executable(three_page_presentation_utils_test
    ThreePagePresentationUtilsTest.cpp
    ${CMAKE_SOURCE_DIR}/UI/NewPages/ThreePagePresentationUtils.cpp
)

target_include_directories(three_page_presentation_utils_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(three_page_presentation_utils_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME three_page_presentation_utils_test
    COMMAND three_page_presentation_utils_test
)
```

```cpp
// tests/unit/ThreePagePresentationUtilsTest.cpp
#include <QtTest/QtTest>

#include "UI/NewPages/ThreePagePresentationUtils.h"

class ThreePagePresentationUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void buildFrameworkSummary_reportsReady();
    void buildServiceGroupSummary_reportsRatio();
    void buildDirectorySummary_reportsReadable();
    void buildToneName_reportsWarning();
    void buildModuleStatusSummary_reportsReady();
    void buildDashboardDicomSummary_reportsCount();
    void formatModuleTimestamp_reportsReadableText();
};

void ThreePagePresentationUtilsTest::buildFrameworkSummary_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildFrameworkSummary(true, 6),
        QStringLiteral("插件框架已联通，当前识别到 6 个插件。"));
}

void ThreePagePresentationUtilsTest::buildServiceGroupSummary_reportsRatio()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildServiceGroupSummary(2, 3),
        QStringLiteral("2/3 个关键服务已就绪。"));
}

void ThreePagePresentationUtilsTest::buildDirectorySummary_reportsReadable()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildDirectorySummary(true, true, QStringLiteral("data")),
        QStringLiteral("data 目录存在且可访问。"));
}

void ThreePagePresentationUtilsTest::buildToneName_reportsWarning()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildToneName(2, 3),
        QStringLiteral("warning"));
}

void ThreePagePresentationUtilsTest::buildModuleStatusSummary_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildModuleStatusSummary(true),
        QStringLiteral("主流程可进入"));
}

void ThreePagePresentationUtilsTest::buildDashboardDicomSummary_reportsCount()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildDashboardDicomSummary(5),
        QStringLiteral("当前病例包含 5 组 DICOM 检查。"));
}

void ThreePagePresentationUtilsTest::formatModuleTimestamp_reportsReadableText()
{
    const QDateTime dateTime(QDate(2026, 4, 14), QTime(9, 30, 0));
    QCOMPARE(
        ThreePagePresentationUtils::formatModuleTimestamp(dateTime),
        QStringLiteral("2026-04-14 09:30"));
}

QTEST_APPLESS_MAIN(ThreePagePresentationUtilsTest)

#include "ThreePagePresentationUtilsTest.moc"
```

- [ ] **Step 2: 运行测试，确认它先失败**

Run: `cmake --preset x64-release -DBUILD_TESTING=ON`

Expected: 配置成功，`tests/unit` 被纳入 `build_x64`

Run: `cmake --build build_x64 --config Release --target three_page_presentation_utils_test`

Expected: 失败，并报出 `ThreePagePresentationUtils.h` 缺失或函数未定义

- [ ] **Step 3: 写出最小可通过的表示层辅助实现**

```cmake
# CMakeLists.txt
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS ${MEDICALPRO_QT_COMPONENTS} Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${MEDICALPRO_QT_COMPONENTS} Test)
```

```cpp
// UI/NewPages/ThreePagePresentationUtils.h
#ifndef THREEPAGEPRESENTATIONUTILS_H
#define THREEPAGEPRESENTATIONUTILS_H

#include <QDateTime>
#include <QString>

namespace ThreePagePresentationUtils
{

QString buildFrameworkSummary(bool frameworkReady, int pluginCount);
QString buildServiceGroupSummary(int readyServices, int totalServices);
QString buildDirectorySummary(bool exists, bool readable, const QString& displayName);
QString buildToneName(int readyCount, int totalCount);
QString buildModuleStatusSummary(bool ready);
QString buildDashboardDicomSummary(int studyCount);
QString formatModuleTimestamp(const QDateTime& dateTime);

} // namespace ThreePagePresentationUtils

#endif // THREEPAGEPRESENTATIONUTILS_H
```

```cpp
// UI/NewPages/ThreePagePresentationUtils.cpp
#include "ThreePagePresentationUtils.h"

namespace ThreePagePresentationUtils
{

QString buildFrameworkSummary(bool frameworkReady, int pluginCount)
{
    return frameworkReady
        ? QStringLiteral("插件框架已联通，当前识别到 %1 个插件。").arg(pluginCount)
        : QStringLiteral("插件框架尚未就绪，请先检查启动日志。");
}

QString buildServiceGroupSummary(int readyServices, int totalServices)
{
    return QStringLiteral("%1/%2 个关键服务已就绪。").arg(readyServices).arg(totalServices);
}

QString buildDirectorySummary(bool exists, bool readable, const QString& displayName)
{
    if (!exists) {
        return QStringLiteral("%1 目录缺失。").arg(displayName);
    }
    if (!readable) {
        return QStringLiteral("%1 目录存在但不可访问。").arg(displayName);
    }
    return QStringLiteral("%1 目录存在且可访问。").arg(displayName);
}

QString buildToneName(int readyCount, int totalCount)
{
    if (totalCount <= 0 || readyCount <= 0) {
        return QStringLiteral("danger");
    }
    if (readyCount == totalCount) {
        return QStringLiteral("ok");
    }
    return QStringLiteral("warning");
}

QString buildModuleStatusSummary(bool ready)
{
    return ready ? QStringLiteral("主流程可进入") : QStringLiteral("部分依赖待确认");
}

QString buildDashboardDicomSummary(int studyCount)
{
    return QStringLiteral("当前病例包含 %1 组 DICOM 检查。").arg(studyCount);
}

QString formatModuleTimestamp(const QDateTime& dateTime)
{
    return dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

} // namespace ThreePagePresentationUtils
```

- [ ] **Step 4: 再跑一次测试，确认表示层基础可用**

Run: `cmake --preset x64-release -DBUILD_TESTING=ON`

Expected: 配置成功且能解析 `Qt::Test`

Run: `cmake --build build_x64 --config Release --target three_page_presentation_utils_test`

Expected: 成功生成 `build_x64/tests/unit/Release/three_page_presentation_utils_test.exe`

Run: `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`

Expected: `100% tests passed`

- [ ] **Step 5: 只提交测试基础与表示层辅助文件**

```bash
git add CMakeLists.txt tests/CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/ThreePagePresentationUtilsTest.cpp UI/NewPages/ThreePagePresentationUtils.h UI/NewPages/ThreePagePresentationUtils.cpp
git commit -m "feat: add phase2 presentation utilities"
```

### Task 2: 重构 WelcomePage 为产品化欢迎入口

**Files:**
- Modify: `UI/Forms/WelcomePage.ui`
- Modify: `UI/NewPages/WelcomePage.h`
- Modify: `UI/NewPages/WelcomePage.cpp`

- [ ] **Step 1: 重写 WelcomePage 的 `.ui` 结构，做成 Hero + 三张状态卡 + 底部双按钮**

```xml
<widget class="QFrame" name="welcomeHeroFrame">
 <layout class="QVBoxLayout" name="welcomeHeroLayout">
  <item>
   <widget class="QLabel" name="productEyebrowLabel">
    <property name="text">
     <string>MEDICALPRO SURGICAL WORKSTATION</string>
    </property>
   </widget>
  </item>
  <item>
   <widget class="QLabel" name="heroTitleLabel">
    <property name="text">
     <string>智能手术导航工作站</string>
    </property>
   </widget>
  </item>
  <item>
   <widget class="QLabel" name="heroSubtitleLabel">
    <property name="text">
     <string>面向术前准备、病例管理与导航流程的统一入口</string>
    </property>
   </widget>
  </item>
 </layout>
</widget>
```

```xml
<widget class="QFrame" name="pluginFrameworkCard">
 <layout class="QVBoxLayout" name="pluginFrameworkCardLayout">
  <item><widget class="QLabel" name="pluginFrameworkTitleLabel"><property name="text"><string>插件框架</string></property></widget></item>
  <item><widget class="QLabel" name="pluginFrameworkStateLabel"><property name="text"><string>待检测</string></property></widget></item>
  <item><widget class="QLabel" name="pluginFrameworkSummaryLabel"><property name="text"><string>进入页面时刷新真实状态</string></property></widget></item>
  <item><widget class="QLabel" name="pluginFrameworkDetailLabel"><property name="text"><string>-</string></property></widget></item>
 </layout>
</widget>
```

```xml
<widget class="QFrame" name="serviceStatusCard">
 <layout class="QVBoxLayout" name="serviceStatusCardLayout">
  <item><widget class="QLabel" name="serviceStatusTitleLabel"><property name="text"><string>关键服务</string></property></widget></item>
  <item><widget class="QLabel" name="serviceStatusStateLabel"><property name="text"><string>待检测</string></property></widget></item>
  <item><widget class="QLabel" name="serviceStatusSummaryLabel"><property name="text"><string>进入页面时刷新真实状态</string></property></widget></item>
  <item><widget class="QLabel" name="serviceStatusDetailLabel"><property name="text"><string>-</string></property></widget></item>
 </layout>
</widget>
```

```xml
<widget class="QFrame" name="dataDirectoryCard">
 <layout class="QVBoxLayout" name="dataDirectoryCardLayout">
  <item><widget class="QLabel" name="dataDirectoryTitleLabel"><property name="text"><string>数据目录</string></property></widget></item>
  <item><widget class="QLabel" name="dataDirectoryStateLabel"><property name="text"><string>待检测</string></property></widget></item>
  <item><widget class="QLabel" name="dataDirectorySummaryLabel"><property name="text"><string>进入页面时刷新真实状态</string></property></widget></item>
  <item><widget class="QLabel" name="dataDirectoryDetailLabel"><property name="text"><string>-</string></property></widget></item>
 </layout>
</widget>
```

- [ ] **Step 2: 扩展 WelcomePage 头文件，加入状态刷新辅助方法**

```cpp
// UI/NewPages/WelcomePage.h
#include <QFrame>
#include <QLabel>

class WelcomePageNew : public BasePage
{
    Q_OBJECT

public:
    explicit WelcomePageNew(QWidget* parent = nullptr);
    ~WelcomePageNew();

    void onActivated() override;

signals:
    void enterSystemRequested();

private slots:
    void on_enterButton_clicked();
    void on_exitButton_clicked();

private:
    void setupUI();
    void refreshRuntimeStatus();
    void applyStatusCard(
        QFrame* card,
        QLabel* stateLabel,
        QLabel* summaryLabel,
        QLabel* detailLabel,
        const QString& tone,
        const QString& stateText,
        const QString& summaryText,
        const QString& detailText);

    Ui::WelcomePage* ui;
};
```

- [ ] **Step 3: 实现欢迎页真实状态检测与卡片刷新**

```cpp
// UI/NewPages/WelcomePage.cpp
#include "ThreePagePresentationUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QStyle>

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Framework/CTKManager.h"
#endif

void WelcomePageNew::setupUI()
{
    const QString logoPath = QCoreApplication::applicationDirPath() + "/data/logo.png";
    if (QFile::exists(logoPath)) {
        QPixmap logo(logoPath);
        ui->logoLabel->setPixmap(logo.scaled(180, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        ui->logoLabel->setText(QStringLiteral("MNS"));
    }

    ui->productEyebrowLabel->setText(QStringLiteral("MEDICALPRO SURGICAL WORKSTATION"));
    ui->heroTitleLabel->setText(QStringLiteral("智能手术导航工作站"));
    ui->heroSubtitleLabel->setText(QStringLiteral("面向术前准备、病例管理与导航流程的统一入口"));
    ui->versionLabel->setText(QStringLiteral("版本 1.0.0"));
}

void WelcomePageNew::onActivated()
{
    BasePage::onActivated();
    refreshRuntimeStatus();
}

void WelcomePageNew::refreshRuntimeStatus()
{
    const QStringList requiredServices = {
        QStringLiteral("UserManagementService"),
        QStringLiteral("DicomViewerService"),
        QStringLiteral("FourViewDisplayService")
    };

#ifdef CTK_PLUGIN_FRAMEWORK
    auto* ctkManager = CTKManager::instance();
    const bool frameworkReady = ctkManager && ctkManager->isCTKAvailable();
    const int pluginCount = ctkManager ? ctkManager->getLoadedPlugins().size() : 0;
    const QStringList missingServices = ctkManager ? ctkManager->getMissingServices(requiredServices) : requiredServices;
#else
    const bool frameworkReady = false;
    const int pluginCount = 0;
    const QStringList missingServices = requiredServices;
#endif

    const int readyServices = requiredServices.size() - missingServices.size();
    const QDir dataDir(QCoreApplication::applicationDirPath() + "/data");
    const QFileInfo dataDirInfo(dataDir.absolutePath());
    const bool dataDirExists = dataDir.exists();
    const bool dataDirReadable = dataDirExists && dataDirInfo.isReadable();

    applyStatusCard(
        ui->pluginFrameworkCard,
        ui->pluginFrameworkStateLabel,
        ui->pluginFrameworkSummaryLabel,
        ui->pluginFrameworkDetailLabel,
        frameworkReady ? QStringLiteral("ok") : QStringLiteral("danger"),
        frameworkReady ? QStringLiteral("框架已初始化") : QStringLiteral("框架未就绪"),
        ThreePagePresentationUtils::buildFrameworkSummary(frameworkReady, pluginCount),
        frameworkReady ? QStringLiteral("欢迎页已与插件主链联通。") : QStringLiteral("请先检查 CTK 初始化与插件加载日志。"));

    applyStatusCard(
        ui->serviceStatusCard,
        ui->serviceStatusStateLabel,
        ui->serviceStatusSummaryLabel,
        ui->serviceStatusDetailLabel,
        ThreePagePresentationUtils::buildToneName(readyServices, requiredServices.size()),
        readyServices == requiredServices.size() ? QStringLiteral("关键服务就绪") : QStringLiteral("关键服务需检查"),
        ThreePagePresentationUtils::buildServiceGroupSummary(readyServices, requiredServices.size()),
        missingServices.isEmpty()
            ? QStringLiteral("UserManagement / DicomViewer / FourViewDisplay 已可用。")
            : QStringLiteral("缺失服务：%1").arg(missingServices.join(QStringLiteral(" / "))));

    applyStatusCard(
        ui->dataDirectoryCard,
        ui->dataDirectoryStateLabel,
        ui->dataDirectorySummaryLabel,
        ui->dataDirectoryDetailLabel,
        ThreePagePresentationUtils::buildToneName(dataDirReadable ? 1 : 0, 1),
        dataDirReadable ? QStringLiteral("目录在线") : QStringLiteral("目录需检查"),
        ThreePagePresentationUtils::buildDirectorySummary(dataDirExists, dataDirReadable, QStringLiteral("data")),
        QStringLiteral("检测路径：%1").arg(QDir::toNativeSeparators(dataDir.absolutePath())));
}

void WelcomePageNew::applyStatusCard(
    QFrame* card,
    QLabel* stateLabel,
    QLabel* summaryLabel,
    QLabel* detailLabel,
    const QString& tone,
    const QString& stateText,
    const QString& summaryText,
    const QString& detailText)
{
    card->setProperty("statusTone", tone);
    stateLabel->setProperty("statusTone", tone);
    stateLabel->setText(stateText);
    summaryLabel->setText(summaryText);
    detailLabel->setText(detailText);

    card->style()->unpolish(card);
    card->style()->polish(card);
    stateLabel->style()->unpolish(stateLabel);
    stateLabel->style()->polish(stateLabel);
    card->update();
}
```

- [ ] **Step 4: 构建并手动核对欢迎页结果**

Run: `cmake --build --preset x64-release`

Expected: `build_x64/Release/medicalpro.exe` 成功更新

Run: `Get-Item build_x64/Release/medicalpro.exe | Select-Object FullName, LastWriteTime`

Expected: 输出 `build_x64/Release/medicalpro.exe` 的最新时间戳

手动验证：

- 运行 `build_x64/Release/medicalpro.exe`
- 第一页仍然是欢迎页
- 欢迎页出现 Hero 区、三张状态卡和底部双按钮
- 进入欢迎页时卡片会刷新真实状态，不要求定时刷新

- [ ] **Step 5: 只提交 WelcomePage 改造**

```bash
git add UI/Forms/WelcomePage.ui UI/NewPages/WelcomePage.h UI/NewPages/WelcomePage.cpp
git commit -m "feat: redesign welcome page for phase2"
```

### Task 3: 重构 ModuleSelectionPage 为正式模块门厅

**Files:**
- Modify: `UI/Forms/ModuleSelectionPage.ui`
- Modify: `UI/NewPages/ModuleSelectionPage.h`
- Modify: `UI/NewPages/ModuleSelectionPage.cpp`

- [ ] **Step 1: 重写模块选择页 `.ui`，加入顶部状态条和双主卡层级**

```xml
<widget class="QFrame" name="moduleStatusBar">
 <layout class="QHBoxLayout" name="moduleStatusBarLayout">
  <item><widget class="QLabel" name="identityCaptionLabel"><property name="text"><string>当前身份</string></property></widget></item>
  <item><widget class="QLabel" name="userInfoLabel"><property name="text"><string>访客模式</string></property></widget></item>
  <item><widget class="QLabel" name="timeCaptionLabel"><property name="text"><string>当前时间</string></property></widget></item>
  <item><widget class="QLabel" name="timeValueLabel"><property name="text"><string>--</string></property></widget></item>
  <item><widget class="QLabel" name="systemCaptionLabel"><property name="text"><string>系统状态</string></property></widget></item>
  <item><widget class="QLabel" name="systemValueLabel"><property name="text"><string>待检测</string></property></widget></item>
  <item><widget class="QPushButton" name="logoutButton"><property name="text"><string>返回欢迎页</string></property></widget></item>
 </layout>
</widget>
```

```xml
<widget class="QLabel" name="ankleSurgeryStateTag">
 <property name="text">
  <string>主流程优先</string>
 </property>
</widget>
```

```xml
<widget class="QLabel" name="systemSettingsStateTag">
 <property name="text">
  <string>配置入口</string>
 </property>
</widget>
```

- [ ] **Step 2: 扩展头文件，加入时钟与状态条刷新能力**

```cpp
// UI/NewPages/ModuleSelectionPage.h
#include <QTimer>

class ModuleSelectionPageNew : public BasePage
{
    Q_OBJECT

public:
    explicit ModuleSelectionPageNew(QWidget* parent = nullptr);
    ~ModuleSelectionPageNew();

    void onActivated() override;
    void setCurrentUser(const QString& username);

signals:
    void systemSettingsRequested();
    void ankleSurgeryRequested();
    void backRequested();

private slots:
    void on_ankleSurgeryButton_clicked();
    void on_systemSettingsButton_clicked();
    void on_logoutButton_clicked();
    void refreshClock();

private:
    void refreshHeaderState();
    void refreshModuleCards();

    Ui::ModuleSelectionPage* ui;
    QString m_currentUser;
    QTimer* m_clockTimer;
};
```

- [ ] **Step 3: 实现状态条、时间刷新和主次卡状态**

```cpp
// UI/NewPages/ModuleSelectionPage.cpp
#include "ThreePagePresentationUtils.h"

#include <QDateTime>

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Framework/CTKManager.h"
#endif

ModuleSelectionPageNew::ModuleSelectionPageNew(QWidget* parent)
    : BasePage(parent)
    , ui(new Ui::ModuleSelectionPage)
    , m_clockTimer(new QTimer(this))
{
    ui->setupUi(this);
    setObjectName("ModuleSelectionPage");

    m_clockTimer->setInterval(1000);
    connect(m_clockTimer, &QTimer::timeout, this, &ModuleSelectionPageNew::refreshClock);
    m_clockTimer->start();
}

void ModuleSelectionPageNew::onActivated()
{
    BasePage::onActivated();
    refreshHeaderState();
    refreshModuleCards();
    refreshClock();
}

void ModuleSelectionPageNew::setCurrentUser(const QString& username)
{
    m_currentUser = username;
    refreshHeaderState();
}

void ModuleSelectionPageNew::refreshClock()
{
    ui->timeValueLabel->setText(
        ThreePagePresentationUtils::formatModuleTimestamp(QDateTime::currentDateTime()));
}

void ModuleSelectionPageNew::refreshHeaderState()
{
    const QString displayName = m_currentUser.isEmpty()
        ? QStringLiteral("访客模式")
        : QStringLiteral("访客模式 / %1").arg(m_currentUser);
    ui->userInfoLabel->setText(displayName);

#ifdef CTK_PLUGIN_FRAMEWORK
    auto* ctkManager = CTKManager::instance();
    const bool ready = ctkManager && ctkManager->verifyRequiredServices({
        QStringLiteral("UserManagementService"),
        QStringLiteral("DicomViewerService"),
        QStringLiteral("FourViewDisplayService")
    });
#else
    const bool ready = false;
#endif

    ui->systemValueLabel->setProperty("statusTone", ready ? QStringLiteral("ok") : QStringLiteral("warning"));
    ui->systemValueLabel->setText(ThreePagePresentationUtils::buildModuleStatusSummary(ready));
}

void ModuleSelectionPageNew::refreshModuleCards()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* ctkManager = CTKManager::instance();
    const bool primaryReady = ctkManager && ctkManager->verifyRequiredServices({
        QStringLiteral("UserManagementService"),
        QStringLiteral("DicomViewerService"),
        QStringLiteral("FourViewDisplayService")
    });
#else
    const bool primaryReady = false;
#endif

    ui->ankleSurgeryStateTag->setProperty(
        "statusTone",
        primaryReady ? QStringLiteral("ok") : QStringLiteral("warning"));
    ui->ankleSurgeryStateTag->setText(
        primaryReady ? QStringLiteral("主流程就绪") : QStringLiteral("部分依赖待确认"));

    ui->systemSettingsStateTag->setProperty("statusTone", QStringLiteral("ok"));
    ui->systemSettingsStateTag->setText(QStringLiteral("配置入口可用"));
}
```

- [ ] **Step 4: 构建并验证门厅页层级**

Run: `cmake --build --preset x64-release`

Expected: 构建成功，无 `ModuleSelectionPage` 编译错误

手动验证：

- 从欢迎页点击进入后先到 `ModuleSelectionPage`
- 顶部状态条能看到身份、时间、系统状态和返回欢迎页按钮
- 踝关节手术卡比系统设置卡更突出
- 不增加登录要求，也不跳过欢迎页

- [ ] **Step 5: 只提交模块门厅页改造**

```bash
git add UI/Forms/ModuleSelectionPage.ui UI/NewPages/ModuleSelectionPage.h UI/NewPages/ModuleSelectionPage.cpp
git commit -m "feat: redesign module selection page for phase2"
```

### Task 4: 重构 DashboardPage 为病例工作台

**Files:**
- Modify: `UI/Forms/DashboardPage.ui`
- Modify: `UI/NewPages/DashboardPage.h`
- Modify: `UI/NewPages/DashboardPage.cpp`

- [ ] **Step 1: 重写 Dashboard `.ui`，加入顶部概览卡和底部 CTA 区**

```xml
<widget class="QFrame" name="overviewStripFrame">
 <layout class="QHBoxLayout" name="overviewStripLayout">
  <item><widget class="QFrame" name="overviewPatientCard"><layout class="QVBoxLayout" name="overviewPatientCardLayout"><item><widget class="QLabel" name="overviewPatientTitleLabel"><property name="text"><string>当前病例</string></property></widget></item><item><widget class="QLabel" name="overviewPatientValueLabel"><property name="text"><string>未选择患者</string></property></widget></item></layout></widget></item>
  <item><widget class="QFrame" name="overviewDiagnosisCard"><layout class="QVBoxLayout" name="overviewDiagnosisCardLayout"><item><widget class="QLabel" name="overviewDiagnosisTitleLabel"><property name="text"><string>诊断</string></property></widget></item><item><widget class="QLabel" name="overviewDiagnosisValueLabel"><property name="text"><string>-</string></property></widget></item></layout></widget></item>
  <item><widget class="QFrame" name="overviewDoctorCard"><layout class="QVBoxLayout" name="overviewDoctorCardLayout"><item><widget class="QLabel" name="overviewDoctorTitleLabel"><property name="text"><string>主治医生</string></property></widget></item><item><widget class="QLabel" name="overviewDoctorValueLabel"><property name="text"><string>-</string></property></widget></item></layout></widget></item>
  <item><widget class="QFrame" name="overviewDicomCard"><layout class="QVBoxLayout" name="overviewDicomCardLayout"><item><widget class="QLabel" name="overviewDicomTitleLabel"><property name="text"><string>DICOM 数量</string></property></widget></item><item><widget class="QLabel" name="overviewDicomValueLabel"><property name="text"><string>0</string></property></widget></item></layout></widget></item>
 </layout>
</widget>
```

```xml
<widget class="QFrame" name="navigationCtaFrame">
 <layout class="QHBoxLayout" name="navigationCtaLayout">
  <item><widget class="QLabel" name="navigationCtaTitleLabel"><property name="text"><string>当前病例已进入工作台</string></property></widget></item>
  <item><widget class="QLabel" name="navigationCtaHintLabel"><property name="text"><string>选择病例后即可进入导航流程</string></property></widget></item>
  <item><widget class="QPushButton" name="enterNavigationButton"><property name="text"><string>进入导航系统</string></property></widget></item>
 </layout>
</widget>
```

- [ ] **Step 2: 扩展头文件，加入概览卡与 CTA 同步函数**

```cpp
// UI/NewPages/DashboardPage.h
class DashboardPageNew : public BasePage
{
    Q_OBJECT

public:
    explicit DashboardPageNew(QWidget* parent = nullptr);
    ~DashboardPageNew();

    void onActivated() override;
    void setCurrentPatientId(int patientId);
    int getCurrentPatientId() const { return m_currentPatientId; }

signals:
    void enterNavigationRequested(int patientId);
    void backToManagementRequested();
    void logoutRequested();

private slots:
    void on_backButton_clicked();
    void on_logoutButton_clicked();
    void on_refreshButton_clicked();
    void on_enterNavigationButton_clicked();
    void on_patientListWidget_currentRowChanged(int currentRow);
    void on_patientSearchEdit_textChanged(const QString& text);

public slots:
    void refreshDashboard() { onActivated(); }

private:
    void loadPatients();
    void loadPatientDetails(int patientId);
    void loadDicomImages(int patientId);
    void clearPatientDetails();
    void setOverviewPatientSummary(const QString& patientName, const QString& diagnosis, const QString& doctorName);
    void setOverviewDicomSummary(int studyCount);
    void updateNavigationCta(bool hasPatient);

    Ui::DashboardPage* ui;
    int m_currentPatientId;
    QList<int> m_patientIds;
};
```

- [ ] **Step 3: 实现概览卡、DICOM 数量和 CTA 联动**

```cpp
// UI/NewPages/DashboardPage.cpp
#include "ThreePagePresentationUtils.h"

void DashboardPageNew::setOverviewPatientSummary(
    const QString& patientName,
    const QString& diagnosis,
    const QString& doctorName)
{
    ui->overviewPatientValueLabel->setText(patientName);
    ui->overviewDiagnosisValueLabel->setText(diagnosis);
    ui->overviewDoctorValueLabel->setText(doctorName);
}

void DashboardPageNew::setOverviewDicomSummary(int studyCount)
{
    ui->overviewDicomValueLabel->setText(QString::number(studyCount));
    ui->navigationCtaHintLabel->setText(
        ThreePagePresentationUtils::buildDashboardDicomSummary(studyCount));
}

void DashboardPageNew::updateNavigationCta(bool hasPatient)
{
    ui->enterNavigationButton->setEnabled(hasPatient);
    ui->navigationCtaTitleLabel->setText(
        hasPatient ? QStringLiteral("当前病例已进入工作台") : QStringLiteral("请选择病例后进入导航流程"));
}

void DashboardPageNew::loadPatientDetails(int patientId)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* userService = CTKManager::instance()->getService<UserManagementService>();
    if (userService) {
        const auto patient = userService->getPatient(patientId);
        if (patient.id > 0) {
            ui->patientNameLabel->setText(patient.name);
            ui->idLabel->setText(QString::number(patient.id));
            ui->genderLabel->setText(patient.gender);
            ui->ageLabel->setText(QString::number(patient.age));
            ui->phoneLabel->setText(patient.phone);
            ui->diagnosisLabel->setText(patient.description);
            ui->doctorNameLabel->setText(QStringLiteral("-"));
            ui->departmentLabel->setText(patient.description);
            setOverviewPatientSummary(patient.name, patient.description, QStringLiteral("-"));
            updateNavigationCta(true);
            return;
        }
    }
#endif

    ui->patientNameLabel->setText(QStringLiteral("患者%1").arg(patientId));
    ui->idLabel->setText(QString::number(patientId));
    ui->genderLabel->setText(QStringLiteral("男"));
    ui->ageLabel->setText(QStringLiteral("45"));
    ui->phoneLabel->setText(QStringLiteral("139xxxx1234"));
    ui->diagnosisLabel->setText(QStringLiteral("踝关节炎，需进一步评估置换方案。"));
    ui->doctorNameLabel->setText(QStringLiteral("张伟 医生"));
    ui->departmentLabel->setText(QStringLiteral("骨科"));
    setOverviewPatientSummary(
        QStringLiteral("患者%1").arg(patientId),
        QStringLiteral("踝关节炎，需进一步评估置换方案。"),
        QStringLiteral("张伟 医生"));
    updateNavigationCta(true);
}

void DashboardPageNew::loadDicomImages(int patientId)
{
    int studyCount = 0;
    ui->noDicomLabel->hide();

    QLayoutItem* child = nullptr;
    while ((child = ui->dicomContentLayout->takeAt(0)) != nullptr) {
        if (child->widget() && child->widget() != ui->noDicomLabel) {
            delete child->widget();
        }
        delete child;
    }

#ifdef CTK_PLUGIN_FRAMEWORK
    auto* dicomService = CTKManager::instance()->getService<DicomViewerService>();
    if (dicomService) {
        const auto studies = dicomService->listStudiesByPatient(patientId);
        studyCount = studies.size();
        if (!studies.isEmpty()) {
            for (const auto& study : studies) {
                auto* card = new QFrame();
                card->setObjectName("dicomStudyCard");

                auto* cardLayout = new QVBoxLayout(card);
                cardLayout->setContentsMargins(12, 12, 12, 12);
                cardLayout->setSpacing(10);

                auto* thumbLabel = new QLabel(card);
                thumbLabel->setObjectName("dicomThumbLabel");
                thumbLabel->setFixedSize(120, 92);
                thumbLabel->setAlignment(Qt::AlignCenter);
                thumbLabel->setText(QStringLiteral("DICOM"));

                auto* dateLabel = new QLabel(study.studyDate.toString(QStringLiteral("yyyy-MM-dd")), card);
                dateLabel->setObjectName("dicomDateLabel");
                dateLabel->setAlignment(Qt::AlignCenter);

                cardLayout->addWidget(thumbLabel, 0, Qt::AlignCenter);
                cardLayout->addWidget(dateLabel, 0, Qt::AlignCenter);
                ui->dicomContentLayout->addWidget(card);
            }
            ui->dicomContentLayout->addStretch();
            setOverviewDicomSummary(studyCount);
            return;
        }
    }
#endif

    setOverviewDicomSummary(studyCount);
    ui->noDicomLabel->setText(QStringLiteral("该患者暂无 DICOM 影像"));
    ui->dicomContentLayout->addWidget(ui->noDicomLabel);
    ui->noDicomLabel->show();
}

void DashboardPageNew::clearPatientDetails()
{
    m_currentPatientId = -1;
    ui->patientNameLabel->setText(QStringLiteral("请选择患者"));
    ui->idLabel->setText(QStringLiteral("-"));
    ui->genderLabel->setText(QStringLiteral("-"));
    ui->ageLabel->setText(QStringLiteral("-"));
    ui->phoneLabel->setText(QStringLiteral("-"));
    ui->diagnosisLabel->setText(QStringLiteral("-"));
    ui->doctorNameLabel->setText(QStringLiteral("-"));
    ui->departmentLabel->setText(QStringLiteral("-"));
    setOverviewPatientSummary(QStringLiteral("未选择患者"), QStringLiteral("-"), QStringLiteral("-"));
    setOverviewDicomSummary(0);
    updateNavigationCta(false);
}
```

- [ ] **Step 4: 构建并手动验证工作台层级**

Run: `cmake --build --preset x64-release`

Expected: 构建成功，无 `DashboardPage` 编译错误

手动验证：

- 左侧仍然是患者列表，右侧变成顶部概览卡 + 中部详情区 + 底部 CTA
- 选择患者后，顶部概览卡同步更新姓名、诊断、主治医生和 DICOM 数量
- 未选择患者时，`进入导航系统` 按钮为禁用态

- [ ] **Step 5: 只提交 Dashboard 工作台改造**

```bash
git add UI/Forms/DashboardPage.ui UI/NewPages/DashboardPage.h UI/NewPages/DashboardPage.cpp
git commit -m "feat: redesign dashboard page for phase2"
```

### Task 5: 补齐公共主题令牌与 QSS 收口

**Files:**
- Modify: `UI/AppTheme.h`
- Modify: `UI/styles/three_pages_theme.qss`

- [ ] **Step 1: 为 Phase 2 视觉层补充主题令牌**

```cpp
// UI/AppTheme.h
inline QMap<QString, QString> threePageTokens()
{
    return {
        { "${PAGE_BG_START}", "#081421" },
        { "${PAGE_BG_END}", "#10243b" },
        { "${PAGE_BG_ACCENT}", "#1a3657" },
        { "${SURFACE_SOFT}", "rgba(9, 19, 31, 0.78)" },
        { "${SURFACE_CARD}", "rgba(13, 27, 42, 0.88)" },
        { "${SURFACE_CARD_HOVER}", "rgba(18, 38, 58, 0.96)" },
        { "${SURFACE_ELEVATED}", "rgba(19, 35, 53, 0.94)" },
        { "${SURFACE_PANEL}", "rgba(7, 16, 27, 0.72)" },
        { "${BORDER_SOFT}", "rgba(124, 160, 191, 0.18)" },
        { "${BORDER_STRONG}", "rgba(124, 160, 191, 0.32)" },
        { "${TEXT_PRIMARY}", "#f4f8fc" },
        { "${TEXT_SECONDARY}", "#b4c4d6" },
        { "${TEXT_MUTED}", "#7f92a8" },
        { "${ACCENT_PRIMARY}", "#ff7a59" },
        { "${ACCENT_PRIMARY_HOVER}", "#ff936f" },
        { "${ACCENT_SECONDARY}", "#5bb6ff" },
        { "${SUCCESS}", "#3ddc97" },
        { "${WARNING}", "#ffbf69" },
        { "${DANGER}", "#ff6b6b" },
        { "${SHADOW}", "rgba(0, 0, 0, 0.24)" }
    };
}
```

- [ ] **Step 2: 用公共 QSS 收口欢迎页、门厅页和工作台页视觉样式**

```qss
QFrame#welcomeHeroFrame,
QFrame#moduleStatusBar,
QFrame#overviewStripFrame,
QFrame#navigationCtaFrame {
    background-color: ${SURFACE_ELEVATED};
    border: 1px solid ${BORDER_SOFT};
    border-radius: 22px;
}

QFrame#pluginFrameworkCard,
QFrame#serviceStatusCard,
QFrame#dataDirectoryCard,
QFrame#overviewPatientCard,
QFrame#overviewDiagnosisCard,
QFrame#overviewDoctorCard,
QFrame#overviewDicomCard {
    background-color: ${SURFACE_SOFT};
    border: 1px solid ${BORDER_SOFT};
    border-radius: 18px;
}

QFrame[statusTone="ok"] {
    border-color: ${SUCCESS};
}

QFrame[statusTone="warning"] {
    border-color: ${WARNING};
}

QFrame[statusTone="danger"] {
    border-color: ${DANGER};
}

QLabel[statusTone="ok"],
QLabel#ankleSurgeryStateTag[statusTone="ok"],
QLabel#systemSettingsStateTag[statusTone="ok"] {
    color: ${SUCCESS};
}

QLabel[statusTone="warning"],
QLabel#ankleSurgeryStateTag[statusTone="warning"] {
    color: ${WARNING};
}

QLabel[statusTone="danger"] {
    color: ${DANGER};
}

QLabel#productEyebrowLabel {
    color: ${ACCENT_SECONDARY};
    font-size: 14px;
    font-weight: 700;
    letter-spacing: 1px;
}

QLabel#heroTitleLabel {
    color: ${TEXT_PRIMARY};
    font-size: 38px;
    font-weight: 700;
}

QLabel#heroSubtitleLabel,
QLabel#navigationCtaHintLabel {
    color: ${TEXT_SECONDARY};
    font-size: 16px;
}

QLabel#ankleSurgeryStateTag,
QLabel#systemSettingsStateTag,
QLabel#pluginFrameworkStateLabel,
QLabel#serviceStatusStateLabel,
QLabel#dataDirectoryStateLabel,
QLabel#systemValueLabel {
    font-size: 14px;
    font-weight: 700;
}

QPushButton#ankleSurgeryButton,
QPushButton#enterNavigationButton,
QPushButton#enterButton {
    min-height: 46px;
}
```

- [ ] **Step 3: 构建并检查样式资源未破坏现有入口**

Run: `cmake --build --preset x64-release`

Expected: `medicalpro` 构建成功，资源系统可正常加载 `three_pages_theme.qss`

手动验证：

- 三页背景、卡片、CTA 语言统一
- Welcome / ModuleSelection / Dashboard 三页明显属于同一套产品
- 不影响 `build_x64/Release/medicalpro.exe` 运行入口

- [ ] **Step 4: 只提交主题与样式收口**

```bash
git add UI/AppTheme.h UI/styles/three_pages_theme.qss
git commit -m "feat: add phase2 theme styling for three pages"
```

### Task 6: 统一验证并把完成情况回写文档

**Files:**
- Modify: `docs/current_status_and_project_overview.md`

- [ ] **Step 1: 进行完整构建与测试验证**

Run: `cmake --preset x64-release -DBUILD_TESTING=ON`

Expected: `build_x64` 重新完成 Release 配置，测试目标可见

Run: `cmake --build build_x64 --config Release --target medicalpro three_page_presentation_utils_test`

Expected: 同时生成 `build_x64/Release/medicalpro.exe` 和测试可执行文件

Run: `ctest --test-dir build_x64 -C Release --output-on-failure`

Expected: `three_page_presentation_utils_test` 通过，输出中无失败用例

- [ ] **Step 2: 手动走完整页面主链**

Run: `build_x64/Release/medicalpro.exe`

Expected: 程序从欢迎页进入，流程为 `Welcome -> ModuleSelection -> Management / Dashboard`，不要求登录，也不跳过欢迎页

手动验证：

- 欢迎页显示 Hero、三张真实状态卡与双按钮
- 模块门厅显示顶部状态条和双主卡
- 工作台显示顶部概览卡、中部详情区和底部导航 CTA
- 控制台输出仍然存在，不因 Phase 2 UI 改造而消失

- [ ] **Step 3: 把 Phase 2 完成情况同步到项目现状文档**

```md
## 1.6 UI Phase 2 已完成
- 已完成 WelcomePage 视觉升级，保留欢迎页入口并接入真实运行状态卡
- 已完成 ModuleSelectionPage 门厅化改版，顶部状态条展示身份、时间与系统状态
- 已完成 DashboardPage 工作台化改版，顶部概览卡与底部 CTA 已接入现有病例数据流
- 已补齐三页 Phase 2 公共 QSS 与主题令牌，三页视觉语言统一
```

```md
## 4. 当前建议
- 后续若继续做 UI 深化，可在 Phase 3 评估 `QWindowKit` 或 `KDDockWidgets`
- 当前主线仍以三页体验打磨、导航流程细节和日志可观测性为优先
```

- [ ] **Step 4: 检索文档与构建产物，确认交付闭环**

Run: `rg -n "UI Phase 2 已完成|WelcomePage 视觉升级|ModuleSelectionPage 门厅化|DashboardPage 工作台化" docs/current_status_and_project_overview.md`

Expected: 文档中能检索到 Phase 2 完成记录

Run: `Get-Item build_x64/Release/medicalpro.exe | Select-Object FullName, LastWriteTime`

Expected: 输出最新 Release 可执行文件路径和时间戳

- [ ] **Step 5: 只提交验证与文档回写**

```bash
git add docs/current_status_and_project_overview.md
git commit -m "docs: update current status after phase2 ui delivery"
```

