#include "MedicalProcessingWidget.h"
#include "MedicalProcessingService.h"
#include "ServiceInterfaces.h"
#include "../MedicalImageCore/MedicalImageCoreService.h"

#include <ctkPluginContext.h>
#include <ctkServiceReference.h>
#include <service/event/ctkEventAdmin.h>
#include <service/event/ctkEvent.h>
#include <ctkDictionary.h>

#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QSplitter>
#include <QDebug>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QUuid>
#include <QTime>

//-----------------------------------------------------------------------------
MedicalProcessingWidget::MedicalProcessingWidget(QWidget* parent)
    : QWidget(parent)
    , m_pluginContext(nullptr)
    , m_processingService(nullptr)
    , m_imageService(nullptr)
    , m_serviceConnected(false)
    , m_processingThread(nullptr)
    , m_processingWorker(nullptr)
    , m_eventAdmin(nullptr)
    , m_mainLayout(nullptr)
    , m_contentLayout(nullptr)
    , m_mainSplitter(nullptr)
    , m_verticalSplitter(nullptr)
    , m_mainToolBar(nullptr)
    , m_previewSplitter(nullptr)
    , m_originalImageView(nullptr)
    , m_processedImageView(nullptr)
    , m_originalImageScene(nullptr)
    , m_processedImageScene(nullptr)
    , m_parameterTabs(nullptr)
    , m_currentProcessingType("BasicFiltering")
{
    qDebug() << "[MedicalProcessingWidget] 创建医学图像处理界面";
    
    // 设置窗口属性
    setWindowTitle("医学图像处理工作台");
    setMinimumSize(1200, 800);
    
    // 初始化UI
    initializeUI();
    
    // 设置样式
    setupStyles();
    
    // 连接信号槽
    connectSignals();

    // 初始化工作线程
    initializeProcessingThread();

    qDebug() << "[MedicalProcessingWidget] 医学图像处理界面创建完成";
}

//-----------------------------------------------------------------------------
MedicalProcessingWidget::~MedicalProcessingWidget()
{
    qDebug() << "[MedicalProcessingWidget] 销毁医学图像处理界面";

    // 清理工作线程
    if (m_processingThread) {
        m_processingThread->quit();
        m_processingThread->wait(3000); // 等待3秒
        if (m_processingThread->isRunning()) {
            m_processingThread->terminate();
            m_processingThread->wait(1000);
        }
        delete m_processingThread;
        m_processingThread = nullptr;
    }

    if (m_processingWorker) {
        delete m_processingWorker;
        m_processingWorker = nullptr;
    }
}

//-----------------------------------------------------------------------------
QObject* MedicalProcessingWidget::getImageCoreService() const
{
    return m_imageService;
}



