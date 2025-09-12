/**
 * @brief 增强光学追踪界面 - 基于demo学习成果的全面改进
 * 
 * 集成以下关键功能：
 * 1. VTK三维渲染和可视化（来自demo的VTKRenderWidget）
 * 2. 针尖校准算法（来自demo的PivotCalibration）
 * 3. UDP通信和实时数据传输（来自demo的UdpClient）
 * 4. 工具实时跟踪和变换矩阵管理（来自demo的MainWindow）
 * 5. 点选和配准功能（来自demo的点配准系统）
 */

#ifndef OPTICAL_TRACKING_WIDGET_H
#define OPTICAL_TRACKING_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QSplitter>
#include <QProgressBar>
#include <QTimer>
#include <QUdpSocket>
#include <QHostAddress>
#include <QRadioButton>
#include <QButtonGroup>
#include <QMessageBox>
#include <QVector3D>

// VTK包含（基于demo学习）
#ifdef VTK_FOUND_AND_LINKED
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkDICOMImageReader.h>
#include <vtkNIFTIImageReader.h>
#include <vtkImageData.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolume.h>
#include <vtkPiecewiseFunction.h>
#include <vtkColorTransferFunction.h>
#include <vtkVolumeProperty.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPointPicker.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkTransformFilter.h>
#include <vtkLandmarkTransform.h>
#include <vtkLineSource.h>
#include <vtkAxesActor.h>
#include <vtkNamedColors.h>
#include <vtkPoints.h>

// Qt VTK Widget（基于demo学习）
#include <QVTKOpenGLNativeWidget.h>
#endif // VTK_FOUND_AND_LINKED

class OpticalTrackingServiceImpl;

/**
 * @brief 增强光学追踪界面 - 集成demo的所有优秀功能
 * 
 * 主要功能模块：
 * 1. 三维可视化：VTK渲染CT/DICOM，实时显示工具和标记
 * 2. 工具跟踪：实时显示探针、参考标记、针尖位置
 * 3. 针尖校准：集成3D Slicer算法，支持自动校准
 * 4. 点配准：支持在3D视图中选点和获取针尖点
 * 5. UDP通信：接收Atracsys设备的实时位姿数据
 * 6. 变换管理：多层变换矩阵管理和实时更新
 */
class OpticalTrackingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OpticalTrackingWidget(QWidget *parent = nullptr);
    virtual ~OpticalTrackingWidget();

    void setTrackingService(OpticalTrackingServiceImpl* service);

private slots:
    // === 控制操作（基于demo） ===
    void onStartStopTracking();
    void onLoadCTData();
    void onTogglePickPoints();
    void onToggleViewInteractor();
    void onStartPivotCalibration();
    void onGetTargetPoint();
    void onPointRegistration();
    void onResetView();
    
    // === UDP通信（基于demo的UdpClient） ===
    void onStartUDPClient();
    void onStopUDPClient();
    void readPendingDatagrams();
    void onUdpDataReceived();
    
    // === 针尖校准（基于demo的PivotCalibration） ===
    void onPivotPrepareTimeout();
    void onPivotSamplingTimeout();
    void onCalibrationComplete();
    
    // === VTK渲染更新（基于demo的MainWindow） ===
    void updateToolVisualization();
    void updateCameraViews();
    void renderScene();
    
    // === 数据处理（基于demo） ===
    void processTransformMatrices(const QList<QList<float>>& centerTransform,
                                 const QList<QList<float>>& refTransform,
                                 const QList<QList<float>>& relativeTransform);
    void processMarkerPoints(const QList<QVector3D>& points);

public slots:
    // === 外部接口 ===
    void onAddPoint(double x, double y, double z, int type);
    void onTransformUpdate(const QList<QList<float>>& centerMatrix,
                          const QList<QList<float>>& refMatrix, 
                          const QList<QList<float>>& relativeMatrix);

private:
    void setupUI();
    void setupVTKRendering();
    void setupUDPCommunication();
    void setupPivotCalibration();
    void setupConnections();
    
    // === VTK场景设置（基于demo的init和initStylet） ===
    void initializeVTKScene();
    void initializeToolVisualization();
    void initializeMarkerPoints();
    
    // === VTK交互（基于demo的VTKPointPickerInteractorStyle） ===
    void setupVTKInteraction();
    
    // === 数据解析（基于demo的parseBuffer） ===
    void parseUDPBuffer(const QByteArray& buffer);
    
    // === 工具管理（基于demo的updateStylet） ===
    void updateProbeVisualization();
    void updateReferenceMarker();
    void updateTipPosition();

#ifdef VTK_FOUND_AND_LINKED
    void setTransformMatrix(const float translation[3], const float rotation[3][3], 
                           vtkMatrix4x4* matrix);
#endif

private:
    // === 主布局 ===
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;
    
    // === VTK三维渲染区域（基于demo的VTKRenderWidget） ===
    QGroupBox* m_renderGroup;
