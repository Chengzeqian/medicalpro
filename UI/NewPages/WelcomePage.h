#ifndef WELCOMEPAGE_NEW_H
#define WELCOMEPAGE_NEW_H

#include "BasePage.h"
#include "PageIndex.h"
#include "Framework/Platform/Contracts/StartupShellSnapshot.h"

#include <QFrame>
#include <QLabel>
#include <QStringList>
#include <functional>

namespace Ui {
class WelcomePage;
}

class QResizeEvent;
class QTimer;

class WelcomePageNew : public BasePage
{
    Q_OBJECT

public:
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

    using RuntimeStatusProvider = std::function<RuntimeStatusSnapshot()>;

    explicit WelcomePageNew(QWidget* parent = nullptr, RuntimeStatusProvider runtimeStatusProvider = {});
    ~WelcomePageNew();

    void onActivated() override;
    void applyStartupShellSnapshot(const StartupShellSnapshot& snapshot);

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void enterSystemRequested();
    void retryStartupRequested();

private slots:
    void on_enterButton_clicked();
    void on_exitButton_clicked();
    void scheduleRuntimeStatusRefresh();

private:
    void setupUI();
    void applyBranding();
    void refreshRuntimeStatus();
    void updateResponsiveLayout();
    void applyShellDecisionState(const StartupShellSnapshot& snapshot);
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
    RuntimeStatusProvider m_runtimeStatusProvider;
    QTimer* m_runtimeStatusRefreshTimer;
    bool m_shellSnapshotActive = false;
    StartupShellState m_shellState = StartupShellState::Booting;
};

#endif // WELCOMEPAGE_NEW_H
