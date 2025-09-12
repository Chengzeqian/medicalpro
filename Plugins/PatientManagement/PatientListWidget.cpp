#include "PatientListWidget.h"
#include "PatientDatabaseService.h"
#include "PatientInfoWidget.h"
#include "PatientSearchWidget.h"
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QSplitter>
#include <QDialog>

PatientListWidget::PatientListWidget(QWidget *parent)
    : QWidget(parent)
    , ui(nullptr)
    , m_pluginContext(nullptr)
    , m_currentPage(0)
    , m_pageSize(50)
    , m_totalCount(0)
    , m_patientInfoWidget(nullptr)
    , m_searchWidget(nullptr)
{
    initializeUI();
    connectSignals();
    applyTheme();
    
    // 创建搜索定时器
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(300); // 300ms延迟
    connect(m_searchTimer, &QTimer::timeout, this, &PatientListWidget::onSearchTimerTimeout);
    
    updateButtonStates();
    
    qDebug() << "PatientListWidget initialized successfully";
}

PatientListWidget::~PatientListWidget()
{
    if (m_patientInfoWidget) {
        delete m_patientInfoWidget;
    }
    if (m_searchWidget) {
        delete m_searchWidget;
    }
    qDebug() << "PatientListWidget destroyed";
}

void PatientListWidget::setPluginContext(ctkPluginContext* context)
{
    m_pluginContext = context;
    
    // 连接数据库服务信号
    PatientDatabaseService* service = getDatabaseService();
    if (service) {
        connect(service, &PatientDatabaseService::patientAdded,
                this, &PatientListWidget::onPatientAdded);
        connect(service, &PatientDatabaseService::patientUpdated,
                this, &PatientListWidget::onPatientUpdated);
        connect(service, &PatientDatabaseService::patientDeleted,
                this, &PatientListWidget::onPatientDeleted);
        connect(service, &PatientDatabaseService::databaseError,
                this, &PatientListWidget::onDatabaseError);
    }
    
    qDebug() << "PatientListWidget plugin context set";
}

void PatientListWidget::initializeUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    // 创建各个组件
    m_toolBar = createToolBar();
    m_searchBar = createSearchBar();
    createPatientTable();
    m_statusBar = createStatusBar();
    createContextMenu();
    
    // 添加到主布局
    m_mainLayout->addWidget(m_toolBar);
    m_mainLayout->addWidget(m_searchBar);
    m_mainLayout->addWidget(m_patientTable);
    m_mainLayout->addWidget(m_statusBar);
    
    setupTableStyle();
}

QWidget* PatientListWidget::createToolBar()
{
    QWidget* toolbar = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    
    // 创建按钮
    m_addButton = new QPushButton("添加患者");
    m_addButton->setIcon(QIcon(":/icons/add.png"));
    m_addButton->setMinimumSize(100, 35);
    
    m_editButton = new QPushButton("编辑");
    m_editButton->setIcon(QIcon(":/icons/edit.png"));
    m_editButton->setMinimumSize(80, 35);
    
    m_deleteButton = new QPushButton("删除");
    m_deleteButton->setIcon(QIcon(":/icons/delete.png"));
    m_deleteButton->setMinimumSize(80, 35);
    
    m_viewButton = new QPushButton("查看详情");
    m_viewButton->setIcon(QIcon(":/icons/view.png"));
    m_viewButton->setMinimumSize(100, 35);
    
    m_refreshButton = new QPushButton("刷新");
    m_refreshButton->setIcon(QIcon(":/icons/refresh.png"));
    m_refreshButton->setMinimumSize(80, 35);
    
    m_exportButton = new QPushButton("导出");
    m_exportButton->setIcon(QIcon(":/icons/export.png"));
    m_exportButton->setMinimumSize(80, 35);
    
    m_advancedSearchButton = new QPushButton("高级搜索");
    m_advancedSearchButton->setIcon(QIcon(":/icons/search.png"));
    m_advancedSearchButton->setMinimumSize(100, 35);
    
    // 添加到布局
    layout->addWidget(m_addButton);
    layout->addWidget(m_editButton);
    layout->addWidget(m_deleteButton);
    layout->addWidget(m_viewButton);
    layout->addSpacing(20);
    layout->addWidget(m_refreshButton);
    layout->addWidget(m_exportButton);
    layout->addSpacing(20);
    layout->addWidget(m_advancedSearchButton);
    layout->addStretch();
    
    return toolbar;
}

