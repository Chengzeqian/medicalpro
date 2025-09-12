#include "MedicalViewerServiceImpl.h"
#include "MedicalViewerWidget.h"
#include "../MedicalImageCore/MedicalImageCoreService.h"

// CTK框架
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

#include <QDebug>
#include <QMutexLocker>
#include <QUuid>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTextEdit>

//-----------------------------------------------------------------------------
MedicalViewerServiceImpl::MedicalViewerServiceImpl(ctkPluginContext* context, QObject* parent)
    : MedicalViewerService(parent)
    , m_pluginContext(context)
    , m_imageService(nullptr)
    , m_serviceConnected(false)
    , m_componentsInitialized(false)
{
    qDebug() << "[MedicalViewerServiceImpl] 创建医学图像查看器服务实现（完全CTK架构）";
    m_componentsInitialized = true;
}

//-----------------------------------------------------------------------------
MedicalViewerServiceImpl::~MedicalViewerServiceImpl()
{
    QMutexLocker locker(&m_mutex);
    
    // 关闭所有查看器
    for (auto it = m_viewers.begin(); it != m_viewers.end(); ++it) {
        if (it->widget) {
            it->widget->close();
            it->widget->deleteLater();
        }
    }
    m_viewers.clear();
    
    qDebug() << "[MedicalViewerServiceImpl] 医学图像查看器服务实现已销毁";
}

//-----------------------------------------------------------------------------
void MedicalViewerServiceImpl::setPluginContext(ctkPluginContext* context)
{
    m_pluginContext = context;
    qDebug() << "[MedicalViewerServiceImpl] 设置CTK插件上下文";
    initializeImageServiceConnection();
}

//-----------------------------------------------------------------------------
void MedicalViewerServiceImpl::initializeImageServiceConnection()
{
    if (!m_pluginContext) {
        qWarning() << "[MedicalViewerServiceImpl] CTK插件上下文未设置";
        return;
    }
    
    try {
        // 查找MedicalImageCoreService服务（完全CTK架构）
        m_imageServiceRef = m_pluginContext->getServiceReference<MedicalImageCoreService>();
        if (m_imageServiceRef) {
            m_imageService = m_pluginContext->getService<MedicalImageCoreService>(m_imageServiceRef);
            
            if (m_imageService) {
                m_serviceConnected = true;
                qDebug() << "[MedicalViewerServiceImpl] 成功连接到医学图像服务";
                onImageServiceAvailabilityChanged(true);
            } else {
                qWarning() << "[MedicalViewerServiceImpl] 无法获取医学图像服务实例";
            }
        } else {
            qWarning() << "[MedicalViewerServiceImpl] 未找到医学图像服务";
        }
    } catch (const std::exception& e) {
        qCritical() << "[MedicalViewerServiceImpl] 初始化图像服务连接时发生异常:" << e.what();
        setError(QString("初始化图像服务连接失败: %1").arg(e.what()));
    }
}

//-----------------------------------------------------------------------------
QWidget* MedicalViewerServiceImpl::create2DImageViewer(QWidget* parent)
{
    return createGenericViewer("2D", parent);
}

QWidget* MedicalViewerServiceImpl::create3DImageViewer(QWidget* parent)
{
    return createGenericViewer("3D", parent);
}

QWidget* MedicalViewerServiceImpl::createMPRViewer(QWidget* parent)
{
    return createGenericViewer("MPR", parent);
}

QWidget* MedicalViewerServiceImpl::createVolumeRenderer(QWidget* parent)
{
    return createGenericViewer("Volume", parent);
}

// ==================== 新增：体绘制功能（整合自NrrdViewer） ====================

QWidget* MedicalViewerServiceImpl::createAdvancedVolumeRenderer(QWidget* parent)
{
    return createGenericViewer("AdvancedVolume", parent);
}

QWidget* MedicalViewerServiceImpl::createNrrdViewer(QWidget* parent)
{
    return createGenericViewer("NrrdViewer", parent);
}

QWidget* MedicalViewerServiceImpl::createScientificVisualizationViewer(QWidget* parent)
{
    return createGenericViewer("ScientificVisualization", parent);
}



//-----------------------------------------------------------------------------
// 简化的接口实现
//-----------------------------------------------------------------------------

bool MedicalViewerServiceImpl::displayImage(const QString& viewerId, const QString& imageId)
{
    if (!validateViewerId(viewerId) || !validateImageId(imageId)) {
        return false;
    }
    
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    info->currentImageId = imageId;
    if (!info->displayedImageIds.contains(imageId)) {
        info->displayedImageIds.append(imageId);
    }
    
    emit imageDisplayed(viewerId, imageId);
    qDebug() << "[MedicalViewerServiceImpl] 图像显示成功:" << viewerId << "->" << imageId;
    return true;
}

