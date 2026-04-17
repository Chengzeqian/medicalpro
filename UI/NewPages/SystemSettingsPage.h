#ifndef SYSTEMSETTINGSPAGE_NEW_H
#define SYSTEMSETTINGSPAGE_NEW_H

#include "BasePage.h"
#include "PageIndex.h"

class QLabel;

namespace Ui {
class SystemSettingsPage;
}

/**
 * @brief 系统设置页面
 *
 * 功能：
 * - 通用设置（语言、主题、自动保存）
 * - 设备设置（跟踪器端口、波特率）
 * - 路径设置（数据路径、DICOM路径）
 */
class SystemSettingsPageNew : public BasePage
{
    Q_OBJECT

public:
    explicit SystemSettingsPageNew(QWidget* parent = nullptr);
    ~SystemSettingsPageNew();

    void onActivated() override;

signals:
    void backRequested();  // MainInterfaceWidget期望的信号

private slots:
    void on_backButton_clicked();
    void on_saveButton_clicked();
    void on_browseDataPathButton_clicked();
    void on_browseDicomPathButton_clicked();

private:
    struct RuntimeStatusSnapshot
    {
        bool frameworkReady;
        int pluginCount;
        int readyServices;
        int totalServices;
        bool dataDirectoryReadable;
        bool dicomDirectoryReadable;
    };

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
};

#endif // SYSTEMSETTINGSPAGE_NEW_H
