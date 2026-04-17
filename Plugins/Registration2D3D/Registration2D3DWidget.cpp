#include "Registration2D3DWidget.h"
#include "Registration2D3DService.h"
#include <QDebug>
#include <QHeaderView>
#include <QDateTime>
#include <QDir>
#include <QSplitter>
#include <QScrollArea>
#include <QScrollBar>
#include <QFileInfo>

Registration2D3DWidget::Registration2D3DWidget(Registration2D3DService* service, QWidget* parent)
    : QWidget(parent)
    , m_service(service)
    , m_mainLayout(nullptr)
    , m_tabWidget(nullptr)
    , m_loadAPBtn(nullptr)
    , m_loadLATBtn(nullptr)
    , m_loadCTBtn(nullptr)
    , m_loadBoneBtn(nullptr)
    , m_startBtn(nullptr)
    , m_cancelBtn(nullptr)
    , m_clearBtn(nullptr)
    , m_resetParamsBtn(nullptr)
    , m_parameterGroup(nullptr)
    , m_kdTreeNumSpin(nullptr)
    , m_toleranceSpin(nullptr)
    , m_initRx(nullptr), m_initRy(nullptr), m_initRz(nullptr)
    , m_initTx(nullptr), m_initTy(nullptr), m_initTz(nullptr)
    , m_rangeRx(nullptr), m_rangeRy(nullptr), m_rangeRz(nullptr)
    , m_rangeTx(nullptr), m_rangeTy(nullptr), m_rangeTz(nullptr)
    , m_apUpDownCheck(nullptr), m_apHorizontalCheck(nullptr)
    , m_latUpDownCheck(nullptr), m_latHorizontalCheck(nullptr)
    , m_generateDRRCheck(nullptr)
    , m_apImageView(nullptr)
    , m_latImageView(nullptr)
    , m_ctImageView(nullptr)
    , m_resultImageView(nullptr)
    , m_resultImageCombo(nullptr)
    , m_progressGroup(nullptr)
    , m_progressBar(nullptr)
    , m_statusLabel(nullptr)
    , m_phaseLabel(nullptr)
    , m_viewLabel(nullptr)
    , m_logTextEdit(nullptr)
    , m_resultGroup(nullptr)
    , m_resultTable(nullptr)
    , m_finalMetricLabel(nullptr)
    , m_durationLabel(nullptr)
    , m_isRegistering(false)
{
    setupUI();
    setupConnections();
    qDebug() << "[Registration2D3DWidget] Enhanced plugin widget created";
}

Registration2D3DWidget::~Registration2D3DWidget()
{
    qDebug() << "[Registration2D3DWidget] Internal plugin widget destroyed";
}

void Registration2D3DWidget::setPatientId(const QString& patientId)
{
    m_patientId = patientId;
}