QWidget* PatientListWidget::createSearchBar()
{
    QWidget* searchBar = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(searchBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    
    // 快速搜索
    QLabel* searchLabel = new QLabel("快速搜索:");
    m_quickSearchEdit = new QLineEdit();
    m_quickSearchEdit->setPlaceholderText("输入姓名、手机号或身份证号");
    m_quickSearchEdit->setMinimumWidth(200);
    
    // 性别过滤
    QLabel* genderLabel = new QLabel("性别:");
    m_genderFilterCombo = new QComboBox();
    m_genderFilterCombo->addItems({"全部", "男", "女", "其他"});
    m_genderFilterCombo->setMinimumWidth(80);
    
    // 年龄范围过滤
    QLabel* ageLabel = new QLabel("年龄:");
    m_ageRangeCombo = new QComboBox();
    m_ageRangeCombo->addItems({
        "全部", "0-18岁", "18-30岁", "30-50岁", "50-70岁", "70岁以上"
    });
    m_ageRangeCombo->setMinimumWidth(100);
    
    // 清除过滤按钮
    m_clearFiltersButton = new QPushButton("清除过滤");
    m_clearFiltersButton->setMinimumSize(80, 30);
    
    layout->addWidget(searchLabel);
    layout->addWidget(m_quickSearchEdit);
    layout->addSpacing(20);
    layout->addWidget(genderLabel);
    layout->addWidget(m_genderFilterCombo);
    layout->addSpacing(10);
    layout->addWidget(ageLabel);
    layout->addWidget(m_ageRangeCombo);
    layout->addSpacing(20);
    layout->addWidget(m_clearFiltersButton);
    layout->addStretch();
    
    return searchBar;
}

void PatientListWidget::createPatientTable()
{
    m_patientTable = new QTableWidget(this);
    m_patientTable->setColumnCount(COL_COUNT);
    
    // 设置表头
    QStringList headers;
    headers << "ID" << "姓名" << "年龄" << "性别" << "手机号" << "登记日期" << "最后访问";
    m_patientTable->setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    m_patientTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_patientTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_patientTable->setAlternatingRowColors(true);
    m_patientTable->setSortingEnabled(true);
    m_patientTable->setShowGrid(false);
    m_patientTable->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 设置列宽
    QHeaderView* header = m_patientTable->horizontalHeader();
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);
    
    // 设置各列的初始宽度
    m_patientTable->setColumnWidth(COL_ID, 60);
    m_patientTable->setColumnWidth(COL_NAME, 120);
    m_patientTable->setColumnWidth(COL_AGE, 60);
    m_patientTable->setColumnWidth(COL_GENDER, 60);
    m_patientTable->setColumnWidth(COL_PHONE, 120);
    m_patientTable->setColumnWidth(COL_REGISTRATION_DATE, 120);
    
    // 隐藏ID列
    m_patientTable->setColumnHidden(COL_ID, true);
}

QWidget* PatientListWidget::createStatusBar()
{
    QWidget* statusBar = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(statusBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    
    m_statusLabel = new QLabel("就绪");
    m_countLabel = new QLabel("患者总数: 0");
    m_loadingProgress = new QProgressBar();
    m_loadingProgress->setVisible(false);
    m_loadingProgress->setRange(0, 0); // 无限进度条
    
    layout->addWidget(m_statusLabel);
    layout->addStretch();
    layout->addWidget(m_loadingProgress);
    layout->addWidget(m_countLabel);
    
    return statusBar;
}

void PatientListWidget::createContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    m_editAction = new QAction("编辑患者", this);
    m_deleteAction = new QAction("删除患者", this);
    m_viewAction = new QAction("查看详情", this);
    m_copyInfoAction = new QAction("复制信息", this);
    
    m_editAction->setIcon(QIcon(":/icons/edit.png"));
    m_deleteAction->setIcon(QIcon(":/icons/delete.png"));
    m_viewAction->setIcon(QIcon(":/icons/view.png"));
    m_copyInfoAction->setIcon(QIcon(":/icons/copy.png"));
    
    m_contextMenu->addAction(m_editAction);
    m_contextMenu->addAction(m_deleteAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_viewAction);
    m_contextMenu->addAction(m_copyInfoAction);
    
    // 连接菜单动作
    connect(m_editAction, &QAction::triggered, this, &PatientListWidget::editSelectedPatient);
    connect(m_deleteAction, &QAction::triggered, this, &PatientListWidget::deleteSelectedPatients);
    connect(m_viewAction, &QAction::triggered, this, &PatientListWidget::viewPatientDetails);
    connect(m_copyInfoAction, &QAction::triggered, this, [this]() {
        PatientInfo patient = getCurrentSelectedPatient();
        if (patient.patientId > 0) {
            QString info = QString("姓名: %1\n年龄: %2\n性别: %3\n手机号: %4")
                          .arg(patient.name)
                          .arg(patient.age)
                          .arg(patient.gender)
                          .arg(patient.phone);
            QApplication::clipboard()->setText(info);
            m_statusLabel->setText("患者信息已复制到剪贴板");
        }
    });
}

