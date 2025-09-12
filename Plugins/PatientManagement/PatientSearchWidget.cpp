#include "PatientSearchWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QDebug>

PatientSearchWidget::PatientSearchWidget(QWidget *parent)
    : QDialog(parent)
    , ui(nullptr)
{
    setWindowTitle("患者高级搜索");
    setWindowModality(Qt::WindowModal);
    setMinimumSize(500, 600);
    resize(600, 700);
    
    initializeUI();
    connectSignals();
    applyStyle();
    
    // 设置默认值
    clearSearchCriteria();
    
    qDebug() << "PatientSearchWidget initialized";
}

PatientSearchWidget::~PatientSearchWidget()
{
    qDebug() << "PatientSearchWidget destroyed";
}

void PatientSearchWidget::initializeUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    m_mainLayout->setSpacing(15);
    
    // 添加标题
    QLabel* titleLabel = new QLabel("高级搜索条件");
    titleLabel->setStyleSheet("QLabel { font-size: 16px; font-weight: bold; color: #2E7D32; }");
    titleLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(titleLabel);
    
    // 创建各个搜索组
    m_mainLayout->addWidget(createBasicSearchGroup());
    m_mainLayout->addWidget(createMedicalSearchGroup());
    m_mainLayout->addWidget(createDateRangeGroup());
    
    // 添加弹性空间
    m_mainLayout->addStretch();
    
    // 创建按钮组
    m_mainLayout->addWidget(createButtonGroup());
}

QGroupBox* PatientSearchWidget::createBasicSearchGroup()
{
    QGroupBox* group = new QGroupBox("基本信息");
    QFormLayout* layout = new QFormLayout(group);
    layout->setSpacing(10);
    
    // 姓名搜索
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText("输入患者姓名（支持模糊搜索）");
    layout->addRow("姓名:", m_nameEdit);
    
    // 手机号搜索
    m_phoneEdit = new QLineEdit();
    m_phoneEdit->setPlaceholderText("输入11位手机号码");
    m_phoneEdit->setMaxLength(11);
    layout->addRow("手机号:", m_phoneEdit);
    
    // 身份证号搜索
    m_idCardEdit = new QLineEdit();
    m_idCardEdit->setPlaceholderText("输入18位身份证号码");
    m_idCardEdit->setMaxLength(18);
    layout->addRow("身份证号:", m_idCardEdit);
    
    // 年龄范围
    QWidget* ageWidget = new QWidget();
    QHBoxLayout* ageLayout = new QHBoxLayout(ageWidget);
    ageLayout->setContentsMargins(0, 0, 0, 0);
    ageLayout->setSpacing(10);
    
    m_ageMinSpinBox = new QSpinBox();
    m_ageMinSpinBox->setRange(0, 150);
    m_ageMinSpinBox->setSpecialValueText("不限");
    m_ageMinSpinBox->setSuffix(" 岁");
    
    QLabel* ageToLabel = new QLabel("至");
    
    m_ageMaxSpinBox = new QSpinBox();
    m_ageMaxSpinBox->setRange(0, 150);
    m_ageMaxSpinBox->setSpecialValueText("不限");
    m_ageMaxSpinBox->setSuffix(" 岁");
    
    ageLayout->addWidget(m_ageMinSpinBox);
    ageLayout->addWidget(ageToLabel);
    ageLayout->addWidget(m_ageMaxSpinBox);
    ageLayout->addStretch();
    
    layout->addRow("年龄范围:", ageWidget);
    
    // 性别选择
    m_genderCombo = new QComboBox();
    m_genderCombo->addItems({"不限", "男", "女", "其他"});
    layout->addRow("性别:", m_genderCombo);
    
    return group;
}

QGroupBox* PatientSearchWidget::createMedicalSearchGroup()
{
    QGroupBox* group = new QGroupBox("医疗信息");
    QFormLayout* layout = new QFormLayout(group);
    layout->setSpacing(10);
    
    // 病史关键词搜索
    m_medicalHistoryEdit = new QTextEdit();
    m_medicalHistoryEdit->setPlaceholderText("输入病史相关关键词，如：高血压、糖尿病、手术史等");
    m_medicalHistoryEdit->setMaximumHeight(80);
    layout->addRow("病史关键词:", m_medicalHistoryEdit);
    
    return group;
}

