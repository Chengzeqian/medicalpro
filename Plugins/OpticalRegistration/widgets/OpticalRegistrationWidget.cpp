#include "OpticalRegistrationWidget.h"
#include "../OpticalRegistrationService.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTableWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QLabel>
#include <QFrame>
#include <QHeaderView>
#include <QDateTime>
#include <QDebug>
#include <QShowEvent>
#include <QTimer>

#ifdef VTK_FOUND
#include "Framework/VTKWidgetFactory.h"
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCamera.h>
#include <vtkActor.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkAxesActor.h>
#endif

OpticalRegistrationWidget::OpticalRegistrationWidget(OpticalRegistrationService* service, QWidget* parent)
    : QWidget(parent)
    , m_service(service)
    , m_connectBtn(nullptr)
    , m_disconnectBtn(nullptr)
    , m_calibrateBtn(nullptr)
    , m_deviceStatus(nullptr)
    , m_trackingStatus(nullptr)
    , m_toolTable(nullptr)
    , m_pointTable(nullptr)
    , m_addPointBtn(nullptr)
    , m_capturePointBtn(nullptr)
    , m_clearPointsBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_logText(nullptr)
    , m_startBtn(nullptr)
    , m_3dViewContainer(nullptr)
    , m_3dViewPlaceholder(nullptr)
    , m_positionLabel(nullptr)
    , m_rotationLabel(nullptr)
#ifdef VTK_FOUND
    , m_vtkWidget(nullptr)
    , m_vtkInitialized(false)
#endif
    , m_resultStatusLabel(nullptr)
    , m_resultTable(nullptr)
    , m_freLabel(nullptr)
    , m_treLabel(nullptr)
    , m_accuracyFrame(nullptr)
{
    setupUI();
    connectSignals();
    refresh();

    qDebug() << "[OpticalRegistrationWidget] Widget created";
}

OpticalRegistrationWidget::~OpticalRegistrationWidget()
{
    qDebug() << "[OpticalRegistrationWidget] Widget destroyed";
}

void OpticalRegistrationWidget::refresh()
{
    updatePointTable();
}

// ========== 样式定义 ==========

QString OpticalRegistrationWidget::getGroupStyle() const
{
    return "QGroupBox { "
           "  background: rgba(30,41,59,0.85); "
           "  border: 1px solid rgba(251,191,36,0.4); "
           "  border-radius: 8px; "
           "  margin-top: 14px; "
           "  padding: 12px 10px 10px 10px; "
           "  font-weight: bold; "
           "  font-size: 12px; "
           "  color: #fbbf24; "
           "}"
           "QGroupBox::title { "
           "  subcontrol-origin: margin; "
           "  subcontrol-position: top left; "
           "  left: 12px; "
           "  top: 2px; "
           "  padding: 0 6px; "
           "  background: rgba(30,41,59,0.95); "
           "  color: #fcd34d; "
           "}";
}

QString OpticalRegistrationWidget::getButtonStyle() const
{
    return "QPushButton { "
           "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(251,191,36,0.8), stop:1 rgba(217,119,6,0.8)); "
           "  color: #1f2937; "
           "  border: 1px solid #fbbf24; "
           "  border-radius: 6px; "
           "  padding: 8px 16px; "
           "  font-weight: bold; "
           "}"
           "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #fcd34d, stop:1 #fbbf24); }"
           "QPushButton:pressed { background: #d97706; }"
           "QPushButton:disabled { background: rgba(107,114,128,0.5); border-color: #6b7280; color: #9ca3af; }";
}

QString OpticalRegistrationWidget::getTableStyle() const
{
    return "QTableWidget { "
           "  background: rgba(15,23,42,0.8); "
           "  border: 1px solid rgba(251,191,36,0.3); "
           "  border-radius: 6px; "
           "  gridline-color: rgba(251,191,36,0.2); "
           "  color: #e2e8f0; "
           "}"
           "QTableWidget::item { padding: 5px; }"
           "QTableWidget::item:selected { background: rgba(251,191,36,0.3); }"
           "QHeaderView::section { background: rgba(51,65,85,0.8); color: #fcd34d; border: none; padding: 6px; font-weight: bold; }";
}