void PatientListWidget::setupTableStyle()
{
    m_patientTable->setStyleSheet(
        "QTableWidget { "
        "    background-color: white; "
        "    border: 1px solid #E0E0E0; "
        "    gridline-color: #F0F0F0; "
        "    selection-background-color: #E3F2FD; "
        "} "
        "QTableWidget::item { "
        "    padding: 8px; "
        "    border: none; "
        "} "
        "QTableWidget::item:selected { "
        "    background-color: #2196F3; "
        "    color: white; "
        "} "
        "QTableWidget::item:hover { "
        "    background-color: #F5F5F5; "
        "} "
        "QHeaderView::section { "
        "    background-color: #FAFAFA; "
        "    padding: 8px; "
        "    border: none; "
        "    border-bottom: 1px solid #E0E0E0; "
        "    font-weight: bold; "
        "    color: #424242; "
        "} "
        "QHeaderView::section:hover { "
        "    background-color: #EEEEEE; "
        "}"
    );
}

void PatientListWidget::connectSignals()
{
    // 工具栏按钮信号
    connect(m_addButton, &QPushButton::clicked, this, &PatientListWidget::addNewPatient);
    connect(m_editButton, &QPushButton::clicked, this, &PatientListWidget::editSelectedPatient);
    connect(m_deleteButton, &QPushButton::clicked, this, &PatientListWidget::deleteSelectedPatients);
    connect(m_viewButton, &QPushButton::clicked, this, &PatientListWidget::viewPatientDetails);
    connect(m_refreshButton, &QPushButton::clicked, this, &PatientListWidget::refreshPatientList);
    connect(m_exportButton, &QPushButton::clicked, this, &PatientListWidget::exportPatientData);
    connect(m_advancedSearchButton, &QPushButton::clicked, this, &PatientListWidget::showAdvancedSearch);
    
    // 搜索栏信号
    connect(m_quickSearchEdit, &QLineEdit::textChanged, this, &PatientListWidget::onQuickSearchTextChanged);
    connect(m_genderFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PatientListWidget::onGenderFilterChanged);
    connect(m_ageRangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PatientListWidget::onAgeFilterChanged);
    connect(m_clearFiltersButton, &QPushButton::clicked, this, [this]() {
        m_quickSearchEdit->clear();
        m_genderFilterCombo->setCurrentIndex(0);
        m_ageRangeCombo->setCurrentIndex(0);
        refreshPatientList();
    });
    
    // 表格信号
    connect(m_patientTable, &QTableWidget::itemSelectionChanged, 
            this, &PatientListWidget::onTableSelectionChanged);
    connect(m_patientTable, &QTableWidget::itemDoubleClicked,
            this, &PatientListWidget::onTableDoubleClicked);
    connect(m_patientTable, &QTableWidget::customContextMenuRequested,
            this, &PatientListWidget::onTableContextMenu);
}

