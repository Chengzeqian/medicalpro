#include "WelcomePage.h"
#include "ui_WelcomePage.h"

#include "Framework/StartupOrchestrator.h"
#include "ThreePagePresentationUtils.h"
#include "WelcomeBrandingUtils.h"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QPixmap>
#include <QResizeEvent>
#include <QStyle>
#include <QTimer>
#include <QtGlobal>
#include <utility>

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Framework/CTKManager.h"
#endif

WelcomePageNew::WelcomePageNew(QWidget* parent, RuntimeStatusProvider runtimeStatusProvider)
    : BasePage(parent)
    , ui(new Ui::WelcomePage)
    , m_runtimeStatusProvider(std::move(runtimeStatusProvider))
    , m_runtimeStatusRefreshTimer(nullptr)
{
    ui->setupUi(this);
    setObjectName("WelcomePage");
    m_runtimeStatusRefreshTimer = new QTimer(this);
    setupUI();
}

WelcomePageNew::~WelcomePageNew()
{
    delete ui;
}

void WelcomePageNew::setupUI()
{
    m_runtimeStatusRefreshTimer->setSingleShot(true);
    m_runtimeStatusRefreshTimer->setInterval(120);
    connect(m_runtimeStatusRefreshTimer, &QTimer::timeout, this, &WelcomePageNew::refreshRuntimeStatus);

#ifdef CTK_PLUGIN_FRAMEWORK
    auto* ctkManager = CTKManager::instance();
    connect(ctkManager, &CTKManager::frameworkInitialized, this, &WelcomePageNew::scheduleRuntimeStatusRefresh);
    connect(ctkManager, &CTKManager::frameworkStarted, this, &WelcomePageNew::scheduleRuntimeStatusRefresh);
    connect(ctkManager, &CTKManager::frameworkStopped, this, &WelcomePageNew::scheduleRuntimeStatusRefresh);
    connect(ctkManager, &CTKManager::pluginLoaded, this, &WelcomePageNew::scheduleRuntimeStatusRefresh);
    connect(ctkManager, &CTKManager::pluginLoadFailed, this, &WelcomePageNew::scheduleRuntimeStatusRefresh);
    connect(ctkManager, &CTKManager::errorOccurred, this, &WelcomePageNew::scheduleRuntimeStatusRefresh);
#endif

    auto* orchestrator = StartupOrchestrator::instance();
    connect(orchestrator, &StartupOrchestrator::phaseCompleted, this, [this](const QString&, bool) {
        scheduleRuntimeStatusRefresh();
    });
    connect(orchestrator, &StartupOrchestrator::startupCompleted, this, [this](bool) {
        scheduleRuntimeStatusRefresh();
    });
    connect(orchestrator, &StartupOrchestrator::diagnosticReportUpdated, this, [this](const QString&) {
        scheduleRuntimeStatusRefresh();
    });

    applyBranding();
    ui->mainLayout->setStretch(0, 1);
    ui->mainLayout->setStretch(1, 0);

    ui->productEyebrowLabel->setText(QStringLiteral("MEDICALPRO SURGICAL WORKSTATION"));
    ui->heroTitleLabel->setText(QStringLiteral("智能手术导航工作站"));
    ui->heroSubtitleLabel->setText(QStringLiteral("统一承接自检、数据管理与导航准备的临床入口。"));
    ui->runtimeSummaryTitleLabel->setText(QStringLiteral("当前运行状态"));
    ui->runtimeSummaryTextLabel->setText(QStringLiteral("进入前确认插件框架、关键服务与数据目录是否可用。"));
    ui->versionLabel->setText(QStringLiteral("版本 1.0.0"));
    ui->frameworkQuickTitleLabel->setText(QStringLiteral("插件框架"));
    ui->serviceQuickTitleLabel->setText(QStringLiteral("关键服务"));
    ui->directoryQuickTitleLabel->setText(QStringLiteral("数据目录"));
    ui->enterButton->setDefault(true);
    ui->enterButton->setAutoDefault(true);
    applyQuickStat(
        ui->frameworkQuickValueLabel,
        ui->frameworkQuickToneLabel,
        QStringLiteral("已识别 0 个插件"),
        QStringLiteral("检查中"),
        QStringLiteral("warning"));
    applyQuickStat(
        ui->serviceQuickValueLabel,
        ui->serviceQuickToneLabel,
        QStringLiteral("0/3 已就绪"),
        QStringLiteral("检查中"),
        QStringLiteral("warning"));
    applyQuickStat(
        ui->directoryQuickValueLabel,
        ui->directoryQuickToneLabel,
        QStringLiteral("data 需检查"),
        QStringLiteral("检查中"),
        QStringLiteral("warning"));
    ui->runtimeDecisionBadge->setText(QStringLiteral("启动自检中"));
    ui->runtimeDecisionBadge->setProperty("statusTone", QStringLiteral("warning"));
    ui->runtimeDecisionBadge->style()->unpolish(ui->runtimeDecisionBadge);
    ui->runtimeDecisionBadge->style()->polish(ui->runtimeDecisionBadge);
    ui->runtimeDecisionBadge->update();
    updateResponsiveLayout();
    scheduleRuntimeStatusRefresh();
}

