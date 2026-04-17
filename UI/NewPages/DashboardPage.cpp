#include "DashboardPage.h"
#include "ui_DashboardPage.h"

#include "ThreePagePresentationUtils.h"

#include <QFrame>
#include <QLabel>
#include <QLayoutItem>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Framework/CTKManager.h"
#include "Plugins/DicomViewer/DicomViewerService.h"
#include "Plugins/UserManagement/UserManagementService.h"
#endif

namespace
{
QString compactText(const QString& text, const QString& fallback)
{
    const QString normalized = text.simplified();
    if (normalized.isEmpty()) {
        return fallback;
    }
    return normalized.size() > 28 ? normalized.left(27) + QStringLiteral("…") : normalized;
}
}

DashboardPageNew::DashboardPageNew(QWidget* parent)
    : BasePage(parent)
    , ui(new Ui::DashboardPage)
    , m_currentPatientId(-1)
    , m_currentDicomStudyCount(0)
{
    ui->setupUi(this);
    setObjectName("DashboardPage");

    ui->dashboardEyebrowLabel->setText(QStringLiteral("CASE WORKBENCH"));
    ui->titleLabel->setText(QStringLiteral("病例工作台"));
    ui->dashboardSubtitleLabel->setText(QStringLiteral("选定病例后，核对基础资料、影像准备与导航入口状态。"));
    ui->backButton->setText(QStringLiteral("返回数据管理"));
    ui->refreshButton->setText(QStringLiteral("刷新病例"));
    ui->patientListSubtitleLabel->setText(QStringLiteral("左侧选定病例后，右侧同步刷新详情、影像与导航状态。"));
    ui->enterNavigationButton->setText(QStringLiteral("继续进入导航"));
    ui->enterNavigationButton->setDefault(true);
    ui->enterNavigationButton->setAutoDefault(true);
    ui->mainSplitter->setStretchFactor(0, 1);
    ui->mainSplitter->setStretchFactor(1, 3);

    setOverviewPatientSummary(QStringLiteral("未选择病例"));
    ui->overviewDiagnosisValueLabel->setText(QStringLiteral("等待病例"));
    ui->overviewDoctorValueLabel->setText(QStringLiteral("待选择"));
    setOverviewDicomSummary(0);
    updateNavigationCta(false);
}

DashboardPageNew::~DashboardPageNew()
{
    delete ui;
}

void DashboardPageNew::onActivated()
{
    BasePage::onActivated();
    loadPatients();
}

void DashboardPageNew::setCurrentPatientId(int patientId)
{
    m_currentPatientId = patientId;

    const int index = m_patientIds.indexOf(patientId);
    if (index >= 0) {
        ui->patientListWidget->setCurrentRow(index);
    }
}

void DashboardPageNew::on_backButton_clicked()
{
    emit backToManagementRequested();
    emit navigateTo(toInt(PageIndex::Management));
}

void DashboardPageNew::on_logoutButton_clicked()
{
    if (showConfirm(QStringLiteral("返回欢迎页"), QStringLiteral("确定返回欢迎页吗？"))) {
        emit logoutRequested();
        emit navigateTo(toInt(PageIndex::Welcome));
    }
}

void DashboardPageNew::on_refreshButton_clicked()
{
    loadPatients();
}

void DashboardPageNew::on_enterNavigationButton_clicked()
{
    if (m_currentPatientId < 0) {
        showWarning(QStringLiteral("进入导航"), QStringLiteral("请先选择一位患者。"));
        return;
    }

    emit enterNavigationRequested(m_currentPatientId);
    emit navigateTo(toInt(PageIndex::Navigation));
}

void DashboardPageNew::on_patientListWidget_currentRowChanged(int currentRow)
{
    if (currentRow < 0 || currentRow >= m_patientIds.size()) {
        clearPatientDetails();
        return;
    }

    m_currentPatientId = m_patientIds[currentRow];
    loadPatientDetails(m_currentPatientId);
    loadDicomImages(m_currentPatientId);
}