void PatientListWidget::applyTheme()
{
    setStyleSheet(
        "QWidget { "
        "    font-family: 'Microsoft YaHei', Arial, sans-serif; "
        "    font-size: 12px; "
        "} "
        "QPushButton { "
        "    background-color: #2196F3; "
        "    border: none; "
        "    border-radius: 4px; "
        "    color: white; "
        "    font-weight: bold; "
        "    padding: 8px 16px; "
        "} "
        "QPushButton:hover { "
        "    background-color: #1976D2; "
        "} "
        "QPushButton:pressed { "
        "    background-color: #1565C0; "
        "} "
        "QPushButton:disabled { "
        "    background-color: #CCCCCC; "
        "    color: #666666; "
        "} "
        "QLineEdit, QComboBox { "
        "    padding: 6px 10px; "
        "    border: 1px solid #CCCCCC; "
        "    border-radius: 4px; "
        "    background-color: white; "
        "} "
        "QLineEdit:focus, QComboBox:focus { "
        "    border: 2px solid #2196F3; "
        "} "
    );
}

void PatientListWidget::refreshPatientList()
{
    showLoadingProgress(true);
    m_statusLabel->setText("正在加载患者列表...");
    
    PatientDatabaseService* service = getDatabaseService();
    if (!service) {
        showLoadingProgress(false);
        m_statusLabel->setText("数据库服务不可用");
        return;
    }
    
    try {
        PatientSearchCriteria criteria = buildSearchCriteria();
        QList<PatientInfo> patients;
        
        if (criteria.isEmpty()) {
            patients = service->getAllPatients();
        } else {
            patients = service->searchPatients(criteria);
        }
        
        loadPatientsToTable(patients);
        m_currentPatients = patients;
        
        showLoadingProgress(false);
        updateStatusDisplay();
        
    } catch (const std::exception& e) {
        showLoadingProgress(false);
        m_statusLabel->setText("加载失败: " + QString(e.what()));
        QMessageBox::critical(this, "加载错误", "患者列表加载失败:\n" + QString(e.what()));
    }
}

void PatientListWidget::searchPatients(const PatientSearchCriteria& criteria)
{
    m_currentCriteria = criteria;
    refreshPatientList();
}

void PatientListWidget::loadPatientsToTable(const QList<PatientInfo>& patients)
{
    m_patientTable->setRowCount(patients.size());
    
    for (int i = 0; i < patients.size(); ++i) {
        updateTableRow(i, patients[i]);
    }
    
    m_totalCount = patients.size();
    emit patientListUpdated(m_totalCount);
}

void PatientListWidget::updateTableRow(int row, const PatientInfo& patient)
{
    // 设置各列数据
    m_patientTable->setItem(row, COL_ID, new QTableWidgetItem(QString::number(patient.patientId)));
    m_patientTable->setItem(row, COL_NAME, new QTableWidgetItem(patient.name));
    m_patientTable->setItem(row, COL_AGE, new QTableWidgetItem(QString::number(patient.age)));
    m_patientTable->setItem(row, COL_GENDER, new QTableWidgetItem(patient.gender));
    m_patientTable->setItem(row, COL_PHONE, new QTableWidgetItem(patient.phone));
    
    QString regDate = patient.registrationDate.isValid() ? 
                     patient.registrationDate.toString("yyyy-MM-dd") : "未设置";
    m_patientTable->setItem(row, COL_REGISTRATION_DATE, new QTableWidgetItem(regDate));
    
    QString lastVisit = patient.lastVisitDate.isValid() ? 
                       patient.lastVisitDate.toString("yyyy-MM-dd") : "从未访问";
    m_patientTable->setItem(row, COL_LAST_VISIT, new QTableWidgetItem(lastVisit));
    
    // 设置行数据
    m_patientTable->item(row, COL_ID)->setData(Qt::UserRole, patient.patientId);
}

PatientInfo PatientListWidget::getPatientFromTableRow(int row) const
{
    if (row < 0 || row >= m_currentPatients.size()) {
        return PatientInfo();
    }
    
    return m_currentPatients[row];
}

PatientInfo PatientListWidget::getCurrentSelectedPatient() const
{
    int currentRow = m_patientTable->currentRow();
    return getPatientFromTableRow(currentRow);
}

void PatientListWidget::selectPatient(int patientId)
{
    int row = findPatientRowById(patientId);
    if (row >= 0) {
        m_patientTable->selectRow(row);
        m_patientTable->setCurrentCell(row, COL_NAME);
    }
}