void WelcomePageNew::applyBranding()
{
    const QString logoPath = WelcomeBrandingUtils::resolveLogoPath(QCoreApplication::applicationDirPath());
    const QPixmap logo(logoPath);

    if (!logoPath.isEmpty() && !logo.isNull()) {
        ui->logoLabel->setText(QString());
        ui->logoLabel->setPixmap(logo.scaled(352, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->logoLabel->setProperty("brandMode", QStringLiteral("image"));
    } else {
        ui->logoLabel->setPixmap(QPixmap());
        ui->logoLabel->setText(WelcomeBrandingUtils::buildFallbackBrandText());
        ui->logoLabel->setProperty("brandMode", QStringLiteral("text"));
    }

    ui->logoLabel->style()->unpolish(ui->logoLabel);
    ui->logoLabel->style()->polish(ui->logoLabel);
    ui->logoLabel->update();
}

void WelcomePageNew::onActivated()
{
    BasePage::onActivated();
    refreshRuntimeStatus();
}

WelcomePageNew::RuntimeStatusSnapshot WelcomePageNew::collectRuntimeStatus() const
{
    if (m_runtimeStatusProvider) {
        return m_runtimeStatusProvider();
    }

    const QStringList requiredServices = {
        QStringLiteral("UserManagementService"),
        QStringLiteral("DicomViewerService"),
        QStringLiteral("FourViewDisplayService")
    };

#ifdef CTK_PLUGIN_FRAMEWORK
    auto* ctkManager = CTKManager::instance();
    const bool frameworkReady = ctkManager && ctkManager->isCTKAvailable();
    const int pluginCount = ctkManager
        ? qMax(ctkManager->getInstalledPlugins().size(), ctkManager->getStartedPlugins().size())
        : 0;
    const QStringList missingServices = ctkManager ? ctkManager->getMissingServices(requiredServices) : requiredServices;
#else
    const bool frameworkReady = false;
    const int pluginCount = 0;
    const QStringList missingServices = requiredServices;
#endif

    int readyServices = requiredServices.size() - missingServices.size();
    if (readyServices < 0) {
        readyServices = 0;
    }

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
    valueLabel->setProperty("statusTone", QString());
    valueLabel->style()->unpolish(valueLabel);
    valueLabel->style()->polish(valueLabel);
    valueLabel->update();

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
    ui->runtimeDecisionBadge->update();

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

void WelcomePageNew::scheduleRuntimeStatusRefresh()
{
    if (!m_runtimeStatusRefreshTimer) {
        return;
    }

    m_runtimeStatusRefreshTimer->start();
}

void WelcomePageNew::updateResponsiveLayout()
{
    const bool stackedHero = width() < 1100;
    ui->heroSplitLayout->setDirection(stackedHero ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    ui->heroSplitLayout->setStretch(0, stackedHero ? 0 : 10);
    ui->heroSplitLayout->setStretch(1, stackedHero ? 0 : 2);
    ui->welcomeHeroFrame->setMinimumHeight(stackedHero ? 0 : 458);
    ui->brandPanelFrame->setMinimumHeight(stackedHero ? 0 : 404);
    ui->runtimeSummaryFrame->setMaximumWidth(stackedHero ? QWIDGETSIZE_MAX : 428);
    ui->enterButton->setMaximumWidth(stackedHero ? QWIDGETSIZE_MAX : 356);
    ui->exitButton->setMaximumWidth(stackedHero ? QWIDGETSIZE_MAX : 144);

    while (ui->statusCardsGridLayout->count() > 0) {
        delete ui->statusCardsGridLayout->takeAt(0);
    }

    if (width() < 900) {
        ui->statusCardsFrame->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->statusCardsGridLayout->addWidget(ui->pluginFrameworkCard, 0, 0);
        ui->statusCardsGridLayout->addWidget(ui->serviceStatusCard, 1, 0);
        ui->statusCardsGridLayout->addWidget(ui->dataDirectoryCard, 2, 0);
        return;
    }

    if (width() < 1280) {
        ui->statusCardsFrame->setMaximumHeight(260);
        ui->statusCardsGridLayout->addWidget(ui->pluginFrameworkCard, 0, 0);
        ui->statusCardsGridLayout->addWidget(ui->serviceStatusCard, 0, 1);
        ui->statusCardsGridLayout->addWidget(ui->dataDirectoryCard, 1, 0, 1, 2);
        return;
    }

    ui->statusCardsFrame->setMaximumHeight(188);
    ui->statusCardsGridLayout->addWidget(ui->pluginFrameworkCard, 0, 0);
    ui->statusCardsGridLayout->addWidget(ui->serviceStatusCard, 0, 1);
    ui->statusCardsGridLayout->addWidget(ui->dataDirectoryCard, 0, 2);
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

void WelcomePageNew::resizeEvent(QResizeEvent* event)
{
    BasePage::resizeEvent(event);
    updateResponsiveLayout();
}

void WelcomePageNew::on_enterButton_clicked()
{
    emit enterSystemRequested();
    emit navigateTo(toInt(PageIndex::ModuleSelection));
}

void WelcomePageNew::on_exitButton_clicked()
{
    emit exitRequested();
}