void Registration2D3DWidget::loadXRayImages(const QString& apImagePath, const QString& latImagePath)
{
    m_apImagePath = apImagePath;
    m_latImagePath = latImagePath;

    if (!apImagePath.isEmpty()) {
        QPixmap apPixmap(apImagePath);
        if (!apPixmap.isNull()) {
            m_apImageView->setPixmap(apPixmap.scaled(m_apImageView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_logTextEdit->append(QString("[%1] AP视图已加载: %2").arg(QTime::currentTime().toString("HH:mm:ss")).arg(apImagePath));
        }
    }

    if (!latImagePath.isEmpty()) {
        QPixmap latPixmap(latImagePath);
        if (!latPixmap.isNull()) {
            m_latImageView->setPixmap(latPixmap.scaled(m_latImageView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_logTextEdit->append(QString("[%1] LAT视图已加载: %2").arg(QTime::currentTime().toString("HH:mm:ss")).arg(latImagePath));
        }
    }
}

void Registration2D3DWidget::loadCTImage(const QString& ctPath)
{
    m_ctImagePath = ctPath;
    QFileInfo fi(ctPath);
    m_ctImageView->setText("CT图像已加载\n" + fi.fileName());
    m_logTextEdit->append(QString("[%1] CT图像已加载: %2").arg(QTime::currentTime().toString("HH:mm:ss")).arg(ctPath));
}

void Registration2D3DWidget::loadBoneSegmentation(const QString& bonePath)
{
    m_boneImagePath = bonePath;
    m_logTextEdit->append(QString("[%1] 骨骼分割已加载: %2").arg(QTime::currentTime().toString("HH:mm:ss")).arg(bonePath));
}

void Registration2D3DWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(8);

    // 控制面板
    setupControlPanel();

    // 创建垂直分割器：上部内容区 + 底部结果区
    QSplitter* verticalSplitter = new QSplitter(Qt::Vertical);

    // ========== 上部：水平分割器（参数 + 图像 + 进度）==========
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧：参数配置
    QWidget* leftWidget = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    setupParameterPanel();
    leftLayout->addWidget(m_parameterGroup);
    leftWidget->setMinimumWidth(380);
    leftWidget->setMaximumWidth(420);

    // 中间：图像显示
    QWidget* centerWidget = new QWidget();
    setupImagePanel();

    // 右侧：仅配准进度
    QWidget* rightWidget = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    setupResultPanel();
    // 只添加进度面板到右侧
    rightLayout->addWidget(m_progressGroup);
    rightLayout->addStretch();
    rightWidget->setMinimumWidth(280);
    rightWidget->setMaximumWidth(350);

    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(centerWidget);
    mainSplitter->addWidget(rightWidget);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 3);
    mainSplitter->setStretchFactor(2, 1);

    // ========== 底部：配准结果 ==========
    QWidget* bottomWidget = new QWidget();
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    // 配准结果放在底部
    bottomLayout->addWidget(m_resultGroup);
    bottomWidget->setMinimumHeight(160);
    bottomWidget->setMaximumHeight(280);

    // 添加到垂直分割器
    verticalSplitter->addWidget(mainSplitter);
    verticalSplitter->addWidget(bottomWidget);
    verticalSplitter->setStretchFactor(0, 4);  // 上部区域占更大比例
    verticalSplitter->setStretchFactor(1, 1);  // 底部结果区较小

    m_mainLayout->addWidget(verticalSplitter);
}

void Registration2D3DWidget::setupControlPanel()
{
    QHBoxLayout* controlLayout = new QHBoxLayout();

    QString btnStyle =
        "QPushButton { padding: 6px 12px; border: 1px solid #3498db; border-radius: 4px; background: #34495e; color: white; font-weight: bold; }"
        "QPushButton:hover { background: #3498db; }"
        "QPushButton:disabled { background: #7f8c8d; border-color: #95a5a6; }";
    QString startBtnStyle =
        "QPushButton { padding: 6px 16px; border: 2px solid #27ae60; border-radius: 4px; background: #27ae60; color: white; font-weight: bold; }"
        "QPushButton:hover { background: #2ecc71; }"
        "QPushButton:disabled { background: #7f8c8d; border-color: #95a5a6; }";
    QString cancelBtnStyle =
        "QPushButton { padding: 6px 12px; border: 2px solid #e74c3c; border-radius: 4px; background: #e74c3c; color: white; font-weight: bold; }"
        "QPushButton:hover { background: #c0392b; }"
        "QPushButton:disabled { background: #7f8c8d; border-color: #95a5a6; }";

    m_loadAPBtn = new QPushButton("📷 AP视图");
    m_loadLATBtn = new QPushButton("📷 LAT视图");
    m_loadCTBtn = new QPushButton("📁 CT图像");
    m_loadBoneBtn = new QPushButton("🦴 骨骼分割");
    m_startBtn = new QPushButton("▶ 开始配准");
    m_cancelBtn = new QPushButton("⏹ 取消");
    m_clearBtn = new QPushButton("🗑️ 清空");
    m_resetParamsBtn = new QPushButton("↺ 重置参数");

    m_cancelBtn->setEnabled(false);

    m_loadAPBtn->setStyleSheet(btnStyle);
    m_loadLATBtn->setStyleSheet(btnStyle);
    m_loadCTBtn->setStyleSheet(btnStyle);
    m_loadBoneBtn->setStyleSheet(btnStyle);
    m_startBtn->setStyleSheet(startBtnStyle);
    m_cancelBtn->setStyleSheet(cancelBtnStyle);
    m_clearBtn->setStyleSheet(btnStyle);
    m_resetParamsBtn->setStyleSheet(btnStyle);

    controlLayout->addWidget(m_loadAPBtn);
    controlLayout->addWidget(m_loadLATBtn);
    controlLayout->addWidget(m_loadCTBtn);
    controlLayout->addWidget(m_loadBoneBtn);
    controlLayout->addSpacing(20);
    controlLayout->addWidget(m_startBtn);
    controlLayout->addWidget(m_cancelBtn);
    controlLayout->addSpacing(20);
    controlLayout->addWidget(m_clearBtn);
    controlLayout->addWidget(m_resetParamsBtn);
    controlLayout->addStretch();

    m_mainLayout->addLayout(controlLayout);
}

void Registration2D3DWidget::setupParameterPanel()
{
    m_parameterGroup = new QGroupBox("配准参数配置");
    m_parameterGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #3498db; }");

    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* paramWidget = new QWidget();
    QVBoxLayout* paramMainLayout = new QVBoxLayout(paramWidget);
    paramMainLayout->setSpacing(10);

    // 基本参数
    QGroupBox* basicGroup = new QGroupBox("基本参数");
    QFormLayout* basicLayout = new QFormLayout(basicGroup);

    m_kdTreeNumSpin = new QSpinBox();
    m_kdTreeNumSpin->setRange(5, 200);
    m_kdTreeNumSpin->setValue(50);
    m_kdTreeNumSpin->setToolTip("K-d树空间划分数量，更大值增加搜索精度但耗时更长");

    m_toleranceSpin = new QDoubleSpinBox();
    m_toleranceSpin->setRange(0.0001, 1.0);
    m_toleranceSpin->setValue(0.001);
    m_toleranceSpin->setDecimals(4);
    m_toleranceSpin->setToolTip("收敛阈值，更小值增加精度但耗时更长");

    basicLayout->addRow("K-d树数量:", m_kdTreeNumSpin);
    basicLayout->addRow("收敛阈值:", m_toleranceSpin);
    paramMainLayout->addWidget(basicGroup);

    // 初始参数 - 使用3列布局更紧凑
    QGroupBox* initGroup = new QGroupBox("初始参数 (角度°/mm)");
    QGridLayout* initLayout = new QGridLayout(initGroup);
    initLayout->setSpacing(6);
    initLayout->setColumnStretch(1, 1);
    initLayout->setColumnStretch(3, 1);
    initLayout->setColumnStretch(5, 1);

    m_initRx = new QDoubleSpinBox(); m_initRx->setRange(-180, 180); m_initRx->setValue(0);
    m_initRy = new QDoubleSpinBox(); m_initRy->setRange(-180, 180); m_initRy->setValue(0);
    m_initRz = new QDoubleSpinBox(); m_initRz->setRange(-180, 180); m_initRz->setValue(0);
    m_initTx = new QDoubleSpinBox(); m_initTx->setRange(-500, 500); m_initTx->setValue(0);
    m_initTy = new QDoubleSpinBox(); m_initTy->setRange(-500, 500); m_initTy->setValue(0);
    m_initTz = new QDoubleSpinBox(); m_initTz->setRange(-500, 500); m_initTz->setValue(0);
    // 设置SpinBox最小宽度
    m_initRx->setMinimumWidth(65); m_initRy->setMinimumWidth(65); m_initRz->setMinimumWidth(65);
    m_initTx->setMinimumWidth(65); m_initTy->setMinimumWidth(65); m_initTz->setMinimumWidth(65);

    initLayout->addWidget(new QLabel("Rx:"), 0, 0); initLayout->addWidget(m_initRx, 0, 1);
    initLayout->addWidget(new QLabel("Ry:"), 0, 2); initLayout->addWidget(m_initRy, 0, 3);
    initLayout->addWidget(new QLabel("Rz:"), 0, 4); initLayout->addWidget(m_initRz, 0, 5);
    initLayout->addWidget(new QLabel("Tx:"), 1, 0); initLayout->addWidget(m_initTx, 1, 1);
    initLayout->addWidget(new QLabel("Ty:"), 1, 2); initLayout->addWidget(m_initTy, 1, 3);
    initLayout->addWidget(new QLabel("Tz:"), 1, 4); initLayout->addWidget(m_initTz, 1, 5);
    paramMainLayout->addWidget(initGroup);

    // 搜索范围 - 使用3列布局更紧凑
    QGroupBox* rangeGroup = new QGroupBox("搜索范围 (角度°/mm)");
    QGridLayout* rangeLayout = new QGridLayout(rangeGroup);
    rangeLayout->setSpacing(6);
    rangeLayout->setColumnStretch(1, 1);
    rangeLayout->setColumnStretch(3, 1);
    rangeLayout->setColumnStretch(5, 1);

    m_rangeRx = new QSpinBox(); m_rangeRx->setRange(1, 90); m_rangeRx->setValue(15);
    m_rangeRy = new QSpinBox(); m_rangeRy->setRange(1, 90); m_rangeRy->setValue(15);
    m_rangeRz = new QSpinBox(); m_rangeRz->setRange(1, 90); m_rangeRz->setValue(15);
    m_rangeTx = new QSpinBox(); m_rangeTx->setRange(1, 200); m_rangeTx->setValue(50);
    m_rangeTy = new QSpinBox(); m_rangeTy->setRange(1, 200); m_rangeTy->setValue(50);
    m_rangeTz = new QSpinBox(); m_rangeTz->setRange(1, 200); m_rangeTz->setValue(50);
    // 设置SpinBox最小宽度
    m_rangeRx->setMinimumWidth(55); m_rangeRy->setMinimumWidth(55); m_rangeRz->setMinimumWidth(55);
    m_rangeTx->setMinimumWidth(55); m_rangeTy->setMinimumWidth(55); m_rangeTz->setMinimumWidth(55);

    rangeLayout->addWidget(new QLabel("Rx:"), 0, 0); rangeLayout->addWidget(m_rangeRx, 0, 1);
    rangeLayout->addWidget(new QLabel("Ry:"), 0, 2); rangeLayout->addWidget(m_rangeRy, 0, 3);
    rangeLayout->addWidget(new QLabel("Rz:"), 0, 4); rangeLayout->addWidget(m_rangeRz, 0, 5);
    rangeLayout->addWidget(new QLabel("Tx:"), 1, 0); rangeLayout->addWidget(m_rangeTx, 1, 1);
    rangeLayout->addWidget(new QLabel("Ty:"), 1, 2); rangeLayout->addWidget(m_rangeTy, 1, 3);
    rangeLayout->addWidget(new QLabel("Tz:"), 1, 4); rangeLayout->addWidget(m_rangeTz, 1, 5);
    paramMainLayout->addWidget(rangeGroup);

    // 图像翻转选项
    QGroupBox* flipGroup = new QGroupBox("图像翻转选项");
    QGridLayout* flipLayout = new QGridLayout(flipGroup);

    m_apUpDownCheck = new QCheckBox("AP上下翻转");
    m_apHorizontalCheck = new QCheckBox("AP左右翻转");
    m_latUpDownCheck = new QCheckBox("LAT上下翻转");
    m_latHorizontalCheck = new QCheckBox("LAT左右翻转");
    m_generateDRRCheck = new QCheckBox("生成验证图像");
    m_generateDRRCheck->setChecked(true);

    flipLayout->addWidget(m_apUpDownCheck, 0, 0);
    flipLayout->addWidget(m_apHorizontalCheck, 0, 1);
    flipLayout->addWidget(m_latUpDownCheck, 1, 0);
    flipLayout->addWidget(m_latHorizontalCheck, 1, 1);
    flipLayout->addWidget(m_generateDRRCheck, 2, 0, 1, 2);
    paramMainLayout->addWidget(flipGroup);

    paramMainLayout->addStretch();
    scrollArea->setWidget(paramWidget);

    QVBoxLayout* groupLayout = new QVBoxLayout(m_parameterGroup);
    groupLayout->addWidget(scrollArea);
}

void Registration2D3DWidget::setupImagePanel()
{
    // 图像显示在中间区域（由父布局管理）
    // 这里创建图像视图控件
    QString viewStyle = "QLabel { background: #1e293b; border: 1px solid #3498db; border-radius: 4px; color: #94a3b8; font-size: 12px; }";

    m_apImageView = new QLabel("AP视图\n点击加载");
    m_apImageView->setAlignment(Qt::AlignCenter);
    m_apImageView->setMinimumSize(200, 200);
    m_apImageView->setStyleSheet(viewStyle);
    m_apImageView->setScaledContents(false);

    m_latImageView = new QLabel("LAT视图\n点击加载");
    m_latImageView->setAlignment(Qt::AlignCenter);
    m_latImageView->setMinimumSize(200, 200);
    m_latImageView->setStyleSheet(viewStyle);

    m_ctImageView = new QLabel("CT图像\n点击加载");
    m_ctImageView->setAlignment(Qt::AlignCenter);
    m_ctImageView->setMinimumSize(200, 200);
    m_ctImageView->setStyleSheet(viewStyle);

    m_resultImageView = new QLabel("配准结果\n等待配准完成");
    m_resultImageView->setAlignment(Qt::AlignCenter);
    m_resultImageView->setMinimumSize(200, 200);
    m_resultImageView->setStyleSheet(viewStyle);
}

void Registration2D3DWidget::setupResultPanel()
{
    // 进度显示区域
    m_progressGroup = new QGroupBox("配准进度");
    m_progressGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #27ae60; }");
    QVBoxLayout* progressLayout = new QVBoxLayout(m_progressGroup);

    QHBoxLayout* statusRow = new QHBoxLayout();
    m_viewLabel = new QLabel("视角: -");
    m_viewLabel->setStyleSheet("color: #3498db; font-weight: bold;");
    m_phaseLabel = new QLabel("阶段: -");
    m_phaseLabel->setStyleSheet("color: #9b59b6;");
    statusRow->addWidget(m_viewLabel);
    statusRow->addWidget(m_phaseLabel);
    statusRow->addStretch();
    progressLayout->addLayout(statusRow);

    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setStyleSheet("color: #2ecc71; font-size: 14px;");
    progressLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar();
    m_progressBar->setStyleSheet(
        "QProgressBar { border: 1px solid #3498db; border-radius: 4px; background: #2c3e50; text-align: center; color: white; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3498db, stop:1 #2ecc71); }");
    progressLayout->addWidget(m_progressBar);

    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setMaximumHeight(120);
    m_logTextEdit->setStyleSheet("QTextEdit { background: #1e293b; color: #94a3b8; border: 1px solid #475569; border-radius: 4px; font-family: monospace; font-size: 11px; }");
    progressLayout->addWidget(m_logTextEdit);

    // 结果显示区域 - 水平布局适合底部显示
    m_resultGroup = new QGroupBox("配准结果");
    m_resultGroup->setStyleSheet(
        "QGroupBox { font-weight: bold; color: #e74c3c; background: rgba(30,41,59,0.9); "
        "border: 1px solid rgba(231,76,60,0.4); border-radius: 8px; margin-top: 8px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 15px; padding: 0 8px; color: #e74c3c; }"
    );
    QHBoxLayout* resultLayout = new QHBoxLayout(m_resultGroup);
    resultLayout->setContentsMargins(12, 12, 12, 8);
    resultLayout->setSpacing(20);

    // 左侧：结果图像选择和指标
    QVBoxLayout* leftResultLayout = new QVBoxLayout();
    leftResultLayout->setSpacing(8);

    // 结果图像选择
    QHBoxLayout* resultImageRow = new QHBoxLayout();
    QLabel* imgLabel = new QLabel("结果图像:");
    imgLabel->setStyleSheet("color: #e2e8f0;");
    resultImageRow->addWidget(imgLabel);
    m_resultImageCombo = new QComboBox();
    m_resultImageCombo->addItems({"AP-DRR", "AP-棋盘格", "AP-边缘", "LAT-DRR", "LAT-棋盘格", "LAT-边缘"});
    m_resultImageCombo->setStyleSheet(
        "QComboBox { background: #334155; color: #e2e8f0; border: 1px solid #475569; border-radius: 4px; padding: 4px 8px; min-width: 120px; }"
        "QComboBox:hover { border-color: #e74c3c; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: #1e293b; color: #e2e8f0; selection-background-color: #e74c3c; }"
    );
    resultImageRow->addWidget(m_resultImageCombo);
    resultImageRow->addStretch();
    leftResultLayout->addLayout(resultImageRow);

    // 关键结果指标
    QHBoxLayout* metricsRow = new QHBoxLayout();
    m_finalMetricLabel = new QLabel("最终度量: -");
    m_finalMetricLabel->setStyleSheet("color: #f39c12; font-weight: bold; font-size: 13px;");
    m_durationLabel = new QLabel("耗时: -");
    m_durationLabel->setStyleSheet("color: #1abc9c; font-size: 13px;");
    metricsRow->addWidget(m_finalMetricLabel);
    metricsRow->addSpacing(20);
    metricsRow->addWidget(m_durationLabel);
    metricsRow->addStretch();
    leftResultLayout->addLayout(metricsRow);
    leftResultLayout->addStretch();

    resultLayout->addLayout(leftResultLayout);

    // 右侧：结果表格 - 2行7列的紧凑布局，完全显示无滚动条
    m_resultTable = new QTableWidget(2, 7);
    m_resultTable->setHorizontalHeaderLabels({"视图", "Rx (°)", "Ry (°)", "Rz (°)", "Tx (mm)", "Ty (mm)", "Tz (mm)"});
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setMinimumSectionSize(55);
    m_resultTable->horizontalHeader()->setFixedHeight(28);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->verticalHeader()->setDefaultSectionSize(32);
    // 禁用滚动条，完全显示
    m_resultTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultTable->setStyleSheet(
        "QTableWidget { background: #1e293b; color: #e2e8f0; gridline-color: #475569; "
        "border: 1px solid #475569; border-radius: 4px; }"
        "QTableWidget::item { padding: 6px; }"
        "QHeaderView::section { background: #334155; color: white; font-weight: bold; padding: 8px; border: none; }"
    );
    // 固定高度 = 表头(28) + 2行(32*2) + 边框(4) = 96
    m_resultTable->setFixedHeight(96);

    // AP行和LAT行
    m_resultTable->setItem(0, 0, new QTableWidgetItem("AP"));
    m_resultTable->setItem(1, 0, new QTableWidgetItem("LAT"));
    for (int col = 1; col <= 6; col++) {
        m_resultTable->setItem(0, col, new QTableWidgetItem("-"));
        m_resultTable->setItem(1, col, new QTableWidgetItem("-"));
    }
    // 设置视图列样式
    if (m_resultTable->item(0, 0)) {
        m_resultTable->item(0, 0)->setBackground(QColor(52, 152, 219, 80));
        m_resultTable->item(0, 0)->setTextAlignment(Qt::AlignCenter);
    }
    if (m_resultTable->item(1, 0)) {
        m_resultTable->item(1, 0)->setBackground(QColor(155, 89, 182, 80));
        m_resultTable->item(1, 0)->setTextAlignment(Qt::AlignCenter);
    }

    resultLayout->addWidget(m_resultTable, 1);

    // 注意：m_progressGroup 和 m_resultGroup 会在 setupUI() 中被添加到布局
}

void Registration2D3DWidget::setupConnections()
{
    connect(m_loadAPBtn, &QPushButton::clicked, this, &Registration2D3DWidget::onLoadAPImage);
    connect(m_loadLATBtn, &QPushButton::clicked, this, &Registration2D3DWidget::onLoadLATImage);
    connect(m_loadCTBtn, &QPushButton::clicked, this, &Registration2D3DWidget::onLoadCTImage);
    connect(m_loadBoneBtn, &QPushButton::clicked, this, &Registration2D3DWidget::onLoadBoneImage);
    connect(m_startBtn, &QPushButton::clicked, this, &Registration2D3DWidget::onStartRegistration);
    connect(m_cancelBtn, &QPushButton::clicked, this, &Registration2D3DWidget::onCancelRegistration);
    connect(m_clearBtn, &QPushButton::clicked, this, &Registration2D3DWidget::onClearAll);
    connect(m_resetParamsBtn, &QPushButton::clicked, this, &Registration2D3DWidget::onResetParameters);
    connect(m_resultImageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Registration2D3DWidget::onShowResultImage);

    // 连接服务信号
    if (m_service) {
        connect(m_service, &Registration2D3DService::registrationStarted,
                this, &Registration2D3DWidget::onServiceRegistrationStarted);
        connect(m_service, &Registration2D3DService::progressUpdated,
                this, &Registration2D3DWidget::onServiceProgressUpdated);
        connect(m_service, &Registration2D3DService::registrationCompleted,
                this, &Registration2D3DWidget::onServiceRegistrationCompleted);
        connect(m_service, &Registration2D3DService::registrationFailed,
                this, &Registration2D3DWidget::onServiceRegistrationFailed);
    }
}

void Registration2D3DWidget::updateUIState(bool registering)
{
    m_isRegistering = registering;
    m_loadAPBtn->setEnabled(!registering);
    m_loadLATBtn->setEnabled(!registering);
    m_loadCTBtn->setEnabled(!registering);
    m_loadBoneBtn->setEnabled(!registering);
    m_startBtn->setEnabled(!registering);
    m_clearBtn->setEnabled(!registering);
    m_resetParamsBtn->setEnabled(!registering);
    m_cancelBtn->setEnabled(registering);
    m_kdTreeNumSpin->setEnabled(!registering);
    m_toleranceSpin->setEnabled(!registering);
    m_parameterGroup->setEnabled(!registering);
}

Registration2D3DParameters Registration2D3DWidget::collectParameters()
{
    Registration2D3DParameters params;
    params.ctPath = m_ctImagePath;
    params.xrayApPath = m_apImagePath;
    params.xrayLatPath = m_latImagePath;
    params.jingguPath = m_boneImagePath;

    // 初始参数
    params.initParams = {
        m_initRx->value(), m_initRy->value(), m_initRz->value(),
        m_initTx->value(), m_initTy->value(), m_initTz->value()
    };

    // 搜索范围
    params.searchRange = {
        m_rangeRx->value(), m_rangeRy->value(), m_rangeRz->value(),
        m_rangeTx->value(), m_rangeTy->value(), m_rangeTz->value()
    };

    params.kdTreeNum = m_kdTreeNumSpin->value();
    params.apUpDown = m_apUpDownCheck->isChecked();
    params.apHorizontal = m_apHorizontalCheck->isChecked();
    params.latUpDown = m_latUpDownCheck->isChecked();
    params.latHorizontal = m_latHorizontalCheck->isChecked();
    params.generateDRR = m_generateDRRCheck->isChecked();
    params.outputDirectory = QDir::tempPath();

    return params;
}

void Registration2D3DWidget::onLoadAPImage()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择AP视图X光图像", "", "图像文件 (*.png *.jpg *.dcm *.mha *.nii *.nii.gz)");
    if (!filePath.isEmpty()) {
        loadXRayImages(filePath, m_latImagePath);
    }
}

void Registration2D3DWidget::onLoadLATImage()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择LAT视图X光图像", "", "图像文件 (*.png *.jpg *.dcm *.mha *.nii *.nii.gz)");
    if (!filePath.isEmpty()) {
        loadXRayImages(m_apImagePath, filePath);
    }
}

