#include "PatientInfoWidget.h"
#include "PatientDatabaseService.h"
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

#include <QApplication>
#include <QMessageBox>
#include <QDateTime>
#include <QScrollArea>
#include <QSplitter>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QTextEdit>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QFont>
#include <QColor>
#include <QPalette>
#include <QStyle>
#include <QDebug>

// 常量定义
namespace {
    const int MIN_AGE = 0;
    const int MAX_AGE = 150;
    const int DEFAULT_AGE = 30;
    
    // 样式常量
    const QString VALID_STYLE = "QLineEdit { border: 2px solid #4CAF50; background-color: #E8F5E8; }";
    const QString INVALID_STYLE = "QLineEdit { border: 2px solid #F44336; background-color: #FFEBEE; }";
    const QString NORMAL_STYLE = "QLineEdit { border: 1px solid #CCCCCC; }";
    
    const QString SUCCESS_COLOR = "#4CAF50";
    const QString ERROR_COLOR = "#F44336";
    const QString WARNING_COLOR = "#FF9800";
    const QString INFO_COLOR = "#2196F3";
}

PatientInfoWidget::PatientInfoWidget(QWidget *parent)
    : QWidget(parent)
    , ui(nullptr)
    , m_pluginContext(nullptr)
    , m_isEditMode(false)
    , m_isReadOnly(false)
    , m_formValid(false)
    , m_unsavedChanges(false)
{
    initializeUI();
    setupValidators();
    connectSignals();
    applyMedicalTheme();
    setupFieldHints();
    
    // 设置默认值
    resetForm();
    
    qDebug() << "PatientInfoWidget initialized successfully";
}

PatientInfoWidget::~PatientInfoWidget()
{
    qDebug() << "PatientInfoWidget destroyed";
}

void PatientInfoWidget::setPluginContext(ctkPluginContext* context)
{
    m_pluginContext = context;
    qDebug() << "PatientInfoWidget plugin context set";
}

void PatientInfoWidget::initializeUI()
{
    // 创建主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    // 创建滚动区域
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 创建中央widget
    m_centralWidget = new QWidget();
    m_centralWidget->setMinimumWidth(800);
    
    // 创建中央widget的布局
    QVBoxLayout* centralLayout = new QVBoxLayout(m_centralWidget);
    centralLayout->setContentsMargins(20, 20, 20, 20);
    centralLayout->setSpacing(15);
    
    // 创建表单状态标签
    m_formStatusLabel = new QLabel("请填写患者基本信息");
    m_formStatusLabel->setAlignment(Qt::AlignCenter);
    m_formStatusLabel->setStyleSheet("QLabel { font-weight: bold; padding: 8px; border-radius: 4px; background-color: #E3F2FD; color: #1976D2; }");
    centralLayout->addWidget(m_formStatusLabel);
    
    // 创建分割器用于布局
    m_splitter = new QSplitter(Qt::Horizontal, m_centralWidget);
    
    // 左侧：基本信息和联系方式
    QWidget* leftWidget = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->addWidget(createBasicInfoGroup());
    leftLayout->addWidget(createContactInfoGroup());
    leftLayout->addStretch();
    
    // 右侧：医疗信息
    QWidget* rightWidget = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->addWidget(createMedicalInfoGroup());
    rightLayout->addStretch();
    
    // 添加到分割器
    m_splitter->addWidget(leftWidget);
    m_splitter->addWidget(rightWidget);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({400, 400});
    
    centralLayout->addWidget(m_splitter);
    
    // 创建按钮组
    centralLayout->addWidget(createButtonGroup());
    
    // 设置滚动区域内容
    m_scrollArea->setWidget(m_centralWidget);
    m_mainLayout->addWidget(m_scrollArea);
}