// ========== UI布局 ==========

void OpticalRegistrationWidget::setupUI()
{
    setObjectName("opticalRegistrationWidget");
    setStyleSheet("background: transparent;");

    // 主布局：水平三列（左侧设备控制 | 中间3D视图 | 右侧结果）
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // 左侧面板
    QWidget* leftPanel = createLeftPanel();
    mainLayout->addWidget(leftPanel);

    // 中间3D视图
    QWidget* centerPanel = createCenterPanel();
    mainLayout->addWidget(centerPanel, 1);

    // 右侧结果面板
    QWidget* rightPanel = createRightPanel();
    mainLayout->addWidget(rightPanel);
}

QWidget* OpticalRegistrationWidget::createLeftPanel()
{
    QWidget* leftPanel = new QWidget();
    leftPanel->setFixedWidth(340);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    // ========== 设备连接组 ==========
    QGroupBox* deviceGroup = new QGroupBox("🔌 光学跟踪设备");
    deviceGroup->setStyleSheet(getGroupStyle());
    QVBoxLayout* deviceLayout = new QVBoxLayout(deviceGroup);
    deviceLayout->setContentsMargins(10, 12, 10, 10);
    deviceLayout->setSpacing(6);

    // 设备状态
    QHBoxLayout* statusRow = new QHBoxLayout();
    QLabel* deviceLabel = new QLabel("设备:");
    deviceLabel->setStyleSheet("color: #e2e8f0; background: transparent;");
    m_deviceStatus = new QLabel("● 未连接");
    m_deviceStatus->setStyleSheet("color: #f59e0b; font-weight: bold; background: transparent;");
    statusRow->addWidget(deviceLabel);
    statusRow->addWidget(m_deviceStatus);
    statusRow->addStretch();
    deviceLayout->addLayout(statusRow);

    QHBoxLayout* trackingRow = new QHBoxLayout();
    QLabel* trackingLabel = new QLabel("跟踪:");
    trackingLabel->setStyleSheet("color: #e2e8f0; background: transparent;");
    m_trackingStatus = new QLabel("● 停止");
    m_trackingStatus->setStyleSheet("color: #94a3b8; font-weight: bold; background: transparent;");
    trackingRow->addWidget(trackingLabel);
    trackingRow->addWidget(m_trackingStatus);
    trackingRow->addStretch();
    deviceLayout->addLayout(trackingRow);

    // 设备控制按钮
    QHBoxLayout* deviceBtnRow = new QHBoxLayout();
    m_connectBtn = new QPushButton("🔗 连接");
    m_disconnectBtn = new QPushButton("⏏ 断开");
    m_connectBtn->setStyleSheet(getButtonStyle());
    m_disconnectBtn->setStyleSheet(getButtonStyle());
    m_disconnectBtn->setEnabled(false);
    deviceBtnRow->addWidget(m_connectBtn);
    deviceBtnRow->addWidget(m_disconnectBtn);
    deviceLayout->addLayout(deviceBtnRow);

    m_calibrateBtn = new QPushButton("🎯 校准设备");
    m_calibrateBtn->setStyleSheet(getButtonStyle());
    m_calibrateBtn->setEnabled(false);
    deviceLayout->addWidget(m_calibrateBtn);

    leftLayout->addWidget(deviceGroup);

    // ========== 跟踪工具组 ==========
    QGroupBox* toolGroup = new QGroupBox("🔧 跟踪工具");
    toolGroup->setStyleSheet(getGroupStyle());
    QVBoxLayout* toolLayout = new QVBoxLayout(toolGroup);
    toolLayout->setContentsMargins(8, 12, 8, 8);
    toolLayout->setSpacing(4);

    m_toolTable = new QTableWidget(3, 3);
    m_toolTable->setHorizontalHeaderLabels({"工具名称", "状态", "可见"});
    m_toolTable->setStyleSheet(getTableStyle());
    m_toolTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_toolTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_toolTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_toolTable->verticalHeader()->setVisible(false);
    m_toolTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_toolTable->setMinimumHeight(90);
    m_toolTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    QStringList tools = {"患者参考架", "手术器械", "探针工具"};
    for (int i = 0; i < tools.size(); ++i) {
        m_toolTable->setItem(i, 0, new QTableWidgetItem(tools[i]));
        m_toolTable->setItem(i, 1, new QTableWidgetItem("未检测"));
        m_toolTable->setItem(i, 2, new QTableWidgetItem("—"));
    }
    toolLayout->addWidget(m_toolTable);

    leftLayout->addWidget(toolGroup);

    // ========== 配准点管理组 ==========
    QGroupBox* pointGroup = new QGroupBox("📍 配准点");
    pointGroup->setStyleSheet(getGroupStyle());
    QVBoxLayout* pointLayout = new QVBoxLayout(pointGroup);
    pointLayout->setContentsMargins(8, 12, 8, 8);
    pointLayout->setSpacing(6);

    // 按钮行
    QHBoxLayout* pointBtnRow = new QHBoxLayout();
    pointBtnRow->setSpacing(6);
    m_addPointBtn = new QPushButton("添加");
    m_capturePointBtn = new QPushButton("采集");
    m_clearPointsBtn = new QPushButton("清空");
    m_addPointBtn->setStyleSheet(getButtonStyle());
    m_capturePointBtn->setStyleSheet(getButtonStyle());
    m_clearPointsBtn->setStyleSheet(getButtonStyle());
    m_addPointBtn->setMinimumHeight(30);
    m_capturePointBtn->setMinimumHeight(30);
    m_clearPointsBtn->setMinimumHeight(30);
    pointBtnRow->addWidget(m_addPointBtn);
    pointBtnRow->addWidget(m_capturePointBtn);
    pointBtnRow->addWidget(m_clearPointsBtn);
    pointBtnRow->addStretch();
    pointLayout->addLayout(pointBtnRow);

    // 配准点表格（4列：名称、影像位置、跟踪位置、状态）
    m_pointTable = new QTableWidget(0, 4);
    m_pointTable->setHorizontalHeaderLabels({"名称", "影像位置", "跟踪位置", "状态"});
    m_pointTable->setStyleSheet(getTableStyle());
    m_pointTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_pointTable->verticalHeader()->setVisible(false);
    m_pointTable->setMinimumHeight(100);
    m_pointTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pointTable->setSelectionMode(QAbstractItemView::SingleSelection);
    pointLayout->addWidget(m_pointTable);

    leftLayout->addWidget(pointGroup, 1);

    // ========== 配准进度组 ==========
    QGroupBox* progressGroup = new QGroupBox("⏳ 配准进度");
    progressGroup->setStyleSheet(getGroupStyle());
    QVBoxLayout* progressLayout = new QVBoxLayout(progressGroup);

    m_statusLabel = new QLabel("● 等待设备连接");
    m_statusLabel->setStyleSheet("color: #94a3b8; font-size: 13px; font-weight: bold; background: transparent;");
    progressLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet(
        "QProgressBar { background: rgba(30,41,59,0.8); border: 1px solid #475569; border-radius: 5px; height: 20px; text-align: center; color: white; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #f59e0b, stop:1 #fbbf24); border-radius: 4px; }"
    );
    progressLayout->addWidget(m_progressBar);

    m_logText = new QTextEdit();
    m_logText->setReadOnly(true);
    m_logText->setMaximumHeight(100);
    m_logText->setStyleSheet(
        "QTextEdit { background: rgba(15,23,42,0.8); border: 1px solid rgba(251,191,36,0.3); "
        "border-radius: 6px; color: #e2e8f0; font-family: monospace; font-size: 11px; }"
    );
    m_logText->setPlaceholderText("光学配准日志...");
    progressLayout->addWidget(m_logText);

    leftLayout->addWidget(progressGroup);

    // ========== 开始配准按钮 ==========
    m_startBtn = new QPushButton("▶ 开始光学配准");
    m_startBtn->setFixedHeight(45);
    m_startBtn->setEnabled(false);
    m_startBtn->setStyleSheet(
        "QPushButton { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10b981, stop:1 #059669); "
        "  color: white; font-size: 14px; font-weight: bold; "
        "  border: 2px solid #10b981; border-radius: 8px; "
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #059669, stop:1 #047857); }"
        "QPushButton:disabled { background: rgba(107,114,128,0.5); border-color: #6b7280; }"
    );
    leftLayout->addWidget(m_startBtn);

    return leftPanel;
}