void Registration2D3DWidget::onLoadCTImage()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择CT图像", "", "医学图像 (*.nii *.nii.gz *.mha *.nrrd *.dcm);;所有文件 (*)");
    if (!filePath.isEmpty()) {
        loadCTImage(filePath);
    }
}

void Registration2D3DWidget::onLoadBoneImage()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择骨骼分割图像", "", "医学图像 (*.nii *.nii.gz *.mha *.nrrd);;所有文件 (*)");
    if (!filePath.isEmpty()) {
        loadBoneSegmentation(filePath);
    }
}

void Registration2D3DWidget::onResetParameters()
{
    m_initRx->setValue(0); m_initRy->setValue(0); m_initRz->setValue(0);
    m_initTx->setValue(0); m_initTy->setValue(0); m_initTz->setValue(0);
    m_rangeRx->setValue(15); m_rangeRy->setValue(15); m_rangeRz->setValue(15);
    m_rangeTx->setValue(50); m_rangeTy->setValue(50); m_rangeTz->setValue(50);
    m_kdTreeNumSpin->setValue(50);
    m_toleranceSpin->setValue(0.001);
    m_apUpDownCheck->setChecked(false);
    m_apHorizontalCheck->setChecked(false);
    m_latUpDownCheck->setChecked(false);
    m_latHorizontalCheck->setChecked(false);
    m_generateDRRCheck->setChecked(true);
    m_logTextEdit->append(QString("[%1] 参数已重置为默认值").arg(QTime::currentTime().toString("HH:mm:ss")));
}

