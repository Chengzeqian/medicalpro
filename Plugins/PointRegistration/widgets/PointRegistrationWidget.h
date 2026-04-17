#ifndef POINT_REGISTRATION_WIDGET_H
#define POINT_REGISTRATION_WIDGET_H

/**
 * @file PointRegistrationWidget.h
 * @brief 基于点的配准界面组件
 *
 * UI布局:
 * - 左侧: 解剖标记点管理（源点+目标点） + 坐标输入 + 变换模式 + 配准进度 + 开始按钮
 * - 中间: 3D视图区域（支持点击选点）
 * - 右侧: 配准结果显示
 */

#include <QWidget>
#include "../PointRegistrationDataStructures.h"

#ifdef VTK_FOUND
#include <vtkSmartPointer.h>
class vtkRenderer;
class vtkGenericOpenGLRenderWindow;
class vtkRenderWindowInteractor;
class vtkActor;
class vtkSphereSource;
class vtkPolyDataMapper;
class QVTKOpenGLNativeWidget;
#endif

class QTableWidget;
class QPushButton;
class QProgressBar;
class QTextEdit;
class QLabel;
class QGroupBox;
class QFrame;
class QComboBox;
class QDoubleSpinBox;
class QRadioButton;
class PointRegistrationService;

/**
 * @brief 点配准界面组件
 *
 * 提供完整的点配准交互界面:
 * - 配准点列表管理（名称、X、Y、Z坐标）
 * - 配准进度显示（状态、进度条、日志）
 * - 3D视图区域（用于点击添加标记点）
 * - 配准结果显示（变换参数、精度指标）
 */
class PointRegistrationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PointRegistrationWidget(PointRegistrationService* service,
                                     QWidget* parent = nullptr);
    ~PointRegistrationWidget() override;

    /**
     * @brief 刷新界面
     */
    void refresh();

    /**
     * @brief 获取3D视图容器（用于外部嵌入VTK渲染窗口）
     */
    QWidget* get3DViewContainer() const { return m_3dViewContainer; }

public slots:
    /**
     * @brief 处理3D视图中的点击选点
     * @param x, y, z 点击位置的世界坐标
     */
    void onPointPicked(double x, double y, double z);

private slots:
    // 点管理槽
    void onAddPoint();
    void onDeletePoint();
    void onClearPoints();
    void onSetSourcePoint();
    void onSetTargetPoint();
    void onPointTableSelectionChanged();
    void onTransformModeChanged(int index);
    void onPickModeChanged();
    void onLoadModel();  // 加载3D骨骼模型

    // 配准执行槽
    void onStartRegistration();

    // 服务信号响应
    void onPointAdded(int index, const QString& name);
    void onPointRemoved(int index);
    void onPointsCleared();
    void onPointUpdated(int index);
    void onRegistrationCompleted(const PointRegistrationResult& result);
    void onRegistrationFailed(const QString& error);
    void onProgressUpdated(int progress, const QString& message);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setupUI();
    QWidget* createLeftPanel();
    QWidget* createCenterPanel();
    QWidget* createRightPanel();
    void connectSignals();
    void updatePointTable();
    void updateResultDisplay(const PointRegistrationResult& result);
    void appendLog(const QString& message);
    QString formatTime() const;

    // VTK相关
    void initializeVTK();
    void updatePointMarkers();
    void addPointMarker(const QVector3D& pos, const QColor& color);
    void clearPointMarkers();

    // 样式定义
    QString getGroupStyle() const;
    QString getButtonStyle() const;
    QString getTableStyle() const;

private:
    PointRegistrationService* m_service;

    // ========== 左侧：标记点管理 ==========
    QTableWidget* m_pointTable;              // 7列: 名称, 源X, 源Y, 源Z, 目标X, 目标Y, 目标Z
    QPushButton* m_addPointBtn;
    QPushButton* m_deletePointBtn;
    QPushButton* m_clearPointsBtn;

    // ========== 左侧：坐标输入 ==========
    QDoubleSpinBox* m_sourceXSpin;
    QDoubleSpinBox* m_sourceYSpin;
    QDoubleSpinBox* m_sourceZSpin;
    QDoubleSpinBox* m_targetXSpin;
    QDoubleSpinBox* m_targetYSpin;
    QDoubleSpinBox* m_targetZSpin;
    QPushButton* m_setSourceBtn;
    QPushButton* m_setTargetBtn;

    // ========== 左侧：变换模式 ==========
    QComboBox* m_transformModeCombo;

    // ========== 左侧：选点模式 ==========
    QRadioButton* m_pickSourceRadio;
    QRadioButton* m_pickTargetRadio;
    bool m_pickingSource;                    // true=选取源点, false=选取目标点

    // ========== 左侧：配准进度 ==========
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QTextEdit* m_logText;
    QPushButton* m_startBtn;                 // 开始配准按钮

    // ========== 中间：3D视图 ==========
    QWidget* m_3dViewContainer;              // 3D视图容器
    QLabel* m_3dViewPlaceholder;             // 占位符标签（无VTK时显示）
    QPushButton* m_loadModelBtn;             // 加载3D模型按钮
    QLabel* m_modelInfoLabel;                // 模型信息标签

#ifdef VTK_FOUND
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderWindowInteractor> m_interactor;
    vtkSmartPointer<vtkActor> m_modelActor;               // 3D骨骼模型Actor
    QVector<vtkSmartPointer<vtkActor>> m_sourceMarkers;   // 源点标记
    QVector<vtkSmartPointer<vtkActor>> m_targetMarkers;   // 目标点标记
    bool m_vtkInitialized;
#endif

    // ========== 右侧：配准结果 ==========
    QLabel* m_resultStatusLabel;             // 结果状态
    QTableWidget* m_resultTable;             // 变换参数表格
    QLabel* m_rmsErrorLabel;
    QLabel* m_maxErrorLabel;
    QFrame* m_accuracyFrame;                 // 精度指标框
};

#endif // POINT_REGISTRATION_WIDGET_H

