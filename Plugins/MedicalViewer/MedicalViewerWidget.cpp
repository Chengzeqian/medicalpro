#include "MedicalViewerWidget.h"
#include "MedicalViewerService.h"
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
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QTime>
#include <QDateTime>
#include <QGraphicsPixmapItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsEllipseItem>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <cmath>

#ifdef VTK_FOUND
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkVolume.h>
#include <vtkVolumeMapper.h>
#include <vtkFixedPointVolumeRayCastMapper.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkFloatArray.h>
#include <vtkCamera.h>
#include <vtkNew.h>
#include <QVTKOpenGLNativeWidget.h>
#endif

//-----------------------------------------------------------------------------
MedicalViewerWidget::MedicalViewerWidget(QWidget* parent)
    : QWidget(parent)
    , m_pluginContext(nullptr)
    , m_viewerService(nullptr)
    , m_imageService(nullptr)
    , m_serviceConnected(false)
    , m_eventAdmin(nullptr)
    , m_mainLayout(nullptr)
    , m_contentLayout(nullptr)
    , m_mainSplitter(nullptr)
    , m_mainToolBar(nullptr)
    , m_displayTabs(nullptr)
    , m_controlTabs(nullptr)
    , m_2dViewerTab(nullptr)
    , m_3dViewerTab(nullptr)
    , m_mprViewerTab(nullptr)
    , m_volumeRenderingTab(nullptr)
    , m_advancedVisualizationTab(nullptr)
    , m_statusPanel(nullptr)
    , m_displayModeCombo(nullptr)
    , m_imageSelector(nullptr)
    , m_currentDisplayMode("2D")
    , m_currentPixmapItem(nullptr)
    , m_currentImageView(nullptr)
    , m_windowCenter(127.5)      // 默认中心值 (8位图像)
    , m_windowWidth(255.0)       // 默认窗口宽度 (8位图像)
    , m_defaultWindowCenter(127.5)
    , m_defaultWindowWidth(255.0)
#ifdef VTK_FOUND
    , m_vtkRenderer(nullptr)
    , m_vtkVolume(nullptr)
    , m_vtkVolumeProperty(nullptr)
    , m_vtkOpacityFunction(nullptr)
    , m_vtkColorFunction(nullptr)
    , m_vtkWidget(nullptr)
#endif
    , m_currentMeasurementMode(NoMeasurement)
    , m_currentMeasurementView(nullptr)
{
    qDebug() << "[MedicalViewerWidget] 创建多模式医学图像查看器界面";
    
    // 设置窗口属性
    setWindowTitle("多模式医学图像查看器");
    setObjectName("MedicalViewerWidget");  // 设置对象名称便于查找
    setMinimumSize(1200, 800);
    
    // 初始化UI
    initializeUI();
    
    // 设置样式
    setupStyles();
    
    // 连接信号槽
    connectSignals();
    
    // 延迟刷新图像列表，确保服务已连接
    QTimer::singleShot(500, this, [this]() {
        qDebug() << "[MedicalViewerWidget] 延迟刷新图像列表";
        if (m_imageService) {
            refreshImageList();
        }
    });
    
    qDebug() << "[MedicalViewerWidget] 多模式医学图像查看器界面创建完成";
}

//-----------------------------------------------------------------------------
MedicalViewerWidget::~MedicalViewerWidget()
{
    qDebug() << "[MedicalViewerWidget] 销毁多模式医学图像查看器界面";
}

