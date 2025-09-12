#ifndef PATIENT_SEARCH_WIDGET_H
#define PATIENT_SEARCH_WIDGET_H

#include "PatientDataStructures.h"
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

QT_BEGIN_NAMESPACE
namespace Ui {
class PatientSearchWidget;
}
QT_END_NAMESPACE

/**
 * @brief 患者高级搜索对话框
 * 
 * 提供复杂的患者搜索条件设置功能。
 */
class PatientSearchWidget : public QDialog
{
    Q_OBJECT

public:
    explicit PatientSearchWidget(QWidget *parent = nullptr);
    ~PatientSearchWidget();

    /**
     * @brief 获取搜索条件
     * @return 搜索条件
     */
    PatientSearchCriteria getSearchCriteria() const;
    
    /**
     * @brief 设置搜索条件
     * @param criteria 搜索条件
     */
    void setSearchCriteria(const PatientSearchCriteria& criteria);
    
    /**
     * @brief 清除所有搜索条件
     */
    void clearSearchCriteria();

signals:
    /**
     * @brief 搜索请求信号
     * @param criteria 搜索条件
     */
    void searchRequested(const PatientSearchCriteria& criteria);

private slots:
    /**
     * @brief 执行搜索
     */
    void onSearchClicked();
    
    /**
     * @brief 清除搜索条件
     */
    void onClearClicked();
    
    /**
     * @brief 取消搜索
     */
    void onCancelClicked();

private:
    /**
     * @brief 初始化UI
     */
    void initializeUI();
    
    /**
     * @brief 创建基本信息搜索组
     * @return 基本信息组
     */
    QGroupBox* createBasicSearchGroup();
    
    /**
     * @brief 创建医疗信息搜索组
     * @return 医疗信息组
     */
    QGroupBox* createMedicalSearchGroup();
    
    /**
     * @brief 创建日期范围搜索组
     * @return 日期范围组
     */
    QGroupBox* createDateRangeGroup();
    
    /**
     * @brief 创建按钮组
     * @return 按钮组widget
     */
    QWidget* createButtonGroup();
    
    /**
     * @brief 连接信号和槽
     */
    void connectSignals();
    
    /**
     * @brief 应用样式
     */
    void applyStyle();

private:
    Ui::PatientSearchWidget *ui;
    
    // 基本信息搜索
    QLineEdit* m_nameEdit;
    QLineEdit* m_phoneEdit;
    QLineEdit* m_idCardEdit;
    QSpinBox* m_ageMinSpinBox;
    QSpinBox* m_ageMaxSpinBox;
    QComboBox* m_genderCombo;
    
    // 医疗信息搜索
    QTextEdit* m_medicalHistoryEdit;
    
    // 日期范围搜索
    QDateTimeEdit* m_regDateStart;
    QDateTimeEdit* m_regDateEnd;
    
    // 按钮
    QPushButton* m_searchButton;
    QPushButton* m_clearButton;
    QPushButton* m_cancelButton;
    
    // 布局
    QVBoxLayout* m_mainLayout;
};

#endif // PATIENT_SEARCH_WIDGET_H
