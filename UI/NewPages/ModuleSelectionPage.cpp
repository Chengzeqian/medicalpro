#include "ModuleSelectionPage.h"
#include "ui_ModuleSelectionPage.h"

#include "ThreePagePresentationUtils.h"

#include <QDateTime>
#include <QLabel>
#include <QStyle>
#include <QTimer>
#include <QWidget>
#include <utility>

namespace
{
const QStringList kRequiredModuleServices = {
    QStringLiteral("UserManagementService"),
    QStringLiteral("DicomViewerService"),
    QStringLiteral("FourViewDisplayService")
};
}

ModuleSelectionPageNew::ModuleSelectionPageNew(QWidget* parent, RuntimeStatusProvider runtimeStatusProvider)
    : BasePage(parent)
    , ui(new Ui::ModuleSelectionPage)
    , m_runtimeStatusProvider(std::move(runtimeStatusProvider))
    , m_clockTimer(new QTimer(this))
{
    ui->setupUi(this);
    setObjectName("ModuleSelectionPage");

    ui->moduleEyebrowLabel->setText(QStringLiteral("SURGICAL WORKFLOW HALL"));
    ui->titleLabel->setText(QStringLiteral("选择当前流程入口"));
    ui->subtitleLabel->setText(QStringLiteral("优先进入数据管理主链；如需排查环境，再使用系统设置入口。"));
    ui->ankleSurgeryDesc->setText(QStringLiteral("继续进入数据管理主链，先核对病例、任务与导航准备。"));
    ui->systemSettingsDesc->setText(QStringLiteral("用于复核运行配置与服务状态，作为主流程外的辅助入口。"));
    ui->ankleSurgeryButton->setText(QStringLiteral("继续进入数据管理"));
    ui->systemSettingsButton->setText(QStringLiteral("查看系统设置"));
    ui->ankleSurgeryButton->setDefault(true);
    ui->ankleSurgeryButton->setAutoDefault(true);

    m_clockTimer->setInterval(15000);
    connect(m_clockTimer, &QTimer::timeout, this, &ModuleSelectionPageNew::refreshClock);
    m_clockTimer->start();

    refreshClock();
    refreshHeaderState();
    refreshModuleCards();
}

ModuleSelectionPageNew::~ModuleSelectionPageNew()
{
    delete ui;
}

void ModuleSelectionPageNew::onActivated()
{
    BasePage::onActivated();
    refreshClock();
    refreshHeaderState();
    refreshModuleCards();
}

void ModuleSelectionPageNew::setCurrentUser(const QString& username)
{
    m_currentUser = username;
    refreshHeaderState();
}

void ModuleSelectionPageNew::on_ankleSurgeryButton_clicked()
{
    emit ankleSurgeryRequested();
    emit navigateTo(toInt(PageIndex::Management));
}

void ModuleSelectionPageNew::on_systemSettingsButton_clicked()
{
    emit systemSettingsRequested();
    emit navigateTo(toInt(PageIndex::SystemSettings));
}

void ModuleSelectionPageNew::on_logoutButton_clicked()
{
    if (showConfirm(QStringLiteral("返回欢迎页"), QStringLiteral("确定返回欢迎页吗？"))) {
        m_currentUser.clear();
        emit logoutRequested();
        emit navigateTo(toInt(PageIndex::Welcome));
    }
}

void ModuleSelectionPageNew::refreshClock()
{
    ui->timeValueLabel->setText(ThreePagePresentationUtils::formatModuleTimestamp(QDateTime::currentDateTime()));
}

ModuleSelectionPageNew::ModuleRuntimeStatus ModuleSelectionPageNew::collectRuntimeStatus() const
{
    if (m_runtimeStatusProvider) {
        return m_runtimeStatusProvider();
    }

    ModuleRuntimeStatus status;
    status.totalServices = kRequiredModuleServices.size();
    status.missingServices = kRequiredModuleServices;
    status.workflowReady = status.frameworkReady && status.readyServices == status.totalServices;
    return status;
}

void ModuleSelectionPageNew::refreshHeaderState()
{
    const QString displayUser = m_currentUser.isEmpty() || m_currentUser == QStringLiteral("访客")
        ? QStringLiteral("访客模式")
        : QStringLiteral("访客 / %1").arg(m_currentUser);
    ui->userInfoLabel->setText(displayUser);

    const ModuleRuntimeStatus status = collectRuntimeStatus();
    const QString tone = status.workflowReady
        ? QStringLiteral("ok")
        : ThreePagePresentationUtils::buildToneName(status.readyServices, status.totalServices);
    ui->systemValueLabel->setText(ThreePagePresentationUtils::buildModuleStatusSummary(status.workflowReady));
    ui->systemValueLabel->setProperty("statusTone", tone);
    polishWidget(ui->systemValueLabel);
}

void ModuleSelectionPageNew::refreshModuleCards()
{
    const ModuleRuntimeStatus status = collectRuntimeStatus();
    const QString workflowTone = status.workflowReady
        ? QStringLiteral("ok")
        : ThreePagePresentationUtils::buildToneName(status.readyServices, status.totalServices);
    const QString workflowLabel = status.workflowReady
        ? QStringLiteral("主链就绪")
        : (status.readyServices > 0 ? QStringLiteral("待确认") : QStringLiteral("需排查"));

    applyStatusTag(ui->ankleSurgeryStateTag, workflowLabel, workflowTone);
    ui->ankleSurgeryHintLabel->setText(
        ThreePagePresentationUtils::buildModuleAccessHint(status.workflowReady, status.missingServices));

    const QString settingsTone = status.frameworkReady ? QStringLiteral("ok") : QStringLiteral("warning");
    applyStatusTag(
        ui->systemSettingsStateTag,
        status.frameworkReady ? QStringLiteral("辅助入口") : QStringLiteral("建议检查"),
        settingsTone);
    ui->systemSettingsHintLabel->setText(buildSettingsHint(status));
}

void ModuleSelectionPageNew::applyStatusTag(QLabel* label, const QString& text, const QString& tone)
{
    label->setText(text);
    label->setProperty("statusTone", tone);
    polishWidget(label);
}

QString ModuleSelectionPageNew::buildSettingsHint(const ModuleRuntimeStatus& status) const
{
    if (status.frameworkReady) {
        return QStringLiteral("如需复核插件、目录或运行参数，可进入系统设置查看。");
    }
    if (status.readyServices > 0) {
        return QStringLiteral("主流程尚未完全就绪，建议先在系统设置里确认环境状态。");
    }
    return QStringLiteral("当前更适合先检查系统设置，确认框架与关键依赖是否可用。");
}

void ModuleSelectionPageNew::polishWidget(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