void DashboardPageNew::on_patientSearchEdit_textChanged(const QString& text)
{
    for (int i = 0; i < ui->patientListWidget->count(); ++i) {
        auto* item = ui->patientListWidget->item(i);
        const bool match = item->text().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void DashboardPageNew::loadPatients()
{
    ui->patientListWidget->clear();
    m_patientIds.clear();
    clearPatientDetails();

#ifdef CTK_PLUGIN_FRAMEWORK
    if (auto* userService = CTKManager::instance()->getService<UserManagementService>()) {
        const auto patients = userService->listPatients();
        for (const auto& patient : patients) {
            m_patientIds.append(patient.id);
            ui->patientListWidget->addItem(QStringLiteral("%1 - %2").arg(patient.id).arg(patient.name));
        }

        if (!patients.isEmpty()) {
            ui->patientListWidget->setCurrentRow(0);
        }
        return;
    }
#endif

    const QStringList testPatients = {
        QStringLiteral("1 - 患者A（踝关节炎）"),
        QStringLiteral("2 - 患者B（踝关节骨折）"),
        QStringLiteral("3 - 患者C（踝关节置换）")
    };
    m_patientIds << 1 << 2 << 3;

    for (const QString& patient : testPatients) {
        ui->patientListWidget->addItem(patient);
    }

    if (ui->patientListWidget->count() > 0) {
        ui->patientListWidget->setCurrentRow(0);
    }
}

void DashboardPageNew::loadPatientDetails(int patientId)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (auto* userService = CTKManager::instance()->getService<UserManagementService>()) {
        const auto patient = userService->getPatient(patientId);
        if (patient.id > 0) {
            const QString diagnosisSummary = compactText(patient.description, QStringLiteral("待补充"));
            ui->patientNameLabel->setText(patient.name);
            ui->idLabel->setText(QString::number(patient.id));
            ui->genderLabel->setText(patient.gender);
            ui->ageLabel->setText(QString::number(patient.age));
            ui->phoneLabel->setText(patient.phone);
            ui->diagnosisLabel->setText(patient.description.isEmpty() ? QStringLiteral("待补充") : patient.description);
            ui->doctorNameLabel->setText(QStringLiteral("-"));
            ui->departmentLabel->setText(patient.description.isEmpty() ? QStringLiteral("待补充") : compactText(patient.description, QStringLiteral("待补充")));

            setOverviewPatientSummary(QStringLiteral("%1 号病例 / %2").arg(patient.id).arg(patient.name));
            ui->overviewDiagnosisValueLabel->setText(diagnosisSummary);
            ui->overviewDoctorValueLabel->setText(QStringLiteral("待分配"));
            updateNavigationCta(true);
            return;
        }
    }
#endif

    ui->patientNameLabel->setText(QStringLiteral("患者%1").arg(patientId));
    ui->idLabel->setText(QString::number(patientId));
    ui->genderLabel->setText(QStringLiteral("男"));
    ui->ageLabel->setText(QStringLiteral("45"));
    ui->phoneLabel->setText(QStringLiteral("139xxxx1234"));
    ui->diagnosisLabel->setText(QStringLiteral("踝关节炎，需进一步评估置换方案。"));
    ui->doctorNameLabel->setText(QStringLiteral("张伟 医生"));
    ui->departmentLabel->setText(QStringLiteral("骨科"));

    setOverviewPatientSummary(QStringLiteral("%1 号病例 / 患者%1").arg(patientId));
    ui->overviewDiagnosisValueLabel->setText(QStringLiteral("踝关节炎评估"));
    ui->overviewDoctorValueLabel->setText(QStringLiteral("张伟 医生"));
    updateNavigationCta(true);
}

void DashboardPageNew::loadDicomImages(int patientId)
{
    m_currentDicomStudyCount = 0;
    ui->noDicomLabel->hide();

    QLayoutItem* child = nullptr;
    while ((child = ui->dicomContentLayout->takeAt(0)) != nullptr) {
        if (child->widget() && child->widget() != ui->noDicomLabel) {
            delete child->widget();
        }
        delete child;
    }

#ifdef CTK_PLUGIN_FRAMEWORK
    if (auto* dicomService = CTKManager::instance()->getService<DicomViewerService>()) {
        const auto studies = dicomService->listStudiesByPatient(patientId);
        m_currentDicomStudyCount = studies.size();

        if (!studies.isEmpty()) {
            for (int index = 0; index < studies.size(); ++index) {
                const auto& study = studies.at(index);
                auto* card = new QFrame();
                card->setObjectName("dicomStudyCard");
                card->setMinimumSize(144, 136);
                card->setMaximumSize(144, 136);

                auto* cardLayout = new QVBoxLayout(card);
                cardLayout->setContentsMargins(12, 12, 12, 12);
                cardLayout->setSpacing(8);

                auto* thumbLabel = new QLabel(card);
                thumbLabel->setObjectName("dicomThumbLabel");
                thumbLabel->setFixedSize(112, 78);
                thumbLabel->setAlignment(Qt::AlignCenter);
                thumbLabel->setText(QStringLiteral("DICOM"));

                const QString studyMeta = !study.studyDescription.isEmpty()
                    ? compactText(study.studyDescription, QStringLiteral("检查 %1").arg(index + 1))
                    : (!study.studyID.isEmpty() ? compactText(study.studyID, QStringLiteral("检查 %1").arg(index + 1))
                                                : QStringLiteral("检查 %1").arg(index + 1));
                auto* modalityLabel = new QLabel(studyMeta, card);
                modalityLabel->setObjectName("dicomMetaLabel");
                modalityLabel->setAlignment(Qt::AlignCenter);

                const QString studyDateText = study.studyDate.isValid()
                    ? study.studyDate.toString(QStringLiteral("yyyy-MM-dd"))
                    : QStringLiteral("日期待补充");
                auto* dateLabel = new QLabel(studyDateText, card);
                dateLabel->setObjectName("dicomDateLabel");
                dateLabel->setAlignment(Qt::AlignCenter);

                cardLayout->addWidget(thumbLabel, 0, Qt::AlignCenter);
                cardLayout->addWidget(modalityLabel, 0, Qt::AlignCenter);
                cardLayout->addWidget(dateLabel, 0, Qt::AlignCenter);

                ui->dicomContentLayout->addWidget(card);
            }
            ui->dicomContentLayout->addStretch();
            setOverviewDicomSummary(m_currentDicomStudyCount);
            updateNavigationCta(m_currentPatientId >= 0);
            return;
        }
    }
#endif

    ui->noDicomLabel->setText(QStringLiteral("该患者暂无 DICOM 影像"));
    ui->dicomContentLayout->addWidget(ui->noDicomLabel);
    ui->noDicomLabel->show();
    setOverviewDicomSummary(m_currentDicomStudyCount);
    updateNavigationCta(m_currentPatientId >= 0);
}

void DashboardPageNew::clearPatientDetails()
{
    m_currentPatientId = -1;
    m_currentDicomStudyCount = 0;
    ui->patientNameLabel->setText(QStringLiteral("请选择患者"));
    ui->idLabel->setText(QStringLiteral("-"));
    ui->genderLabel->setText(QStringLiteral("-"));
    ui->ageLabel->setText(QStringLiteral("-"));
    ui->phoneLabel->setText(QStringLiteral("-"));
    ui->diagnosisLabel->setText(QStringLiteral("-"));
    ui->doctorNameLabel->setText(QStringLiteral("-"));
    ui->departmentLabel->setText(QStringLiteral("-"));

    setOverviewPatientSummary(QStringLiteral("未选择病例"));
    ui->overviewDiagnosisValueLabel->setText(QStringLiteral("等待病例"));
    ui->overviewDoctorValueLabel->setText(QStringLiteral("待选择"));
    setOverviewDicomSummary(0);
    updateNavigationCta(false);

    QLayoutItem* child = nullptr;
    while ((child = ui->dicomContentLayout->takeAt(0)) != nullptr) {
        if (child->widget() && child->widget() != ui->noDicomLabel) {
            delete child->widget();
        }
        delete child;
    }

    ui->noDicomLabel->setText(QStringLiteral("暂无 DICOM 影像"));
    ui->dicomContentLayout->addWidget(ui->noDicomLabel);
    ui->noDicomLabel->show();
}

void DashboardPageNew::setOverviewPatientSummary(const QString& summary)
{
    ui->overviewPatientValueLabel->setText(summary);
}

void DashboardPageNew::setOverviewDicomSummary(int studyCount)
{
    const QString tone = studyCount > 0 ? QStringLiteral("ok") : QStringLiteral("warning");
    ui->overviewDicomValueLabel->setText(studyCount > 0 ? QStringLiteral("%1 组检查").arg(studyCount) : QStringLiteral("暂无影像"));
    ui->overviewDicomValueLabel->setToolTip(ThreePagePresentationUtils::buildDashboardDicomSummary(studyCount));
    ui->overviewDicomCard->setProperty("statusTone", tone);
    ui->overviewDicomValueLabel->setProperty("statusTone", tone);
    polishWidget(ui->overviewDicomCard);
    polishWidget(ui->overviewDicomValueLabel);
}

void DashboardPageNew::updateNavigationCta(bool patientSelected)
{
    const QString tone = ThreePagePresentationUtils::buildDashboardNavigationTone(patientSelected, m_currentDicomStudyCount);
    const QString title = !patientSelected
        ? QStringLiteral("先选择病例")
        : (m_currentDicomStudyCount > 0 ? QStringLiteral("病例与影像已就绪") : QStringLiteral("病例已就绪，可继续"));

    ui->navigationCtaTitleLabel->setText(title);
    ui->navigationCtaHintLabel->setText(
        ThreePagePresentationUtils::buildDashboardNavigationHint(patientSelected, m_currentDicomStudyCount));
    ui->navigationCtaFrame->setProperty("statusTone", tone);
    ui->navigationCtaTitleLabel->setProperty("statusTone", tone);
    polishWidget(ui->navigationCtaFrame);
    polishWidget(ui->navigationCtaTitleLabel);
    ui->enterNavigationButton->setEnabled(patientSelected);
}

void DashboardPageNew::polishWidget(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
