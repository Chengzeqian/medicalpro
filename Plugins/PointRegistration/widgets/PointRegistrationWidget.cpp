#include "PointRegistrationWidget.h"
#include "../PointRegistrationService.h"

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
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QShowEvent>

#include <QFileDialog>
#include <QFileInfo>

#ifdef VTK_FOUND
#include "Framework/VTKWidgetFactory.h"
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCamera.h>
#include <vtkActor.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkPointPicker.h>
#include <vtkCallbackCommand.h>
#include <vtkObjectFactory.h>
#include <vtkSTLReader.h>
#include <vtkOBJReader.h>
#include <vtkPLYReader.h>
#include <vtkPolyData.h>
#endif

// 静态回调函数用于VTK点击事件
#ifdef VTK_FOUND
static PointRegistrationWidget* g_currentWidget = nullptr;

static void OnLeftButtonDown(vtkObject* caller, unsigned long eventId, void* clientData, void* callData)
{
    Q_UNUSED(eventId); Q_UNUSED(callData);

    PointRegistrationWidget* widget = static_cast<PointRegistrationWidget*>(clientData);
    if (!widget) return;

    vtkRenderWindowInteractor* interactor = static_cast<vtkRenderWindowInteractor*>(caller);
    if (!interactor) return;

    int* clickPos = interactor->GetEventPosition();

    vtkSmartPointer<vtkPointPicker> picker = vtkSmartPointer<vtkPointPicker>::New();
    picker->SetTolerance(0.005);

    vtkRenderer* renderer = interactor->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
    if (picker->Pick(clickPos[0], clickPos[1], 0, renderer)) {
        double* pos = picker->GetPickPosition();
        qDebug() << "[PointRegistrationWidget] 点击位置:" << pos[0] << pos[1] << pos[2];
        widget->onPointPicked(pos[0], pos[1], pos[2]);
    }
}
#endif

PointRegistrationWidget::PointRegistrationWidget(PointRegistrationService* service, QWidget* parent)
    : QWidget(parent)
    , m_service(service)
    , m_pointTable(nullptr)
    , m_addPointBtn(nullptr)
    , m_deletePointBtn(nullptr)
    , m_clearPointsBtn(nullptr)
    , m_sourceXSpin(nullptr)
    , m_sourceYSpin(nullptr)
    , m_sourceZSpin(nullptr)
    , m_targetXSpin(nullptr)
    , m_targetYSpin(nullptr)
    , m_targetZSpin(nullptr)
    , m_setSourceBtn(nullptr)
    , m_setTargetBtn(nullptr)
    , m_transformModeCombo(nullptr)
    , m_pickSourceRadio(nullptr)
    , m_pickTargetRadio(nullptr)
    , m_pickingSource(true)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_logText(nullptr)
    , m_startBtn(nullptr)
    , m_3dViewContainer(nullptr)
    , m_3dViewPlaceholder(nullptr)
    , m_loadModelBtn(nullptr)
    , m_modelInfoLabel(nullptr)
#ifdef VTK_FOUND
    , m_vtkWidget(nullptr)
    , m_vtkInitialized(false)
#endif
    , m_resultStatusLabel(nullptr)
    , m_resultTable(nullptr)
    , m_rmsErrorLabel(nullptr)
    , m_maxErrorLabel(nullptr)
    , m_accuracyFrame(nullptr)
{
    setupUI();
    connectSignals();
    refresh();

    qDebug() << "[PointRegistrationWidget] Widget创建完成";
}

PointRegistrationWidget::~PointRegistrationWidget()
{
#ifdef VTK_FOUND
    clearPointMarkers();
#endif
    qDebug() << "[PointRegistrationWidget] Widget销毁";
}

void PointRegistrationWidget::refresh()
{
    updatePointTable();
}

// ========== 样式定义 ==========

QString PointRegistrationWidget::getGroupStyle() const
{
    return "QGroupBox { "
           "  background: rgba(30,41,59,0.85); "
           "  border: 1px solid rgba(96,165,250,0.4); "
           "  border-radius: 8px; "
           "  margin-top: 14px; "
           "  padding: 12px 10px 10px 10px; "
           "  font-weight: bold; "
           "  font-size: 12px; "
           "  color: #60a5fa; "
           "}"
           "QGroupBox::title { "
           "  subcontrol-origin: margin; "
           "  subcontrol-position: top left; "
           "  left: 12px; "
           "  top: 2px; "
           "  padding: 0 6px; "
           "  background: rgba(30,41,59,0.95); "
           "  color: #93c5fd; "
           "}";
}

QString PointRegistrationWidget::getButtonStyle() const
{
    return "QPushButton { "
           "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(59,130,246,0.8), stop:1 rgba(37,99,235,0.8)); "
           "  color: white; "
           "  border: 1px solid #60a5fa; "
           "  border-radius: 6px; "
           "  padding: 8px 16px; "
           "  font-weight: bold; "
           "}"
           "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #60a5fa, stop:1 #3b82f6); }"
           "QPushButton:pressed { background: #2563eb; }"
           "QPushButton:disabled { background: rgba(107,114,128,0.5); border-color: #6b7280; color: #9ca3af; }";
}

