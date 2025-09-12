/**
 * @brief 增强光学追踪界面实现 - 基于demo学习成果的全面改进
 */

#include "OpticalTrackingWidget.h"
#include "OpticalTrackingServiceImpl.h"
#include <QApplication>
#include <QFileDialog>
#include <QDateTime>
#include <QDebug>
#include <QNetworkDatagram>
#include <sstream>
#include <iostream>

// 临时禁用VTK功能以解决链接问题
#ifdef VTK_FOUND_AND_LINKED
// VTK自定义交互样式（基于demo的VTKPointPickerInteractorStyle）
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkObjectFactory.h>
#include <vtkCamera.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRendererCollection.h>
#include <vtkAbstractPicker.h>

class VTKPointPickerInteractorStyle : public vtkInteractorStyleTrackballCamera
{
public:
    static VTKPointPickerInteractorStyle* New();
    vtkTypeMacro(VTKPointPickerInteractorStyle, vtkInteractorStyleTrackballCamera);

    VTKPointPickerInteractorStyle()
    {
        m_widget = nullptr;
    }

    void SetWidget(OpticalTrackingWidget* widget)
    {
        m_widget = widget;
    }

    virtual void OnLeftButtonDown() override
    {
        if (m_widget && m_widget->property("pickPointsMode").toBool())
        {
            // 获取鼠标点击位置
            int* clickPos = this->GetInteractor()->GetEventPosition();
            // 使用点选器选取3D点
            this->Interactor->GetPicker()->Pick(clickPos[0], clickPos[1], 0,
                                              this->Interactor->GetRenderWindow()->GetRenderers()->GetFirstRenderer());
            double picked[3];
            this->Interactor->GetPicker()->GetPickPosition(picked);
            
            // 在3D视图中添加特征点
            m_widget->onAddPoint(picked[0], picked[1], picked[2], 2);
        }
        
        // 传递给基类处理
        vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
    }

private:
    OpticalTrackingWidget* m_widget;
};

vtkStandardNewMacro(VTKPointPickerInteractorStyle);
#endif // VTK_FOUND_AND_LINKED

