#include "ManagementPage.h"
#include "ui_ManagementPage.h"

#include "Framework/Platform/Facades/IdentityAppService.h"
#include "ThreePagePresentationUtils.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

ManagementPageNew::ManagementPageNew(QWidget* parent, IdentityAppService* identityAppService)
    : BasePage(parent)
    , ui(new Ui::ManagementPage)
    , m_doctorOverviewCard(nullptr)
    , m_doctorOverviewValueLabel(nullptr)
    , m_doctorOverviewStateLabel(nullptr)
    , m_doctorOverviewHintLabel(nullptr)
    , m_patientOverviewCard(nullptr)
    , m_patientOverviewValueLabel(nullptr)
    , m_patientOverviewStateLabel(nullptr)
    , m_patientOverviewHintLabel(nullptr)
    , m_surgeryOverviewCard(nullptr)
    , m_surgeryOverviewValueLabel(nullptr)
    , m_surgeryOverviewStateLabel(nullptr)
    , m_surgeryOverviewHintLabel(nullptr)
    , m_currentEntityValueLabel(nullptr)
    , m_currentEntitySummaryLabel(nullptr)
    , m_currentEntityHintLabel(nullptr)
    , m_managementFlowFrame(nullptr)
    , m_managementFlowHintLabel(nullptr)
    , m_enterDashboardButtonSecondary(nullptr)
    , m_identityAppService(identityAppService)
{
    ui->setupUi(this);
    setObjectName("ManagementPage");
    setStyleSheet(QString());

    setupTable();
    setupPageCopy();

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int) {
        refreshCurrentTabPresentation();
    });

    refreshOverviewCards();
    refreshCurrentTabPresentation();
}

ManagementPageNew::~ManagementPageNew()
{
    delete ui;
}

void ManagementPageNew::onActivated()
{
    BasePage::onActivated();

    loadDoctors();
    loadPatients();
    loadSurgeries();
    refreshOverviewCards();
    refreshCurrentTabPresentation();
}

void ManagementPageNew::setupTable()
{
    const auto setupTableWidget = [](QTableWidget* table) {
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        table->horizontalHeader()->setMinimumSectionSize(92);
        table->setAlternatingRowColors(true);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setShowGrid(false);
        table->setFocusPolicy(Qt::NoFocus);
        table->setWordWrap(false);
        table->verticalHeader()->setDefaultSectionSize(44);
        table->verticalHeader()->setVisible(false);
    };

    setupTableWidget(ui->doctorTable);
    setupTableWidget(ui->patientTable);
    setupTableWidget(ui->surgeryTable);
    ui->tabWidget->setDocumentMode(true);
    ui->tabWidget->tabBar()->setExpanding(false);
}