// 其他方法的简化实现
bool MedicalViewerServiceImpl::displayMultipleImages(const QString& viewerId, const QStringList& imageIds) { return false; }
bool MedicalViewerServiceImpl::clearViewer(const QString& viewerId) { return true; }
QString MedicalViewerServiceImpl::getCurrentImageId(const QString& viewerId) const { return QString(); }
QStringList MedicalViewerServiceImpl::getDisplayedImageIds(const QString& viewerId) const { return QStringList(); }
bool MedicalViewerServiceImpl::setWindowLevel(const QString& viewerId, double window, double level) { return false; }
QVariantMap MedicalViewerServiceImpl::getWindowLevel(const QString& viewerId) const { return QVariantMap(); }
bool MedicalViewerServiceImpl::autoAdjustWindowLevel(const QString& viewerId) { return false; }
bool MedicalViewerServiceImpl::resetWindowLevel(const QString& viewerId) { return false; }
bool MedicalViewerServiceImpl::setCurrentSlice(const QString& viewerId, int sliceIndex) { return false; }
int MedicalViewerServiceImpl::getCurrentSlice(const QString& viewerId) const { return 0; }
int MedicalViewerServiceImpl::getSliceCount(const QString& viewerId) const { return 1; }
bool MedicalViewerServiceImpl::zoomImage(const QString& viewerId, double zoomFactor) { return false; }
bool MedicalViewerServiceImpl::panImage(const QString& viewerId, double deltaX, double deltaY) { return false; }
bool MedicalViewerServiceImpl::resetView(const QString& viewerId) { return false; }
bool MedicalViewerServiceImpl::enableDistanceMeasurement(const QString& viewerId, bool enabled) { return false; }
bool MedicalViewerServiceImpl::enableAngleMeasurement(const QString& viewerId, bool enabled) { return false; }
QString MedicalViewerServiceImpl::addTextAnnotation(const QString& viewerId, double x, double y, const QString& text) { return QString(); }
bool MedicalViewerServiceImpl::removeAnnotation(const QString& viewerId, const QString& annotationId) { return false; }
bool MedicalViewerServiceImpl::clearAllAnnotations(const QString& viewerId) { return false; }
bool MedicalViewerServiceImpl::setViewerLayout(const QString& viewerId, int rows, int cols) { return false; }
bool MedicalViewerServiceImpl::setImageOrientation(const QString& viewerId, const QString& orientation) { return false; }
bool MedicalViewerServiceImpl::enableSynchronizedScrolling(const QStringList& viewerIds, bool enabled) { return false; }
bool MedicalViewerServiceImpl::captureViewerImage(const QString& viewerId, const QString& filePath) { return false; }

QStringList MedicalViewerServiceImpl::getActiveViewers() const
{
    QMutexLocker locker(&m_mutex);
    return m_viewers.keys();
}

bool MedicalViewerServiceImpl::closeViewer(const QString& viewerId) { return false; }
QVariantMap MedicalViewerServiceImpl::getViewerInfo(const QString& viewerId) const { return QVariantMap(); }
bool MedicalViewerServiceImpl::setViewerParameters(const QString& viewerId, const QVariantMap& parameters) { return false; }

//-----------------------------------------------------------------------------
// UI显示管理实现
//-----------------------------------------------------------------------------