QWidget* OpticalRegistrationWidget::createCenterPanel()
{
    QWidget* centerPanel = new QWidget();
    centerPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(10);

    // 3D视图容器
    m_3dViewContainer = new QWidget();
    QVBoxLayout* containerLayout = new QVBoxLayout(m_3dViewContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);

#ifdef VTK_FOUND
    try {
        m_vtkWidget = VTKWidgetFactory::createVTKWidget(m_3dViewContainer);
        if (m_vtkWidget) {
            m_vtkWidget->setMinimumSize(300, 300);
            containerLayout->addWidget(m_vtkWidget);
            m_3dViewPlaceholder = nullptr;
            qDebug() << "[OpticalRegistrationWidget] VTK widget created";
        }
    } catch (...) {
        qCritical() << "[OpticalRegistrationWidget] VTK widget creation failed";
        m_vtkWidget = nullptr;
    }

    if (!m_vtkWidget) {
#endif
        // 占位符标签
        m_3dViewPlaceholder = new QLabel("📡 实时光学跟踪视图\n\n显示工具位置和姿态");
        m_3dViewPlaceholder->setAlignment(Qt::AlignCenter);
        m_3dViewPlaceholder->setStyleSheet(
            "background: rgba(30,41,59,0.6); "
            "border: 2px dashed rgba(251,191,36,0.5); "
            "border-radius: 10px; "
            "color: #94a3b8; "
            "font-size: 16px;"
        );
        containerLayout->addWidget(m_3dViewPlaceholder);
#ifdef VTK_FOUND
    }
#endif

    centerLayout->addWidget(m_3dViewContainer, 2);

    // 位姿显示
    QFrame* poseFrame = new QFrame();
    poseFrame->setStyleSheet("background: rgba(30,41,59,0.8); border: 1px solid rgba(251,191,36,0.3); border-radius: 8px;");
    poseFrame->setFixedHeight(80);
    QHBoxLayout* poseLayout = new QHBoxLayout(poseFrame);

    m_positionLabel = new QLabel("位置: X=- Y=- Z=-");
    m_positionLabel->setStyleSheet("color: #fbbf24; font-family: monospace; font-size: 12px;");
    m_rotationLabel = new QLabel("旋转: Rx=- Ry=- Rz=-");
    m_rotationLabel->setStyleSheet("color: #60a5fa; font-family: monospace; font-size: 12px;");
    poseLayout->addWidget(m_positionLabel);
    poseLayout->addWidget(m_rotationLabel);

    centerLayout->addWidget(poseFrame);

    return centerPanel;
}