void ManagementPageNew::setupPageCopy()
{
    auto* mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (!mainLayout) {
        return;
    }

    ui->titleLabel->setText(QStringLiteral("数据管理中台"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->doctorTab), QStringLiteral("医生"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->patientTab), QStringLiteral("患者"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->surgeryTab), QStringLiteral("手术"));
    ui->backButton->setText(QStringLiteral("返回模块页"));
    ui->backButton->setMinimumHeight(40);
    ui->enterDashboardButton->setText(QStringLiteral("继续进入病例工作台"));
    ui->enterDashboardButton->setMinimumHeight(46);
    ui->enterDashboardButton->setMinimumWidth(196);
    ui->enterDashboardButton->setDefault(true);
    ui->enterDashboardButton->setAutoDefault(true);

    ui->doctorSearchEdit->setPlaceholderText(QStringLiteral("按姓名、科室或职称搜索医生"));
    ui->patientSearchEdit->setPlaceholderText(QStringLiteral("按姓名、诊断或主治医生搜索患者"));
    ui->surgerySearchEdit->setPlaceholderText(QStringLiteral("按手术类型、患者或状态搜索任务"));
    ui->addDoctorButton->setText(QStringLiteral("新增医生"));
    ui->editDoctorButton->setText(QStringLiteral("编辑"));
    ui->deleteDoctorButton->setText(QStringLiteral("删除"));
    ui->refreshDoctorButton->setText(QStringLiteral("刷新"));
    ui->addPatientButton->setText(QStringLiteral("新增患者"));
    ui->editPatientButton->setText(QStringLiteral("编辑"));
    ui->deletePatientButton->setText(QStringLiteral("删除"));
    ui->refreshPatientButton->setText(QStringLiteral("刷新"));
    ui->addSurgeryButton->setText(QStringLiteral("新增手术"));
    ui->editSurgeryButton->setText(QStringLiteral("编辑"));
    ui->deleteSurgeryButton->setText(QStringLiteral("删除"));
    ui->refreshSurgeryButton->setText(QStringLiteral("刷新"));

    if (mainLayout->count() > 0 && mainLayout->itemAt(0)->layout()) {
        auto* headerItem = mainLayout->takeAt(0);
        auto* headerLayout = qobject_cast<QHBoxLayout*>(headerItem->layout());
        auto* headerFrame = new QFrame(this);
        headerFrame->setObjectName("managementHeaderFrame");
        headerFrame->setLayout(headerLayout);
        headerLayout->setContentsMargins(18, 18, 18, 18);
        headerLayout->setSpacing(12);

        const int titleIndex = headerLayout->indexOf(ui->titleLabel);
        if (titleIndex >= 0) {
            auto* titleItem = headerLayout->takeAt(titleIndex);
            delete titleItem;

            auto* titleContainer = new QWidget(headerFrame);
            auto* titleLayout = new QVBoxLayout(titleContainer);
            titleLayout->setContentsMargins(0, 0, 0, 0);
            titleLayout->setSpacing(4);

            auto* eyebrowLabel = new QLabel(QStringLiteral("DATA OPERATIONS HUB"), titleContainer);
            eyebrowLabel->setObjectName("managementEyebrowLabel");
            auto* subtitleLabel = new QLabel(
                QStringLiteral("先核对医生、患者与手术任务，再进入病例工作台。"),
                titleContainer);
            subtitleLabel->setObjectName("subtitleLabel");
            subtitleLabel->setWordWrap(true);

            ui->titleLabel->setParent(titleContainer);
            titleLayout->addWidget(eyebrowLabel);
            titleLayout->addWidget(ui->titleLabel);
            titleLayout->addWidget(subtitleLabel);
            headerLayout->insertWidget(titleIndex, titleContainer, 1);
        }

        mainLayout->insertWidget(0, headerFrame);
        delete headerItem;
    }

    createOverviewCards(mainLayout);
    createContentContext(mainLayout);
    createFlowFrame(mainLayout);
}

void ManagementPageNew::createOverviewCards(QVBoxLayout* mainLayout)
{
    auto* overviewCardsFrame = new QFrame(this);
    overviewCardsFrame->setObjectName("overviewCardsFrame");
    auto* overviewCardsLayout = new QHBoxLayout(overviewCardsFrame);
    overviewCardsLayout->setContentsMargins(0, 0, 0, 0);
    overviewCardsLayout->setSpacing(14);

    const auto createCard = [overviewCardsLayout](
                                const QString& frameName,
                                const QString& eyebrowName,
                                const QString& eyebrowText,
                                const QString& valueName,
                                const QString& stateName,
                                const QString& hintName,
                                QFrame*& cardOut,
                                QLabel*& valueOut,
                                QLabel*& stateOut,
                                QLabel*& hintOut) {
        auto* card = new QFrame();
        card->setObjectName(frameName);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 16, 16, 16);
        cardLayout->setSpacing(8);

        auto* eyebrowLabel = new QLabel(eyebrowText, card);
        eyebrowLabel->setObjectName(eyebrowName);

        auto* topRowLayout = new QHBoxLayout();
        topRowLayout->setSpacing(10);

        auto* valueLabel = new QLabel(card);
        valueLabel->setObjectName(valueName);
        auto* stateLabel = new QLabel(QStringLiteral("待补充"), card);
        stateLabel->setObjectName(stateName);
        auto* hintLabel = new QLabel(card);
        hintLabel->setObjectName(hintName);
        hintLabel->setWordWrap(true);

        topRowLayout->addWidget(valueLabel);
        topRowLayout->addWidget(stateLabel, 0, Qt::AlignTop);
        topRowLayout->addStretch();

        cardLayout->addWidget(eyebrowLabel);
        cardLayout->addLayout(topRowLayout);
        cardLayout->addWidget(hintLabel);

        overviewCardsLayout->addWidget(card, 1);
        cardOut = card;
        valueOut = valueLabel;
        stateOut = stateLabel;
        hintOut = hintLabel;
    };

    createCard(
        QStringLiteral("doctorOverviewCard"),
        QStringLiteral("doctorOverviewEyebrowLabel"),
        QStringLiteral("DOCTORS"),
        QStringLiteral("doctorOverviewValueLabel"),
        QStringLiteral("doctorOverviewStateLabel"),
        QStringLiteral("doctorOverviewHintLabel"),
        m_doctorOverviewCard,
        m_doctorOverviewValueLabel,
        m_doctorOverviewStateLabel,
        m_doctorOverviewHintLabel);

    createCard(
        QStringLiteral("patientOverviewCard"),
        QStringLiteral("patientOverviewEyebrowLabel"),
        QStringLiteral("PATIENTS"),
        QStringLiteral("patientOverviewValueLabel"),
        QStringLiteral("patientOverviewStateLabel"),
        QStringLiteral("patientOverviewHintLabel"),
        m_patientOverviewCard,
        m_patientOverviewValueLabel,
        m_patientOverviewStateLabel,
        m_patientOverviewHintLabel);

    createCard(
        QStringLiteral("surgeryOverviewCard"),
        QStringLiteral("surgeryOverviewEyebrowLabel"),
        QStringLiteral("SURGERIES"),
        QStringLiteral("surgeryOverviewValueLabel"),
        QStringLiteral("surgeryOverviewStateLabel"),
        QStringLiteral("surgeryOverviewHintLabel"),
        m_surgeryOverviewCard,
        m_surgeryOverviewValueLabel,
        m_surgeryOverviewStateLabel,
        m_surgeryOverviewHintLabel);

    const int tabIndex = mainLayout->indexOf(ui->tabWidget);
    mainLayout->insertWidget(tabIndex, overviewCardsFrame);
}

