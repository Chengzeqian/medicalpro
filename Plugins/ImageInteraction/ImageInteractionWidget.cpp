#include "ImageInteractionWidget.h"
#include "ImageInteractionService.h"
#include "ServiceInterfaces.h"

#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QSplitter>
#include <QDebug>
#include <QTimer>
#include <QStandardPaths>
#include <QFormLayout>
#include <QGraphicsPixmapItem>
#include <QTime>

//-----------------------------------------------------------------------------
ImageInteractionWidget::ImageInteractionWidget(QWidget* parent)
    : QWidget(parent)
    , m_pluginContext(nullptr)
    , m_interactionService(nullptr)
    , m_imageService(nullptr)
    , m_serviceConnected(false)
    , m_currentMode("PointSelection")
    , m_currentTool("Point")
{
    qDebug() << "[ImageInteractionWidget] 创建图像交互工具界面";
    
    setWindowTitle("图像交互工具");
    setMinimumSize(1000, 700);
    
    initializeUI();
    setupStyles();
    connectSignals();
    
    qDebug() << "[ImageInteractionWidget] 图像交互工具界面创建完成";
}

//-----------------------------------------------------------------------------
ImageInteractionWidget::~ImageInteractionWidget()
{
    qDebug() << "[ImageInteractionWidget] 销毁图像交互工具界面";
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::setPluginContext(ctkPluginContext* context)
{
    m_pluginContext = context;
    qDebug() << "[ImageInteractionWidget] 设置CTK插件上下文";
    initializeServiceConnections();
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::loadImageForInteraction(const QString& imageId)
{
    if (imageId.isEmpty()) {
        updateStatus("无效的图像ID");
        return;
    }
    
    m_currentImageId = imageId;
    updateStatus(QString("已加载图像: %1").arg(imageId));
    
    // 更新图像选择器
    for (int i = 0; i < m_imageSelector->count(); ++i) {
        if (m_imageSelector->itemData(i).toString() == imageId) {
            m_imageSelector->setCurrentIndex(i);
            break;
        }
    }
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::setInteractionMode(const QString& mode)
{
    if (m_currentMode == mode) return;
    
    m_currentMode = mode;
    
    // 切换到相应的工具面板
    if (mode == "PointSelection") {
        m_toolTabs->setCurrentWidget(m_pointSelectionPanel);
    } else if (mode == "Measurement") {
        m_toolTabs->setCurrentWidget(m_measurementPanel);
    } else if (mode == "Annotation") {
        m_toolTabs->setCurrentWidget(m_annotationPanel);
    } else if (mode == "ROI") {
        m_toolTabs->setCurrentWidget(m_roiPanel);
    } else if (mode == "3DInteraction") {
        m_toolTabs->setCurrentWidget(m_3dInteractionPanel);
    }
    
    updateStatus(QString("切换到交互模式: %1").arg(mode));
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::refreshImageList()
{
    updateStatus("刷新图像列表...");
    
    if (m_imageService) {
        QStringList imageIds = m_imageService->getLoadedImages();
        
        m_imageSelector->clear();
        m_imageSelector->addItem("请选择图像...", "");
        
        for (const QString& imageId : imageIds) {
            QString displayName = imageId;
            if (m_imageService) {
                QMap<QString, QVariant> info = m_imageService->getImageMetadata(imageId);
                if (!info.isEmpty()) {
                    displayName = info.value("filename", imageId).toString();
                }
            }
            m_imageSelector->addItem(displayName, imageId);
        }
        
        updateStatus(QString("发现 %1 个可交互的图像").arg(imageIds.size()));
    } else {
        updateStatus("图像服务不可用");
    }
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::resetAllTools()
{
    updateStatus("重置所有交互工具...");
    
    // 清除选择
    m_selectedPoints.clear();
    m_interactions.clear();
    
    // 重置工具状态
    if (m_toolButtonGroup) {
        m_toolButtonGroup->setExclusive(false);
        foreach (QAbstractButton* button, m_toolButtonGroup->buttons()) {
            button->setChecked(false);
        }
        m_toolButtonGroup->setExclusive(true);
    }
    
    // 清空结果表格
    m_resultTable->setRowCount(0);
    m_coordinateDisplay->clear();
    
    updateStatus("所有工具已重置");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::initializeUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(6);
    
    createMainToolBar();
    m_mainLayout->addWidget(m_mainToolBar);
    
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    createInteractionArea();
    createToolPanels();
    
    m_mainSplitter->addWidget(m_interactionSplitter);
    m_mainSplitter->addWidget(m_toolTabs);
    m_mainSplitter->setStretchFactor(0, 2);
    m_mainSplitter->setStretchFactor(1, 1);
    
    m_mainLayout->addWidget(m_mainSplitter);
    
    // 状态栏
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("就绪");
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_progressBar);
    
    m_mainLayout->addLayout(statusLayout);
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::createMainToolBar()
{
    m_mainToolBar = new QToolBar("交互工具栏", this);
    m_mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    
    // 交互模式选择
    m_interactionModeCombo = new QComboBox();
    m_interactionModeCombo->addItem("点选工具", "PointSelection");
    m_interactionModeCombo->addItem("测量工具", "Measurement");
    m_interactionModeCombo->addItem("标注工具", "Annotation");
    m_interactionModeCombo->addItem("ROI工具", "ROI");
    m_interactionModeCombo->addItem("3D交互", "3DInteraction");
    
    m_mainToolBar->addWidget(new QLabel("交互模式:"));
    m_mainToolBar->addWidget(m_interactionModeCombo);
    m_mainToolBar->addSeparator();
    
    // 图像选择
    m_imageSelector = new QComboBox();
    m_imageSelector->setMinimumWidth(200);
    m_imageSelector->addItem("请选择图像...", "");
    
    m_mainToolBar->addWidget(new QLabel("图像:"));
    m_mainToolBar->addWidget(m_imageSelector);
    m_mainToolBar->addSeparator();
    
    // 快速工具按钮
    m_toolButtonGroup = new QButtonGroup(this);
    m_toolButtonGroup->setExclusive(true);
    
    m_pointToolBtn = new QPushButton("📍 点选");
    m_regionToolBtn = new QPushButton("🔲 区域");
    m_distanceBtn = new QPushButton("📏 距离");
    m_angleBtn = new QPushButton("📐 角度");
    
    m_pointToolBtn->setCheckable(true);
    m_regionToolBtn->setCheckable(true);
    m_distanceBtn->setCheckable(true);
    m_angleBtn->setCheckable(true);
    
    m_toolButtonGroup->addButton(m_pointToolBtn, 0);
    m_toolButtonGroup->addButton(m_regionToolBtn, 1);
    m_toolButtonGroup->addButton(m_distanceBtn, 2);
    m_toolButtonGroup->addButton(m_angleBtn, 3);
    
    m_mainToolBar->addWidget(m_pointToolBtn);
    m_mainToolBar->addWidget(m_regionToolBtn);
    m_mainToolBar->addWidget(m_distanceBtn);
    m_mainToolBar->addWidget(m_angleBtn);
    
    m_mainToolBar->addSeparator();
    
    // 操作按钮
    m_mainToolBar->addAction("🔄 重置", this, &ImageInteractionWidget::resetAllTools);
    m_mainToolBar->addAction("💾 保存", this, &ImageInteractionWidget::onSaveInteractionData);
    m_mainToolBar->addAction("📤 导出", this, &ImageInteractionWidget::onExportResults);
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::createInteractionArea()
{
    m_interactionSplitter = new QSplitter(Qt::Vertical, this);
    
    // 图像显示区域
    QGroupBox* imageGroup = new QGroupBox("图像交互区域");
    QVBoxLayout* imageLayout = new QVBoxLayout(imageGroup);
    
    m_imageScene = new QGraphicsScene();
    m_imageView = new QGraphicsView(m_imageScene);
    m_imageView->setMinimumHeight(400);
    m_imageView->setStyleSheet("border: 1px solid #dee2e6; background-color: #f8f9fa;");
    
    // 添加示例内容
    QGraphicsTextItem* placeholderText = m_imageScene->addText("图像交互区域\n\n等待图像加载...\n\n支持的交互：\n• 点击选择点\n• 拖拽绘制区域\n• 右键添加标注");
    placeholderText->setPos(50, 50);
    placeholderText->setDefaultTextColor(QColor("#6c757d"));
    
    imageLayout->addWidget(m_imageView);
    
    // 坐标显示
    m_coordinateDisplay = new QTextEdit();
    m_coordinateDisplay->setMaximumHeight(100);
    m_coordinateDisplay->setPlainText("坐标信息将在此显示...");
    
    imageLayout->addWidget(new QLabel("坐标信息:"));
    imageLayout->addWidget(m_coordinateDisplay);
    
    m_interactionSplitter->addWidget(imageGroup);
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::createToolPanels()
{
    m_toolTabs = new QTabWidget();
    m_toolTabs->setTabPosition(QTabWidget::North);
    
    m_pointSelectionPanel = createPointSelectionPanel();
    m_measurementPanel = createMeasurementPanel();
    m_annotationPanel = createAnnotationPanel();
    m_roiPanel = createROIPanel();
    m_3dInteractionPanel = create3DInteractionPanel();
    m_dataManagementPanel = createDataManagementPanel();
    
    m_toolTabs->addTab(m_pointSelectionPanel, "点选工具");
    m_toolTabs->addTab(m_measurementPanel, "测量工具");
    m_toolTabs->addTab(m_annotationPanel, "标注工具");
    m_toolTabs->addTab(m_roiPanel, "ROI工具");
    m_toolTabs->addTab(m_3dInteractionPanel, "3D交互");
    m_toolTabs->addTab(m_dataManagementPanel, "数据管理");
}

//-----------------------------------------------------------------------------
QWidget* ImageInteractionWidget::createPointSelectionPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 点选工具
    QGroupBox* pointGroup = new QGroupBox("点选工具");
    QVBoxLayout* pointLayout = new QVBoxLayout(pointGroup);
    
    QPushButton* singlePointBtn = new QPushButton("单点选择");
    QPushButton* multiPointBtn = new QPushButton("多点选择");
    QPushButton* seedPointBtn = new QPushButton("种子点");
    
    pointLayout->addWidget(singlePointBtn);
    pointLayout->addWidget(multiPointBtn);
    pointLayout->addWidget(seedPointBtn);
    
    layout->addWidget(pointGroup);
    
    // 选择参数
    QGroupBox* paramGroup = new QGroupBox("选择参数");
    QFormLayout* paramLayout = new QFormLayout(paramGroup);
    
    m_sensitivitySlider = new QSlider(Qt::Horizontal);
    m_sensitivitySlider->setRange(1, 100);
    m_sensitivitySlider->setValue(50);
    
    m_toleranceSpin = new QSpinBox();
    m_toleranceSpin->setRange(1, 50);
    m_toleranceSpin->setValue(5);
    
    m_snapToEdgeCheck = new QCheckBox("边缘吸附");
    m_realTimeUpdateCheck = new QCheckBox("实时更新");
    m_realTimeUpdateCheck->setChecked(true);
    
    paramLayout->addRow("灵敏度:", m_sensitivitySlider);
    paramLayout->addRow("容差 (像素):", m_toleranceSpin);
    paramLayout->addRow("", m_snapToEdgeCheck);
    paramLayout->addRow("", m_realTimeUpdateCheck);
    
    layout->addWidget(paramGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
QWidget* ImageInteractionWidget::createMeasurementPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 测量工具
    QGroupBox* measureGroup = new QGroupBox("测量工具");
    QGridLayout* measureLayout = new QGridLayout(measureGroup);
    
    QPushButton* distanceBtn = new QPushButton("距离测量");
    QPushButton* angleBtn = new QPushButton("角度测量");
    QPushButton* areaBtn = new QPushButton("面积测量");
    QPushButton* volumeBtn = new QPushButton("体积测量");
    
    measureLayout->addWidget(distanceBtn, 0, 0);
    measureLayout->addWidget(angleBtn, 0, 1);
    measureLayout->addWidget(areaBtn, 1, 0);
    measureLayout->addWidget(volumeBtn, 1, 1);
    
    layout->addWidget(measureGroup);
    
    // 测量结果
    QGroupBox* resultGroup = new QGroupBox("测量结果");
    QVBoxLayout* resultLayout = new QVBoxLayout(resultGroup);
    
    m_resultTable = new QTableWidget(0, 3);
    QStringList headers;
    headers << "类型" << "数值" << "单位";
    m_resultTable->setHorizontalHeaderLabels(headers);
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    
    resultLayout->addWidget(m_resultTable);
    
    QPushButton* clearResultsBtn = new QPushButton("清除结果");
    resultLayout->addWidget(clearResultsBtn);
    
    layout->addWidget(resultGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
QWidget* ImageInteractionWidget::createAnnotationPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 标注工具
    QGroupBox* annotationGroup = new QGroupBox("标注工具");
    QGridLayout* annotationLayout = new QGridLayout(annotationGroup);
    
    QPushButton* textBtn = new QPushButton("文本标注");
    QPushButton* arrowBtn = new QPushButton("箭头标注");
    QPushButton* shapeBtn = new QPushButton("形状标注");
    QPushButton* colorBtn = new QPushButton("颜色标记");
    
    annotationLayout->addWidget(textBtn, 0, 0);
    annotationLayout->addWidget(arrowBtn, 0, 1);
    annotationLayout->addWidget(shapeBtn, 1, 0);
    annotationLayout->addWidget(colorBtn, 1, 1);
    
    layout->addWidget(annotationGroup);
    
    // 标注属性
    QGroupBox* attrGroup = new QGroupBox("标注属性");
    QFormLayout* attrLayout = new QFormLayout(attrGroup);
    
    QComboBox* colorCombo = new QComboBox();
    colorCombo->addItems({"红色", "绿色", "蓝色", "黄色", "紫色"});
    
    QSpinBox* fontSizeSpin = new QSpinBox();
    fontSizeSpin->setRange(8, 72);
    fontSizeSpin->setValue(12);
    
    attrLayout->addRow("颜色:", colorCombo);
    attrLayout->addRow("字体大小:", fontSizeSpin);
    
    layout->addWidget(attrGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
QWidget* ImageInteractionWidget::createROIPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // ROI工具
    QGroupBox* roiGroup = new QGroupBox("ROI绘制工具");
    QGridLayout* roiLayout = new QGridLayout(roiGroup);
    
    QPushButton* rectBtn = new QPushButton("矩形ROI");
    QPushButton* circleBtn = new QPushButton("圆形ROI");
    QPushButton* polygonBtn = new QPushButton("多边形ROI");
    QPushButton* freehandBtn = new QPushButton("自由绘制");
    
    roiLayout->addWidget(rectBtn, 0, 0);
    roiLayout->addWidget(circleBtn, 0, 1);
    roiLayout->addWidget(polygonBtn, 1, 0);
    roiLayout->addWidget(freehandBtn, 1, 1);
    
    layout->addWidget(roiGroup);
    
    // ROI列表
    QGroupBox* listGroup = new QGroupBox("ROI列表");
    QVBoxLayout* listLayout = new QVBoxLayout(listGroup);
    
    QListWidget* roiList = new QListWidget();
    roiList->addItem("ROI_1 - 矩形 (100x50)");
    roiList->addItem("ROI_2 - 圆形 (半径30)");
    
    listLayout->addWidget(roiList);
    
    QHBoxLayout* roiBtnLayout = new QHBoxLayout();
    QPushButton* editBtn = new QPushButton("编辑");
    QPushButton* deleteBtn = new QPushButton("删除");
    QPushButton* exportBtn = new QPushButton("导出");
    
    roiBtnLayout->addWidget(editBtn);
    roiBtnLayout->addWidget(deleteBtn);
    roiBtnLayout->addWidget(exportBtn);
    
    listLayout->addLayout(roiBtnLayout);
    
    layout->addWidget(listGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
QWidget* ImageInteractionWidget::create3DInteractionPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 3D工具
    QGroupBox* tools3DGroup = new QGroupBox("3D交互工具");
    QVBoxLayout* tools3DLayout = new QVBoxLayout(tools3DGroup);
    
    QPushButton* point3DBtn = new QPushButton("3D点选择");
    QPushButton* region3DBtn = new QPushButton("3D区域生长");
    QPushButton* cross3DBtn = new QPushButton("3D截面");
    QPushButton* volume3DBtn = new QPushButton("体积交互");
    
    tools3DLayout->addWidget(point3DBtn);
    tools3DLayout->addWidget(region3DBtn);
    tools3DLayout->addWidget(cross3DBtn);
    tools3DLayout->addWidget(volume3DBtn);
    
    layout->addWidget(tools3DGroup);
    
    // 3D参数
    QGroupBox* param3DGroup = new QGroupBox("3D参数");
    QFormLayout* param3DLayout = new QFormLayout(param3DGroup);
    
    QSlider* threshold3DSlider = new QSlider(Qt::Horizontal);
    threshold3DSlider->setRange(0, 255);
    threshold3DSlider->setValue(127);
    
    QSpinBox* radius3DSpin = new QSpinBox();
    radius3DSpin->setRange(1, 50);
    radius3DSpin->setValue(5);
    
    param3DLayout->addRow("3D阈值:", threshold3DSlider);
    param3DLayout->addRow("作用半径:", radius3DSpin);
    
    layout->addWidget(param3DGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
QWidget* ImageInteractionWidget::createDataManagementPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 数据操作
    QGroupBox* dataGroup = new QGroupBox("数据管理");
    QVBoxLayout* dataLayout = new QVBoxLayout(dataGroup);
    
    QPushButton* saveBtn = new QPushButton("保存交互数据");
    QPushButton* loadBtn = new QPushButton("加载交互数据");
    QPushButton* exportBtn = new QPushButton("导出结果");
    QPushButton* clearBtn = new QPushButton("清除所有数据");
    
    dataLayout->addWidget(saveBtn);
    dataLayout->addWidget(loadBtn);
    dataLayout->addWidget(exportBtn);
    dataLayout->addWidget(clearBtn);
    
    layout->addWidget(dataGroup);
    
    // 统计信息
    QGroupBox* statsGroup = new QGroupBox("统计信息");
    QFormLayout* statsLayout = new QFormLayout(statsGroup);
    
    QLabel* pointCountLabel = new QLabel("0");
    QLabel* roiCountLabel = new QLabel("0");
    QLabel* annotationCountLabel = new QLabel("0");
    
    statsLayout->addRow("选择点数:", pointCountLabel);
    statsLayout->addRow("ROI数量:", roiCountLabel);
    statsLayout->addRow("标注数量:", annotationCountLabel);
    
    layout->addWidget(statsGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::setupStyles()
{
    setStyleSheet(R"(
        QWidget {
            background-color: #f8f9fa;
            font-family: "Microsoft YaHei", Arial, sans-serif;
        }
        
        QPushButton {
            background-color: #007bff;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 4px;
            font-weight: 500;
            min-width: 80px;
        }
        
        QPushButton:hover {
            background-color: #0056b3;
        }
        
        QPushButton:checked {
            background-color: #28a745;
        }
        
        QGroupBox {
            font-weight: bold;
            border: 2px solid #dee2e6;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 15px;
            background-color: white;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 8px 0 8px;
            color: #495057;
        }
        
        QTabWidget::pane {
            border: 1px solid #dee2e6;
            border-radius: 6px;
            background-color: white;
        }
        
        QTabBar::tab {
            background-color: #e9ecef;
            color: #495057;
            padding: 8px 16px;
            margin: 2px;
            border-radius: 4px;
            font-weight: 500;
        }
        
        QTabBar::tab:selected {
            background-color: #007bff;
            color: white;
        }
        
        QTableWidget, QListWidget, QTextEdit {
            border: 1px solid #dee2e6;
            border-radius: 4px;
            background-color: white;
        }
        
        QComboBox, QSpinBox, QSlider {
            min-width: 100px;
        }
        
        QToolBar {
            background-color: #ffffff;
            border: 1px solid #dee2e6;
            border-radius: 6px;
            padding: 4px;
        }
    )");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::connectSignals()
{
    // 主控制信号
    connect(m_interactionModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageInteractionWidget::onInteractionModeChanged);
    connect(m_imageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageInteractionWidget::onImageSelectionChanged);
    
    // 工具按钮信号
    connect(m_pointToolBtn, &QPushButton::clicked, this, &ImageInteractionWidget::onPointSelectionTool);
    connect(m_regionToolBtn, &QPushButton::clicked, this, &ImageInteractionWidget::onRegionSelectionTool);
    connect(m_distanceBtn, &QPushButton::clicked, this, &ImageInteractionWidget::onDistanceMeasurement);
    connect(m_angleBtn, &QPushButton::clicked, this, &ImageInteractionWidget::onAngleMeasurement);
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::initializeServiceConnections()
{
    if (!m_pluginContext) {
        qWarning() << "[ImageInteractionWidget] CTK插件上下文未设置";
        return;
    }
    
    // 获取交互服务
    ctkServiceReference interactionServiceRef = m_pluginContext->getServiceReference<ImageInteractionService>();
    if (interactionServiceRef) {
        m_interactionService = qobject_cast<ImageInteractionService*>(
            m_pluginContext->getService(interactionServiceRef));
        
        if (m_interactionService) {
            qDebug() << "[ImageInteractionWidget] ImageInteractionService连接成功";
            onInteractionServiceAvailable();
        }
    }
    
    // 获取图像服务
    ctkServiceReference imageServiceRef = m_pluginContext->getServiceReference<UnifiedMedicalImageService>();
    if (imageServiceRef) {
        m_imageService = qobject_cast<UnifiedMedicalImageService*>(
            m_pluginContext->getService(imageServiceRef));
        
        if (m_imageService) {
            qDebug() << "[ImageInteractionWidget] UnifiedMedicalImageService连接成功";
            onImageServiceAvailable();
        }
    }
    
    m_serviceConnected = (m_interactionService != nullptr && m_imageService != nullptr);
    
    if (m_serviceConnected) {
        updateStatus("服务连接成功，交互工具已就绪");
        refreshImageList();
    } else {
        updateStatus("等待服务连接...");
    }
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onInteractionModeChanged()
{
    QString mode = m_interactionModeCombo->currentData().toString();
    setInteractionMode(mode);
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onImageSelectionChanged()
{
    QString imageId = m_imageSelector->currentData().toString();
    if (!imageId.isEmpty()) {
        loadImageForInteraction(imageId);
    }
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onToolSelectionChanged()
{
    // 工具选择变化处理
    updateStatus("工具选择已更改");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onPointSelectionTool()
{
    m_currentTool = "Point";
    updateStatus("已选择点选工具");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onRegionSelectionTool()
{
    m_currentTool = "Region";
    updateStatus("已选择区域选择工具");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onSeedPointTool()
{
    m_currentTool = "Seed";
    updateStatus("已选择种子点工具");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onDistanceMeasurement()
{
    m_currentTool = "Distance";
    updateStatus("已选择距离测量工具");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onAngleMeasurement()
{
    m_currentTool = "Angle";
    updateStatus("已选择角度测量工具");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onAreaMeasurement()
{
    m_currentTool = "Area";
    updateStatus("已选择面积测量工具");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onVolumeMeasurement()
{
    m_currentTool = "Volume";
    updateStatus("已选择体积测量工具");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onTextAnnotation()
{
    updateStatus("添加文本标注");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onArrowAnnotation()
{
    updateStatus("添加箭头标注");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onShapeAnnotation()
{
    updateStatus("添加形状标注");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onRectangleROI()
{
    updateStatus("绘制矩形ROI");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onCircleROI()
{
    updateStatus("绘制圆形ROI");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onPolygonROI()
{
    updateStatus("绘制多边形ROI");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onFreehandROI()
{
    updateStatus("自由绘制ROI");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::on3DPointSelection()
{
    updateStatus("3D点选择模式");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::on3DRegionGrowing()
{
    updateStatus("3D区域生长模式");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::on3DCrossSection()
{
    updateStatus("3D截面模式");
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onSaveInteractionData()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "保存交互数据", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "Interaction Data (*.json);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        updateStatus(QString("保存交互数据到: %1").arg(fileName));
        // TODO: 实际保存逻辑
    }
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onExportResults()
{
    QMessageBox::information(this, "导出结果", 
        QString("交互结果导出功能\n\n将导出：\n- 选择的点坐标\n- 测量结果\n- ROI数据\n- 标注信息"));
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onClearAll()
{
    resetAllTools();
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onInteractionServiceAvailable()
{
    if (m_interactionService) {
        updateStatus("图像交互服务已连接");
    }
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::onImageServiceAvailable()
{
    if (m_imageService) {
        updateStatus("医学图像核心服务已连接");
    }
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::updateStatus(const QString& message)
{
    m_statusLabel->setText(message);
    
    if (message.contains("错误") || message.contains("失败")) {
        m_statusLabel->setStyleSheet("QLabel { color: #dc3545; font-weight: bold; }");
    } else if (message.contains("成功") || message.contains("完成")) {
        m_statusLabel->setStyleSheet("QLabel { color: #28a745; font-weight: bold; }");
    } else {
        m_statusLabel->setStyleSheet("QLabel { color: #007bff; font-weight: bold; }");
    }
    
    qDebug() << "[ImageInteractionWidget]" << message;
}

//-----------------------------------------------------------------------------
void ImageInteractionWidget::updateProgress(int value)
{
    if (value >= 0 && value <= 100) {
        m_progressBar->setValue(value);
        m_progressBar->setVisible(true);
        
        if (value == 100) {
            QTimer::singleShot(2000, [this]() {
                m_progressBar->setVisible(false);
            });
        }
    } else {
        m_progressBar->setVisible(false);
    }
}