QGroupBox* PatientSearchWidget::createDateRangeGroup()
{
    QGroupBox* group = new QGroupBox("日期范围");
    QFormLayout* layout = new QFormLayout(group);
    layout->setSpacing(10);
    
    // 登记日期范围
    QWidget* regDateWidget = new QWidget();
    QHBoxLayout* regDateLayout = new QHBoxLayout(regDateWidget);
    regDateLayout->setContentsMargins(0, 0, 0, 0);
    regDateLayout->setSpacing(10);
    
    m_regDateStart = new QDateTimeEdit();
    m_regDateStart->setCalendarPopup(true);
    m_regDateStart->setDisplayFormat("yyyy-MM-dd");
    m_regDateStart->setSpecialValueText("不限制");
    m_regDateStart->setDate(QDate());
    
    QLabel* regDateToLabel = new QLabel("至");
    
    m_regDateEnd = new QDateTimeEdit();
    m_regDateEnd->setCalendarPopup(true);
    m_regDateEnd->setDisplayFormat("yyyy-MM-dd");
    m_regDateEnd->setSpecialValueText("不限制");
    m_regDateEnd->setDate(QDate());
    
    regDateLayout->addWidget(m_regDateStart);
    regDateLayout->addWidget(regDateToLabel);
    regDateLayout->addWidget(m_regDateEnd);
    regDateLayout->addStretch();
    
    layout->addRow("登记日期:", regDateWidget);
    
    return group;
}

QWidget* PatientSearchWidget::createButtonGroup()
{
    QWidget* buttonWidget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(buttonWidget);
    layout->setContentsMargins(0, 10, 0, 0);
    
    layout->addStretch();
    
    // 清除按钮
    m_clearButton = new QPushButton("清除条件");
    m_clearButton->setMinimumSize(100, 35);
    m_clearButton->setStyleSheet(
        "QPushButton { "
        "    background-color: #FF9800; "
        "    border: none; "
        "    border-radius: 4px; "
        "    color: white; "
        "    font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #F57C00; } "
        "QPushButton:pressed { background-color: #EF6C00; }"
    );
    
    // 取消按钮
    m_cancelButton = new QPushButton("取消");
    m_cancelButton->setMinimumSize(100, 35);
    m_cancelButton->setStyleSheet(
        "QPushButton { "
        "    background-color: #9E9E9E; "
        "    border: none; "
        "    border-radius: 4px; "
        "    color: white; "
        "    font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #757575; } "
        "QPushButton:pressed { background-color: #616161; }"
    );
    
    // 搜索按钮
    m_searchButton = new QPushButton("开始搜索");
    m_searchButton->setMinimumSize(100, 35);
    m_searchButton->setStyleSheet(
        "QPushButton { "
        "    background-color: #4CAF50; "
        "    border: none; "
        "    border-radius: 4px; "
        "    color: white; "
        "    font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #45A049; } "
        "QPushButton:pressed { background-color: #3D8B40; }"
    );
    
    layout->addWidget(m_clearButton);
    layout->addWidget(m_cancelButton);
    layout->addWidget(m_searchButton);
    
    return buttonWidget;
}

void PatientSearchWidget::connectSignals()
{
    connect(m_searchButton, &QPushButton::clicked, this, &PatientSearchWidget::onSearchClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &PatientSearchWidget::onClearClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &PatientSearchWidget::onCancelClicked);
    
    // 设置默认按钮
    m_searchButton->setDefault(true);
    m_searchButton->setAutoDefault(true);
}

