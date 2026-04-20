#include "SystemSettingsPage.h"
#include "ui_SystemSettingsPage.h"

#include "ThreePagePresentationUtils.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QStyle>
#include <utility>

namespace
{
const QStringList kRequiredSystemSettingsServices = {
    QStringLiteral("UserManagementService"),
    QStringLiteral("DicomViewerService"),
    QStringLiteral("FourViewDisplayService")
};
}

SystemSettingsPageNew::SystemSettingsPageNew(QWidget* parent, RuntimeStatusProvider runtimeStatusProvider)
    : BasePage(parent)
    , ui(new Ui::SystemSettingsPage)
    , m_runtimeStatusProvider(std::move(runtimeStatusProvider))
{
    ui->setupUi(this);
    setObjectName("SystemSettingsPage");
    ui->scrollArea->viewport()->setObjectName(QStringLiteral("systemSettingsScrollViewport"));
    ui->scrollArea->viewport()->setAutoFillBackground(false);
    ui->systemRecommendationBadge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->systemRecommendationLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    setupPageCopy();

    auto* diagnosticsButton = new QPushButton(QStringLiteral("运行诊断"), ui->systemSettingsHeaderFrame);
    diagnosticsButton->setObjectName(QStringLiteral("diagnosticsButton"));
    diagnosticsButton->setCursor(Qt::PointingHandCursor);
    diagnosticsButton->setMinimumSize(176, 52);
    ui->headerLayout->insertWidget(2, diagnosticsButton);
    connect(diagnosticsButton, &QPushButton::clicked, this, [this]() {
        emit diagnosticsRequested();
        emit navigateTo(toInt(PageIndex::Diagnostics));
    });

    loadSettings();
    refreshRuntimeStatus();
}

SystemSettingsPageNew::~SystemSettingsPageNew()
{
    delete ui;
}

void SystemSettingsPageNew::onActivated()
{
    BasePage::onActivated();
    loadSettings();
    refreshRuntimeStatus();
}

void SystemSettingsPageNew::on_backButton_clicked()
{
    emit backRequested();
    emit navigateTo(toInt(PageIndex::ModuleSelection));
}

void SystemSettingsPageNew::on_saveButton_clicked()
{
    saveSettings();
    refreshRuntimeStatus();
    showInfo(QStringLiteral("设置"), QStringLiteral("设置保存成功！"));
}

void SystemSettingsPageNew::on_browseDataPathButton_clicked()
{
    const QString path = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择数据目录"),
        ui->dataPathEdit->text());
    if (!path.isEmpty()) {
        ui->dataPathEdit->setText(path);
        refreshRuntimeStatus();
    }
}

void SystemSettingsPageNew::on_browseDicomPathButton_clicked()
{
    const QString path = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择 DICOM 目录"),
        ui->dicomPathEdit->text());
    if (!path.isEmpty()) {
        ui->dicomPathEdit->setText(path);
        refreshRuntimeStatus();
    }
}

void SystemSettingsPageNew::setupPageCopy()
{
    setWindowTitle(QStringLiteral("系统设置"));
    ui->systemSettingsEyebrowLabel->setText(QStringLiteral("SYSTEM HEALTH & CONFIG"));
    ui->titleLabel->setText(QStringLiteral("系统设置与环境检查"));
    ui->subtitleLabel->setText(QStringLiteral("先确认插件、服务与路径状态，再调整当前运行配置。"));

    ui->frameworkStatusTitleLabel->setText(QStringLiteral("插件框架"));
    ui->serviceStatusTitleLabel->setText(QStringLiteral("关键服务"));
    ui->pathStatusTitleLabel->setText(QStringLiteral("数据与影像路径"));

    ui->generalSettingsTitleLabel->setText(QStringLiteral("常规设置"));
    ui->generalSettingsDescLabel->setText(QStringLiteral("调整语言、主题与自动保存策略。"));
    ui->deviceSettingsTitleLabel->setText(QStringLiteral("设备连接"));
    ui->deviceSettingsDescLabel->setText(QStringLiteral("确认当前追踪器串口与通信参数。"));
    ui->pathSettingsTitleLabel->setText(QStringLiteral("路径配置"));
    ui->pathSettingsDescLabel->setText(QStringLiteral("核对数据目录与 DICOM 目录是否可访问。"));

    ui->languageLabel->setText(QStringLiteral("语言"));
    ui->themeLabel->setText(QStringLiteral("主题"));
    ui->autoSaveLabel->setText(QStringLiteral("自动保存"));
    ui->autoSaveCheck->setText(QStringLiteral("启用自动保存"));
    ui->trackerPortLabel->setText(QStringLiteral("追踪器端口"));
    ui->baudRateLabel->setText(QStringLiteral("波特率"));
    ui->dataPathLabel->setText(QStringLiteral("数据路径"));
    ui->dicomPathLabel->setText(QStringLiteral("DICOM 路径"));

    ui->languageCombo->setItemText(0, QStringLiteral("English"));
    ui->languageCombo->setItemText(1, QStringLiteral("中文"));
    ui->themeCombo->setItemText(0, QStringLiteral("深色"));
    ui->themeCombo->setItemText(1, QStringLiteral("浅色"));

    ui->backButton->setText(QStringLiteral("返回模块页"));
    ui->saveButton->setText(QStringLiteral("保存设置"));
    ui->saveButton->setDefault(true);
    ui->saveButton->setAutoDefault(true);
    ui->browseDataPathButton->setText(QStringLiteral("浏览"));
    ui->browseDicomPathButton->setText(QStringLiteral("浏览"));
}