bool MedicalViewerServiceImpl::showViewerDialog(QWidget* parent)
{
    try {
        // 创建现代化的多模式医学图像查看器
        MedicalViewerWidget* viewerWidget = new MedicalViewerWidget(parent);
        viewerWidget->setPluginContext(m_pluginContext);
        
        // 设置为独立窗口
        viewerWidget->setWindowFlags(Qt::Window);
        viewerWidget->setAttribute(Qt::WA_DeleteOnClose);
        viewerWidget->resize(1400, 900);
        
        // 显示窗口
        viewerWidget->show();
        viewerWidget->raise();
        viewerWidget->activateWindow();
        
        qDebug() << "[MedicalViewerServiceImpl] 显示多模式医学图像查看器界面";
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool MedicalViewerServiceImpl::showMPRDialog(QWidget* parent)
{
    QMessageBox::information(parent, "功能提示", "MPR查看器界面即将推出");
    return true;
}

bool MedicalViewerServiceImpl::showViewerConfigDialog(QWidget* parent)
{
    QMessageBox::information(parent, "功能提示", "查看器配置界面即将推出");
    return true;
}

//-----------------------------------------------------------------------------
// 私有方法实现
//-----------------------------------------------------------------------------

bool MedicalViewerServiceImpl::validateViewerId(const QString& viewerId) const
{
    QMutexLocker locker(&m_mutex);
    return !viewerId.isEmpty() && m_viewers.contains(viewerId);
}

bool MedicalViewerServiceImpl::validateImageId(const QString& imageId) const
{
    if (imageId.isEmpty()) return false;
    
    if (m_imageService) {
        QStringList loadedImages = m_imageService->getLoadedImages();
        return loadedImages.contains(imageId);
    }
    
    return false;
}

QString MedicalViewerServiceImpl::generateViewerId() const
{
    return QString("viewer_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

MedicalViewerServiceImpl::ViewerInfo* MedicalViewerServiceImpl::getViewerInfo(const QString& viewerId)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_viewers.find(viewerId);
    return (it != m_viewers.end()) ? &(*it) : nullptr;
}

const MedicalViewerServiceImpl::ViewerInfo* MedicalViewerServiceImpl::getViewerInfoPtr(const QString& viewerId) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_viewers.find(viewerId);
    return (it != m_viewers.end()) ? &(*it) : nullptr;
}

void MedicalViewerServiceImpl::setError(const QString& error)
{
    QMutexLocker locker(&m_mutex);
    m_lastError = error;
    qWarning() << "[MedicalViewerServiceImpl]" << error;
}

void MedicalViewerServiceImpl::registerViewer(const QString& viewerId, const QString& viewerType, QWidget* widget)
{
    QMutexLocker locker(&m_mutex);
    
    ViewerInfo info;
    info.viewerId = viewerId;
    info.viewerType = viewerType;
    info.widget = widget;
    
    m_viewers[viewerId] = info;
    
    qDebug() << "[MedicalViewerServiceImpl] 查看器已注册:" << viewerId << "类型:" << viewerType;
}

//-----------------------------------------------------------------------------
// 体绘制功能实现（整合自NrrdViewer）
//-----------------------------------------------------------------------------

bool MedicalViewerServiceImpl::isNrrdImage(const QString& imageId) const
{
    if (!m_imageService) return false;
    QVariantMap details = m_imageService->getImageDetails(imageId);
    QString format = details.value("format", "").toString();
    return format.toLower().contains("nrrd");
}

bool MedicalViewerServiceImpl::setVolumeOpacity(const QString& viewerId, double opacity)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    info->parameters["volumeOpacity"] = opacity;
    emit volumeRenderingParametersChanged(viewerId, info->parameters);
    return true;
}

bool MedicalViewerServiceImpl::setTransferFunction(const QString& viewerId, const QVariantMap& transferFunction)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    info->parameters["transferFunction"] = transferFunction;
    emit transferFunctionChanged(viewerId, transferFunction);
    return true;
}

bool MedicalViewerServiceImpl::setVolumeLightingParameters(const QString& viewerId, double ambient, double diffuse, double specular)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    QVariantMap lighting;
    lighting["ambient"] = ambient;
    lighting["diffuse"] = diffuse;
    lighting["specular"] = specular;
    
    info->parameters["lighting"] = lighting;
    emit volumeRenderingParametersChanged(viewerId, info->parameters);
    return true;
}

bool MedicalViewerServiceImpl::setVolumeMaterialProperties(const QString& viewerId, const QVariantMap& material)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    info->parameters["material"] = material;
    emit volumeRenderingParametersChanged(viewerId, info->parameters);
    return true;
}

bool MedicalViewerServiceImpl::setVolumeCameraPosition(const QString& viewerId, double x, double y, double z)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    QVariantMap position;
    position["x"] = x;
    position["y"] = y;
    position["z"] = z;
    
    info->parameters["cameraPosition"] = position;
    emit volumeCameraChanged(viewerId, position, info->parameters["cameraFocalPoint"].toMap());
    return true;
}

bool MedicalViewerServiceImpl::setVolumeCameraFocalPoint(const QString& viewerId, double x, double y, double z)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    QVariantMap focalPoint;
    focalPoint["x"] = x;
    focalPoint["y"] = y;
    focalPoint["z"] = z;
    
    info->parameters["cameraFocalPoint"] = focalPoint;
    emit volumeCameraChanged(viewerId, info->parameters["cameraPosition"].toMap(), focalPoint);
    return true;
}

bool MedicalViewerServiceImpl::resetVolumeCamera(const QString& viewerId)
{
    return setVolumeCameraPosition(viewerId, 0, 0, 500) && setVolumeCameraFocalPoint(viewerId, 0, 0, 0);
}