// 构造函数
OpticalTrackingWidget::OpticalTrackingWidget(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_mainSplitter(nullptr)
    , m_renderGroup(nullptr)
    , m_vtkWidget(nullptr)
#ifdef VTK_FOUND_AND_LINKED
    , m_renderer(vtkSmartPointer<vtkRenderer>::New())
    , m_renderWindow(vtkSmartPointer<vtkRenderWindow>::New())
    , m_interactor(nullptr)
    , m_dicomReader(vtkSmartPointer<vtkDICOMImageReader>::New())
    , m_niftiReader(vtkSmartPointer<vtkNIFTIImageReader>::New())
    , m_imageData(nullptr)
    , m_volumeMapper(vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New())
    , m_volume(vtkSmartPointer<vtkVolume>::New())
    , m_landmarkTransform(vtkSmartPointer<vtkLandmarkTransform>::New())
#else
    , m_renderer(nullptr)
    , m_renderWindow(nullptr)
    , m_interactor(nullptr)
    , m_dicomReader(nullptr)
    , m_niftiReader(nullptr)
    , m_imageData(nullptr)
    , m_volumeMapper(nullptr)
    , m_volume(nullptr)
    , m_landmarkTransform(nullptr)
#endif
    , m_controlGroup(nullptr)
    , m_startTrackingBtn(nullptr)
    , m_loadCTBtn(nullptr)
    , m_startUDPBtn(nullptr)
    , m_resetViewBtn(nullptr)
    , m_pickPointsRadio(nullptr)
    , m_viewInteractorRadio(nullptr)
    , m_interactionGroup(nullptr)
    , m_pivotCalibrationBtn(nullptr)
    , m_getTargetPointBtn(nullptr)
    , m_registrationBtn(nullptr)
    , m_calibrationProgress(nullptr)
    , m_calibrationStatusLabel(nullptr)
    , m_udpSocket(new QUdpSocket(this))
    , m_udpTimer(new QTimer(this))
    , m_udpIP("192.168.1.101")
    , m_udpPort(8888)
    , m_pivotPrepareTimer(new QTimer(this))
    , m_pivotSamplingTimer(new QTimer(this))
    , m_renderTimer(new QTimer(this))
    , m_prepareTime(3)
    , m_samplingTime(5)
    , m_isPivotCalibrating(false)
    , m_pickPointsMode(false)
    , m_statusGroup(nullptr)
    , m_deviceStatusLabel(nullptr)
    , m_trackingStatusLabel(nullptr)
    , m_udpStatusLabel(nullptr)
    , m_frameRateLabel(nullptr)
    , m_logTextEdit(nullptr)
    , m_dataTable(nullptr)
    , m_trackingService(nullptr)
    , m_isTracking(false)
    , m_isUDPActive(false)
    , m_deviceConnected(false)
    , m_frameCount(0)
    , m_frameRate(0.0)
{
    // 初始化坐标
    m_centerCoordinate[0] = m_centerCoordinate[1] = m_centerCoordinate[2] = 0.0;
    m_tipCoordinate[0] = m_tipCoordinate[1] = m_tipCoordinate[2] = 0.0;
    m_pickedCoordinate[0] = m_pickedCoordinate[1] = m_pickedCoordinate[2] = 0.0;

    setupUI();
    setupVTKRendering();
    setupUDPCommunication();
    setupPivotCalibration();
    setupConnections();
    
    // 启动渲染定时器
    m_renderTimer->start(50); // 20fps渲染频率
}

OpticalTrackingWidget::~OpticalTrackingWidget()
{
    if (m_udpSocket && m_udpSocket->state() == QAbstractSocket::BoundState) {
        m_udpSocket->close();
    }
}

void OpticalTrackingWidget::setupUI()
{
    setWindowTitle("Enhanced Optical Tracking - Demo Integration");
    setMinimumSize(1400, 900);
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);
    
    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧：VTK渲染区域
    m_renderGroup = new QGroupBox("三维可视化与工具跟踪", this);
    QVBoxLayout* renderLayout = new QVBoxLayout(m_renderGroup);
    
    m_vtkWidget = new QVTKOpenGLNativeWidget(this);
    m_vtkWidget->setMinimumSize(800, 600);
    renderLayout->addWidget(m_vtkWidget);
    
    // 交互控制
    QHBoxLayout* interactionLayout = new QHBoxLayout();
    m_interactionGroup = new QButtonGroup(this);
    m_pickPointsRadio = new QRadioButton("点选模式", this);
    m_viewInteractorRadio = new QRadioButton("视角控制", this);
    m_viewInteractorRadio->setChecked(true);
    
    m_interactionGroup->addButton(m_pickPointsRadio);
    m_interactionGroup->addButton(m_viewInteractorRadio);
    
    interactionLayout->addWidget(m_pickPointsRadio);
    interactionLayout->addWidget(m_viewInteractorRadio);
    interactionLayout->addStretch();
    
    renderLayout->addLayout(interactionLayout);
    
    // 右侧：控制面板和状态信息
    QWidget* rightWidget = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    // 控制面板
    m_controlGroup = new QGroupBox("控制面板", this);
    QGridLayout* controlLayout = new QGridLayout(m_controlGroup);
    
    m_startTrackingBtn = new QPushButton("开始跟踪", this);
    m_loadCTBtn = new QPushButton("加载CT数据", this);
    m_startUDPBtn = new QPushButton("启动UDP接收", this);
    m_resetViewBtn = new QPushButton("重置视角", this);
    
    controlLayout->addWidget(m_startTrackingBtn, 0, 0);
    controlLayout->addWidget(m_loadCTBtn, 0, 1);
    controlLayout->addWidget(m_startUDPBtn, 1, 0);
    controlLayout->addWidget(m_resetViewBtn, 1, 1);
    
    // 校准面板
    QGroupBox* calibrationGroup = new QGroupBox("针尖校准", this);
    QVBoxLayout* calibrationLayout = new QVBoxLayout(calibrationGroup);
    
    m_pivotCalibrationBtn = new QPushButton("开始校准", this);
    m_getTargetPointBtn = new QPushButton("获取目标点", this);
    m_registrationBtn = new QPushButton("点配准", this);
    
    m_calibrationProgress = new QProgressBar(this);
    m_calibrationProgress->setVisible(false);
    m_calibrationStatusLabel = new QLabel("校准状态：未开始", this);
    
    calibrationLayout->addWidget(m_pivotCalibrationBtn);
    calibrationLayout->addWidget(m_getTargetPointBtn);
    calibrationLayout->addWidget(m_registrationBtn);
    calibrationLayout->addWidget(m_calibrationProgress);
    calibrationLayout->addWidget(m_calibrationStatusLabel);
    
    // 状态信息面板
    m_statusGroup = new QGroupBox("状态信息", this);
    QVBoxLayout* statusLayout = new QVBoxLayout(m_statusGroup);
    
    m_deviceStatusLabel = new QLabel("设备状态：未连接", this);
    m_trackingStatusLabel = new QLabel("跟踪状态：未开始", this);
    m_udpStatusLabel = new QLabel("UDP状态：未连接", this);
    m_frameRateLabel = new QLabel("帧率：0 fps", this);
    
    statusLayout->addWidget(m_deviceStatusLabel);
    statusLayout->addWidget(m_trackingStatusLabel);
    statusLayout->addWidget(m_udpStatusLabel);
    statusLayout->addWidget(m_frameRateLabel);
    
    // 数据表格
    m_dataTable = new QTableWidget(this);
    m_dataTable->setColumnCount(7);
    QStringList headers;
    headers << "ID" << "几何ID" << "X(mm)" << "Y(mm)" << "Z(mm)" << "误差(mm)" << "状态";
    m_dataTable->setHorizontalHeaderLabels(headers);
    m_dataTable->setMaximumHeight(200);
    
    // 日志区域
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setMaximumHeight(150);
    m_logTextEdit->setPlainText("=== 增强光学跟踪系统日志 ===\n");
    
    // 组装右侧布局
    rightLayout->addWidget(m_controlGroup);
    rightLayout->addWidget(calibrationGroup);
    rightLayout->addWidget(m_statusGroup);
    rightLayout->addWidget(m_dataTable);
    rightLayout->addWidget(m_logTextEdit);
    
    // 添加到主分割器
    m_mainSplitter->addWidget(m_renderGroup);
    m_mainSplitter->addWidget(rightWidget);
    m_mainSplitter->setStretchFactor(0, 3); // VTK区域占3/4
    m_mainSplitter->setStretchFactor(1, 1); // 控制区域占1/4
    
    m_mainLayout->addWidget(m_mainSplitter);
}

