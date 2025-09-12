#ifndef PATIENT_LIST_WIDGET_H
#define PATIENT_LIST_WIDGET_H

#include "PatientDataStructures.h"
#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QHeaderView>

QT_BEGIN_NAMESPACE
namespace Ui {
class PatientListWidget;
}
QT_END_NAMESPACE

class PatientDatabaseService;
class ctkPluginContext;
class PatientInfoWidget;
class PatientSearchWidget;

/**
 * @brief 患者列表管理界面
 * 
 * 提供患者列表的显示、搜索、编辑和管理功能。
 * 支持分页加载、实时搜索和批量操作。
 */
class PatientListWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit PatientListWidget(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~PatientListWidget();

    /**
     * @brief 设置插件上下文
     * @param context CTK插件上下文
     */
    void setPluginContext(ctkPluginContext* context);
    
    /**
     * @brief 刷新患者列表
     */
    void refreshPatientList();
    
    /**
     * @brief 搜索患者
     * @param criteria 搜索条件
     */
    void searchPatients(const PatientSearchCriteria& criteria);
    
    /**
     * @brief 选择指定患者
     * @param patientId 患者ID
     */
    void selectPatient(int patientId);
    
    /**
     * @brief 获取当前选中的患者
     * @return 选中的患者信息，未选中返回无效PatientInfo
     */
    PatientInfo getCurrentSelectedPatient() const;
    
    /**
     * @brief 获取选中的患者ID列表
     * @return 患者ID列表
     */
    QList<int> getSelectedPatientIds() const;

public slots:
    /**
     * @brief 添加新患者
     */
    void addNewPatient();
    
    /**
     * @brief 编辑选中患者
     */
    void editSelectedPatient();
    
    /**
     * @brief 删除选中患者
     */
    void deleteSelectedPatients();
    
    /**
     * @brief 查看选中患者详情
     */
    void viewPatientDetails();
    
    /**
     * @brief 导出患者数据
     */
    void exportPatientData();
    
    /**
     * @brief 高级搜索
     */
    void showAdvancedSearch();

signals:
    /**
     * @brief 患者选择变化信号
     * @param patient 选中的患者信息
     */
    void patientSelectionChanged(const PatientInfo& patient);
    
    /**
     * @brief 患者双击信号
     * @param patient 双击的患者信息
     */
    void patientDoubleClicked(const PatientInfo& patient);
    
    /**
     * @brief 请求编辑患者信号
     * @param patient 要编辑的患者信息
     */
    void editPatientRequested(const PatientInfo& patient);
    
    /**
     * @brief 患者列表更新信号
     * @param totalCount 患者总数
     */
    void patientListUpdated(int totalCount);

private slots:
    /**
     * @brief 处理表格选择变化
     */
    void onTableSelectionChanged();
    
    /**
     * @brief 处理表格双击
     */
    void onTableDoubleClicked();
    
    /**
     * @brief 处理右键菜单
     * @param position 菜单位置
     */
    void onTableContextMenu(const QPoint& position);
    
    /**
     * @brief 处理快速搜索
     */
    void onQuickSearchTextChanged();
    
    /**
     * @brief 处理搜索定时器
     */
    void onSearchTimerTimeout();
    
    /**
     * @brief 处理性别过滤
     */
    void onGenderFilterChanged();
    
    /**
     * @brief 处理年龄范围过滤
     */
    void onAgeFilterChanged();
    
    /**
     * @brief 处理分页
     */
    void onPageChanged();
    
    /**
     * @brief 处理数据库服务信号
     */
    void onPatientAdded(const PatientInfo& patient);
    void onPatientUpdated(const PatientInfo& patient);
    void onPatientDeleted(int patientId);
    void onDatabaseError(const QString& error);

private:
    /**
     * @brief 初始化UI界面
     */
    void initializeUI();
    
    /**
     * @brief 创建工具栏
     * @return 工具栏widget
     */
    QWidget* createToolBar();
    
    /**
     * @brief 创建搜索栏
     * @return 搜索栏widget
     */
    QWidget* createSearchBar();
    
    /**
     * @brief 创建患者表格
     */
    void createPatientTable();
    
    /**
     * @brief 创建状态栏
     * @return 状态栏widget
     */
    QWidget* createStatusBar();
    
    /**
     * @brief 创建右键菜单
     */
    void createContextMenu();
    
    /**
     * @brief 设置表格样式
     */
    void setupTableStyle();
    
    /**
     * @brief 连接信号和槽
     */
    void connectSignals();
    
    /**
     * @brief 应用主题样式
     */
    void applyTheme();
    
    /**
     * @brief 加载患者数据到表格
     * @param patients 患者列表
     */
    void loadPatientsToTable(const QList<PatientInfo>& patients);
    
    /**
     * @brief 更新表格行
     * @param row 行号
     * @param patient 患者信息
     */
    void updateTableRow(int row, const PatientInfo& patient);
    
    /**
     * @brief 从表格行获取患者信息
     * @param row 行号
     * @return 患者信息
     */
    PatientInfo getPatientFromTableRow(int row) const;
    
    /**
     * @brief 查找患者在表格中的行号
     * @param patientId 患者ID
     * @return 行号，未找到返回-1
     */
    int findPatientRowById(int patientId) const;
    
    /**
     * @brief 更新状态显示
     */
    void updateStatusDisplay();
    
    /**
     * @brief 更新按钮状态
     */
    void updateButtonStates();
    
    /**
     * @brief 获取数据库服务
     * @return 数据库服务指针
     */
    PatientDatabaseService* getDatabaseService();
    
    /**
     * @brief 构建搜索条件
     * @return 搜索条件
     */
    PatientSearchCriteria buildSearchCriteria() const;
    
    /**
     * @brief 确认删除操作
     * @param patientCount 要删除的患者数量
     * @return 确认删除返回true
     */
    bool confirmDelete(int patientCount);
    
    /**
     * @brief 显示加载进度
     * @param show 是否显示
     */
    void showLoadingProgress(bool show);

private:
    Ui::PatientListWidget *ui;                 // UI文件指针
    
    // CTK集成
    ctkPluginContext* m_pluginContext;         // 插件上下文
    
    // 主要布局组件
    QVBoxLayout* m_mainLayout;                 // 主布局
    QWidget* m_toolBar;                        // 工具栏
    QWidget* m_searchBar;                      // 搜索栏
    QTableWidget* m_patientTable;              // 患者表格
    QWidget* m_statusBar;                      // 状态栏
    
    // 工具栏按钮
    QPushButton* m_addButton;                  // 添加按钮
    QPushButton* m_editButton;                 // 编辑按钮
    QPushButton* m_deleteButton;               // 删除按钮
    QPushButton* m_viewButton;                 // 查看按钮
    QPushButton* m_exportButton;               // 导出按钮
    QPushButton* m_refreshButton;              // 刷新按钮
    QPushButton* m_advancedSearchButton;       // 高级搜索按钮
    
    // 搜索组件
    QLineEdit* m_quickSearchEdit;              // 快速搜索输入框
    QComboBox* m_genderFilterCombo;            // 性别过滤
    QComboBox* m_ageRangeCombo;                // 年龄范围过滤
    QPushButton* m_clearFiltersButton;         // 清除过滤按钮
    
    // 状态组件
    QLabel* m_statusLabel;                     // 状态标签
    QLabel* m_countLabel;                      // 计数标签
    QProgressBar* m_loadingProgress;           // 加载进度条
    
    // 右键菜单
    QMenu* m_contextMenu;                      // 右键菜单
    QAction* m_editAction;                     // 编辑动作
    QAction* m_deleteAction;                   // 删除动作
    QAction* m_viewAction;                     // 查看动作
    QAction* m_copyInfoAction;                 // 复制信息动作
    
    // 搜索相关
    QTimer* m_searchTimer;                     // 搜索延迟定时器
    PatientSearchCriteria m_currentCriteria;   // 当前搜索条件
    
    // 数据相关
    QList<PatientInfo> m_currentPatients;      // 当前显示的患者列表
    PatientInfo m_selectedPatient;             // 当前选中的患者
    
    // 分页相关（预留）
    int m_currentPage;                         // 当前页
    int m_pageSize;                            // 页大小
    int m_totalCount;                          // 总数
    
    // 子窗口
    PatientInfoWidget* m_patientInfoWidget;    // 患者信息窗口
    PatientSearchWidget* m_searchWidget;       // 高级搜索窗口
    
    // 表格列索引
    enum ColumnIndex {
        COL_ID = 0,
        COL_NAME,
        COL_AGE,
        COL_GENDER,
        COL_PHONE,
        COL_REGISTRATION_DATE,
        COL_LAST_VISIT,
        COL_COUNT
    };
};

#endif // PATIENT_LIST_WIDGET_H