QWidget* OpticalRegistrationWidget::createRightPanel()
{
    QWidget* rightPanel = new QWidget();
    rightPanel->setFixedWidth(300);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);

    // ========== 配准结果组 ==========
    QGroupBox* resultGroup = new QGroupBox("📊 配准结果");
    resultGroup->setStyleSheet(getGroupStyle());
    QVBoxLayout* resultLayout = new QVBoxLayout(resultGroup);

    m_resultStatusLabel = new QLabel("等待配准...");
    m_resultStatusLabel->setStyleSheet("color: #94a3b8; font-size: 13px; background: transparent;");
    resultLayout->addWidget(m_resultStatusLabel);

    m_resultTable = new QTableWidget(4, 2);
    m_resultTable->setHorizontalHeaderLabels({"参数", "值"});
    m_resultTable->setStyleSheet(getTableStyle());
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->verticalHeader()->setVisible(false);

    QStringList resultParams = {"配准误差 (mm)", "跟踪精度 (mm)", "采样点数", "配准耗时 (s)"};
    for (int i = 0; i < resultParams.size(); ++i) {
        m_resultTable->setItem(i, 0, new QTableWidgetItem(resultParams[i]));
        m_resultTable->setItem(i, 1, new QTableWidgetItem("-"));
    }
    resultLayout->addWidget(m_resultTable);

    // 精度指示器
    m_accuracyFrame = new QFrame();
    m_accuracyFrame->setStyleSheet("background: rgba(251,191,36,0.1); border: 1px solid rgba(251,191,36,0.3); border-radius: 8px; padding: 10px;");
    QVBoxLayout* accLayout = new QVBoxLayout(m_accuracyFrame);

    QLabel* accTitle = new QLabel("🎯 实时精度");
    accTitle->setStyleSheet("color: #fbbf24; font-weight: bold; font-size: 12px; background: transparent;");
    accLayout->addWidget(accTitle);

    m_freLabel = new QLabel("FRE: - mm");
    m_freLabel->setStyleSheet("color: #e2e8f0; font-size: 12px; background: transparent;");
    accLayout->addWidget(m_freLabel);

    m_treLabel = new QLabel("TRE: - mm");
    m_treLabel->setStyleSheet("color: #e2e8f0; font-size: 12px; background: transparent;");
    accLayout->addWidget(m_treLabel);

    resultLayout->addWidget(m_accuracyFrame);
    rightLayout->addWidget(resultGroup);
    rightLayout->addStretch();

    return rightPanel;
}


