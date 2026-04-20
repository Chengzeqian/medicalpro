#ifndef SYSTEMSETTINGSPAGE_NEW_H
#define SYSTEMSETTINGSPAGE_NEW_H

#include "BasePage.h"
#include "PageIndex.h"

#include <functional>

class QLabel;

namespace Ui {
class SystemSettingsPage;
}

class SystemSettingsPageNew : public BasePage
{
    Q_OBJECT

public:
    struct RuntimeStatusSnapshot
    {
        bool frameworkReady = false;
        int pluginCount = 0;
        int readyServices = 0;
        int totalServices = 0;
        bool dataDirectoryReadable = false;
        bool dicomDirectoryReadable = false;
    };

    using RuntimeStatusProvider = std::function<RuntimeStatusSnapshot()>;

    explicit SystemSettingsPageNew(QWidget* parent = nullptr, RuntimeStatusProvider runtimeStatusProvider = {});
    ~SystemSettingsPageNew() override;

    void onActivated() override;

signals:
    void backRequested();
    void diagnosticsRequested();

private slots:
    void on_backButton_clicked();
    void on_saveButton_clicked();
    void on_browseDataPathButton_clicked();
    void on_browseDicomPathButton_clicked();

private:
    void setupPageCopy();
    void loadSettings();
    void saveSettings();
    RuntimeStatusSnapshot collectRuntimeStatus() const;
    void refreshRuntimeStatus();
    void applyStatusCard(
        QLabel* badgeLabel,
        QLabel* summaryLabel,
        QLabel* detailLabel,
        const QString& tone,
        const QString& badgeText,
        const QString& summaryText,
        const QString& detailText);
    void refreshRecommendation(const RuntimeStatusSnapshot& snapshot);
    void polishWidget(QWidget* widget);

    Ui::SystemSettingsPage* ui;
    RuntimeStatusProvider m_runtimeStatusProvider;
};

#endif // SYSTEMSETTINGSPAGE_NEW_H
