#ifndef PATIENT_INFO_WIDGET_H
#define PATIENT_INFO_WIDGET_H

#include "PatientDataStructures.h"
#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QTextEdit>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QScrollArea>
#include <QRegularExpressionValidator>

QT_BEGIN_NAMESPACE
namespace Ui {
class PatientInfoWidget;
}
QT_END_NAMESPACE

class PatientDatabaseService;
class ctkPluginContext;

/**
 * @brief 患者信息录入和编辑界面
 * 
 * 提供专业的医疗软件界面，用于患者基本信息的录入、编辑和显示。
 * 集成了数据验证、实时反馈和CTK医疗主题样式。
 */
class PatientInfoWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit PatientInfoWidget(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~PatientInfoWidget();

    /**
     * @brief 设置插件上下文（用于访问CTK服务）
     * @param context CTK插件上下文
     */
    void setPluginContext(ctkPluginContext* context);
    
    /**
     * @brief 设置编辑模式的患者信息
     * @param patient 要编辑的患者信息
     */
    void setPatientInfo(const PatientInfo& patient);
    
    /**
     * @brief 获取当前表单的患者信息
     * @return 患者信息
     */
    PatientInfo getPatientInfo() const;
    
    /**
     * @brief 清空所有表单内容
     */
    void clearForm();
    
    /**
     * @brief 设置表单为只读模式
     * @param readOnly 是否只读
     */
    void setReadOnly(bool readOnly);
    
    /**
     * @brief 验证当前表单数据
     * @return 验证通过返回true，否则返回false
     */
    bool validateForm();

public slots:
    /**
     * @brief 保存患者信息
     */
    void savePatient();
    
    /**
     * @brief 取消操作
     */
    void cancelOperation();
    
    /**
     * @brief 重置表单
     */
    void resetForm();

signals:
    /**
     * @brief 患者保存成功信号
     * @param patient 保存的患者信息
     * @param isNewPatient 是否为新患者
     */
    void patientSaved(const PatientInfo& patient, bool isNewPatient);
    
    /**
     * @brief 操作取消信号
     */
    void operationCancelled();
    
    /**
     * @brief 表单验证状态改变信号
     * @param isValid 是否有效
     */
    void validationStateChanged(bool isValid);

private slots:
    /**
     * @brief 处理姓名输入变化
     */
    void onNameChanged();
    
    /**
     * @brief 处理手机号输入变化
     */
    void onPhoneChanged();
    
    /**
     * @brief 处理身份证号输入变化
     */
    void onIdCardChanged();
    
    /**
     * @brief 处理年龄变化
     */
    void onAgeChanged();
    
    /**
     * @brief 实时验证表单
     */
    void validateCurrentForm();
    
    /**
     * @brief 自动填充紧急联系人信息
     */
    void autoFillEmergencyContact();

private:
    /**
     * @brief 初始化UI界面
     */
    void initializeUI();
    
    /**
     * @brief 创建基本信息组
     * @return 基本信息组框
     */
    QGroupBox* createBasicInfoGroup();
    
    /**
     * @brief 创建联系方式组
     * @return 联系方式组框
     */
    QGroupBox* createContactInfoGroup();
    
    /**
     * @brief 创建医疗信息组
     * @return 医疗信息组框
     */
    QGroupBox* createMedicalInfoGroup();
    
    /**
     * @brief 创建按钮组
     * @return 按钮组widget
     */
    QWidget* createButtonGroup();
    
    /**
     * @brief 设置数据验证器
     */
    void setupValidators();
    
    /**
     * @brief 连接信号和槽
     */
    void connectSignals();
    
    /**
     * @brief 应用医疗主题样式
     */
    void applyMedicalTheme();
    
    /**
     * @brief 设置字段提示信息
     */
    void setupFieldHints();
    
    /**
     * @brief 更新验证状态显示
     * @param field 字段名称
     * @param isValid 是否有效
     * @param message 提示消息
     */
    void updateValidationDisplay(const QString& field, bool isValid, const QString& message = QString());
    
    /**
     * @brief 获取数据库服务
     * @return 数据库服务指针，失败返回nullptr
     */
    PatientDatabaseService* getDatabaseService();
    
    /**
     * @brief 显示操作结果消息
     * @param success 是否成功
     * @param message 消息内容
     */
    void showResultMessage(bool success, const QString& message);

private:
    Ui::PatientInfoWidget *ui;             // UI文件指针
    
    // CTK集成
    ctkPluginContext* m_pluginContext;     // 插件上下文
    
    // 主要布局组件
    QScrollArea* m_scrollArea;             // 滚动区域
    QWidget* m_centralWidget;              // 中央widget
    QVBoxLayout* m_mainLayout;             // 主布局
    QSplitter* m_splitter;                 // 分割器
    
    // 基本信息组件
    QLineEdit* m_nameEdit;                 // 姓名输入
    QSpinBox* m_ageSpinBox;                // 年龄选择
    QComboBox* m_genderComboBox;           // 性别选择
    QLineEdit* m_phoneEdit;                // 手机号输入
    QLineEdit* m_idCardEdit;               // 身份证号输入
    QTextEdit* m_addressEdit;              // 地址输入
    
    // 联系方式组件
    QLineEdit* m_emergencyContactEdit;     // 紧急联系人
    QLineEdit* m_emergencyPhoneEdit;       // 紧急联系电话
    
    // 医疗信息组件
    QTextEdit* m_medicalHistoryEdit;       // 病史输入
    QTextEdit* m_allergiesEdit;            // 过敏史输入
    QTextEdit* m_medicationsEdit;          // 当前用药输入
    QTextEdit* m_notesEdit;                // 备注输入
    
    // 日期时间组件
    QDateTimeEdit* m_registrationDateEdit; // 登记日期
    QDateTimeEdit* m_lastVisitDateEdit;    // 最后访问日期
    
    // 按钮组件
    QPushButton* m_saveButton;             // 保存按钮
    QPushButton* m_cancelButton;           // 取消按钮
    QPushButton* m_resetButton;            // 重置按钮
    
    // 验证状态标签
    QLabel* m_nameValidationLabel;         // 姓名验证状态
    QLabel* m_phoneValidationLabel;        // 手机号验证状态
    QLabel* m_idCardValidationLabel;       // 身份证验证状态
    QLabel* m_formStatusLabel;             // 表单状态标签
    
    // 验证器
    QRegularExpressionValidator* m_phoneValidator;    // 手机号验证器
    QRegularExpressionValidator* m_idCardValidator;   // 身份证号验证器
    
    // 状态变量
    PatientInfo m_currentPatient;          // 当前编辑的患者
    bool m_isEditMode;                     // 是否编辑模式
    bool m_isReadOnly;                     // 是否只读模式
    bool m_formValid;                      // 表单验证状态
    bool m_unsavedChanges;                 // 是否有未保存更改
};

#endif // PATIENT_INFO_WIDGET_H