void ManagementPageNew::createContentContext(QVBoxLayout* mainLayout)
{
    const int tabIndex = mainLayout->indexOf(ui->tabWidget);
    auto* tabItem = mainLayout->takeAt(tabIndex);
    delete tabItem;

    auto* contentFrame = new QFrame(this);
    contentFrame->setObjectName("managementContentFrame");
    auto* contentLayout = new QVBoxLayout(contentFrame);
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(14);

    auto* entityContextFrame = new QFrame(contentFrame);
    entityContextFrame->setObjectName("entityContextFrame");
    auto* entityContextLayout = new QVBoxLayout(entityContextFrame);
    entityContextLayout->setContentsMargins(14, 14, 14, 14);
    entityContextLayout->setSpacing(6);

    auto* captionLabel = new QLabel(QStringLiteral("当前工作视图"), entityContextFrame);
    captionLabel->setObjectName("currentEntityCaptionLabel");

    auto* headerRowLayout = new QHBoxLayout();
    headerRowLayout->setSpacing(14);

    m_currentEntityValueLabel = new QLabel(QStringLiteral("医生数据视图"), entityContextFrame);
    m_currentEntityValueLabel->setObjectName("currentEntityValueLabel");
    m_currentEntitySummaryLabel = new QLabel(
        QStringLiteral("先筛选并核对当前对象，再继续新增、编辑、删除或刷新。"),
        entityContextFrame);
    m_currentEntitySummaryLabel->setObjectName("currentEntitySummaryLabel");
    m_currentEntitySummaryLabel->setWordWrap(true);

    headerRowLayout->addWidget(m_currentEntityValueLabel);
    headerRowLayout->addWidget(m_currentEntitySummaryLabel, 1);

    m_currentEntityHintLabel = new QLabel(
        QStringLiteral("当前可切换到医生视图，继续查看、检索和维护术者资料。"),
        entityContextFrame);
    m_currentEntityHintLabel->setObjectName("currentEntityHintLabel");
    m_currentEntityHintLabel->setWordWrap(true);

    entityContextLayout->addWidget(captionLabel);
    entityContextLayout->addLayout(headerRowLayout);
    entityContextLayout->addWidget(m_currentEntityHintLabel);

    contentLayout->addWidget(entityContextFrame);
    contentLayout->addWidget(ui->tabWidget, 1);

    mainLayout->insertWidget(tabIndex, contentFrame, 1);
}