QGroupBox* PatientInfoWidget::createBasicInfoGroup()
{
    QGroupBox* group = new QGroupBox("基本信息");
    group->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; }");
    
    QFormLayout* layout = new QFormLayout(group);
    layout->setLabelAlignment(Qt::AlignRight);
    layout->setSpacing(12);
    
    // 姓名输入
    QWidget* nameWidget = new QWidget();
    QHBoxLayout* nameLayout = new QHBoxLayout(nameWidget);
    nameLayout->setContentsMargins(0, 0, 0, 0);
    
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText("请输入患者真实姓名");
    m_nameEdit->setMaxLength(50);
    
    m_nameValidationLabel = new QLabel();
    m_nameValidationLabel->setFixedSize(20, 20);
    
    nameLayout->addWidget(m_nameEdit);
    nameLayout->addWidget(m_nameValidationLabel);
    
    layout->addRow("姓名 *:", nameWidget);
    
    // 年龄输入
    m_ageSpinBox = new QSpinBox();
    m_ageSpinBox->setRange(MIN_AGE, MAX_AGE);
    m_ageSpinBox->setValue(DEFAULT_AGE);
    m_ageSpinBox->setSuffix(" 岁");
    m_ageSpinBox->setMinimumWidth(120);
    
    layout->addRow("年龄:", m_ageSpinBox);
    
    // 性别选择
    m_genderComboBox = new QComboBox();
    m_genderComboBox->addItems({"男", "女", "其他"});
    m_genderComboBox->setMinimumWidth(120);
    
    layout->addRow("性别:", m_genderComboBox);
    
    // 手机号输入
    QWidget* phoneWidget = new QWidget();
    QHBoxLayout* phoneLayout = new QHBoxLayout(phoneWidget);
    phoneLayout->setContentsMargins(0, 0, 0, 0);
    
    m_phoneEdit = new QLineEdit();
    m_phoneEdit->setPlaceholderText("请输入11位手机号码");
    m_phoneEdit->setMaxLength(11);
    
    m_phoneValidationLabel = new QLabel();
    m_phoneValidationLabel->setFixedSize(20, 20);
    
    phoneLayout->addWidget(m_phoneEdit);
    phoneLayout->addWidget(m_phoneValidationLabel);
    
    layout->addRow("手机号:", phoneWidget);
    
    // 身份证号输入
    QWidget* idCardWidget = new QWidget();
    QHBoxLayout* idCardLayout = new QHBoxLayout(idCardWidget);
    idCardLayout->setContentsMargins(0, 0, 0, 0);
    
    m_idCardEdit = new QLineEdit();
    m_idCardEdit->setPlaceholderText("请输入18位身份证号码");
    m_idCardEdit->setMaxLength(18);
    
    m_idCardValidationLabel = new QLabel();
    m_idCardValidationLabel->setFixedSize(20, 20);
    
    idCardLayout->addWidget(m_idCardEdit);
    idCardLayout->addWidget(m_idCardValidationLabel);
    
    layout->addRow("身份证号:", idCardWidget);
    
    // 登记日期
    m_registrationDateEdit = new QDateTimeEdit(QDateTime::currentDateTime());
    m_registrationDateEdit->setCalendarPopup(true);
    m_registrationDateEdit->setDisplayFormat("yyyy-MM-dd hh:mm");
    m_registrationDateEdit->setMinimumWidth(180);
    
    layout->addRow("登记日期:", m_registrationDateEdit);
    
    // 最后访问日期
    m_lastVisitDateEdit = new QDateTimeEdit();
    m_lastVisitDateEdit->setCalendarPopup(true);
    m_lastVisitDateEdit->setDisplayFormat("yyyy-MM-dd hh:mm");
    m_lastVisitDateEdit->setMinimumWidth(180);
    m_lastVisitDateEdit->setSpecialValueText("未设置");
    
    layout->addRow("最后访问:", m_lastVisitDateEdit);
    
    return group;
}