QString PointRegistrationWidget::getTableStyle() const
{
    return "QTableWidget { "
           "  background: rgba(15,23,42,0.8); "
           "  border: 1px solid rgba(96,165,250,0.3); "
           "  border-radius: 6px; "
           "  gridline-color: rgba(96,165,250,0.2); "
           "  color: #e2e8f0; "
           "}"
           "QTableWidget::item { padding: 5px; }"
           "QTableWidget::item:selected { background: rgba(59,130,246,0.4); }"
           "QHeaderView::section { "
           "  background: rgba(51,65,85,0.8); "
           "  color: #93c5fd; "
           "  border: none; "
           "  padding: 6px; "
           "  font-weight: bold; "
           "}";
}

// ========== UI布局 ==========

void PointRegistrationWidget::setupUI()
{
    setObjectName("pointRegistrationWidget");
    setStyleSheet("background: transparent;");

    // 主布局：水平三列（左侧点管理 | 中间3D视图 | 右侧结果）
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

    // 列宽比例：左 2 / 中 4 / 右 2，根据窗口宽度自适应
    mainLayout->setStretch(0, 2);
    mainLayout->setStretch(1, 4);
    mainLayout->setStretch(2, 2);
}

QWidget* PointRegistrationWidget::createLeftPanel()
{
    QWidget* leftPanel = new QWidget();
    // 左侧给一个合适的最小宽度，允许在高分辨率下进一步拉宽
    leftPanel->setMinimumWidth(360);
    leftPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    // ========== 标记点列表组 ==========
    QGroupBox* pointGroup = new QGroupBox("📍 配准点对");
    pointGroup->setStyleSheet(getGroupStyle());
    QVBoxLayout* pointLayout = new QVBoxLayout(pointGroup);
    pointLayout->setContentsMargins(8, 12, 8, 8);
    pointLayout->setSpacing(6);

    // 按钮行
    QHBoxLayout* pointBtnRow = new QHBoxLayout();
    pointBtnRow->setSpacing(6);
    m_addPointBtn = new QPushButton("添加");
    m_deletePointBtn = new QPushButton("删除");
    m_clearPointsBtn = new QPushButton("清空");
    m_addPointBtn->setStyleSheet(getButtonStyle());
    m_deletePointBtn->setStyleSheet(getButtonStyle());
    m_clearPointsBtn->setStyleSheet(getButtonStyle());
    // 使用最小高度，避免在高 DPI 下文本被裁剪
    m_addPointBtn->setMinimumHeight(30);
    m_deletePointBtn->setMinimumHeight(30);
    m_clearPointsBtn->setMinimumHeight(30);
    m_addPointBtn->setMinimumWidth(70);
    m_deletePointBtn->setMinimumWidth(70);
    m_clearPointsBtn->setMinimumWidth(70);
    pointBtnRow->addWidget(m_addPointBtn);
    pointBtnRow->addWidget(m_deletePointBtn);
    pointBtnRow->addWidget(m_clearPointsBtn);
    pointBtnRow->addStretch();
    pointLayout->addLayout(pointBtnRow);

    // 标记点表格（7列：名称、源X、源Y、源Z、目标X、目标Y、目标Z）
    m_pointTable = new QTableWidget(0, 7);
    m_pointTable->setHorizontalHeaderLabels({"名称", "源X", "源Y", "源Z", "目标X", "目标Y", "目标Z"});
    m_pointTable->setStyleSheet(getTableStyle());
    m_pointTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    for (int i = 1; i < 7; ++i) {
        m_pointTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    }
    m_pointTable->horizontalHeader()->setMinimumSectionSize(40);
    m_pointTable->setMinimumHeight(120);
    m_pointTable->verticalHeader()->setVisible(false);
    m_pointTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pointTable->setSelectionMode(QAbstractItemView::SingleSelection);
    pointLayout->addWidget(m_pointTable);

    leftLayout->addWidget(pointGroup);

    // ========== 坐标输入组 ==========
    QGroupBox* coordGroup = new QGroupBox("📐 坐标输入");
    coordGroup->setStyleSheet(getGroupStyle());
    QVBoxLayout* coordLayout = new QVBoxLayout(coordGroup);
    coordLayout->setContentsMargins(8, 12, 8, 8);
    coordLayout->setSpacing(6);

    QString spinStyle =
        "QDoubleSpinBox { background: rgba(15,23,42,0.8); border: 1px solid rgba(96,165,250,0.3); "
        "border-radius: 4px; color: #e2e8f0; padding: 3px; min-width: 60px; }"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 16px; }";

    // 源点坐标
    QHBoxLayout* sourceRow = new QHBoxLayout();
    QLabel* sourceLabel = new QLabel("源点:");
    sourceLabel->setStyleSheet("color: #60a5fa; font-weight: bold; background: transparent; min-width: 40px;");
    m_sourceXSpin = new QDoubleSpinBox();
    m_sourceYSpin = new QDoubleSpinBox();
    m_sourceZSpin = new QDoubleSpinBox();
    for (auto* spin : {m_sourceXSpin, m_sourceYSpin, m_sourceZSpin}) {
        spin->setRange(-9999.0, 9999.0);
        spin->setDecimals(2);
        spin->setStyleSheet(spinStyle);
    }
    m_setSourceBtn = new QPushButton("设置");
    m_setSourceBtn->setStyleSheet(getButtonStyle());
    m_setSourceBtn->setMinimumSize(50, 26);
    sourceRow->addWidget(sourceLabel);
    sourceRow->addWidget(m_sourceXSpin);
    sourceRow->addWidget(m_sourceYSpin);
    sourceRow->addWidget(m_sourceZSpin);
    sourceRow->addWidget(m_setSourceBtn);
    coordLayout->addLayout(sourceRow);

    // 目标点坐标
    QHBoxLayout* targetRow = new QHBoxLayout();
    QLabel* targetLabel = new QLabel("目标:");
    targetLabel->setStyleSheet("color: #10b981; font-weight: bold; background: transparent; min-width: 40px;");
    m_targetXSpin = new QDoubleSpinBox();
    m_targetYSpin = new QDoubleSpinBox();
    m_targetZSpin = new QDoubleSpinBox();
    for (auto* spin : {m_targetXSpin, m_targetYSpin, m_targetZSpin}) {
        spin->setRange(-9999.0, 9999.0);
        spin->setDecimals(2);
        spin->setStyleSheet(spinStyle);
    }
    m_setTargetBtn = new QPushButton("设置");
    m_setTargetBtn->setStyleSheet(getButtonStyle());
    m_setTargetBtn->setMinimumSize(50, 26);
    targetRow->addWidget(targetLabel);
    targetRow->addWidget(m_targetXSpin);
    targetRow->addWidget(m_targetYSpin);
    targetRow->addWidget(m_targetZSpin);
    targetRow->addWidget(m_setTargetBtn);
    coordLayout->addLayout(targetRow);

    // 提示
    QLabel* tipLabel = new QLabel("💡 选择表格中的点，输入坐标后点击设置");
    tipLabel->setStyleSheet("color: #94a3b8; font-size: 10px; background: transparent;");
    tipLabel->setWordWrap(true);
    coordLayout->addWidget(tipLabel);

    leftLayout->addWidget(coordGroup);

    // ========== 变换模式组 ==========
    QGroupBox* modeGroup = new QGroupBox("⚙️ 变换模式");
    modeGroup->setStyleSheet(getGroupStyle());
    QVBoxLayout* modeLayout = new QVBoxLayout(modeGroup);
    modeLayout->setContentsMargins(8, 12, 8, 8);

    m_transformModeCombo = new QComboBox();
    m_transformModeCombo->addItem("刚体变换 (6DOF)", static_cast<int>(TransformMode::RigidBody));
    m_transformModeCombo->addItem("相似性变换 (7DOF)", static_cast<int>(TransformMode::Similarity));
    m_transformModeCombo->addItem("仿射变换 (12DOF)", static_cast<int>(TransformMode::Affine));
    m_transformModeCombo->setStyleSheet(
        "QComboBox { background: rgba(15,23,42,0.8); border: 1px solid rgba(96,165,250,0.3); "
        "border-radius: 4px; color: #e2e8f0; padding: 6px; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: rgba(30,41,59,0.95); color: #e2e8f0; "
        "selection-background-color: rgba(59,130,246,0.5); }"
    );
    modeLayout->addWidget(m_transformModeCombo);

    leftLayout->addWidget(modeGroup);

    // ========== 3D选点模式组 ==========
    QGroupBox* pickGroup = new QGroupBox("🎯 3D选点模式");
    pickGroup->setStyleSheet(getGroupStyle());
    QHBoxLayout* pickLayout = new QHBoxLayout(pickGroup);
    pickLayout->setContentsMargins(8, 12, 8, 8);

    QString radioStyle =
        "QRadioButton { color: #e2e8f0; background: transparent; }"
        "QRadioButton::indicator { width: 14px; height: 14px; }"
        "QRadioButton::indicator:checked { background: #3b82f6; border: 2px solid #60a5fa; border-radius: 7px; }"
        "QRadioButton::indicator:unchecked { background: rgba(30,41,59,0.8); border: 2px solid #475569; border-radius: 7px; }";

    m_pickSourceRadio = new QRadioButton("选取源点");
    m_pickSourceRadio->setStyleSheet(radioStyle + " QRadioButton { color: #60a5fa; }");
    m_pickSourceRadio->setChecked(true);
    m_pickTargetRadio = new QRadioButton("选取目标点");
    m_pickTargetRadio->setStyleSheet(radioStyle + " QRadioButton { color: #10b981; }");

    QButtonGroup* pickBtnGroup = new QButtonGroup(this);
    pickBtnGroup->addButton(m_pickSourceRadio, 0);
    pickBtnGroup->addButton(m_pickTargetRadio, 1);

    pickLayout->addWidget(m_pickSourceRadio);
    pickLayout->addWidget(m_pickTargetRadio);
    pickLayout->addStretch();

    leftLayout->addWidget(pickGroup);

    // ========== 配准进度组 ==========
    QGroupBox* progressGroup = new QGroupBox("⏳ 配准进度");
    progressGroup->setStyleSheet(getGroupStyle());
    QVBoxLayout* progressLayout = new QVBoxLayout(progressGroup);
    progressLayout->setContentsMargins(8, 12, 8, 8);

    m_statusLabel = new QLabel("● 就绪");
    m_statusLabel->setStyleSheet("color: #10b981; font-size: 12px; font-weight: bold; background: transparent;");
    progressLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet(
        "QProgressBar { background: rgba(30,41,59,0.8); border: 1px solid #475569; border-radius: 5px; height: 18px; text-align: center; color: white; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #10b981); border-radius: 4px; }"
    );
    progressLayout->addWidget(m_progressBar);

    m_logText = new QTextEdit();
    m_logText->setReadOnly(true);
    // 至少给一点高度，允许在竖向空间充足时自动增高
    m_logText->setMinimumHeight(80);
    m_logText->setStyleSheet(
        "QTextEdit { background: rgba(15,23,42,0.8); border: 1px solid rgba(96,165,250,0.3); "
        "border-radius: 6px; color: #e2e8f0; font-family: monospace; font-size: 10px; }"
    );
    m_logText->setPlaceholderText("配准日志...");
    progressLayout->addWidget(m_logText);

    leftLayout->addWidget(progressGroup);

    // ========== 开始配准按钮 ==========
    m_startBtn = new QPushButton("▶ 开始配准");
    m_startBtn->setMinimumHeight(40);
    m_startBtn->setStyleSheet(
        "QPushButton { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10b981, stop:1 #059669); "
        "  color: white; font-size: 13px; font-weight: bold; "
        "  border: 2px solid #10b981; border-radius: 8px; "
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #059669, stop:1 #047857); }"
        "QPushButton:disabled { background: rgba(107,114,128,0.5); border-color: #6b7280; }"
    );
    leftLayout->addWidget(m_startBtn);

    leftLayout->addStretch();
    return leftPanel;
}