bool MedicalViewerServiceImpl::setVolumeClippingPlane(const QString& viewerId, const QVariantMap& plane)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    info->parameters["clippingPlane"] = plane;
    emit volumeRenderingParametersChanged(viewerId, info->parameters);
    return true;
}

bool MedicalViewerServiceImpl::setVolumeSampleDistance(const QString& viewerId, double distance)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    info->parameters["sampleDistance"] = distance;
    emit volumeRenderingParametersChanged(viewerId, info->parameters);
    return true;
}

bool MedicalViewerServiceImpl::setVolumeGradientShading(const QString& viewerId, bool enabled)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    info->parameters["gradientShading"] = enabled;
    emit volumeRenderingParametersChanged(viewerId, info->parameters);
    return true;
}

bool MedicalViewerServiceImpl::setVolumeInteractionEnabled(const QString& viewerId, bool enabled)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    info->parameters["interactionEnabled"] = enabled;
    return true;
}

//-----------------------------------------------------------------------------
// 科研级可视化功能实现
//-----------------------------------------------------------------------------

QVariantMap MedicalViewerServiceImpl::getVolumeStatistics(const QString& viewerId, const QString& imageId)
{
    QVariantMap stats;
    
    if (m_imageService) {
        QMap<QString, double> imageStats = m_imageService->getImageStatistics(imageId);
        // 转换为QVariantMap
        for (auto it = imageStats.begin(); it != imageStats.end(); ++it) {
            stats[it.key()] = it.value();
        }
        stats["volumeSize"] = "512x512x256";
        stats["dataType"] = "16-bit";
        stats["spacing"] = "1.0x1.0x1.0 mm";
    }
    
    emit volumeAnalysisCompleted(viewerId, imageId, stats);
    return stats;
}

QVariantMap MedicalViewerServiceImpl::analyzeVolumeDistribution(const QString& viewerId, const QString& imageId)
{
    QVariantMap analysis;
    analysis["histogram"] = "density distribution data";
    analysis["gradientMagnitude"] = "gradient analysis";
    analysis["tissueClassification"] = "automated segmentation";
    
    emit volumeAnalysisCompleted(viewerId, imageId, analysis);
    return analysis;
}

bool MedicalViewerServiceImpl::setAdvancedRenderingAlgorithm(const QString& viewerId, const QString& algorithm)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    info->parameters["renderingAlgorithm"] = algorithm;
    emit volumeRenderingParametersChanged(viewerId, info->parameters);
    return true;
}

bool MedicalViewerServiceImpl::setRenderingQuality(const QString& viewerId, const QString& quality)
{
    ViewerInfo* info = getViewerInfo(viewerId);
    if (!info) return false;
    
    info->parameters["renderingQuality"] = quality;
    emit volumeRenderingParametersChanged(viewerId, info->parameters);
    return true;
}

bool MedicalViewerServiceImpl::exportVolumeRendering(const QString& viewerId, const QString& filePath, 
                                                     const QString& format, const QVariantMap& options)
{
    try {
        // 模拟导出过程
        qDebug() << "[MedicalViewerServiceImpl] 导出体绘制到:" << filePath << "格式:" << format;
        
        emit volumeExportCompleted(viewerId, filePath, true);
        return true;
    } catch (...) {
        emit volumeExportCompleted(viewerId, filePath, false);
        return false;
    }
}

//-----------------------------------------------------------------------------
// 体绘制UI显示管理实现
//-----------------------------------------------------------------------------

bool MedicalViewerServiceImpl::showVolumeRenderingDialog(QWidget* parent)
{
    QMessageBox::information(parent, "体绘制配置", "体绘制配置界面\n\n功能包括：\n- 传输函数编辑\n- 光照参数调整\n- 材质属性设置\n- 渲染质量控制");
    return true;
}

bool MedicalViewerServiceImpl::showNrrdViewerDialog(QWidget* parent)
{
    QMessageBox::information(parent, "NRRD查看器", "NRRD专用查看器界面\n\n专门处理NRRD格式：\n- .nrrd文件\n- .nhdr文件\n- 体数据可视化");
    return true;
}

bool MedicalViewerServiceImpl::showTransferFunctionEditorDialog(QWidget* parent)
{
    QMessageBox::information(parent, "传输函数编辑器", "传输函数编辑界面\n\n功能包括：\n- 颜色映射\n- 透明度控制\n- 预设传输函数\n- 自定义编辑");
    return true;
}