void ManagementPageNew::createFlowFrame(QVBoxLayout* mainLayout)
{
    m_managementFlowFrame = new QFrame(this);
    m_managementFlowFrame->setObjectName("managementFlowFrame");
    auto* flowLayout = new QHBoxLayout(m_managementFlowFrame);
    flowLayout->setContentsMargins(18, 16, 18, 16);
    flowLayout->setSpacing(14);

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(3);

    auto* titleLabel = new QLabel(QStringLiteral("下一步：病例工作台"), m_managementFlowFrame);
    titleLabel->setObjectName("managementFlowTitleLabel");
    m_managementFlowHintLabel = new QLabel(
        ThreePagePresentationUtils::buildManagementEntryHint(false),
        m_managementFlowFrame);
    m_managementFlowHintLabel->setObjectName("managementFlowHintLabel");
    m_managementFlowHintLabel->setWordWrap(true);

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(m_managementFlowHintLabel);

    m_enterDashboardButtonSecondary = new QPushButton(QStringLiteral("继续进入病例工作台"), m_managementFlowFrame);
    m_enterDashboardButtonSecondary->setObjectName("enterDashboardButtonSecondary");
    m_enterDashboardButtonSecondary->setMinimumSize(196, 46);
    m_enterDashboardButtonSecondary->setCursor(Qt::PointingHandCursor);
    connect(
        m_enterDashboardButtonSecondary,
        &QPushButton::clicked,
        this,
        &ManagementPageNew::on_enterDashboardButtonSecondary_clicked);

    flowLayout->addLayout(textLayout, 1);
    flowLayout->addWidget(m_enterDashboardButtonSecondary, 0, Qt::AlignVCenter);

    mainLayout->addWidget(m_managementFlowFrame);
}

void ManagementPageNew::refreshOverviewCards()
{
    if (!m_doctorOverviewCard || !m_patientOverviewCard || !m_surgeryOverviewCard) {
        return;
    }

    applyOverviewCard(
        m_doctorOverviewCard,
        m_doctorOverviewValueLabel,
        m_doctorOverviewStateLabel,
        m_doctorOverviewHintLabel,
        QStringLiteral("医生数据"),
        ui->doctorTable->rowCount());

    applyOverviewCard(
        m_patientOverviewCard,
        m_patientOverviewValueLabel,
        m_patientOverviewStateLabel,
        m_patientOverviewHintLabel,
        QStringLiteral("患者数据"),
        ui->patientTable->rowCount());

    applyOverviewCard(
        m_surgeryOverviewCard,
        m_surgeryOverviewValueLabel,
        m_surgeryOverviewStateLabel,
        m_surgeryOverviewHintLabel,
        QStringLiteral("手术任务"),
        ui->surgeryTable->rowCount());
}

void ManagementPageNew::refreshCurrentTabPresentation()
{
    if (!m_currentEntityValueLabel || !m_managementFlowHintLabel || !m_managementFlowFrame) {
        return;
    }

    const QString entityName = currentEntityName();
    const int visibleCount = visibleRowCount(currentEntityTable());
    const QString tone = visibleCount > 0 ? QStringLiteral("ok") : QStringLiteral("warning");

    m_currentEntityValueLabel->setText(currentEntityTitle());
    m_currentEntityValueLabel->setProperty("statusTone", tone);
    m_currentEntitySummaryLabel->setText(currentEntityHint());
    m_currentEntityHintLabel->setText(
        ThreePagePresentationUtils::buildManagementOverviewHint(entityName, visibleCount));
    m_managementFlowHintLabel->setText(
        ThreePagePresentationUtils::buildManagementEntryHint(visibleCount > 0));
    m_managementFlowFrame->setProperty("statusTone", tone);

    polishWidget(m_currentEntityValueLabel);
    polishWidget(m_managementFlowFrame);
}

void ManagementPageNew::applyOverviewCard(
    QFrame* card,
    QLabel* valueLabel,
    QLabel* stateLabel,
    QLabel* hintLabel,
    const QString& entityName,
    int totalCount)
{
    const QString tone = totalCount > 0 ? QStringLiteral("ok") : QStringLiteral("warning");

    valueLabel->setText(ThreePagePresentationUtils::buildManagementOverviewValue(entityName, totalCount));
    stateLabel->setText(totalCount > 0 ? QStringLiteral("已同步") : QStringLiteral("待补充"));
    hintLabel->setText(ThreePagePresentationUtils::buildManagementOverviewHint(entityName, totalCount));

    card->setProperty("statusTone", tone);
    stateLabel->setProperty("statusTone", tone);
    polishWidget(card);
    polishWidget(stateLabel);
}