void OpticalTrackingWidget::setupVTKRendering()
{
#ifdef VTK_FOUND_AND_LINKED
    // 基于demo的VTK初始化
    m_renderWindow->AddRenderer(m_renderer);
    m_vtkWidget->setRenderWindow(m_renderWindow);
    m_interactor = m_vtkWidget->interactor();
    
    // 初始化VTK场景
    initializeVTKScene();
    initializeToolVisualization();
    
    // 设置交互样式
    setupVTKInteraction();
#else
    // 非VTK环境的替代实现
    m_vtkWidget = new QWidget(m_renderGroup);
    m_vtkWidget->setMinimumSize(400, 300);
    m_vtkWidget->setStyleSheet("QWidget { background-color: #2b2b2b; border: 1px solid #555; }");
    
    QLabel* placeholder = new QLabel("VTK渲染功能需要VTK库支持", m_vtkWidget);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: white;");
    
    QVBoxLayout* layout = new QVBoxLayout(m_vtkWidget);
    layout->addWidget(placeholder);
#endif
}

void OpticalTrackingWidget::initializeVTKScene()
{
#ifdef VTK_FOUND_AND_LINKED
    // 基于demo的场景初始化
    m_renderer->GetActiveCamera()->ParallelProjectionOn();
    m_renderer->GetActiveCamera()->SetParallelScale(246.711);
    m_renderer->GetActiveCamera()->SetViewUp(0.0, 0.0, 1.0);
    m_renderer->GetActiveCamera()->SetPosition(1770.0, 197.0, 225.0);
    m_renderer->GetActiveCamera()->SetClippingRange(437.275, 2690.4);
    m_renderer->GetActiveCamera()->OrthogonalizeViewUp();
    m_renderer->ResetCamera();
    m_renderer->SetBackground(0.3, 0.3, 0.3);
    
    // 添加坐标轴
    m_axesActor = vtkSmartPointer<vtkAxesActor>::New();
    m_axesActor->SetTotalLength(100, 100, 100);
    m_renderer->AddActor(m_axesActor);
    
    // 初始化点集合
    m_sourcePoints = vtkSmartPointer<vtkPoints>::New();
    m_targetPoints = vtkSmartPointer<vtkPoints>::New();
    
    m_renderWindow->Render();
    
    // 记录日志
    m_logTextEdit->append(QString("[%1] VTK场景初始化完成").arg(QDateTime::currentDateTime().toString()));
#endif
}

void OpticalTrackingWidget::initializeToolVisualization()
{
#ifdef VTK_FOUND_AND_LINKED
    // 基于demo的工具可视化初始化
    
    // 探针中心球体（红色）
    m_probeCenterSource = vtkSmartPointer<vtkSphereSource>::New();
    m_probeCenterSource->SetRadius(1);
    m_probeCenterSource->Update();
    
    m_probeCenterMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_probeCenterMapper->SetInputData(m_probeCenterSource->GetOutput());
    
    m_probeCenterActor = vtkSmartPointer<vtkActor>::New();
    m_probeCenterActor->SetMapper(m_probeCenterMapper);
    m_probeCenterActor->GetProperty()->SetColor(1.0, 0.0, 0.0); // 红色
    
    m_centerToTrackMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    m_centerToTrackTransform = vtkSmartPointer<vtkTransform>::New();
    m_probeCenterActor->SetUserTransform(m_centerToTrackTransform);
    m_renderer->AddActor(m_probeCenterActor);
    
    // 探针针尖球体（蓝色）
    m_probeTipSource = vtkSmartPointer<vtkSphereSource>::New();
    m_probeTipSource->SetRadius(1);
    m_probeTipSource->Update();
    
    m_probeTipMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_probeTipMapper->SetInputData(m_probeTipSource->GetOutput());
    
    m_probeTipActor = vtkSmartPointer<vtkActor>::New();
    m_probeTipActor->SetMapper(m_probeTipMapper);
    m_probeTipActor->GetProperty()->SetColor(0.0, 0.0, 1.0); // 蓝色
    
    // 针尖到中心的变换矩阵（基于demo的配置）
    m_tipToCenterMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    m_tipToCenterTransform = vtkSmartPointer<vtkTransform>::New();
    m_tipToTrackTransform = vtkSmartPointer<vtkTransform>::New();
    
    // 设置针尖相对探针中心的变换（基于demo的centerToTipTransVector）
    std::vector<std::vector<double>> centerToTipTransVector = {
        {0.0, -0.07, 1.0, 159.50},
        {1.0, 0.0, 0.0, -0.120},
        {-0.0, 1.0, 0.07, 11.810},
        {0.0, 0.0, 0.0, 1.0}
    };
    
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m_tipToCenterMatrix->SetElement(i, j, centerToTipTransVector[i][j]);
        }
    }
    
    m_tipToCenterTransform->SetMatrix(m_tipToCenterMatrix);
    m_tipToTrackTransform->Concatenate(m_centerToTrackTransform);
    m_tipToTrackTransform->Concatenate(m_tipToCenterTransform);
    m_probeTipActor->SetUserTransform(m_tipToTrackTransform);
    m_renderer->AddActor(m_probeTipActor);
    
    // 探针线段（连接中心和针尖）
    m_probeLineSource = vtkSmartPointer<vtkLineSource>::New();
    m_probeLineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_probeLineMapper->SetInputData(m_probeLineSource->GetOutput());
    
    m_probeLineActor = vtkSmartPointer<vtkActor>::New();
    m_probeLineActor->SetMapper(m_probeLineMapper);
    m_probeLineActor->GetProperty()->SetLineWidth(2);
    vtkSmartPointer<vtkNamedColors> colors = vtkSmartPointer<vtkNamedColors>::New();
    m_probeLineActor->GetProperty()->SetColor(colors->GetColor3d("Peacock").GetData());
    m_renderer->AddActor(m_probeLineActor);
    
    // 参考标记球体（绿色）
    m_refMarkerSource = vtkSmartPointer<vtkSphereSource>::New();
    m_refMarkerSource->SetRadius(1);
    m_refMarkerSource->Update();
    
    m_refMarkerMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_refMarkerMapper->SetInputData(m_refMarkerSource->GetOutput());
    
    m_refMarkerActor = vtkSmartPointer<vtkActor>::New();
    m_refMarkerActor->SetMapper(m_refMarkerMapper);
    m_refMarkerActor->GetProperty()->SetColor(0.0, 1.0, 0.0); // 绿色
    
    m_refToTrackMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    m_refToTrackTransform = vtkSmartPointer<vtkTransform>::New();
    m_refMarkerActor->SetUserTransform(m_refToTrackTransform);
    m_renderer->AddActor(m_refMarkerActor);
    
    m_logTextEdit->append(QString("[%1] 工具可视化初始化完成").arg(QDateTime::currentDateTime().toString()));