void PatientSearchWidget::applyStyle()
{
    setStyleSheet(
        "QDialog { "
        "    background-color: #FAFAFA; "
        "    font-family: 'Microsoft YaHei', Arial, sans-serif; "
        "    font-size: 12px; "
        "} "
        "QGroupBox { "
        "    border: 2px solid #E0E0E0; "
        "    border-radius: 8px; "
        "    margin-top: 10px; "
        "    padding-top: 15px; "
        "    background-color: white; "
        "} "
        "QGroupBox::title { "
        "    subcontrol-origin: margin; "
        "    subcontrol-position: top left; "
        "    padding: 5px 15px; "
        "    background-color: #E3F2FD; "
        "    border: 1px solid #2196F3; "
        "    border-radius: 4px; "
        "    color: #1976D2; "
        "    font-weight: bold; "
        "} "
        "QLineEdit, QSpinBox, QComboBox, QDateTimeEdit { "
        "    padding: 8px 12px; "
        "    border: 1px solid #CCCCCC; "
        "    border-radius: 4px; "
        "    background-color: white; "
        "    selection-background-color: #E3F2FD; "
        "} "
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QDateTimeEdit:focus { "
        "    border: 2px solid #2196F3; "
        "    outline: none; "
        "} "
        "QTextEdit { "
        "    padding: 8px; "
        "    border: 1px solid #CCCCCC; "
        "    border-radius: 4px; "
        "    background-color: white; "
        "    selection-background-color: #E3F2FD; "
        "} "
        "QTextEdit:focus { "
        "    border: 2px solid #2196F3; "
        "    outline: none; "
        "} "
    );
}

PatientSearchCriteria PatientSearchWidget::getSearchCriteria() const
{
    PatientSearchCriteria criteria;
    
    // 基本信息
    criteria.nameFilter = m_nameEdit->text().trimmed();
    criteria.phoneFilter = m_phoneEdit->text().trimmed();
    criteria.idCardFilter = m_idCardEdit->text().trimmed();
    
    // 年龄范围
    if (m_ageMinSpinBox->value() > 0) {
        criteria.ageMin = m_ageMinSpinBox->value();
    }
    if (m_ageMaxSpinBox->value() > 0) {
        criteria.ageMax = m_ageMaxSpinBox->value();
    }
    
    // 性别
    if (m_genderCombo->currentIndex() > 0) {
        criteria.genderFilter = m_genderCombo->currentText();
    }
    
    // 医疗信息
    criteria.medicalHistoryFilter = m_medicalHistoryEdit->toPlainText().trimmed();
    
    // 日期范围
    if (m_regDateStart->date().isValid() && !m_regDateStart->date().isNull()) {
        criteria.registrationDateStart = m_regDateStart->dateTime();
    }
    if (m_regDateEnd->date().isValid() && !m_regDateEnd->date().isNull()) {
        criteria.registrationDateEnd = m_regDateEnd->dateTime();
    }
    
    return criteria;
}

void PatientSearchWidget::setSearchCriteria(const PatientSearchCriteria& criteria)
{
    // 基本信息
    m_nameEdit->setText(criteria.nameFilter);
    m_phoneEdit->setText(criteria.phoneFilter);
    m_idCardEdit->setText(criteria.idCardFilter);
    
    // 年龄范围
    m_ageMinSpinBox->setValue(criteria.ageMin >= 0 ? criteria.ageMin : 0);
    m_ageMaxSpinBox->setValue(criteria.ageMax >= 0 ? criteria.ageMax : 0);
    
    // 性别
    if (!criteria.genderFilter.isEmpty()) {
        int index = m_genderCombo->findText(criteria.genderFilter);
        if (index >= 0) {
            m_genderCombo->setCurrentIndex(index);
        }
    } else {
        m_genderCombo->setCurrentIndex(0);
    }
    
    // 医疗信息
    m_medicalHistoryEdit->setPlainText(criteria.medicalHistoryFilter);
    
    // 日期范围
    if (criteria.registrationDateStart.isValid()) {
        m_regDateStart->setDateTime(criteria.registrationDateStart);
    } else {
        m_regDateStart->setDate(QDate());
    }
    
    if (criteria.registrationDateEnd.isValid()) {
        m_regDateEnd->setDateTime(criteria.registrationDateEnd);
    } else {
        m_regDateEnd->setDate(QDate());
    }
}

void PatientSearchWidget::clearSearchCriteria()
{
    // 清空所有输入控件
    m_nameEdit->clear();
    m_phoneEdit->clear();
    m_idCardEdit->clear();
    m_ageMinSpinBox->setValue(0);
    m_ageMaxSpinBox->setValue(0);
    m_genderCombo->setCurrentIndex(0);
    m_medicalHistoryEdit->clear();
    m_regDateStart->setDate(QDate());
    m_regDateEnd->setDate(QDate());
    
    // 设置焦点到第一个输入控件
    m_nameEdit->setFocus();
}

void PatientSearchWidget::onSearchClicked()
{
    PatientSearchCriteria criteria = getSearchCriteria();
    
    // 检查是否有有效的搜索条件
    if (criteria.isEmpty()) {
        // 可以显示提示信息，或者直接允许搜索所有患者
    }
    
    emit searchRequested(criteria);
    accept(); // 关闭对话框
}

void PatientSearchWidget::onClearClicked()
{
    clearSearchCriteria();
}

void PatientSearchWidget::onCancelClicked()
{
    reject(); // 关闭对话框，不发送搜索信号
}