int ManagementPageNew::visibleRowCount(QTableWidget* table) const
{
    if (!table) {
        return 0;
    }

    int count = 0;
    for (int row = 0; row < table->rowCount(); ++row) {
        if (!table->isRowHidden(row)) {
            ++count;
        }
    }
    return count;
}

QString ManagementPageNew::currentEntityName() const
{
    switch (ui->tabWidget->currentIndex()) {
    case 0:
        return QStringLiteral("医生数据");
    case 1:
        return QStringLiteral("患者数据");
    default:
        return QStringLiteral("手术任务");
    }
}

QString ManagementPageNew::currentEntityTitle() const
{
    switch (ui->tabWidget->currentIndex()) {
    case 0:
        return QStringLiteral("医生数据视图");
    case 1:
        return QStringLiteral("患者数据视图");
    default:
        return QStringLiteral("手术任务视图");
    }
}

QString ManagementPageNew::currentEntityHint() const
{
    switch (ui->tabWidget->currentIndex()) {
    case 0:
        return QStringLiteral("先核对术者资料与科室信息，再进入病例工作台。");
    case 1:
        return QStringLiteral("先核对病例基础资料，为病例工作台提供稳定上下文。");
    default:
        return QStringLiteral("先核对当前手术任务，再把流程收口到病例工作台。");
    }
}

QTableWidget* ManagementPageNew::currentEntityTable() const
{
    switch (ui->tabWidget->currentIndex()) {
    case 0:
        return ui->doctorTable;
    case 1:
        return ui->patientTable;
    default:
        return ui->surgeryTable;
    }
}

void ManagementPageNew::polishWidget(QWidget* widget)
{
    if (!widget) {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void ManagementPageNew::on_backButton_clicked()
{
    emit backRequested();
    emit navigateTo(toInt(PageIndex::ModuleSelection));
}

void ManagementPageNew::on_enterDashboardButton_clicked()
{
    emit enterMainSystemRequested();
    emit navigateTo(toInt(PageIndex::Dashboard));
}

void ManagementPageNew::on_enterDashboardButtonSecondary_clicked()
{
    on_enterDashboardButton_clicked();
}

void ManagementPageNew::on_addDoctorButton_clicked()
{
    showInfo(QStringLiteral("新增医生"), QStringLiteral("新增医生对话框将在此处接入。"));
}

void ManagementPageNew::on_editDoctorButton_clicked()
{
    const int row = ui->doctorTable->currentRow();
    if (row < 0) {
        showWarning(QStringLiteral("编辑医生"), QStringLiteral("请先选择一位医生。"));
        return;
    }

    showInfo(QStringLiteral("编辑医生"), QStringLiteral("编辑医生对话框将在此处接入。"));
}

void ManagementPageNew::on_deleteDoctorButton_clicked()
{
    const int row = ui->doctorTable->currentRow();
    if (row < 0) {
        showWarning(QStringLiteral("删除医生"), QStringLiteral("请先选择一位医生。"));
        return;
    }

    if (showConfirm(QStringLiteral("删除医生"), QStringLiteral("确定要删除这位医生吗？"))) {
        ui->doctorTable->removeRow(row);
        refreshOverviewCards();
        refreshCurrentTabPresentation();
    }
}

void ManagementPageNew::on_refreshDoctorButton_clicked()
{
    loadDoctors();
    refreshOverviewCards();
    refreshCurrentTabPresentation();
}

void ManagementPageNew::on_doctorSearchEdit_textChanged(const QString& text)
{
    for (int row = 0; row < ui->doctorTable->rowCount(); ++row) {
        bool match = false;
        for (int column = 0; column < ui->doctorTable->columnCount(); ++column) {
            auto* item = ui->doctorTable->item(row, column);
            if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }
        ui->doctorTable->setRowHidden(row, !match);
    }

    refreshCurrentTabPresentation();
}

void ManagementPageNew::loadDoctors()
{
    ui->doctorTable->setRowCount(0);

    if (m_identityAppService) {
        const auto doctors = m_identityAppService->listDoctors();
        for (const auto& doctor : doctors) {
            const int row = ui->doctorTable->rowCount();
            ui->doctorTable->insertRow(row);
            ui->doctorTable->setItem(row, 0, new QTableWidgetItem(QString::number(doctor.id)));
            ui->doctorTable->setItem(row, 1, new QTableWidgetItem(doctor.username));
            ui->doctorTable->setItem(row, 2, new QTableWidgetItem(doctor.department));
            ui->doctorTable->setItem(row, 3, new QTableWidgetItem(doctor.jobTitle));
            ui->doctorTable->setItem(row, 4, new QTableWidgetItem(doctor.phone));
            ui->doctorTable->setItem(row, 5, new QTableWidgetItem(doctor.email));
        }
        return;
    }

    const QStringList testDoctors = { QStringLiteral("张伟"), QStringLiteral("李明"), QStringLiteral("王芳") };
    for (int index = 0; index < testDoctors.size(); ++index) {
        const int row = ui->doctorTable->rowCount();
        ui->doctorTable->insertRow(row);
        ui->doctorTable->setItem(row, 0, new QTableWidgetItem(QString::number(index + 1)));
        ui->doctorTable->setItem(row, 1, new QTableWidgetItem(testDoctors.at(index)));
        ui->doctorTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("骨科")));
        ui->doctorTable->setItem(row, 3, new QTableWidgetItem(QStringLiteral("主任医师")));
        ui->doctorTable->setItem(row, 4, new QTableWidgetItem(QStringLiteral("138xxxx%1").arg(1000 + index)));
        ui->doctorTable->setItem(row, 5, new QTableWidgetItem(QStringLiteral("doctor%1@hospital.com").arg(index + 1)));
    }
}

