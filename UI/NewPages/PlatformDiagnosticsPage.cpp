#include "PlatformDiagnosticsPage.h"
#include "ui_PlatformDiagnosticsPage.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <utility>

namespace
{
QTableWidgetItem* makeReadOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}
}

PlatformDiagnosticsPage::PlatformDiagnosticsPage(QWidget* parent, SnapshotProvider snapshotProvider)
    : BasePage(parent)
    , ui(new Ui::PlatformDiagnosticsPage)
    , m_snapshotProvider(std::move(snapshotProvider))
{
    ui->setupUi(this);
    setObjectName(QStringLiteral("PlatformDiagnosticsPage"));

    ui->backButton->setText(QStringLiteral("返回设置页"));
    ui->eyebrowLabel->setText(QStringLiteral("PLATFORM DIAGNOSTICS"));
    ui->titleLabel->setText(QStringLiteral("平台运行诊断"));
    ui->subtitleLabel->setText(QStringLiteral("集中查看运行模式、插件快照、启动跟踪和恢复建议。"));
    ui->runtimeModeCaptionLabel->setText(QStringLiteral("运行模式"));
    ui->frameworkReadyCaptionLabel->setText(QStringLiteral("框架状态"));
    ui->recoveryHintsCaptionLabel->setText(QStringLiteral("恢复建议"));
    ui->pluginTableTitleLabel->setText(QStringLiteral("插件快照"));
    ui->traceTableTitleLabel->setText(QStringLiteral("启动跟踪"));

    setupTables();
    refreshSnapshot();
}

PlatformDiagnosticsPage::~PlatformDiagnosticsPage()
{
    delete ui;
}

void PlatformDiagnosticsPage::onActivated()
{
    BasePage::onActivated();
    refreshSnapshot();
}

void PlatformDiagnosticsPage::refreshSnapshot()
{
    const auto snapshot = m_snapshotProvider ? m_snapshotProvider() : PlatformDiagnosticSnapshot{};

    ui->runtimeModeValueLabel->setText(runtimeModeText(snapshot.runtimeMode));
    ui->frameworkReadyValueLabel->setText(snapshot.frameworkReady ? QStringLiteral("已就绪") : QStringLiteral("未就绪"));
    ui->recoveryHintsValueLabel->setText(
        snapshot.recoveryHints.isEmpty() ? QStringLiteral("暂无") : snapshot.recoveryHints.join(QStringLiteral(" | ")));

    populatePluginTable(snapshot.plugins);
    populateTraceTable(snapshot.startupTrace);
}

void PlatformDiagnosticsPage::on_backButton_clicked()
{
    emit backRequested();
}

void PlatformDiagnosticsPage::setupTables()
{
    ui->pluginTableWidget->setColumnCount(4);
    ui->pluginTableWidget->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("插件 ID"),
        QStringLiteral("CTK 名称"),
        QStringLiteral("状态"),
        QStringLiteral("缺失依赖")
    });
    ui->pluginTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->pluginTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->pluginTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->pluginTableWidget->setSelectionMode(QAbstractItemView::NoSelection);

    ui->traceTableWidget->setColumnCount(5);
    ui->traceTableWidget->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("阶段"),
        QStringLiteral("说明"),
        QStringLiteral("结果"),
        QStringLiteral("耗时(ms)"),
        QStringLiteral("详情")
    });
    ui->traceTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->traceTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->traceTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->traceTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
}

void PlatformDiagnosticsPage::populatePluginTable(const QVector<PlatformPluginRuntimeSnapshot>& plugins)
{
    ui->pluginTableWidget->clearContents();
    ui->pluginTableWidget->setRowCount(plugins.size());

    for (int row = 0; row < plugins.size(); ++row) {
        const auto& plugin = plugins.at(row);
        ui->pluginTableWidget->setItem(row, 0, makeReadOnlyItem(plugin.pluginId));
        ui->pluginTableWidget->setItem(row, 1, makeReadOnlyItem(plugin.ctkSymbolicName));
        ui->pluginTableWidget->setItem(row, 2, makeReadOnlyItem(pluginStateText(plugin.state)));
        ui->pluginTableWidget->setItem(row, 3, makeReadOnlyItem(missingDependenciesText(plugin)));
    }
}

void PlatformDiagnosticsPage::populateTraceTable(const QVector<PlatformStartupTraceEntry>& startupTrace)
{
    ui->traceTableWidget->clearContents();
    ui->traceTableWidget->setRowCount(startupTrace.size());

    for (int row = 0; row < startupTrace.size(); ++row) {
        const auto& traceEntry = startupTrace.at(row);
        ui->traceTableWidget->setItem(row, 0, makeReadOnlyItem(traceEntry.phaseKey));
        ui->traceTableWidget->setItem(row, 1, makeReadOnlyItem(traceEntry.phaseLabel));
        ui->traceTableWidget->setItem(row, 2, makeReadOnlyItem(traceEntry.success ? QStringLiteral("成功") : QStringLiteral("失败")));
        ui->traceTableWidget->setItem(row, 3, makeReadOnlyItem(QString::number(traceEntry.elapsedMs)));
        ui->traceTableWidget->setItem(row, 4, makeReadOnlyItem(traceEntry.detail));
    }
}

QString PlatformDiagnosticsPage::runtimeModeText(PlatformRuntimeMode runtimeMode) const
{
    switch (runtimeMode) {
    case PlatformRuntimeMode::ObserveOnly:
        return QStringLiteral("observe_only");
    case PlatformRuntimeMode::FacadeMode:
        return QStringLiteral("facade_mode");
    case PlatformRuntimeMode::OrchestrateCore:
        return QStringLiteral("orchestrate_core");
    }

    return QStringLiteral("unknown");
}

QString PlatformDiagnosticsPage::pluginStateText(PlatformPluginState state) const
{
    switch (state) {
    case PlatformPluginState::Discovered:
        return QStringLiteral("discovered");
    case PlatformPluginState::Installed:
        return QStringLiteral("installed");
    case PlatformPluginState::Starting:
        return QStringLiteral("starting");
    case PlatformPluginState::Ready:
        return QStringLiteral("ready");
    case PlatformPluginState::Degraded:
        return QStringLiteral("degraded");
    case PlatformPluginState::Failed:
        return QStringLiteral("failed");
    }

    return QStringLiteral("unknown");
}

QString PlatformDiagnosticsPage::missingDependenciesText(const PlatformPluginRuntimeSnapshot& snapshot) const
{
    QStringList parts;

    if (!snapshot.missingRequiredServices.isEmpty()) {
        parts.append(QStringLiteral("服务:%1").arg(snapshot.missingRequiredServices.join(QStringLiteral(", "))));
    }
    if (!snapshot.missingRequiredCapabilities.isEmpty()) {
        parts.append(QStringLiteral("能力:%1").arg(snapshot.missingRequiredCapabilities.join(QStringLiteral(", "))));
    }
    if (!snapshot.missingRequiredPlugins.isEmpty()) {
        parts.append(QStringLiteral("插件:%1").arg(snapshot.missingRequiredPlugins.join(QStringLiteral(", "))));
    }

    return parts.isEmpty() ? QStringLiteral("无") : parts.join(QStringLiteral(" | "));
}
