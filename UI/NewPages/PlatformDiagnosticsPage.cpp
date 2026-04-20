#include "PlatformDiagnosticsPage.h"
#include "ui_PlatformDiagnosticsPage.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <algorithm>
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

    ui->backButton->setText(QStringLiteral("Back to settings"));
    ui->eyebrowLabel->setText(QStringLiteral("PLATFORM DIAGNOSTICS"));
    ui->titleLabel->setText(QStringLiteral("Runtime diagnostics"));
    ui->subtitleLabel->setText(QStringLiteral("Summary, problems, plugin lifecycle and startup timeline."));
    ui->runtimeModeCaptionLabel->setText(QStringLiteral("Runtime mode"));
    ui->frameworkReadyCaptionLabel->setText(QStringLiteral("Framework ready"));
    ui->platformReadyCaptionLabel->setText(QStringLiteral("Platform ready"));
    ui->startupReadyPathCaptionLabel->setText(QStringLiteral("Startup ready path"));
    ui->warmupTailCaptionLabel->setText(QStringLiteral("Warmup tail"));
    ui->blockingPointCaptionLabel->setText(QStringLiteral("Blocking point"));
    ui->failurePointCaptionLabel->setText(QStringLiteral("Failure point"));
    ui->fullObservedStartupCaptionLabel->setText(QStringLiteral("Full observed startup"));
    ui->slowestPluginCaptionLabel->setText(QStringLiteral("Slowest plugin"));
    ui->recoveryHintsCaptionLabel->setText(QStringLiteral("Recovery hints"));
    ui->problemTableTitleLabel->setText(QStringLiteral("Problems"));
    ui->pluginTableTitleLabel->setText(QStringLiteral("Plugins"));
    ui->traceTableTitleLabel->setText(QStringLiteral("Timeline"));

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
    const auto snapshot = m_snapshotProvider ? m_snapshotProvider() : PlatformDiagnosticSnapshot {};

    ui->runtimeModeValueLabel->setText(runtimeModeText(snapshot.summary.runtimeMode));
    ui->frameworkReadyValueLabel->setText(boolText(snapshot.summary.frameworkReady));
    ui->platformReadyValueLabel->setText(boolText(snapshot.summary.platformReady));
    ui->startupReadyPathValueLabel->setText(QStringLiteral("%1 ms").arg(snapshot.summary.startupReadyPathMs));
    ui->warmupTailValueLabel->setText(warmupTailText(snapshot));
    ui->blockingPointValueLabel->setText(
        snapshot.summary.blockingSpanLabel.isEmpty() ? QStringLiteral("none") : snapshot.summary.blockingSpanLabel);
    ui->failurePointValueLabel->setText(
        snapshot.summary.failurePointLabel.isEmpty() ? QStringLiteral("none") : snapshot.summary.failurePointLabel);
    ui->fullObservedStartupValueLabel->setText(QStringLiteral("%1 ms").arg(snapshot.summary.fullObservedStartupMs));
    ui->slowestPluginValueLabel->setText(slowestPluginText(snapshot));
    ui->recoveryHintsValueLabel->setText(
        snapshot.recoveryHints.isEmpty() ? QStringLiteral("none") : snapshot.recoveryHints.join(QStringLiteral(" | ")));
    applyBlockingPointEmphasis(!snapshot.summary.platformReady);

    auto pluginLifecycle = snapshot.pluginLifecycle;
    applyPluginSorting(&pluginLifecycle);

    populateProblemTable(snapshot.problems);
    populatePluginTable(pluginLifecycle, snapshot.summary.slowestPluginId);
    populateTraceTable(snapshot.startupTrace);
}

void PlatformDiagnosticsPage::on_backButton_clicked()
{
    emit backRequested();
}

void PlatformDiagnosticsPage::setupTables()
{
    ui->problemTableWidget->setColumnCount(6);
    ui->problemTableWidget->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("Severity"),
        QStringLiteral("Plugin"),
        QStringLiteral("Reason"),
        QStringLiteral("Impact Capability"),
        QStringLiteral("Detail"),
        QStringLiteral("Recovery Hint")
    });
    ui->problemTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->problemTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->problemTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->problemTableWidget->setSelectionMode(QAbstractItemView::NoSelection);

    ui->pluginTableWidget->setColumnCount(14);
    ui->pluginTableWidget->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("Plugin ID"),
        QStringLiteral("Symbolic Name"),
        QStringLiteral("Bootstrap"),
        QStringLiteral("Startup Policy"),
        QStringLiteral("State"),
        QStringLiteral("Install(ms)"),
        QStringLiteral("Start(ms)"),
        QStringLiteral("Service Ready(ms)"),
        QStringLiteral("Warmup(ms)"),
        QStringLiteral("Blocking(ms)"),
        QStringLiteral("Slowest Step"),
        QStringLiteral("Missing Dependencies"),
        QStringLiteral("Last Reason"),
        QStringLiteral("Recovery")
    });
    ui->pluginTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->pluginTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->pluginTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->pluginTableWidget->setSelectionMode(QAbstractItemView::NoSelection);

    ui->traceTableWidget->setColumnCount(8);
    ui->traceTableWidget->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("Start Offset(ms)"),
        QStringLiteral("Duration(ms)"),
        QStringLiteral("Scope"),
        QStringLiteral("Plugin / Phase"),
        QStringLiteral("Step"),
        QStringLiteral("Result"),
        QStringLiteral("Blocking"),
        QStringLiteral("Detail")
    });
    ui->traceTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->traceTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->traceTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->traceTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
}