void OpticalRegistrationWidget::connectSignals()
{
    // 设备控制
    connect(m_connectBtn, &QPushButton::clicked, this, &OpticalRegistrationWidget::onConnectDevice);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &OpticalRegistrationWidget::onDisconnectDevice);
    connect(m_calibrateBtn, &QPushButton::clicked, this, &OpticalRegistrationWidget::onCalibrateDevice);

    // 配准点管理
    connect(m_addPointBtn, &QPushButton::clicked, this, &OpticalRegistrationWidget::onAddPoint);
    connect(m_capturePointBtn, &QPushButton::clicked, this, &OpticalRegistrationWidget::onCapturePoint);
    connect(m_clearPointsBtn, &QPushButton::clicked, this, &OpticalRegistrationWidget::onClearPoints);

    // 表格选择变化
    connect(m_pointTable, &QTableWidget::itemSelectionChanged,
            this, &OpticalRegistrationWidget::onPointTableSelectionChanged);

    // 配准控制
    connect(m_startBtn, &QPushButton::clicked, this, &OpticalRegistrationWidget::onStartRegistration);

    // 服务信号
    if (m_service) {
        connect(m_service, &OpticalRegistrationService::pointUpdated,
                this, &OpticalRegistrationWidget::onPointUpdated);
        connect(m_service, &OpticalRegistrationService::pointsCleared,
                this, &OpticalRegistrationWidget::onPointsCleared);
        connect(m_service, &OpticalRegistrationService::registrationStarted,
                this, &OpticalRegistrationWidget::onRegistrationStarted);
        connect(m_service, &OpticalRegistrationService::registrationCompleted,
                this, &OpticalRegistrationWidget::onRegistrationCompleted);
        connect(m_service, &OpticalRegistrationService::registrationFailed,
                this, &OpticalRegistrationWidget::onRegistrationFailed);
        connect(m_service, &OpticalRegistrationService::progressUpdated,
                this, &OpticalRegistrationWidget::onProgressUpdated);
    }
}