QWidget* PointRegistrationWidget::createCenterPanel()
{
    QWidget* centerPanel = new QWidget();
    // 中间 3D 视图作为主视觉区域，允许在水平方向尽可能扩展
    centerPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);

    // ========== 加载3D模型按钮组 ==========
    QGroupBox* loadGroup = new QGroupBox("📂 3D模型");
    loadGroup->setStyleSheet(getGroupStyle());
    QHBoxLayout* loadLayout = new QHBoxLayout(loadGroup);
    loadLayout->setContentsMargins(8, 12, 8, 8);
    loadLayout->setSpacing(8);

    m_loadModelBtn = new QPushButton("加载骨骼模型");
    m_loadModelBtn->setStyleSheet(getButtonStyle());
    m_loadModelBtn->setMinimumHeight(32);
    m_loadModelBtn->setMinimumWidth(120);
    loadLayout->addWidget(m_loadModelBtn);

    m_modelInfoLabel = new QLabel("未加载模型");
    m_modelInfoLabel->setStyleSheet("color: #94a3b8; font-size: 11px; background: transparent;");
    loadLayout->addWidget(m_modelInfoLabel, 1);

    centerLayout->addWidget(loadGroup);

    // 3D视图容器
    m_3dViewContainer = new QWidget();
    QVBoxLayout* containerLayout = new QVBoxLayout(m_3dViewContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);

