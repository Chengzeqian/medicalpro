# WelcomePage Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `WelcomePage` 升级为符合已确认 spec 的“医疗旗舰工作站入口页”，保留欢迎页入口、免登录流程、真实运行状态摘要和进入 `ModuleSelectionPage` 的主链。

**Architecture:** 继续沿用现有 `Qt Widgets + .ui + NewPages + QSS` 架构。把可测试的状态摘要和结论文案下沉到 `ThreePagePresentationUtils`，把页面结构重构集中在 `WelcomePage.ui`，把运行时绑定、响应式布局和状态刷新集中在 `WelcomePage.cpp`，最后用 Welcome 专属 QSS 选择器和少量主题令牌收口视觉层。

**Tech Stack:** Qt Widgets、Qt Designer `.ui`、Qt Style Sheets、Qt Test、CMake、CTK 服务状态查询

---

## File Structure

- Modify: `UI/NewPages/ThreePagePresentationUtils.h`
- Modify: `UI/NewPages/ThreePagePresentationUtils.cpp`
- Modify: `tests/unit/ThreePagePresentationUtilsTest.cpp`
- Modify: `UI/Forms/WelcomePage.ui`
- Modify: `UI/NewPages/WelcomePage.h`
- Modify: `UI/NewPages/WelcomePage.cpp`
- Modify: `UI/AppTheme.h`
- Modify: `UI/styles/three_pages_theme.qss`
- Modify: `docs/current_status_and_project_overview.md`

职责边界：

- `ThreePagePresentationUtils.*` 只负责 WelcomePage 运行摘要与结论文案，保持纯 `QtCore`，便于单元测试。
- `ThreePagePresentationUtilsTest.cpp` 只覆盖 WelcomePage 的纯文案与 tone 决策逻辑，不碰 GUI。
- `WelcomePage.ui` 只负责结构、对象名、布局层级和尺寸约束，不放运行时判断逻辑。
- `WelcomePage.cpp/.h` 只负责真实状态采集、界面赋值、响应式布局切换和状态卡刷新。
- `AppTheme.h` 只新增 WelcomePage Phase 2 需要的主题令牌。
- `three_pages_theme.qss` 只收口 WelcomePage Phase 2 的视觉样式，不回退已有三页公共基础样式。
- `current_status_and_project_overview.md` 只记录本轮 WelcomePage Phase 2 完成情况。

### Task 1: 扩展 WelcomePage 表示层文案并补齐单元测试

**Files:**
- Modify: `UI/NewPages/ThreePagePresentationUtils.h`
- Modify: `UI/NewPages/ThreePagePresentationUtils.cpp`
- Modify: `tests/unit/ThreePagePresentationUtilsTest.cpp`

- [ ] **Step 1: 先把 WelcomePage 新文案对应的失败测试加进去**

```cpp
// tests/unit/ThreePagePresentationUtilsTest.cpp
private slots:
    void buildWelcomeRuntimeSummary_reportsReady();
    void buildWelcomeRuntimeSummary_reportsWarning();
    void buildWelcomeRuntimeSummary_reportsBlocked();
    void buildWelcomeDecisionLabel_reportsReady();
    void buildWelcomeDecisionLabel_reportsWarning();
    void buildWelcomeDecisionLabel_reportsBlocked();
    void buildWelcomeDecisionTone_reportsReady();
    void buildWelcomeDecisionTone_reportsWarning();
    void buildWelcomeDecisionTone_reportsBlocked();
```

```cpp
// tests/unit/ThreePagePresentationUtilsTest.cpp
void ThreePagePresentationUtilsTest::buildWelcomeRuntimeSummary_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeRuntimeSummary(true, 3, 3, true),
        QStringLiteral("插件框架已联通，关键服务持续自检中，系统当前处于可进入状态。"));
}

void ThreePagePresentationUtilsTest::buildWelcomeRuntimeSummary_reportsWarning()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeRuntimeSummary(true, 2, 3, true),
        QStringLiteral("插件框架已联通，部分关键服务仍在检查，系统当前可进入但建议先确认状态。"));
}

void ThreePagePresentationUtilsTest::buildWelcomeRuntimeSummary_reportsBlocked()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeRuntimeSummary(false, 0, 3, false),
        QStringLiteral("插件框架或关键依赖仍在检查，当前暂不建议进入主流程。"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionLabel_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionLabel(true, 3, 3, true),
        QStringLiteral("主流程可进入"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionLabel_reportsWarning()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionLabel(true, 2, 3, true),
        QStringLiteral("可进入，建议检查"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionLabel_reportsBlocked()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionLabel(false, 0, 3, false),
        QStringLiteral("暂不建议进入"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionTone_reportsReady()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionTone(true, 3, 3, true),
        QStringLiteral("ok"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionTone_reportsWarning()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionTone(true, 2, 3, true),
        QStringLiteral("warning"));
}

void ThreePagePresentationUtilsTest::buildWelcomeDecisionTone_reportsBlocked()
{
    QCOMPARE(
        ThreePagePresentationUtils::buildWelcomeDecisionTone(false, 0, 3, false),
        QStringLiteral("danger"));
}
```