QGroupBox* PatientInfoWidget::createContactInfoGroup()
{
    QGroupBox* group = new QGroupBox("联系方式");
    group->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; }");
    
    QFormLayout* layout = new QFormLayout(group);
    layout->setLabelAlignment(Qt::AlignRight);
    layout->setSpacing(12);
    
    // 地址输入
    m_addressEdit = new QTextEdit();
    m_addressEdit->setPlaceholderText("请输入详细地址");
    m_addressEdit->setMaximumHeight(80);
    
    layout->addRow("地址:", m_addressEdit);
    
    // 紧急联系人
    QWidget* emergencyWidget = new QWidget();
    QHBoxLayout* emergencyLayout = new QHBoxLayout(emergencyWidget);
    emergencyLayout->setContentsMargins(0, 0, 0, 0);
    
    m_emergencyContactEdit = new QLineEdit();
    m_emergencyContactEdit->setPlaceholderText("紧急联系人姓名");
    
    QPushButton* autoFillButton = new QPushButton("自动填充");
    autoFillButton->setMaximumWidth(80);
    autoFillButton->setToolTip("根据患者信息自动填充联系人");
    connect(autoFillButton, &QPushButton::clicked, 
            this, &PatientInfoWidget::autoFillEmergencyContact);
    
    emergencyLayout->addWidget(m_emergencyContactEdit);
    emergencyLayout->addWidget(autoFillButton);
    
    layout->addRow("紧急联系人:", emergencyWidget);
    
    // 紧急联系电话
    m_emergencyPhoneEdit = new QLineEdit();
    m_emergencyPhoneEdit->setPlaceholderText("紧急联系人电话");
    m_emergencyPhoneEdit->setMaxLength(11);
    
    layout->addRow("紧急电话:", m_emergencyPhoneEdit);
    
    return group;
}

QGroupBox* PatientInfoWidget::createMedicalInfoGroup()
{
    QGroupBox* group = new QGroupBox("医疗信息");
    group->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; }");
    
    QVBoxLayout* layout = new QVBoxLayout(group);
    layout->setSpacing(15);
    
    // 病史
    QLabel* historyLabel = new QLabel("病史:");
    historyLabel->setStyleSheet("QLabel { font-weight: bold; }");
    m_medicalHistoryEdit = new QTextEdit();
    m_medicalHistoryEdit->setPlaceholderText("请详细描述患者的既往病史、家族病史等相关医疗信息");
    m_medicalHistoryEdit->setMaximumHeight(100);
    
    layout->addWidget(historyLabel);
    layout->addWidget(m_medicalHistoryEdit);
    
    // 过敏史
    QLabel* allergiesLabel = new QLabel("过敏史:");
    allergiesLabel->setStyleSheet("QLabel { font-weight: bold; color: #F44336; }");
    m_allergiesEdit = new QTextEdit();
    m_allergiesEdit->setPlaceholderText("请详细描述已知的药物过敏、食物过敏等信息");
    m_allergiesEdit->setMaximumHeight(80);
    m_allergiesEdit->setStyleSheet("QTextEdit { border: 2px solid #F44336; }");
    
    layout->addWidget(allergiesLabel);
    layout->addWidget(m_allergiesEdit);
    
    // 当前用药
    QLabel* medicationsLabel = new QLabel("当前用药:");
    medicationsLabel->setStyleSheet("QLabel { font-weight: bold; }");
    m_medicationsEdit = new QTextEdit();
    m_medicationsEdit->setPlaceholderText("请详细记录患者当前正在使用的药物");
    m_medicationsEdit->setMaximumHeight(80);
    
    layout->addWidget(medicationsLabel);
    layout->addWidget(m_medicationsEdit);
    
    // 备注
    QLabel* notesLabel = new QLabel("备注:");
    notesLabel->setStyleSheet("QLabel { font-weight: bold; }");
    m_notesEdit = new QTextEdit();
    m_notesEdit->setPlaceholderText("其他需要记录的重要信息");
    m_notesEdit->setMaximumHeight(100);
    
    layout->addWidget(notesLabel);
    layout->addWidget(m_notesEdit);
    
    return group;
}