int PatientListWidget::findPatientRowById(int patientId) const
{
    for (int i = 0; i < m_patientTable->rowCount(); ++i) {
        QTableWidgetItem* idItem = m_patientTable->item(i, COL_ID);
        if (idItem && idItem->data(Qt::UserRole).toInt() == patientId) {
            return i;
        }
    }
    return -1;
}

PatientSearchCriteria PatientListWidget::buildSearchCriteria() const
{
    PatientSearchCriteria criteria;
    
    // 快速搜索文本
    QString searchText = m_quickSearchEdit->text().trimmed();
    if (!searchText.isEmpty()) {
        // 判断搜索文本类型
        if (searchText.contains(QRegularExpression("^1[3-9]\\d{9}$"))) {
            criteria.phoneFilter = searchText;
        } else if (searchText.length() == 18 && searchText.contains(QRegularExpression("^\\d{17}[\\dXx]$"))) {
            criteria.idCardFilter = searchText;
        } else {
            criteria.nameFilter = searchText;
        }
    }
    
    // 性别过滤
    if (m_genderFilterCombo->currentIndex() > 0) {
        criteria.genderFilter = m_genderFilterCombo->currentText();
    }
    
    // 年龄范围过滤
    if (m_ageRangeCombo->currentIndex() > 0) {
        QString ageRange = m_ageRangeCombo->currentText();
        if (ageRange == "0-18岁") {
            criteria.ageMin = 0;
            criteria.ageMax = 18;
        } else if (ageRange == "18-30岁") {
            criteria.ageMin = 18;
            criteria.ageMax = 30;
        } else if (ageRange == "30-50岁") {
            criteria.ageMin = 30;
            criteria.ageMax = 50;
        } else if (ageRange == "50-70岁") {
            criteria.ageMin = 50;
            criteria.ageMax = 70;
        } else if (ageRange == "70岁以上") {
            criteria.ageMin = 70;
            criteria.ageMax = 150;
        }
    }
    
    return criteria;
}

// 槽函数实现
void PatientListWidget::onTableSelectionChanged()
{
    PatientInfo selectedPatient = getCurrentSelectedPatient();
    m_selectedPatient = selectedPatient;
    
    updateButtonStates();
    
    if (selectedPatient.patientId > 0) {
        emit patientSelectionChanged(selectedPatient);
    }
}

void PatientListWidget::onTableDoubleClicked()
{
    PatientInfo patient = getCurrentSelectedPatient();
    if (patient.patientId > 0) {
        emit patientDoubleClicked(patient);
        viewPatientDetails();
    }
}

void PatientListWidget::onTableContextMenu(const QPoint& position)
{
    if (m_patientTable->itemAt(position)) {
        m_contextMenu->exec(m_patientTable->mapToGlobal(position));
    }
}

void PatientListWidget::onQuickSearchTextChanged()
{
    m_searchTimer->start();
}

void PatientListWidget::onSearchTimerTimeout()
{
    refreshPatientList();
}

void PatientListWidget::onGenderFilterChanged()
{
    refreshPatientList();
}

void PatientListWidget::onAgeFilterChanged()
{
    refreshPatientList();
}

void PatientListWidget::onPageChanged()
{
    // 分页功能预留，当前版本暂未实现
    refreshPatientList();
}

void PatientListWidget::updateStatusDisplay()
{
    m_countLabel->setText(QString("患者总数: %1").arg(m_totalCount));
    
    int selectedCount = m_patientTable->selectionModel()->selectedRows().count();
    if (selectedCount > 0) {
        m_statusLabel->setText(QString("已选择 %1 个患者").arg(selectedCount));
    } else {
        m_statusLabel->setText("就绪");
    }
}

void PatientListWidget::updateButtonStates()
{
    int selectedCount = m_patientTable->selectionModel()->selectedRows().count();
    bool hasSelection = selectedCount > 0;
    bool singleSelection = selectedCount == 1;
    
    m_editButton->setEnabled(singleSelection);
    m_deleteButton->setEnabled(hasSelection);
    m_viewButton->setEnabled(singleSelection);
    m_exportButton->setEnabled(m_totalCount > 0);
}

void PatientListWidget::showLoadingProgress(bool show)
{
    m_loadingProgress->setVisible(show);
}