- [ ] **Step 2: 运行测试目标，确认它先因为新函数缺失而失败**

Run: `cmake --build build_x64 --config Release --target three_page_presentation_utils_test`

Expected: 编译失败，并提示 `buildWelcomeRuntimeSummary`、`buildWelcomeDecisionLabel` 或 `buildWelcomeDecisionTone` 未声明或未定义

- [ ] **Step 3: 在表示层辅助文件里补齐 WelcomePage 运行摘要与结论函数**

```cpp
// UI/NewPages/ThreePagePresentationUtils.h
QString buildWelcomeRuntimeSummary(
    bool frameworkReady,
    int readyServices,
    int totalServices,
    bool dataDirectoryReadable);
QString buildWelcomeDecisionLabel(
    bool frameworkReady,
    int readyServices,
    int totalServices,
    bool dataDirectoryReadable);
QString buildWelcomeDecisionTone(
    bool frameworkReady,
    int readyServices,
    int totalServices,
    bool dataDirectoryReadable);
```

```cpp
// UI/NewPages/ThreePagePresentationUtils.cpp
QString buildWelcomeRuntimeSummary(
    bool frameworkReady,
    int readyServices,
    int totalServices,
    bool dataDirectoryReadable)
{
    if (!frameworkReady || !dataDirectoryReadable || readyServices <= 0 || totalServices <= 0) {
        return QStringLiteral("插件框架或关键依赖仍在检查，当前暂不建议进入主流程。");
    }
    if (readyServices == totalServices) {
        return QStringLiteral("插件框架已联通，关键服务持续自检中，系统当前处于可进入状态。");
    }
    return QStringLiteral("插件框架已联通，部分关键服务仍在检查，系统当前可进入但建议先确认状态。");
}

QString buildWelcomeDecisionLabel(
    bool frameworkReady,
    int readyServices,
    int totalServices,
    bool dataDirectoryReadable)
{
    if (!frameworkReady || !dataDirectoryReadable || readyServices <= 0 || totalServices <= 0) {
        return QStringLiteral("暂不建议进入");
    }
    if (readyServices == totalServices) {
        return QStringLiteral("主流程可进入");
    }
    return QStringLiteral("可进入，建议检查");
}

QString buildWelcomeDecisionTone(
    bool frameworkReady,
    int readyServices,
    int totalServices,
    bool dataDirectoryReadable)
{
    if (!frameworkReady || !dataDirectoryReadable || readyServices <= 0 || totalServices <= 0) {
        return QStringLiteral("danger");
    }
    if (readyServices == totalServices) {
        return QStringLiteral("ok");
    }
    return QStringLiteral("warning");
}
```

- [ ] **Step 4: 重新编译并运行单元测试，确认 WelcomePage 文案逻辑稳定**

Run: `cmake --build build_x64 --config Release --target three_page_presentation_utils_test`

Expected: 成功生成 `build_x64/tests/unit/Release/three_page_presentation_utils_test.exe`

Run: `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`

Expected: `100% tests passed`

- [ ] **Step 5: 只提交表示层与测试**

```bash
git add UI/NewPages/ThreePagePresentationUtils.h UI/NewPages/ThreePagePresentationUtils.cpp tests/unit/ThreePagePresentationUtilsTest.cpp
git commit -m "feat: add welcome page summary presentation logic"
```

### Task 2: 重构 WelcomePage.ui 为旗舰工作站入口布局

**Files:**
- Modify: `UI/Forms/WelcomePage.ui`

- [ ] **Step 1: 把顶部结构改成左品牌区 + 右运行摘要区的双栏主视觉**