void PlatformDiagnosticsPage::populatePluginTable(
    const QVector<PlatformPluginLifecycleSnapshot>& plugins,
    const QString& slowestPluginId)
{
    ui->pluginTableWidget->clearContents();
    ui->pluginTableWidget->setRowCount(plugins.size());

    for (int row = 0; row < plugins.size(); ++row) {
        const auto& plugin = plugins.at(row);
        ui->pluginTableWidget->setItem(row, 0, makeReadOnlyItem(plugin.pluginId));
        ui->pluginTableWidget->setItem(row, 1, makeReadOnlyItem(plugin.ctkSymbolicName));
        ui->pluginTableWidget->setItem(row, 2, makeReadOnlyItem(bootstrapText(plugin.bootstrapLevel)));
        ui->pluginTableWidget->setItem(row, 3, makeReadOnlyItem(startupPolicyText(plugin.startupPolicy)));
        ui->pluginTableWidget->setItem(row, 4, makeReadOnlyItem(pluginStateText(plugin.state)));
        ui->pluginTableWidget->setItem(row, 5, makeReadOnlyItem(QString::number(plugin.installMs)));
        ui->pluginTableWidget->setItem(row, 6, makeReadOnlyItem(QString::number(plugin.startMs)));
        ui->pluginTableWidget->setItem(row, 7, makeReadOnlyItem(QString::number(plugin.serviceReadyMs)));
        ui->pluginTableWidget->setItem(row, 8, makeReadOnlyItem(QString::number(plugin.warmupMs)));
        ui->pluginTableWidget->setItem(row, 9, makeReadOnlyItem(QString::number(plugin.blockingMs)));
        ui->pluginTableWidget->setItem(row, 10, makeReadOnlyItem(lifecycleStepText(plugin.slowestStep)));
        ui->pluginTableWidget->setItem(row, 11, makeReadOnlyItem(missingDependenciesText(plugin)));
        ui->pluginTableWidget->setItem(
            row,
            12,
            makeReadOnlyItem(plugin.lastReasonCode.isEmpty() ? QStringLiteral("none") : plugin.lastReasonCode));
        ui->pluginTableWidget->setItem(row, 13, makeReadOnlyItem(recoveryText(plugin.recoveryHints)));
        applyRowEmphasis(ui->pluginTableWidget, row, plugin.pluginId == slowestPluginId);
    }
}

void PlatformDiagnosticsPage::populateTraceTable(const QVector<PlatformStartupTraceEntry>& startupTrace)
{
    auto sortedTrace = startupTrace;
    std::stable_sort(
        sortedTrace.begin(),
        sortedTrace.end(),
        [](const PlatformStartupTraceEntry& lhs, const PlatformStartupTraceEntry& rhs) {
            if (lhs.startOffsetMs != rhs.startOffsetMs) return lhs.startOffsetMs < rhs.startOffsetMs;
            return lhs.elapsedMs > rhs.elapsedMs;
        });

    ui->traceTableWidget->clearContents();
    ui->traceTableWidget->setRowCount(sortedTrace.size());

    for (int row = 0; row < sortedTrace.size(); ++row) {
        const auto& traceEntry = sortedTrace.at(row);
        ui->traceTableWidget->setItem(row, 0, makeReadOnlyItem(QString::number(traceEntry.startOffsetMs)));
        ui->traceTableWidget->setItem(row, 1, makeReadOnlyItem(QString::number(traceEntry.elapsedMs)));
        ui->traceTableWidget->setItem(row, 2, makeReadOnlyItem(traceScopeText(traceEntry)));
        ui->traceTableWidget->setItem(row, 3, makeReadOnlyItem(traceSubjectText(traceEntry)));
        ui->traceTableWidget->setItem(row, 4, makeReadOnlyItem(lifecycleStepText(traceEntry.step)));
        ui->traceTableWidget->setItem(row, 5, makeReadOnlyItem(resultText(traceEntry.result)));
        ui->traceTableWidget->setItem(row, 6, makeReadOnlyItem(boolText(traceEntry.blockingStartup)));
        ui->traceTableWidget->setItem(row, 7, makeReadOnlyItem(traceEntry.detail));
    }
}