#ifdef VTK_FOUND
    // 使用VTK Widget（带异常保护）
    try {
        m_vtkWidget = VTKWidgetFactory::createVTKWidget(m_3dViewContainer);
        if (m_vtkWidget) {
            m_vtkWidget->setMinimumSize(300, 300);
            containerLayout->addWidget(m_vtkWidget);
            m_3dViewPlaceholder = nullptr;
            qDebug() << "[PointRegistrationWidget] VTK Widget已创建";
        } else {
            qWarning() << "[PointRegistrationWidget] VTK Widget创建失败，使用占位符";
        }
    } catch (const std::exception& e) {
        qCritical() << "[PointRegistrationWidget] VTK Widget创建异常:" << e.what();
        m_vtkWidget = nullptr;
    } catch (...) {
        qCritical() << "[PointRegistrationWidget] VTK Widget创建未知异常";
        m_vtkWidget = nullptr;
    }

    if (!m_vtkWidget) {
#endif
        // 占位符标签（无VTK或创建失败时使用）
        m_3dViewPlaceholder = new QLabel("🎲 3D视图 - 点击添加标记点\n(VTK未启用)");
        m_3dViewPlaceholder->setAlignment(Qt::AlignCenter);
        m_3dViewPlaceholder->setStyleSheet(
            "background: rgba(30,41,59,0.6); "
            "border: 2px dashed rgba(96,165,250,0.5); "
            "border-radius: 10px; "
            "color: #94a3b8; "
            "font-size: 16px; "
            "min-height: 300px;"
        );
        containerLayout->addWidget(m_3dViewPlaceholder);
#ifdef VTK_FOUND
    }
#endif

    centerLayout->addWidget(m_3dViewContainer, 1);
    return centerPanel;
}