PatientDatabaseService* PatientListWidget::getDatabaseService()
{
    if (!m_pluginContext) {
        return nullptr;
    }
    
    ctkServiceReference serviceRef = m_pluginContext->getServiceReference<PatientDatabaseService>();
    if (!serviceRef) {
        return nullptr;
    }
    
    return m_pluginContext->getService<PatientDatabaseService>(serviceRef);
}

// 主要操作槽函数
void PatientListWidget::addNewPatient()
{
    if (!m_patientInfoWidget) {
        m_patientInfoWidget = new PatientInfoWidget(this);
        m_patientInfoWidget->setPluginContext(m_pluginContext);
        
        connect(m_patientInfoWidget, &PatientInfoWidget::patientSaved,
                this, [this](const PatientInfo&, bool isNew) {
                    if (isNew) {
                        refreshPatientList();
                    }
                    m_patientInfoWidget->hide();
                });
        
        connect(m_patientInfoWidget, &PatientInfoWidget::operationCancelled,
                this, [this]() {
                    m_patientInfoWidget->hide();
                });
    }
    
    m_patientInfoWidget->clearForm();
    m_patientInfoWidget->setReadOnly(false);
    m_patientInfoWidget->show();
    m_patientInfoWidget->raise();
}

void PatientListWidget::editSelectedPatient()
{
    PatientInfo patient = getCurrentSelectedPatient();
    if (patient.patientId <= 0) {
        QMessageBox::warning(this, "提示", "请选择要编辑的患者");
        return;
    }
    
    emit editPatientRequested(patient);
    
    if (!m_patientInfoWidget) {
        m_patientInfoWidget = new PatientInfoWidget(this);
        m_patientInfoWidget->setPluginContext(m_pluginContext);
        
        connect(m_patientInfoWidget, &PatientInfoWidget::patientSaved,
                this, [this](const PatientInfo&, bool) {
                    refreshPatientList();
                    m_patientInfoWidget->hide();
                });
        
        connect(m_patientInfoWidget, &PatientInfoWidget::operationCancelled,
                this, [this]() {
                    m_patientInfoWidget->hide();
                });
    }
    
    m_patientInfoWidget->setPatientInfo(patient);
    m_patientInfoWidget->setReadOnly(false);
    m_patientInfoWidget->show();
    m_patientInfoWidget->raise();
}

void PatientListWidget::deleteSelectedPatients()
{
    QList<int> selectedPatientIds = getSelectedPatientIds();
    if (selectedPatientIds.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择要删除的患者");
        return;
    }
    
    if (!confirmDelete(selectedPatientIds.size())) {
        return;
    }
    
    PatientDatabaseService* service = getDatabaseService();
    if (!service) {
        QMessageBox::critical(this, "系统错误", "无法连接到数据库服务");
        return;
    }
    
    int successCount = 0;
    int failureCount = 0;
    
    showLoadingProgress(true);
    
    for (int patientId : selectedPatientIds) {
        if (service->deletePatient(patientId)) {
            successCount++;
        } else {
            failureCount++;
        }
    }
    
    showLoadingProgress(false);
    
    QString message;
    if (failureCount == 0) {
        message = QString("成功删除 %1 个患者").arg(successCount);
        QMessageBox::information(this, "删除成功", message);
    } else {
        message = QString("删除完成: 成功 %1 个，失败 %2 个").arg(successCount).arg(failureCount);
        QMessageBox::warning(this, "删除结果", message);
    }
    
    refreshPatientList();
}

void PatientListWidget::viewPatientDetails()
{
    PatientInfo patient = getCurrentSelectedPatient();
    if (patient.patientId <= 0) {
        QMessageBox::warning(this, "提示", "请选择要查看的患者");
        return;
    }
    
    if (!m_patientInfoWidget) {
        m_patientInfoWidget = new PatientInfoWidget(this);
        m_patientInfoWidget->setPluginContext(m_pluginContext);
        
        connect(m_patientInfoWidget, &PatientInfoWidget::operationCancelled,
                this, [this]() {
                    m_patientInfoWidget->hide();
                });
    }
    
    m_patientInfoWidget->setPatientInfo(patient);
    m_patientInfoWidget->setReadOnly(true);
    m_patientInfoWidget->show();
    m_patientInfoWidget->raise();
}

