#ifndef MODULESELECTIONPAGE_NEW_H
#define MODULESELECTIONPAGE_NEW_H

#include "BasePage.h"
#include "PageIndex.h"

#include <QStringList>
#include <functional>

class QLabel;
class QTimer;
class QWidget;

namespace Ui {
class ModuleSelectionPage;
}

class ModuleSelectionPageNew : public BasePage
{
    Q_OBJECT

public:
    struct ModuleRuntimeStatus
    {
        bool frameworkReady = false;
        bool workflowReady = false;
        int readyServices = 0;
        int totalServices = 0;
        QStringList missingServices;
    };

    using RuntimeStatusProvider = std::function<ModuleRuntimeStatus()>;

    explicit ModuleSelectionPageNew(QWidget* parent = nullptr, RuntimeStatusProvider runtimeStatusProvider = {});
    ~ModuleSelectionPageNew();

    void onActivated() override;
    void setCurrentUser(const QString& username);

signals:
    void systemSettingsRequested();
    void ankleSurgeryRequested();
    void backRequested();

private slots:
    void on_ankleSurgeryButton_clicked();
    void on_systemSettingsButton_clicked();
    void on_logoutButton_clicked();
    void refreshClock();

private:
    ModuleRuntimeStatus collectRuntimeStatus() const;
    void refreshHeaderState();
    void refreshModuleCards();
    void applyStatusTag(QLabel* label, const QString& text, const QString& tone);
    QString buildSettingsHint(const ModuleRuntimeStatus& status) const;
    void polishWidget(QWidget* widget);

    Ui::ModuleSelectionPage* ui;
    RuntimeStatusProvider m_runtimeStatusProvider;
    QString m_currentUser;
    QTimer* m_clockTimer;
};

#endif // MODULESELECTIONPAGE_NEW_H