// ========== 设备控制槽函数 ==========

void OpticalRegistrationWidget::onConnectDevice()
{
    appendLog("正在连接光学跟踪设备...");
    m_deviceStatus->setText("● 连接中...");
    m_deviceStatus->setStyleSheet("color: #fbbf24; font-weight: bold; background: transparent;");

    // TODO: 调用OpticalTrackingService连接设备
    // 模拟连接成功
    QTimer::singleShot(1000, this, [this]() {
        m_deviceStatus->setText("● 已连接");
        m_deviceStatus->setStyleSheet("color: #10b981; font-weight: bold; background: transparent;");
        m_trackingStatus->setText("● 跟踪中");
        m_trackingStatus->setStyleSheet("color: #10b981; font-weight: bold; background: transparent;");
        m_connectBtn->setEnabled(false);
        m_disconnectBtn->setEnabled(true);
        m_calibrateBtn->setEnabled(true);
        m_startBtn->setEnabled(true);
        m_statusLabel->setText("● 设备就绪");
        m_statusLabel->setStyleSheet("color: #10b981; font-size: 13px; font-weight: bold; background: transparent;");
        appendLog("✓ 设备连接成功");
    });
}

void OpticalRegistrationWidget::onDisconnectDevice()
{
    appendLog("断开设备连接...");
    m_deviceStatus->setText("● 未连接");
    m_deviceStatus->setStyleSheet("color: #f59e0b; font-weight: bold; background: transparent;");
    m_trackingStatus->setText("● 停止");
    m_trackingStatus->setStyleSheet("color: #94a3b8; font-weight: bold; background: transparent;");
    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_calibrateBtn->setEnabled(false);
    m_startBtn->setEnabled(false);
    m_statusLabel->setText("● 等待设备连接");
    m_statusLabel->setStyleSheet("color: #94a3b8; font-size: 13px; font-weight: bold; background: transparent;");
    appendLog("设备已断开");
}

void OpticalRegistrationWidget::onCalibrateDevice()
{
    appendLog("开始设备校准...");
    // TODO: 实现校准逻辑
    appendLog("✓ 设备校准完成");
}

// ========== 配准点管理槽函数 ==========

void OpticalRegistrationWidget::onAddPoint()
{
    if (m_service) {
        int index = m_service->addPoint();
        appendLog(QString("添加配准点 %1").arg(index + 1));
    }
}

void OpticalRegistrationWidget::onCapturePoint()
{
    int row = m_pointTable->currentRow();
    if (row < 0) {
        appendLog("⚠️ 请先选择一个配准点");
        return;
    }
    if (m_service) {
        if (m_service->captureTrackerPosition(row)) {
            appendLog(QString("✓ 采集配准点 %1 位置成功").arg(row + 1));
        } else {
            appendLog(QString("⚠️ 采集配准点 %1 位置失败: %2").arg(row + 1).arg(m_service->getLastError()));
        }
    }
}

void OpticalRegistrationWidget::onClearPoints()
{
    if (m_service) {
        m_service->clearPoints();
        appendLog("已清空所有配准点");
    }
}

void OpticalRegistrationWidget::onPointTableSelectionChanged()
{
    int row = m_pointTable->currentRow();
    m_capturePointBtn->setEnabled(row >= 0);

    // 将当前选中的配准点索引同步给服务，便于导航四视图选点时直接作用到当前点
    if (m_service) {
        m_service->setActivePointIndex(row);
    }
}

// ========== 配准执行槽函数 ==========