```xml
<!-- UI/Forms/WelcomePage.ui -->
<item>
 <widget class="QFrame" name="welcomeHeroFrame">
  <property name="frameShape">
   <enum>QFrame::NoFrame</enum>
  </property>
  <layout class="QHBoxLayout" name="heroSplitLayout">
   <property name="spacing">
    <number>24</number>
   </property>
   <item>
    <widget class="QFrame" name="brandPanelFrame">
     <property name="minimumSize">
      <size>
       <width>0</width>
       <height>380</height>
      </size>
     </property>
     <layout class="QVBoxLayout" name="brandPanelLayout">
      <property name="leftMargin">
       <number>28</number>
      </property>
      <property name="topMargin">
       <number>28</number>
      </property>
      <property name="rightMargin">
       <number>28</number>
      </property>
      <property name="bottomMargin">
       <number>28</number>
      </property>
      <property name="spacing">
       <number>16</number>
      </property>
      <item>
       <widget class="QLabel" name="logoLabel"/>
      </item>
      <item>
       <widget class="QLabel" name="productEyebrowLabel"/>
      </item>
      <item>
       <widget class="QLabel" name="heroTitleLabel">
        <property name="wordWrap">
         <bool>true</bool>
        </property>
       </widget>
      </item>
      <item>
       <widget class="QLabel" name="heroSubtitleLabel">
        <property name="wordWrap">
         <bool>true</bool>
        </property>
       </widget>
      </item>
      <item>
       <spacer name="brandStretchSpacer">
        <property name="orientation">
         <enum>Qt::Vertical</enum>
        </property>
        <property name="sizeType">
         <enum>QSizePolicy::Expanding</enum>
        </property>
       </spacer>
      </item>
      <item>
       <layout class="QHBoxLayout" name="heroButtonLayout">
        <property name="spacing">
         <number>12</number>
        </property>
        <item>
         <widget class="QPushButton" name="enterButton"/>
        </item>
        <item>
         <widget class="QPushButton" name="exitButton"/>
        </item>
       </layout>
      </item>
      <item>
       <widget class="QLabel" name="versionLabel"/>
      </item>
     </layout>
    </widget>
   </item>
   <item>
    <widget class="QFrame" name="runtimeSummaryFrame">
     <property name="minimumSize">
      <size>
       <width>360</width>
       <height>380</height>
      </size>
     </property>
     <layout class="QVBoxLayout" name="runtimeSummaryLayout">
      <property name="leftMargin">
       <number>24</number>
      </property>
      <property name="topMargin">
       <number>24</number>
      </property>
      <property name="rightMargin">
       <number>24</number>
      </property>
      <property name="bottomMargin">
       <number>24</number>
      </property>
      <property name="spacing">
       <number>16</number>
      </property>
      <item>
       <widget class="QLabel" name="runtimeSummaryTitleLabel"/>
      </item>
      <item>
       <widget class="QLabel" name="runtimeSummaryTextLabel">
        <property name="wordWrap">
         <bool>true</bool>
        </property>
       </widget>
      </item>
      <item>
       <widget class="QFrame" name="frameworkQuickStatFrame"/>
      </item>
      <item>
       <widget class="QFrame" name="serviceQuickStatFrame"/>
      </item>
      <item>
       <widget class="QFrame" name="directoryQuickStatFrame"/>
      </item>
      <item>
       <spacer name="runtimeSummaryStretchSpacer">
        <property name="orientation">
         <enum>Qt::Vertical</enum>
        </property>
        <property name="sizeType">
         <enum>QSizePolicy::Expanding</enum>
        </property>
       </spacer>
      </item>
      <item>
       <widget class="QLabel" name="runtimeDecisionBadge"/>
      </item>
     </layout>
    </widget>
   </item>
  </layout>
 </widget>
</item>
```

- [ ] **Step 2: 给右侧三条速览补齐稳定对象名，便于 cpp 精确赋值**

```xml
<!-- UI/Forms/WelcomePage.ui -->
<widget class="QFrame" name="frameworkQuickStatFrame">
 <layout class="QVBoxLayout" name="frameworkQuickStatLayout">
  <property name="spacing">
   <number>6</number>
  </property>
  <item><widget class="QLabel" name="frameworkQuickTitleLabel"><property name="text"><string>插件框架</string></property></widget></item>
  <item><widget class="QLabel" name="frameworkQuickValueLabel"><property name="text"><string>已识别 0 个插件</string></property></widget></item>
  <item><widget class="QLabel" name="frameworkQuickToneLabel"><property name="text"><string>检查中</string></property></widget></item>
 </layout>
</widget>

<widget class="QFrame" name="serviceQuickStatFrame">
 <layout class="QVBoxLayout" name="serviceQuickStatLayout">
  <property name="spacing">
   <number>6</number>
  </property>
  <item><widget class="QLabel" name="serviceQuickTitleLabel"><property name="text"><string>关键服务</string></property></widget></item>
  <item><widget class="QLabel" name="serviceQuickValueLabel"><property name="text"><string>0/3 已就绪</string></property></widget></item>
  <item><widget class="QLabel" name="serviceQuickToneLabel"><property name="text"><string>检查中</string></property></widget></item>
 </layout>
</widget>

<widget class="QFrame" name="directoryQuickStatFrame">
 <layout class="QVBoxLayout" name="directoryQuickStatLayout">
  <property name="spacing">
   <number>6</number>
  </property>
  <item><widget class="QLabel" name="directoryQuickTitleLabel"><property name="text"><string>数据目录</string></property></widget></item>
  <item><widget class="QLabel" name="directoryQuickValueLabel"><property name="text"><string>data 可访问</string></property></widget></item>
  <item><widget class="QLabel" name="directoryQuickToneLabel"><property name="text"><string>检查中</string></property></widget></item>
 </layout>
</widget>
```