#endif
}

void OpticalTrackingWidget::setupVTKInteraction()
{
#ifdef VTK_FOUND_AND_LINKED
    // 设置自定义交互样式（基于demo的VTKPointPickerInteractorStyle）
    vtkSmartPointer<VTKPointPickerInteractorStyle> style = 
        vtkSmartPointer<VTKPointPickerInteractorStyle>::New();
    style->SetWidget(this);
    
    m_interactor->SetInteractorStyle(style);
    
    // 设置点选器
    vtkSmartPointer<vtkPointPicker> picker = vtkSmartPointer<vtkPointPicker>::New();
    m_interactor->SetPicker(picker);
#endif
}

void OpticalTrackingWidget::setupUDPCommunication()
{
    // 基于demo的UdpClient实现
    m_udpSocket->bind(QHostAddress::Any, m_udpPort);
    
    // 初始化变换矩阵
    m_centerTransformMatrix = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}
    };
    m_refTransformMatrix = m_centerTransformMatrix;
    m_relativeTransformMatrix = m_centerTransformMatrix;
    
    m_logTextEdit->append(QString("[%1] UDP通信初始化完成，监听端口：%2")
                         .arg(QDateTime::currentDateTime().toString()).arg(m_udpPort));
}

void OpticalTrackingWidget::setupPivotCalibration()
{
    // 基于demo的针尖校准设置
    m_pivotPrepareTimer->setSingleShot(false);
    m_pivotPrepareTimer->setInterval(3000);
    
    m_pivotSamplingTimer->setSingleShot(false);
    m_pivotSamplingTimer->setInterval(5000);
    
    m_logTextEdit->append(QString("[%1] 针尖校准系统初始化完成")
                         .arg(QDateTime::currentDateTime().toString()));
}

void OpticalTrackingWidget::setupConnections()
{
    // 控制按钮连接
    connect(m_startTrackingBtn, &QPushButton::clicked, this, &OpticalTrackingWidget::onStartStopTracking);
    connect(m_loadCTBtn, &QPushButton::clicked, this, &OpticalTrackingWidget::onLoadCTData);
    connect(m_startUDPBtn, &QPushButton::clicked, this, &OpticalTrackingWidget::onStartUDPClient);
    connect(m_resetViewBtn, &QPushButton::clicked, this, &OpticalTrackingWidget::onResetView);
    
    // 交互模式连接
    connect(m_pickPointsRadio, &QRadioButton::clicked, this, &OpticalTrackingWidget::onTogglePickPoints);
    connect(m_viewInteractorRadio, &QRadioButton::clicked, this, &OpticalTrackingWidget::onToggleViewInteractor);
    
    // 校准按钮连接
    connect(m_pivotCalibrationBtn, &QPushButton::clicked, this, &OpticalTrackingWidget::onStartPivotCalibration);
    connect(m_getTargetPointBtn, &QPushButton::clicked, this, &OpticalTrackingWidget::onGetTargetPoint);
    connect(m_registrationBtn, &QPushButton::clicked, this, &OpticalTrackingWidget::onPointRegistration);
    
    // UDP连接
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &OpticalTrackingWidget::readPendingDatagrams);
    connect(m_udpTimer, &QTimer::timeout, this, &OpticalTrackingWidget::onUdpDataReceived);
    
    // 校准定时器连接
    connect(m_pivotPrepareTimer, &QTimer::timeout, this, &OpticalTrackingWidget::onPivotPrepareTimeout);
    connect(m_pivotSamplingTimer, &QTimer::timeout, this, &OpticalTrackingWidget::onPivotSamplingTimeout);
    
    // 渲染定时器连接
    connect(m_renderTimer, &QTimer::timeout, this, &OpticalTrackingWidget::renderScene);
}