QWidget* PointRegistrationWidget::createRightPanel()
{
    QWidget* rightPanel = new QWidget();
    rightPanel->setMinimumWidth(260);
    rightPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    // ========== 配准结果组 ==========
    QGroupBox* resultGroup = new QGroupBox("📊 配准结果");
    resultGroup->setStyleSheet(getGroupStyle());
    QVBoxLayout* resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setContentsMargins(8, 14, 8, 8);
    resultLayout->setSpacing(6);

    m_resultStatusLabel = new QLabel("等待配准...");
    m_resultStatusLabel->setStyleSheet("color: #94a3b8; font-size: 12px; background: transparent;");
    resultLayout->addWidget(m_resultStatusLabel);

    // 结果表格 - 自适应高度
    m_resultTable = new QTableWidget(6, 2);
    m_resultTable->setHorizontalHeaderLabels({"参数", "值"});
    m_resultTable->setStyleSheet(getTableStyle());
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setMinimumSectionSize(50);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QStringList resultParams = {"平移X(mm)", "平移Y(mm)", "平移Z(mm)",
                                 "旋转X(°)", "旋转Y(°)", "旋转Z(°)"};
    for (int i = 0; i < resultParams.size(); ++i) {
        m_resultTable->setItem(i, 0, new QTableWidgetItem(resultParams[i]));
        m_resultTable->setItem(i, 1, new QTableWidgetItem("-"));
        m_resultTable->item(i, 0)->setFlags(Qt::ItemIsEnabled);
        m_resultTable->item(i, 1)->setFlags(Qt::ItemIsEnabled);
        m_resultTable->setRowHeight(i, 26);
    }
    // 计算表格推荐高度：表头高度 + 6行 * 26px，作为最小高度，允许在高 DPI 下自动增高
    int tableHeight = m_resultTable->horizontalHeader()->height() + 6 * 26 + 4;
    m_resultTable->setMinimumHeight(tableHeight);
    resultLayout->addWidget(m_resultTable);

    // 精度指标
    m_accuracyFrame = new QFrame();
    m_accuracyFrame->setStyleSheet("background: rgba(16,185,129,0.1); border: 1px solid rgba(16,185,129,0.3); border-radius: 8px; padding: 8px;");
    QVBoxLayout* accLayout = new QVBoxLayout(m_accuracyFrame);
    accLayout->setContentsMargins(8, 6, 8, 6);
    accLayout->setSpacing(4);

    QLabel* accTitle = new QLabel("🎯 配准精度");
    accTitle->setStyleSheet("color: #10b981; font-weight: bold; font-size: 11px; background: transparent;");
    accLayout->addWidget(accTitle);

    m_rmsErrorLabel = new QLabel("RMS误差: - mm");
    m_rmsErrorLabel->setStyleSheet("color: #e2e8f0; font-size: 11px; background: transparent;");
    accLayout->addWidget(m_rmsErrorLabel);

    m_maxErrorLabel = new QLabel("最大误差: - mm");
    m_maxErrorLabel->setStyleSheet("color: #e2e8f0; font-size: 11px; background: transparent;");
    accLayout->addWidget(m_maxErrorLabel);

    resultLayout->addWidget(m_accuracyFrame);
    rightLayout->addWidget(resultGroup);
    rightLayout->addStretch();

    return rightPanel;
}

void PointRegistrationWidget::connectSignals()
{
    // 点管理
    connect(m_addPointBtn, &QPushButton::clicked, this, &PointRegistrationWidget::onAddPoint);
    connect(m_deletePointBtn, &QPushButton::clicked, this, &PointRegistrationWidget::onDeletePoint);
    connect(m_clearPointsBtn, &QPushButton::clicked, this, &PointRegistrationWidget::onClearPoints);

    // 坐标设置
    connect(m_setSourceBtn, &QPushButton::clicked, this, &PointRegistrationWidget::onSetSourcePoint);
    connect(m_setTargetBtn, &QPushButton::clicked, this, &PointRegistrationWidget::onSetTargetPoint);

    // 表格选择变化
    connect(m_pointTable, &QTableWidget::itemSelectionChanged,
            this, &PointRegistrationWidget::onPointTableSelectionChanged);

    // 变换模式
    connect(m_transformModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PointRegistrationWidget::onTransformModeChanged);

    // 选点模式
    connect(m_pickSourceRadio, &QRadioButton::toggled, this, &PointRegistrationWidget::onPickModeChanged);
    connect(m_pickTargetRadio, &QRadioButton::toggled, this, &PointRegistrationWidget::onPickModeChanged);

    // 配准控制
    connect(m_startBtn, &QPushButton::clicked, this, &PointRegistrationWidget::onStartRegistration);

    // 加载模型
    connect(m_loadModelBtn, &QPushButton::clicked, this, &PointRegistrationWidget::onLoadModel);

    // 服务信号
    if (m_service) {
        connect(m_service, &PointRegistrationService::pointAdded,
                this, &PointRegistrationWidget::onPointAdded);
        connect(m_service, &PointRegistrationService::pointRemoved,
                this, &PointRegistrationWidget::onPointRemoved);
        connect(m_service, &PointRegistrationService::pointsCleared,
                this, &PointRegistrationWidget::onPointsCleared);
        connect(m_service, &PointRegistrationService::pointUpdated,
                this, &PointRegistrationWidget::onPointUpdated);
        connect(m_service, &PointRegistrationService::registrationCompleted,
                this, &PointRegistrationWidget::onRegistrationCompleted);
        connect(m_service, &PointRegistrationService::registrationFailed,
                this, &PointRegistrationWidget::onRegistrationFailed);
        connect(m_service, &PointRegistrationService::progressUpdated,
                this, &PointRegistrationWidget::onProgressUpdated);
    }
}

// ========== 槽函数实现 ==========

void PointRegistrationWidget::onAddPoint()
{
    if (m_service) {
        m_service->addPoint();
        appendLog("添加新配准点");
    }
}

void PointRegistrationWidget::onDeletePoint()
{
    int row = m_pointTable->currentRow();
    if (row >= 0 && m_service) {
        m_service->removePoint(row);
        appendLog(QString("删除配准点 %1").arg(row + 1));
    }
}

void PointRegistrationWidget::onClearPoints()
{
    if (m_service) {
        m_service->clearPoints();
        appendLog("清空所有配准点");
    }
}

void PointRegistrationWidget::onLoadModel()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择3D骨骼模型",
        QString(),
        "3D模型文件 (*.stl *.obj *.ply *.vtk *.vtp);;STL文件 (*.stl);;OBJ文件 (*.obj);;所有文件 (*.*)"
    );

    if (filePath.isEmpty()) {
        return;
    }

    appendLog(QString("正在加载模型: %1").arg(QFileInfo(filePath).fileName()));