- [ ] **Step 3: 把下方状态卡改成可响应式重排的 `QGridLayout` 容器**

```xml
<!-- UI/Forms/WelcomePage.ui -->
<item>
 <widget class="QFrame" name="statusCardsFrame">
  <property name="frameShape">
   <enum>QFrame::NoFrame</enum>
  </property>
  <layout class="QGridLayout" name="statusCardsGridLayout">
   <property name="horizontalSpacing">
    <number>16</number>
   </property>
   <property name="verticalSpacing">
    <number>16</number>
   </property>
   <item row="0" column="0">
    <widget class="QFrame" name="pluginFrameworkCard"/>
   </item>
   <item row="0" column="1">
    <widget class="QFrame" name="serviceStatusCard"/>
   </item>
   <item row="0" column="2">
    <widget class="QFrame" name="dataDirectoryCard"/>
   </item>
  </layout>
 </widget>
</item>
```

- [ ] **Step 4: 只编译 `NewPagesLib`，先确认 `.ui` 结构和对象名没有 UIC 错误**

Run: `cmake --build build_x64 --config Release --target NewPagesLib`

Expected: `WelcomePage.ui` 能成功通过 AUTOUIC 生成，不出现对象重名或缺少闭合标签错误

- [ ] **Step 5: 只提交 WelcomePage 结构改版**

```bash
git add UI/Forms/WelcomePage.ui
git commit -m "feat: rebuild welcome page layout for phase2"
```

### Task 3: 重构 WelcomePage 运行时绑定与响应式布局

**Files:**
- Modify: `UI/NewPages/WelcomePage.h`
- Modify: `UI/NewPages/WelcomePage.cpp`

- [ ] **Step 1: 扩展头文件，加入运行时快照和响应式辅助方法**

```cpp
// UI/NewPages/WelcomePage.h
#include <QStringList>

class QResizeEvent;

class WelcomePageNew : public BasePage
{
    Q_OBJECT

public:
    explicit WelcomePageNew(QWidget* parent = nullptr);
    ~WelcomePageNew();

    void onActivated() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void enterSystemRequested();

private slots:
    void on_enterButton_clicked();
    void on_exitButton_clicked();

private:
    struct RuntimeStatusSnapshot
    {
        bool frameworkReady;
        int pluginCount;
        int readyServices;
        int totalServices;
        bool dataDirectoryExists;
        bool dataDirectoryReadable;
        QStringList missingServices;
    };

    void setupUI();
    void refreshRuntimeStatus();
    void updateResponsiveLayout();
    RuntimeStatusSnapshot collectRuntimeStatus() const;
    void applySummaryPanel(const RuntimeStatusSnapshot& snapshot);
    void applyQuickStat(
        QLabel* valueLabel,
        QLabel* toneLabel,
        const QString& valueText,
        const QString& toneText,
        const QString& toneName);
    QString buildServiceDetailText(const QStringList& missingServices) const;
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

- [ ] **Step 2: 实现静态文案、真实运行摘要、右侧速览、结论标签和响应式布局**

```cpp
// UI/NewPages/WelcomePage.cpp
#include <QBoxLayout>
#include <QGridLayout>
#include <QResizeEvent>
```

```cpp
// UI/NewPages/WelcomePage.cpp
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
    ui->runtimeSummaryTitleLabel->setText(QStringLiteral("当前运行状态"));
    ui->runtimeSummaryTextLabel->setText(QStringLiteral("插件框架已联通，关键服务持续自检中，系统当前处于可进入状态。"));
    ui->versionLabel->setText(QStringLiteral("版本 1.0.0"));

    ui->frameworkQuickTitleLabel->setText(QStringLiteral("插件框架"));
    ui->serviceQuickTitleLabel->setText(QStringLiteral("关键服务"));
    ui->directoryQuickTitleLabel->setText(QStringLiteral("数据目录"));

    ui->enterButton->setText(QStringLiteral("进入系统"));
    ui->exitButton->setText(QStringLiteral("退出"));

    updateResponsiveLayout();
}

WelcomePageNew::RuntimeStatusSnapshot WelcomePageNew::collectRuntimeStatus() const
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

    const int readyServices = qMax(0, requiredServices.size() - missingServices.size());
    const QDir dataDir(QCoreApplication::applicationDirPath() + "/data");
    const QFileInfo dataDirInfo(dataDir.absolutePath());
    const bool dataDirectoryExists = dataDir.exists();
    const bool dataDirectoryReadable = dataDirectoryExists && dataDirInfo.isReadable();

    return {
        frameworkReady,
        pluginCount,
        readyServices,
        requiredServices.size(),
        dataDirectoryExists,
        dataDirectoryReadable,
        missingServices
    };
}

