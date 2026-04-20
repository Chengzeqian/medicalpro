#ifndef MANAGEMENTPAGE_NEW_H
#define MANAGEMENTPAGE_NEW_H

#include "BasePage.h"
#include "PageIndex.h"

class QFrame;
class QLabel;
class QPushButton;
class QTableWidget;
class QVBoxLayout;
class QWidget;
class IdentityAppService;

namespace Ui {
class ManagementPage;
}

class ManagementPageNew : public BasePage
{
    Q_OBJECT

public:
    explicit ManagementPageNew(QWidget* parent = nullptr, IdentityAppService* identityAppService = nullptr);
    ~ManagementPageNew();

    void onActivated() override;

signals:
    void backRequested();
    void enterMainSystemRequested();

private slots:
    void on_backButton_clicked();
    void on_enterDashboardButton_clicked();
    void on_enterDashboardButtonSecondary_clicked();

    void on_addDoctorButton_clicked();
    void on_editDoctorButton_clicked();
    void on_deleteDoctorButton_clicked();
    void on_refreshDoctorButton_clicked();
    void on_doctorSearchEdit_textChanged(const QString& text);

    void on_addPatientButton_clicked();
    void on_editPatientButton_clicked();
    void on_deletePatientButton_clicked();
    void on_refreshPatientButton_clicked();
    void on_patientSearchEdit_textChanged(const QString& text);

    void on_addSurgeryButton_clicked();
    void on_editSurgeryButton_clicked();
    void on_deleteSurgeryButton_clicked();
    void on_refreshSurgeryButton_clicked();
    void on_surgerySearchEdit_textChanged(const QString& text);

private:
    void setupTable();
    void setupPageCopy();
    void createOverviewCards(QVBoxLayout* mainLayout);
    void createContentContext(QVBoxLayout* mainLayout);
    void createFlowFrame(QVBoxLayout* mainLayout);
    void refreshOverviewCards();
    void refreshCurrentTabPresentation();
    void applyOverviewCard(
        QFrame* card,
        QLabel* valueLabel,
        QLabel* stateLabel,
        QLabel* hintLabel,
        const QString& entityName,
        int totalCount);
    int visibleRowCount(QTableWidget* table) const;
    QString currentEntityName() const;
    QString currentEntityTitle() const;
    QString currentEntityHint() const;
    QTableWidget* currentEntityTable() const;
    void polishWidget(QWidget* widget);
    void loadDoctors();
    void loadPatients();
    void loadSurgeries();

    Ui::ManagementPage* ui;
    QFrame* m_doctorOverviewCard;
    QLabel* m_doctorOverviewValueLabel;
    QLabel* m_doctorOverviewStateLabel;
    QLabel* m_doctorOverviewHintLabel;
    QFrame* m_patientOverviewCard;
    QLabel* m_patientOverviewValueLabel;
    QLabel* m_patientOverviewStateLabel;
    QLabel* m_patientOverviewHintLabel;
    QFrame* m_surgeryOverviewCard;
    QLabel* m_surgeryOverviewValueLabel;
    QLabel* m_surgeryOverviewStateLabel;
    QLabel* m_surgeryOverviewHintLabel;
    QLabel* m_currentEntityValueLabel;
    QLabel* m_currentEntitySummaryLabel;
    QLabel* m_currentEntityHintLabel;
    QFrame* m_managementFlowFrame;
    QLabel* m_managementFlowHintLabel;
    QPushButton* m_enterDashboardButtonSecondary;
    IdentityAppService* m_identityAppService;
};

#endif // MANAGEMENTPAGE_NEW_H