#ifdef VTK_FOUND_AND_LINKED
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderWindowInteractor> m_interactor;
    
    // === CT/DICOM显示（基于demo的on_openCT） ===
    vtkSmartPointer<vtkDICOMImageReader> m_dicomReader;
    vtkSmartPointer<vtkNIFTIImageReader> m_niftiReader;
    vtkSmartPointer<vtkImageData> m_imageData;
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> m_volumeMapper;
    vtkSmartPointer<vtkVolume> m_volume;
    vtkSmartPointer<vtkLandmarkTransform> m_landmarkTransform;
    
    // === 工具可视化（基于demo的initStylet） ===
    // 探针中心
    vtkSmartPointer<vtkSphereSource> m_probeCenterSource;
    vtkSmartPointer<vtkPolyDataMapper> m_probeCenterMapper;
    vtkSmartPointer<vtkActor> m_probeCenterActor;
    vtkSmartPointer<vtkMatrix4x4> m_centerToTrackMatrix;
    vtkSmartPointer<vtkTransform> m_centerToTrackTransform;
    
    // 探针针尖
    vtkSmartPointer<vtkSphereSource> m_probeTipSource;
    vtkSmartPointer<vtkPolyDataMapper> m_probeTipMapper;
    vtkSmartPointer<vtkActor> m_probeTipActor;
    vtkSmartPointer<vtkMatrix4x4> m_tipToCenterMatrix;
    vtkSmartPointer<vtkTransform> m_tipToCenterTransform;
    vtkSmartPointer<vtkTransform> m_tipToTrackTransform;
    
    // 探针线段
    vtkSmartPointer<vtkLineSource> m_probeLineSource;
    vtkSmartPointer<vtkPolyDataMapper> m_probeLineMapper;
    vtkSmartPointer<vtkActor> m_probeLineActor;
    
    // 参考标记
    vtkSmartPointer<vtkSphereSource> m_refMarkerSource;
    vtkSmartPointer<vtkPolyDataMapper> m_refMarkerMapper;
    vtkSmartPointer<vtkActor> m_refMarkerActor;
    vtkSmartPointer<vtkMatrix4x4> m_refToTrackMatrix;
    vtkSmartPointer<vtkTransform> m_refToTrackTransform;
    
    // 坐标轴
    vtkSmartPointer<vtkAxesActor> m_axesActor;
    
    // === 点配准系统（基于demo的点配准） ===
    vtkSmartPointer<vtkPoints> m_sourcePoints;  // VTK中选择的点
    vtkSmartPointer<vtkPoints> m_targetPoints;  // 针尖获取的点
    QList<vtkSmartPointer<vtkActor>> m_sourcePointActors;
    QList<vtkSmartPointer<vtkActor>> m_targetPointActors;
#else
    // 非VTK环境下的占位符
    QWidget* m_vtkWidget;
    void* m_renderer;
    void* m_renderWindow;
    void* m_interactor;
    void* m_dicomReader;
    void* m_niftiReader;
    void* m_imageData;
    void* m_volumeMapper;
    void* m_volume;
    void* m_landmarkTransform;
    void* m_probeCenterSource;
    void* m_probeCenterMapper;
    void* m_probeCenterActor;
    void* m_centerToTrackMatrix;
    void* m_centerToTrackTransform;
    void* m_probeTipSource;
    void* m_probeTipMapper;
    void* m_probeTipActor;
    void* m_tipToCenterMatrix;
    void* m_tipToCenterTransform;
    void* m_tipToTrackTransform;
    void* m_probeLineSource;
    void* m_probeLineMapper;
    void* m_probeLineActor;
    void* m_refMarkerSource;
    void* m_refMarkerMapper;
    void* m_refMarkerActor;
    void* m_refToTrackMatrix;
    void* m_refToTrackTransform;
    void* m_axesActor;
    void* m_sourcePoints;
    void* m_targetPoints;
    QList<void*> m_sourcePointActors;
    QList<void*> m_targetPointActors;
#endif
    
    // === 控制面板 ===
    QGroupBox* m_controlGroup;
    QPushButton* m_startTrackingBtn;
    QPushButton* m_loadCTBtn;
    QPushButton* m_startUDPBtn;
    QPushButton* m_resetViewBtn;
    
    // 点选控制
    QRadioButton* m_pickPointsRadio;
    QRadioButton* m_viewInteractorRadio;
    QButtonGroup* m_interactionGroup;
    
    // 校准控制
    QPushButton* m_pivotCalibrationBtn;
    QPushButton* m_getTargetPointBtn;
    QPushButton* m_registrationBtn;
    QProgressBar* m_calibrationProgress;
    QLabel* m_calibrationStatusLabel;
    
    // === UDP通信（基于demo的UdpClient） ===
    QUdpSocket* m_udpSocket;
    QTimer* m_udpTimer;
    QString m_udpIP;
    quint16 m_udpPort;
    
    // 变换矩阵缓存
    QList<QList<float>> m_centerTransformMatrix;
    QList<QList<float>> m_refTransformMatrix;
    QList<QList<float>> m_relativeTransformMatrix;
    QList<QVector3D> m_markerPoints;
    
    // === 针尖校准（基于demo的PivotCalibration） ===
    QTimer* m_pivotPrepareTimer;
    QTimer* m_pivotSamplingTimer;
    QTimer* m_renderTimer;
    int m_prepareTime;
    int m_samplingTime;
    bool m_isPivotCalibrating;
    bool m_pickPointsMode;
    
    // === 状态信息 ===
    QGroupBox* m_statusGroup;
    QLabel* m_deviceStatusLabel;
    QLabel* m_trackingStatusLabel;
    QLabel* m_udpStatusLabel;
    QLabel* m_frameRateLabel;
    QTextEdit* m_logTextEdit;
    
    // === 数据表格 ===
    QTableWidget* m_dataTable;
    
    // === 服务对象 ===
    OpticalTrackingServiceImpl* m_trackingService;
    QString m_currentSessionId;
    
    // === 状态变量 ===
    bool m_isTracking;
    bool m_isUDPActive;
    bool m_deviceConnected;
    int m_frameCount;
    double m_frameRate;
    
    // === 工具坐标 ===
    double m_centerCoordinate[3];
    double m_tipCoordinate[3];
    double m_pickedCoordinate[3];
};

#endif // OPTICAL_TRACKING_WIDGET_H