QWidget* PatientInfoWidget::createButtonGroup()
{
    QWidget* buttonWidget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(buttonWidget);
    layout->setContentsMargins(0, 10, 0, 0);
    
    // 添加弹性空间
    layout->addStretch();
    
    // 重置按钮
    m_resetButton = new QPushButton("重置");
    m_resetButton->setMinimumSize(100, 35);
    m_resetButton->setStyleSheet(
        "QPushButton { "
        "    background-color: #FFC107; "
        "    border: none; "
        "    border-radius: 4px; "
        "    color: white; "
        "    font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #FFB300; } "
        "QPushButton:pressed { background-color: #FFA000; }"
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
    
    // 保存按钮
    m_saveButton = new QPushButton("保存");
    m_saveButton->setMinimumSize(100, 35);
    m_saveButton->setStyleSheet(
        "QPushButton { "
        "    background-color: #4CAF50; "
        "    border: none; "
        "    border-radius: 4px; "
        "    color: white; "
        "    font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #45A049; } "
        "QPushButton:pressed { background-color: #3D8B40; } "
        "QPushButton:disabled { "
        "    background-color: #CCCCCC; "
        "    color: #666666; "
        "}"
    );
    
    layout->addWidget(m_resetButton);
    layout->addWidget(m_cancelButton);
    layout->addWidget(m_saveButton);
    
    return buttonWidget;
}

void PatientInfoWidget::setupValidators()
{
    // 手机号验证器
    QRegularExpression phoneRegex("^1[3-9]\\d{9}$");
    m_phoneValidator = new QRegularExpressionValidator(phoneRegex, this);
    m_phoneEdit->setValidator(m_phoneValidator);
    
    // 身份证号验证器
    QRegularExpression idCardRegex("^[1-9]\\d{5}(18|19|20)\\d{2}((0[1-9])|(1[0-2]))(([0-2][1-9])|10|20|30|31)\\d{3}[0-9Xx]$");
    m_idCardValidator = new QRegularExpressionValidator(idCardRegex, this);
    m_idCardEdit->setValidator(m_idCardValidator);
}

void PatientInfoWidget::connectSignals()
{
    // 按钮信号连接
    connect(m_saveButton, &QPushButton::clicked, this, &PatientInfoWidget::savePatient);
    connect(m_cancelButton, &QPushButton::clicked, this, &PatientInfoWidget::cancelOperation);
    connect(m_resetButton, &QPushButton::clicked, this, &PatientInfoWidget::resetForm);
    
    // 表单验证信号连接
    connect(m_nameEdit, &QLineEdit::textChanged, this, &PatientInfoWidget::onNameChanged);
    connect(m_phoneEdit, &QLineEdit::textChanged, this, &PatientInfoWidget::onPhoneChanged);
    connect(m_idCardEdit, &QLineEdit::textChanged, this, &PatientInfoWidget::onIdCardChanged);
    connect(m_ageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &PatientInfoWidget::onAgeChanged);
    
    // 实时验证
    connect(m_nameEdit, &QLineEdit::textChanged, this, &PatientInfoWidget::validateCurrentForm);
    connect(m_phoneEdit, &QLineEdit::textChanged, this, &PatientInfoWidget::validateCurrentForm);
    connect(m_idCardEdit, &QLineEdit::textChanged, this, &PatientInfoWidget::validateCurrentForm);
    
    // 表单修改检测
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this]() { m_unsavedChanges = true; });
    connect(m_phoneEdit, &QLineEdit::textChanged, this, [this]() { m_unsavedChanges = true; });
    connect(m_idCardEdit, &QLineEdit::textChanged, this, [this]() { m_unsavedChanges = true; });
    connect(m_ageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, [this]() { m_unsavedChanges = true; });
}