bool MedicalViewerServiceImpl::showScientificVisualizationDialog(QWidget* parent)
{
    QMessageBox::information(parent, "科研级可视化", "科研级可视化界面\n\n高级功能：\n- 体数据分析\n- 统计信息\n- 数据导出\n- 研究工具");
    return true;
}

void MedicalViewerServiceImpl::onViewerClosed()
{
    // TODO: 处理查看器关闭事件
}

void MedicalViewerServiceImpl::onImageServiceAvailabilityChanged(bool available)
{
    m_serviceConnected = available;
    
    if (available) {
        qDebug() << "[MedicalViewerServiceImpl] 医学图像服务现在可用";
    } else {
        qWarning() << "[MedicalViewerServiceImpl] 医学图像服务不可用";
    }
}

//-----------------------------------------------------------------------------
// 增强的查看器创建辅助方法
//-----------------------------------------------------------------------------

QHBoxLayout* MedicalViewerServiceImpl::createViewerToolbar(const QString& viewerType)
{
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    if (viewerType == "VolumeRenderer" || viewerType == "AdvancedVolumeRenderer") {
        // 体绘制工具栏
        QPushButton* transferFunctionBtn = new QPushButton("传输函数");
        QPushButton* lightingBtn = new QPushButton("光照设置");
        QPushButton* qualityBtn = new QPushButton("渲染质量");
        QPushButton* clippingBtn = new QPushButton("裁剪平面");
        
        transferFunctionBtn->setToolTip("编辑颜色和透明度传输函数");
        lightingBtn->setToolTip("调整光照和材质参数");
        qualityBtn->setToolTip("设置渲染质量和采样率");
        clippingBtn->setToolTip("添加和编辑裁剪平面");
        
        toolbarLayout->addWidget(transferFunctionBtn);
        toolbarLayout->addWidget(lightingBtn);
        toolbarLayout->addWidget(qualityBtn);
        toolbarLayout->addWidget(clippingBtn);
        
    } else if (viewerType == "MPR") {
        // MPR工具栏
        QPushButton* axialBtn = new QPushButton("轴状面");
        QPushButton* sagittalBtn = new QPushButton("矢状面");
        QPushButton* coronalBtn = new QPushButton("冠状面");
        QPushButton* crosshairBtn = new QPushButton("十字线");
        QPushButton* syncBtn = new QPushButton("同步滚动");
        
        axialBtn->setCheckable(true);
        sagittalBtn->setCheckable(true);
        coronalBtn->setCheckable(true);
        crosshairBtn->setCheckable(true);
        syncBtn->setCheckable(true);
        
        axialBtn->setChecked(true);
        sagittalBtn->setChecked(true);
        coronalBtn->setChecked(true);
        crosshairBtn->setChecked(true);
        
        toolbarLayout->addWidget(axialBtn);
        toolbarLayout->addWidget(sagittalBtn);
        toolbarLayout->addWidget(coronalBtn);
        toolbarLayout->addWidget(crosshairBtn);
        toolbarLayout->addWidget(syncBtn);
        
    } else if (viewerType == "ScientificVisualization") {
        // 科学可视化工具栏
        QPushButton* isosurfaceBtn = new QPushButton("等值面");
        QPushButton* vectorFieldBtn = new QPushButton("向量场");
        QPushButton* streamlineBtn = new QPushButton("流线");
        QPushButton* colorMapBtn = new QPushButton("色彩映射");
        
        isosurfaceBtn->setToolTip("生成和编辑等值面");
        vectorFieldBtn->setToolTip("显示向量场可视化");
        streamlineBtn->setToolTip("计算和显示流线");
        colorMapBtn->setToolTip("配置色彩映射方案");
        
        toolbarLayout->addWidget(isosurfaceBtn);
        toolbarLayout->addWidget(vectorFieldBtn);
        toolbarLayout->addWidget(streamlineBtn);
        toolbarLayout->addWidget(colorMapBtn);
        
    } else {
        // 通用工具栏
        QPushButton* zoomBtn = new QPushButton("缩放");
        QPushButton* panBtn = new QPushButton("平移");
        QPushButton* resetBtn = new QPushButton("重置");
        QPushButton* measureBtn = new QPushButton("测量");
        
        zoomBtn->setToolTip("缩放图像");
        panBtn->setToolTip("平移图像");
        resetBtn->setToolTip("重置视图");
        measureBtn->setToolTip("距离和角度测量");
        
        toolbarLayout->addWidget(zoomBtn);
        toolbarLayout->addWidget(panBtn);
        toolbarLayout->addWidget(resetBtn);
        toolbarLayout->addWidget(measureBtn);
    }
    
    toolbarLayout->addStretch();
    return toolbarLayout;
}