void ManagementPageNew::on_addPatientButton_clicked()
{
    showInfo(QStringLiteral("新增患者"), QStringLiteral("新增患者对话框将在此处接入。"));
}

void ManagementPageNew::on_editPatientButton_clicked()
{
    const int row = ui->patientTable->currentRow();
    if (row < 0) {
        showWarning(QStringLiteral("编辑患者"), QStringLiteral("请先选择一位患者。"));
        return;
    }

    showInfo(QStringLiteral("编辑患者"), QStringLiteral("编辑患者对话框将在此处接入。"));
}

void ManagementPageNew::on_deletePatientButton_clicked()
{
    const int row = ui->patientTable->currentRow();
    if (row < 0) {
        showWarning(QStringLiteral("删除患者"), QStringLiteral("请先选择一位患者。"));
        return;
    }

    if (showConfirm(QStringLiteral("删除患者"), QStringLiteral("确定要删除这位患者吗？"))) {
        ui->patientTable->removeRow(row);
        refreshOverviewCards();
        refreshCurrentTabPresentation();
    }
}

void ManagementPageNew::on_refreshPatientButton_clicked()
{
    loadPatients();
    refreshOverviewCards();
    refreshCurrentTabPresentation();
}

void ManagementPageNew::on_patientSearchEdit_textChanged(const QString& text)
{
    for (int row = 0; row < ui->patientTable->rowCount(); ++row) {
        bool match = false;
        for (int column = 0; column < ui->patientTable->columnCount(); ++column) {
            auto* item = ui->patientTable->item(row, column);
            if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }
        ui->patientTable->setRowHidden(row, !match);
    }

    refreshCurrentTabPresentation();
}

void ManagementPageNew::loadPatients()
{
    ui->patientTable->setRowCount(0);

    if (m_identityAppService) {
        const auto patients = m_identityAppService->listPatients();
        for (const auto& patient : patients) {
            const int row = ui->patientTable->rowCount();
            ui->patientTable->insertRow(row);
            ui->patientTable->setItem(row, 0, new QTableWidgetItem(QString::number(patient.id)));
            ui->patientTable->setItem(row, 1, new QTableWidgetItem(patient.name));
            ui->patientTable->setItem(row, 2, new QTableWidgetItem(patient.gender));
            ui->patientTable->setItem(row, 3, new QTableWidgetItem(QString::number(patient.age)));
            ui->patientTable->setItem(row, 4, new QTableWidgetItem(patient.phone));
            ui->patientTable->setItem(row, 5, new QTableWidgetItem(patient.description));
            ui->patientTable->setItem(row, 6, new QTableWidgetItem(QString()));
        }
        return;
    }

    ui->patientTable->insertRow(0);
    ui->patientTable->setItem(0, 0, new QTableWidgetItem(QStringLiteral("1")));
    ui->patientTable->setItem(0, 1, new QTableWidgetItem(QStringLiteral("患者A")));
    ui->patientTable->setItem(0, 2, new QTableWidgetItem(QStringLiteral("男")));
    ui->patientTable->setItem(0, 3, new QTableWidgetItem(QStringLiteral("45")));
    ui->patientTable->setItem(0, 4, new QTableWidgetItem(QStringLiteral("139xxxx1234")));
    ui->patientTable->setItem(0, 5, new QTableWidgetItem(QStringLiteral("踝关节炎")));
    ui->patientTable->setItem(0, 6, new QTableWidgetItem(QStringLiteral("张伟")));
}