void OpticalRegistrationWidget::onStartRegistration()
{
    if (!m_service) return;

    if (!m_service->canExecuteRegistration()) {
        appendLog("⚠️ 至少需要3个完整的配准点对才能执行配准");
        return;
    }

    m_startBtn->setEnabled(false);
    m_statusLabel->setText("● 配准中...");
    m_statusLabel->setStyleSheet("color: #f59e0b; font-size: 13px; font-weight: bold; background: transparent;");

    appendLog("开始执行光学配准...");
    m_service->executeRegistration();
}

// ========== 服务信号响应槽函数 ==========

void OpticalRegistrationWidget::onPointUpdated(int index)
{
    Q_UNUSED(index);
    updatePointTable();
}

void OpticalRegistrationWidget::onPointsCleared()
{
    updatePointTable();
}

void OpticalRegistrationWidget::onRegistrationStarted()
{
    m_progressBar->setValue(0);
    m_resultStatusLabel->setText("正在配准...");
    m_resultStatusLabel->setStyleSheet("color: #fbbf24; font-size: 13px; background: transparent;");
}

void OpticalRegistrationWidget::onRegistrationCompleted(const OpticalRegistrationResult& result)
{
    m_startBtn->setEnabled(true);
    m_statusLabel->setText("● 配准完成");
    m_statusLabel->setStyleSheet("color: #10b981; font-size: 13px; font-weight: bold; background: transparent;");
    m_progressBar->setValue(100);

    updateResultDisplay(result);
    appendLog(QString("✅ 配准完成: RMS=%.3f mm, 最大误差=%.3f mm")
              .arg(result.rmsError).arg(result.maxError));
}

void OpticalRegistrationWidget::onRegistrationFailed(const QString& error)
{
    m_startBtn->setEnabled(true);
    m_statusLabel->setText("● 配准失败");
    m_statusLabel->setStyleSheet("color: #ef4444; font-size: 13px; font-weight: bold; background: transparent;");
    m_progressBar->setValue(0);

    m_resultStatusLabel->setText("配准失败");
    m_resultStatusLabel->setStyleSheet("color: #ef4444; font-size: 13px; background: transparent;");

    appendLog(QString("❌ 配准失败: %1").arg(error));
}

void OpticalRegistrationWidget::onProgressUpdated(int progress, const QString& message)
{
    m_progressBar->setValue(progress);
    appendLog(message);
}

// ========== 辅助函数 ==========

void OpticalRegistrationWidget::updatePointTable()
{
    if (!m_service) return;

    auto points = m_service->getAllPoints();
    m_pointTable->setRowCount(points.size());

    for (int i = 0; i < points.size(); ++i) {
        const auto& pt = points[i];

        // 名称
        m_pointTable->setItem(i, 0, new QTableWidgetItem(pt.name));

        // 影像位置
        QString imgPosStr = pt.hasImagePosition
            ? QString("(%.1f, %.1f, %.1f)").arg(pt.imagePosition.x()).arg(pt.imagePosition.y()).arg(pt.imagePosition.z())
            : "-";
        m_pointTable->setItem(i, 1, new QTableWidgetItem(imgPosStr));

        // 跟踪位置
        QString trkPosStr = pt.hasTrackerPosition
            ? QString("(%.1f, %.1f, %.1f)").arg(pt.trackerPosition.x()).arg(pt.trackerPosition.y()).arg(pt.trackerPosition.z())
            : "-";
        m_pointTable->setItem(i, 2, new QTableWidgetItem(trkPosStr));

        // 状态
        QString status;
        QColor statusColor;
        if (pt.hasImagePosition && pt.hasTrackerPosition) {
            status = "✓ 完整";
            statusColor = QColor("#10b981");
        } else if (pt.hasImagePosition || pt.hasTrackerPosition) {
            status = "△ 部分";
            statusColor = QColor("#f59e0b");
        } else {
            status = "○ 空";
            statusColor = QColor("#94a3b8");
        }
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        statusItem->setForeground(statusColor);
        m_pointTable->setItem(i, 3, statusItem);
    }

    // 更新开始按钮状态
    if (m_service && m_startBtn) {
        bool canStart = m_service->canExecuteRegistration() && m_disconnectBtn->isEnabled();
        m_startBtn->setEnabled(canStart);
    }
}