void SystemSettingsPageNew::loadSettings()
{
    QSettings settings("MedicalPro", "NavigationSystem");

    ui->languageCombo->setCurrentIndex(settings.value("general/language", 0).toInt());
    ui->themeCombo->setCurrentIndex(settings.value("general/theme", 0).toInt());
    ui->autoSaveCheck->setChecked(settings.value("general/autoSave", true).toBool());

    ui->trackerPortCombo->setCurrentText(settings.value("device/trackerPort", "COM1").toString());
    ui->baudRateCombo->setCurrentText(settings.value("device/baudRate", "115200").toString());

    ui->dataPathEdit->setText(settings.value("paths/dataPath", "./data").toString());
    ui->dicomPathEdit->setText(settings.value("paths/dicomPath", "./dicom").toString());
}

void SystemSettingsPageNew::saveSettings()
{
    QSettings settings("MedicalPro", "NavigationSystem");

    settings.setValue("general/language", ui->languageCombo->currentIndex());
    settings.setValue("general/theme", ui->themeCombo->currentIndex());
    settings.setValue("general/autoSave", ui->autoSaveCheck->isChecked());

    settings.setValue("device/trackerPort", ui->trackerPortCombo->currentText());
    settings.setValue("device/baudRate", ui->baudRateCombo->currentText());

    settings.setValue("paths/dataPath", ui->dataPathEdit->text());
    settings.setValue("paths/dicomPath", ui->dicomPathEdit->text());

    settings.sync();
}

SystemSettingsPageNew::RuntimeStatusSnapshot SystemSettingsPageNew::collectRuntimeStatus() const
{
    RuntimeStatusSnapshot snapshot;
    if (m_runtimeStatusProvider) {
        snapshot = m_runtimeStatusProvider();
    }

    if (snapshot.totalServices <= 0) snapshot.totalServices = kRequiredSystemSettingsServices.size();
    if (snapshot.readyServices < 0) snapshot.readyServices = 0;
    if (snapshot.readyServices > snapshot.totalServices) snapshot.readyServices = snapshot.totalServices;

    const QDir dataDir(ui->dataPathEdit->text());
    const QFileInfo dataInfo(dataDir.absolutePath());
    const bool dataReadable = dataDir.exists() && dataInfo.isReadable();

    const QDir dicomDir(ui->dicomPathEdit->text());
    const QFileInfo dicomInfo(dicomDir.absolutePath());
    const bool dicomReadable = dicomDir.exists() && dicomInfo.isReadable();

    snapshot.dataDirectoryReadable = dataReadable;
    snapshot.dicomDirectoryReadable = dicomReadable;
    return snapshot;
}