#ifdef VTK_FOUND
    try {
        if (m_vtkWidget && m_renderer) {
            // 加载3D模型
            vtkSmartPointer<vtkPolyData> polyData;

            QString ext = QFileInfo(filePath).suffix().toLower();
            if (ext == "stl") {
                vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
                reader->SetFileName(filePath.toStdString().c_str());
                reader->Update();
                polyData = reader->GetOutput();
            } else if (ext == "obj") {
                vtkSmartPointer<vtkOBJReader> reader = vtkSmartPointer<vtkOBJReader>::New();
                reader->SetFileName(filePath.toStdString().c_str());
                reader->Update();
                polyData = reader->GetOutput();
            } else if (ext == "ply") {
                vtkSmartPointer<vtkPLYReader> reader = vtkSmartPointer<vtkPLYReader>::New();
                reader->SetFileName(filePath.toStdString().c_str());
                reader->Update();
                polyData = reader->GetOutput();
            } else {
                appendLog("⚠️ 不支持的文件格式");
                return;
            }

            if (polyData && polyData->GetNumberOfPoints() > 0) {
                // 移除旧模型
                if (m_modelActor) {
                    m_renderer->RemoveActor(m_modelActor);
                }

                // 创建新模型Actor
                vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
                mapper->SetInputData(polyData);

                m_modelActor = vtkSmartPointer<vtkActor>::New();
                m_modelActor->SetMapper(mapper);
                m_modelActor->GetProperty()->SetColor(0.9, 0.9, 0.85);  // 骨骼颜色
                m_modelActor->GetProperty()->SetOpacity(0.8);

                m_renderer->AddActor(m_modelActor);
                m_renderer->ResetCamera();
                m_renderWindow->Render();

                // 更新信息标签
                m_modelInfoLabel->setText(QString("%1 (%2 点)")
                    .arg(QFileInfo(filePath).fileName())
                    .arg(polyData->GetNumberOfPoints()));
                m_modelInfoLabel->setStyleSheet("color: #10b981; font-size: 11px; background: transparent;");

                appendLog(QString("✓ 模型加载成功，共 %1 个点").arg(polyData->GetNumberOfPoints()));
            } else {
                appendLog("⚠️ 模型加载失败或模型为空");
            }
        }
    } catch (const std::exception& e) {
        qCritical() << "[PointRegistrationWidget] 模型加载异常:" << e.what();
        appendLog(QString("⚠️ 加载异常: %1").arg(e.what()));
    } catch (...) {
        qCritical() << "[PointRegistrationWidget] 模型加载未知异常";
        appendLog("⚠️ 加载时发生未知异常");
    }
#else
    appendLog("⚠️ VTK未启用，无法加载3D模型");
#endif
}

void PointRegistrationWidget::onSetSourcePoint()
{
    int row = m_pointTable->currentRow();
    if (row < 0) {
        appendLog("⚠️ 请先选择一个配准点");
        return;
    }
    if (m_service) {
        QVector3D pos(m_sourceXSpin->value(), m_sourceYSpin->value(), m_sourceZSpin->value());
        m_service->setSourcePosition(row, pos);
        appendLog(QString("设置点 %1 源坐标: (%.2f, %.2f, %.2f)")
                  .arg(row + 1).arg(pos.x()).arg(pos.y()).arg(pos.z()));
    }
}

void PointRegistrationWidget::onSetTargetPoint()
{
    int row = m_pointTable->currentRow();
    if (row < 0) {
        appendLog("⚠️ 请先选择一个配准点");
        return;
    }
    if (m_service) {
        QVector3D pos(m_targetXSpin->value(), m_targetYSpin->value(), m_targetZSpin->value());
        m_service->setTargetPosition(row, pos);
        appendLog(QString("设置点 %1 目标坐标: (%.2f, %.2f, %.2f)")
                  .arg(row + 1).arg(pos.x()).arg(pos.y()).arg(pos.z()));
    }
}

void PointRegistrationWidget::onPointTableSelectionChanged()
{
    int row = m_pointTable->currentRow();
    if (row >= 0 && m_service) {
        RegistrationPoint pt = m_service->getPoint(row);
        if (pt.hasSource) {
            m_sourceXSpin->setValue(pt.sourcePosition.x());
            m_sourceYSpin->setValue(pt.sourcePosition.y());
            m_sourceZSpin->setValue(pt.sourcePosition.z());
        }
        if (pt.hasTarget) {
            m_targetXSpin->setValue(pt.targetPosition.x());
            m_targetYSpin->setValue(pt.targetPosition.y());
            m_targetZSpin->setValue(pt.targetPosition.z());
        }
    }
}

void PointRegistrationWidget::onTransformModeChanged(int index)
{
    if (m_service && index >= 0) {
        TransformMode mode = static_cast<TransformMode>(m_transformModeCombo->itemData(index).toInt());
        m_service->setTransformMode(mode);
        appendLog(QString("变换模式: %1").arg(transformModeToString(mode)));
    }
}

void PointRegistrationWidget::onStartRegistration()
{
    if (!m_service) return;

    if (!m_service->canExecuteRegistration()) {
        appendLog("⚠️ 至少需要3个完整的点对才能执行配准");
        return;
    }

    m_startBtn->setEnabled(false);
    m_statusLabel->setText("● 配准中...");
    m_statusLabel->setStyleSheet("color: #f59e0b; font-size: 13px; font-weight: bold; background: transparent;");

    appendLog("开始执行配准...");
    m_service->executeRegistration();
}