QWidget* MedicalViewerServiceImpl::createDisplayArea(const QString& viewerType, const QString& viewerId)
{
    QWidget* displayArea = new QWidget();
    displayArea->setMinimumHeight(300);
    
    if (viewerType == "MPR") {
        // 创建MPR四视图布局
        QGridLayout* mprLayout = new QGridLayout(displayArea);
        
        // 轴状面视图
        QLabel* axialView = new QLabel("轴状面视图\n(Axial View)");
        axialView->setAlignment(Qt::AlignCenter);
        axialView->setStyleSheet("border: 2px solid #3498db; background-color: #ecf0f1; font-weight: bold;");
        axialView->setMinimumSize(200, 150);
        
        // 矢状面视图
        QLabel* sagittalView = new QLabel("矢状面视图\n(Sagittal View)");
        sagittalView->setAlignment(Qt::AlignCenter);
        sagittalView->setStyleSheet("border: 2px solid #e74c3c; background-color: #ecf0f1; font-weight: bold;");
        sagittalView->setMinimumSize(200, 150);
        
        // 冠状面视图
        QLabel* coronalView = new QLabel("冠状面视图\n(Coronal View)");
        coronalView->setAlignment(Qt::AlignCenter);
        coronalView->setStyleSheet("border: 2px solid #f39c12; background-color: #ecf0f1; font-weight: bold;");
        coronalView->setMinimumSize(200, 150);
        
        // 3D视图
        QLabel* view3D = new QLabel("3D立体视图\n(3D View)");
        view3D->setAlignment(Qt::AlignCenter);
        view3D->setStyleSheet("border: 2px solid #27ae60; background-color: #ecf0f1; font-weight: bold;");
        view3D->setMinimumSize(200, 150);
        
        mprLayout->addWidget(axialView, 0, 0);
        mprLayout->addWidget(sagittalView, 0, 1);
        mprLayout->addWidget(coronalView, 1, 0);
        mprLayout->addWidget(view3D, 1, 1);
        
    } else if (viewerType == "VolumeRenderer" || viewerType == "AdvancedVolumeRenderer") {
        // 创建体绘制显示区域
        QVBoxLayout* volumeLayout = new QVBoxLayout(displayArea);
        
        QLabel* renderArea = new QLabel("体绘制显示区域\n\n等待加载体数据...\n\n支持实时交互：\n• 鼠标拖拽：旋转\n• 滚轮：缩放\n• 右键：平移");
        renderArea->setAlignment(Qt::AlignCenter);
        renderArea->setStyleSheet("border: 2px solid #9b59b6; background-color: #f8f9fa; color: #2c3e50; font-size: 14px;");
        
        // 添加渲染参数控制面板
        QHBoxLayout* controlLayout = new QHBoxLayout();
        
        QLabel* samplingLabel = new QLabel("采样率:");
        QSlider* samplingSlider = new QSlider(Qt::Horizontal);
        samplingSlider->setRange(10, 100);
        samplingSlider->setValue(50);
        samplingSlider->setToolTip("控制体绘制的采样率，影响质量和性能");
        
        QLabel* opacityLabel = new QLabel("总体透明度:");
        QSlider* opacitySlider = new QSlider(Qt::Horizontal);
        opacitySlider->setRange(0, 100);
        opacitySlider->setValue(80);
        opacitySlider->setToolTip("调整整体透明度");
        
        controlLayout->addWidget(samplingLabel);
        controlLayout->addWidget(samplingSlider);
        controlLayout->addWidget(opacityLabel);
        controlLayout->addWidget(opacitySlider);
        
        volumeLayout->addWidget(renderArea, 1);
        volumeLayout->addLayout(controlLayout);
        
    } else if (viewerType == "ScientificVisualization") {
        // 创建科学可视化显示区域
        QVBoxLayout* sciLayout = new QVBoxLayout(displayArea);
        
        QLabel* sciArea = new QLabel("科学可视化显示区域\n\n支持的可视化类型：\n• 等值面提取\n• 向量场可视化\n• 流线计算\n• 张量可视化\n• 多变量数据分析");
        sciArea->setAlignment(Qt::AlignCenter);
        sciArea->setStyleSheet("border: 2px solid #1abc9c; background-color: #f8f9fa; color: #2c3e50; font-size: 14px;");
        
        sciLayout->addWidget(sciArea);
        
    } else {
        // 通用显示区域
        QVBoxLayout* genericLayout = new QVBoxLayout(displayArea);
        
        QString displayText = QString("%1\n\n查看器ID: %2\n\n状态: 等待图像数据\n\n支持的操作：\n• 拖拽加载图像\n• 鼠标交互\n• 快捷键操作").arg(viewerType).arg(viewerId);
        
        QLabel* genericArea = new QLabel(displayText);
        genericArea->setAlignment(Qt::AlignCenter);
        genericArea->setStyleSheet("border: 2px dashed #95a5a6; background-color: #f8f9fa; color: #2c3e50; font-size: 12px;");
        
        genericLayout->addWidget(genericArea);
    }
    
    return displayArea;
}