void PatientInfoWidget::applyMedicalTheme()
{
    // 设置整体样式
    setStyleSheet(
        "QWidget { "
        "    font-family: 'Microsoft YaHei', Arial, sans-serif; "
        "    font-size: 12px; "
        "} "
        "QGroupBox { "
        "    border: 2px solid #E0E0E0; "
        "    border-radius: 8px; "
        "    margin-top: 10px; "
        "    padding-top: 15px; "
        "    background-color: #FAFAFA; "
        "} "
        "QGroupBox::title { "
        "    subcontrol-origin: margin; "
        "    subcontrol-position: top left; "
        "    padding: 5px 15px; "
        "    background-color: white; "
        "    border: 1px solid #E0E0E0; "
        "    border-radius: 4px; "
        "    color: #2E7D32; "
        "} "
        "QLineEdit, QSpinBox, QComboBox { "
        "    padding: 8px 12px; "
        "    border: 1px solid #CCCCCC; "
        "    border-radius: 4px; "
        "    background-color: white; "
        "    selection-background-color: #E3F2FD; "
        "} "
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus { "
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
        "QDateTimeEdit { "
        "    padding: 8px 12px; "
        "    border: 1px solid #CCCCCC; "
        "    border-radius: 4px; "
        "    background-color: white; "
        "} "
        "QDateTimeEdit:focus { "
        "    border: 2px solid #2196F3; "
        "} "
        "QScrollArea { "
        "    background-color: transparent; "
        "    border: none; "
        "} "
        "QSplitter::handle { "
        "    background-color: #E0E0E0; "
        "    width: 2px; "
        "} "
    );
}

void PatientInfoWidget::setupFieldHints()
{
    // 设置工具提示
    m_nameEdit->setToolTip("患者的真实姓名，用于身份识别");
    m_ageSpinBox->setToolTip("患者的实际年龄");
    m_genderComboBox->setToolTip("患者的性别信息");
    m_phoneEdit->setToolTip("11位手机号码，用于联系和通知");
    m_idCardEdit->setToolTip("18位身份证号码，用于身份验证");
    m_addressEdit->setToolTip("患者的详细居住地址");
    m_emergencyContactEdit->setToolTip("紧急情况下的联系人");
    m_emergencyPhoneEdit->setToolTip("紧急联系人的电话号码");
    m_medicalHistoryEdit->setToolTip("患者的既往病史和家族病史");
    m_allergiesEdit->setToolTip("已知的药物、食物等过敏信息");
    m_medicationsEdit->setToolTip("患者当前正在使用的药物");
    m_notesEdit->setToolTip("其他需要记录的重要医疗信息");
}

// 验证和状态更新方法
void PatientInfoWidget::onNameChanged()
{
    QString name = m_nameEdit->text().trimmed();
    bool isValid = !name.isEmpty() && name.length() >= 2;
    
    updateValidationDisplay("name", isValid, 
        isValid ? "姓名格式正确" : "姓名不能为空且至少2个字符");
}

void PatientInfoWidget::onPhoneChanged()
{
    QString phone = m_phoneEdit->text();
    int pos = 0;
    bool isValid = phone.isEmpty() || m_phoneValidator->validate(phone, pos) == QValidator::Acceptable;
    
    updateValidationDisplay("phone", isValid,
        phone.isEmpty() ? "手机号码为可选项" : (isValid ? "手机号格式正确" : "手机号格式不正确"));
}

void PatientInfoWidget::onIdCardChanged()
{
    QString idCard = m_idCardEdit->text();
    int pos = 0;
    bool isValid = idCard.isEmpty() || m_idCardValidator->validate(idCard, pos) == QValidator::Acceptable;
    
    updateValidationDisplay("idcard", isValid,
        idCard.isEmpty() ? "身份证号为可选项" : (isValid ? "身份证号格式正确" : "身份证号格式不正确"));
}

void PatientInfoWidget::onAgeChanged()
{
    int age = m_ageSpinBox->value();
    bool isValid = age >= MIN_AGE && age <= MAX_AGE;
    
    if (!isValid) {
        m_ageSpinBox->setValue(qBound(MIN_AGE, age, MAX_AGE));
    }
}

void PatientInfoWidget::validateCurrentForm()
{
    bool wasValid = m_formValid;
    
    // 检查必填字段
    bool nameValid = !m_nameEdit->text().trimmed().isEmpty();
    QString phoneText = m_phoneEdit->text();
    int pos = 0;
    bool phoneValid = phoneText.isEmpty() || 
                     m_phoneValidator->validate(phoneText, pos) == QValidator::Acceptable;
    QString idCardText = m_idCardEdit->text();
    int idPos = 0;
    bool idCardValid = idCardText.isEmpty() || 
                      m_idCardValidator->validate(idCardText, idPos) == QValidator::Acceptable;
    
    m_formValid = nameValid && phoneValid && idCardValid;
    
    // 更新保存按钮状态
    m_saveButton->setEnabled(m_formValid && !m_isReadOnly);
    
    // 更新表单状态显示
    if (m_formValid) {
        m_formStatusLabel->setText("表单填写完整，可以保存");
        m_formStatusLabel->setStyleSheet("QLabel { font-weight: bold; padding: 8px; border-radius: 4px; background-color: #E8F5E8; color: #2E7D32; }");
    } else {
        m_formStatusLabel->setText("请完善必填信息后再保存");
        m_formStatusLabel->setStyleSheet("QLabel { font-weight: bold; padding: 8px; border-radius: 4px; background-color: #FFEBEE; color: #C62828; }");
    }
    
    // 发送验证状态变化信号
    if (wasValid != m_formValid) {
        emit validationStateChanged(m_formValid);
    }
}

void PatientInfoWidget::updateValidationDisplay(const QString& field, bool isValid, const QString& message)
{
    QLabel* label = nullptr;
    QLineEdit* edit = nullptr;
    
    if (field == "name") {
        label = m_nameValidationLabel;
        edit = m_nameEdit;
    } else if (field == "phone") {
        label = m_phoneValidationLabel;
        edit = m_phoneEdit;
    } else if (field == "idcard") {
        label = m_idCardValidationLabel;
        edit = m_idCardEdit;
    }
    
    if (label && edit) {
        if (isValid) {
            label->setText("✓");
            label->setStyleSheet(QString("QLabel { color: %1; font-weight: bold; }").arg(SUCCESS_COLOR));
            edit->setStyleSheet(VALID_STYLE);
        } else {
            label->setText("✗");
            label->setStyleSheet(QString("QLabel { color: %1; font-weight: bold; }").arg(ERROR_COLOR));
            edit->setStyleSheet(INVALID_STYLE);
        }
        
        label->setToolTip(message);
    }
}

void PatientInfoWidget::autoFillEmergencyContact()
{
    QString patientName = m_nameEdit->text().trimmed();
    if (!patientName.isEmpty()) {
        // 简单的自动填充逻辑，实际应用中可能需要更复杂的逻辑
        if (m_emergencyContactEdit->text().trimmed().isEmpty()) {
            m_emergencyContactEdit->setText(patientName + "的家属");
        }
        
        // 如果有电话号码，可以给出提示
        QString phone = m_phoneEdit->text().trimmed();
        if (!phone.isEmpty() && m_emergencyPhoneEdit->text().trimmed().isEmpty()) {
            QMessageBox::information(this, "提示", 
                "建议填写与患者不同的紧急联系电话\n患者手机号: " + phone);
            m_emergencyPhoneEdit->setFocus();
        }
    } else {
        QMessageBox::warning(this, "提示", "请先填写患者姓名");
        m_nameEdit->setFocus();
    }
}

// 主要操作方法
void PatientInfoWidget::setPatientInfo(const PatientInfo& patient)
{
    m_currentPatient = patient;
    m_isEditMode = (patient.patientId > 0);
    
    // 填充表单数据
    m_nameEdit->setText(patient.name);
    m_ageSpinBox->setValue(patient.age > 0 ? patient.age : DEFAULT_AGE);
    m_genderComboBox->setCurrentText(patient.gender.isEmpty() ? "男" : patient.gender);
    m_phoneEdit->setText(patient.phone);
    m_idCardEdit->setText(patient.idCard);
    m_addressEdit->setPlainText(patient.address);
    m_emergencyContactEdit->setText(patient.emergencyContact);
    m_emergencyPhoneEdit->setText(patient.emergencyPhone);
    m_medicalHistoryEdit->setPlainText(patient.medicalHistory);
    m_allergiesEdit->setPlainText(patient.allergies);
    m_medicationsEdit->setPlainText(patient.currentMedications);
    m_notesEdit->setPlainText(patient.notes);
    
    // 设置日期
    if (patient.registrationDate.isValid()) {
        m_registrationDateEdit->setDateTime(patient.registrationDate);
    } else {
        m_registrationDateEdit->setDateTime(QDateTime::currentDateTime());
    }
    
    if (patient.lastVisitDate.isValid()) {
        m_lastVisitDateEdit->setDateTime(patient.lastVisitDate);
    } else {
        m_lastVisitDateEdit->clear();
    }
    
    // 更新UI状态
    m_saveButton->setText(m_isEditMode ? "更新" : "保存");
    m_formStatusLabel->setText(m_isEditMode ? "正在编辑患者信息" : "正在添加新患者");
    
    // 重新验证表单
    validateCurrentForm();
    m_unsavedChanges = false;
    
    qDebug() << "Patient info loaded for" << (m_isEditMode ? "editing" : "creation");
}

PatientInfo PatientInfoWidget::getPatientInfo() const
{
    PatientInfo patient = m_currentPatient;
    
    patient.name = m_nameEdit->text().trimmed();
    patient.age = m_ageSpinBox->value();
    patient.gender = m_genderComboBox->currentText();
    patient.phone = m_phoneEdit->text().trimmed();
    patient.idCard = m_idCardEdit->text().trimmed();
    patient.address = m_addressEdit->toPlainText().trimmed();
    patient.emergencyContact = m_emergencyContactEdit->text().trimmed();
    patient.emergencyPhone = m_emergencyPhoneEdit->text().trimmed();
    patient.medicalHistory = m_medicalHistoryEdit->toPlainText().trimmed();
    patient.allergies = m_allergiesEdit->toPlainText().trimmed();
    patient.currentMedications = m_medicationsEdit->toPlainText().trimmed();
    patient.notes = m_notesEdit->toPlainText().trimmed();
    patient.registrationDate = m_registrationDateEdit->dateTime();
    patient.lastVisitDate = m_lastVisitDateEdit->dateTime();
    
    return patient;
}

void PatientInfoWidget::clearForm()
{
    m_nameEdit->clear();
    m_ageSpinBox->setValue(DEFAULT_AGE);
    m_genderComboBox->setCurrentIndex(0);
    m_phoneEdit->clear();
    m_idCardEdit->clear();
    m_addressEdit->clear();
    m_emergencyContactEdit->clear();
    m_emergencyPhoneEdit->clear();
    m_medicalHistoryEdit->clear();
    m_allergiesEdit->clear();
    m_medicationsEdit->clear();
    m_notesEdit->clear();
    m_registrationDateEdit->setDateTime(QDateTime::currentDateTime());
    m_lastVisitDateEdit->clear();
    
    // 清除验证状态
    m_nameValidationLabel->clear();
    m_phoneValidationLabel->clear();
    m_idCardValidationLabel->clear();
    
    // 重置样式
    m_nameEdit->setStyleSheet(NORMAL_STYLE);
    m_phoneEdit->setStyleSheet(NORMAL_STYLE);
    m_idCardEdit->setStyleSheet(NORMAL_STYLE);
    
    m_currentPatient = PatientInfo();
    m_isEditMode = false;
    m_unsavedChanges = false;
    
    validateCurrentForm();
}

void PatientInfoWidget::setReadOnly(bool readOnly)
{
    m_isReadOnly = readOnly;
    
    // 设置所有输入控件的只读状态
    m_nameEdit->setReadOnly(readOnly);
    m_ageSpinBox->setReadOnly(readOnly);
    m_genderComboBox->setEnabled(!readOnly);
    m_phoneEdit->setReadOnly(readOnly);
    m_idCardEdit->setReadOnly(readOnly);
    m_addressEdit->setReadOnly(readOnly);
    m_emergencyContactEdit->setReadOnly(readOnly);
    m_emergencyPhoneEdit->setReadOnly(readOnly);
    m_medicalHistoryEdit->setReadOnly(readOnly);
    m_allergiesEdit->setReadOnly(readOnly);
    m_medicationsEdit->setReadOnly(readOnly);
    m_notesEdit->setReadOnly(readOnly);
    m_registrationDateEdit->setReadOnly(readOnly);
    m_lastVisitDateEdit->setReadOnly(readOnly);
    
    // 设置按钮状态
    m_saveButton->setEnabled(!readOnly && m_formValid);
    m_resetButton->setEnabled(!readOnly);
    
    if (readOnly) {
        m_formStatusLabel->setText("只读模式 - 无法编辑");
        m_formStatusLabel->setStyleSheet("QLabel { font-weight: bold; padding: 8px; border-radius: 4px; background-color: #FFF3E0; color: #E65100; }");
    } else {
        validateCurrentForm();
    }
}

bool PatientInfoWidget::validateForm()
{
    validateCurrentForm();
    return m_formValid;
}

void PatientInfoWidget::savePatient()
{
    if (!validateForm()) {
        QMessageBox::warning(this, "验证失败", "请检查并完善表单信息后再保存");
        return;
    }
    
    PatientDatabaseService* service = getDatabaseService();
    if (!service) {
        QMessageBox::critical(this, "系统错误", "无法连接到数据库服务，请检查系统配置");
        return;
    }
    
    PatientInfo patient = getPatientInfo();
    bool success = false;
    QString operation;
    
    try {
        if (m_isEditMode) {
            success = service->updatePatient(patient);
            operation = "更新";
        } else {
            success = service->addPatient(patient);
            operation = "添加";
        }
        
        if (success) {
            showResultMessage(true, QString("患者信息%1成功").arg(operation));
            emit patientSaved(patient, !m_isEditMode);
            
            if (!m_isEditMode) {
                // 新患者添加成功后清空表单
                clearForm();
            } else {
                m_unsavedChanges = false;
            }
        } else {
            showResultMessage(false, QString("患者信息%1失败，请重试").arg(operation));
        }
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "操作异常", 
            QString("患者信息%1时发生异常:\n%2").arg(operation, e.what()));
    } catch (...) {
        QMessageBox::critical(this, "未知错误", 
            QString("患者信息%1时发生未知错误").arg(operation));
    }
}

