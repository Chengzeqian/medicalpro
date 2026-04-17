#ifndef OPTICAL_REGISTRATION_WIDGET_H
#define OPTICAL_REGISTRATION_WIDGET_H

/**
 * @file OpticalRegistrationWidget.h
 * @brief 光学配准界面组件
 *
 * UI布局:
 * - 左侧: 设备控制 + 跟踪工具状态 + 配准进度 + 开始按钮
 * - 中间: 实时光学跟踪视图 + 位姿显示
 * - 右侧: 配准结果显示 + 精度指标
 */

#include <QWidget>
#include "../OpticalRegistrationService.h"

#ifdef VTK_FOUND
#include <vtkSmartPointer.h>
class vtkRenderer;
class vtkGenericOpenGLRenderWindow;
class vtkRenderWindowInteractor;
class vtkActor;
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
class OpticalRegistrationService;

/**
 * @brief 光学配准界面组件
 *
 * 提供完整的光学配准交互界面:
 * - 设备连接/断开控制
 * - 跟踪工具状态显示
 * - 配准点采集和管理
 * - 实时跟踪视图（3D位姿可视化）
 * - 配准结果和精度显示
 */
class OpticalRegistrationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OpticalRegistrationWidget(OpticalRegistrationService* service,
                                       QWidget* parent = nullptr);
    ~OpticalRegistrationWidget() override;

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
     * @brief 更新位姿显示
     * @param position 位置 [x, y, z]
     * @param rotation 旋转 [rx, ry, rz]
     */
    void updatePoseDisplay(const QVector3D& position, const QVector3D& rotation);

private slots:
    // 设备控制槽
    void onConnectDevice();
    void onDisconnectDevice();
    void onCalibrateDevice();

    // 配准点管理槽
    void onAddPoint();
    void onCapturePoint();
    void onClearPoints();
    void onPointTableSelectionChanged();

    // 配准执行槽
    void onStartRegistration();

    // 服务信号响应
    void onPointUpdated(int index);
    void onPointsCleared();
    void onRegistrationStarted();
    void onRegistrationCompleted(const OpticalRegistrationResult& result);
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
    void updateResultDisplay(const OpticalRegistrationResult& result);
    void appendLog(const QString& message);
    QString formatTime() const;

    // VTK相关
    void initializeVTK();
    void updateToolMarkers();

    // 样式定义
    QString getGroupStyle() const;
    QString getButtonStyle() const;
    QString getTableStyle() const;

private:
    OpticalRegistrationService* m_service;

    // ========== 左侧：设备控制 ==========
    QPushButton* m_connectBtn;
    QPushButton* m_disconnectBtn;
    QPushButton* m_calibrateBtn;
    QLabel* m_deviceStatus;
    QLabel* m_trackingStatus;
    QTableWidget* m_toolTable;

    // ========== 左侧：配准点管理 ==========
    QTableWidget* m_pointTable;
    QPushButton* m_addPointBtn;
    QPushButton* m_capturePointBtn;
    QPushButton* m_clearPointsBtn;

    // ========== 左侧：配准进度 ==========
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QTextEdit* m_logText;
    QPushButton* m_startBtn;

    // ========== 中间：实时视图 ==========
    QWidget* m_3dViewContainer;
    QLabel* m_3dViewPlaceholder;
    QLabel* m_positionLabel;
    QLabel* m_rotationLabel;

#ifdef VTK_FOUND
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    bool m_vtkInitialized;
#endif

    // ========== 右侧：配准结果 ==========
    QLabel* m_resultStatusLabel;
    QTableWidget* m_resultTable;
    QLabel* m_freLabel;
    QLabel* m_treLabel;
    QFrame* m_accuracyFrame;
};

#endif // OPTICAL_REGISTRATION_WIDGET_H