void ManagementPageNew::on_addSurgeryButton_clicked()
{
    showInfo(QStringLiteral("新增手术"), QStringLiteral("新增手术对话框将在此处接入。"));
}

void ManagementPageNew::on_editSurgeryButton_clicked()
{
    const int row = ui->surgeryTable->currentRow();
    if (row < 0) {
        showWarning(QStringLiteral("编辑手术"), QStringLiteral("请先选择一条手术任务。"));
        return;
    }

    showInfo(QStringLiteral("编辑手术"), QStringLiteral("编辑手术对话框将在此处接入。"));
}

void ManagementPageNew::on_deleteSurgeryButton_clicked()
{
    const int row = ui->surgeryTable->currentRow();
    if (row < 0) {
        showWarning(QStringLiteral("删除手术"), QStringLiteral("请先选择一条手术任务。"));
        return;
    }

    if (showConfirm(QStringLiteral("删除手术"), QStringLiteral("确定要删除这条手术任务吗？"))) {
        ui->surgeryTable->removeRow(row);
        refreshOverviewCards();
        refreshCurrentTabPresentation();
    }
}

void ManagementPageNew::on_refreshSurgeryButton_clicked()
{
    loadSurgeries();
    refreshOverviewCards();
    refreshCurrentTabPresentation();
}

void ManagementPageNew::on_surgerySearchEdit_textChanged(const QString& text)
{
    for (int row = 0; row < ui->surgeryTable->rowCount(); ++row) {
        bool match = false;
        for (int column = 0; column < ui->surgeryTable->columnCount(); ++column) {
            auto* item = ui->surgeryTable->item(row, column);
            if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }
        ui->surgeryTable->setRowHidden(row, !match);
    }

    refreshCurrentTabPresentation();
}

void ManagementPageNew::loadSurgeries()
{
    ui->surgeryTable->setRowCount(0);

    if (m_identityAppService) {
        const auto surgeries = m_identityAppService->listSurgeries();
        for (const auto& surgery : surgeries) {
            const int row = ui->surgeryTable->rowCount();
            ui->surgeryTable->insertRow(row);
            ui->surgeryTable->setItem(row, 0, new QTableWidgetItem(QString::number(surgery.id)));
            ui->surgeryTable->setItem(row, 1, new QTableWidgetItem(QString()));
            ui->surgeryTable->setItem(row, 2, new QTableWidgetItem(QString()));
            ui->surgeryTable->setItem(row, 3, new QTableWidgetItem(surgery.name));
            ui->surgeryTable->setItem(row, 4, new QTableWidgetItem(surgery.createdAt.toString(QStringLiteral("yyyy-MM-dd"))));
            ui->surgeryTable->setItem(row, 5, new QTableWidgetItem(surgery.isActive ? QStringLiteral("进行中") : QStringLiteral("已完成")));
        }
        return;
    }

    ui->surgeryTable->insertRow(0);
    ui->surgeryTable->setItem(0, 0, new QTableWidgetItem(QStringLiteral("1")));
    ui->surgeryTable->setItem(0, 1, new QTableWidgetItem(QStringLiteral("患者A")));
    ui->surgeryTable->setItem(0, 2, new QTableWidgetItem(QStringLiteral("张伟")));
    ui->surgeryTable->setItem(0, 3, new QTableWidgetItem(QStringLiteral("踝关节置换")));
    ui->surgeryTable->setItem(0, 4, new QTableWidgetItem(QStringLiteral("2025-01-15")));
    ui->surgeryTable->setItem(0, 5, new QTableWidgetItem(QStringLiteral("已计划")));
}