// 控制操作槽函数实现
void OpticalTrackingWidget::onStartStopTracking()
{
    m_isTracking = !m_isTracking;
    
    if (m_isTracking) {
        m_startTrackingBtn->setText("停止跟踪");
        m_trackingStatusLabel->setText("跟踪状态：运行中");
        m_logTextEdit->append(QString("[%1] 开始光学跟踪").arg(QDateTime::currentDateTime().toString()));
        
        if (m_trackingService) {
            // 如果没有活动会话，创建一个默认会话
            if (m_currentSessionId.isEmpty()) {
                // 先扫描并连接到第一个可用设备
                QStringList devices = m_trackingService->scanAvailableDevices();
                if (!devices.isEmpty()) {
                    QString deviceId = devices.first();
                    if (m_trackingService->connectToDevice(deviceId)) {
                        m_currentSessionId = m_trackingService->createTrackingSession(deviceId, "DefaultSession");
                    }
                }
            }
            
            if (!m_currentSessionId.isEmpty()) {
                m_trackingService->startTracking(m_currentSessionId);
            }
        }
    } else {
        m_startTrackingBtn->setText("开始跟踪");
        m_trackingStatusLabel->setText("跟踪状态：已停止");
        m_logTextEdit->append(QString("[%1] 停止光学跟踪").arg(QDateTime::currentDateTime().toString()));
        
        if (m_trackingService && !m_currentSessionId.isEmpty()) {
            m_trackingService->stopTracking(m_currentSessionId);
        }
    }
}

void OpticalTrackingWidget::onLoadCTData()
{
#ifdef VTK_FOUND_AND_LINKED
    // 基于demo的CT加载实现
    QString fileName = QFileDialog::getOpenFileName(this, 
        "选择CT数据", "", 
        "DICOM文件 (*.dcm);;NIFTI文件 (*.nii *.nii.gz);;所有文件 (*.*)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    m_logTextEdit->append(QString("[%1] 开始加载CT数据：%2")
                         .arg(QDateTime::currentDateTime().toString()).arg(fileName));
    
    // 基于demo的加载逻辑
    if (fileName.endsWith(".gz") || fileName.endsWith(".nii")) {
        m_niftiReader->SetFileName(fileName.toStdString().c_str());
        m_niftiReader->Update();
        m_imageData = m_niftiReader->GetOutput();
    } else {
        // 对于DICOM，假设选择的是目录
        QFileInfo fileInfo(fileName);
        QString dirPath = fileInfo.absolutePath();
        
        m_dicomReader->SetDirectoryName(dirPath.toStdString().c_str());
        m_dicomReader->Update();
        m_imageData = m_dicomReader->GetOutput();
    }
    
    if (m_imageData) {
        // 配置体绘制（基于demo的体绘制配置）
        m_volumeMapper->SetInputData(m_imageData);
        m_volumeMapper->Update();
        
        // 设置传输函数（基于demo的配置）
        vtkSmartPointer<vtkPiecewiseFunction> compositeOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
        compositeOpacity->AddPoint(-3024, 0, 0.5, 0.0);
        compositeOpacity->AddPoint(-16, 0, .49, .61);
        compositeOpacity->AddPoint(641, .72, .5, 0.0);
        compositeOpacity->AddPoint(3071, .71, 0.5, 0.0);
        
        vtkSmartPointer<vtkColorTransferFunction> color = vtkSmartPointer<vtkColorTransferFunction>::New();
        color->AddRGBPoint(-3024, 0, 0, 0, 0.5, 0.0);
        color->AddRGBPoint(-16, 0.73, 0.25, 0.30, 0.49, .61);
        color->AddRGBPoint(641, .90, .82, .56, .5, 0.0);
        color->AddRGBPoint(3071, 1, 1, 1, .5, 0.0);
        
        vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
        volumeProperty->SetColor(color);
        volumeProperty->SetScalarOpacity(compositeOpacity);
        volumeProperty->SetInterpolationTypeToLinear();
        volumeProperty->ShadeOn();
        volumeProperty->SetAmbient(0.1);
        volumeProperty->SetDiffuse(0.9);
        volumeProperty->SetSpecular(0.2);
        volumeProperty->SetSpecularPower(10.0);
        
        m_volume->SetMapper(m_volumeMapper);
        m_volume->SetProperty(volumeProperty);
        m_volume->SetUserTransform(m_landmarkTransform);
        
        // 移除旧的volume并添加新的
        vtkVolumeCollection* allVolumes = m_renderer->GetVolumes();
        if (allVolumes->GetNextVolume() != nullptr) {
            m_renderer->RemoveVolume(allVolumes->GetNextVolume());
        }
        m_renderer->AddVolume(m_volume);
        
        m_renderWindow->Render();
        
        m_logTextEdit->append(QString("[%1] CT数据加载完成").arg(QDateTime::currentDateTime().toString()));
    } else {
        m_logTextEdit->append(QString("[%1] CT数据加载失败").arg(QDateTime::currentDateTime().toString()));
        QMessageBox::warning(this, "错误", "无法加载CT数据文件");
    }
#else
    // 非VTK环境的替代实现
    m_logTextEdit->append(QString("[%1] VTK功能未启用，无法加载CT数据").arg(QDateTime::currentDateTime().toString()));
    QMessageBox::information(this, "信息", "VTK功能未启用，无法加载CT数据");
#endif
}