void Registration2D3DWidget::onShowResultImage(int index)
{
    if (!m_lastResult.isSuccess()) return;

    QString imagePath;
    switch (index) {
        case 0: imagePath = m_lastResult.apResult.drrImagePath; break;
        case 1: imagePath = m_lastResult.apResult.checkerboardPath; break;
        case 2: imagePath = m_lastResult.apResult.edgeOverlayPath; break;
        case 3: imagePath = m_lastResult.latResult.drrImagePath; break;
        case 4: imagePath = m_lastResult.latResult.checkerboardPath; break;
        case 5: imagePath = m_lastResult.latResult.edgeOverlayPath; break;
    }

    if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
        QPixmap pixmap(imagePath);
        if (!pixmap.isNull()) {
            m_resultImageView->setPixmap(pixmap.scaled(m_resultImageView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void Registration2D3DWidget::loadResultImages(const Registration2D3DResult& result)
{
    m_lastResult = result;

    // 更新结果表格 - 新布局：2行7列（视图, Rx, Ry, Rz, Tx, Ty, Tz）
    // 第0行：AP结果
    m_resultTable->item(0, 1)->setText(QString::number(result.apResult.rx, 'f', 2));
    m_resultTable->item(0, 2)->setText(QString::number(result.apResult.ry, 'f', 2));
    m_resultTable->item(0, 3)->setText(QString::number(result.apResult.rz, 'f', 2));
    m_resultTable->item(0, 4)->setText(QString::number(result.apResult.tx, 'f', 2));
    m_resultTable->item(0, 5)->setText(QString::number(result.apResult.ty, 'f', 2));
    m_resultTable->item(0, 6)->setText(QString::number(result.apResult.tz, 'f', 2));

    // 第1行：LAT结果
    m_resultTable->item(1, 1)->setText(QString::number(result.latResult.rx, 'f', 2));
    m_resultTable->item(1, 2)->setText(QString::number(result.latResult.ry, 'f', 2));
    m_resultTable->item(1, 3)->setText(QString::number(result.latResult.rz, 'f', 2));
    m_resultTable->item(1, 4)->setText(QString::number(result.latResult.tx, 'f', 2));
    m_resultTable->item(1, 5)->setText(QString::number(result.latResult.ty, 'f', 2));
    m_resultTable->item(1, 6)->setText(QString::number(result.latResult.tz, 'f', 2));

    m_finalMetricLabel->setText(QString("最终度量: %1").arg(result.finalMetric, 0, 'f', 4));
    m_durationLabel->setText(QString("耗时: %1秒").arg(result.durationSeconds));

    // 加载第一张结果图像
    onShowResultImage(0);
}

void Registration2D3DWidget::onStartRegistration()
{
    if (m_apImagePath.isEmpty() || m_latImagePath.isEmpty() || m_ctImagePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先加载所有必需的图像（AP视图、LAT视图、CT图像）");
        return;
    }

    if (!m_service) {
        QMessageBox::critical(this, "错误", "配准服务未初始化");
        return;
    }

    Registration2D3DParameters params = collectParameters();
    m_currentRegistrationId = m_service->startRegistration(params);

    if (m_currentRegistrationId.isEmpty()) {
        QString error = m_service->getLastError();
        QMessageBox::critical(this, "错误", "启动配准失败: " + error);
    } else {
        m_logTextEdit->append(QString("[%1] 配准任务已启动，ID: %2").arg(QTime::currentTime().toString("HH:mm:ss")).arg(m_currentRegistrationId.left(8)));
    }
}

void Registration2D3DWidget::onCancelRegistration()
{
    if (!m_currentRegistrationId.isEmpty() && m_service) {
        m_service->cancelRegistration(m_currentRegistrationId);
        m_logTextEdit->append(QString("[%1] 配准已取消").arg(QTime::currentTime().toString("HH:mm:ss")));
    }
}

void Registration2D3DWidget::onClearAll()
{
    m_apImagePath.clear();
    m_latImagePath.clear();
    m_ctImagePath.clear();
    m_boneImagePath.clear();
    m_currentRegistrationId.clear();

    m_apImageView->clear();
    m_apImageView->setText("AP视图\n点击加载");
    m_latImageView->clear();
    m_latImageView->setText("LAT视图\n点击加载");
    m_ctImageView->clear();
    m_ctImageView->setText("CT图像\n点击加载");
    m_resultImageView->clear();
    m_resultImageView->setText("配准结果\n等待配准完成");

    m_progressBar->setValue(0);
    m_statusLabel->setText("就绪");
    m_viewLabel->setText("视角: -");
    m_phaseLabel->setText("阶段: -");
    m_logTextEdit->clear();

    // 重置结果表格
    for (int i = 0; i < 6; i++) {
        m_resultTable->item(i, 1)->setText("-");
        m_resultTable->item(i, 2)->setText("-");
    }
    m_finalMetricLabel->setText("最终度量: -");
    m_durationLabel->setText("耗时: -");

    m_logTextEdit->append(QString("[%1] 所有数据已清空").arg(QTime::currentTime().toString("HH:mm:ss")));
}

void Registration2D3DWidget::onServiceRegistrationStarted(const QString& registrationId)
{
    if (registrationId == m_currentRegistrationId) {
        updateUIState(true);
        m_statusLabel->setText("配准进行中...");
        m_viewLabel->setText("视角: 初始化");
        m_phaseLabel->setText("阶段: 准备");
        m_progressBar->setValue(0);
        m_logTextEdit->append(QString("[%1] ====== 配准任务开始 ======").arg(QTime::currentTime().toString("HH:mm:ss")));
        emit registrationStarted();
    }
}

void Registration2D3DWidget::onServiceProgressUpdated(const QString& registrationId, const Registration2D3DProgress& progress)
{
    if (registrationId == m_currentRegistrationId) {
        m_progressBar->setValue(progress.percentage);
        m_statusLabel->setText(progress.message);
        m_viewLabel->setText(QString("视角: %1").arg(progress.currentView.isEmpty() ? "-" : progress.currentView));
        m_phaseLabel->setText(QString("阶段: %1").arg(progress.currentPhase));

        // 根据阶段设置不同的颜色
        QString phaseColor = "#9b59b6";
        if (progress.currentPhase.contains("优化")) phaseColor = "#e74c3c";
        else if (progress.currentPhase.contains("完成")) phaseColor = "#27ae60";
        m_phaseLabel->setStyleSheet(QString("color: %1;").arg(phaseColor));

        m_logTextEdit->append(QString("[%1] [%2%] [%3] %4")
            .arg(QTime::currentTime().toString("HH:mm:ss"))
            .arg(progress.percentage, 3)
            .arg(progress.currentView.isEmpty() ? "-" : progress.currentView)
            .arg(progress.message));

        // 自动滚动到底部
        m_logTextEdit->verticalScrollBar()->setValue(m_logTextEdit->verticalScrollBar()->maximum());
    }
}

void Registration2D3DWidget::onServiceRegistrationCompleted(const QString& registrationId, const Registration2D3DResult& result)
{
    if (registrationId == m_currentRegistrationId) {
        updateUIState(false);
        m_progressBar->setValue(100);
        m_statusLabel->setText("✓ 配准完成!");
        m_statusLabel->setStyleSheet("color: #27ae60; font-size: 14px; font-weight: bold;");
        m_viewLabel->setText("视角: 完成");
        m_phaseLabel->setText("阶段: 完成");

        // 加载结果数据到UI
        loadResultImages(result);

        m_logTextEdit->append(QString("[%1] ====== 配准成功完成 ======").arg(QTime::currentTime().toString("HH:mm:ss")));
        m_logTextEdit->append(QString("  AP结果: Rx=%1°, Ry=%2°, Rz=%3°, Tx=%4mm, Ty=%5mm, Tz=%6mm")
            .arg(result.apResult.rx, 0, 'f', 2).arg(result.apResult.ry, 0, 'f', 2).arg(result.apResult.rz, 0, 'f', 2)
            .arg(result.apResult.tx, 0, 'f', 2).arg(result.apResult.ty, 0, 'f', 2).arg(result.apResult.tz, 0, 'f', 2));
        m_logTextEdit->append(QString("  LAT结果: Rx=%1°, Ry=%2°, Rz=%3°, Tx=%4mm, Ty=%5mm, Tz=%6mm")
            .arg(result.latResult.rx, 0, 'f', 2).arg(result.latResult.ry, 0, 'f', 2).arg(result.latResult.rz, 0, 'f', 2)
            .arg(result.latResult.tx, 0, 'f', 2).arg(result.latResult.ty, 0, 'f', 2).arg(result.latResult.tz, 0, 'f', 2));
        m_logTextEdit->append(QString("  最终度量: %1, 耗时: %2秒").arg(result.finalMetric, 0, 'f', 4).arg(result.durationSeconds));

        emit registrationCompleted(result);
    }
}

void Registration2D3DWidget::onServiceRegistrationFailed(const QString& registrationId, const QString& errorMessage)
{
    if (registrationId == m_currentRegistrationId) {
        updateUIState(false);
        m_statusLabel->setText("✗ 配准失败");
        m_statusLabel->setStyleSheet("color: #e74c3c; font-size: 14px; font-weight: bold;");
        m_viewLabel->setText("视角: 错误");
        m_phaseLabel->setText("阶段: 失败");

        m_logTextEdit->append(QString("[%1] ====== 配准失败 ======").arg(QTime::currentTime().toString("HH:mm:ss")));
        m_logTextEdit->append(QString("  错误: %1").arg(errorMessage));

        QMessageBox::critical(this, "配准失败", errorMessage);
        emit registrationFailed(errorMessage);
    }
}