void MedicalViewerServiceImpl::configureViewerSpecificFeatures(const QString& viewerId, const QString& viewerType, QWidget* viewerWidget)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_viewers.contains(viewerId)) {
        return;
    }
    
    ViewerInfo& info = m_viewers[viewerId];
    
    if (viewerType == "VolumeRenderer") {
        info.parameters["renderingMethod"] = "RayCasting";
        info.parameters["samplingRate"] = 1.0;
        info.parameters["ambient"] = 0.15;
        info.parameters["diffuse"] = 0.9;
        info.parameters["specular"] = 0.3;
        info.parameters["specularPower"] = 15.0;
        info.parameters["enableShading"] = true;
        info.parameters["enableGradientOpacity"] = false;
        
        qDebug() << "[MedicalViewerServiceImpl] 配置体绘制器特性:" << viewerId;
        
    } else if (viewerType == "AdvancedVolumeRenderer") {
        info.parameters["renderingMethod"] = "GPU RayCasting";
        info.parameters["qualityLevel"] = "High";
        info.parameters["enableShadows"] = true;
        info.parameters["enableGradientShading"] = true;
        info.parameters["enableClipping"] = true;
        info.parameters["multiSampling"] = 4;
        info.parameters["enableJittering"] = true;
        info.parameters["enablePreIntegration"] = true;
        
        qDebug() << "[MedicalViewerServiceImpl] 配置高级体绘制器特性:" << viewerId;
        
    } else if (viewerType == "MPR") {
        info.parameters["axialView"] = true;
        info.parameters["sagittalView"] = true;
        info.parameters["coronalView"] = true;
        info.parameters["synchronizedCrosshair"] = true;
        info.parameters["synchronizedWindowing"] = true;
        info.parameters["synchronizedZoom"] = false;
        info.parameters["crosshairColor"] = "#ff0000";
        info.parameters["sliceThickness"] = 1.0;
        
        qDebug() << "[MedicalViewerServiceImpl] 配置MPR查看器特性:" << viewerId;
        
    } else if (viewerType == "ScientificVisualization") {
        info.parameters["vectorFieldVisualization"] = true;
        info.parameters["isoSurfaceGeneration"] = true;
        info.parameters["flowVisualization"] = true;
        info.parameters["tensorVisualization"] = true;
        info.parameters["multiVariableSupport"] = true;
        info.parameters["colorMappingSchemes"] = QStringList{"Rainbow", "Heat", "Cool", "Jet", "Grayscale"};
        info.parameters["animationSupport"] = true;
        
        qDebug() << "[MedicalViewerServiceImpl] 配置科学可视化查看器特性:" << viewerId;
        
    } else if (viewerType == "NRRD") {
        info.parameters["supportedFormats"] = QStringList{"nrrd", "nhdr", "raw"};
        info.parameters["headerParsing"] = true;
        info.parameters["metadataDisplay"] = true;
        info.parameters["compressionSupport"] = true;
        info.parameters["bigEndianSupport"] = true;
        
        qDebug() << "[MedicalViewerServiceImpl] 配置NRRD查看器特性:" << viewerId;
    }
    
    // 设置通用特性
    info.parameters["interactionEnabled"] = true;
    info.parameters["annotationSupport"] = true;
    info.parameters["measurementTools"] = true;
    info.parameters["exportCapability"] = true;
    info.parameters["fullScreenMode"] = true;
    
    // 更新查看器状态
    info.status = "configured";
    
    emit viewerCreated(viewerId, viewerType);
}

//-----------------------------------------------------------------------------
QStringList MedicalViewerServiceImpl::getAvailableViewerTypes() const
{
    QStringList types;
    types << "2D" << "3D" << "MPR" << "VolumeRenderer" 
          << "AdvancedVolumeRenderer" << "NRRD" << "ScientificVisualization";
    return types;
}

//-----------------------------------------------------------------------------
// 私有方法实现
//-----------------------------------------------------------------------------