void SystemSettingsPageNew::refreshRuntimeStatus()
{
    const RuntimeStatusSnapshot snapshot = collectRuntimeStatus();
    const QString frameworkTone = snapshot.frameworkReady ? QStringLiteral("ok") : QStringLiteral("danger");
    const QString serviceTone = ThreePagePresentationUtils::buildToneName(snapshot.readyServices, snapshot.totalServices);
    const QString pathTone = (snapshot.dataDirectoryReadable && snapshot.dicomDirectoryReadable)
        ? QStringLiteral("ok")
        : ((snapshot.dataDirectoryReadable || snapshot.dicomDirectoryReadable) ? QStringLiteral("warning") : QStringLiteral("danger"));

    ui->frameworkStatusCard->setProperty("statusTone", frameworkTone);
    ui->serviceStatusCard->setProperty("statusTone", serviceTone);
    ui->pathStatusCard->setProperty("statusTone", pathTone);
    polishWidget(ui->frameworkStatusCard);
    polishWidget(ui->serviceStatusCard);
    polishWidget(ui->pathStatusCard);

    applyStatusCard(
        ui->frameworkStatusBadgeLabel,
        ui->frameworkStatusSummaryLabel,
        ui->frameworkStatusDetailLabel,
        frameworkTone,
        snapshot.frameworkReady ? QStringLiteral("已联通") : QStringLiteral("待检查"),
        ThreePagePresentationUtils::buildFrameworkSummary(snapshot.frameworkReady, snapshot.pluginCount),
        snapshot.frameworkReady
            ? QStringLiteral("插件框架已完成初始化，可继续检查服务与路径。")
            : QStringLiteral("请先确认 CTK 初始化和插件加载状态。"));

    applyStatusCard(
        ui->serviceStatusBadgeLabel,
        ui->serviceStatusSummaryLabel,
        ui->serviceStatusDetailLabel,
        serviceTone,
        serviceTone == QStringLiteral("ok")
            ? QStringLiteral("正常")
            : (serviceTone == QStringLiteral("warning") ? QStringLiteral("待确认") : QStringLiteral("异常")),
        ThreePagePresentationUtils::buildServiceGroupSummary(snapshot.readyServices, snapshot.totalServices),
        snapshot.readyServices == snapshot.totalServices
            ? QStringLiteral("关键服务当前均已可用。")
            : QStringLiteral("关键服务未完全就绪，建议先回模块页复核。"));

    applyStatusCard(
        ui->pathStatusBadgeLabel,
        ui->pathStatusSummaryLabel,
        ui->pathStatusDetailLabel,
        pathTone,
        pathTone == QStringLiteral("ok")
            ? QStringLiteral("可访问")
            : (pathTone == QStringLiteral("warning") ? QStringLiteral("需复核") : QStringLiteral("不可用")),
        ThreePagePresentationUtils::buildSystemSettingsPathSummary(
            snapshot.dataDirectoryReadable,
            snapshot.dicomDirectoryReadable),
        QStringLiteral("data：%1 | DICOM：%2")
            .arg(QDir::toNativeSeparators(ui->dataPathEdit->text()), QDir::toNativeSeparators(ui->dicomPathEdit->text())));

    refreshRecommendation(snapshot);
}

void SystemSettingsPageNew::applyStatusCard(
    QLabel* badgeLabel,
    QLabel* summaryLabel,
    QLabel* detailLabel,
    const QString& tone,
    const QString& badgeText,
    const QString& summaryText,
    const QString& detailText)
{
    badgeLabel->setText(badgeText);
    badgeLabel->setProperty("statusTone", tone);
    summaryLabel->setText(summaryText);
    detailLabel->setText(detailText);

    polishWidget(badgeLabel);
    polishWidget(summaryLabel);
    polishWidget(detailLabel);
}

void SystemSettingsPageNew::refreshRecommendation(const RuntimeStatusSnapshot& snapshot)
{
    const QString tone = ThreePagePresentationUtils::buildSystemSettingsRecommendationTone(
        snapshot.frameworkReady,
        snapshot.readyServices,
        snapshot.totalServices,
        snapshot.dataDirectoryReadable,
        snapshot.dicomDirectoryReadable);

    ui->systemRecommendationFrame->setProperty("statusTone", tone);
    ui->systemRecommendationBadge->setText(
        tone == QStringLiteral("ok")
            ? QStringLiteral("当前判断")
            : (tone == QStringLiteral("warning") ? QStringLiteral("建议先检查") : QStringLiteral("优先排查")));
    ui->systemRecommendationBadge->setProperty("statusTone", tone);
    ui->systemRecommendationLabel->setText(
        ThreePagePresentationUtils::buildSystemSettingsRecommendation(
            snapshot.frameworkReady,
            snapshot.readyServices,
            snapshot.totalServices,
            snapshot.dataDirectoryReadable,
            snapshot.dicomDirectoryReadable));

    polishWidget(ui->systemRecommendationFrame);
    polishWidget(ui->systemRecommendationBadge);
    polishWidget(ui->systemRecommendationLabel);
}

void SystemSettingsPageNew::polishWidget(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