//-----------------------------------------------------------------------------
MedicalImageCoreService* MedicalViewerWidget::getImageCoreService() const
{
    if (!m_imageService) {
        return nullptr;
    }
    
    auto coreService = qobject_cast<MedicalImageCoreService*>(m_imageService);
    if (!coreService) {
        qDebug() << "[MedicalViewerWidget] 警告：无法转换为MedicalImageCoreService，实际类型:" 
                 << m_imageService->metaObject()->className();
    }
    
    return coreService;
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::setPluginContext(ctkPluginContext* context)
{
    m_pluginContext = context;
    qDebug() << "[MedicalViewerWidget] 设置CTK插件上下文";

    // 初始化EventAdmin服务
    initializeEventAdmin();

    // 初始化服务连接
    initializeServiceConnections();
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::displayImage(const QString& imageId)
{
    if (imageId.isEmpty()) {
        updateStatus("无效的图像ID");
        return;
    }
    
    m_currentImageId = imageId;
    updateStatus(QString("正在显示图像: %1").arg(imageId));
    
    // 简化版本 - 主要用于兼容性
    displayImageAsync(imageId);
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::displayImageAsync(const QString& imageId)
{
    if (imageId.isEmpty()) {
        updateStatus("无效的图像ID");
        m_imageSelector->setEnabled(true);
        return;
    }
    
    try {
        m_currentImageId = imageId;
        updateStatus(QString("正在准备显示图像: %1").arg(imageId.left(8) + "..."));
        
        // 获取图像信息（直接调用方式）
        if (auto coreService = getImageCoreService()) {
            QVariantMap imageInfo = coreService->getImageDetails(imageId);
            QString format = imageInfo.value("format", "Unknown").toString();
            QList<int> dimensionsList = imageInfo.value("dimensions").value<QList<int>>();
            
            // 解析图像维度（在整个函数作用域有效）
            int imgWidth = dimensionsList.size() > 0 ? dimensionsList[0] : 0;
            int imgHeight = dimensionsList.size() > 1 ? dimensionsList[1] : 0;
            int imgDepth = dimensionsList.size() > 2 ? dimensionsList[2] : 1;
            
            // 显示图像信息而不是创建复杂的VTK组件
            QString infoText = QString("图像ID: %1\n格式: %2\n").arg(imageId).arg(format);
            
            if (!dimensionsList.isEmpty()) {
                qDebug() << "[MedicalViewerWidget] 2D渲染器正确解析维度:" << dimensionsList << "→" << imgWidth << "x" << imgHeight << "x" << imgDepth;
                
                if (imgDepth > 1) {
                    infoText += QString("维度: %1×%2×%3\n").arg(imgWidth).arg(imgHeight).arg(imgDepth);
                } else {
                    infoText += QString("维度: %1×%2\n").arg(imgWidth).arg(imgHeight);
                }
            }
            
            infoText += QString("显示模式: %1\n").arg(m_currentDisplayMode);
            infoText += QString("状态: 图像渲染引擎已就绪\n\n");
            
            // 添加智能推荐
            QStringList recommendedModes;
            if (imgDepth > 1) {
                recommendedModes << "🎨 3D体绘制" << "📐 MPR重建" << "🔬 科研级可视化";
                infoText += "💡 推荐查看模式:\n";
                infoText += QString("• %1 (适合3D数据: %2×%3×%4)\n").arg(recommendedModes.join("\n• ")).arg(imgWidth).arg(imgHeight).arg(imgDepth);
            } else {
                recommendedModes << "🖼️ 2D切片查看";
                infoText += "💡 推荐查看模式:\n";
                infoText += QString("• %1 (适合2D数据: %2×%3)\n").arg(recommendedModes.join("\n• ")).arg(imgWidth).arg(imgHeight);
            }
            
            // 根据显示模式渲染图像
            if (m_currentDisplayMode == "2D") {
                render2DImage(imageId, infoText);
            } else if (m_currentDisplayMode == "3D") {
                render3DImage(imageId, infoText);  // 3D体绘制统一在这里处理
            } else if (m_currentDisplayMode == "MPR") {
                renderMPRImage(imageId, infoText);
        } else if (m_currentDisplayMode == "Advanced") {
                renderAdvancedVisualization(imageId, infoText);
            } else {
                // 默认2D显示
                render2DImage(imageId, infoText);
            }
        }
        
    } catch (const std::exception& e) {
        updateStatus(QString("显示图像时发生错误: %1").arg(e.what()));
        qWarning() << "[MedicalViewerWidget] displayImageAsync异常:" << e.what();
    } catch (...) {
        updateStatus("显示图像时发生未知错误");
        qWarning() << "[MedicalViewerWidget] displayImageAsync发生未知异常";
    }
    
    // 重新启用图像选择器
    m_imageSelector->setEnabled(true);
    
    // 记录到日志
    m_logTextEdit->append(QString("[%1] 图像显示完成: %2")
                         .arg(QTime::currentTime().toString())
                         .arg(imageId.left(8) + "..."));
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::setDisplayMode(const QString& mode)
{
    if (m_currentDisplayMode == mode) {
        return;
    }
    
    m_currentDisplayMode = mode;
    
    // 切换到相应的标签页
    if (mode == "2D") {
        m_displayTabs->setCurrentWidget(m_2dViewerTab);
    } else if (mode == "3D") {
        m_displayTabs->setCurrentWidget(m_3dViewerTab);
    } else if (mode == "MPR") {
        m_displayTabs->setCurrentWidget(m_mprViewerTab);
    } else if (mode == "Volume") {
        m_displayTabs->setCurrentWidget(m_volumeRenderingTab);
        // 注意：原右侧控制面板已移除，体绘制控制现在集成在体绘制标签页内
    } else if (mode == "Advanced") {
        m_displayTabs->setCurrentWidget(m_advancedVisualizationTab);
        // 注意：原右侧控制面板已移除，高级控制现在集成在各标签页内
    }
    
    updateStatus(QString("切换到%1显示模式").arg(mode));
    
    // 如果有当前图像，重新显示
    if (!m_currentImageId.isEmpty()) {
        displayImage(m_currentImageId);
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::refreshImageList()
{
    updateStatus("刷新图像列表...");
    qDebug() << "[MedicalViewerWidget] 开始刷新图像列表";
    
    // 安全检查
    if (!m_imageSelector) {
        qWarning() << "[MedicalViewerWidget] m_imageSelector 为空";
        return;
    }
    
    if (!m_imageService) {
        qDebug() << "[MedicalViewerWidget] 图像服务未连接";
        m_imageSelector->clear();
        m_imageSelector->addItem("❌ 图像服务未连接", "");
        m_imageSelector->addItem("💡 请检查插件状态", "");
        updateStatus("图像服务未连接");
        return;
    }
    
    try {
        // 强制重新获取服务实例，确保使用正确的服务（CTK标准方式）
        ctkServiceReference freshServiceRef = m_pluginContext->getServiceReference("medical.MedicalImageCoreService");
        if (freshServiceRef) {
            QObject* freshService = m_pluginContext->getService(freshServiceRef);
            if (freshService) {
                qDebug() << "[MedicalViewerWidget] 🔄 使用刷新的服务实例:" << freshService;
                qDebug() << "[MedicalViewerWidget] 🔄 原服务实例:" << m_imageService;
                m_imageService = freshService; // 更新服务实例
            }
        }
        
        qDebug() << "[MedicalViewerWidget] 🔍 即将调用 getLoadedImages() (直接调用方式)";
        qDebug() << "[MedicalViewerWidget] 服务对象指针:" << m_imageService;
        qDebug() << "[MedicalViewerWidget] 服务对象类型:" << m_imageService->metaObject()->className();
        
        QStringList imageIds;
        
        // 🔧 使用辅助函数直接调用（避免QMetaObject::invokeMethod，因为方法没有Q_INVOKABLE标签）
        if (auto coreService = getImageCoreService()) {
            qDebug() << "[MedicalViewerWidget] ✅ 成功获取MedicalImageCoreService";
            imageIds = coreService->getLoadedImages();
            qDebug() << "[MedicalViewerWidget] 直接调用结果:" << imageIds << "数量:" << imageIds.size();
        } else {
            qDebug() << "[MedicalViewerWidget] ❌ 无法获取MedicalImageCoreService";
        }
        
        // 过滤和验证图像ID
        QStringList validImageIds;
        for (const QString& imageId : imageIds) {
            // 跳过明显的测试数据或无效数据
            if (imageId.isEmpty() || 
                imageId.contains("Unknown") || 
                imageId.startsWith("DICOM") || 
                imageId.startsWith("NRRD") || 
                imageId.startsWith("NIfTI") ||
                imageId.startsWith("PNG") ||
                imageId.startsWith("JPEG") ||
                imageId.startsWith("TIFF") ||
                imageId.startsWith("BMP") ||
                imageId.startsWith("MetaImage")) {
                qDebug() << "[MedicalViewerWidget] 跳过无效ID:" << imageId;
                continue;
            }
            
            // 验证图像是否有效（CTK方式）
            bool isValid = false;
            if (auto coreService = getImageCoreService()) {
                isValid = coreService->isValid(imageId);
            }
            if (isValid) {
                validImageIds.append(imageId);
                qDebug() << "[MedicalViewerWidget] 有效图像ID:" << imageId;
            } else {
                qDebug() << "[MedicalViewerWidget] 无效图像ID:" << imageId;
            }
        }
        
        qDebug() << "[MedicalViewerWidget] 过滤后的有效图像ID列表:" << validImageIds << "数量:" << validImageIds.size();
        
        m_imageSelector->clear();
        m_imageSelector->addItem("请选择图像...", "");
        
        if (validImageIds.isEmpty()) {
            qDebug() << "[MedicalViewerWidget] 警告：没有找到有效的已加载图像";
            
            m_imageSelector->addItem("⚠️ 暂无有效图像数据", "");
            m_imageSelector->addItem("💡 请先在医学图像管理器中加载图像", "");
            m_imageSelector->addItem("🔄 然后点击上方的「刷新图像」按钮", "");
            updateStatus("暂无图像数据 - 请先在管理器中加载图像");
            return;
        }
        
        for (const QString& imageId : validImageIds) {
            QString displayName = imageId;
            
            if (auto coreService = getImageCoreService()) {
                QVariantMap info = coreService->getImageDetails(imageId);
                if (!info.isEmpty()) {
                    // 构建详细的显示名称
                    QString format = info.value("format", "Unknown").toString();
                    QString filename = info.value("filename", imageId).toString();
                    QVariantMap dimensions = info.value("dimensions", QVariantMap()).toMap();
                    
                    // 获取简短的图像ID（前8位）
                    QString shortId = imageId.left(8);
                    if (imageId.length() > 8) {
                        shortId += "...";
                    }
                    
                    // 构建维度信息
                    QString dimStr = "";
                    QList<int> dimensionsList = info.value("dimensions").value<QList<int>>();
                    if (!dimensionsList.isEmpty()) {
                        int imgWidth = dimensionsList.size() > 0 ? dimensionsList[0] : 0;
                        int imgHeight = dimensionsList.size() > 1 ? dimensionsList[1] : 0;
                        int imgDepth = dimensionsList.size() > 2 ? dimensionsList[2] : 1;
                        if (imgDepth > 1) {
                            dimStr = QString("%1×%2×%3").arg(imgWidth).arg(imgHeight).arg(imgDepth);
                        } else {
                            dimStr = QString("%1×%2").arg(imgWidth).arg(imgHeight);
                        }
                    }
                    
                    // 最终显示格式：短ID [格式] (维度)
                    if (!dimStr.isEmpty()) {
                        displayName = QString("%1 [%2] (%3)").arg(shortId).arg(format).arg(dimStr);
    } else {
                        displayName = QString("%1 [%2]").arg(shortId).arg(format);
                    }
                } else {
                    // 如果没有详细信息，显示简短ID和格式
                    QString shortId = imageId.left(8);
                    if (imageId.length() > 8) {
                        shortId += "...";
                    }
                    displayName = QString("%1 [Unknown]").arg(shortId);
                }
            }
            
            m_imageSelector->addItem(displayName, imageId);
            qDebug() << "[MedicalViewerWidget] 添加图像:" << displayName << "ID:" << imageId;
        }
        
        updateStatus(QString("已加载 %1 个有效图像").arg(validImageIds.size()));
        
    } catch (const std::exception& e) {
        qWarning() << "[MedicalViewerWidget] 刷新图像列表时发生异常:" << e.what();
        m_imageSelector->clear();
        m_imageSelector->addItem("❌ 刷新失败", "");
        m_imageSelector->addItem("💡 请重试或重启应用程序", "");
        updateStatus("刷新图像列表失败");
    } catch (...) {
        qWarning() << "[MedicalViewerWidget] 刷新图像列表时发生未知异常";
        m_imageSelector->clear();
        m_imageSelector->addItem("❌ 刷新失败", "");
        m_imageSelector->addItem("💡 请重试或重启应用程序", "");
        updateStatus("刷新图像列表失败");
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::resetAllViewers()
{
    updateStatus("重置所有查看器...");
    
    if (m_viewerService) {
        for (const QString& viewerId : m_activeViewerIds) {
            m_viewerService->resetView(viewerId);
        }
    }
    
    // 注意：原右侧控制面板组件已移除，重置功能已整合到各查看器标签页内
    
    updateStatus("所有查看器已重置");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::initializeUI()
{
    // 创建主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(6);
    
    // 创建工具栏
    createMainToolBar();
    m_mainLayout->addWidget(m_mainToolBar);
    
    // 创建内容区域
    m_contentLayout = new QHBoxLayout();
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 创建显示区域和控制面板
    createDisplayArea();
    createControlPanels();
    
    // 设置分割器
    m_mainSplitter->addWidget(m_displayTabs);
    m_mainSplitter->addWidget(m_controlTabs);
    m_mainSplitter->setStretchFactor(0, 3);  // 显示区域占3/4
    m_mainSplitter->setStretchFactor(1, 1);  // 控制面板占1/4
    
    m_contentLayout->addWidget(m_mainSplitter);
    m_mainLayout->addLayout(m_contentLayout);
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::createMainToolBar()
{
    // 创建主工具栏容器
    QWidget* toolbarContainer = new QWidget();
    QVBoxLayout* containerLayout = new QVBoxLayout(toolbarContainer);
    containerLayout->setSpacing(5);
    containerLayout->setContentsMargins(5, 5, 5, 5);
    
    // === 第一行：图像数据选择区域 ===
    QFrame* imageFrame = new QFrame();
    imageFrame->setFrameStyle(QFrame::StyledPanel);
    imageFrame->setStyleSheet("QFrame { background-color: #e8f4fd; padding: 8px; border-radius: 6px; border: 1px solid #b3d9ff; }");
    QHBoxLayout* imageLayout = new QHBoxLayout(imageFrame);
    
    QLabel* imageIcon = new QLabel("📋");
    imageIcon->setStyleSheet("font-size: 16px;");
    imageLayout->addWidget(imageIcon);
    
    QLabel* imageLabel = new QLabel("图像数据源:");
    imageLabel->setStyleSheet("font-weight: bold; color: #2c5aa0;");
    imageLayout->addWidget(imageLabel);
    
    m_imageSelector = new QComboBox();
    m_imageSelector->setMinimumWidth(350);
    m_imageSelector->setStyleSheet(R"(
        QComboBox {
            padding: 6px 12px;
            border: 2px solid #4CAF50;
            border-radius: 4px;
            background-color: white;
            font-weight: bold;
        }
        QComboBox:focus {
            border-color: #45a049;
        }
    )");
    m_imageSelector->setToolTip("选择要查看的图像数据（从管理器加载的图像）");
    m_imageSelector->addItem("请先在管理器中加载图像数据...", "");
    imageLayout->addWidget(m_imageSelector);
    
    QPushButton* refreshImageBtn = new QPushButton("🔄 刷新图像");
    refreshImageBtn->setToolTip("从图像管理器刷新可用图像列表");
    refreshImageBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 6px 12px; border: none; border-radius: 4px; font-weight: bold; }");
    imageLayout->addWidget(refreshImageBtn);
    
    imageLayout->addStretch();
    containerLayout->addWidget(imageFrame);
    
    // === 第二行：显示模式选择区域 ===
    QFrame* modeFrame = new QFrame();
    modeFrame->setFrameStyle(QFrame::StyledPanel);
    modeFrame->setStyleSheet("QFrame { background-color: #fff8e1; padding: 8px; border-radius: 6px; border: 1px solid #ffcc80; }");
    QHBoxLayout* modeLayout = new QHBoxLayout(modeFrame);
    
    QLabel* modeIcon = new QLabel("🔍");
    modeIcon->setStyleSheet("font-size: 16px;");
    modeLayout->addWidget(modeIcon);
    
    QLabel* modeLabel = new QLabel("显示模式:");
    modeLabel->setStyleSheet("font-weight: bold; color: #e65100;");
    modeLayout->addWidget(modeLabel);
    
    m_displayModeCombo = new QComboBox();
    m_displayModeCombo->addItem("2D切片查看器", "2D");
    m_displayModeCombo->addItem("3D体绘制", "3D");
    m_displayModeCombo->addItem("MPR多平面重建", "MPR");
    m_displayModeCombo->addItem("科研级可视化", "Advanced");
    m_displayModeCombo->setCurrentIndex(0);
    m_displayModeCombo->setMinimumWidth(200);
    m_displayModeCombo->setStyleSheet(R"(
        QComboBox {
            padding: 6px 12px;
            border: 2px solid #FF9800;
            border-radius: 4px;
            background-color: white;
            font-weight: bold;
        }
    )");
    m_displayModeCombo->setToolTip("选择图像显示和渲染方式");
    modeLayout->addWidget(m_displayModeCombo);
    
    modeLayout->addStretch();
    containerLayout->addWidget(modeFrame);
    
    // === 第三行：操作工具栏 ===
    QToolBar* operationsToolBar = new QToolBar("操作工具栏");
    operationsToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    operationsToolBar->setStyleSheet("QToolBar { border: 1px solid #ddd; background-color: #f9f9f9; padding: 4px; }");
    
    // 主要操作按钮
    QPushButton* loadDisplayBtn = new QPushButton("▶️ 加载显示");
    loadDisplayBtn->setToolTip("使用选定模式显示选中的图像");
    loadDisplayBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #4CAF50;
            color: white;
            font-weight: bold;
            padding: 8px 16px;
            border: none;
            border-radius: 6px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #45a049;
        }
        QPushButton:pressed {
            background-color: #3d8b40;
        }
    )");
    operationsToolBar->addWidget(loadDisplayBtn);
    
    operationsToolBar->addSeparator();
    
    // 其他操作按钮
    m_resetAction = operationsToolBar->addAction("🔄 重置", this, &MedicalViewerWidget::resetAllViewers);
    m_screenshotAction = operationsToolBar->addAction("📷 截图", this, &MedicalViewerWidget::onScreenshot);
    m_exportAction = operationsToolBar->addAction("💾 导出", this, &MedicalViewerWidget::onExportData);
    
    operationsToolBar->addSeparator();
    m_settingsAction = operationsToolBar->addAction("⚙️ 设置", [this]() {
        QMessageBox::information(this, "设置", "查看器设置界面\n\n即将实现的功能");
    });
    
    containerLayout->addWidget(operationsToolBar);
    
    // 创建最终的主工具栏并添加容器
    m_mainToolBar = new QToolBar();
    m_mainToolBar->addWidget(toolbarContainer);
    
    // 连接信号
    connect(refreshImageBtn, &QPushButton::clicked, this, &MedicalViewerWidget::refreshImageList);
    connect(loadDisplayBtn, &QPushButton::clicked, this, &MedicalViewerWidget::onLoadDisplayClicked);
    connect(m_imageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MedicalViewerWidget::onImageSelectionChanged);
    connect(m_displayModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MedicalViewerWidget::onDisplayModeChanged);
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::createDisplayArea()
{
    m_displayTabs = new QTabWidget();
    m_displayTabs->setTabPosition(QTabWidget::North);
    
    // 创建各种查看器标签页
    m_2dViewerTab = create2DViewerTab();
    m_3dViewerTab = create3DViewerTab();
    m_mprViewerTab = createMPRViewerTab();
    m_volumeRenderingTab = createVolumeRenderingTab();
    m_advancedVisualizationTab = createAdvancedVisualizationTab();
    
    m_displayTabs->addTab(m_2dViewerTab, "2D 查看器");
    m_displayTabs->addTab(m_3dViewerTab, "3D 体绘制");
    m_displayTabs->addTab(m_mprViewerTab, "MPR 重建");
    m_displayTabs->addTab(m_advancedVisualizationTab, "科研级可视化");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::createControlPanels()
{
    // 优化UI设计：移除重复的控制面板，只保留状态信息面板
    // 所有具体的控制功能都整合到左侧的各个查看器标签页中
    m_statusPanel = createStatusPanel();
    
    // 直接使用状态面板，不需要标签页容器
    // 这样界面更简洁，避免功能重复
    m_controlTabs = m_statusPanel;
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::create2DViewerTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // 创建2D显示区域
    QLabel* displayArea = new QLabel("2D医学图像显示区域");
    displayArea->setAlignment(Qt::AlignCenter);
    displayArea->setMinimumHeight(400);
    displayArea->setStyleSheet(R"(
        QLabel {
            border: 2px dashed #6c757d;
            border-radius: 8px;
            background-color: #f8f9fa;
            color: #6c757d;
            font-size: 16px;
            font-weight: bold;
        }
    )");
    
    layout->addWidget(displayArea);
    
    // 添加2D特有的控制工具
    QHBoxLayout* toolLayout = new QHBoxLayout();
    
    QPushButton* axialBtn = new QPushButton("轴状面");
    QPushButton* coronalBtn = new QPushButton("冠状面");
    QPushButton* sagittalBtn = new QPushButton("矢状面");
    
    toolLayout->addWidget(axialBtn);
    toolLayout->addWidget(coronalBtn);
    toolLayout->addWidget(sagittalBtn);
    toolLayout->addStretch();
    
    layout->addLayout(toolLayout);
    
    return tab;
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::create3DViewerTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // 创建3D显示区域
    QLabel* displayArea = new QLabel("3D医学图像显示区域\n\n支持交互旋转、缩放、平移");
    displayArea->setAlignment(Qt::AlignCenter);
    displayArea->setMinimumHeight(400);
    displayArea->setStyleSheet(R"(
        QLabel {
            border: 2px dashed #007bff;
            border-radius: 8px;
            background-color: #e7f3ff;
            color: #007bff;
            font-size: 16px;
            font-weight: bold;
        }
    )");
    
    layout->addWidget(displayArea);
    
    // 添加3D控制工具
    QHBoxLayout* toolLayout = new QHBoxLayout();
    
    QPushButton* rotateBtn = new QPushButton("旋转模式");
    QPushButton* zoomBtn = new QPushButton("缩放模式");
    QPushButton* panBtn = new QPushButton("平移模式");
    QPushButton* resetCameraBtn = new QPushButton("重置视角");
    
    toolLayout->addWidget(rotateBtn);
    toolLayout->addWidget(zoomBtn);
    toolLayout->addWidget(panBtn);
    toolLayout->addWidget(resetCameraBtn);
    toolLayout->addStretch();
    
    layout->addLayout(toolLayout);
    
    return tab;
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::createMPRViewerTab()
{
    QWidget* tab = new QWidget();
    QGridLayout* layout = new QGridLayout(tab);
    
    // 创建四象限MPR显示
    QStringList viewNames = {"轴状面", "冠状面", "矢状面", "3D重建"};
    QStringList colors = {"#dc3545", "#28a745", "#ffc107", "#007bff"};
    
    for (int i = 0; i < 4; ++i) {
        QLabel* viewArea = new QLabel(viewNames[i] + "\n\nMPR显示区域");
        viewArea->setAlignment(Qt::AlignCenter);
        viewArea->setMinimumHeight(200);
        viewArea->setStyleSheet(QString(R"(
            QLabel {
                border: 2px solid %1;
                border-radius: 8px;
                background-color: #f8f9fa;
                color: %1;
                font-size: 14px;
                font-weight: bold;
            }
        )").arg(colors[i]));
        
        layout->addWidget(viewArea, i/2, i%2);
    }
    
    return tab;
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::createVolumeRenderingTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // 创建体绘制显示区域
    QLabel* displayArea = new QLabel("高级体绘制显示区域\n\n支持传输函数、光照控制、材质设置");
    displayArea->setAlignment(Qt::AlignCenter);
    displayArea->setMinimumHeight(400);
    displayArea->setStyleSheet(R"(
        QLabel {
            border: 2px solid #6f42c1;
            border-radius: 8px;
            background-color: #f8f5ff;
            color: #6f42c1;
            font-size: 16px;
            font-weight: bold;
        }
    )");
    
    layout->addWidget(displayArea);
    
    // 添加体绘制快速控制
    QHBoxLayout* quickControlLayout = new QHBoxLayout();
    
    QPushButton* presetBtn1 = new QPushButton("骨骼预设");
    QPushButton* presetBtn2 = new QPushButton("软组织预设");
    QPushButton* presetBtn3 = new QPushButton("血管预设");
    QPushButton* customBtn = new QPushButton("自定义设置");
    
    quickControlLayout->addWidget(presetBtn1);
    quickControlLayout->addWidget(presetBtn2);
    quickControlLayout->addWidget(presetBtn3);
    quickControlLayout->addWidget(customBtn);
    quickControlLayout->addStretch();
    
    layout->addLayout(quickControlLayout);
    
    return tab;
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::createAdvancedVisualizationTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // 创建科研级可视化显示区域
    QLabel* displayArea = new QLabel("科研级可视化显示区域\n\n支持数据分析、统计、高级算法");
    displayArea->setAlignment(Qt::AlignCenter);
    displayArea->setMinimumHeight(300);
    displayArea->setStyleSheet(R"(
        QLabel {
            border: 2px solid #fd7e14;
            border-radius: 8px;
            background-color: #fff5f0;
            color: #fd7e14;
            font-size: 16px;
            font-weight: bold;
        }
    )");
    
    layout->addWidget(displayArea);
    
    // 添加科研工具
    QHBoxLayout* researchToolLayout = new QHBoxLayout();
    
    QPushButton* analyzeBtn = new QPushButton("数据分析");
    QPushButton* statisticsBtn = new QPushButton("统计信息");
    QPushButton* algorithmBtn = new QPushButton("高级算法");
    QPushButton* exportBtn = new QPushButton("数据导出");
    
    researchToolLayout->addWidget(analyzeBtn);
    researchToolLayout->addWidget(statisticsBtn);
    researchToolLayout->addWidget(algorithmBtn);
    researchToolLayout->addWidget(exportBtn);
    researchToolLayout->addStretch();
    
    layout->addLayout(researchToolLayout);
    
    // 添加统计信息显示区域
    QTextEdit* statsDisplay = new QTextEdit();
    statsDisplay->setPlainText("等待数据分析...\n\n将显示：\n- 体数据统计\n- 密度分布\n- 梯度分析\n- 组织分类");
    statsDisplay->setMaximumHeight(150);
    layout->addWidget(statsDisplay);
    
    return tab;
}

// 注意：原 createBasicControlPanel() 函数已移除
// 基本控制功能已整合到各个查看器标签页内，避免界面重复

// 注意：原 createVolumeControlPanel() 函数已移除
// 体绘制控制功能已整合到3D体绘制标签页内，避免界面重复

// 注意：原 createMeasurementPanel() 函数已移除
// 测量工具功能已整合到各个查看器标签页内，避免界面重复

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::createStatusPanel()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    // 状态信息
    QGroupBox* statusGroup = new QGroupBox("状态信息");
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);
    
    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("QLabel { color: #28a745; font-weight: bold; }");
    
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addWidget(m_progressBar);
    
    layout->addWidget(statusGroup);
    
    // 日志信息
    QGroupBox* logGroup = new QGroupBox("操作日志");
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    
    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setMaximumHeight(200);
    m_logTextEdit->append("多模式医学图像查看器已启动");
    
    logLayout->addWidget(m_logTextEdit);
    
    layout->addWidget(logGroup);
    
    layout->addStretch();
    
    return panel;
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::setupStyles()
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
        
        QTabWidget::tab-bar {
            alignment: center;
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
        
        QComboBox, QSpinBox, QDoubleSpinBox {
            border: 1px solid #ced4da;
            border-radius: 4px;
            padding: 6px 8px;
            background-color: white;
            min-width: 100px;
        }
        
        QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #007bff;
        }
        
        QTextEdit {
            border: 1px solid #dee2e6;
            border-radius: 4px;
            background-color: white;
            font-family: "Consolas", monospace;
            font-size: 12px;
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
        
        QToolBar QLabel {
            color: #495057;
            font-weight: 500;
            margin: 0 4px;
        }
    )");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::connectSignals()
{
    // 显示模式切换
    connect(m_displayModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MedicalViewerWidget::onDisplayModeChanged);
    
    // 图像选择
    connect(m_imageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MedicalViewerWidget::onImageSelectionChanged);
    
    // 注意：原右侧控制面板的信号连接已移除
    // 所有控制信号连接现在都在各个查看器标签页的创建函数中单独处理
    // 这样避免了界面功能重复，让控制更加直观和集中
    
    // 注意：原右侧控制面板的信号连接已完全移除
    // 所有控制功能现在都在各个查看器标签页内独立实现
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::initializeServiceConnections()
{
    if (!m_pluginContext) {
        qWarning() << "[MedicalViewerWidget] CTK插件上下文未设置";
        return;
    }
    
    // 获取查看器服务
    ctkServiceReference viewerServiceRef = m_pluginContext->getServiceReference<MedicalViewerService>();
    if (viewerServiceRef) {
        m_viewerService = qobject_cast<MedicalViewerService*>(
            m_pluginContext->getService(viewerServiceRef));
        
        if (m_viewerService) {
            qDebug() << "[MedicalViewerWidget] MedicalViewerService连接成功";
            onViewerServiceAvailable();
        }
    }
    
    // 获取图像服务（CTK标准方式）
    ctkServiceReference imageServiceRef = m_pluginContext->getServiceReference("medical.MedicalImageCoreService");
    if (imageServiceRef) {
        m_imageService = m_pluginContext->getService(imageServiceRef);
        
        if (m_imageService) {
            qDebug() << "[MedicalViewerWidget] MedicalImageCoreService连接成功";
            qDebug() << "[MedicalViewerWidget] 服务实例地址:" << m_imageService;
            
            // 立即测试获取图像列表（直接调用方式）
            QStringList testImages;
            if (auto coreService = getImageCoreService()) {
                testImages = coreService->getLoadedImages();
                qDebug() << "[MedicalViewerWidget] 初始化时直接调用成功";
            } else {
                qDebug() << "[MedicalViewerWidget] 初始化时直接调用失败：无法转换类型";
            }
            qDebug() << "[MedicalViewerWidget] 立即测试获取图像列表:" << testImages << "数量:" << testImages.size();
            
            onImageServiceAvailable();
        } else {
            qDebug() << "[MedicalViewerWidget] MedicalImageCoreService转换失败";
        }
    } else {
        qDebug() << "[MedicalViewerWidget] 未找到MedicalImageCoreService服务引用";
    }
    
    m_serviceConnected = (m_viewerService != nullptr && m_imageService != nullptr);
    
    qDebug() << "[MedicalViewerWidget] 服务连接状态 - ViewerService:" << (m_viewerService != nullptr) 
             << "ImageService:" << (m_imageService != nullptr);
    
    if (m_serviceConnected) {
        updateStatus("查看器已就绪 - 正在从管理器读取图像数据...");
        qDebug() << "[MedicalViewerWidget] 所有服务连接成功，开始刷新图像列表";
        refreshImageList();
    } else {
        updateStatus("等待连接到图像管理器...");
        qDebug() << "[MedicalViewerWidget] 警告：服务连接不完整";
        if (!m_viewerService) qDebug() << "[MedicalViewerWidget] - MedicalViewerService未连接";
        if (!m_imageService) qDebug() << "[MedicalViewerWidget] - MedicalImageCoreService未连接";
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onDisplayModeChanged()
{
    QString mode = m_displayModeCombo->currentData().toString();
    setDisplayMode(mode);
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onImageSelectionChanged()
{
    // 安全检查，防止崩溃
    if (!m_imageSelector) {
        qWarning() << "[MedicalViewerWidget] m_imageSelector 为空";
        return;
    }
    
    // 图像选择变化时仅更新状态，不自动加载显示
    QString imageId = m_imageSelector->currentData().toString();
    QString currentText = m_imageSelector->currentText();
    
    qDebug() << "[MedicalViewerWidget] 图像选择变化 - imageId:" << imageId << "currentText:" << currentText;
    
    // 如果是空选项或提示文本，跳过处理
    if (imageId.isEmpty() || currentText.contains("请选择") || currentText.contains("请先") || 
        currentText.contains("暂无") || currentText.contains("Unknown")) {
        updateStatus("请选择有效的图像数据");
        return;
    }
    
    // 检查服务状态
    if (!m_imageService) {
        qWarning() << "[MedicalViewerWidget] m_imageService 为空";
        updateStatus("图像服务未连接");
        return;
    }
    
    // 验证图像ID（CTK方式）
    bool isValid = false;
    if (auto coreService = getImageCoreService()) {
        isValid = coreService->isValid(imageId);
    }
    if (!isValid) {
        qWarning() << "[MedicalViewerWidget] 无效的图像ID:" << imageId;
        updateStatus("所选图像无效");
        return;
    }
    
    try {
        QVariantMap imageInfo;
        if (auto coreService = getImageCoreService()) {
            imageInfo = coreService->getImageDetails(imageId);
        }
        QString format = imageInfo.value("format", "Unknown").toString();
        updateStatus(QString("已选择图像: %1 [%2] - 点击「▶️ 加载显示」按钮开始查看")
                    .arg(imageId.left(8) + "...")
                    .arg(format));
        
        // 记录操作日志
        if (m_logTextEdit) {
        m_logTextEdit->append(QString("[%1] 选择图像: %2")
                             .arg(QTime::currentTime().toString())
                                 .arg(currentText));
        }
    } catch (const std::exception& e) {
        qWarning() << "[MedicalViewerWidget] 获取图像信息时发生异常:" << e.what();
        updateStatus("获取图像信息失败");
    } catch (...) {
        qWarning() << "[MedicalViewerWidget] 获取图像信息时发生未知异常";
        updateStatus("获取图像信息失败");
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onLoadDisplayClicked()
{
    // 安全检查
    if (!m_imageSelector || !m_displayModeCombo) {
        qWarning() << "[MedicalViewerWidget] UI组件为空";
        QMessageBox::warning(this, "错误", "界面组件未正确初始化");
        return;
    }
    
    QString imageId = m_imageSelector->currentData().toString();
    QString displayMode = m_displayModeCombo->currentData().toString();
    QString currentText = m_imageSelector->currentText();
    
    qDebug() << "[MedicalViewerWidget] 加载显示点击 - imageId:" << imageId << "displayMode:" << displayMode << "currentText:" << currentText;
    
    // 检查是否选择了有效图像
    if (imageId.isEmpty() || currentText.contains("请选择") || currentText.contains("请先") || 
        currentText.contains("暂无") || currentText.contains("Unknown")) {
        QMessageBox::information(this, "提示", 
            "请先选择要查看的图像数据\n\n"
            "💡 如果列表为空，请先在医学图像管理器中加载图像");
        return;
    }
    
    // 检查服务状态
    if (!m_imageService) {
        qWarning() << "[MedicalViewerWidget] m_imageService 为空";
        QMessageBox::warning(this, "错误", 
            "图像服务未连接\n\n"
            "请检查插件状态或重启应用程序");
        return;
    }
    
    // 验证图像ID（CTK方式）
    bool isValid = false;
    if (auto coreService = getImageCoreService()) {
        isValid = coreService->isValid(imageId);
    }
    if (!isValid) {
        qWarning() << "[MedicalViewerWidget] 无效的图像ID:" << imageId;
        QMessageBox::warning(this, "错误", 
            "所选图像数据无效或已被释放\n\n"
            "请刷新图像列表或重新选择");
        return;
    }
    
    // 防止重复选择同一图像
    if (imageId == m_currentImageId) {
        updateStatus("图像已经在显示中");
        return;
    }
    
    // 禁用按钮防止重复点击
    QWidget* loadBtn = qobject_cast<QWidget*>(sender());
    if (loadBtn) {
        loadBtn->setEnabled(false);
    }
    
    // 更新状态并开始加载
    updateStatus(QString("正在使用 %1 模式加载图像: %2...")
                .arg(displayMode)
                .arg(imageId.left(8) + "..."));
    
    // 异步加载显示，避免UI卡死
    m_imageSelector->setEnabled(false);  // 暂时禁用选择器
    
    QTimer::singleShot(100, this, [this, imageId, displayMode, loadBtn]() {
        displayImageAsync(imageId);
        
        // 重新启用控件
        if (loadBtn) {
            loadBtn->setEnabled(true);
        }
        m_imageSelector->setEnabled(true);
        
        QVariantMap imageInfo;
        if (auto coreService = getImageCoreService()) {
            imageInfo = coreService->getImageDetails(imageId);
        }
        QString format = imageInfo.value("format", "Unknown").toString();
        updateStatus(QString("图像已加载: %1 [%2] - %3 模式")
                    .arg(imageId.left(8) + "...")
                    .arg(format)
                    .arg(displayMode));
        
        // 记录操作日志
        m_logTextEdit->append(QString("[%1] 加载显示图像: %2 (%3 模式)")
                             .arg(QTime::currentTime().toString())
                             .arg(imageId.left(8) + "...")
                             .arg(displayMode));
    });
}

//-----------------------------------------------------------------------------  
void MedicalViewerWidget::onWindowLevelChanged()
{
    // 注意：原右侧控制面板已移除，窗宽窗位控制现在集成在2D查看器标签页内
    // 该函数保留以避免编译错误，但实际功能已移动到各查看器内部
    updateStatus("窗宽窗位功能已集成到2D查看器标签页中");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onSliceChanged()
{
    // 注意：原右侧控制面板已移除，切片控制现在集成在MPR查看器标签页内
    updateStatus("切片控制功能已集成到MPR查看器标签页中");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onZoomChanged()
{
    // 注意：原右侧控制面板已移除，缩放控制现在集成在各查看器标签页内
    updateStatus("缩放控制功能已集成到各查看器标签页中");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onMPRSliceChanged(int sliceIndex)
{
    // 获取发出信号的控件
    QObject* sender = QObject::sender();
    if (!sender) return;
    
    // 查找对应的MPR视图信息
    MedicalViewerWidget::MPRViewInfo* targetView = nullptr;
    for (MedicalViewerWidget::MPRViewInfo* viewInfo : m_mprViews) {
        if (viewInfo->sliceSlider == sender || viewInfo->sliceSpinBox == sender) {
            targetView = viewInfo;
            break;
        }
    }
    
    if (!targetView) {
        qDebug() << "[MedicalViewerWidget] ❌ 无法找到对应的MPR视图信息";
        return;
    }
    
    // 使用互斥锁保护临界区
    QMutexLocker locker(targetView->taskMutex);
    
    // 更新当前切片索引和待处理切片
    targetView->currentSlice = sliceIndex;
    targetView->pendingSlice = sliceIndex;
    
    qDebug() << "[MedicalViewerWidget] 🔄 MPR切片变化:" << targetView->viewType 
             << "切片:" << sliceIndex << "/" << targetView->totalSlices;
    
    // 停止之前的防抖动定时器
    targetView->debounceTimer->stop();
    
    // 如果当前没有任务在运行，立即处理
    if (!targetView->taskRunning) {
        regenerateMPRSlice(targetView);
    } else {
        // 否则启动防抖动定时器，延迟处理
        // 断开之前的连接避免重复
        disconnect(targetView->debounceTimer, &QTimer::timeout, nullptr, nullptr);
        connect(targetView->debounceTimer, &QTimer::timeout, [this, targetView]() {
            QMutexLocker locker(targetView->taskMutex);
            if (!targetView->taskRunning && targetView->pendingSlice >= 0) {
                targetView->currentSlice = targetView->pendingSlice;
                targetView->pendingSlice = -1;
                regenerateMPRSlice(targetView);
            }
        });
        targetView->debounceTimer->start();
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::regenerateMPRSlice(MedicalViewerWidget::MPRViewInfo* viewInfo)
{
    if (!viewInfo || !viewInfo->imageLabel) {
        qDebug() << "[MedicalViewerWidget] ❌ 无效的MPR视图信息";
        return;
    }
    
    // 检查任务状态，如果已经在运行则跳过
    if (viewInfo->taskRunning) {
        qDebug() << "[MedicalViewerWidget] ⚠️ 任务已在运行，跳过重复请求:" << viewInfo->viewType;
        return;
    }
    
    // 设置任务运行状态
    viewInfo->taskRunning = true;
    
    // 取消之前的异步任务（避免多个任务同时运行）
    if (viewInfo->currentWatcher) {
        qDebug() << "[MedicalViewerWidget] 🛑 取消之前的切片生成任务";
        viewInfo->currentWatcher->cancel();
        viewInfo->currentWatcher->deleteLater();
        viewInfo->currentWatcher = nullptr;
    }
    
    // 获取图像核心服务
    auto coreService = this->getImageCoreService();
    if (!coreService) {
        qDebug() << "[MedicalViewerWidget] ❌ 无法获取图像核心服务";
        // 重置任务状态
        viewInfo->taskRunning = false;
        return;
    }
    
    qDebug() << "[MedicalViewerWidget] 🔄 开始重新生成MPR切片:" << viewInfo->viewType 
             << "切片:" << viewInfo->currentSlice;
    
    // 不显示加载指示器，保持界面清洁，直接开始异步更新
    
    // 异步生成新切片
    auto future = QtConcurrent::run([coreService, viewInfo]() -> QPixmap {
        // 获取图像数据
        void* pixelData = coreService->getImagePixelData(viewInfo->imageId);
        QString dataType = coreService->getImageDataType(viewInfo->imageId);
        QList<int> dimensions = coreService->getImageDimensions(viewInfo->imageId);
        
        qDebug() << "[AsyncMPR-Update] 数据类型:" << dataType;
        qDebug() << "[AsyncMPR-Update] 维度:" << dimensions;
        qDebug() << "[AsyncMPR-Update] 切片索引:" << viewInfo->currentSlice;
        
        if (!pixelData || dimensions.size() < 3) {
            qDebug() << "[AsyncMPR-Update] ❌ 无效的图像数据";
            return QPixmap();
        }
        
        int width = dimensions[0];
        int height = dimensions[1];
        int depth = dimensions[2];
        
        // 计算实际切片索引（1-based转0-based）
        int sliceIndex = viewInfo->currentSlice - 1;
        
        // 根据视图类型调整切片索引
        if (viewInfo->viewType == "axial") {
            sliceIndex = qBound(0, sliceIndex, depth - 1);
        } else if (viewInfo->viewType == "coronal") {
            sliceIndex = qBound(0, sliceIndex, height - 1);
        } else if (viewInfo->viewType == "sagittal") {
            sliceIndex = qBound(0, sliceIndex, width - 1);
        }
        
        qDebug() << "[AsyncMPR-Update] 实际切片索引:" << sliceIndex;
        
        // 创建切片图像
        QPixmap slicePixmap(480, 360);
        slicePixmap.fill(Qt::black);
        
        QPainter painter(&slicePixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // 处理多种数据类型
        if (dataType == "unsigned char" || dataType == "UChar") {
            unsigned char* ucharData = static_cast<unsigned char*>(pixelData);
            QImage sliceImage(480, 360, QImage::Format_Grayscale8);
            sliceImage.fill(Qt::black);
            
            // 快速计算数据范围
            int totalPixels = width * height * depth;
            unsigned char minVal = ucharData[0], maxVal = ucharData[0];
            int step = qMax(1, totalPixels / 10000);
            for (int i = 0; i < totalPixels; i += step) {
                unsigned char val = ucharData[i];
                minVal = qMin(minVal, val);
                maxVal = qMax(maxVal, val);
            }
            
            // 提取指定切片的数据
            for (int y = 0; y < 360; y++) {
                for (int x = 0; x < 480; x++) {
                    unsigned char value = 0;
                    bool validPixel = false;
                    
                    if (viewInfo->viewType == "axial") {
                        // 轴状面：固定Z (sliceIndex)
                        int origX = x * width / 480;
                        int origY = y * height / 360;
                        if (origX >= 0 && origX < width && origY >= 0 && origY < height) {
                            int index = sliceIndex * width * height + origY * width + origX;
                            if (index >= 0 && index < totalPixels) {
                                value = ucharData[index];
                                validPixel = true;
                            }
                        }
                    } else if (viewInfo->viewType == "coronal") {
                        // 冠状面：固定Y (sliceIndex)
                        int origX = x * width / 480;
                        int origZ = y * depth / 360;
                        if (origX >= 0 && origX < width && origZ >= 0 && origZ < depth) {
                            int index = origZ * width * height + sliceIndex * width + origX;
                            if (index >= 0 && index < totalPixels) {
                                value = ucharData[index];
                                validPixel = true;
                            }
                        }
                    } else if (viewInfo->viewType == "sagittal") {
                        // 矢状面：固定X (sliceIndex)
                        int origY = x * height / 480;
                        int origZ = y * depth / 360;
                        if (origY >= 0 && origY < height && origZ >= 0 && origZ < depth) {
                            int index = origZ * width * height + origY * width + sliceIndex;
                            if (index >= 0 && index < totalPixels) {
                                value = ucharData[index];
                                validPixel = true;
                            }
                        }
                    }
                    
                    // 计算灰度值
                    int grayValue = 0;
                    if (validPixel) {
                        if (maxVal > minVal) {
                            grayValue = static_cast<int>((value - minVal) * 255 / (maxVal - minVal));
                        } else {
                            grayValue = static_cast<int>(value);
                        }
                        grayValue = qBound(0, grayValue, 255);
                    }
                    sliceImage.setPixel(x, y, qRgb(grayValue, grayValue, grayValue));
                }
            }
            
            // 绘制图像到pixmap
            painter.drawImage(0, 0, sliceImage);
            
        } else if (dataType == "short" || dataType == "Short") {
            // 处理short类型（CT图像常用）
            short* shortData = static_cast<short*>(pixelData);
            QImage sliceImage(480, 360, QImage::Format_Grayscale8);
            sliceImage.fill(Qt::black);
            
            // 快速计算数据范围
            int totalPixels = width * height * depth;
            short minVal = shortData[0], maxVal = shortData[0];
            int step = qMax(1, totalPixels / 10000);
            for (int i = 0; i < totalPixels; i += step) {
                short val = shortData[i];
                minVal = qMin(minVal, val);
                maxVal = qMax(maxVal, val);
            }
            
            qDebug() << "[AsyncMPR-Update] short数据范围:" << minVal << "到" << maxVal;
            
            // 提取指定切片的数据
            for (int y = 0; y < 360; y++) {
                for (int x = 0; x < 480; x++) {
                    short value = 0;
                    bool validPixel = false;
                    
                    if (viewInfo->viewType == "axial") {
                        int origX = x * width / 480;
                        int origY = y * height / 360;
                        if (origX >= 0 && origX < width && origY >= 0 && origY < height) {
                            int index = sliceIndex * width * height + origY * width + origX;
                            if (index >= 0 && index < totalPixels) {
                                value = shortData[index];
                                validPixel = true;
                            }
                        }
                    } else if (viewInfo->viewType == "coronal") {
                        int origX = x * width / 480;
                        int origZ = y * depth / 360;
                        if (origX >= 0 && origX < width && origZ >= 0 && origZ < depth) {
                            int index = origZ * width * height + sliceIndex * width + origX;
                            if (index >= 0 && index < totalPixels) {
                                value = shortData[index];
                                validPixel = true;
                            }
                        }
                    } else if (viewInfo->viewType == "sagittal") {
                        int origY = x * height / 480;
                        int origZ = y * depth / 360;
                        if (origY >= 0 && origY < height && origZ >= 0 && origZ < depth) {
                            int index = origZ * width * height + origY * width + sliceIndex;
                            if (index >= 0 && index < totalPixels) {
                                value = shortData[index];
                                validPixel = true;
                            }
                        }
                    }
                    
                    // 计算灰度值
                    int grayValue = 0;
                    if (validPixel) {
                        if (maxVal > minVal) {
                            grayValue = static_cast<int>((value - minVal) * 255 / (maxVal - minVal));
                        } else {
                            grayValue = static_cast<int>(value);
                        }
                        grayValue = qBound(0, grayValue, 255);
                    }
                    sliceImage.setPixel(x, y, qRgb(grayValue, grayValue, grayValue));
                }
            }
            
            painter.drawImage(0, 0, sliceImage);
            
        } else if (dataType == "float" || dataType == "Float") {
            // 处理float类型
            float* floatData = static_cast<float*>(pixelData);
            QImage sliceImage(480, 360, QImage::Format_Grayscale8);
            sliceImage.fill(Qt::black);
            
            // 快速计算数据范围
            int totalPixels = width * height * depth;
            float minVal = floatData[0], maxVal = floatData[0];
            int step = qMax(1, totalPixels / 10000);
            for (int i = 0; i < totalPixels; i += step) {
                float val = floatData[i];
                minVal = qMin(minVal, val);
                maxVal = qMax(maxVal, val);
            }
            
            qDebug() << "[AsyncMPR-Update] float数据范围:" << minVal << "到" << maxVal;
            
            // 提取指定切片的数据
            for (int y = 0; y < 360; y++) {
                for (int x = 0; x < 480; x++) {
                    float value = 0;
                    bool validPixel = false;
                    
                    if (viewInfo->viewType == "axial") {
                        int origX = x * width / 480;
                        int origY = y * height / 360;
                        if (origX >= 0 && origX < width && origY >= 0 && origY < height) {
                            int index = sliceIndex * width * height + origY * width + origX;
                            if (index >= 0 && index < totalPixels) {
                                value = floatData[index];
                                validPixel = true;
                            }
                        }
                    } else if (viewInfo->viewType == "coronal") {
                        int origX = x * width / 480;
                        int origZ = y * depth / 360;
                        if (origX >= 0 && origX < width && origZ >= 0 && origZ < depth) {
                            int index = origZ * width * height + sliceIndex * width + origX;
                            if (index >= 0 && index < totalPixels) {
                                value = floatData[index];
                                validPixel = true;
                            }
                        }
                    } else if (viewInfo->viewType == "sagittal") {
                        int origY = x * height / 480;
                        int origZ = y * depth / 360;
                        if (origY >= 0 && origY < height && origZ >= 0 && origZ < depth) {
                            int index = origZ * width * height + origY * width + sliceIndex;
                            if (index >= 0 && index < totalPixels) {
                                value = floatData[index];
                                validPixel = true;
                            }
                        }
                    }
                    
                    // 计算灰度值
                    int grayValue = 0;
                    if (validPixel) {
                        if (maxVal > minVal) {
                            grayValue = static_cast<int>((value - minVal) * 255 / (maxVal - minVal));
                        } else {
                            grayValue = 128; // 默认中灰色
                        }
                        grayValue = qBound(0, grayValue, 255);
                    }
                    sliceImage.setPixel(x, y, qRgb(grayValue, grayValue, grayValue));
                }
            }
            
            painter.drawImage(0, 0, sliceImage);
            
        } else {
            // 不支持的数据类型，显示提示
            painter.setPen(QPen(Qt::cyan, 2));
            painter.drawText(50, 180, QString("不支持的数据类型: %1").arg(dataType));
            qDebug() << "[AsyncMPR-Update] ❌ 不支持的数据类型:" << dataType;
        }
        
        // 添加十字线和标签
        painter.setPen(QPen(Qt::red, 1));
        painter.drawLine(240, 0, 240, 360);  // 垂直线
        painter.drawLine(0, 180, 480, 180);  // 水平线
        
        painter.setPen(QPen(Qt::yellow, 2));
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        QString labelText = QString("%1 - %2/%3").arg(viewInfo->viewType).arg(viewInfo->currentSlice).arg(viewInfo->totalSlices);
        painter.drawText(10, 350, labelText);
        
        painter.end();
        
        qDebug() << "[AsyncMPR-Update] ✅ 切片生成完成:" << viewInfo->viewType;
        return slicePixmap;
    });
    
    // 设置异步完成监听器
    viewInfo->currentWatcher = new QFutureWatcher<QPixmap>(this);
    connect(viewInfo->currentWatcher, &QFutureWatcher<QPixmap>::finished, [this, viewInfo]() {
        // 获取异步结果
        QPixmap result = viewInfo->currentWatcher->result();
        
        // 在UI线程中更新界面
        QMetaObject::invokeMethod(this, [viewInfo, result]() {
            if (viewInfo && viewInfo->imageLabel) {
                // 恢复样式并设置新图像
                viewInfo->imageLabel->setStyleSheet("QLabel { border: 1px solid #ccc; background-color: #000; }");
                viewInfo->imageLabel->setText("");
                viewInfo->imageLabel->setPixmap(result);
                
                qDebug() << "[AsyncMPR-Update] ✅ UI更新完成:" << viewInfo->viewType;
            }
        }, Qt::QueuedConnection);
        
        // 清理watcher（任务完成后）
        if (viewInfo->currentWatcher) {
            viewInfo->currentWatcher->deleteLater();
            viewInfo->currentWatcher = nullptr;
        }
        
        // 重置任务状态并处理队列中的待处理请求
        QMutexLocker locker(viewInfo->taskMutex);
        viewInfo->taskRunning = false;
        
        // 检查是否有待处理的切片请求
        if (viewInfo->pendingSlice >= 0 && viewInfo->pendingSlice != viewInfo->currentSlice) {
            qDebug() << "[MedicalViewerWidget] 🔄 处理队列中的切片请求:" << viewInfo->viewType 
                     << "从" << viewInfo->currentSlice << "到" << viewInfo->pendingSlice;
            viewInfo->currentSlice = viewInfo->pendingSlice;
            viewInfo->pendingSlice = -1;
            
            // 延迟执行下一个任务，避免立即重新进入
            QTimer::singleShot(10, this, [this, viewInfo]() {
                regenerateMPRSlice(viewInfo);
            });
        }
    });
    
    // 启动异步任务
    viewInfo->currentWatcher->setFuture(future);
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::clearMPRViews()
{
    // 清理所有MPR视图信息
    for (MedicalViewerWidget::MPRViewInfo* viewInfo : m_mprViews) {
        // 取消并清理异步任务
        if (viewInfo->currentWatcher) {
            viewInfo->currentWatcher->cancel();
            viewInfo->currentWatcher->deleteLater();
        }
        
        // 停止并清理防抖动定时器
        if (viewInfo->debounceTimer) {
            viewInfo->debounceTimer->stop();
            viewInfo->debounceTimer->deleteLater();
        }
        
        // 清理互斥锁
        if (viewInfo->taskMutex) {
            delete viewInfo->taskMutex;
        }
        
        delete viewInfo;
    }
    m_mprViews.clear();
    
    qDebug() << "[MedicalViewerWidget] 🧹 MPR视图信息已清理（包括异步任务和同步资源）";
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onOpacityChanged()
{
    // 注意：原右侧控制面板已移除，透明度控制现在集成在3D体绘制标签页内
    updateStatus("透明度控制功能已集成到3D体绘制标签页中");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onTransferFunctionChanged()
{
    // 注意：原右侧控制面板已移除，传输函数控制现在集成在3D体绘制标签页内
    updateStatus("传输函数控制功能已集成到3D体绘制标签页中");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onLightingChanged()
{
    // 注意：原右侧控制面板已移除，光照控制现在集成在3D体绘制标签页内
    updateStatus("光照控制功能已集成到3D体绘制标签页中");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onRenderingQualityChanged()
{
    // 注意：原右侧控制面板已移除，渲染质量控制现在集成在3D体绘制标签页内
    updateStatus("渲染质量控制功能已集成到3D体绘制标签页中");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onCameraReset()
{
    if (!m_serviceConnected || m_activeViewerIds.isEmpty()) return;
    
    // 重置体绘制相机
    for (const QString& viewerId : m_activeViewerIds) {
        if (m_viewerService && (m_currentDisplayMode == "3D" || m_currentDisplayMode == "Advanced")) {
            m_viewerService->resetVolumeCamera(viewerId);
        }
    }
    
    updateStatus("相机视角已重置");
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::applyTransferFunctionPreset(int presetIndex, double dataRange[2])
{
    if (!m_vtkColorFunction || !m_vtkOpacityFunction) {
        qDebug() << "[MedicalViewerWidget] ❌ 传输函数对象未初始化";
        return;
    }
    
    // 清除现有的传输函数点
    m_vtkColorFunction->RemoveAllPoints();
    m_vtkOpacityFunction->RemoveAllPoints();
    
    double minVal = dataRange[0];
    double maxVal = dataRange[1];
    double range = maxVal - minVal;
    
    qDebug() << "[MedicalViewerWidget] 🎨 应用传输函数预设" << presetIndex << "数据范围:" << minVal << "到" << maxVal;
    
    switch (presetIndex) {
        case 0: // 🦴 骨骼显示 (高对比)
            qDebug() << "[MedicalViewerWidget] 应用骨骼显示预设";
            
            // 骨骼颜色映射：突出骨骼结构
            m_vtkColorFunction->AddRGBPoint(minVal, 0.0, 0.0, 0.0);                    // 背景：黑色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.3, 0.2, 0.1, 0.0);        // 软组织：深棕
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.6, 0.9, 0.7, 0.4);        // 骨骼：米黄
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.8, 1.0, 0.9, 0.8);        // 高密度骨骼：白色
            m_vtkColorFunction->AddRGBPoint(maxVal, 1.0, 1.0, 1.0);                    // 最高密度：纯白
            
            // 骨骼透明度：突出显示骨骼
            m_vtkOpacityFunction->AddPoint(minVal, 0.0);                          // 背景：完全透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.2, 0.0);              // 软组织：透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.5, 0.2);              // 开始显示
            m_vtkOpacityFunction->AddPoint(minVal + range*0.7, 0.8);              // 骨骼：强显示
            m_vtkOpacityFunction->AddPoint(maxVal, 0.95);                         // 高密度：几乎不透明
            break;
            
        case 1: // 🫁 软组织显示 (中等) - 默认
            qDebug() << "[MedicalViewerWidget] 应用软组织显示预设";
            
            // 软组织颜色映射
            m_vtkColorFunction->AddRGBPoint(minVal, 0.0, 0.0, 0.0);                    // 背景：黑色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.1, 0.0, 0.2, 0.4);        // 低密度：深蓝
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.3, 0.5, 0.1, 0.1);        // 软组织：红棕
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.6, 0.9, 0.6, 0.3);        // 器官：橙黄
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.8, 1.0, 0.9, 0.7);        // 高密度：浅黄
            m_vtkColorFunction->AddRGBPoint(maxVal, 1.0, 1.0, 1.0);                    // 最高：白色
            
            // 软组织透明度：平衡显示
            m_vtkOpacityFunction->AddPoint(minVal, 0.0);                          // 背景：透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.05, 0.0);             // 低值：透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.15, 0.03);            // 开始微显
            m_vtkOpacityFunction->AddPoint(minVal + range*0.4, 0.2);              // 软组织：可见
            m_vtkOpacityFunction->AddPoint(minVal + range*0.7, 0.6);              // 重要结构
            m_vtkOpacityFunction->AddPoint(maxVal, 0.85);                         // 高密度
            break;
            
        case 2: // 🩸 血管显示 (透明)
            qDebug() << "[MedicalViewerWidget] 应用血管显示预设";
            
            // 血管颜色映射：突出血管结构
            m_vtkColorFunction->AddRGBPoint(minVal, 0.0, 0.0, 0.0);                    // 背景：黑色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.2, 0.0, 0.0, 0.5);        // 低密度：深蓝
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.4, 0.8, 0.0, 0.0);        // 血管：红色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.6, 1.0, 0.3, 0.0);        // 主要血管：橙红
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.8, 1.0, 0.8, 0.2);        // 高密度：黄色
            m_vtkColorFunction->AddRGBPoint(maxVal, 1.0, 1.0, 1.0);                    // 最高：白色
            
            // 血管透明度：高透明度以显示内部结构
            m_vtkOpacityFunction->AddPoint(minVal, 0.0);                          // 背景：完全透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.1, 0.0);              // 低值：透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.3, 0.1);              // 开始显示
            m_vtkOpacityFunction->AddPoint(minVal + range*0.5, 0.4);              // 血管：中等透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.7, 0.7);              // 主要结构
            m_vtkOpacityFunction->AddPoint(maxVal, 0.8);                          // 高密度
            break;
            
        case 3: // 🧠 CT标准显示
            qDebug() << "[MedicalViewerWidget] 应用CT标准显示预设";
            
            // CT标准颜色映射：医学标准
            m_vtkColorFunction->AddRGBPoint(minVal, 0.0, 0.0, 0.0);                    // 空气：黑色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.1, 0.0, 0.0, 0.3);        // 低密度：深蓝
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.3, 0.4, 0.2, 0.1);        // 软组织：棕色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.5, 0.7, 0.5, 0.3);        // 器官：浅棕
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.7, 0.9, 0.8, 0.6);        // 骨骼：米色
            m_vtkColorFunction->AddRGBPoint(maxVal, 1.0, 1.0, 1.0);                    // 高密度：白色
            
            // CT标准透明度
            m_vtkOpacityFunction->AddPoint(minVal, 0.0);                          // 空气：透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.1, 0.0);              // 低密度：透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.25, 0.05);            // 开始显示
            m_vtkOpacityFunction->AddPoint(minVal + range*0.5, 0.3);              // 软组织
            m_vtkOpacityFunction->AddPoint(minVal + range*0.75, 0.7);             // 骨骼
            m_vtkOpacityFunction->AddPoint(maxVal, 0.9);                          // 高密度
            break;
            
        case 4: // 🔬 研究模式 (详细)
            qDebug() << "[MedicalViewerWidget] 应用研究模式预设";
            
            // 研究模式：彩虹色谱，显示更多细节
            m_vtkColorFunction->AddRGBPoint(minVal, 0.0, 0.0, 0.0);                    // 黑色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.15, 0.0, 0.0, 1.0);       // 蓝色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.3, 0.0, 1.0, 1.0);        // 青色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.45, 0.0, 1.0, 0.0);       // 绿色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.6, 1.0, 1.0, 0.0);        // 黄色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.75, 1.0, 0.5, 0.0);       // 橙色
            m_vtkColorFunction->AddRGBPoint(maxVal, 1.0, 0.0, 0.0);                    // 红色
            
            // 研究模式透明度：显示所有细节
            m_vtkOpacityFunction->AddPoint(minVal, 0.0);                          // 背景：透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.05, 0.01);            // 极低值：微显
            m_vtkOpacityFunction->AddPoint(minVal + range*0.2, 0.1);              // 低值：可见
            m_vtkOpacityFunction->AddPoint(minVal + range*0.4, 0.3);              // 中值：明显
            m_vtkOpacityFunction->AddPoint(minVal + range*0.6, 0.5);              // 高值：清晰
            m_vtkOpacityFunction->AddPoint(minVal + range*0.8, 0.7);              // 很高值
            m_vtkOpacityFunction->AddPoint(maxVal, 0.85);                         // 最高值
            break;
            
        case 5: // ⚡ 快速预览
            qDebug() << "[MedicalViewerWidget] 应用快速预览预设";
            
            // 快速预览：简单的灰度映射
            m_vtkColorFunction->AddRGBPoint(minVal, 0.0, 0.0, 0.0);                    // 黑色
            m_vtkColorFunction->AddRGBPoint(minVal + range*0.5, 0.5, 0.5, 0.5);        // 灰色
            m_vtkColorFunction->AddRGBPoint(maxVal, 1.0, 1.0, 1.0);                    // 白色
            
            // 快速预览透明度：高透明度，快速显示
            m_vtkOpacityFunction->AddPoint(minVal, 0.0);                          // 背景：透明
            m_vtkOpacityFunction->AddPoint(minVal + range*0.3, 0.1);              // 开始显示
            m_vtkOpacityFunction->AddPoint(minVal + range*0.7, 0.5);              // 主要结构
            m_vtkOpacityFunction->AddPoint(maxVal, 0.7);                          // 高密度
            break;
            
        default:
            qDebug() << "[MedicalViewerWidget] ❌ 未知的传输函数预设索引:" << presetIndex;
            // 使用默认的软组织显示
            applyTransferFunctionPreset(1, dataRange);
            return;
    }
    
    qDebug() << "[MedicalViewerWidget] ✅ 传输函数预设" << presetIndex << "应用完成";
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onMeasurementToggled()
{
    // 注意：该函数现在只处理左侧查看器标签页内的测量工具
    // 原右侧控制面板的测量工具已移除
    QCheckBox* sender = qobject_cast<QCheckBox*>(QObject::sender());
    if (!sender) return;
    
    bool enabled = sender->isChecked();
    
    // 重置当前测量状态
    if (m_currentMeasurementView) {
        m_currentMeasurementView->removeEventFilter(this);
        m_currentMeasurementView->scene()->removeEventFilter(this);
        m_currentMeasurementView = nullptr;
    }
    m_currentMeasurementMode = NoMeasurement;
    m_measurementPoints.clear();
    
    // 设置新的测量模式（只处理来自左侧标签页的信号）
    if (enabled) {
        QGraphicsView* currentImageView = nullptr;
        if (m_currentDisplayMode == "2D" && m_currentImageView) {
            currentImageView = m_currentImageView;
        }
        
        if (currentImageView) {
            // 通过sender的objectName或其他方式识别具体的测量类型
            QString senderName = sender->objectName();
            if (senderName.contains("distance")) {
                enableMeasurement(currentImageView, "distance");
            } else if (senderName.contains("angle")) {
                enableMeasurement(currentImageView, "angle");
            } else {
                updateStatus("面积测量功能开发中...");
                sender->setChecked(false);
            }
        } else {
            updateStatus("请先切换到2D模式以使用测量工具");
            sender->setChecked(false);
        }
    } else {
        updateStatus("测量工具已关闭");
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onAnnotationToggled()
{
    if (!m_serviceConnected || m_activeViewerIds.isEmpty()) return;
    
    // 获取发送信号的复选框
    QCheckBox* sender = qobject_cast<QCheckBox*>(QObject::sender());
    if (!sender) return;
    
    bool enabled = sender->isChecked();
    
    // 应用标注设置到所有活动查看器
    for (const QString& viewerId : m_activeViewerIds) {
        if (m_viewerService) {
            // 这里可以根据具体的标注功能进行扩展
            // 例如：启用/禁用文本标注、箭头标注等
            updateStatus(QString("标注功能: %1").arg(enabled ? "开启" : "关闭"));
        }
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onScreenshot()
{
    if (!m_serviceConnected || m_activeViewerIds.isEmpty()) {
        QMessageBox::warning(this, "截图失败", "没有活动的查看器");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this,
        "保存截图", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "PNG Images (*.png);;JPEG Images (*.jpg);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        // 截图第一个活动查看器
        QString viewerId = m_activeViewerIds.first();
        if (m_viewerService) {
            bool success = m_viewerService->captureViewerImage(viewerId, fileName);
            
            QString message = success ? 
                QString("截图已保存到: %1").arg(fileName) : 
                "截图保存失败";
            updateStatus(message);
            
            if (success) {
                m_logTextEdit->append(QString("[%1] 截图保存: %2")
                                     .arg(QTime::currentTime().toString())
                                     .arg(QFileInfo(fileName).fileName()));
            }
        }
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onExportData()
{
    if (!m_serviceConnected || m_currentImageId.isEmpty()) {
        QMessageBox::warning(this, "导出失败", "没有可导出的数据");
        return;
    }
    
    QMessageBox::information(this, "数据导出", 
        QString("数据导出功能\n\n当前图像ID: %1\n显示模式: %2\n\n将实现：\n- 原始数据导出\n- 处理结果导出\n- 参数配置导出")
        .arg(m_currentImageId)
        .arg(m_currentDisplayMode));
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onViewerServiceAvailable()
{
    if (m_viewerService) {
        // 连接查看器服务信号
        connect(m_viewerService, &MedicalViewerService::viewerCreated,
                [this](const QString& viewerId, const QString& viewerType) {
                    m_logTextEdit->append(QString("[%1] 创建查看器: %2 (%3)")
                                         .arg(QTime::currentTime().toString())
                                         .arg(viewerId)
                                         .arg(viewerType));
                });
        
        connect(m_viewerService, &MedicalViewerService::imageDisplayed,
                [this](const QString& viewerId, const QString& imageId) {
                    m_logTextEdit->append(QString("[%1] 显示图像: %2 在查看器 %3")
                                         .arg(QTime::currentTime().toString())
                                         .arg(imageId)
                                         .arg(viewerId));
                });
        
        connect(m_viewerService, &MedicalViewerService::viewerError,
                [this](const QString& viewerId, const QString& error) {
                    updateStatus(QString("查看器错误: %1").arg(error));
                    m_logTextEdit->append(QString("[%1] 错误: %2")
                                         .arg(QTime::currentTime().toString())
                                         .arg(error));
                });
        
        updateStatus("医学图像查看器服务已连接");
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onImageServiceAvailable()
{
    if (m_imageService) {
        // 连接图像服务信号（CTK标准方式）
        connect(m_imageService, SIGNAL(imageLoaded(QString, QString)),
                this, SLOT(onImageLoadedFromService(QString, QString)));
        
        updateStatus("医学图像核心服务已连接");
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::onImageLoadedFromService(const QString& imageId, const QString& filePath)
{
                    refreshImageList();
                    m_logTextEdit->append(QString("[%1] 图像已加载: %2 (路径: %3)")
                                         .arg(QTime::currentTime().toString())
                                         .arg(imageId)
                                         .arg(filePath));
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::updateStatus(const QString& message)
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
    
    qDebug() << "[MedicalViewerWidget]" << message;
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::updateProgress(int value)
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
void MedicalViewerWidget::render2DImage(const QString& imageId, const QString& infoText)
{
    qDebug() << "[MedicalViewerWidget] 开始2D图像渲染:" << imageId.left(8) + "...";
    
    // 清理2D查看器标签页
    if (m_2dViewerTab) {
        if (m_2dViewerTab->layout()) {
            QLayoutItem* item;
            while ((item = m_2dViewerTab->layout()->takeAt(0)) != nullptr) {
                delete item->widget();
                delete item;
            }
        } else {
            QVBoxLayout* layout = new QVBoxLayout(m_2dViewerTab);
            m_2dViewerTab->setLayout(layout);
        }
        
        // 创建图像信息面板
        QTextEdit* infoDisplay = new QTextEdit();
        infoDisplay->setPlainText(infoText);
        infoDisplay->setReadOnly(true);
        infoDisplay->setMaximumHeight(120);
        infoDisplay->setStyleSheet("QTextEdit { background-color: #f5f5f5; border: 1px solid #ddd; font-size: 11px; }");
        
        // 使用exportImage导出为临时PNG文件
        if (auto coreService = getImageCoreService()) {
            QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            QString tempFile = QDir(tempDir).absoluteFilePath(QString("temp_image_%1.png").arg(imageId));
            
            updateStatus("正在导出图像为PNG格式...");
            QVariantMap exportOptions;
            exportOptions["quality"] = 95;
            exportOptions["normalize"] = true; // 归一化像素值到0-255
            
            if (coreService->exportImage(imageId, "PNG", tempFile, exportOptions)) {
                qDebug() << "[MedicalViewerWidget] 图像已导出到:" << tempFile;
                
                // 加载PNG图像
                QPixmap pixmap(tempFile);
                if (!pixmap.isNull()) {
                    // 保存原始图像用于窗口/级别调整
                    m_originalPixmap = pixmap;
                    
                    // 创建QGraphicsView显示图像
                    QGraphicsView* imageView = new QGraphicsView();
                    QGraphicsScene* scene = new QGraphicsScene();
                    
                    // 添加图像到场景
                    QGraphicsPixmapItem* pixmapItem = scene->addPixmap(pixmap);
                    imageView->setScene(scene);
                    
                    // 保存当前显示组件的引用
                    m_currentPixmapItem = pixmapItem;
                    m_currentImageView = imageView;
                    
                    // 设置视图属性
                    imageView->setRenderHint(QPainter::Antialiasing);
                    imageView->setDragMode(QGraphicsView::RubberBandDrag);
                    imageView->setInteractive(true);
                    imageView->setStyleSheet("QGraphicsView { border: 1px solid #ccc; background-color: #000; }");
                    
                    // 缩放到适合窗口大小
                    imageView->fitInView(pixmapItem, Qt::KeepAspectRatio);
                    
                    // 创建上方控制面板（缩放控制）
                    QWidget* zoomPanel = new QWidget();
                    QHBoxLayout* zoomLayout = new QHBoxLayout(zoomPanel);
                    zoomLayout->setContentsMargins(5, 5, 5, 5);
                    
                    QPushButton* fitBtn = new QPushButton("适应窗口");
                    QPushButton* originalBtn = new QPushButton("原始大小");
                    QPushButton* zoomInBtn = new QPushButton("放大");
                    QPushButton* zoomOutBtn = new QPushButton("缩小");
                    QPushButton* distanceBtn = new QPushButton("距离测量");
                    QPushButton* angleBtn = new QPushButton("角度测量");
                    QPushButton* clearBtn = new QPushButton("清除测量");
                    
                    // 设置测量按钮样式
                    distanceBtn->setStyleSheet("QPushButton { background-color: #e8f4f8; }");
                    angleBtn->setStyleSheet("QPushButton { background-color: #f8e8f4; }");
                    clearBtn->setStyleSheet("QPushButton { background-color: #f8f4e8; }");
                    
                    zoomLayout->addWidget(fitBtn);
                    zoomLayout->addWidget(originalBtn);
                    zoomLayout->addWidget(zoomInBtn);
                    zoomLayout->addWidget(zoomOutBtn);
                    zoomLayout->addWidget(distanceBtn);
                    zoomLayout->addWidget(angleBtn);
                    zoomLayout->addWidget(clearBtn);
                    zoomLayout->addStretch();
                    
                    // 创建窗口/级别控制面板
                    QWidget* windowLevelPanel = createWindowLevelControls(imageId);
                    
                    // 连接缩放控制
                    connect(fitBtn, &QPushButton::clicked, [imageView, pixmapItem]() {
                        imageView->fitInView(pixmapItem, Qt::KeepAspectRatio);
                    });
                    connect(originalBtn, &QPushButton::clicked, [imageView]() {
                        imageView->resetTransform();
                    });
                    connect(zoomInBtn, &QPushButton::clicked, [imageView]() {
                        imageView->scale(1.2, 1.2);
                    });
                    connect(zoomOutBtn, &QPushButton::clicked, [imageView]() {
                        imageView->scale(0.8, 0.8);
                    });
                    
                    // 连接测量工具
                    connect(distanceBtn, &QPushButton::clicked, [this, imageView]() {
                        enableMeasurement(imageView, "distance");
                        updateStatus("距离测量模式已启用 - 点击两点进行测量");
                    });
                    connect(angleBtn, &QPushButton::clicked, [this, imageView]() {
                        enableMeasurement(imageView, "angle");
                        updateStatus("角度测量模式已启用 - 点击三点进行测量");
                    });
                    connect(clearBtn, &QPushButton::clicked, [this, imageView]() {
                        enableMeasurement(imageView, "clear");
                        updateStatus("测量标记已清除");
                    });
                    
                    // 创建主要显示区域的水平分隔器
                    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal);
                    
                    // 左侧：图像显示区域
                    QWidget* imageWidget = new QWidget();
                    QVBoxLayout* imageLayout = new QVBoxLayout(imageWidget);
                    imageLayout->setContentsMargins(0, 0, 0, 0);
                    imageLayout->addWidget(zoomPanel);
                    imageLayout->addWidget(imageView);
                    
                    // 右侧：控制面板
                    QWidget* controlsWidget = new QWidget();
                    QVBoxLayout* controlsLayout = new QVBoxLayout(controlsWidget);
                    controlsLayout->setContentsMargins(5, 0, 0, 0);
                    controlsLayout->addWidget(windowLevelPanel);
                    controlsLayout->addStretch(); // 将控制面板推到顶部
                    
                    // 设置分隔器
                    mainSplitter->addWidget(imageWidget);
                    mainSplitter->addWidget(controlsWidget);
                    mainSplitter->setStretchFactor(0, 3); // 图像区域占3/4
                    mainSplitter->setStretchFactor(1, 1); // 控制区域占1/4
                    mainSplitter->setSizes({600, 200});
                    
                    // 添加到布局
                    m_2dViewerTab->layout()->addWidget(infoDisplay);
                    m_2dViewerTab->layout()->addWidget(mainSplitter);
                    
                    updateStatus(QString("2D图像显示成功: %1 (%2x%3)").arg(imageId.left(8) + "...").arg(pixmap.width()).arg(pixmap.height()));
                    
                    // 清理临时文件
                    QTimer::singleShot(5000, [tempFile]() {
                        QFile::remove(tempFile);
                    });
                    
                } else {
                    // 显示错误信息
                    QLabel* errorLabel = new QLabel("无法加载导出的图像文件");
                    errorLabel->setAlignment(Qt::AlignCenter);
                    errorLabel->setStyleSheet("QLabel { color: red; font-size: 14px; }");
                    m_2dViewerTab->layout()->addWidget(infoDisplay);
                    m_2dViewerTab->layout()->addWidget(errorLabel);
                    updateStatus("图像加载失败：无法读取导出文件");
                }
            } else {
                // 显示导出失败信息
                QLabel* errorLabel = new QLabel("图像导出失败\n可能是不支持的图像格式或数据损坏");
                errorLabel->setAlignment(Qt::AlignCenter);
                errorLabel->setStyleSheet("QLabel { color: red; font-size: 14px; }");
                m_2dViewerTab->layout()->addWidget(infoDisplay);
                m_2dViewerTab->layout()->addWidget(errorLabel);
                updateStatus("图像导出失败：" + coreService->getLastError());
            }
        } else {
            // 显示服务不可用信息
            QLabel* errorLabel = new QLabel("图像核心服务不可用");
            errorLabel->setAlignment(Qt::AlignCenter);
            errorLabel->setStyleSheet("QLabel { color: red; font-size: 14px; }");
            m_2dViewerTab->layout()->addWidget(errorLabel);
            updateStatus("服务不可用：无法获取图像核心服务");
        }
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::render3DImage(const QString& imageId, const QString& infoText)
{
    qDebug() << "[MedicalViewerWidget] 开始3D图像渲染:" << imageId.left(8) + "...";
    
#ifdef VTK_FOUND
    if (m_3dViewerTab) {
        // 清理现有内容
        if (m_3dViewerTab->layout()) {
            QLayoutItem* item;
            while ((item = m_3dViewerTab->layout()->takeAt(0)) != nullptr) {
                delete item->widget();
                delete item;
            }
        } else {
            QVBoxLayout* layout = new QVBoxLayout(m_3dViewerTab);
            m_3dViewerTab->setLayout(layout);
        }
        
        // 创建图像信息面板
        QTextEdit* infoDisplay = new QTextEdit();
        infoDisplay->setPlainText(infoText);
        infoDisplay->setReadOnly(true);
        infoDisplay->setMaximumHeight(100);
        infoDisplay->setStyleSheet("QTextEdit { background-color: #f5f5f5; border: 1px solid #ddd; font-size: 11px; }");
        
        try {
            // 获取图像信息
            auto coreService = getImageCoreService();
            if (!coreService) {
                throw std::runtime_error("图像核心服务不可用");
            }
            
            QVariantMap imageInfo = coreService->getImageDetails(imageId);
            QList<int> dimensionsList = imageInfo.value("dimensions").value<QList<int>>();
            
            int imgWidth = dimensionsList.size() > 0 ? dimensionsList[0] : 0;
            int imgHeight = dimensionsList.size() > 1 ? dimensionsList[1] : 0;
            int imgDepth = dimensionsList.size() > 2 ? dimensionsList[2] : 1;
            
            qDebug() << "[MedicalViewerWidget] 正确解析3D维度:" << dimensionsList << "→" << imgWidth << "x" << imgHeight << "x" << imgDepth;
            
            if (imgDepth <= 1) {
                // 对于2D图像，显示信息提示
                QLabel* infoLabel = new QLabel(QString("当前图像为2D图像 (%1×%2)\n\n3D体渲染需要3D数据集\n请加载具有深度信息的图像")
                                               .arg(imgWidth).arg(imgHeight));
                infoLabel->setAlignment(Qt::AlignCenter);
                infoLabel->setStyleSheet("QLabel { color: #666; font-size: 14px; border: 2px dashed #ccc; padding: 20px; }");
                
                m_3dViewerTab->layout()->addWidget(infoDisplay);
                m_3dViewerTab->layout()->addWidget(infoLabel);
                
                updateStatus("2D图像无法进行3D渲染");
                return;
            }
            
            // 创建VTK渲染组件
            m_vtkWidget = new QVTKOpenGLNativeWidget();
            vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
            m_vtkWidget->setRenderWindow(renderWindow);
            
            m_vtkRenderer = vtkSmartPointer<vtkRenderer>::New();
            renderWindow->AddRenderer(m_vtkRenderer);
            
            // 设置渲染器背景
            m_vtkRenderer->SetBackground(0.1, 0.1, 0.2); // 深蓝色背景
            
            // 获取真实的医学图像数据
            void* pixelData = coreService->getImagePixelData(imageId);
            QString dataType = coreService->getImageDataType(imageId);
            
            vtkSmartPointer<vtkImageData> imageData;
            
            if (pixelData) {
                qDebug() << "[MedicalViewerWidget] 从像素数据创建VTK图像，数据类型:" << dataType << "维度:" << imgWidth << "x" << imgHeight << "x" << imgDepth;
                
                imageData = vtkSmartPointer<vtkImageData>::New();
                imageData->SetDimensions(imgWidth, imgHeight, imgDepth);
                
                // 根据数据类型设置标量类型
                if (dataType == "float" || dataType == "Float") {
                    imageData->AllocateScalars(VTK_FLOAT, 1);
                    memcpy(imageData->GetScalarPointer(), pixelData, 
                           imgWidth * imgHeight * imgDepth * sizeof(float));
                } else if (dataType == "short" || dataType == "Short") {
                    imageData->AllocateScalars(VTK_SHORT, 1);
                    memcpy(imageData->GetScalarPointer(), pixelData, 
                           imgWidth * imgHeight * imgDepth * sizeof(short));
                } else if (dataType == "unsigned short" || dataType == "UShort") {
                    imageData->AllocateScalars(VTK_UNSIGNED_SHORT, 1);
                    memcpy(imageData->GetScalarPointer(), pixelData, 
                           imgWidth * imgHeight * imgDepth * sizeof(unsigned short));
                } else if (dataType == "unsigned char" || dataType == "UChar") {
                    imageData->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
                    memcpy(imageData->GetScalarPointer(), pixelData, 
                           imgWidth * imgHeight * imgDepth * sizeof(unsigned char));
                } else {
                    // 默认使用float
                    qDebug() << "[MedicalViewerWidget] 未知数据类型，使用默认float";
                    imageData->AllocateScalars(VTK_FLOAT, 1);
                    memcpy(imageData->GetScalarPointer(), pixelData, 
                           imgWidth * imgHeight * imgDepth * sizeof(float));
                }
                
                // 设置体素间距
                QList<double> spacing = coreService->getImageSpacing(imageId);
                if (spacing.size() >= 3) {
                    imageData->SetSpacing(spacing[0], spacing[1], spacing[2]);
                    qDebug() << "[MedicalViewerWidget] 设置体素间距:" << spacing[0] << spacing[1] << spacing[2];
                }
                
                // 设置原点
                QList<double> origin = coreService->getImageOrigin(imageId);
                if (origin.size() >= 3) {
                    imageData->SetOrigin(origin[0], origin[1], origin[2]);
                    qDebug() << "[MedicalViewerWidget] 设置原点:" << origin[0] << origin[1] << origin[2];
                }
                
                qDebug() << "[MedicalViewerWidget] VTK图像数据创建成功";
            }
            
            // 如果仍然没有数据，创建错误提示
            if (!imageData) {
                QLabel* errorLabel = new QLabel("无法获取图像数据进行3D渲染\n请检查图像是否正确加载");
                errorLabel->setAlignment(Qt::AlignCenter);
                errorLabel->setStyleSheet("QLabel { color: #ff6b6b; font-size: 16px; }");
                m_3dViewerTab->layout()->addWidget(infoDisplay);
                m_3dViewerTab->layout()->addWidget(errorLabel);
                updateStatus("3D渲染失败：无法获取图像数据");
                return;
            }
            
            // 创建体映射器 (优先使用GPU，回退到CPU)
            vtkNew<vtkGPUVolumeRayCastMapper> volumeMapper;
            volumeMapper->SetInputData(imageData);
            
            // 创建体属性
            m_vtkVolumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
            
            // 获取图像数据范围
            double range[2];
            imageData->GetScalarRange(range);
            qDebug() << "[MedicalViewerWidget] 图像数据范围:" << range[0] << "到" << range[1] << "数据类型:" << dataType;
            
            // 创建智能传输函数（根据数据类型和范围优化）
            m_vtkColorFunction = vtkSmartPointer<vtkColorTransferFunction>::New();
            m_vtkOpacityFunction = vtkSmartPointer<vtkPiecewiseFunction>::New();
            
            // 根据数据类型和范围设置不同的传输函数
            if (dataType == "unsigned char" || dataType == "UChar") {
                // unsigned char (0-255) - 通常是分割或标记图像
                qDebug() << "[MedicalViewerWidget] 应用优化的UChar传输函数";
                
                // 更激进的颜色映射：突出内部结构
                m_vtkColorFunction->AddRGBPoint(0, 0.0, 0.0, 0.0);        // 0: 完全透明
                m_vtkColorFunction->AddRGBPoint(30, 0.0, 0.3, 0.6);       // 低密度：深蓝色
                m_vtkColorFunction->AddRGBPoint(60, 0.0, 0.8, 0.0);       // 软组织：绿色
                m_vtkColorFunction->AddRGBPoint(120, 1.0, 1.0, 0.0);      // 中等：黄色
                m_vtkColorFunction->AddRGBPoint(180, 1.0, 0.5, 0.0);      // 高密度：橙色
                m_vtkColorFunction->AddRGBPoint(255, 1.0, 0.0, 0.0);      // 最高：红色
                
                // 更激进的透明度：让内部结构可见
                m_vtkOpacityFunction->AddPoint(0, 0.0);        // 背景完全透明
                m_vtkOpacityFunction->AddPoint(10, 0.0);       // 低值完全透明
                m_vtkOpacityFunction->AddPoint(25, 0.02);      // 开始微显
                m_vtkOpacityFunction->AddPoint(50, 0.1);       // 软组织微透明
                m_vtkOpacityFunction->AddPoint(80, 0.3);       // 中等结构
                m_vtkOpacityFunction->AddPoint(120, 0.6);      // 重要结构
                m_vtkOpacityFunction->AddPoint(180, 0.85);     // 高密度
                m_vtkOpacityFunction->AddPoint(255, 0.95);     // 最高密度
                
            } else if (dataType == "short" || dataType == "Short") {
                // short (-32768 到 32767) - 通常是CT数据（HU值）
                qDebug() << "[MedicalViewerWidget] 应用优化的Short传输函数（医学CT风格）";
                
                double minHU = range[0];
                double maxHU = range[1];
                double range_span = maxHU - minHU;
                
                // 优化的CT颜色映射：突出器官结构
                m_vtkColorFunction->AddRGBPoint(minHU, 0.0, 0.0, 0.0);                    // 空气：黑色
                m_vtkColorFunction->AddRGBPoint(minHU + range_span*0.05, 0.0, 0.0, 0.3);  // 低密度：深蓝
                m_vtkColorFunction->AddRGBPoint(minHU + range_span*0.2, 0.5, 0.0, 0.5);   // 软组织：紫色
                m_vtkColorFunction->AddRGBPoint(minHU + range_span*0.4, 0.0, 0.9, 0.0);   // 器官：绿色
                m_vtkColorFunction->AddRGBPoint(minHU + range_span*0.6, 1.0, 1.0, 0.0);   // 中密度：黄色
                m_vtkColorFunction->AddRGBPoint(minHU + range_span*0.8, 1.0, 0.4, 0.0);   // 骨骼：橙色
                m_vtkColorFunction->AddRGBPoint(maxHU, 1.0, 1.0, 1.0);                    // 高密度：白色
                
                // 优化的CT透明度：显示内部结构
                m_vtkOpacityFunction->AddPoint(minHU, 0.0);                           // 空气：完全透明
                m_vtkOpacityFunction->AddPoint(minHU + range_span*0.05, 0.0);         // 低密度：透明
                m_vtkOpacityFunction->AddPoint(minHU + range_span*0.15, 0.02);        // 开始微显
                m_vtkOpacityFunction->AddPoint(minHU + range_span*0.3, 0.1);          // 软组织：微透明
                m_vtkOpacityFunction->AddPoint(minHU + range_span*0.5, 0.4);          // 器官：半透明
                m_vtkOpacityFunction->AddPoint(minHU + range_span*0.7, 0.7);          // 骨骼：较不透明
                m_vtkOpacityFunction->AddPoint(maxHU, 0.9);                           // 高密度：几乎不透明
                
            } else {
                // float/double或其他类型 - 优化的通用医学图像风格
                qDebug() << "[MedicalViewerWidget] 应用优化的通用传输函数";
                
                double min_val = range[0];
                double max_val = range[1];
                double range_span = max_val - min_val;
                
                // 智能颜色映射：根据实际数据范围优化
                m_vtkColorFunction->AddRGBPoint(min_val, 0.0, 0.0, 0.0);                      // 最小：黑色
                m_vtkColorFunction->AddRGBPoint(min_val + range_span*0.1, 0.0, 0.0, 0.5);     // 深蓝
                m_vtkColorFunction->AddRGBPoint(min_val + range_span*0.25, 0.0, 0.6, 0.9);    // 蓝色
                m_vtkColorFunction->AddRGBPoint(min_val + range_span*0.4, 0.0, 0.9, 0.0);     // 绿色
                m_vtkColorFunction->AddRGBPoint(min_val + range_span*0.6, 1.0, 1.0, 0.0);     // 黄色
                m_vtkColorFunction->AddRGBPoint(min_val + range_span*0.8, 1.0, 0.5, 0.0);     // 橙色
                m_vtkColorFunction->AddRGBPoint(max_val, 1.0, 0.0, 0.0);                      // 最大：红色
                
                // 智能透明度：突出重要结构
                m_vtkOpacityFunction->AddPoint(min_val, 0.0);                             // 最小：完全透明
                m_vtkOpacityFunction->AddPoint(min_val + range_span*0.05, 0.0);           // 低值：透明
                m_vtkOpacityFunction->AddPoint(min_val + range_span*0.15, 0.03);          // 开始微显
                m_vtkOpacityFunction->AddPoint(min_val + range_span*0.3, 0.15);           // 轻微可见
                m_vtkOpacityFunction->AddPoint(min_val + range_span*0.5, 0.4);            // 中等可见
                m_vtkOpacityFunction->AddPoint(min_val + range_span*0.7, 0.7);            // 较不透明
                m_vtkOpacityFunction->AddPoint(max_val, 0.85);                            // 最大：接近不透明
            }
            
            qDebug() << "[MedicalViewerWidget] 🎨 传输函数设置完成，数据范围:" << range[0] << "到" << range[1];
            
            // 应用传输函数
            m_vtkVolumeProperty->SetColor(m_vtkColorFunction);
            m_vtkVolumeProperty->SetScalarOpacity(m_vtkOpacityFunction);
            m_vtkVolumeProperty->ShadeOn();
            m_vtkVolumeProperty->SetInterpolationTypeToLinear();
            
            // 创建体对象
            m_vtkVolume = vtkSmartPointer<vtkVolume>::New();
            m_vtkVolume->SetMapper(volumeMapper);
            m_vtkVolume->SetProperty(m_vtkVolumeProperty);
            
            // 添加到渲染器
            m_vtkRenderer->AddVolume(m_vtkVolume);
            
            // 设置相机
            vtkCamera* camera = m_vtkRenderer->GetActiveCamera();
            camera->SetPosition(imgWidth * 1.5, imgHeight * 1.5, imgDepth * 1.5);
            camera->SetFocalPoint(imgWidth / 2.0, imgHeight / 2.0, imgDepth / 2.0);
            camera->SetViewUp(0, 0, 1);
            m_vtkRenderer->ResetCamera();
            
            // 设置交互样式
            vtkNew<vtkInteractorStyleTrackballCamera> style;
            m_vtkWidget->interactor()->SetInteractorStyle(style);
            
            // 创建控制面板
            QWidget* controlPanel = create3DControls(imageId);
            
            // 创建主分隔器
            QSplitter* mainSplitter = new QSplitter(Qt::Horizontal);
            
            // 左侧：VTK渲染区域
            QWidget* renderWidget = new QWidget();
            QVBoxLayout* renderLayout = new QVBoxLayout(renderWidget);
            renderLayout->setContentsMargins(0, 0, 0, 0);
            renderLayout->addWidget(m_vtkWidget);
            
            // 右侧：控制面板
            QWidget* controlsWidget = new QWidget();
            QVBoxLayout* controlsLayout = new QVBoxLayout(controlsWidget);
            controlsLayout->setContentsMargins(5, 0, 0, 0);
            controlsLayout->addWidget(controlPanel);
            controlsLayout->addStretch();
            
            // 设置分隔器
            mainSplitter->addWidget(renderWidget);
            mainSplitter->addWidget(controlsWidget);
            mainSplitter->setStretchFactor(0, 3);
            mainSplitter->setStretchFactor(1, 1);
            mainSplitter->setSizes({600, 200});
            
            // 添加到布局
            m_3dViewerTab->layout()->addWidget(infoDisplay);
            m_3dViewerTab->layout()->addWidget(mainSplitter);
            
            updateStatus(QString("3D体渲染成功: %1 (%2×%3×%4)").arg(imageId.left(8) + "...").arg(imgWidth).arg(imgHeight).arg(imgDepth));
            
        } catch (const std::exception& e) {
            // 显示错误信息
            QLabel* errorLabel = new QLabel(QString("3D渲染失败:\n%1").arg(e.what()));
            errorLabel->setAlignment(Qt::AlignCenter);
            errorLabel->setStyleSheet("QLabel { color: red; font-size: 14px; }");
            m_3dViewerTab->layout()->addWidget(infoDisplay);
            m_3dViewerTab->layout()->addWidget(errorLabel);
            updateStatus("3D渲染失败: " + QString(e.what()));
        }
    }
#else
    // VTK不可用时的降级处理
    if (m_3dViewerTab) {
        if (m_3dViewerTab->layout()) {
            QLayoutItem* item;
            while ((item = m_3dViewerTab->layout()->takeAt(0)) != nullptr) {
                delete item->widget();
                delete item;
            }
        } else {
            QVBoxLayout* layout = new QVBoxLayout(m_3dViewerTab);
            m_3dViewerTab->setLayout(layout);
        }
        
        QLabel* noVtkLabel = new QLabel("VTK库不可用\n\n3D体渲染需要VTK支持\n请检查VTK库是否正确安装");
        noVtkLabel->setAlignment(Qt::AlignCenter);
        noVtkLabel->setStyleSheet("QLabel { color: red; font-size: 14px; border: 2px dashed #f00; padding: 20px; }");
        m_3dViewerTab->layout()->addWidget(noVtkLabel);
        
        updateStatus("VTK不可用，无法进行3D渲染");
    }
#endif
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::renderMPRImage(const QString& imageId, const QString& infoText)
{
    qDebug() << "[MedicalViewerWidget] 开始MPR图像渲染:" << imageId.left(8) + "...";
    
    // 清理之前的MPR视图信息
    clearMPRViews();
    
    if (m_mprViewerTab) {
        // 清理现有内容
        if (m_mprViewerTab->layout()) {
            QLayoutItem* item;
            while ((item = m_mprViewerTab->layout()->takeAt(0)) != nullptr) {
                delete item->widget();
                delete item;
            }
        } else {
            QVBoxLayout* layout = new QVBoxLayout(m_mprViewerTab);
            m_mprViewerTab->setLayout(layout);
        }
        
        // 创建图像信息面板
        QTextEdit* infoDisplay = new QTextEdit();
        infoDisplay->setPlainText(infoText);
        infoDisplay->setReadOnly(true);
        infoDisplay->setMaximumHeight(80);
        infoDisplay->setStyleSheet("QTextEdit { background-color: #f5f5f5; border: 1px solid #ddd; font-size: 11px; }");
        
        try {
            // 获取图像信息
            auto coreService = getImageCoreService();
            if (!coreService) {
                throw std::runtime_error("图像核心服务不可用");
            }
            
            QVariantMap imageInfo = coreService->getImageDetails(imageId);
            QList<int> dimensionsList = imageInfo.value("dimensions").value<QList<int>>();
            
            int imgWidth = dimensionsList.size() > 0 ? dimensionsList[0] : 0;
            int imgHeight = dimensionsList.size() > 1 ? dimensionsList[1] : 0;
            int imgDepth = dimensionsList.size() > 2 ? dimensionsList[2] : 1;
            
            qDebug() << "[MedicalViewerWidget] 正确解析MPR维度:" << dimensionsList << "→" << imgWidth << "x" << imgHeight << "x" << imgDepth;
            
            if (imgDepth <= 1) {
                // 对于2D图像，显示信息提示
                QLabel* infoLabel = new QLabel(QString("当前图像为2D图像 (%1×%2)\n\nMPR重建需要3D数据集\n请加载具有深度信息的图像")
                                               .arg(imgWidth).arg(imgHeight));
                infoLabel->setAlignment(Qt::AlignCenter);
                infoLabel->setStyleSheet("QLabel { color: #666; font-size: 14px; border: 2px dashed #ccc; padding: 20px; }");
                
                m_mprViewerTab->layout()->addWidget(infoDisplay);
                m_mprViewerTab->layout()->addWidget(infoLabel);
                
                updateStatus("2D图像无法进行MPR重建");
                return;
            }
            
            // 创建主容器 - 垂直布局
            QWidget* mainContainer = new QWidget();
            QVBoxLayout* mainLayout = new QVBoxLayout(mainContainer);
            mainLayout->setContentsMargins(5, 5, 5, 5);
            mainLayout->setSpacing(5);
            
            // 创建MPR视图容器（居中放大）
            QWidget* mprContainer = new QWidget();
            QHBoxLayout* mprHLayout = new QHBoxLayout(mprContainer);
            mprHLayout->setContentsMargins(0, 0, 0, 0);
            
            // 创建MPR 2x2网格（响应式布局）
            QWidget* mprWidget = new QWidget();
            mprWidget->setMinimumSize(1200, 800);  // 最小尺寸更大
            mprWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // 支持缩放
            QGridLayout* mprLayout = new QGridLayout(mprWidget);
            mprLayout->setSpacing(5);
            mprLayout->setContentsMargins(10, 10, 10, 10);
            
            // 轴状面 (Axial) - 左上
            QWidget* axialWidget = createMPRViewWidget("轴状面 (Axial)", imageId, "axial", imgWidth, imgHeight, imgDepth);
            mprLayout->addWidget(axialWidget, 0, 0);
            
            // 冠状面 (Coronal) - 右上
            QWidget* coronalWidget = createMPRViewWidget("冠状面 (Coronal)", imageId, "coronal", imgWidth, imgDepth, imgHeight);
            mprLayout->addWidget(coronalWidget, 0, 1);
            
            // 矢状面 (Sagittal) - 左下 - 修正参数顺序
            QWidget* sagittalWidget = createMPRViewWidget("矢状面 (Sagittal)", imageId, "sagittal", imgHeight, imgDepth, imgWidth);
            mprLayout->addWidget(sagittalWidget, 1, 0);
            
            // 3D概览 - 右下
            QWidget* overviewWidget = createMPROverviewWidget("3D概览", imageId, imgWidth, imgHeight, imgDepth);
            mprLayout->addWidget(overviewWidget, 1, 1);
            
            // 将MPR网格居中
            mprHLayout->addStretch();
            mprHLayout->addWidget(mprWidget);
            mprHLayout->addStretch();
            
            // 创建下方信息区域（水平布局）
            QWidget* bottomInfoContainer = new QWidget();
            QHBoxLayout* bottomLayout = new QHBoxLayout(bottomInfoContainer);
            bottomLayout->setContentsMargins(0, 0, 0, 0);
            bottomLayout->setSpacing(10);
            
            // 左侧：图像信息
            infoDisplay->setMaximumHeight(120);  // 限制信息面板高度
            infoDisplay->setMinimumWidth(400);
            
            // 右侧：MPR控制面板
            QWidget* controlPanel = createMPRControls(imageId);
            controlPanel->setMaximumHeight(120);
            controlPanel->setMinimumWidth(300);
            
            bottomLayout->addWidget(infoDisplay);
            bottomLayout->addWidget(controlPanel);
            bottomLayout->addStretch();
            
            // 组装主布局
            mainLayout->addWidget(mprContainer, 1);  // MPR视图占主要空间
            mainLayout->addWidget(bottomInfoContainer);  // 底部信息区域
            
            // 添加到MPR标签页
            m_mprViewerTab->layout()->addWidget(mainContainer);
            
            updateStatus(QString("MPR重建成功: %1 (%2×%3×%4)").arg(imageId.left(8) + "...").arg(imgWidth).arg(imgHeight).arg(imgDepth));
            
        } catch (const std::exception& e) {
            // 显示错误信息
            QLabel* errorLabel = new QLabel(QString("MPR重建失败:\n%1").arg(e.what()));
            errorLabel->setAlignment(Qt::AlignCenter);
            errorLabel->setStyleSheet("QLabel { color: red; font-size: 14px; }");
            m_mprViewerTab->layout()->addWidget(infoDisplay);
            m_mprViewerTab->layout()->addWidget(errorLabel);
            updateStatus("MPR重建失败: " + QString(e.what()));
        }
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::renderVolumeImage(const QString& imageId, const QString& infoText)
{
    qDebug() << "[MedicalViewerWidget] 开始体绘制渲染:" << imageId.left(8) + "...";
    
    // 暂时显示开发中信息
    if (m_volumeRenderingTab) {
        if (m_volumeRenderingTab->layout()) {
            QLayoutItem* item;
            while ((item = m_volumeRenderingTab->layout()->takeAt(0)) != nullptr) {
                delete item->widget();
                delete item;
            }
        } else {
            QVBoxLayout* layout = new QVBoxLayout(m_volumeRenderingTab);
            m_volumeRenderingTab->setLayout(layout);
        }
        
        QLabel* developingLabel = new QLabel("体绘制功能开发中...\n\n将集成VTK Volume Rendering");
        developingLabel->setAlignment(Qt::AlignCenter);
        developingLabel->setStyleSheet("QLabel { color: #666; font-size: 14px; border: 2px dashed #ccc; padding: 20px; }");
        m_volumeRenderingTab->layout()->addWidget(developingLabel);
        
        updateStatus("体绘制功能开发中");
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::renderAdvancedVisualization(const QString& imageId, const QString& infoText)
{
    qDebug() << "[MedicalViewerWidget] 开始高级可视化渲染:" << imageId.left(8) + "...";
    
    // 暂时显示开发中信息
    if (m_advancedVisualizationTab) {
        if (m_advancedVisualizationTab->layout()) {
            QLayoutItem* item;
            while ((item = m_advancedVisualizationTab->layout()->takeAt(0)) != nullptr) {
                delete item->widget();
                delete item;
            }
        } else {
            QVBoxLayout* layout = new QVBoxLayout(m_advancedVisualizationTab);
            m_advancedVisualizationTab->setLayout(layout);
        }
        
        QLabel* developingLabel = new QLabel("高级可视化功能开发中...\n\n将提供科研级图像分析工具");
        developingLabel->setAlignment(Qt::AlignCenter);
        developingLabel->setStyleSheet("QLabel { color: #666; font-size: 14px; border: 2px dashed #ccc; padding: 20px; }");
        m_advancedVisualizationTab->layout()->addWidget(developingLabel);
        
        updateStatus("高级可视化功能开发中");
    }
}

//-----------------------------------------------------------------------------
QPixmap MedicalViewerWidget::applyWindowLevel(const QPixmap& originalPixmap, double windowCenter, double windowWidth)
{
    if (originalPixmap.isNull() || windowWidth <= 0) {
        return originalPixmap;
    }
    
    qDebug() << "[MedicalViewerWidget] 🖼️ 应用窗宽窗位:" << "中心=" << windowCenter << "宽度=" << windowWidth;
    
    // 标准医学图像窗宽窗位算法
    // 计算窗口的上下边界
    double windowMin = windowCenter - (windowWidth / 2.0);
    double windowMax = windowCenter + (windowWidth / 2.0);
    
    qDebug() << "[MedicalViewerWidget] 📊 窗口范围:" << windowMin << "到" << windowMax;
    
    QImage image = originalPixmap.toImage();
    
    // 应用窗宽窗位变换
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QRgb pixel = image.pixel(x, y);
            
            // 对于灰度图像，使用灰度值；对于彩色图像，计算亮度
            double intensity;
            if (qRed(pixel) == qGreen(pixel) && qGreen(pixel) == qBlue(pixel)) {
                // 灰度图像
                intensity = qRed(pixel);
            } else {
                // 彩色图像，计算亮度 (YUV亮度公式)
                intensity = 0.299 * qRed(pixel) + 0.587 * qGreen(pixel) + 0.114 * qBlue(pixel);
            }
            
            // 应用标准窗宽窗位公式
            double newIntensity;
            if (intensity <= windowMin) {
                newIntensity = 0.0;  // 窗口以下全黑
            } else if (intensity >= windowMax) {
                newIntensity = 255.0;  // 窗口以上全白
            } else {
                // 线性映射到0-255范围
                newIntensity = ((intensity - windowMin) / windowWidth) * 255.0;
            }
            
            // 限制在0-255范围内
            int outputValue = qBound(0, static_cast<int>(newIntensity), 255);
            
            // 如果原图是灰度图，保持灰度；否则保持原色调但调整亮度
            int newR, newG, newB;
            if (qRed(pixel) == qGreen(pixel) && qGreen(pixel) == qBlue(pixel)) {
                // 灰度图像 - 直接设置为新的强度值
                newR = newG = newB = outputValue;
            } else {
                // 彩色图像 - 保持色调，只调整亮度
                double factor = (intensity > 0) ? (outputValue / intensity) : 1.0;
                factor = qBound(0.0, factor, 3.0);  // 限制缩放因子
                
                newR = qBound(0, static_cast<int>(qRed(pixel) * factor), 255);
                newG = qBound(0, static_cast<int>(qGreen(pixel) * factor), 255);
                newB = qBound(0, static_cast<int>(qBlue(pixel) * factor), 255);
            }
            
            image.setPixel(x, y, qRgb(newR, newG, newB));
        }
    }
    
    qDebug() << "[MedicalViewerWidget] ✅ 窗宽窗位处理完成";
    
    return QPixmap::fromImage(image);
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::createWindowLevelControls(const QString& imageId)
{
    QWidget* controlWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(controlWidget);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(8);
    
    // 标题
    QLabel* titleLabel = new QLabel("窗口/级别调整");
    titleLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; }");
    layout->addWidget(titleLabel);
    
    // 窗口中心 (级别) 控制
    QHBoxLayout* centerLayout = new QHBoxLayout();
    QLabel* centerLabel = new QLabel("级别:");
    centerLabel->setMinimumWidth(40);
    
    QSlider* centerSlider = new QSlider(Qt::Horizontal);
    centerSlider->setRange(0, 255);
    centerSlider->setValue(static_cast<int>(m_windowCenter));
    centerSlider->setObjectName("centerSlider");
    
    QSpinBox* centerSpinBox = new QSpinBox();
    centerSpinBox->setRange(0, 255);
    centerSpinBox->setValue(static_cast<int>(m_windowCenter));
    centerSpinBox->setObjectName("centerSpinBox");
    centerSpinBox->setMinimumWidth(60);
    
    centerLayout->addWidget(centerLabel);
    centerLayout->addWidget(centerSlider, 1);
    centerLayout->addWidget(centerSpinBox);
    layout->addLayout(centerLayout);
    
    // 窗口宽度控制
    QHBoxLayout* widthLayout = new QHBoxLayout();
    QLabel* widthLabel = new QLabel("窗口:");
    widthLabel->setMinimumWidth(40);
    
    QSlider* widthSlider = new QSlider(Qt::Horizontal);
    widthSlider->setRange(1, 512);
    widthSlider->setValue(static_cast<int>(m_windowWidth));
    widthSlider->setObjectName("widthSlider");
    
    QSpinBox* widthSpinBox = new QSpinBox();
    widthSpinBox->setRange(1, 512);
    widthSpinBox->setValue(static_cast<int>(m_windowWidth));
    widthSpinBox->setObjectName("widthSpinBox");
    widthSpinBox->setMinimumWidth(60);
    
    widthLayout->addWidget(widthLabel);
    widthLayout->addWidget(widthSlider, 1);
    widthLayout->addWidget(widthSpinBox);
    layout->addLayout(widthLayout);
    
    // 预设按钮
    QHBoxLayout* presetLayout = new QHBoxLayout();
    QPushButton* resetBtn = new QPushButton("重置");
    QPushButton* autoBtn = new QPushButton("自动");
    QPushButton* boneBtn = new QPushButton("骨窗");
    QPushButton* softBtn = new QPushButton("软组织");
    
    resetBtn->setMaximumWidth(50);
    autoBtn->setMaximumWidth(50);
    boneBtn->setMaximumWidth(60);
    softBtn->setMaximumWidth(60);
    
    presetLayout->addWidget(resetBtn);
    presetLayout->addWidget(autoBtn);
    presetLayout->addWidget(boneBtn);
    presetLayout->addWidget(softBtn);
    presetLayout->addStretch();
    layout->addLayout(presetLayout);
    
    // 连接信号槽
    auto updateWindowLevel = [this, centerSlider, centerSpinBox, widthSlider, widthSpinBox]() {
        m_windowCenter = centerSlider->value();
        m_windowWidth = widthSlider->value();
        
        // 同步控件值
        centerSpinBox->blockSignals(true);
        widthSpinBox->blockSignals(true);
        centerSpinBox->setValue(static_cast<int>(m_windowCenter));
        widthSpinBox->setValue(static_cast<int>(m_windowWidth));
        centerSpinBox->blockSignals(false);
        widthSpinBox->blockSignals(false);
        
        // 应用窗口/级别调整
        if (!m_originalPixmap.isNull() && m_currentPixmapItem && m_currentImageView) {
            QPixmap adjustedPixmap = applyWindowLevel(m_originalPixmap, m_windowCenter, m_windowWidth);
            m_currentPixmapItem->setPixmap(adjustedPixmap);
            
            updateStatus(QString("窗口/级别已调整: 级别=%1, 窗口=%2").arg(m_windowCenter).arg(m_windowWidth));
        }
    };
    
    // 连接滑动条信号
    connect(centerSlider, &QSlider::valueChanged, updateWindowLevel);
    connect(widthSlider, &QSlider::valueChanged, updateWindowLevel);
    
    // 连接数值框信号
    connect(centerSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [centerSlider](int value) {
        centerSlider->setValue(value);
    });
    connect(widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [widthSlider](int value) {
        widthSlider->setValue(value);
    });
    
    // 连接预设按钮
    connect(resetBtn, &QPushButton::clicked, [centerSlider, widthSlider, this]() {
        centerSlider->setValue(static_cast<int>(m_defaultWindowCenter));
        widthSlider->setValue(static_cast<int>(m_defaultWindowWidth));
    });
    
    connect(autoBtn, &QPushButton::clicked, [centerSlider, widthSlider, this]() {
        // 自动窗宽窗位 - 标准8位图像范围
        centerSlider->setValue(127);  // 中等灰度
        widthSlider->setValue(255);   // 全动态范围
        updateStatus("已应用自动窗宽窗位设置");
    });
    
    connect(boneBtn, &QPushButton::clicked, [centerSlider, widthSlider, this]() {
        // 骨窗 - 适合观察骨骼结构
        centerSlider->setValue(200);  // 高窗位突出骨骼
        widthSlider->setValue(300);   // 较宽窗宽显示骨骼细节
        updateStatus("已应用骨窗设置 (适合骨骼观察)");
    });
    
    connect(softBtn, &QPushButton::clicked, [centerSlider, widthSlider, this]() {
        // 软组织窗 - 适合观察软组织
        centerSlider->setValue(60);   // 低窗位突出软组织
        widthSlider->setValue(120);   // 较窄窗宽增强软组织对比度
        updateStatus("已应用软组织窗设置 (适合软组织观察)");
    });
    
    controlWidget->setStyleSheet(
        "QWidget { background-color: #f8f8f8; border: 1px solid #ddd; border-radius: 4px; }"
        "QSlider::groove:horizontal { height: 6px; background: #e0e0e0; border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 12px; height: 12px; background: #4285f4; border-radius: 6px; margin: -3px 0; }"
        "QPushButton { padding: 4px 8px; border: 1px solid #ccc; border-radius: 3px; background-color: #fff; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
        "QPushButton:pressed { background-color: #e0e0e0; }"
    );
    
    return controlWidget;
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::create3DControls(const QString& imageId)
{
    QWidget* controlWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(controlWidget);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(8);
    
    // 标题
    QLabel* titleLabel = new QLabel("3D体渲染控制");
    titleLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; }");
    layout->addWidget(titleLabel);
    
    // 传输函数控制
    QGroupBox* transferGroup = new QGroupBox("传输函数");
    QVBoxLayout* transferLayout = new QVBoxLayout(transferGroup);
    
    // 传输函数预设选择
    QHBoxLayout* presetLayout = new QHBoxLayout();
    QLabel* presetLabel = new QLabel("预设:");
    presetLabel->setMinimumWidth(60);
    
    QComboBox* transferPresetCombo = new QComboBox();
    transferPresetCombo->addItems({
        "🦴 骨骼显示 (高对比)",
        "🫁 软组织显示 (中等)",
        "🩸 血管显示 (透明)",
        "🧠 CT标准显示",
        "🔬 研究模式 (详细)",
        "⚡ 快速预览"
    });
    transferPresetCombo->setCurrentIndex(1); // 默认软组织
    transferPresetCombo->setObjectName("transferPresetCombo");
    
    presetLayout->addWidget(presetLabel);
    presetLayout->addWidget(transferPresetCombo, 1);
    transferLayout->addLayout(presetLayout);
    
    // 透明度控制
    QHBoxLayout* opacityLayout = new QHBoxLayout();
    QLabel* opacityLabel = new QLabel("透明度:");
    opacityLabel->setMinimumWidth(60);
    
    QSlider* opacitySlider = new QSlider(Qt::Horizontal);
    opacitySlider->setRange(0, 100);
    opacitySlider->setValue(50);
    opacitySlider->setObjectName("opacitySlider");
    
    QSpinBox* opacitySpinBox = new QSpinBox();
    opacitySpinBox->setRange(0, 100);
    opacitySpinBox->setValue(50);
    opacitySpinBox->setSuffix("%");
    opacitySpinBox->setMinimumWidth(60);
    
    opacityLayout->addWidget(opacityLabel);
    opacityLayout->addWidget(opacitySlider, 1);
    opacityLayout->addWidget(opacitySpinBox);
    transferLayout->addLayout(opacityLayout);
    
    layout->addWidget(transferGroup);
    
    // 渲染质量控制
    QGroupBox* qualityGroup = new QGroupBox("渲染质量");
    QVBoxLayout* qualityLayout = new QVBoxLayout(qualityGroup);
    
    QComboBox* qualityCombo = new QComboBox();
    qualityCombo->addItems({"低质量 (快速)", "中等质量", "高质量 (精细)", "超高质量"});
    qualityCombo->setCurrentIndex(1);
    qualityLayout->addWidget(qualityCombo);
    
    layout->addWidget(qualityGroup);
    
    // 光照控制
    QGroupBox* lightingGroup = new QGroupBox("光照设置");
    QVBoxLayout* lightingLayout = new QVBoxLayout(lightingGroup);
    
    QCheckBox* shadingCheck = new QCheckBox("启用阴影");
    shadingCheck->setChecked(true);
    lightingLayout->addWidget(shadingCheck);
    
    QCheckBox* ambientCheck = new QCheckBox("环境光");
    ambientCheck->setChecked(true);
    lightingLayout->addWidget(ambientCheck);
    
    layout->addWidget(lightingGroup);
    
    // 相机控制
    QGroupBox* cameraGroup = new QGroupBox("视角控制");
    QVBoxLayout* cameraLayout = new QVBoxLayout(cameraGroup);
    
    QHBoxLayout* viewButtons = new QHBoxLayout();
    QPushButton* frontBtn = new QPushButton("正面");
    QPushButton* sideBtn = new QPushButton("侧面");
    QPushButton* topBtn = new QPushButton("顶部");
    QPushButton* resetBtn = new QPushButton("重置");
    
    frontBtn->setMaximumWidth(50);
    sideBtn->setMaximumWidth(50);
    topBtn->setMaximumWidth(50);
    resetBtn->setMaximumWidth(50);
    
    viewButtons->addWidget(frontBtn);
    viewButtons->addWidget(sideBtn);
    viewButtons->addWidget(topBtn);
    viewButtons->addWidget(resetBtn);
    cameraLayout->addLayout(viewButtons);
    
    layout->addWidget(cameraGroup);
    
    // 导出功能
    QGroupBox* exportGroup = new QGroupBox("导出");
    QVBoxLayout* exportLayout = new QVBoxLayout(exportGroup);
    
    QPushButton* screenshotBtn = new QPushButton("截图");
    QPushButton* exportBtn = new QPushButton("导出3D模型");
    
    exportLayout->addWidget(screenshotBtn);
    exportLayout->addWidget(exportBtn);
    
    layout->addWidget(exportGroup);
    
    layout->addStretch();
    
    // 连接信号槽
    // 1. 滑块和数值框同步
    connect(opacitySlider, &QSlider::valueChanged, [opacitySpinBox](int value) {
        opacitySpinBox->setValue(value);
    });
    
    connect(opacitySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [opacitySlider](int value) {
        opacitySlider->setValue(value);
    });
    
    // 2. 透明度实际功能 - 直接操作VTK
    connect(opacitySlider, &QSlider::valueChanged, [this](int value) {
        double opacity = value / 100.0;
        qDebug() << "[MedicalViewerWidget] 🎨 3D透明度调整:" << opacity;
        
#ifdef VTK_FOUND
        // 直接操作VTK透明度传输函数
        if (m_vtkOpacityFunction && m_vtkRenderer && m_vtkWidget) {
            // 获取图像数据范围
            double range[2];
            if (m_vtkVolume && m_vtkVolume->GetMapper()) {
                vtkDataObject* dataObj = m_vtkVolume->GetMapper()->GetInputDataObject(0, 0);
                vtkDataSet* inputData = vtkDataSet::SafeDownCast(dataObj);
                if (inputData) {
                    inputData->GetScalarRange(range);
                    
                    // 重新设置透明度传输函数，应用新的整体透明度
                    m_vtkOpacityFunction->RemoveAllPoints();
                    m_vtkOpacityFunction->AddPoint(range[0], 0.0);                       // 最小值：透明
                    m_vtkOpacityFunction->AddPoint(range[0] + (range[1] - range[0]) * 0.1, 0.0);  // 低值：透明
                    m_vtkOpacityFunction->AddPoint(range[0] + (range[1] - range[0]) * 0.3, 0.1 * opacity);  // 开始可见
                    m_vtkOpacityFunction->AddPoint(range[0] + (range[1] - range[0]) * 0.6, 0.5 * opacity);  // 半透明
                    m_vtkOpacityFunction->AddPoint(range[1], 0.8 * opacity);             // 最大值：根据滑块调整
                    
                    // 更新渲染
                    m_vtkWidget->renderWindow()->Render();
                    
                    qDebug() << "[MedicalViewerWidget] ✅ 3D VTK透明度更新成功";
                }
            }
        }
#endif
        
        updateStatus(QString("3D体绘制透明度: %1%").arg(value));
    });
    
    // 传输函数预设选择功能
    connect(transferPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        qDebug() << "[MedicalViewerWidget] 🎨 传输函数预设变更:" << index;
        
#ifdef VTK_FOUND
        if (m_vtkColorFunction && m_vtkOpacityFunction && m_vtkRenderer && m_vtkWidget) {
            // 获取图像数据范围
            double range[2];
            if (m_vtkVolume && m_vtkVolume->GetMapper()) {
                vtkDataObject* dataObj = m_vtkVolume->GetMapper()->GetInputDataObject(0, 0);
                vtkDataSet* inputData = vtkDataSet::SafeDownCast(dataObj);
                if (inputData) {
                    inputData->GetScalarRange(range);
                    applyTransferFunctionPreset(index, range);
                    m_vtkWidget->renderWindow()->Render();
                    qDebug() << "[MedicalViewerWidget] ✅ 传输函数预设应用成功";
                }
            }
        }
#endif
        
        QStringList presetNames = {"骨骼显示", "软组织显示", "血管显示", "CT标准显示", "研究模式", "快速预览"};
        updateStatus(QString("3D传输函数: %1").arg(presetNames.value(index, "未知")));
    });
    
    connect(qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        qDebug() << "[MedicalViewerWidget] 渲染质量改变为索引:" << index;
        updateStatus(QString("3D渲染质量: %1").arg(index));
    });
    
    // 3. 视角控制按钮功能
    connect(frontBtn, &QPushButton::clicked, [this]() {
        setCamera3DView("front");
    });
    
    connect(sideBtn, &QPushButton::clicked, [this]() {
        setCamera3DView("side");
    });
    
    connect(topBtn, &QPushButton::clicked, [this]() {
        setCamera3DView("top");
    });
    
    connect(resetBtn, &QPushButton::clicked, [this]() {
        setCamera3DView("reset");
    });
    
    // 样式设置
    controlWidget->setStyleSheet(
        "QGroupBox { font-weight: bold; border: 1px solid #ccc; border-radius: 4px; margin: 4px 0; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QSlider::groove:horizontal { height: 6px; background: #e0e0e0; border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 12px; height: 12px; background: #4285f4; border-radius: 6px; margin: -3px 0; }"
        "QPushButton { padding: 4px 8px; border: 1px solid #ccc; border-radius: 3px; background-color: #fff; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
        "QPushButton:pressed { background-color: #e0e0e0; }"
    );
    
    return controlWidget;
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::createMPRViewWidget(const QString& title, const QString& imageId, const QString& viewType, int viewWidth, int viewHeight, int sliceCount)
{
    QWidget* viewWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(viewWidget);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    
    // 添加异常处理和边界检查
    try {
    
    // 标题栏（减小高度）
    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("QLabel { background-color: #e0e0e0; border: 1px solid #ccc; padding: 1px 3px; font-weight: bold; font-size: 10px; }");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setMaximumHeight(20);  // 限制标题栏高度
    layout->addWidget(titleLabel);
    
    // 图像显示区域（响应式尺寸）
    QLabel* imageLabel = new QLabel();
    imageLabel->setMinimumSize(560, 400);  // 更大的最小尺寸
    imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // 支持缩放
    imageLabel->setStyleSheet("QLabel { border: 1px solid #ccc; background-color: #000; }");
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setScaledContents(true);  // 内容随标签缩放
    
    // 尝试从真实医学图像数据创建切片（合理分辨率）
    QPixmap slicePixmap(480, 360);  // 使用更安全的480x360分辨率
    slicePixmap.fill(Qt::black);
    
    QPainter painter(&slicePixmap);
    painter.setPen(QPen(Qt::white, 1));
    painter.setRenderHint(QPainter::Antialiasing);
    
        // 获取图像服务并创建真实切片 - 异步处理
    bool sliceCreated = false;

    if (auto coreService = getImageCoreService()) {
        qDebug() << "[MedicalViewerWidget] ===================";
        qDebug() << "[MedicalViewerWidget] 启动异步MPR切片提取";
        qDebug() << "[MedicalViewerWidget] 图像ID:" << imageId;
        qDebug() << "[MedicalViewerWidget] 视图类型:" << viewType;
        
        // 创建加载指示器
        QLabel* loadingLabel = new QLabel("🔄 正在处理切片...");
        loadingLabel->setAlignment(Qt::AlignCenter);
        loadingLabel->setStyleSheet("QLabel { color: #666; font-size: 12px; }");
        layout->addWidget(loadingLabel);
        
        // 异步获取切片数据 - 不阻塞UI线程
        auto future = QtConcurrent::run([coreService, imageId, viewType, viewWidth, viewHeight, sliceCount]() -> QPixmap {
            void* pixelData = coreService->getImagePixelData(imageId);
            QString dataType = coreService->getImageDataType(imageId);
            QList<int> dimensions = coreService->getImageDimensions(imageId);
            
            qDebug() << "[AsyncMPR] 工作线程 - 数据类型:" << dataType;
            qDebug() << "[AsyncMPR] 工作线程 - 维度:" << dimensions;
            qDebug() << "[AsyncMPR] 工作线程 - 像素数据指针:" << pixelData;
        
            if (pixelData && dimensions.size() >= 3) {
                int width = dimensions[0];
                int height = dimensions[1];
                int depth = dimensions[2];
                
                // 边界检查
                if (width <= 0 || height <= 0 || depth <= 0) {
                    qDebug() << "[AsyncMPR] ❌ 无效的图像维度:" << width << "x" << height << "x" << depth;
                    return QPixmap();
                }
                
                if (width > 2048 || height > 2048 || depth > 2048) {
                    qDebug() << "[AsyncMPR] ⚠️ 图像尺寸过大:" << width << "x" << height << "x" << depth;
                }
                
                // 在工作线程中创建切片
                QPixmap slicePixmap(480, 360);
                slicePixmap.fill(Qt::black);
                
                QPainter painter(&slicePixmap);
                painter.setRenderHint(QPainter::Antialiasing);
            
                // 计算中心切片索引
                int centerSlice = 0;
                if (viewType == "axial") {
                    centerSlice = depth / 2;
                } else if (viewType == "coronal") {
                    centerSlice = height / 2;
                } else if (viewType == "sagittal") {
                    centerSlice = width / 2;
                }
                
                qDebug() << "[AsyncMPR]" << viewType << "视图 - 图像尺寸:" << width << "x" << height << "x" << depth << "中心切片:" << centerSlice;
            
                // 处理不同数据类型 - 支持多种数据类型
                QImage sliceImage(480, 360, QImage::Format_Grayscale8);
                sliceImage.fill(Qt::black);
                bool sliceCreated = false;
                
                // 专门处理NRRD常见的unsigned char类型
                if (dataType == "unsigned char" || dataType == "UChar") {
                    // 支持unsigned char类型（常见于NRRD分割图像）
                    unsigned char* ucharData = static_cast<unsigned char*>(pixelData);

                    // 快速计算数据范围
                    unsigned char minVal = ucharData[0], maxVal = ucharData[0];
                    int totalPixels = width * height * depth;
                    int step = qMax(1, totalPixels / 10000);
                    for (int i = 0; i < totalPixels; i += step) {
                        unsigned char val = ucharData[i];
                        minVal = qMin(minVal, val);
                        maxVal = qMax(maxVal, val);
                    }

                    qDebug() << "[AsyncMPR] unsigned char数据范围:" << static_cast<int>(minVal) << "到" << static_cast<int>(maxVal);

                    // 提取切片数据
                    for (int y = 0; y < 360; y++) {
                        for (int x = 0; x < 480; x++) {
                            int origX = x * width / 480;
                            int origY = y * height / 360;

                            unsigned char value = 0;
                            bool validPixel = false;

                            if (viewType == "axial") {
                                if (origX >= 0 && origX < width && origY >= 0 && origY < height && centerSlice >= 0 && centerSlice < depth) {
                                    int index = centerSlice * width * height + origY * width + origX;
                                    if (index >= 0 && index < totalPixels) {
                                        value = ucharData[index];
                                        validPixel = true;
                                    }
                                }
                            } else if (viewType == "coronal") {
                                int origZ = y * depth / 360;
                                if (origX >= 0 && origX < width && centerSlice >= 0 && centerSlice < height && origZ >= 0 && origZ < depth) {
                                    int index = origZ * width * height + centerSlice * width + origX;
                                    if (index >= 0 && index < totalPixels) {
                                        value = ucharData[index];
                                        validPixel = true;
                                    }
                                }
                            } else if (viewType == "sagittal") {
                                // 矢状面：固定X (centerSlice)，显示YZ平面
                                int origY = x * height / 480;  // x方向对应Y轴 (前后)
                                int origZ = y * depth / 360;   // y方向对应Z轴 (上下)
                                if (centerSlice >= 0 && centerSlice < width && origY >= 0 && origY < height && origZ >= 0 && origZ < depth) {
                                    int index = origZ * width * height + origY * width + centerSlice;
                                    if (index >= 0 && index < totalPixels) {
                                        value = ucharData[index];
                                        validPixel = true;
                                    }
                                }
                            }

                            int grayValue = 0;
                            if (validPixel) {
                                if (maxVal > minVal) {
                                    grayValue = static_cast<int>((value - minVal) * 255 / (maxVal - minVal));
                                } else {
                                    grayValue = static_cast<int>(value);
                                }
                                grayValue = qBound(0, grayValue, 255);
                            }
                            sliceImage.setPixel(x, y, qRgb(grayValue, grayValue, grayValue));
                        }
                    }

                    sliceCreated = true;
                    qDebug() << "[AsyncMPR] MPR切片生成成功(unsigned char):" << viewType << "中心切片:" << centerSlice;

                } else {
                    qDebug() << "[AsyncMPR] ❌ 不支持的数据类型:" << dataType;
                    // 对于不支持的类型，创建占位符
                    QPainter painter(&slicePixmap);
                    painter.setPen(QPen(Qt::cyan, 2));
                    painter.drawText(50, 180, QString("不支持的数据类型: %1").arg(dataType));
                    painter.end();
                    return slicePixmap;
                }
                
                // 如果成功创建切片，绘制到pixmap
                if (sliceCreated) {
                    painter.drawImage(0, 0, sliceImage);
                } else {
                    // 绘制占位符
                    painter.setPen(QPen(Qt::cyan, 2));
                    painter.drawText(50, 180, QString("%1 - 数据加载中").arg(viewType));
                }
                
                // 添加十字线和标签
                painter.setPen(QPen(Qt::red, 1));
                painter.drawLine(240, 0, 240, 360);  // 垂直线
                painter.drawLine(0, 180, 480, 180);  // 水平线
                
                painter.setPen(QPen(Qt::yellow, 2));
                painter.setFont(QFont("Arial", 10, QFont::Bold));
                if (viewType == "axial") {
                    painter.drawText(10, 350, "轴状面");
                } else if (viewType == "coronal") {
                    painter.drawText(10, 350, "冠状面");
                } else if (viewType == "sagittal") {
                    painter.drawText(10, 350, "矢状面");
                }
                
                painter.end();
                return slicePixmap;
            } else {
                qDebug() << "[AsyncMPR] ❌ 无效的像素数据或维度";
                QPixmap errorPixmap(480, 360);
                errorPixmap.fill(Qt::black);
                QPainter painter(&errorPixmap);
                painter.setPen(QPen(Qt::red, 2));
                painter.drawText(100, 180, "无法获取图像数据");
                painter.end();
                return errorPixmap;
            }
        });
        
        // 设置异步完成监听器
        auto* watcher = new QFutureWatcher<QPixmap>(this);
        connect(watcher, &QFutureWatcher<QPixmap>::finished, [this, watcher, imageLabel, loadingLabel, layout]() {
            // 获取异步结果
            QPixmap result = watcher->result();
            
            // 在UI线程中更新界面
            QMetaObject::invokeMethod(this, [imageLabel, loadingLabel, layout, result]() {
                // 移除加载指示器
                layout->removeWidget(loadingLabel);
                loadingLabel->deleteLater();
                
                // 设置最终的图像
                imageLabel->setPixmap(result);
                layout->addWidget(imageLabel);
                
                qDebug() << "[AsyncMPR] ✅ UI更新完成";
            }, Qt::QueuedConnection);
            
            // 清理watcher
            watcher->deleteLater();
        });
        
        // 启动异步任务
        watcher->setFuture(future);
        
        qDebug() << "[MedicalViewerWidget] 异步MPR任务已启动";
        qDebug() << "[MedicalViewerWidget] =====================";
    } else {
        qDebug() << "[MedicalViewerWidget] ❌ 无法获取图像核心服务";
        // 创建错误显示
        QLabel* errorLabel = new QLabel("无法获取图像服务");
        errorLabel->setStyleSheet("QLabel { color: red; }");
        layout->addWidget(errorLabel);
    }
    
    // 切片选择滑动条（紧凑布局）
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->setContentsMargins(2, 2, 2, 2);
    sliderLayout->setSpacing(3);
    
    QLabel* sliceLabel = new QLabel("切片:");
    sliceLabel->setMinimumWidth(25);
    sliceLabel->setStyleSheet("font-size: 9px;");
    
    QSlider* sliceSlider = new QSlider(Qt::Horizontal);
    sliceSlider->setRange(1, sliceCount);
    sliceSlider->setValue(sliceCount / 2);
    sliceSlider->setMaximumHeight(15);
    
    QSpinBox* sliceSpinBox = new QSpinBox();
    sliceSpinBox->setRange(1, sliceCount);
    sliceSpinBox->setValue(sliceCount / 2);
    sliceSpinBox->setMaximumWidth(45);
    sliceSpinBox->setMaximumHeight(20);
    sliceSpinBox->setStyleSheet("font-size: 9px;");
    
    sliderLayout->addWidget(sliceLabel);
    sliderLayout->addWidget(sliceSlider, 1);
    sliderLayout->addWidget(sliceSpinBox);
    layout->addLayout(sliderLayout);
    
    // 创建MPR视图信息结构
    MedicalViewerWidget::MPRViewInfo* viewInfo = new MedicalViewerWidget::MPRViewInfo();
    viewInfo->imageId = imageId;
    viewInfo->viewType = viewType;
    viewInfo->imageLabel = imageLabel;
    viewInfo->sliceSlider = sliceSlider;
    viewInfo->sliceSpinBox = sliceSpinBox;
    viewInfo->totalSlices = sliceCount;
    viewInfo->currentSlice = sliceCount / 2;
    
    // 直接实时更新，无需定时器
    viewInfo->currentWatcher = nullptr;
    
    // 初始化任务队列和同步控制
    viewInfo->debounceTimer = new QTimer(this);
    viewInfo->debounceTimer->setSingleShot(true);
    viewInfo->debounceTimer->setInterval(30); // 30ms防抖动
    viewInfo->taskMutex = new QMutex();
    viewInfo->taskRunning = false;
    viewInfo->pendingSlice = -1;
    
    // 保存视图信息
    m_mprViews.append(viewInfo);
    
    // 连接切片选择信号 - 滑块和数值框同步
    connect(sliceSlider, &QSlider::valueChanged, sliceSpinBox, &QSpinBox::setValue);
    connect(sliceSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), sliceSlider, &QSlider::setValue);
    
    // 连接到实时切片更新 - 无加载提示，流畅体验
    connect(sliceSlider, &QSlider::valueChanged, this, &MedicalViewerWidget::onMPRSliceChanged);
    connect(sliceSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MedicalViewerWidget::onMPRSliceChanged);
    
    viewWidget->setStyleSheet("QWidget { border: 1px solid #aaa; }");
    
    } catch (const std::exception& e) {
        qDebug() << "[MedicalViewerWidget] ❌ MPR视图创建异常:" << e.what();
        
        // 创建错误显示
        QLabel* errorLabel = new QLabel(QString("创建%1视图时发生错误:\n%2").arg(title).arg(e.what()));
        errorLabel->setStyleSheet("QLabel { color: red; background-color: #ffe6e6; border: 1px solid red; padding: 10px; }");
        errorLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(errorLabel);
        
        qDebug() << "[MedicalViewerWidget] 已添加错误显示标签";
        
    } catch (...) {
        qDebug() << "[MedicalViewerWidget] ❌ MPR视图创建发生未知异常";
        
        // 创建通用错误显示
        QLabel* errorLabel = new QLabel(QString("创建%1视图时发生未知错误").arg(title));
        errorLabel->setStyleSheet("QLabel { color: red; background-color: #ffe6e6; border: 1px solid red; padding: 10px; }");
        errorLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(errorLabel);
        
        qDebug() << "[MedicalViewerWidget] 已添加未知错误显示标签";
    }
    
    return viewWidget;
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::createMPROverviewWidget(const QString& title, const QString& imageId, int imgWidth, int imgHeight, int imgDepth)
{
    QWidget* overviewWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(overviewWidget);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    
    // 标题栏
    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("QLabel { background-color: #e0e0e0; border: 1px solid #ccc; padding: 2px 5px; font-weight: bold; font-size: 11px; }");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    // 3D概览显示区域
    QLabel* overviewLabel = new QLabel();
    overviewLabel->setMinimumSize(200, 200);
    overviewLabel->setStyleSheet("QLabel { border: 1px solid #ccc; background-color: #001122; }");
    overviewLabel->setAlignment(Qt::AlignCenter);
    
    // 创建3D概览图像
    QPixmap overviewPixmap(200, 200);
    overviewPixmap.fill(QColor(0, 17, 34));
    
    QPainter painter(&overviewPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::white, 2));
    
    // 绘制3D立方体框架
    painter.drawRect(50, 50, 100, 100);
    painter.drawRect(70, 30, 100, 100);
    
    // 连接前后面
    painter.drawLine(50, 50, 70, 30);
    painter.drawLine(150, 50, 170, 30);
    painter.drawLine(150, 150, 170, 130);
    painter.drawLine(50, 150, 70, 130);
    
    // 绘制切片平面指示
    painter.setPen(QPen(Qt::red, 1, Qt::DashLine));
    painter.drawLine(50, 100, 150, 100);
    painter.setPen(QPen(Qt::green, 1, Qt::DashLine));
    painter.drawLine(100, 50, 100, 150);
    painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
    painter.drawLine(110, 30, 110, 130);
    
    painter.setPen(QPen(Qt::white, 1));
    painter.drawText(10, 20, QString("%1×%2×%3").arg(imgWidth).arg(imgHeight).arg(imgDepth));
    
    painter.end();
    
    overviewLabel->setPixmap(overviewPixmap);
    layout->addWidget(overviewLabel);
    
    QLabel* syncLabel = new QLabel("3D定位视图");
    syncLabel->setStyleSheet("QLabel { font-size: 10px; color: #666; }");
    syncLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(syncLabel);
    
    overviewWidget->setStyleSheet("QWidget { border: 1px solid #aaa; }");
    
    return overviewWidget;
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerWidget::createMPRControls(const QString& imageId)
{
    QWidget* controlWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(controlWidget);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(8);
    
    // 标题
    QLabel* titleLabel = new QLabel("MPR控制");
    titleLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; }");
    layout->addWidget(titleLabel);
    
    // 切片同步控制
    QGroupBox* syncGroup = new QGroupBox("切片同步");
    QVBoxLayout* syncLayout = new QVBoxLayout(syncGroup);
    
    QCheckBox* syncCheck = new QCheckBox("启用视图同步");
    syncCheck->setChecked(true);
    syncLayout->addWidget(syncCheck);
    
    QCheckBox* crosshairCheck = new QCheckBox("显示十字线");
    crosshairCheck->setChecked(true);
    syncLayout->addWidget(crosshairCheck);
    
    layout->addWidget(syncGroup);
    
    // 测量工具
    QGroupBox* measureGroup = new QGroupBox("测量工具");
    QVBoxLayout* measureLayout = new QVBoxLayout(measureGroup);
    
    QPushButton* distanceBtn = new QPushButton("距离测量");
    QPushButton* angleBtn = new QPushButton("角度测量");
    QPushButton* clearBtn = new QPushButton("清除测量");
    
    measureLayout->addWidget(distanceBtn);
    measureLayout->addWidget(angleBtn);
    measureLayout->addWidget(clearBtn);
    
    layout->addWidget(measureGroup);
    
    layout->addStretch();
    
    // 连接信号槽
    connect(syncCheck, &QCheckBox::toggled, [this](bool checked) {
        updateStatus(QString("MPR视图同步: %1").arg(checked ? "启用" : "禁用"));
    });
    
    // 样式设置
    controlWidget->setStyleSheet(
        "QGroupBox { font-weight: bold; border: 1px solid #ccc; border-radius: 4px; margin: 4px 0; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QPushButton { padding: 4px 8px; border: 1px solid #ccc; border-radius: 3px; background-color: #fff; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
        "QPushButton:pressed { background-color: #e0e0e0; }"
    );
    
    return controlWidget;
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::enableMeasurement(QGraphicsView* imageView, const QString& measureType)
{
    if (!imageView || !imageView->scene()) {
        return;
    }
    
    QGraphicsScene* scene = imageView->scene();
    
    if (measureType == "clear") {
        // 清除所有测量标记
        QList<QGraphicsItem*> items = scene->items();
        for (QGraphicsItem* item : items) {
            if (item->data(0).toString() == "measurement") {
                scene->removeItem(item);
                delete item;
            }
        }
        
        // 重置测量状态
        m_currentMeasurementMode = NoMeasurement;
        m_measurementPoints.clear();
        m_currentMeasurementView = nullptr;
        
        qDebug() << "[MedicalViewerWidget] 清除所有测量标记";
        return;
    }
    
    // 设置测量模式
    if (measureType == "distance") {
        m_currentMeasurementMode = DistanceMeasurement;
        m_currentMeasurementView = imageView;
        m_measurementPoints.clear();
        
        // 安装事件过滤器以捕获鼠标点击
        imageView->installEventFilter(this);
        imageView->scene()->installEventFilter(this);
        
        qDebug() << "[MedicalViewerWidget] 🔍 距离测量模式已启用 - 请点击两个点";
        updateStatus("距离测量模式 - 点击第一个点");
        
    } else if (measureType == "angle") {
        m_currentMeasurementMode = AngleMeasurement;
        m_currentMeasurementView = imageView;
        m_measurementPoints.clear();
        
        // 安装事件过滤器以捕获鼠标点击
        imageView->installEventFilter(this);
        imageView->scene()->installEventFilter(this);
        
        qDebug() << "[MedicalViewerWidget] 📐 角度测量模式已启用 - 请点击三个点";
        updateStatus("角度测量模式 - 点击第一个点（起始）");
    }
}

//-----------------------------------------------------------------------------
bool MedicalViewerWidget::eventFilter(QObject* obj, QEvent* event)
{
    // 处理测量工具的鼠标点击事件
    if (m_currentMeasurementMode != NoMeasurement && m_currentMeasurementView) {
        
        if (event->type() == QEvent::GraphicsSceneMousePress) {
            QGraphicsSceneMouseEvent* mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QPointF scenePos = mouseEvent->scenePos();
                
                // 处理测量点击
                bool handled = handleMeasurementClick(scenePos);
                if (handled) {
                    return true;  // 事件已处理
                }
            }
        }
    }
    
    // 调用父类的事件过滤器
    return QWidget::eventFilter(obj, event);
}

//-----------------------------------------------------------------------------
bool MedicalViewerWidget::handleMeasurementClick(QPointF scenePos)
{
    if (m_currentMeasurementMode == NoMeasurement || !m_currentMeasurementView) {
        return false;
    }
    
    QGraphicsScene* scene = m_currentMeasurementView->scene();
    if (!scene) {
        return false;
    }
    
    // 添加测量点
    m_measurementPoints.append(scenePos);
    
    // 在场景中添加点标记
    QColor pointColor = (m_currentMeasurementMode == DistanceMeasurement) ? Qt::red : Qt::blue;
    QGraphicsEllipseItem* point = scene->addEllipse(scenePos.x() - 3, scenePos.y() - 3, 6, 6, 
                                                   QPen(pointColor, 2), QBrush(pointColor));
    point->setData(0, "measurement");
    
    qDebug() << "[MedicalViewerWidget] 添加测量点:" << m_measurementPoints.size() << "位置:" << scenePos;
    
    // 根据测量模式处理点击
    if (m_currentMeasurementMode == DistanceMeasurement) {
        if (m_measurementPoints.size() == 1) {
            updateStatus("距离测量模式 - 点击第二个点");
        } else if (m_measurementPoints.size() == 2) {
            finalizeMeasurement();
            return true;
        }
        
    } else if (m_currentMeasurementMode == AngleMeasurement) {
        if (m_measurementPoints.size() == 1) {
            updateStatus("角度测量模式 - 点击第二个点（顶点）");
        } else if (m_measurementPoints.size() == 2) {
            updateStatus("角度测量模式 - 点击第三个点（结束）");
        } else if (m_measurementPoints.size() == 3) {
            finalizeMeasurement();
            return true;
        }
    }
    
    return true;
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::finalizeMeasurement()
{
    if (!m_currentMeasurementView || !m_currentMeasurementView->scene()) {
        return;
    }
    
    QGraphicsScene* scene = m_currentMeasurementView->scene();
    
    if (m_currentMeasurementMode == DistanceMeasurement && m_measurementPoints.size() == 2) {
        // 计算距离并绘制线条和标签
        QPointF p1 = m_measurementPoints[0];
        QPointF p2 = m_measurementPoints[1];
        
        double distance = calculateDistance(p1, p2);
        
        // 绘制测量线
        QGraphicsLineItem* line = scene->addLine(p1.x(), p1.y(), p2.x(), p2.y(), QPen(Qt::red, 2));
        line->setData(0, "measurement");
        
        // 计算标签位置（线段中点）
        QPointF labelPos((p1.x() + p2.x()) / 2, (p1.y() + p2.y()) / 2 - 20);
        
        // 添加距离标签
        QGraphicsTextItem* label = scene->addText(QString("%.2f px").arg(distance), QFont("Arial", 12));
        label->setPos(labelPos);
        label->setDefaultTextColor(Qt::red);
        label->setData(0, "measurement");
        
        qDebug() << "[MedicalViewerWidget] ✅ 距离测量完成:" << distance << "像素";
        updateStatus(QString("距离测量完成: %.2f px").arg(distance));
        
    } else if (m_currentMeasurementMode == AngleMeasurement && m_measurementPoints.size() == 3) {
        // 计算角度并绘制线条和标签
        QPointF p1 = m_measurementPoints[0];  // 起始点
        QPointF p2 = m_measurementPoints[1];  // 顶点
        QPointF p3 = m_measurementPoints[2];  // 结束点
        
        double angle = calculateAngle(p1, p2, p3);
        
        // 绘制两条线
        QGraphicsLineItem* line1 = scene->addLine(p2.x(), p2.y(), p1.x(), p1.y(), QPen(Qt::blue, 2));
        QGraphicsLineItem* line2 = scene->addLine(p2.x(), p2.y(), p3.x(), p3.y(), QPen(Qt::blue, 2));
        line1->setData(0, "measurement");
        line2->setData(0, "measurement");
        
        // 计算标签位置（顶点附近）
        QPointF labelPos(p2.x() + 15, p2.y() - 15);
        
        // 添加角度标签
        QGraphicsTextItem* label = scene->addText(QString("%.1f°").arg(angle), QFont("Arial", 12));
        label->setPos(labelPos);
        label->setDefaultTextColor(Qt::blue);
        label->setData(0, "measurement");
        
        qDebug() << "[MedicalViewerWidget] ✅ 角度测量完成:" << angle << "度";
        updateStatus(QString("角度测量完成: %.1f°").arg(angle));
    }
    
    // 重置测量状态
    m_currentMeasurementMode = NoMeasurement;
    m_measurementPoints.clear();
    
    // 移除事件过滤器
    if (m_currentMeasurementView) {
        m_currentMeasurementView->removeEventFilter(this);
        m_currentMeasurementView->scene()->removeEventFilter(this);
        m_currentMeasurementView = nullptr;
    }
}

//-----------------------------------------------------------------------------
double MedicalViewerWidget::calculateDistance(const QPointF& p1, const QPointF& p2)
{
    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    return sqrt(dx * dx + dy * dy);
}

//-----------------------------------------------------------------------------
double MedicalViewerWidget::calculateAngle(const QPointF& p1, const QPointF& p2, const QPointF& p3)
{
    // 计算两个向量：从顶点p2到p1和从顶点p2到p3
    QPointF v1 = p1 - p2;  // 向量1
    QPointF v2 = p3 - p2;  // 向量2
    
    // 计算向量的长度
    double len1 = sqrt(v1.x() * v1.x() + v1.y() * v1.y());
    double len2 = sqrt(v2.x() * v2.x() + v2.y() * v2.y());
    
    if (len1 == 0 || len2 == 0) {
        return 0.0;  // 避免除零
    }
    
    // 计算向量的点积
    double dotProduct = v1.x() * v2.x() + v1.y() * v2.y();
    
    // 计算夹角的余弦值
    double cosAngle = dotProduct / (len1 * len2);
    
    // 确保余弦值在有效范围内
    cosAngle = qMax(-1.0, qMin(1.0, cosAngle));
    
    // 计算角度（弧度转度）
    double angle = acos(cosAngle) * 180.0 / M_PI;
    
    return angle;
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::setCamera3DView(const QString& viewType)
{
#ifdef VTK_FOUND
    if (!m_vtkRenderer || !m_vtkWidget) {
        qDebug() << "[MedicalViewerWidget] ❌ VTK渲染器或控件未初始化";
        updateStatus("3D视角控制不可用：VTK未初始化");
        return;
    }
    
    vtkCamera* camera = m_vtkRenderer->GetActiveCamera();
    if (!camera) {
        qDebug() << "[MedicalViewerWidget] ❌ 无法获取VTK相机";
        return;
    }
    
    // 获取场景边界来计算合适的相机位置
    double bounds[6];
    m_vtkRenderer->ComputeVisiblePropBounds(bounds);
    
    // 计算场景中心点
    double center[3] = {
        (bounds[0] + bounds[1]) / 2.0,
        (bounds[2] + bounds[3]) / 2.0,
        (bounds[4] + bounds[5]) / 2.0
    };
    
    // 计算场景尺寸
    double size = sqrt(pow(bounds[1] - bounds[0], 2) + 
                       pow(bounds[3] - bounds[2], 2) + 
                       pow(bounds[5] - bounds[4], 2));
    
    double distance = size * 1.5;  // 相机距离
    
    qDebug() << "[MedicalViewerWidget] 🎥 设置3D视角:" << viewType;
    qDebug() << "[MedicalViewerWidget] 场景中心:" << center[0] << center[1] << center[2];
    qDebug() << "[MedicalViewerWidget] 场景尺寸:" << size << "相机距离:" << distance;
    
    // 设置焦点为场景中心
    camera->SetFocalPoint(center[0], center[1], center[2]);
    
    if (viewType == "front") {
        // 正面视图：从前方（-Y方向）观察
        camera->SetPosition(center[0], center[1] - distance, center[2]);
        camera->SetViewUp(0, 0, 1);  // Z轴向上
        updateStatus("已切换到正面视角");
        
    } else if (viewType == "side") {
        // 侧面视图：从右侧（+X方向）观察
        camera->SetPosition(center[0] + distance, center[1], center[2]);
        camera->SetViewUp(0, 0, 1);  // Z轴向上
        updateStatus("已切换到侧面视角");
        
    } else if (viewType == "top") {
        // 顶部视图：从上方（+Z方向）观察
        camera->SetPosition(center[0], center[1], center[2] + distance);
        camera->SetViewUp(0, 1, 0);  // Y轴向上
        updateStatus("已切换到顶部视角");
        
    } else if (viewType == "reset") {
        // 重置视角：等角视图（默认3D视角）
        camera->SetPosition(center[0] + distance * 0.7, 
                           center[1] - distance * 0.7, 
                           center[2] + distance * 0.7);
        camera->SetViewUp(0, 0, 1);  // Z轴向上
        updateStatus("已重置到默认3D视角");
        
    } else {
        qDebug() << "[MedicalViewerWidget] ❌ 未知的视角类型:" << viewType;
        updateStatus("未知的视角类型: " + viewType);
        return;
    }
    
    // 自动调整相机距离以适合整个场景
    m_vtkRenderer->ResetCameraClippingRange();
    
    // 刷新渲染
    m_vtkWidget->renderWindow()->Render();
    
    qDebug() << "[MedicalViewerWidget] ✅ 3D视角切换完成:" << viewType;
    
#else
    updateStatus("3D视角控制不可用：VTK未编译");
    qDebug() << "[MedicalViewerWidget] ❌ VTK未编译，无法设置3D视角";
#endif
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::initializeEventAdmin()
{
    if (!m_pluginContext) {
        qWarning() << "[MedicalViewerWidget] CTK插件上下文未设置，无法初始化EventAdmin";
        return;
    }

    // 获取EventAdmin服务
    ctkServiceReference eventAdminRef = m_pluginContext->getServiceReference<ctkEventAdmin>();
    if (eventAdminRef) {
        m_eventAdmin = m_pluginContext->getService<ctkEventAdmin>(eventAdminRef);
        if (m_eventAdmin) {
            qDebug() << "[MedicalViewerWidget] EventAdmin服务连接成功";

            // 设置定时器监听图像事件（简单的轮询方式）
            QTimer* eventTimer = new QTimer(this);
            eventTimer->setInterval(3000); // 3秒检查一次
            connect(eventTimer, &QTimer::timeout, this, [this]() {
                // 检查是否有新的图像数据
                static QStringList lastImageList;
                QStringList currentImageList;

                if (auto coreService = getImageCoreService()) {
                    currentImageList = coreService->getLoadedImages();
                }

                if (currentImageList != lastImageList) {
                    qDebug() << "[MedicalViewerWidget] 🔄 检测到图像列表变化，自动刷新";
                    this->refreshImageList();
                    lastImageList = currentImageList;

                    // 更新日志
                    if (m_logTextEdit) {
                        m_logTextEdit->append(QString("[%1] 🔄 图像列表已自动更新")
                                             .arg(QTime::currentTime().toString()));
                    }
                }
            });
            eventTimer->start();

            qDebug() << "[MedicalViewerWidget] ✅ 图像事件监听已启动 (间隔3秒)";
        } else {
            qWarning() << "[MedicalViewerWidget] 无法获取EventAdmin服务实例";
        }
    } else {
        qWarning() << "[MedicalViewerWidget] 未找到EventAdmin服务";
    }
}

//-----------------------------------------------------------------------------
void MedicalViewerWidget::handleImageEvent(const QString& eventType, const QString& imageId)
{
    qDebug() << "[MedicalViewerWidget] 处理图像事件:" << eventType << "imageId:" << imageId;

    if (eventType == "loaded" || eventType == "list_updated") {
        // 图像加载或列表更新，刷新图像列表
        refreshImageList();

        if (m_logTextEdit) {
            m_logTextEdit->append(QString("[%1] 📥 收到图像事件: %2")
                                 .arg(QTime::currentTime().toString())
                                 .arg(eventType));
        }
    } else if (eventType == "selected") {
        // 图像选择事件，可以自动加载显示
        if (!imageId.isEmpty()) {
            // 在图像选择器中选中对应的图像
            for (int i = 0; i < m_imageSelector->count(); ++i) {
                if (m_imageSelector->itemData(i).toString() == imageId) {
                    m_imageSelector->setCurrentIndex(i);
                    break;
                }
            }

            if (m_logTextEdit) {
                m_logTextEdit->append(QString("[%1] 🎯 收到图像选择事件: %2")
                                     .arg(QTime::currentTime().toString())
                                     .arg(imageId.left(8) + "..."));
            }
        }
    }
}