void PointRegistrationWidget::onPointAdded(int index, const QString& name)
{
    Q_UNUSED(index); Q_UNUSED(name);
    updatePointTable();
}

void PointRegistrationWidget::onPointRemoved(int index)
{
    Q_UNUSED(index);
    updatePointTable();
    updatePointMarkers();
}

void PointRegistrationWidget::onPointsCleared()
{
    updatePointTable();
    updatePointMarkers();
}

void PointRegistrationWidget::onPointUpdated(int index)
{
    Q_UNUSED(index);
    updatePointTable();
    updatePointMarkers();
}

void PointRegistrationWidget::onRegistrationCompleted(const PointRegistrationResult& result)
{
    m_startBtn->setEnabled(true);
    m_statusLabel->setText("● 配准完成");
    m_statusLabel->setStyleSheet("color: #10b981; font-weight: bold;");

    updateResultDisplay(result);
    appendLog(QString("✅ 配准完成: RMS=%.3f mm").arg(result.rmsError));
}

void PointRegistrationWidget::onRegistrationFailed(const QString& error)
{
    m_startBtn->setEnabled(true);
    m_statusLabel->setText("● 配准失败");
    m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");

    appendLog(QString("❌ 配准失败: %1").arg(error));
}

void PointRegistrationWidget::onProgressUpdated(int progress, const QString& message)
{
    m_progressBar->setValue(progress);
    appendLog(message);
}

// ========== 辅助函数 ==========

void PointRegistrationWidget::updatePointTable()
{
    if (!m_service) return;

    auto points = m_service->getAllPoints();
    m_pointTable->setRowCount(points.size());

    // 7列：名称、源X、源Y、源Z、目标X、目标Y、目标Z
    for (int i = 0; i < points.size(); ++i) {
        const auto& pt = points[i];

        m_pointTable->setItem(i, 0, new QTableWidgetItem(pt.name));

        // 源点坐标
        if (pt.hasSource) {
            m_pointTable->setItem(i, 1, new QTableWidgetItem(QString::number(pt.sourcePosition.x(), 'f', 1)));
            m_pointTable->setItem(i, 2, new QTableWidgetItem(QString::number(pt.sourcePosition.y(), 'f', 1)));
            m_pointTable->setItem(i, 3, new QTableWidgetItem(QString::number(pt.sourcePosition.z(), 'f', 1)));
        } else {
            m_pointTable->setItem(i, 1, new QTableWidgetItem("-"));
            m_pointTable->setItem(i, 2, new QTableWidgetItem("-"));
            m_pointTable->setItem(i, 3, new QTableWidgetItem("-"));
        }

        // 目标点坐标
        if (pt.hasTarget) {
            m_pointTable->setItem(i, 4, new QTableWidgetItem(QString::number(pt.targetPosition.x(), 'f', 1)));
            m_pointTable->setItem(i, 5, new QTableWidgetItem(QString::number(pt.targetPosition.y(), 'f', 1)));
            m_pointTable->setItem(i, 6, new QTableWidgetItem(QString::number(pt.targetPosition.z(), 'f', 1)));
        } else {
            m_pointTable->setItem(i, 4, new QTableWidgetItem("-"));
            m_pointTable->setItem(i, 5, new QTableWidgetItem("-"));
            m_pointTable->setItem(i, 6, new QTableWidgetItem("-"));
        }

        // 设置颜色区分完整和不完整的点对
        for (int col = 0; col < 7; ++col) {
            QTableWidgetItem* item = m_pointTable->item(i, col);
            if (item) {
                if (pt.isComplete()) {
                    item->setForeground(QColor("#10b981")); // 绿色表示完整
                } else {
                    item->setForeground(QColor("#f59e0b")); // 橙色表示不完整
                }
            }
        }
    }
}

void PointRegistrationWidget::updateResultDisplay(const PointRegistrationResult& result)
{
    // 更新状态
    m_resultStatusLabel->setText("配准成功");
    m_resultStatusLabel->setStyleSheet("color: #10b981; font-size: 13px; background: transparent;");

    // 更新精度指标
    m_rmsErrorLabel->setText(QString("RMS误差: %1 mm").arg(result.rmsError, 0, 'f', 3));
    m_maxErrorLabel->setText(QString("最大误差: %1 mm").arg(result.maxError, 0, 'f', 3));

    // 更新变换参数表格
    m_resultTable->item(0, 1)->setText(QString::number(result.translationX, 'f', 3));
    m_resultTable->item(1, 1)->setText(QString::number(result.translationY, 'f', 3));
    m_resultTable->item(2, 1)->setText(QString::number(result.translationZ, 'f', 3));
    m_resultTable->item(3, 1)->setText(QString::number(result.rotationX, 'f', 3));
    m_resultTable->item(4, 1)->setText(QString::number(result.rotationY, 'f', 3));
    m_resultTable->item(5, 1)->setText(QString::number(result.rotationZ, 'f', 3));
}

void PointRegistrationWidget::appendLog(const QString& message)
{
    m_logText->append(QString("[%1] %2").arg(formatTime(), message));
}