void OpticalTrackingWidget::onStartUDPClient()
{
    m_isUDPActive = !m_isUDPActive;
    
    if (m_isUDPActive) {
        m_startUDPBtn->setText("停止UDP接收");
        m_udpStatusLabel->setText("UDP状态：接收中");
        m_udpTimer->start(50); // 50ms间隔，20fps
        m_logTextEdit->append(QString("[%1] 启动UDP数据接收").arg(QDateTime::currentDateTime().toString()));
    } else {
        m_startUDPBtn->setText("启动UDP接收");
        m_udpStatusLabel->setText("UDP状态：已停止");
        m_udpTimer->stop();
        m_logTextEdit->append(QString("[%1] 停止UDP数据接收").arg(QDateTime::currentDateTime().toString()));
    }
}

void OpticalTrackingWidget::onStopUDPClient()
{
    // 强制停止UDP接收
    if (m_isUDPActive) {
        m_isUDPActive = false;
        m_startUDPBtn->setText("启动UDP接收");
        m_udpStatusLabel->setText("UDP状态：已停止");
        m_udpTimer->stop();
        m_logTextEdit->append(QString("[%1] 强制停止UDP数据接收").arg(QDateTime::currentDateTime().toString()));
    }
}

void OpticalTrackingWidget::onResetView()
{
#ifdef VTK_FOUND_AND_LINKED
    m_renderer->ResetCamera();
    m_renderWindow->Render();
    m_logTextEdit->append(QString("[%1] 重置3D视角").arg(QDateTime::currentDateTime().toString()));
#else
    m_logTextEdit->append(QString("[%1] VTK功能未启用，无法重置视角").arg(QDateTime::currentDateTime().toString()));
#endif
}

void OpticalTrackingWidget::onTogglePickPoints()
{
    m_pickPointsMode = true;
    setProperty("pickPointsMode", true);
    m_logTextEdit->append(QString("[%1] 切换到点选模式").arg(QDateTime::currentDateTime().toString()));
}

void OpticalTrackingWidget::onToggleViewInteractor()
{
    m_pickPointsMode = false;
    setProperty("pickPointsMode", false);
    m_logTextEdit->append(QString("[%1] 切换到视角控制模式").arg(QDateTime::currentDateTime().toString()));
}

void OpticalTrackingWidget::onStartPivotCalibration()
{
    m_isPivotCalibrating = true;
    m_pivotCalibrationBtn->setEnabled(false);
    m_calibrationProgress->setVisible(true);
    m_calibrationProgress->setValue(0);
    m_calibrationStatusLabel->setText("校准状态：准备中...");
    
    m_pivotPrepareTimer->start();
    
    m_logTextEdit->append(QString("[%1] 开始针尖校准，准备时间：%2秒")
                         .arg(QDateTime::currentDateTime().toString()).arg(m_prepareTime));
}

void OpticalTrackingWidget::onGetTargetPoint()
{
    // 基于demo的获取针尖点实现
    if (m_probeTipActor) {
        double* tipCenter = m_probeTipActor->GetCenter();
        onAddPoint(tipCenter[0], tipCenter[1], tipCenter[2], 1); // type 1 = 针尖点
        
        m_logTextEdit->append(QString("[%1] 获取目标点：(%.2f, %.2f, %.2f)")
                             .arg(QDateTime::currentDateTime().toString())
                             .arg(tipCenter[0]).arg(tipCenter[1]).arg(tipCenter[2]));
    }
}

void OpticalTrackingWidget::onPointRegistration()
{
#ifdef VTK_FOUND_AND_LINKED
    // 基于demo的点配准实现
    int sourceNum = m_sourcePoints->GetNumberOfPoints();
    int targetNum = m_targetPoints->GetNumberOfPoints();
    
    m_logTextEdit->append(QString("[%1] 开始点配准，源点数：%2，目标点数：%3")
                         .arg(QDateTime::currentDateTime().toString()).arg(sourceNum).arg(targetNum));
    
    if (sourceNum < 3 || targetNum < 3 || sourceNum != targetNum) {
        QMessageBox::warning(this, "警告", "源点集或目标点集数量不符合要求！\n需要至少3个对应点");
        return;
    }
    
    // 执行配准
    m_landmarkTransform->SetSourceLandmarks(m_sourcePoints);
    m_landmarkTransform->SetTargetLandmarks(m_targetPoints);
    m_landmarkTransform->SetModeToRigidBody();
    m_landmarkTransform->Update();
    
    m_logTextEdit->append(QString("[%1] 点配准完成").arg(QDateTime::currentDateTime().toString()));
    
    // 更新渲染
    m_renderWindow->Render();
#else
    m_logTextEdit->append(QString("[%1] VTK功能未启用，无法进行点配准").arg(QDateTime::currentDateTime().toString()));
    QMessageBox::information(this, "信息", "VTK功能未启用，无法进行点配准");
#endif
}