void PatientListWidget::exportPatientData()
{
    if (m_totalCount == 0) {
        QMessageBox::information(this, "提示", "没有患者数据可以导出");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出患者数据", 
        QString("患者数据_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "CSV文件 (*.csv)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出失败", "无法创建文件: " + fileName);
        return;
    }
    
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    
    // 写入CSV头
    stream << "ID,姓名,年龄,性别,手机号,身份证号,地址,登记日期,最后访问\n";
    
    // 写入数据
    for (const PatientInfo& patient : m_currentPatients) {
        stream << patient.patientId << ","
               << "\"" << patient.name << "\","
               << patient.age << ","
               << "\"" << patient.gender << "\","
               << "\"" << patient.phone << "\","
               << "\"" << patient.idCard << "\","
               << "\"" << QString(patient.address).replace("\"", "\"\"") << "\","
               << "\"" << patient.registrationDate.toString("yyyy-MM-dd hh:mm:ss") << "\","
               << "\"" << (patient.lastVisitDate.isValid() ? 
                          patient.lastVisitDate.toString("yyyy-MM-dd hh:mm:ss") : "从未访问") << "\"\n";
    }
    
    file.close();
    
    QMessageBox::information(this, "导出成功", 
        QString("成功导出 %1 条患者数据到文件:\n%2").arg(m_currentPatients.size()).arg(fileName));
}

void PatientListWidget::showAdvancedSearch()
{
    // 这里可以创建高级搜索对话框
    QMessageBox::information(this, "高级搜索", "高级搜索功能开发中...");
}

QList<int> PatientListWidget::getSelectedPatientIds() const
{
    QList<int> ids;
    QList<QTableWidgetItem*> selectedItems = m_patientTable->selectedItems();
    
    QSet<int> rows;
    for (QTableWidgetItem* item : selectedItems) {
        rows.insert(item->row());
    }
    
    for (int row : rows) {
        QTableWidgetItem* idItem = m_patientTable->item(row, COL_ID);
        if (idItem) {
            ids.append(idItem->data(Qt::UserRole).toInt());
        }
    }
    
    return ids;
}

bool PatientListWidget::confirmDelete(int patientCount)
{
    QString message;
    if (patientCount == 1) {
        message = "确定要删除选中的患者吗？\n\n此操作将同时删除该患者的所有相关记录（影像、手术记录等），且无法恢复。";
    } else {
        message = QString("确定要删除选中的 %1 个患者吗？\n\n此操作将同时删除这些患者的所有相关记录（影像、手术记录等），且无法恢复。").arg(patientCount);
    }
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认删除");
    msgBox.setText(message);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setIcon(QMessageBox::Warning);
    int ret = msgBox.exec();
    
    return ret == QMessageBox::Yes;
}

// 数据库事件处理
void PatientListWidget::onPatientAdded(const PatientInfo& patient)
{
    Q_UNUSED(patient)
    // 可以选择性地添加新行而不是完全刷新
    refreshPatientList();
}

void PatientListWidget::onPatientUpdated(const PatientInfo& patient)
{
    // 查找并更新对应的行
    int row = findPatientRowById(patient.patientId);
    if (row >= 0) {
        updateTableRow(row, patient);
        // 更新内存中的患者信息
        for (int i = 0; i < m_currentPatients.size(); ++i) {
            if (m_currentPatients[i].patientId == patient.patientId) {
                m_currentPatients[i] = patient;
                break;
            }
        }
    }
}

void PatientListWidget::onPatientDeleted(int patientId)
{
    int row = findPatientRowById(patientId);
    if (row >= 0) {
        m_patientTable->removeRow(row);
        // 从内存列表中移除
        for (int i = 0; i < m_currentPatients.size(); ++i) {
            if (m_currentPatients[i].patientId == patientId) {
                m_currentPatients.removeAt(i);
                break;
            }
        }
        m_totalCount--;
        updateStatusDisplay();
    }
}

void PatientListWidget::onDatabaseError(const QString& error)
{
    m_statusLabel->setText("数据库错误: " + error);
    qWarning() << "PatientListWidget database error:" << error;
}