QString PointRegistrationWidget::formatTime() const
{
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void PointRegistrationWidget::onPickModeChanged()
{
    m_pickingSource = m_pickSourceRadio->isChecked();
    QString mode = m_pickingSource ? "源点" : "目标点";
    appendLog(QString("切换选点模式: %1").arg(mode));
}

void PointRegistrationWidget::showEvent(QShowEvent* event)
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
void PointRegistrationWidget::initializeVTK()
{
    if (m_vtkInitialized || !m_vtkWidget) return;

    try {
        qDebug() << "[PointRegistrationWidget] 初始化VTK渲染管线...";

        // 创建渲染窗口
        m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();

        // 创建渲染器
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        m_renderer->SetBackground(0.1, 0.15, 0.2);  // 深蓝灰色背景
        m_renderer->SetBackground2(0.05, 0.08, 0.12);
        m_renderer->GradientBackgroundOn();
        m_renderWindow->AddRenderer(m_renderer);

        // 连接到Widget
        m_vtkWidget->setRenderWindow(m_renderWindow);

        // 获取交互器并设置样式
        m_interactor = m_renderWindow->GetInteractor();
        if (m_interactor) {
            vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
                vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
            m_interactor->SetInteractorStyle(style);

            // 添加点击回调
            vtkSmartPointer<vtkCallbackCommand> clickCallback =
                vtkSmartPointer<vtkCallbackCommand>::New();
            clickCallback->SetCallback(OnLeftButtonDown);
            clickCallback->SetClientData(this);
            m_interactor->AddObserver(vtkCommand::LeftButtonPressEvent, clickCallback);
        }

        // 设置相机
        vtkCamera* camera = m_renderer->GetActiveCamera();
        camera->SetPosition(0, 0, 500);
        camera->SetFocalPoint(0, 0, 0);
        camera->SetViewUp(0, 1, 0);
        m_renderer->ResetCamera();

        m_renderWindow->Render();
        m_vtkInitialized = true;

        qDebug() << "[PointRegistrationWidget] VTK初始化完成";
        appendLog("3D视图已初始化 - 点击选取标记点");
    } catch (const std::exception& e) {
        qCritical() << "[PointRegistrationWidget] VTK初始化异常:" << e.what();
        m_vtkInitialized = false;
    } catch (...) {
        qCritical() << "[PointRegistrationWidget] VTK初始化未知异常";
        m_vtkInitialized = false;
    }
}

void PointRegistrationWidget::updatePointMarkers()
{
    if (!m_vtkInitialized || !m_renderer) return;

    clearPointMarkers();

    if (!m_service) return;

    const auto points = m_service->getAllPoints();
    for (const auto& pt : points) {
        if (pt.hasSource) {
            addPointMarker(pt.sourcePosition, QColor(96, 165, 250));  // 蓝色源点
        }
        if (pt.hasTarget) {
            addPointMarker(pt.targetPosition, QColor(16, 185, 129));  // 绿色目标点
        }
    }

    m_renderWindow->Render();
}

void PointRegistrationWidget::addPointMarker(const QVector3D& pos, const QColor& color)
{
    if (!m_renderer) return;

    // 创建球体
    vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetCenter(pos.x(), pos.y(), pos.z());
    sphere->SetRadius(3.0);
    sphere->SetPhiResolution(16);
    sphere->SetThetaResolution(16);

    // 创建映射器
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(sphere->GetOutputPort());

    // 创建Actor
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
    actor->GetProperty()->SetAmbient(0.3);
    actor->GetProperty()->SetDiffuse(0.7);
    actor->GetProperty()->SetSpecular(0.3);

    m_renderer->AddActor(actor);

    // 根据颜色判断是源点还是目标点
    if (color.blue() > color.green()) {
        m_sourceMarkers.append(actor);
    } else {
        m_targetMarkers.append(actor);
    }
}

void PointRegistrationWidget::clearPointMarkers()
{
    if (!m_renderer) return;

    for (auto& actor : m_sourceMarkers) {
        m_renderer->RemoveActor(actor);
    }
    m_sourceMarkers.clear();

    for (auto& actor : m_targetMarkers) {
        m_renderer->RemoveActor(actor);
    }
    m_targetMarkers.clear();
}

void PointRegistrationWidget::onPointPicked(double x, double y, double z)
{
    int row = m_pointTable->currentRow();

    // 如果没有选中行，自动添加新点
    if (row < 0) {
        if (m_service) {
            m_service->addPoint();
            row = m_service->pointCount() - 1;
            m_pointTable->selectRow(row);
        }
    }

    if (row < 0 || !m_service) return;

    QVector3D pos(x, y, z);

    if (m_pickingSource) {
        // 设置源点
        m_service->setSourcePosition(row, pos);
        m_sourceXSpin->setValue(x);
        m_sourceYSpin->setValue(y);
        m_sourceZSpin->setValue(z);
        appendLog(QString("选取源点 %1: (%.2f, %.2f, %.2f)").arg(row + 1).arg(x).arg(y).arg(z));
    } else {
        // 设置目标点
        m_service->setTargetPosition(row, pos);
        m_targetXSpin->setValue(x);
        m_targetYSpin->setValue(y);
        m_targetZSpin->setValue(z);
        appendLog(QString("选取目标点 %1: (%.2f, %.2f, %.2f)").arg(row + 1).arg(x).arg(y).arg(z));
    }

    updatePointMarkers();
}

#else
// 无VTK时的空实现
void PointRegistrationWidget::initializeVTK() {}
void PointRegistrationWidget::updatePointMarkers() {}
void PointRegistrationWidget::addPointMarker(const QVector3D&, const QColor&) {}
void PointRegistrationWidget::clearPointMarkers() {}
void PointRegistrationWidget::onPointPicked(double, double, double) {
    appendLog("⚠️ VTK未启用，无法使用3D选点功能");
}
#endif