void PatientInfoWidget::cancelOperation()
{
    if (m_unsavedChanges) {
        int ret = QMessageBox::question(this, "确认取消", 
            "当前表单有未保存的更改，确定要取消吗？",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        
        if (ret != QMessageBox::Yes) {
            return;
        }
    }
    
    emit operationCancelled();
}

void PatientInfoWidget::resetForm()
{
    if (m_unsavedChanges) {
        int ret = QMessageBox::question(this, "确认重置", 
            "当前表单有未保存的更改，确定要重置吗？",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        
        if (ret != QMessageBox::Yes) {
            return;
        }
    }
    
    if (m_isEditMode) {
        // 编辑模式：恢复原始数据
        setPatientInfo(m_currentPatient);
    } else {
        // 新增模式：清空表单
        clearForm();
    }
}

PatientDatabaseService* PatientInfoWidget::getDatabaseService()
{
    if (!m_pluginContext) {
        qWarning() << "Plugin context not set";
        return nullptr;
    }
    
    ctkServiceReference serviceRef = m_pluginContext->getServiceReference<PatientDatabaseService>();
    if (!serviceRef) {
        qWarning() << "PatientDatabaseService not found";
        return nullptr;
    }
    
    return m_pluginContext->getService<PatientDatabaseService>(serviceRef);
}

void PatientInfoWidget::showResultMessage(bool success, const QString& message)
{
    if (success) {
        QMessageBox::information(this, "操作成功", message);
    } else {
        QMessageBox::warning(this, "操作失败", message);
    }
}