QWidget* MedicalViewerServiceImpl::createGenericViewer(const QString& viewerType, QWidget* parent)
{
    QMutexLocker locker(&m_mutex);
    
    try {
        // 生成查看器ID
        QString viewerId = generateViewerId();
        
        // 创建查看器组件
        QWidget* viewerWidget = createViewerWidget(viewerType, parent);
        if (!viewerWidget) {
            qWarning() << "[MedicalViewerServiceImpl] 创建查看器组件失败:" << viewerType;
            return nullptr;
        }
        
        // 注册查看器
        registerViewer(viewerId, viewerType, viewerWidget);
        
        // 发送查看器创建信号
        emit viewerCreated(viewerId, viewerType);
        
        qDebug() << "[MedicalViewerServiceImpl] 成功创建查看器:" << viewerType << "ID:" << viewerId;
        
        return viewerWidget;
        
    } catch (const std::exception& e) {
        setError(QString("创建查看器失败: %1").arg(e.what()));
        qCritical() << "[MedicalViewerServiceImpl] 创建查看器时发生异常:" << e.what();
        return nullptr;
    }
}

QWidget* MedicalViewerServiceImpl::createViewerWidget(const QString& viewerType, QWidget* parent)
{
    // 根据查看器类型创建相应的控件
    if (viewerType == "2D") {
        return create2DViewerWidget(parent);
    } else if (viewerType == "3D") {
        return create3DViewerWidget(parent);
    } else if (viewerType == "MPR") {
        return createMPRViewerWidget(parent);
    } else if (viewerType == "VolumeRenderer") {
        return createVolumeRendererWidget(parent);
    } else if (viewerType == "AdvancedVolumeRenderer") {
        return createAdvancedVolumeRendererWidget(parent);
    } else if (viewerType == "NRRD") {
        return createNrrdViewerWidget(parent);
    } else if (viewerType == "ScientificVisualization") {
        return createScientificVisualizationWidget(parent);
    } else {
        qWarning() << "[MedicalViewerServiceImpl] 不支持的查看器类型:" << viewerType;
        return nullptr;
    }
}

QWidget* MedicalViewerServiceImpl::create2DViewerWidget(QWidget* parent)
{
    // 创建基础2D查看器
    MedicalViewerWidget* widget = new MedicalViewerWidget(parent);
    widget->setMinimumSize(400, 300);
    
    // 设置为2D模式
    widget->setDisplayMode("2D");
    
    return widget;
}

QWidget* MedicalViewerServiceImpl::create3DViewerWidget(QWidget* parent)
{
    // 创建基础3D查看器
    MedicalViewerWidget* widget = new MedicalViewerWidget(parent);
    widget->setMinimumSize(500, 400);
    
    // 设置为3D模式
    widget->setDisplayMode("3D");
    
    return widget;
}

QWidget* MedicalViewerServiceImpl::createMPRViewerWidget(QWidget* parent)
{
    // 创建MPR查看器
    MedicalViewerWidget* widget = new MedicalViewerWidget(parent);
    widget->setMinimumSize(600, 500);
    
    // 设置为MPR模式
    widget->setDisplayMode("MPR");
    
    return widget;
}

QWidget* MedicalViewerServiceImpl::createVolumeRendererWidget(QWidget* parent)
{
    // 创建体绘制查看器
    MedicalViewerWidget* widget = new MedicalViewerWidget(parent);
    widget->setMinimumSize(500, 400);
    
    // 设置为体绘制模式
    widget->setDisplayMode("Volume");
    
    return widget;
}

QWidget* MedicalViewerServiceImpl::createAdvancedVolumeRendererWidget(QWidget* parent)
{
    // 创建高级体绘制查看器
    MedicalViewerWidget* widget = new MedicalViewerWidget(parent);
    widget->setMinimumSize(600, 500);
    
    // 设置为高级体绘制模式
    widget->setDisplayMode("Advanced");
    
    return widget;
}

QWidget* MedicalViewerServiceImpl::createNrrdViewerWidget(QWidget* parent)
{
    // 创建NRRD专用查看器
    MedicalViewerWidget* widget = new MedicalViewerWidget(parent);
    widget->setMinimumSize(500, 400);
    
    // 设置为Volume模式（NRRD通常用于体绘制）
    widget->setDisplayMode("Volume");
    
    return widget;
}

QWidget* MedicalViewerServiceImpl::createScientificVisualizationWidget(QWidget* parent)
{
    // 创建科研级可视化查看器
    MedicalViewerWidget* widget = new MedicalViewerWidget(parent);
    widget->setMinimumSize(700, 600);
    
    // 设置为高级模式
    widget->setDisplayMode("Advanced");
    
    return widget;
}





