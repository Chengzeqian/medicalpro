#ifndef DASHBOARDPAGE_NEW_H
#define DASHBOARDPAGE_NEW_H

#include "BasePage.h"
#include "PageIndex.h"

namespace Ui {
class DashboardPage;
}

class QWidget;
class IdentityAppService;
class ImagingAppService;
class NavigationAppService;

class DashboardPageNew : public BasePage
{
    Q_OBJECT

public:
    explicit DashboardPageNew(
        QWidget* parent = nullptr,
        IdentityAppService* identityAppService = nullptr,
        ImagingAppService* imagingAppService = nullptr,
        NavigationAppService* navigationAppService = nullptr);
    ~DashboardPageNew();

    void onActivated() override;
    void setCurrentPatientId(int patientId);
    int getCurrentPatientId() const { return m_currentPatientId; }

signals:
    void enterNavigationRequested(int patientId);
    void backToManagementRequested();
    void logoutRequested();

private slots:
    void on_backButton_clicked();
    void on_logoutButton_clicked();
    void on_refreshButton_clicked();
    void on_enterNavigationButton_clicked();
    void on_patientListWidget_currentRowChanged(int currentRow);
    void on_patientSearchEdit_textChanged(const QString& text);

public slots:
    void refreshDashboard() { onActivated(); }

private:
    void loadPatients();
    void loadPatientDetails(int patientId);
    void loadDicomImages(int patientId);
    void clearPatientDetails();
    void setOverviewPatientSummary(const QString& summary);
    void setOverviewDicomSummary(int studyCount);
    void updateNavigationCta(bool patientSelected);
    void polishWidget(QWidget* widget);

    Ui::DashboardPage* ui;
    int m_currentPatientId;
    int m_currentDicomStudyCount;
    QList<int> m_patientIds;
    IdentityAppService* m_identityAppService;
    ImagingAppService* m_imagingAppService;
    NavigationAppService* m_navigationAppService;
};

#endif // DASHBOARDPAGE_NEW_H