void WelcomePageNew::applyQuickStat(
    QLabel* valueLabel,
    QLabel* toneLabel,
    const QString& valueText,
    const QString& toneText,
    const QString& toneName)
{
    valueLabel->setText(valueText);
    toneLabel->setText(toneText);
    toneLabel->setProperty("statusTone", toneName);
    toneLabel->style()->unpolish(toneLabel);
    toneLabel->style()->polish(toneLabel);
    toneLabel->update();
}

QString WelcomePageNew::buildServiceDetailText(const QStringList& missingServices) const
{
    return missingServices.isEmpty()
        ? QStringLiteral("UserManagement / DicomViewer / FourViewDisplay 已可用。")
        : QStringLiteral("缺失服务：%1").arg(missingServices.join(QStringLiteral(" / ")));
}

void WelcomePageNew::applySummaryPanel(const RuntimeStatusSnapshot& snapshot)
{
    const QString decisionTone = ThreePagePresentationUtils::buildWelcomeDecisionTone(
        snapshot.frameworkReady,
        snapshot.readyServices,
        snapshot.totalServices,
        snapshot.dataDirectoryReadable);
    const QString serviceTone = ThreePagePresentationUtils::buildToneName(
        snapshot.readyServices,
        snapshot.totalServices);
    const QString directoryTone = snapshot.dataDirectoryReadable ? QStringLiteral("ok") : QStringLiteral("danger");

    ui->runtimeSummaryTextLabel->setText(
        ThreePagePresentationUtils::buildWelcomeRuntimeSummary(
            snapshot.frameworkReady,
            snapshot.readyServices,
            snapshot.totalServices,
            snapshot.dataDirectoryReadable));
    ui->runtimeDecisionBadge->setText(
        ThreePagePresentationUtils::buildWelcomeDecisionLabel(
            snapshot.frameworkReady,
            snapshot.readyServices,
            snapshot.totalServices,
            snapshot.dataDirectoryReadable));
    ui->runtimeDecisionBadge->setProperty("statusTone", decisionTone);
    ui->runtimeDecisionBadge->style()->unpolish(ui->runtimeDecisionBadge);
    ui->runtimeDecisionBadge->style()->polish(ui->runtimeDecisionBadge);

    applyQuickStat(
        ui->frameworkQuickValueLabel,
        ui->frameworkQuickToneLabel,
        QStringLiteral("已识别 %1 个插件").arg(snapshot.pluginCount),
        snapshot.frameworkReady ? QStringLiteral("正常") : QStringLiteral("异常"),
        snapshot.frameworkReady ? QStringLiteral("ok") : QStringLiteral("danger"));

    applyQuickStat(
        ui->serviceQuickValueLabel,
        ui->serviceQuickToneLabel,
        QStringLiteral("%1/%2 已就绪").arg(snapshot.readyServices).arg(snapshot.totalServices),
        serviceTone == QStringLiteral("ok")
            ? QStringLiteral("正常")
            : (serviceTone == QStringLiteral("warning") ? QStringLiteral("建议检查") : QStringLiteral("未就绪")),
        serviceTone);

    applyQuickStat(
        ui->directoryQuickValueLabel,
        ui->directoryQuickToneLabel,
        snapshot.dataDirectoryReadable ? QStringLiteral("data 可访问") : QStringLiteral("data 需检查"),
        snapshot.dataDirectoryReadable ? QStringLiteral("可访问") : QStringLiteral("不可用"),
        directoryTone);
}

