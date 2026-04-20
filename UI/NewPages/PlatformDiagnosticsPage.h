#ifndef PLATFORMDIAGNOSTICSPAGE_H
#define PLATFORMDIAGNOSTICSPAGE_H

#include "BasePage.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

#include <functional>

class QTableWidget;

namespace Ui {
class PlatformDiagnosticsPage;
}

class PlatformDiagnosticsPage : public BasePage
{
    Q_OBJECT

public:
    using SnapshotProvider = std::function<PlatformDiagnosticSnapshot()>;

    explicit PlatformDiagnosticsPage(QWidget* parent = nullptr, SnapshotProvider snapshotProvider = {});
    ~PlatformDiagnosticsPage() override;

    void onActivated() override;
    void refreshSnapshot();

signals:
    void backRequested();

private slots:
    void on_backButton_clicked();

private:
    void setupTables();
    void populatePluginTable(const QVector<PlatformPluginLifecycleSnapshot>& plugins, const QString& slowestPluginId);
    void populateTraceTable(const QVector<PlatformStartupTraceEntry>& startupTrace);
    void populateProblemTable(const QVector<PlatformDiagnosticProblem>& problems);
    void applyPluginSorting(QVector<PlatformPluginLifecycleSnapshot>* plugins) const;
    void applyBlockingPointEmphasis(bool highlight);
    void applyRowEmphasis(QTableWidget* table, int row, bool highlight) const;
    QString runtimeModeText(PlatformRuntimeMode runtimeMode) const;
    QString bootstrapText(PlatformBootstrapLevel level) const;
    QString startupPolicyText(PlatformStartupPolicy policy) const;
    QString pluginStateText(PlatformPluginState state) const;
    QString resultText(PlatformLifecycleResult result) const;
    QString severityText(PlatformDiagnosticSeverity severity) const;
    QString lifecycleStepText(PlatformLifecycleStep step) const;
    QString boolText(bool value) const;
    QString warmupTailText(const PlatformDiagnosticSnapshot& snapshot) const;
    QString slowestPluginText(const PlatformDiagnosticSnapshot& snapshot) const;
    QString pluginDisplayText(const PlatformPluginLifecycleSnapshot& plugin) const;
    QString recoveryText(const QStringList& recoveryHints) const;
    QString traceScopeText(const PlatformStartupTraceEntry& traceEntry) const;
    QString traceSubjectText(const PlatformStartupTraceEntry& traceEntry) const;
    QString listText(const QStringList& values) const;
    QString missingDependenciesText(const PlatformPluginLifecycleSnapshot& snapshot) const;

    Ui::PlatformDiagnosticsPage* ui;
    SnapshotProvider m_snapshotProvider;
};

#endif // PLATFORMDIAGNOSTICSPAGE_H