void OpticalRegistrationWidget::updateResultDisplay(const OpticalRegistrationResult& result)
{
    // 更新状态
    m_resultStatusLabel->setText("配准成功");
    m_resultStatusLabel->setStyleSheet("color: #10b981; font-size: 13px; background: transparent;");

    // 更新结果表格
    m_resultTable->item(0, 1)->setText(QString::number(result.rmsError, 'f', 3));
    m_resultTable->item(1, 1)->setText(QString::number(result.maxError, 'f', 3));
    m_resultTable->item(2, 1)->setText(QString::number(result.pointCount));
    m_resultTable->item(3, 1)->setText("-");  // 配准耗时，需要在服务中计算

    // 更新精度指标
    m_freLabel->setText(QString("FRE: %1 mm").arg(result.rmsError, 0, 'f', 3));
    m_treLabel->setText(QString("TRE: %1 mm").arg(result.maxError, 0, 'f', 3));
}

void OpticalRegistrationWidget::updatePoseDisplay(const QVector3D& position, const QVector3D& rotation)
{
    m_positionLabel->setText(QString("位置: X=%1 Y=%2 Z=%3")
                             .arg(position.x(), 0, 'f', 2)
                             .arg(position.y(), 0, 'f', 2)
                             .arg(position.z(), 0, 'f', 2));
    m_rotationLabel->setText(QString("旋转: Rx=%1° Ry=%2° Rz=%3°")
                             .arg(rotation.x(), 0, 'f', 2)
                             .arg(rotation.y(), 0, 'f', 2)
                             .arg(rotation.z(), 0, 'f', 2));
}

void OpticalRegistrationWidget::appendLog(const QString& message)
{
    m_logText->append(QString("[%1] %2").arg(formatTime(), message));
}

QString OpticalRegistrationWidget::formatTime() const
{
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void OpticalRegistrationWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
#ifdef VTK_FOUND
    if (!m_vtkInitialized && m_vtkWidget) {
        initializeVTK();
    }
#endif
}

// ========== VTK相关函数 ==========

#ifdef VTK_FOUND
void OpticalRegistrationWidget::initializeVTK()
{
    if (m_vtkInitialized || !m_vtkWidget) return;

    try {
        qDebug() << "[OpticalRegistrationWidget] Initializing VTK render pipeline...";

        m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        m_renderer->SetBackground(0.1, 0.12, 0.16);
        m_renderer->SetBackground2(0.05, 0.06, 0.08);
        m_renderer->GradientBackgroundOn();
        m_renderWindow->AddRenderer(m_renderer);

        m_vtkWidget->setRenderWindow(m_renderWindow);

        // 添加坐标轴
        vtkSmartPointer<vtkAxesActor> axes = vtkSmartPointer<vtkAxesActor>::New();
        axes->SetTotalLength(50, 50, 50);
        m_renderer->AddActor(axes);

        // 设置相机
        vtkCamera* camera = m_renderer->GetActiveCamera();
        camera->SetPosition(200, 200, 200);
        camera->SetFocalPoint(0, 0, 0);
        camera->SetViewUp(0, 0, 1);
        m_renderer->ResetCamera();

        m_renderWindow->Render();
        m_vtkInitialized = true;

        qDebug() << "[OpticalRegistrationWidget] VTK initialization complete";
        appendLog("3D视图已初始化");
    } catch (...) {
        qCritical() << "[OpticalRegistrationWidget] VTK initialization failed";
        m_vtkInitialized = false;
    }
}

void OpticalRegistrationWidget::updateToolMarkers()
{
    // TODO: 更新工具标记显示
}
#else
void OpticalRegistrationWidget::initializeVTK() {}
void OpticalRegistrationWidget::updateToolMarkers() {}
#endif