void WelcomePageNew::refreshRuntimeStatus()
{
    const RuntimeStatusSnapshot snapshot = collectRuntimeStatus();
    applySummaryPanel(snapshot);

    applyStatusCard(
        ui->pluginFrameworkCard,
        ui->pluginFrameworkStateLabel,
        ui->pluginFrameworkSummaryLabel,
        ui->pluginFrameworkDetailLabel,
        snapshot.frameworkReady ? QStringLiteral("ok") : QStringLiteral("danger"),
        snapshot.frameworkReady ? QStringLiteral("框架已初始化") : QStringLiteral("框架未就绪"),
        ThreePagePresentationUtils::buildFrameworkSummary(snapshot.frameworkReady, snapshot.pluginCount),
        snapshot.frameworkReady
            ? QStringLiteral("RegistrationCore 延迟加载策略已生效。")
            : QStringLiteral("请先检查 CTK 初始化与插件加载日志。"));

    applyStatusCard(
        ui->serviceStatusCard,
        ui->serviceStatusStateLabel,
        ui->serviceStatusSummaryLabel,
        ui->serviceStatusDetailLabel,
        ThreePagePresentationUtils::buildToneName(snapshot.readyServices, snapshot.totalServices),
        snapshot.readyServices == snapshot.totalServices ? QStringLiteral("关键服务就绪") : QStringLiteral("关键服务需检查"),
        ThreePagePresentationUtils::buildServiceGroupSummary(snapshot.readyServices, snapshot.totalServices),
        buildServiceDetailText(snapshot.missingServices));

    applyStatusCard(
        ui->dataDirectoryCard,
        ui->dataDirectoryStateLabel,
        ui->dataDirectorySummaryLabel,
        ui->dataDirectoryDetailLabel,
        snapshot.dataDirectoryReadable ? QStringLiteral("ok") : QStringLiteral("danger"),
        snapshot.dataDirectoryReadable ? QStringLiteral("目录在线") : QStringLiteral("目录需检查"),
        ThreePagePresentationUtils::buildDirectorySummary(
            snapshot.dataDirectoryExists,
            snapshot.dataDirectoryReadable,
            QStringLiteral("data")),
        QStringLiteral("Debug / Release 配置已同步，检测路径：%1")
            .arg(QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/data")));
}

void WelcomePageNew::updateResponsiveLayout()
{
    ui->heroSplitLayout->setDirection(width() < 1040 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);

    while (ui->statusCardsGridLayout->count() > 0) {
        QLayoutItem* item = ui->statusCardsGridLayout->takeAt(0);
        delete item;
    }

    if (width() < 900) {
        ui->statusCardsGridLayout->addWidget(ui->pluginFrameworkCard, 0, 0);
        ui->statusCardsGridLayout->addWidget(ui->serviceStatusCard, 1, 0);
        ui->statusCardsGridLayout->addWidget(ui->dataDirectoryCard, 2, 0);
        return;
    }

    if (width() < 1280) {
        ui->statusCardsGridLayout->addWidget(ui->pluginFrameworkCard, 0, 0);
        ui->statusCardsGridLayout->addWidget(ui->serviceStatusCard, 0, 1);
        ui->statusCardsGridLayout->addWidget(ui->dataDirectoryCard, 1, 0, 1, 2);
        return;
    }

    ui->statusCardsGridLayout->addWidget(ui->pluginFrameworkCard, 0, 0);
    ui->statusCardsGridLayout->addWidget(ui->serviceStatusCard, 0, 1);
    ui->statusCardsGridLayout->addWidget(ui->dataDirectoryCard, 0, 2);
}

void WelcomePageNew::resizeEvent(QResizeEvent* event)
{
    BasePage::resizeEvent(event);
    updateResponsiveLayout();
}
```

- [ ] **Step 3: 重新编译主程序，先确认 WelcomePage 新对象名和 cpp 调用完全一致**

Run: `cmake --build --preset x64-release`

Expected: `NewPagesLib` 和 `medicalpro` 构建成功，不出现 `Ui::WelcomePage` 缺失成员或布局对象名错误

- [ ] **Step 4: 手动验证欢迎页主链与响应式行为**

Run: `build_x64/Release/medicalpro.exe`

Expected:
- 程序第一页仍然是 WelcomePage
- `进入系统` 进入 `ModuleSelectionPage`
- 不需要登录，也不跳过欢迎页
- 首屏左侧是品牌欢迎区，右侧是运行摘要区
- 下方 3 张卡存在且会在页面激活时刷新真实状态
- 把窗口缩窄后，上方双区能切到上下布局，下方卡片能切换为 `2 + 1` 或单列

- [ ] **Step 5: 只提交 WelcomePage 逻辑重构**

```bash
git add UI/NewPages/WelcomePage.h UI/NewPages/WelcomePage.cpp
git commit -m "feat: bind runtime summary for welcome page phase2"
```

### Task 4: 用 Welcome 专属主题令牌和 QSS 收口视觉层

**Files:**
- Modify: `UI/AppTheme.h`
- Modify: `UI/styles/three_pages_theme.qss`

- [ ] **Step 1: 给主题令牌补齐 WelcomePage Phase 2 需要的材质和强调色**

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
        { "${SURFACE_ELEVATED}", "rgba(10, 22, 34, 0.94)" },
        { "${SURFACE_PANEL}", "rgba(7, 16, 27, 0.72)" },
        { "${BORDER_SOFT}", "rgba(124, 160, 191, 0.18)" },
        { "${BORDER_STRONG}", "rgba(124, 160, 191, 0.32)" },
        { "${TEXT_PRIMARY}", "#f4f8fc" },
        { "${TEXT_SECONDARY}", "#b4c4d6" },
        { "${TEXT_MUTED}", "#7f92a8" },
        { "${ACCENT_PRIMARY}", "#ff7a59" },
        { "${ACCENT_PRIMARY_HOVER}", "#ff936f" },
        { "${ACCENT_SECONDARY}", "#5bb6ff" },
        { "${WELCOME_PRIMARY}", "#38C6D9" },
        { "${WELCOME_AMBER}", "#F3A53B" },
        { "${SUCCESS}", "#3ddc97" },
        { "${WARNING}", "#ffbf69" },
        { "${DANGER}", "#ff6b6b" },
        { "${SHADOW}", "rgba(0, 0, 0, 0.24)" }
    };
}
```

- [ ] **Step 2: 为 WelcomePage 新对象名补齐品牌区、摘要区、速览块、结论标签和状态卡样式**

```qss
/* UI/styles/three_pages_theme.qss */
QWidget#WelcomePage QFrame#brandPanelFrame,
QWidget#WelcomePage QFrame#runtimeSummaryFrame {
    background-color: ${SURFACE_ELEVATED};
    border: 1px solid ${BORDER_SOFT};
    border-radius: 22px;
}

QWidget#WelcomePage QFrame#brandPanelFrame {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 1, y2: 1,
        stop: 0 rgba(12, 28, 43, 0.96),
        stop: 1 rgba(16, 39, 56, 0.92)
    );
    border-color: rgba(56, 198, 217, 0.18);
}

QWidget#WelcomePage QLabel#productEyebrowLabel {
    color: ${WELCOME_PRIMARY};
    font-size: 13px;
    font-weight: 700;
    letter-spacing: 1px;
}

QWidget#WelcomePage QLabel#heroTitleLabel {
    color: ${TEXT_PRIMARY};
    font-size: 36px;
    font-weight: 700;
}

QWidget#WelcomePage QLabel#versionLabel {
    color: ${WELCOME_AMBER};
    font-size: 13px;
}

QWidget#WelcomePage QLabel#heroSubtitleLabel,
QWidget#WelcomePage QLabel#runtimeSummaryTextLabel {
    color: ${TEXT_SECONDARY};
    font-size: 15px;
}

QWidget#WelcomePage QLabel#runtimeSummaryTitleLabel {
    color: ${TEXT_PRIMARY};
    font-size: 20px;
    font-weight: 700;
}

QWidget#WelcomePage QFrame#frameworkQuickStatFrame,
QWidget#WelcomePage QFrame#serviceQuickStatFrame,
QWidget#WelcomePage QFrame#directoryQuickStatFrame {
    background-color: ${SURFACE_PANEL};
    border: 1px solid ${BORDER_SOFT};
    border-radius: 14px;
}

QWidget#WelcomePage QLabel#frameworkQuickTitleLabel,
QWidget#WelcomePage QLabel#serviceQuickTitleLabel,
QWidget#WelcomePage QLabel#directoryQuickTitleLabel {
    color: ${TEXT_MUTED};
    font-size: 13px;
    font-weight: 600;
}

QWidget#WelcomePage QLabel#frameworkQuickValueLabel,
QWidget#WelcomePage QLabel#serviceQuickValueLabel,
QWidget#WelcomePage QLabel#directoryQuickValueLabel {
    color: ${TEXT_PRIMARY};
    font-size: 18px;
    font-weight: 700;
}

QWidget#WelcomePage QLabel#runtimeDecisionBadge {
    min-height: 34px;
    padding: 0 14px;
    border-radius: 17px;
    font-size: 14px;
    font-weight: 700;
}

QWidget#WelcomePage QLabel#runtimeDecisionBadge[statusTone="ok"] {
    background-color: rgba(61, 220, 151, 0.16);
    color: ${SUCCESS};
}

QWidget#WelcomePage QLabel#runtimeDecisionBadge[statusTone="warning"] {
    background-color: rgba(255, 191, 105, 0.16);
    color: ${WARNING};
}

QWidget#WelcomePage QLabel#runtimeDecisionBadge[statusTone="danger"] {
    background-color: rgba(255, 107, 107, 0.16);
    color: ${DANGER};
}

QWidget#WelcomePage QFrame#pluginFrameworkCard,
QWidget#WelcomePage QFrame#serviceStatusCard,
QWidget#WelcomePage QFrame#dataDirectoryCard {
    background-color: ${SURFACE_SOFT};
    border: 1px solid ${BORDER_SOFT};
    border-radius: 18px;
}

QWidget#WelcomePage QPushButton#enterButton {
    min-height: 48px;
    min-width: 160px;
    background-color: ${WELCOME_PRIMARY};
    border-color: ${WELCOME_PRIMARY};
    color: #081421;
}

QWidget#WelcomePage QPushButton#enterButton:hover {
    background-color: #54d4e4;
    border-color: #54d4e4;
}

QWidget#WelcomePage QPushButton#exitButton {
    min-height: 44px;
    min-width: 112px;
    background-color: transparent;
    border-color: ${BORDER_STRONG};
    color: ${TEXT_SECONDARY};
}
```

- [ ] **Step 3: 构建主程序，确认新增 token 和样式选择器不会破坏现有主题**

Run: `cmake --build --preset x64-release`

Expected: `medicalpro` 构建成功，`AppTheme` 仍能加载 `three_pages_theme.qss`

- [ ] **Step 4: 手动检查 WelcomePage 视觉层是否收口到 spec 方向**

Run: `build_x64/Release/medicalpro.exe`

Expected:
- 页面整体是深蓝灰工作站底色
- 左侧品牌区与右侧摘要区是两块清晰面板
- `进入系统` 按钮明显高于 `退出`
- 右侧结论标签能随状态切换颜色
- 下方三张卡和上方摘要区视觉语言一致

- [ ] **Step 5: 只提交 WelcomePage 视觉层**

```bash
git add UI/AppTheme.h UI/styles/three_pages_theme.qss
git commit -m "feat: style welcome page phase2 visual hierarchy"
```

### Task 5: 统一验证并回写项目现状文档

**Files:**
- Modify: `docs/current_status_and_project_overview.md`

- [ ] **Step 1: 跑完整构建和单元测试，确认 WelcomePage 改造没有打断主链**

Run: `cmake --build --preset x64-release`

Expected: `build_x64/Release/medicalpro.exe` 时间戳更新，`Framework`、`NewPagesLib`、`medicalpro` 继续通过

Run: `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`

Expected: `three_page_presentation_utils_test` 继续通过

- [ ] **Step 2: 手动走完整欢迎页主链**

Run: `build_x64/Release/medicalpro.exe`

Expected:
- 欢迎页作为第一屏显示
- 不出现登录强制页
- `进入系统` 进入 `ModuleSelectionPage`
- 控制台日志仍然存在
- WelcomePage 运行摘要、结论标签和 3 张状态卡都能看到

- [ ] **Step 3: 把 WelcomePage Phase 2 完成情况写回项目总览文档**

```md
## 1.6 UI Phase 2 当前进展
- `ThreePagePresentationUtils` 已补齐 WelcomePage 运行摘要与可进入性结论文案，并有单元测试覆盖。
- WelcomePage 已升级为医疗旗舰工作站入口页，采用左品牌区 / 右运行摘要区 / 下方三状态卡结构。
- WelcomePage 保留欢迎页入口，不要求登录，主按钮继续进入 `ModuleSelectionPage`。
- WelcomePage 右侧摘要区已接入 3 条核心速览与状态结论标签。
- WelcomePage 下方 3 张状态卡继续承接真实运行状态：插件框架、关键服务、`data` 目录。
- WelcomePage 已补齐窄窗口下的上下布局与状态卡重排能力。
```

- [ ] **Step 4: 检查文档和产物时间戳，确认交付闭环**

Run: `rg -n "WelcomePage 已升级为医疗旗舰工作站入口页|右侧摘要区已接入 3 条核心速览|状态卡重排能力" docs/current_status_and_project_overview.md`

Expected: 能在文档中检索到 WelcomePage Phase 2 完成记录

Run: `Get-Item build_x64/Release/medicalpro.exe | Select-Object FullName, LastWriteTime`

Expected: 输出最新 Release 主程序时间戳

- [ ] **Step 5: 只提交验证与文档更新**

```bash
git add docs/current_status_and_project_overview.md
git commit -m "docs: update current status after welcome page phase2"
```

## 2026-04-15 后续推进记录

- WelcomePage 在原 Phase 2 结构基础上，继续完成了一轮“验收向视觉微调”代码落地。
- 本轮继续收口的重点为：
  - 左品牌区 / 右摘要区的主次分离
  - `进入系统` 与 `退出` 的按钮强弱
  - 窄窗口下右侧摘要区宽度与下方状态卡压屏感
- 对应代码已落在：
  - `UI/styles/three_pages_theme.qss`
  - `UI/NewPages/WelcomePage.cpp`
- 2026-04-15 已再次完成：
  - `cmake --build build_x64 --config Release --target medicalpro`
  - `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`
  - `build_x64/Release/medicalpro.exe` 8 秒稳定启动检查

## 2026-04-16 联动微调记录

- 在四页联合视觉微调第二轮中，WelcomePage 继续承担“首屏聚焦入口”的角色。
- 本轮 WelcomePage 继续压缩：
  - 左侧品牌区内部的纵向弹性空白
  - 右侧运行摘要区宽度与速览块厚度
  - 底部三张状态卡的边距与卡片高度感
- 对应改动已落在：
  - `UI/Forms/WelcomePage.ui`
  - `UI/NewPages/WelcomePage.cpp`
  - `UI/styles/three_pages_theme.qss`
- 2026-04-16 已再次完成：
  - `cmake --build build_x64 --config Release --target medicalpro`
  - `ctest --test-dir build_x64 -C Release -R three_page_presentation_utils_test --output-on-failure`
  - `build_x64/Release/medicalpro.exe` 8 秒稳定启动检查
