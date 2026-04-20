#ifndef PLATFORMDIAGNOSTICSPAGE_H
#define PLATFORMDIAGNOSTICSPAGE_H

#include "BasePage.h"
#include "Framework/Platform/Contracts/PlatformSnapshots.h"

#include <functional>

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
    void populatePluginTable(const QVector<PlatformPluginRuntimeSnapshot>& plugins);
    void populateTraceTable(const QVector<PlatformStartupTraceEntry>& startupTrace);
    QString runtimeModeText(PlatformRuntimeMode runtimeMode) const;
    QString pluginStateText(PlatformPluginState state) const;
    QString missingDependenciesText(const PlatformPluginRuntimeSnapshot& snapshot) const;

    Ui::PlatformDiagnosticsPage* ui;
    SnapshotProvider m_snapshotProvider;
};

#endif // PLATFORMDIAGNOSTICSPAGE_H