void PlatformDiagnosticsPage::populateProblemTable(const QVector<PlatformDiagnosticProblem>& problems)
{
    ui->problemTableWidget->clearContents();
    ui->problemTableWidget->setRowCount(problems.size());

    for (int row = 0; row < problems.size(); ++row) {
        const auto& problem = problems.at(row);
        ui->problemTableWidget->setItem(row, 0, makeReadOnlyItem(severityText(problem.severity)));
        ui->problemTableWidget->setItem(row, 1, makeReadOnlyItem(problem.pluginId));
        ui->problemTableWidget->setItem(row, 2, makeReadOnlyItem(problem.reasonCode));
        ui->problemTableWidget->setItem(row, 3, makeReadOnlyItem(listText(problem.impactCapabilities)));
        ui->problemTableWidget->setItem(row, 4, makeReadOnlyItem(problem.detail));
        ui->problemTableWidget->setItem(row, 5, makeReadOnlyItem(listText(problem.recoveryHints)));
    }
}

void PlatformDiagnosticsPage::applyPluginSorting(QVector<PlatformPluginLifecycleSnapshot>* plugins) const
{
    if (!plugins) return;

    const auto statePriority = [](PlatformPluginState state) {
        switch (state) {
        case PlatformPluginState::Failed:
            return 6;
        case PlatformPluginState::Degraded:
            return 5;
        case PlatformPluginState::Starting:
            return 4;
        case PlatformPluginState::Installed:
            return 3;
        case PlatformPluginState::Discovered:
            return 2;
        case PlatformPluginState::Ready:
            return 1;
        }
        return 0;
    };

    std::sort(
        plugins->begin(),
        plugins->end(),
        [statePriority](const PlatformPluginLifecycleSnapshot& lhs, const PlatformPluginLifecycleSnapshot& rhs) {
            const auto lhsPriority = statePriority(lhs.state);
            const auto rhsPriority = statePriority(rhs.state);
            if (lhsPriority != rhsPriority) return lhsPriority > rhsPriority;
            if (lhs.blockingMs != rhs.blockingMs) return lhs.blockingMs > rhs.blockingMs;
            return lhs.pluginId < rhs.pluginId;
        });
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

QString PlatformDiagnosticsPage::bootstrapText(PlatformBootstrapLevel level) const
{
    switch (level) {
    case PlatformBootstrapLevel::Core:
        return QStringLiteral("core");
    case PlatformBootstrapLevel::Deferred:
        return QStringLiteral("deferred");
    }

    return QStringLiteral("unknown");
}

QString PlatformDiagnosticsPage::startupPolicyText(PlatformStartupPolicy policy) const
{
    switch (policy) {
    case PlatformStartupPolicy::Eager:
        return QStringLiteral("eager");
    case PlatformStartupPolicy::OnDemand:
        return QStringLiteral("on_demand");
    case PlatformStartupPolicy::Disabled:
        return QStringLiteral("disabled");
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

QString PlatformDiagnosticsPage::resultText(PlatformLifecycleResult result) const
{
    switch (result) {
    case PlatformLifecycleResult::Running:
        return QStringLiteral("running");
    case PlatformLifecycleResult::Succeeded:
        return QStringLiteral("succeeded");
    case PlatformLifecycleResult::Failed:
        return QStringLiteral("failed");
    case PlatformLifecycleResult::Degraded:
        return QStringLiteral("degraded");
    case PlatformLifecycleResult::Skipped:
        return QStringLiteral("skipped");
    case PlatformLifecycleResult::Timeout:
        return QStringLiteral("timeout");
    }

    return QStringLiteral("unknown");
}

QString PlatformDiagnosticsPage::severityText(PlatformDiagnosticSeverity severity) const
{
    switch (severity) {
    case PlatformDiagnosticSeverity::Critical:
        return QStringLiteral("critical");
    case PlatformDiagnosticSeverity::Error:
        return QStringLiteral("error");
    case PlatformDiagnosticSeverity::Warning:
        return QStringLiteral("warning");
    case PlatformDiagnosticSeverity::Info:
        return QStringLiteral("info");
    }

    return QStringLiteral("unknown");
}

QString PlatformDiagnosticsPage::lifecycleStepText(PlatformLifecycleStep step) const
{
    switch (step) {
    case PlatformLifecycleStep::Install:
        return QStringLiteral("install");
    case PlatformLifecycleStep::Start:
        return QStringLiteral("start");
    case PlatformLifecycleStep::ServiceReady:
        return QStringLiteral("service_ready");
    case PlatformLifecycleStep::Warmup:
        return QStringLiteral("warmup");
    case PlatformLifecycleStep::None:
        break;
    }
    return QStringLiteral("none");
}

QString PlatformDiagnosticsPage::boolText(bool value) const
{
    return value ? QStringLiteral("yes") : QStringLiteral("no");
}

QString PlatformDiagnosticsPage::warmupTailText(const PlatformDiagnosticSnapshot& snapshot) const
{
    if (snapshot.summary.runtimeMode != PlatformRuntimeMode::OrchestrateCore) {
        return QStringLiteral("skipped_by_mode");
    }

    return QStringLiteral("%1 ms").arg(snapshot.summary.startupWarmupTailMs);
}

QString PlatformDiagnosticsPage::slowestPluginText(const PlatformDiagnosticSnapshot& snapshot) const
{
    for (const auto& plugin : snapshot.pluginLifecycle) {
        if (plugin.pluginId != snapshot.summary.slowestPluginId) continue;
        return pluginDisplayText(plugin);
    }

    return snapshot.summary.slowestPluginId.isEmpty() ? QStringLiteral("none") : snapshot.summary.slowestPluginId;
}

QString PlatformDiagnosticsPage::pluginDisplayText(const PlatformPluginLifecycleSnapshot& plugin) const
{
    if (!plugin.displayName.isEmpty() && plugin.displayName != plugin.pluginId) {
        return QStringLiteral("%1 (%2)").arg(plugin.displayName, plugin.pluginId);
    }
    if (!plugin.ctkSymbolicName.isEmpty() && plugin.ctkSymbolicName != plugin.pluginId) {
        return QStringLiteral("%1 (%2)").arg(plugin.ctkSymbolicName, plugin.pluginId);
    }

    return plugin.pluginId.isEmpty() ? QStringLiteral("none") : plugin.pluginId;
}

void PlatformDiagnosticsPage::applyBlockingPointEmphasis(bool highlight)
{
    auto font = ui->blockingPointValueLabel->font();
    font.setBold(highlight);
    ui->blockingPointValueLabel->setFont(font);
}

void PlatformDiagnosticsPage::applyRowEmphasis(QTableWidget* table, int row, bool highlight) const
{
    if (!table) return;

    for (int column = 0; column < table->columnCount(); ++column) {
        auto* item = table->item(row, column);
        if (!item) continue;
        auto font = item->font();
        font.setBold(highlight);
        item->setFont(font);
    }
}

QString PlatformDiagnosticsPage::recoveryText(const QStringList& recoveryHints) const
{
    return recoveryHints.isEmpty() ? QStringLiteral("none") : recoveryHints.join(QStringLiteral(" | "));
}

QString PlatformDiagnosticsPage::traceScopeText(const PlatformStartupTraceEntry& traceEntry) const
{
    return (!traceEntry.pluginId.isEmpty() || !traceEntry.ctkSymbolicName.isEmpty())
        ? QStringLiteral("plugin")
        : QStringLiteral("phase");
}

QString PlatformDiagnosticsPage::traceSubjectText(const PlatformStartupTraceEntry& traceEntry) const
{
    if (!traceEntry.ctkSymbolicName.isEmpty()) return traceEntry.ctkSymbolicName;
    if (!traceEntry.pluginId.isEmpty()) return traceEntry.pluginId;
    if (!traceEntry.phaseLabel.isEmpty()) return traceEntry.phaseLabel;
    return traceEntry.phaseKey;
}

QString PlatformDiagnosticsPage::listText(const QStringList& values) const
{
    return values.isEmpty() ? QStringLiteral("none") : values.join(QStringLiteral(" | "));
}

QString PlatformDiagnosticsPage::missingDependenciesText(const PlatformPluginLifecycleSnapshot& snapshot) const
{
    QStringList parts;

    if (!snapshot.missingRequiredServices.isEmpty()) {
        parts.append(QStringLiteral("services:%1").arg(snapshot.missingRequiredServices.join(QStringLiteral(", "))));
    }
    if (!snapshot.missingRequiredCapabilities.isEmpty()) {
        parts.append(
            QStringLiteral("capabilities:%1").arg(snapshot.missingRequiredCapabilities.join(QStringLiteral(", "))));
    }
    if (!snapshot.missingRequiredPlugins.isEmpty()) {
        parts.append(QStringLiteral("plugins:%1").arg(snapshot.missingRequiredPlugins.join(QStringLiteral(", "))));
    }

    return parts.isEmpty() ? QStringLiteral("none") : parts.join(QStringLiteral(" | "));
}