void OpticalTrackingWidget::onAddPoint(double x, double y, double z, int type)
{
#ifdef VTK_FOUND_AND_LINKED
    // 基于demo的添加点实现
    vtkSmartPointer<vtkSphereSource> sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetCenter(x, y, z);
    sphereSource->SetRadius(2.0);
    sphereSource->SetPhiResolution(20);
    sphereSource->SetThetaResolution(20);
    
    vtkSmartPointer<vtkPolyDataMapper> sphereMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    sphereMapper->SetInputConnection(sphereSource->GetOutputPort());
    
    vtkSmartPointer<vtkActor> sphereActor = vtkSmartPointer<vtkActor>::New();
    sphereActor->SetMapper(sphereMapper);
    
    double point[3] = {x, y, z};
    
    if (type == 1) {
        // 针尖获取的点（红色）
        sphereActor->GetProperty()->SetColor(1.0, 0.0, 0.0);
        m_targetPoints->InsertNextPoint(point);
        m_targetPointActors.append(sphereActor);
        
        m_logTextEdit->append(QString("[%1] 添加目标点：(%.2f, %.2f, %.2f)")
                             .arg(QDateTime::currentDateTime().toString()).arg(x).arg(y).arg(z));
    } else if (type == 2) {
        // VTK中选择的点（蓝色）
        sphereActor->GetProperty()->SetColor(0.0, 0.0, 1.0);
        m_sourcePoints->InsertNextPoint(point);
        m_sourcePointActors.append(sphereActor);
        
        m_logTextEdit->append(QString("[%1] 添加源点：(%.2f, %.2f, %.2f)")
                             .arg(QDateTime::currentDateTime().toString()).arg(x).arg(y).arg(z));
    }
    
    m_renderer->AddActor(sphereActor);
    m_renderWindow->Render();
#else
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(z)
    Q_UNUSED(type)
    m_logTextEdit->append(QString("[%1] VTK功能未启用，无法添加点").arg(QDateTime::currentDateTime().toString()));
#endif
}

// UDP通信实现
void OpticalTrackingWidget::readPendingDatagrams()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        // 解析数据（基于demo的parseBuffer）
        parseUDPBuffer(datagram);
    }
}

void OpticalTrackingWidget::parseUDPBuffer(const QByteArray& buffer)
{
    // 基于demo的parseBuffer实现
    std::istringstream stream(buffer.toStdString());
    std::string line;
    
    // 临时存储结构
    struct Marker {
        std::string geometryId;
        float translationMM[3];
        float rotation[3][3];
        float registrationErrorMM;
    };
    
    std::vector<Marker> markers;
    m_markerPoints.clear();
    
    while (std::getline(stream, line)) {
        // 解析点坐标
        size_t indexStart = line.find("Index");
        if (indexStart != std::string::npos) {
            size_t xyzStart = line.find("XYZ:(") + 5;
            size_t xyzEnd = line.find(")");
            std::string xyz = line.substr(xyzStart, xyzEnd - xyzStart);
            std::istringstream xyzStream(xyz);
            float x, y, z;
            xyzStream >> x >> y >> z;
            QVector3D pointCoord(x, y, z - 1200.0f); // 基于demo的偏移
            m_markerPoints.append(pointCoord);
        }
        
        // 解析几何体数据
        size_t geometryStart = line.find("geometry");
        if (geometryStart != std::string::npos) {
            Marker marker;
            // 解析几何ID
            size_t geometryStart = line.find("geometry ") + 9;
            size_t geometryEnd = line.find(" (");
            marker.geometryId = line.substr(geometryStart, geometryEnd - geometryStart);
            
            // 解析位置
            std::getline(stream, line);
            size_t posStart = line.find("(") + 1;
            size_t posEnd = line.find(")");
            std::string posStr = line.substr(posStart, posEnd - posStart);
            std::istringstream posStream(posStr);
            posStream >> marker.translationMM[0] >> marker.translationMM[1] >> marker.translationMM[2];
            marker.registrationErrorMM = 0.0f;
            
            // 解析旋转矩阵
            std::getline(stream, line);
            size_t rotStart = line.find("(") + 1;
            size_t rotEnd = line.find(")");
            std::string rotStr = line.substr(rotStart, rotEnd - rotStart);
            std::istringstream rotStream(rotStr);
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    rotStream >> marker.rotation[r][c];
                }
            }
            markers.push_back(marker);
        }
        
        // 解析相对变换
        size_t relativeStart = line.find("relative to");
        if (relativeStart != std::string::npos) {
            // 解析相对平移
            std::getline(stream, line);
            size_t transStart = line.find("(") + 1;
            size_t transEnd = line.find(")");
            std::string transStr = line.substr(transStart, transEnd - transStart);
            std::istringstream transStream(transStr);
            transStream >> m_relativeTransformMatrix[0][3] >> m_relativeTransformMatrix[1][3] >> m_relativeTransformMatrix[2][3];
            
            // 解析相对旋转
            std::getline(stream, line);
            size_t rotStart = line.find("(") + 1;
            size_t rotEnd = line.find(")");
            std::string rotStr = line.substr(rotStart, rotEnd - rotStart);
            std::istringstream rotStream(rotStr);
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    rotStream >> m_relativeTransformMatrix[r][c];
                }
            }
        }
    }
    
    // 更新变换矩阵