//-----------------------------------------------------------------------------
void MedicalProcessingWidget::setPluginContext(ctkPluginContext* context)
{
    m_pluginContext = context;
    qDebug() << "[MedicalProcessingWidget] 设置CTK插件上下文";

    // 初始化EventAdmin服务
    initializeEventAdmin();

    // 启动图像列表轮询
    setupImageListPolling();

    // 初始化服务连接
    initializeServiceConnections();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::loadImageForProcessing(const QString& imageId)
{
    if (imageId.isEmpty()) {
        updateStatus("无效的图像ID");
        return;
    }
    
    m_currentImageId = imageId;
    
    // 更新图像选择器
    for (int i = 0; i < m_imageSelector->count(); ++i) {
        if (m_imageSelector->itemData(i).toString() == imageId) {
            m_imageSelector->setCurrentIndex(i);
            break;
        }
    }
    
    updateStatus(QString("已加载图像: %1").arg(imageId));
    updatePreviewImages();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::setProcessingType(const QString& processingType)
{
    if (m_currentProcessingType == processingType) {
        return;
    }
    
    m_currentProcessingType = processingType;
    
    // 更新处理类型选择器
    for (int i = 0; i < m_processingTypeCombo->count(); ++i) {
        if (m_processingTypeCombo->itemData(i).toString() == processingType) {
            m_processingTypeCombo->setCurrentIndex(i);
            break;
        }
    }
    
    // 切换到相应的参数面板
    if (processingType == "BasicFiltering") {
        m_parameterTabs->setCurrentWidget(m_basicProcessingPanel);
    } else if (processingType == "Segmentation") {
        m_parameterTabs->setCurrentWidget(m_segmentationPanel);
    } else if (processingType == "Registration") {
        m_parameterTabs->setCurrentWidget(m_registrationPanel);
    } else if (processingType == "Reconstruction") {
        m_parameterTabs->setCurrentWidget(m_reconstructionPanel);
    } else if (processingType == "BatchProcessing") {
        m_parameterTabs->setCurrentWidget(m_batchProcessingPanel);
    }
    
    updateStatus(QString("切换到处理类型: %1").arg(processingType));
    updateParameterDisplay();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::refreshImageList()
{
    updateStatus("刷新图像列表...");

    QStringList imageIds;

    // 首先尝试直接从MedicalImageCore服务获取图像列表
    if (m_imageService) {
        qDebug() << "[MedicalProcessingWidget] 直接从MedicalImageCore服务获取图像列表";
        imageIds = m_imageService->getLoadedImages();
        qDebug() << "[MedicalProcessingWidget] 从服务获取到图像列表:" << imageIds << "数量:" << imageIds.size();
    }

    // 如果直接服务调用失败，尝试使用EventAdmin请求
    if (imageIds.isEmpty() && m_eventAdmin) {
        qDebug() << "[MedicalProcessingWidget] 服务调用未获取到图像，尝试使用EventAdmin";
        ctkDictionary props;
        props["requestId"] = QUuid::createUuid().toString();
        props["requestType"] = "getImageList";

        ctkEvent requestEvent("medical/image/list_request", props);
        m_eventAdmin->sendEvent(requestEvent);

        qDebug() << "[MedicalProcessingWidget] 已发送图像列表请求事件";
        updateStatus("已发送图像列表请求，等待响应...");
        return;
    }

    // 如果EventAdmin也不可用，记录日志
    if (imageIds.isEmpty()) {
        qDebug() << "[MedicalProcessingWidget] 无法获取图像列表";
        updateStatus("暂无可用图像，请先加载图像");
    }

    // 如果两种方法都不可用，使用演示数据
    if (imageIds.isEmpty()) {
        qWarning() << "[MedicalProcessingWidget] 无法获取图像列表，使用演示数据";
        imageIds << "demo_image_1" << "demo_image_2" << "demo_image_3";
        qDebug() << "[MedicalProcessingWidget] 使用演示图像列表:" << imageIds << "数量:" << imageIds.size();
    }
        
        m_imageSelector->clear();
        m_imageSelector->addItem("请选择图像...", "");
        
        m_referenceImageCombo->clear();
        m_referenceImageCombo->addItem("请选择参考图像...", "");
        
        // 简化处理，直接使用所有图像ID（在实际应用中，验证逻辑应该在图像服务中处理）
        QStringList validImageIds;
        for (const QString& imageId : imageIds) {
            if (!imageId.isEmpty()) {
                validImageIds.append(imageId);
                qDebug() << "[MedicalProcessingWidget] 添加图像ID:" << imageId;
            }
        }
        
        for (const QString& imageId : validImageIds) {
            // 简化显示名称（在实际应用中，详细信息应该通过事件获取）
            QString displayName = QString("图像: %1").arg(imageId);

            m_imageSelector->addItem(displayName, imageId);
            m_referenceImageCombo->addItem(displayName, imageId);
        }

        updateStatus(QString("发现 %1 个可处理的图像").arg(validImageIds.size()));

    // 注意：在实际应用中，这里应该等待图像服务的响应事件
    // 当前为了演示目的，直接处理静态数据
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::resetAllParameters()
{
    updateStatus("重置所有处理参数...");
    
    // 重置基础处理参数
    m_filterStrengthSlider->setValue(50);
    m_brightnessSlider->setValue(50);
    m_contrastSlider->setValue(50);
    m_gammaSlider->setValue(50);
    m_noiseReductionSlider->setValue(30);
    m_noiseReductionCheck->setChecked(false);
    
    // 重置分割参数
    m_thresholdSlider->setValue(127);
    m_thresholdSpinBox->setValue(127);
    m_seedPointXSpin->setValue(0);
    m_seedPointYSpin->setValue(0);
    m_regionGrowingToleranceSlider->setValue(10);
    m_morphologyCheck->setChecked(false);
    
    // 重置配准参数
    m_registrationAccuracySlider->setValue(70);
    m_automaticRegistrationCheck->setChecked(true);
    
    // 重置3D重建参数
    m_isosurfaceValueSlider->setValue(50);
    m_smoothingSlider->setValue(30);
    m_decimationCheck->setChecked(false);
    m_decimationRatioSlider->setValue(50);
    
    updateStatus("所有参数已重置为默认值");
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::applyProcessing()
{
    if (!validateParameters()) {
        return;
    }

    if (!m_processingService || m_currentImageId.isEmpty()) {
        QMessageBox::warning(this, "处理失败", "服务不可用或未选择图像");
        return;
    }

    // 检查工作线程是否正在运行
    if (m_processingThread && m_processingThread->isRunning()) {
        QMessageBox::information(this, "处理中", "图像处理正在进行中，请等待完成");
        return;
    }

    // 生成操作ID
    m_currentOperationId = QUuid::createUuid().toString();

    // 创建参数映射
    QVariantMap parameters = createParameterMap();

    updateStatus("正在启动图像处理...");
    updateProgress(0);

    // 禁用处理按钮
    if (m_applyAction) {
        m_applyAction->setEnabled(false);
    }

    try {
        // 设置工作线程参数
        if (m_processingWorker) {
            m_processingWorker->setProcessingParameters(
                m_currentImageId,
                m_currentProcessingType,
                parameters,
                m_processingService
            );

            // 启动工作线程
            if (m_processingThread) {
                m_processingThread->start();
                updateStatus("图像处理已在后台启动...");
                qDebug() << "[MedicalProcessingWidget] ✅ 图像处理线程已启动";
            }
        }

    } catch (const std::exception& e) {
        QMessageBox::critical(this, "处理失败", QString("启动图像处理失败: %1").arg(e.what()));
        updateStatus("图像处理启动失败");

        // 重新启用处理按钮
        if (m_applyAction) {
            m_applyAction->setEnabled(true);
        }
    } catch (...) {
        QMessageBox::critical(this, "处理失败", "启动图像处理时发生未知错误");
        updateStatus("图像处理启动失败");

        // 重新启用处理按钮
        if (m_applyAction) {
            m_applyAction->setEnabled(true);
        }
    }

    // 记录操作日志
    if (m_logTextEdit) {
        m_logTextEdit->append(QString("[%1] 开始处理: %2 (操作ID: %3)")
                             .arg(QTime::currentTime().toString())
                             .arg(m_currentProcessingType)
                             .arg(m_currentOperationId));
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::previewProcessing()
{
    if (!validateParameters()) {
        return;
    }
    
    updateStatus("生成处理预览...");
    
    // 创建预览参数（降低质量以提高速度）
    QVariantMap parameters = createParameterMap();
    parameters["previewMode"] = true;
    parameters["quality"] = "low";
    
    // 模拟预览过程
    QTimer::singleShot(1000, [this]() {
        updateStatus("预览已生成");
        // TODO: 实际的预览逻辑
    });
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::initializeUI()
{
    // 创建主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(6);
    
    // 创建工具栏
    createMainToolBar();
    m_mainLayout->addWidget(m_mainToolBar);
    
    // 创建处理选择区域
    createProcessingSelectionArea();
    
    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 创建图像预览区域和参数控制面板
    createImagePreviewArea();
    createParameterControlPanels();
    
    // 设置分割器
    m_mainSplitter->addWidget(m_previewSplitter);
    m_mainSplitter->addWidget(m_parameterTabs);
    m_mainSplitter->setStretchFactor(0, 2);  // 图像预览占2/3
    m_mainSplitter->setStretchFactor(1, 1);  // 参数面板占1/3
    
    m_mainLayout->addWidget(m_mainSplitter);
    
    // 创建状态栏
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
void MedicalProcessingWidget::createMainToolBar()
{
    m_mainToolBar = new QToolBar("主工具栏", this);
    m_mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    
    // 操作按钮
    m_previewAction = m_mainToolBar->addAction("👀 预览", this, &MedicalProcessingWidget::previewProcessing);
    m_applyAction = m_mainToolBar->addAction("▶️ 应用处理", this, &MedicalProcessingWidget::applyProcessing);
    m_resetAction = m_mainToolBar->addAction("🔄 重置参数", this, &MedicalProcessingWidget::resetAllParameters);
    
    m_mainToolBar->addSeparator();
    
    m_saveAction = m_mainToolBar->addAction("💾 保存结果", this, &MedicalProcessingWidget::onSaveResult);
    m_exportAction = m_mainToolBar->addAction("📤 导出数据", this, &MedicalProcessingWidget::onExportResult);
    
    m_mainToolBar->addSeparator();
    
    m_settingsAction = m_mainToolBar->addAction("⚙️ 设置", [this]() {
        QMessageBox::information(this, "处理设置", "图像处理设置界面\n\n配置选项：\n- 处理算法参数\n- 性能设置\n- 输出格式");
    });
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::createProcessingSelectionArea()
{
    QGroupBox* selectionGroup = new QGroupBox("处理选择");
    QHBoxLayout* selectionLayout = new QHBoxLayout(selectionGroup);
    
    // 处理类型选择
    selectionLayout->addWidget(new QLabel("处理类型:"));
    m_processingTypeCombo = new QComboBox();
    m_processingTypeCombo->addItem("基础滤波", "BasicFiltering");
    m_processingTypeCombo->addItem("图像分割", "Segmentation");
    m_processingTypeCombo->addItem("图像配准", "Registration");
    m_processingTypeCombo->addItem("3D重建", "Reconstruction");
    m_processingTypeCombo->addItem("批量处理", "BatchProcessing");
    selectionLayout->addWidget(m_processingTypeCombo);
    
    // 添加垂直分隔线
    QFrame* separator1 = new QFrame();
    separator1->setFrameShape(QFrame::VLine);
    separator1->setFrameShadow(QFrame::Sunken);
    selectionLayout->addWidget(separator1);
    
    // 图像选择
    selectionLayout->addWidget(new QLabel("目标图像:"));
    m_imageSelector = new QComboBox();
    m_imageSelector->setMinimumWidth(200);
    m_imageSelector->addItem("请选择图像...", "");
    selectionLayout->addWidget(m_imageSelector);
    
    // 添加垂直分隔线
    QFrame* separator2 = new QFrame();
    separator2->setFrameShape(QFrame::VLine);
    separator2->setFrameShadow(QFrame::Sunken);
    selectionLayout->addWidget(separator2);
    
    // 预设选择
    selectionLayout->addWidget(new QLabel("参数预设:"));
    m_presetCombo = new QComboBox();
    m_presetCombo->addItems({"默认", "快速处理", "高质量", "自定义"});
    selectionLayout->addWidget(m_presetCombo);
    
    // 刷新按钮
    m_refreshBtn = new QPushButton("🔄 刷新");
    m_refreshBtn->setToolTip("刷新可用图像列表");
    selectionLayout->addWidget(m_refreshBtn);
    
    selectionLayout->addStretch();
    
    m_mainLayout->addWidget(selectionGroup);
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::createImagePreviewArea()
{
    m_previewSplitter = new QSplitter(Qt::Vertical, this);
    
    // 原始图像预览
    QGroupBox* originalGroup = new QGroupBox("原始图像");
    QVBoxLayout* originalLayout = new QVBoxLayout(originalGroup);
    
    m_originalImageScene = new QGraphicsScene();
    m_originalImageView = new QGraphicsView(m_originalImageScene);
    m_originalImageView->setMinimumHeight(200);
    m_originalImageView->setStyleSheet("border: 1px solid #dee2e6; background-color: #f8f9fa;");
    
    m_originalImageLabel = new QLabel("等待图像加载...");
    m_originalImageLabel->setAlignment(Qt::AlignCenter);
    m_originalImageLabel->setStyleSheet("color: #6c757d; font-style: italic;");
    
    originalLayout->addWidget(m_originalImageView);
    originalLayout->addWidget(m_originalImageLabel);
    
    // 处理结果预览
    QGroupBox* processedGroup = new QGroupBox("处理结果");
    QVBoxLayout* processedLayout = new QVBoxLayout(processedGroup);
    
    m_processedImageScene = new QGraphicsScene();
    m_processedImageView = new QGraphicsView(m_processedImageScene);
    m_processedImageView->setMinimumHeight(200);
    m_processedImageView->setStyleSheet("border: 1px solid #dee2e6; background-color: #f8f9fa;");
    
    m_processedImageLabel = new QLabel("等待处理结果...");
    m_processedImageLabel->setAlignment(Qt::AlignCenter);
    m_processedImageLabel->setStyleSheet("color: #6c757d; font-style: italic;");
    
    processedLayout->addWidget(m_processedImageView);
    processedLayout->addWidget(m_processedImageLabel);
    
    m_previewSplitter->addWidget(originalGroup);
    m_previewSplitter->addWidget(processedGroup);
    m_previewSplitter->setStretchFactor(0, 1);
    m_previewSplitter->setStretchFactor(1, 1);
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::createParameterControlPanels()
{
    m_parameterTabs = new QTabWidget();
    m_parameterTabs->setTabPosition(QTabWidget::North);
    
    // 创建各种参数控制面板
    m_basicProcessingPanel = createBasicProcessingPanel();
    m_segmentationPanel = createSegmentationPanel();
    m_registrationPanel = createRegistrationPanel();
    m_reconstructionPanel = createReconstructionPanel();
    m_batchProcessingPanel = createBatchProcessingPanel();
    m_resultManagementPanel = createResultManagementPanel();
    
    m_parameterTabs->addTab(m_basicProcessingPanel, "基础处理");
    m_parameterTabs->addTab(m_segmentationPanel, "图像分割");
    m_parameterTabs->addTab(m_registrationPanel, "图像配准");
    m_parameterTabs->addTab(m_reconstructionPanel, "3D重建");
    m_parameterTabs->addTab(m_batchProcessingPanel, "批量处理");
    m_parameterTabs->addTab(m_resultManagementPanel, "结果管理");
    
    // 添加日志面板
    QScrollArea* logScrollArea = new QScrollArea();
    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setMaximumHeight(150);
    m_logTextEdit->append("医学图像处理工作台已启动");
    logScrollArea->setWidget(m_logTextEdit);
    logScrollArea->setWidgetResizable(true);
    
    m_parameterTabs->addTab(logScrollArea, "操作日志");
}

//-----------------------------------------------------------------------------
QWidget* MedicalProcessingWidget::createBasicProcessingPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 滤波设置
    QGroupBox* filterGroup = new QGroupBox("滤波设置");
    QFormLayout* filterLayout = new QFormLayout(filterGroup);
    
    m_filterTypeCombo = new QComboBox();
    m_filterTypeCombo->addItems({"高斯滤波", "均值滤波", "中值滤波", "双边滤波", "边缘保持"});
    
    m_filterStrengthSlider = new QSlider(Qt::Horizontal);
    m_filterStrengthSlider->setRange(1, 100);
    m_filterStrengthSlider->setValue(50);
    
    filterLayout->addRow("滤波类型:", m_filterTypeCombo);
    filterLayout->addRow("滤波强度:", m_filterStrengthSlider);
    
    layout->addWidget(filterGroup);
    
    // 图像增强
    QGroupBox* enhanceGroup = new QGroupBox("图像增强");
    QFormLayout* enhanceLayout = new QFormLayout(enhanceGroup);
    
    m_brightnessSlider = new QSlider(Qt::Horizontal);
    m_brightnessSlider->setRange(0, 100);
    m_brightnessSlider->setValue(50);
    
    m_contrastSlider = new QSlider(Qt::Horizontal);
    m_contrastSlider->setRange(0, 100);
    m_contrastSlider->setValue(50);
    
    m_gammaSlider = new QSlider(Qt::Horizontal);
    m_gammaSlider->setRange(10, 300);
    m_gammaSlider->setValue(100);
    
    enhanceLayout->addRow("亮度调整:", m_brightnessSlider);
    enhanceLayout->addRow("对比度:", m_contrastSlider);
    enhanceLayout->addRow("伽马校正:", m_gammaSlider);
    
    layout->addWidget(enhanceGroup);
    
    // 噪声处理
    QGroupBox* noiseGroup = new QGroupBox("噪声处理");
    QVBoxLayout* noiseLayout = new QVBoxLayout(noiseGroup);
    
    m_noiseReductionCheck = new QCheckBox("启用噪声抑制");
    m_noiseReductionSlider = new QSlider(Qt::Horizontal);
    m_noiseReductionSlider->setRange(1, 100);
    m_noiseReductionSlider->setValue(30);
    m_noiseReductionSlider->setEnabled(false);
    
    noiseLayout->addWidget(m_noiseReductionCheck);
    noiseLayout->addWidget(new QLabel("降噪强度:"));
    noiseLayout->addWidget(m_noiseReductionSlider);
    
    layout->addWidget(noiseGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
QWidget* MedicalProcessingWidget::createSegmentationPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 分割方法
    QGroupBox* methodGroup = new QGroupBox("分割方法");
    QFormLayout* methodLayout = new QFormLayout(methodGroup);
    
    m_segmentationMethodCombo = new QComboBox();
    m_segmentationMethodCombo->addItems({"阈值分割", "区域生长", "边缘检测", "聚类分割", "深度学习"});
    
    methodLayout->addRow("分割算法:", m_segmentationMethodCombo);
    
    layout->addWidget(methodGroup);
    
    // 阈值分割参数
    QGroupBox* thresholdGroup = new QGroupBox("阈值参数");
    QFormLayout* thresholdLayout = new QFormLayout(thresholdGroup);

    // 阈值滑块 - 扩大范围到1200
    m_thresholdSlider = new QSlider(Qt::Horizontal);
    m_thresholdSlider->setRange(0, 1200);
    m_thresholdSlider->setValue(127);

    // 阈值输入框 - 方便精确输入
    m_thresholdSpinBox = new QSpinBox();
    m_thresholdSpinBox->setRange(0, 1200);
    m_thresholdSpinBox->setValue(127);
    m_thresholdSpinBox->setSuffix(" HU");  // 添加单位后缀
    m_thresholdSpinBox->setToolTip("输入阈值（0-1200 HU）");

    // 创建水平布局来放置滑块和输入框
    QWidget* thresholdWidget = new QWidget();
    QHBoxLayout* thresholdHLayout = new QHBoxLayout(thresholdWidget);
    thresholdHLayout->setContentsMargins(0, 0, 0, 0);
    thresholdHLayout->addWidget(m_thresholdSlider, 3);  // 滑块占3份
    thresholdHLayout->addWidget(m_thresholdSpinBox, 1); // 输入框占1份

    thresholdLayout->addRow("阈值:", thresholdWidget);
    
    layout->addWidget(thresholdGroup);
    
    // 区域生长参数
    QGroupBox* regionGroup = new QGroupBox("区域生长参数");
    QFormLayout* regionLayout = new QFormLayout(regionGroup);
    
    QHBoxLayout* seedLayout = new QHBoxLayout();
    m_seedPointXSpin = new QSpinBox();
    m_seedPointXSpin->setRange(0, 1000);
    m_seedPointYSpin = new QSpinBox();
    m_seedPointYSpin->setRange(0, 1000);
    seedLayout->addWidget(m_seedPointXSpin);
    seedLayout->addWidget(new QLabel(","));
    seedLayout->addWidget(m_seedPointYSpin);
    
    m_regionGrowingToleranceSlider = new QSlider(Qt::Horizontal);
    m_regionGrowingToleranceSlider->setRange(1, 50);
    m_regionGrowingToleranceSlider->setValue(10);
    
    regionLayout->addRow("种子点 (x,y):", seedLayout);
    regionLayout->addRow("生长容差:", m_regionGrowingToleranceSlider);
    
    layout->addWidget(regionGroup);
    
    // 后处理
    QGroupBox* postGroup = new QGroupBox("后处理");
    QVBoxLayout* postLayout = new QVBoxLayout(postGroup);
    
    m_morphologyCheck = new QCheckBox("形态学后处理");
    postLayout->addWidget(m_morphologyCheck);
    
    layout->addWidget(postGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
QWidget* MedicalProcessingWidget::createRegistrationPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 配准类型
    QGroupBox* typeGroup = new QGroupBox("配准设置");
    QFormLayout* typeLayout = new QFormLayout(typeGroup);
    
    m_registrationTypeCombo = new QComboBox();
    m_registrationTypeCombo->addItems({"刚体配准", "仿射配准", "可变形配准", "多模态配准"});
    
    m_referenceImageCombo = new QComboBox();
    m_referenceImageCombo->addItem("请选择参考图像...", "");
    
    m_transformTypeCombo = new QComboBox();
    m_transformTypeCombo->addItems({"平移", "旋转", "缩放", "全变换"});
    
    typeLayout->addRow("配准类型:", m_registrationTypeCombo);
    typeLayout->addRow("参考图像:", m_referenceImageCombo);
    typeLayout->addRow("变换类型:", m_transformTypeCombo);
    
    layout->addWidget(typeGroup);
    
    // 配准参数
    QGroupBox* paramGroup = new QGroupBox("配准参数");
    QFormLayout* paramLayout = new QFormLayout(paramGroup);
    
    m_registrationAccuracySlider = new QSlider(Qt::Horizontal);
    m_registrationAccuracySlider->setRange(10, 100);
    m_registrationAccuracySlider->setValue(70);
    
    m_automaticRegistrationCheck = new QCheckBox("自动配准");
    m_automaticRegistrationCheck->setChecked(true);
    
    paramLayout->addRow("配准精度:", m_registrationAccuracySlider);
    paramLayout->addRow("配准模式:", m_automaticRegistrationCheck);
    
    layout->addWidget(paramGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
QWidget* MedicalProcessingWidget::createReconstructionPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 重建方法
    QGroupBox* methodGroup = new QGroupBox("重建方法");
    QFormLayout* methodLayout = new QFormLayout(methodGroup);
    
    m_reconstructionMethodCombo = new QComboBox();
    m_reconstructionMethodCombo->addItems({"Marching Cubes", "表面渲染", "体绘制", "多平面重建"});
    
    methodLayout->addRow("重建算法:", m_reconstructionMethodCombo);
    
    layout->addWidget(methodGroup);
    
    // 重建参数
    QGroupBox* paramGroup = new QGroupBox("重建参数");
    QFormLayout* paramLayout = new QFormLayout(paramGroup);
    
    m_isosurfaceValueSlider = new QSlider(Qt::Horizontal);
    m_isosurfaceValueSlider->setRange(0, 255);
    m_isosurfaceValueSlider->setValue(127);
    
    m_smoothingSlider = new QSlider(Qt::Horizontal);
    m_smoothingSlider->setRange(0, 100);
    m_smoothingSlider->setValue(30);
    
    paramLayout->addRow("等值面值:", m_isosurfaceValueSlider);
    paramLayout->addRow("平滑强度:", m_smoothingSlider);
    
    layout->addWidget(paramGroup);
    
    // 优化设置
    QGroupBox* optimizeGroup = new QGroupBox("优化设置");
    QVBoxLayout* optimizeLayout = new QVBoxLayout(optimizeGroup);
    
    m_decimationCheck = new QCheckBox("启用网格简化");
    m_decimationRatioSlider = new QSlider(Qt::Horizontal);
    m_decimationRatioSlider->setRange(10, 90);
    m_decimationRatioSlider->setValue(50);
    m_decimationRatioSlider->setEnabled(false);
    
    optimizeLayout->addWidget(m_decimationCheck);
    optimizeLayout->addWidget(new QLabel("简化比例:"));
    optimizeLayout->addWidget(m_decimationRatioSlider);
    
    layout->addWidget(optimizeGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
QWidget* MedicalProcessingWidget::createBatchProcessingPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 批量图像列表
    QGroupBox* imageGroup = new QGroupBox("批量图像");
    QVBoxLayout* imageLayout = new QVBoxLayout(imageGroup);
    
    m_batchImageList = new QListWidget();
    m_batchImageList->setSelectionMode(QAbstractItemView::MultiSelection);
    
    QHBoxLayout* batchBtnLayout = new QHBoxLayout();
    m_addToBatchBtn = new QPushButton("添加到批量");
    m_removeFromBatchBtn = new QPushButton("从批量移除");
    
    batchBtnLayout->addWidget(m_addToBatchBtn);
    batchBtnLayout->addWidget(m_removeFromBatchBtn);
    batchBtnLayout->addStretch();
    
    imageLayout->addWidget(m_batchImageList);
    imageLayout->addLayout(batchBtnLayout);
    
    layout->addWidget(imageGroup);
    
    // 批量操作设置
    QGroupBox* operationGroup = new QGroupBox("批量操作");
    QFormLayout* operationLayout = new QFormLayout(operationGroup);
    
    m_batchOperationCombo = new QComboBox();
    m_batchOperationCombo->addItems({"批量滤波", "批量分割", "批量配准", "批量重建", "格式转换"});
    
    operationLayout->addRow("操作类型:", m_batchOperationCombo);
    
    layout->addWidget(operationGroup);
    
    // 批量控制
    QGroupBox* controlGroup = new QGroupBox("批量控制");
    QVBoxLayout* controlLayout = new QVBoxLayout(controlGroup);
    
    QHBoxLayout* batchControlLayout = new QHBoxLayout();
    m_startBatchBtn = new QPushButton("开始批量处理");
    m_stopBatchBtn = new QPushButton("停止批量处理");
    m_stopBatchBtn->setEnabled(false);
    
    batchControlLayout->addWidget(m_startBatchBtn);
    batchControlLayout->addWidget(m_stopBatchBtn);
    
    m_batchProgressBar = new QProgressBar();
    
    controlLayout->addLayout(batchControlLayout);
    controlLayout->addWidget(m_batchProgressBar);
    
    layout->addWidget(controlGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
QWidget* MedicalProcessingWidget::createResultManagementPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 结果列表
    QGroupBox* resultGroup = new QGroupBox("处理结果");
    QVBoxLayout* resultLayout = new QVBoxLayout(resultGroup);
    
    m_resultTable = new QTableWidget(0, 4);
    QStringList headers;
    headers << "操作ID" << "处理类型" << "原始图像" << "结果图像";
    m_resultTable->setHorizontalHeaderLabels(headers);
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    resultLayout->addWidget(m_resultTable);
    
    layout->addWidget(resultGroup);
    
    // 结果操作
    QGroupBox* actionGroup = new QGroupBox("结果操作");
    QVBoxLayout* actionLayout = new QVBoxLayout(actionGroup);
    
    QHBoxLayout* resultBtnLayout = new QHBoxLayout();
    m_saveResultBtn = new QPushButton("保存结果");
    m_exportResultBtn = new QPushButton("导出数据");
    m_compareResultsBtn = new QPushButton("结果对比");
    m_deleteResultBtn = new QPushButton("删除结果");
    
    resultBtnLayout->addWidget(m_saveResultBtn);
    resultBtnLayout->addWidget(m_exportResultBtn);
    resultBtnLayout->addWidget(m_compareResultsBtn);
    resultBtnLayout->addWidget(m_deleteResultBtn);
    
    actionLayout->addLayout(resultBtnLayout);
    
    layout->addWidget(actionGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::setupStyles()
{
    setStyleSheet(R"(
        QWidget {
            background-color: #f8f9fa;
            font-family: "Microsoft YaHei", Arial, sans-serif;
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
        
        QTabBar::tab:hover {
            background-color: #6c757d;
            color: white;
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
        
        QPushButton:pressed {
            background-color: #004085;
        }
        
        QPushButton:disabled {
            background-color: #6c757d;
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
        
        QSlider::groove:horizontal {
            border: 1px solid #ced4da;
            height: 6px;
            background-color: #e9ecef;
            border-radius: 3px;
        }
        
        QSlider::handle:horizontal {
            background-color: #007bff;
            border: 1px solid #0056b3;
            width: 16px;
            margin: -6px 0;
            border-radius: 8px;
        }
        
        QSlider::handle:horizontal:hover {
            background-color: #0056b3;
        }
        
        QComboBox, QSpinBox {
            border: 1px solid #ced4da;
            border-radius: 4px;
            padding: 6px 8px;
            background-color: white;
            min-width: 100px;
        }
        
        QComboBox:focus, QSpinBox:focus {
            border-color: #007bff;
        }
        
        QTextEdit, QListWidget, QTableWidget {
            border: 1px solid #dee2e6;
            border-radius: 4px;
            background-color: white;
        }
        
        QTableWidget::item:selected {
            background-color: #007bff;
            color: white;
        }
        
        QCheckBox {
            spacing: 8px;
            font-weight: 500;
        }
        
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 2px solid #ced4da;
            border-radius: 3px;
            background-color: white;
        }
        
        QCheckBox::indicator:checked {
            background-color: #007bff;
            border-color: #0056b3;
        }
        
        QProgressBar {
            border: 1px solid #dee2e6;
            border-radius: 4px;
            text-align: center;
            background-color: #e9ecef;
            font-weight: bold;
        }
        
        QProgressBar::chunk {
            background-color: #28a745;
            border-radius: 3px;
        }
        
        QToolBar {
            background-color: #ffffff;
            border: 1px solid #dee2e6;
            border-radius: 6px;
            padding: 4px;
            spacing: 6px;
        }
        
        QSplitter::handle {
            background-color: #dee2e6;
        }
        
        QSplitter::handle:horizontal {
            width: 4px;
        }
        
        QSplitter::handle:vertical {
            height: 4px;
        }
        
        QGraphicsView {
            border: 1px solid #dee2e6;
            border-radius: 4px;
        }
    )");
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::connectSignals()
{
    // 主控制信号
    connect(m_processingTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MedicalProcessingWidget::onProcessingTypeChanged);
    connect(m_imageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MedicalProcessingWidget::onImageSelectionChanged);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MedicalProcessingWidget::onPresetSelected);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MedicalProcessingWidget::refreshImageList);
    
    // 基础处理参数信号
    connect(m_filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MedicalProcessingWidget::onBasicFilterChanged);
    connect(m_filterStrengthSlider, &QSlider::valueChanged, this, &MedicalProcessingWidget::onParameterChanged);
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &MedicalProcessingWidget::onEnhancementChanged);
    connect(m_contrastSlider, &QSlider::valueChanged, this, &MedicalProcessingWidget::onEnhancementChanged);
    connect(m_gammaSlider, &QSlider::valueChanged, this, &MedicalProcessingWidget::onEnhancementChanged);
    connect(m_noiseReductionCheck, &QCheckBox::toggled, this, &MedicalProcessingWidget::onNoiseReductionChanged);
    connect(m_noiseReductionSlider, &QSlider::valueChanged, this, &MedicalProcessingWidget::onNoiseReductionChanged);
    
    // 分割参数信号
    connect(m_segmentationMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MedicalProcessingWidget::onSegmentationTypeChanged);
    connect(m_thresholdSlider, &QSlider::valueChanged, this, &MedicalProcessingWidget::onParameterChanged);
    connect(m_thresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MedicalProcessingWidget::onParameterChanged);

    // 阈值滑块和输入框同步
    connect(m_thresholdSlider, &QSlider::valueChanged, m_thresholdSpinBox, &QSpinBox::setValue);
    connect(m_thresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_thresholdSlider, &QSlider::setValue);
    
    // 配准参数信号
    connect(m_registrationTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MedicalProcessingWidget::onRegistrationChanged);
    connect(m_referenceImageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MedicalProcessingWidget::onParameterChanged);
    
    // 3D重建参数信号
    connect(m_reconstructionMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MedicalProcessingWidget::onReconstructionChanged);
    connect(m_isosurfaceValueSlider, &QSlider::valueChanged, this, &MedicalProcessingWidget::onParameterChanged);
    connect(m_decimationCheck, &QCheckBox::toggled, [this](bool enabled) {
        m_decimationRatioSlider->setEnabled(enabled);
    });
    
    // 批量处理信号
    connect(m_addToBatchBtn, &QPushButton::clicked, this, &MedicalProcessingWidget::onBatchProcessingSetup);
    connect(m_startBatchBtn, &QPushButton::clicked, this, &MedicalProcessingWidget::onBatchProcessingStart);
    connect(m_stopBatchBtn, &QPushButton::clicked, this, &MedicalProcessingWidget::onBatchProcessingStop);
    
    // 结果管理信号
    connect(m_saveResultBtn, &QPushButton::clicked, this, &MedicalProcessingWidget::onSaveResult);
    connect(m_exportResultBtn, &QPushButton::clicked, this, &MedicalProcessingWidget::onExportResult);
    connect(m_compareResultsBtn, &QPushButton::clicked, this, &MedicalProcessingWidget::onCompareResults);
    
    // 噪声抑制启用状态
    connect(m_noiseReductionCheck, &QCheckBox::toggled, m_noiseReductionSlider, &QSlider::setEnabled);
}









//-----------------------------------------------------------------------------
void MedicalProcessingWidget::initializeEventAdmin()
{
    if (!m_pluginContext) {
        qWarning() << "[MedicalProcessingWidget] CTK插件上下文未设置，无法初始化EventAdmin";
        return;
    }

    // 获取EventAdmin服务
    ctkServiceReference eventAdminRef = m_pluginContext->getServiceReference<ctkEventAdmin>();
    if (eventAdminRef) {
        m_eventAdmin = m_pluginContext->getService<ctkEventAdmin>(eventAdminRef);
        if (m_eventAdmin) {
            qDebug() << "[MedicalProcessingWidget] EventAdmin服务连接成功";
        } else {
            qWarning() << "[MedicalProcessingWidget] 无法获取EventAdmin服务实例";
        }
    } else {
        qWarning() << "[MedicalProcessingWidget] 未找到EventAdmin服务";
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::setupImageListPolling()
{
    qDebug() << "[MedicalProcessingWidget] 启动图像列表智能轮询";

    // 创建定时器，每5秒检查一次图像列表更新（降低频率）
    QTimer* pollingTimer = new QTimer(this);
    pollingTimer->setInterval(5000); // 5秒间隔，减少系统负载

    connect(pollingTimer, &QTimer::timeout, this, [this]() {
        // 静默检查图像列表变化
        if (m_imageService) {
            QStringList currentImages = m_imageService->getLoadedImages();
            static QStringList lastImages;

            if (currentImages != lastImages) {
                qDebug() << "[MedicalProcessingWidget] 🔄 检测到图像列表变化，自动更新界面";
                this->refreshImageList();
                lastImages = currentImages;

                // 更新日志（用户友好的提示）
                if (m_logTextEdit) {
                    m_logTextEdit->append(QString("[%1] 🔄 图像列表已自动更新")
                                         .arg(QTime::currentTime().toString()));
                }
            }
        }
    });

    pollingTimer->start();
    qDebug() << "[MedicalProcessingWidget] ✅ 智能轮询已启动 (间隔5秒)";

    // 在日志中提示用户
    if (m_logTextEdit) {
        m_logTextEdit->append(QString("[%1] ✅ 图像列表自动同步已启用，也可点击刷新按钮手动更新")
                             .arg(QTime::currentTime().toString()));
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::initializeServiceConnections()
{
    if (!m_pluginContext) {
        qWarning() << "[MedicalProcessingWidget] CTK插件上下文未设置";
        return;
    }
    
    // 获取处理服务
    ctkServiceReference processingServiceRef = m_pluginContext->getServiceReference<MedicalProcessingService>();
    if (processingServiceRef) {
        m_processingService = qobject_cast<MedicalProcessingService*>(
            m_pluginContext->getService(processingServiceRef));
        
        if (m_processingService) {
            qDebug() << "[MedicalProcessingWidget] MedicalProcessingService连接成功";
            onProcessingServiceAvailable();
        }
    }
    
    // 获取图像服务（使用字符串方式以确保与插件注册一致）
    ctkServiceReference imageServiceRef = m_pluginContext->getServiceReference("medical.MedicalImageCoreService");
    if (imageServiceRef) {
        QObject* serviceObj = m_pluginContext->getService(imageServiceRef);
        m_imageService = qobject_cast<MedicalImageCoreService*>(serviceObj);
        
        if (m_imageService) {
            qDebug() << "[MedicalProcessingWidget] MedicalImageCoreService连接成功";
            qDebug() << "[MedicalProcessingWidget] 服务实例地址:" << m_imageService;
            
            // 图像服务连接成功，发送初始化完成事件
            if (m_eventAdmin) {
                ctkDictionary props;
                props["serviceType"] = "MedicalImageCoreService";
                props["status"] = "connected";

                ctkEvent serviceEvent("medical/service/connected", props);
                m_eventAdmin->sendEvent(serviceEvent);

                qDebug() << "[MedicalProcessingWidget] 已发送图像服务连接事件";
            }
            
            onImageServiceAvailable();
        } else {
            qDebug() << "[MedicalProcessingWidget] MedicalImageCoreService转换失败";
        }
    } else {
        qDebug() << "[MedicalProcessingWidget] 未找到MedicalImageCoreService服务引用";
    }
    
    m_serviceConnected = (m_processingService != nullptr && m_imageService != nullptr);
    
    if (m_serviceConnected) {
        updateStatus("服务连接成功，处理工作台已就绪");
        refreshImageList();
    } else {
        updateStatus("等待服务连接...");
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onProcessingTypeChanged()
{
    QString type = m_processingTypeCombo->currentData().toString();
    setProcessingType(type);
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onImageSelectionChanged()
{
    QString imageId = m_imageSelector->currentData().toString();
    if (!imageId.isEmpty()) {
        loadImageForProcessing(imageId);
        
        // 记录操作日志
        QString imageName = m_imageSelector->currentText();
        m_logTextEdit->append(QString("[%1] 选择处理图像: %2")
                             .arg(QTime::currentTime().toString())
                             .arg(imageName));
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onParameterChanged()
{
    // 参数变化时自动更新预览
    if (m_serviceConnected && !m_currentImageId.isEmpty()) {
        QTimer::singleShot(500, this, &MedicalProcessingWidget::previewProcessing);
    }
    
    updateParameterDisplay();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onPresetSelected()
{
    QString preset = m_presetCombo->currentText();
    
    if (preset == "快速处理") {
        // 设置快速处理参数
        m_filterStrengthSlider->setValue(30);
        m_registrationAccuracySlider->setValue(50);
        updateStatus("已应用快速处理预设");
    } else if (preset == "高质量") {
        // 设置高质量参数
        m_filterStrengthSlider->setValue(70);
        m_registrationAccuracySlider->setValue(90);
        updateStatus("已应用高质量预设");
    } else if (preset == "默认") {
        // 重置为默认参数
        resetAllParameters();
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onBasicFilterChanged()
{
    QString filterType = m_filterTypeCombo->currentText();
    updateStatus(QString("切换滤波类型: %1").arg(filterType));
    onParameterChanged();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onEnhancementChanged()
{
    int brightness = m_brightnessSlider->value();
    int contrast = m_contrastSlider->value();
    int gamma = m_gammaSlider->value();
    
    updateStatus(QString("图像增强: 亮度%1 对比度%2 伽马%3")
                 .arg(brightness).arg(contrast).arg(gamma));
    onParameterChanged();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onNoiseReductionChanged()
{
    bool enabled = m_noiseReductionCheck->isChecked();
    int strength = m_noiseReductionSlider->value();
    
    updateStatus(QString("噪声抑制: %1 (强度: %2)")
                 .arg(enabled ? "开启" : "关闭").arg(strength));
    onParameterChanged();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onSegmentationTypeChanged()
{
    QString method = m_segmentationMethodCombo->currentText();
    updateStatus(QString("切换分割方法: %1").arg(method));
    onParameterChanged();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onRegistrationChanged()
{
    QString type = m_registrationTypeCombo->currentText();
    updateStatus(QString("切换配准类型: %1").arg(type));
    onParameterChanged();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onReconstructionChanged()
{
    QString method = m_reconstructionMethodCombo->currentText();
    updateStatus(QString("切换重建方法: %1").arg(method));
    onParameterChanged();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onBatchProcessingSetup()
{
    QString currentImageId = m_imageSelector->currentData().toString();
    if (!currentImageId.isEmpty()) {
        QString imageName = m_imageSelector->currentText();
        
        // 检查是否已在批量列表中
        bool alreadyAdded = false;
        for (int i = 0; i < m_batchImageList->count(); ++i) {
            if (m_batchImageList->item(i)->data(Qt::UserRole).toString() == currentImageId) {
                alreadyAdded = true;
                break;
            }
        }
        
        if (!alreadyAdded) {
            QListWidgetItem* item = new QListWidgetItem(imageName);
            item->setData(Qt::UserRole, currentImageId);
            m_batchImageList->addItem(item);
            m_batchImageIds.append(currentImageId);
            
            updateStatus(QString("已添加到批量处理: %1").arg(imageName));
        } else {
            updateStatus("图像已在批量列表中");
        }
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onBatchProcessingStart()
{
    if (m_batchImageIds.isEmpty()) {
        QMessageBox::warning(this, "批量处理", "请先添加图像到批量列表");
        return;
    }
    
    QString operation = m_batchOperationCombo->currentText();
    updateStatus(QString("开始批量处理: %1 (%2个图像)")
                 .arg(operation).arg(m_batchImageIds.size()));
    
    m_startBatchBtn->setEnabled(false);
    m_stopBatchBtn->setEnabled(true);
    m_batchProgressBar->setValue(0);
    m_batchProgressBar->setMaximum(m_batchImageIds.size());
    
    // 模拟批量处理
    QTimer* batchTimer = new QTimer(this);
    connect(batchTimer, &QTimer::timeout, [this, batchTimer]() {
        static int processedCount = 0;
        processedCount++;
        
        m_batchProgressBar->setValue(processedCount);
        updateStatus(QString("批量处理进度: %1/%2")
                     .arg(processedCount).arg(m_batchImageIds.size()));
        
        if (processedCount >= m_batchImageIds.size()) {
            batchTimer->stop();
            batchTimer->deleteLater();
            onBatchProcessingStop();
            processedCount = 0;
        }
    });
    
    batchTimer->start(2000); // 每2秒处理一个图像（模拟）
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onBatchProcessingStop()
{
    m_startBatchBtn->setEnabled(true);
    m_stopBatchBtn->setEnabled(false);
    updateStatus("批量处理已完成");
    
    m_logTextEdit->append(QString("[%1] 批量处理完成")
                         .arg(QTime::currentTime().toString()));
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onSaveResult()
{
    int currentRow = m_resultTable->currentRow();
    if (currentRow < 0) {
        QMessageBox::information(this, "保存结果", "请选择要保存的处理结果");
        return;
    }
    
    QString operationId = m_resultTable->item(currentRow, 0)->text();
    QString resultImageId = m_processingResults.value(operationId);
    
    if (!resultImageId.isEmpty()) {
        QString fileName = QFileDialog::getSaveFileName(this,
            "保存处理结果", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            "Medical Images (*.dcm *.nii *.nrrd);;All Files (*)");
        
        if (!fileName.isEmpty()) {
            updateStatus(QString("保存处理结果到: %1").arg(fileName));
            // TODO: 实际保存逻辑
        }
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onExportResult()
{
    QMessageBox::information(this, "数据导出", 
        QString("处理结果导出功能\n\n将实现：\n- 处理结果导出\n- 参数配置导出\n- 报告生成\n- 批量导出"));
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onCompareResults()
{
    QList<QTableWidgetItem*> selectedItems = m_resultTable->selectedItems();
    if (selectedItems.size() < 2) {
        QMessageBox::information(this, "结果对比", "请选择至少两个处理结果进行对比");
        return;
    }
    
    updateStatus("启动结果对比界面...");
    // TODO: 实现结果对比功能
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onProcessingServiceAvailable()
{
    if (m_processingService) {
        // 连接处理服务信号
        connect(m_processingService, &MedicalProcessingService::processingCompleted,
                this, &MedicalProcessingWidget::onProcessingCompleted);
        connect(m_processingService, &MedicalProcessingService::processingFailed,
                this, &MedicalProcessingWidget::onProcessingFailed);
        connect(m_processingService, &MedicalProcessingService::processingProgress,
                this, &MedicalProcessingWidget::onProcessingProgress);
        
        updateStatus("医学图像处理服务已连接");
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onImageServiceAvailable()
{
    updateStatus("医学图像核心服务已连接");

    // 通过事件机制请求初始图像列表
    refreshImageList();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onProcessingCompleted(const QString& operationId, const QString& resultImageId)
{
    // 记录处理结果
    m_processingResults[operationId] = resultImageId;
    
    // 添加到结果表格
    int row = m_resultTable->rowCount();
    m_resultTable->insertRow(row);
    m_resultTable->setItem(row, 0, new QTableWidgetItem(operationId));
    m_resultTable->setItem(row, 1, new QTableWidgetItem(m_currentProcessingType));
    m_resultTable->setItem(row, 2, new QTableWidgetItem(m_currentImageId));
    m_resultTable->setItem(row, 3, new QTableWidgetItem(resultImageId));
    
    updateStatus(QString("处理完成: %1").arg(operationId));
    updateProgress(100);
    
    m_logTextEdit->append(QString("[%1] 处理完成: %2 -> %3")
                         .arg(QTime::currentTime().toString())
                         .arg(operationId)
                         .arg(resultImageId));
    
    // 更新预览
    updatePreviewImages();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onProcessingFailed(const QString& operationId, const QString& error)
{
    updateStatus(QString("处理失败: %1").arg(error));
    updateProgress(0);
    
    m_logTextEdit->append(QString("[%1] 处理失败: %2 - %3")
                         .arg(QTime::currentTime().toString())
                         .arg(operationId)
                         .arg(error));
    
    QMessageBox::warning(this, "处理失败", error);
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::onProcessingProgress(const QString& operationId, int progress)
{
    Q_UNUSED(operationId);
    updateProgress(progress);
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::updatePreviewImages()
{
    // 更新原始图像预览
    if (!m_currentImageId.isEmpty() && m_imageService) {
        loadAndDisplayImage(m_currentImageId, m_originalImageScene, m_originalImageView, m_originalImageLabel, "原始图像");
    } else {
        clearImageDisplay(m_originalImageScene, m_originalImageLabel, "等待图像加载...");
    }

    // 更新处理结果预览
    QString resultImageId = m_processingResults.value(m_currentOperationId);
    if (!resultImageId.isEmpty()) {
        loadAndDisplayImage(resultImageId, m_processedImageScene, m_processedImageView, m_processedImageLabel, "处理结果");
    } else {
        clearImageDisplay(m_processedImageScene, m_processedImageLabel, "等待处理结果...");
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::updateParameterDisplay()
{
    // 根据当前处理类型更新参数显示
    if (m_currentProcessingType == "BasicFiltering") {
        QString filterInfo = QString("滤波: %1 (强度: %2)")
                            .arg(m_filterTypeCombo->currentText())
                            .arg(m_filterStrengthSlider->value());
        updateStatus(filterInfo);
    } else if (m_currentProcessingType == "Segmentation") {
        QString segInfo = QString("分割: %1 (阈值: %2 HU)")
                         .arg(m_segmentationMethodCombo->currentText())
                         .arg(m_thresholdSpinBox->value());
        updateStatus(segInfo);
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::updateStatus(const QString& message)
{
    m_statusLabel->setText(message);
    
    // 根据消息类型设置颜色
    if (message.contains("错误") || message.contains("失败")) {
        m_statusLabel->setStyleSheet("QLabel { color: #dc3545; font-weight: bold; }");
    } else if (message.contains("成功") || message.contains("完成")) {
        m_statusLabel->setStyleSheet("QLabel { color: #28a745; font-weight: bold; }");
    } else if (message.contains("警告")) {
        m_statusLabel->setStyleSheet("QLabel { color: #ffc107; font-weight: bold; }");
    } else {
        m_statusLabel->setStyleSheet("QLabel { color: #007bff; font-weight: bold; }");
    }
    
    qDebug() << "[MedicalProcessingWidget]" << message;
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::updateProgress(int value)
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

//-----------------------------------------------------------------------------
QVariantMap MedicalProcessingWidget::createParameterMap()
{
    QVariantMap parameters;
    
    if (m_currentProcessingType == "BasicFiltering") {
        parameters["filterType"] = m_filterTypeCombo->currentText();
        parameters["filterStrength"] = m_filterStrengthSlider->value();
        parameters["brightness"] = m_brightnessSlider->value();
        parameters["contrast"] = m_contrastSlider->value();
        parameters["gamma"] = m_gammaSlider->value() / 100.0;
        parameters["noiseReduction"] = m_noiseReductionCheck->isChecked();
        parameters["noiseReductionStrength"] = m_noiseReductionSlider->value();
    } else if (m_currentProcessingType == "Segmentation") {
        parameters["method"] = m_segmentationMethodCombo->currentText();
        parameters["threshold"] = m_thresholdSpinBox->value();  // 使用输入框的值
        // 修复：为阈值分割提供正确的参数
        parameters["lowerThreshold"] = 0.0;  // 下阈值设为0
        parameters["upperThreshold"] = static_cast<double>(m_thresholdSpinBox->value());  // 上阈值使用输入框值
        parameters["seedPointX"] = m_seedPointXSpin->value();
        parameters["seedPointY"] = m_seedPointYSpin->value();
        parameters["tolerance"] = m_regionGrowingToleranceSlider->value();
        parameters["morphology"] = m_morphologyCheck->isChecked();
    } else if (m_currentProcessingType == "Registration") {
        parameters["registrationType"] = m_registrationTypeCombo->currentText();
        parameters["transformType"] = m_transformTypeCombo->currentText();
        parameters["accuracy"] = m_registrationAccuracySlider->value();
        parameters["automatic"] = m_automaticRegistrationCheck->isChecked();
    } else if (m_currentProcessingType == "Reconstruction") {
        parameters["method"] = m_reconstructionMethodCombo->currentText();
        parameters["isosurfaceValue"] = m_isosurfaceValueSlider->value();
        parameters["smoothing"] = m_smoothingSlider->value();
        parameters["decimation"] = m_decimationCheck->isChecked();
        parameters["decimationRatio"] = m_decimationRatioSlider->value() / 100.0;
    }
    
    return parameters;
}

//-----------------------------------------------------------------------------
bool MedicalProcessingWidget::validateParameters()
{
    if (m_currentImageId.isEmpty()) {
        QMessageBox::warning(this, "参数验证", "请选择要处理的图像");
        return false;
    }
    
    if (m_currentProcessingType == "Registration") {
        QString referenceImageId = m_referenceImageCombo->currentData().toString();
        if (referenceImageId.isEmpty()) {
            QMessageBox::warning(this, "参数验证", "配准操作需要选择参考图像");
            return false;
        }
        
        if (referenceImageId == m_currentImageId) {
            QMessageBox::warning(this, "参数验证", "参考图像不能与目标图像相同");
            return false;
        }
    }
    
    return true;
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::updateProcessingProgress(int value)
{
    // 更新进度条显示
    updateProgress(value);
    
    // 更新状态信息
    updateStatus(QString("处理进度: %1%").arg(value));

    // 强制界面更新
    QApplication::processEvents();
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::loadAndDisplayImage(const QString& imageId,
                                                 QGraphicsScene* scene,
                                                 QGraphicsView* view,
                                                 QLabel* label,
                                                 const QString& labelPrefix)
{
    if (!m_imageService || imageId.isEmpty()) {
        clearImageDisplay(scene, label, "无效的图像数据");
        return;
    }

    try {
        // 直接使用CTK服务调用获取图像详细信息
        QVariantMap details = m_imageService->getImageDetails(imageId);

        if (!details.isEmpty()) {
            // 显示图像信息
            scene->clear();

            // 获取图像基本信息
            QString format = details.value("format", "Unknown").toString();
            QVariantList dimensionsList = details.value("dimensions", QVariantList()).toList();
            QString dataType = details.value("dataType", "Unknown").toString();
            QString filePath = details.value("filePath", "").toString();

            // 构建尺寸字符串
            QStringList dimensionStrings;
            for (const QVariant& dim : dimensionsList) {
                dimensionStrings << QString::number(dim.toInt());
            }
            QString dimensionsStr = dimensionStrings.join("×");

            // 创建信息文本
            QString infoText = QString("图像信息:\n格式: %1\n尺寸: %2\n数据类型: %3\n文件: %4")
                              .arg(format)
                              .arg(dimensionsStr.isEmpty() ? "未知" : dimensionsStr)
                              .arg(dataType)
                              .arg(QFileInfo(filePath).fileName());

            // 添加文本到场景
            QGraphicsTextItem* textItem = scene->addText(infoText);
            textItem->setDefaultTextColor(Qt::darkBlue);
            QFont font = textItem->font();
            font.setPointSize(10);
            textItem->setFont(font);

            // 调整视图以适应文本
            view->fitInView(textItem, Qt::KeepAspectRatio);

            // 更新标签
            label->setText(QString("%1: %2 (%3)")
                          .arg(labelPrefix)
                          .arg(imageId.left(8) + "...")
                          .arg(dimensionsStr.isEmpty() ? "信息显示" : dimensionsStr));

            qDebug() << "[MedicalProcessingWidget] ✅ 成功显示图像信息:" << imageId;

        } else {
            clearImageDisplay(scene, label, "无法获取图像信息");
            qWarning() << "[MedicalProcessingWidget] ❌ 无法获取图像详细信息:" << imageId;
        }

    } catch (const std::exception& e) {
        clearImageDisplay(scene, label, "图像加载异常");
        qWarning() << "[MedicalProcessingWidget] 图像加载异常:" << e.what();
    } catch (...) {
        clearImageDisplay(scene, label, "未知图像加载错误");
        qWarning() << "[MedicalProcessingWidget] 未知图像加载错误";
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::clearImageDisplay(QGraphicsScene* scene, QLabel* label, const QString& message)
{
    if (scene) {
        scene->clear();

        // 添加提示文本
        QGraphicsTextItem* textItem = scene->addText(message);
        textItem->setDefaultTextColor(Qt::gray);
        QFont font = textItem->font();
        font.setItalic(true);
        textItem->setFont(font);
    }

    if (label) {
        label->setText(message);
    }
}

//-----------------------------------------------------------------------------
void MedicalProcessingWidget::initializeProcessingThread()
{
    // 创建工作线程
    m_processingThread = new QThread(this);

    // 创建工作对象
    m_processingWorker = new ImageProcessingWorker();

    // 将工作对象移动到工作线程
    m_processingWorker->moveToThread(m_processingThread);

    // 连接信号槽
    connect(m_processingThread, &QThread::started,
            m_processingWorker, &ImageProcessingWorker::processImage);

    connect(m_processingWorker, &ImageProcessingWorker::progressUpdated,
            this, [this](int percentage) {
                updateProgress(percentage);
                updateStatus(QString("处理进度: %1%").arg(percentage));
            });

    connect(m_processingWorker, &ImageProcessingWorker::processingCompleted,
            this, [this](const QString& operationId, const QString& resultImageId) {
                // 处理完成
                onProcessingCompleted(operationId, resultImageId);

                // 重新启用处理按钮
                if (m_applyAction) {
                    m_applyAction->setEnabled(true);
                }

                // 停止线程
                if (m_processingThread) {
                    m_processingThread->quit();
                }

                updateStatus("图像处理完成");
                updateProgress(100);
            });

    connect(m_processingWorker, &ImageProcessingWorker::processingFailed,
            this, [this](const QString& operationId, const QString& error) {
                // 处理失败
                onProcessingFailed(operationId, error);

                // 重新启用处理按钮
                if (m_applyAction) {
                    m_applyAction->setEnabled(true);
                }

                // 停止线程
                if (m_processingThread) {
                    m_processingThread->quit();
                }

                updateStatus(QString("图像处理失败: %1").arg(error));
                updateProgress(0);
            });

    qDebug() << "[MedicalProcessingWidget] ✅ 图像处理工作线程初始化完成";
}

//=============================================================================
// ImageProcessingWorker 实现
//=============================================================================

ImageProcessingWorker::ImageProcessingWorker(QObject* parent)
    : QObject(parent)
    , m_processingService(nullptr)
{
}

void ImageProcessingWorker::setProcessingParameters(const QString& imageId,
                                                   const QString& processingType,
                                                   const QVariantMap& parameters,
                                                   MedicalProcessingService* processingService)
{
    m_imageId = imageId;
    m_processingType = processingType;
    m_parameters = parameters;
    m_processingService = processingService;
    m_operationId = QUuid::createUuid().toString();
}

void ImageProcessingWorker::processImage()
{
    if (!m_processingService || m_imageId.isEmpty()) {
        emit processingFailed(m_operationId, "无效的处理服务或图像ID");
        return;
    }

    try {
        qDebug() << "[ImageProcessingWorker] 🚀 开始处理图像:" << m_imageId;
        qDebug() << "[ImageProcessingWorker] 处理类型:" << m_processingType;

        emit progressUpdated(10);

        QString resultImageId;

        if (m_processingType == "BasicFiltering") {
            // 基础滤波处理
            QString operation = m_parameters.value("operation", "高斯滤波").toString();

            emit progressUpdated(30);

            if (operation.contains("高斯")) {
                double sigma = m_parameters.value("sigma", 1.0).toDouble();

                // 直接使用CTK服务调用
                resultImageId = m_processingService->gaussianFilter(m_imageId, sigma);

                if (resultImageId.isEmpty()) {
                    emit processingFailed(m_operationId, "高斯滤波处理失败");
                    return;
                }

            } else if (operation.contains("中值")) {
                int radius = m_parameters.value("radius", 1).toInt();

                // 直接使用CTK服务调用
                resultImageId = m_processingService->medianFilter(m_imageId, radius);

                if (resultImageId.isEmpty()) {
                    emit processingFailed(m_operationId, "中值滤波处理失败");
                    return;
                }
            }

        } else if (m_processingType == "Segmentation") {
            // 图像分割处理
            QString operation = m_parameters.value("operation", "阈值分割").toString();

            emit progressUpdated(30);

            if (operation.contains("阈值")) {
                // 修复：从参数中正确获取阈值
                double lower = m_parameters.value("lowerThreshold", 0.0).toDouble();
                double upper = m_parameters.value("upperThreshold", static_cast<double>(m_parameters.value("threshold", 127).toInt())).toDouble();

                qDebug() << "[ImageProcessingWorker] 阈值分割参数 - 下阈值:" << lower << "上阈值:" << upper;

                // 直接使用CTK服务调用
                resultImageId = m_processingService->thresholdSegmentation(m_imageId, lower, upper);

                if (resultImageId.isEmpty()) {
                    emit processingFailed(m_operationId, "阈值分割处理失败");
                    return;
                }
            }
        }

        emit progressUpdated(80);

        // 模拟一些处理时间
        QThread::msleep(500);

        emit progressUpdated(100);

        if (!resultImageId.isEmpty()) {
            qDebug() << "[ImageProcessingWorker] ✅ 图像处理完成:" << resultImageId;
            emit processingCompleted(m_operationId, resultImageId);
        } else {
            emit processingFailed(m_operationId, "处理结果为空");
        }

    } catch (const std::exception& e) {
        qWarning() << "[ImageProcessingWorker] 图像处理异常:" << e.what();
        emit processingFailed(m_operationId, QString("处理异常: %1").arg(e.what()));
    } catch (...) {
        qWarning() << "[ImageProcessingWorker] 图像处理发生未知异常";
        emit processingFailed(m_operationId, "未知处理异常");
    }
}