#ifdef VTK_FOUND_AND_LINKED
    for (const auto& marker : markers) {
        if (marker.geometryId == "20") {
            setTransformMatrix(marker.translationMM, marker.rotation, m_refToTrackMatrix);
        } else {
            setTransformMatrix(marker.translationMM, marker.rotation, m_centerToTrackMatrix);
        }
    }
#endif
}

#ifdef VTK_FOUND_AND_LINKED
void OpticalTrackingWidget::setTransformMatrix(const float translation[3], const float rotation[3][3], vtkMatrix4x4* matrix)
{
    // 基于demo的setMatrixValue实现
    matrix->SetElement(0, 3, translation[0]);
    matrix->SetElement(1, 3, translation[1]);
    matrix->SetElement(2, 3, translation[2] - 1200.0); // 基于demo的偏移
    
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            matrix->SetElement(r, c, rotation[r][c]);
        }
    }
}
#endif

void OpticalTrackingWidget::onUdpDataReceived()
{
    // 更新帧率统计
    m_frameCount++;
    static QTime lastTime = QTime::currentTime();
    QTime currentTime = QTime::currentTime();
    int elapsed = lastTime.msecsTo(currentTime);
    
    if (elapsed >= 1000) { // 每秒更新一次帧率
        m_frameRate = (double)m_frameCount * 1000.0 / elapsed;
        m_frameRateLabel->setText(QString("帧率：%.1f fps").arg(m_frameRate));
        m_frameCount = 0;
        lastTime = currentTime;
    }
}

// 针尖校准相关实现
void OpticalTrackingWidget::onPivotPrepareTimeout()
{
    m_pivotPrepareTimer->stop();
    m_calibrationStatusLabel->setText("校准状态：采样中...");
    m_calibrationProgress->setValue(33);
    m_pivotSamplingTimer->start();
    
    m_logTextEdit->append(QString("[%1] 开始校准采样，采样时间：%2秒")
                         .arg(QDateTime::currentDateTime().toString()).arg(m_samplingTime));
}

void OpticalTrackingWidget::onPivotSamplingTimeout()
{
    m_pivotSamplingTimer->stop();
    m_calibrationStatusLabel->setText("校准状态：计算中...");
    m_calibrationProgress->setValue(66);
    
    // 模拟校准计算
    QTimer::singleShot(2000, this, &OpticalTrackingWidget::onCalibrationComplete);
    
    m_logTextEdit->append(QString("[%1] 开始校准计算").arg(QDateTime::currentDateTime().toString()));
}

void OpticalTrackingWidget::onCalibrationComplete()
{
    m_isPivotCalibrating = false;
    m_pivotCalibrationBtn->setEnabled(true);
    m_calibrationProgress->setValue(100);
    m_calibrationStatusLabel->setText("校准状态：完成");
    
    // 隐藏进度条
    QTimer::singleShot(3000, [this]() {
        m_calibrationProgress->setVisible(false);
        m_calibrationStatusLabel->setText("校准状态：就绪");
    });
    
    m_logTextEdit->append(QString("[%1] 针尖校准完成").arg(QDateTime::currentDateTime().toString()));
}

// 渲染更新
void OpticalTrackingWidget::updateToolVisualization()
{
    // 基于demo的updateStylet实现
    if (m_centerToTrackMatrix && m_centerToTrackTransform) {
        m_centerToTrackTransform->SetMatrix(m_centerToTrackMatrix);
        m_centerToTrackTransform->Update();
    }
    
    if (m_tipToTrackTransform) {
        m_tipToTrackTransform->Update();
    }
    
    if (m_refToTrackMatrix && m_refToTrackTransform) {
        m_refToTrackTransform->SetMatrix(m_refToTrackMatrix);
        m_refToTrackTransform->Update();
    }
    
    // 更新探针线段
    if (m_probeCenterActor && m_probeTipActor && m_probeLineSource) {
        double* centerPos = m_probeCenterActor->GetCenter();
        double* tipPos = m_probeTipActor->GetCenter();
        
        m_probeLineSource->SetPoint1(centerPos);
        m_probeLineSource->SetPoint2(tipPos);
        m_probeLineSource->Update();
        
        // 保存坐标
        for (int i = 0; i < 3; ++i) {
            m_centerCoordinate[i] = centerPos[i];
            m_tipCoordinate[i] = tipPos[i];
        }
    }
}

void OpticalTrackingWidget::renderScene()
{
    if (m_isUDPActive) {
        updateToolVisualization();
    }
    
    m_renderWindow->Render();
}

void OpticalTrackingWidget::setTrackingService(OpticalTrackingServiceImpl* service)
{
    m_trackingService = service;
    
    if (m_trackingService) {
        m_deviceStatusLabel->setText("设备状态：已连接");
        m_logTextEdit->append(QString("[%1] 跟踪服务已连接")
                             .arg(QDateTime::currentDateTime().toString()));
    }
}

// 未使用的方法存根
void OpticalTrackingWidget::updateCameraViews() {}
void OpticalTrackingWidget::processTransformMatrices(const QList<QList<float>>&, const QList<QList<float>>&, const QList<QList<float>>&) {}
void OpticalTrackingWidget::processMarkerPoints(const QList<QVector3D>&) {}
void OpticalTrackingWidget::onTransformUpdate(const QList<QList<float>>&, const QList<QList<float>>&, const QList<QList<float>>&) {}
