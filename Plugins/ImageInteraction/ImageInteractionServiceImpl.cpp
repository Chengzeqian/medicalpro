#include "ImageInteractionServiceImpl.h"
#include "../MedicalImageCore/MedicalImageCoreService.h"

// CTK框架
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

#include <QDebug>
#include <QMutexLocker>
#include <QUuid>
#include <QDateTime>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QFormLayout>
#include <cmath>

//-----------------------------------------------------------------------------
ImageInteractionServiceImpl::ImageInteractionServiceImpl(ctkPluginContext* context, QObject* parent)
    : ImageInteractionService(parent)
    , m_pluginContext(context)
    , m_imageService(nullptr)
    , m_serviceConnected(false)
    , m_componentsInitialized(false)
{
    qDebug() << "[ImageInteractionServiceImpl] 创建图像交互服务实现（完全CTK架构）";
    
    // 初始化默认组件参数
    initializeDefaultParameters();
    
    m_componentsInitialized = true;
    qDebug() << "[ImageInteractionServiceImpl] 图像交互服务实现创建完成";
}

//-----------------------------------------------------------------------------
ImageInteractionServiceImpl::~ImageInteractionServiceImpl()
{
    QMutexLocker locker(&m_mutex);
    
    // 关闭所有交互组件
    for (auto it = m_components.begin(); it != m_components.end(); ++it) {
        if (it->widget) {
            it->widget->close();
            it->widget->deleteLater();
        }
    }
    m_components.clear();
    
    qDebug() << "[ImageInteractionServiceImpl] 图像交互服务实现已销毁";
}

//-----------------------------------------------------------------------------
void ImageInteractionServiceImpl::setPluginContext(ctkPluginContext* context)
{
    m_pluginContext = context;
    qDebug() << "[ImageInteractionServiceImpl] 设置CTK插件上下文";
    
    // 初始化图像服务连接
    initializeImageServiceConnection();
}

//-----------------------------------------------------------------------------
void ImageInteractionServiceImpl::initializeImageServiceConnection()
{
    if (!m_pluginContext) {
        qWarning() << "[ImageInteractionServiceImpl] CTK插件上下文未设置";
        return;
    }
    
    try {
        // 查找MedicalImageCoreService服务（完全CTK架构）
        m_imageServiceRef = m_pluginContext->getServiceReference<MedicalImageCoreService>();
        if (m_imageServiceRef) {
            m_imageService = m_pluginContext->getService<MedicalImageCoreService>(m_imageServiceRef);
            
            if (m_imageService) {
                m_serviceConnected = true;
                qDebug() << "[ImageInteractionServiceImpl] 成功连接到医学图像服务";
                onImageServiceAvailabilityChanged(true);
            } else {
                qWarning() << "[ImageInteractionServiceImpl] 无法获取医学图像服务实例";
            }
        } else {
            qWarning() << "[ImageInteractionServiceImpl] 未找到医学图像服务";
        }
    } catch (const std::exception& e) {
        qCritical() << "[ImageInteractionServiceImpl] 初始化图像服务连接时发生异常:" << e.what();
        setError(QString("初始化图像服务连接失败: %1").arg(e.what()));
    }
}

//-----------------------------------------------------------------------------
void ImageInteractionServiceImpl::initializeDefaultParameters()
{
    // 点选器默认参数
    m_defaultComponentParameters["PointPicker"] = QVariantMap{
        {"maxPoints", 10},
        {"pointSize", 5.0},
        {"pointColor", "255,0,0"},
        {"enabled", true}
    };
    
    // 交互面板默认参数
    m_defaultComponentParameters["InteractionPanel"] = QVariantMap{
        {"showCoordinates", true},
        {"showMeasurements", true},
        {"showAnnotations", true}
    };
    
    // 测量工具栏默认参数
    m_defaultComponentParameters["MeasurementToolbar"] = QVariantMap{
        {"distanceEnabled", true},
        {"angleEnabled", true},
        {"volumeEnabled", false},
        {"precision", 2}
    };
    
    // 标注编辑器默认参数
    m_defaultComponentParameters["AnnotationEditor"] = QVariantMap{
        {"fontSize", 12},
        {"textColor", "255,255,255"},
        {"arrowColor", "255,255,0"},
        {"showLabels", true}
    };
    
    qDebug() << "[ImageInteractionServiceImpl] 默认组件参数已初始化";
}

//-----------------------------------------------------------------------------
// 交互组件创建实现
//-----------------------------------------------------------------------------

QWidget* ImageInteractionServiceImpl::createPointPicker(QWidget* parent)
{
    return createGenericComponent("PointPicker", parent);
}

QWidget* ImageInteractionServiceImpl::createInteractionPanel(QWidget* parent)
{
    return createGenericComponent("InteractionPanel", parent);
}

QWidget* ImageInteractionServiceImpl::createMeasurementToolbar(QWidget* parent)
{
    return createGenericComponent("MeasurementToolbar", parent);
}

QWidget* ImageInteractionServiceImpl::createAnnotationEditor(QWidget* parent)
{
    return createGenericComponent("AnnotationEditor", parent);
}

QWidget* ImageInteractionServiceImpl::createGenericComponent(const QString& componentType, QWidget* parent)
{
    try {
        QString componentId = generateComponentId();
        
        // 创建组件控件
        QWidget* componentWidget = new QWidget(parent);
        componentWidget->setMinimumSize(300, 200);
        
        QVBoxLayout* layout = new QVBoxLayout(componentWidget);
        
        // 添加标题
        QLabel* titleLabel = new QLabel(QString("%1 [%2]").arg(componentType).arg(componentId));
        titleLabel->setStyleSheet("font-weight: bold; padding: 5px; background-color: #e0e0e0;");
        layout->addWidget(titleLabel);
        
        // 根据组件类型添加特定内容
        QWidget* contentWidget = createComponentContent(componentType, componentId);
        layout->addWidget(contentWidget, 1);
        
        // 添加控制按钮
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        QPushButton* bindBtn = new QPushButton("绑定图像");
        QPushButton* clearBtn = new QPushButton("清除数据");
        
        buttonLayout->addWidget(bindBtn);
        buttonLayout->addWidget(clearBtn);
        buttonLayout->addStretch();
        
        layout->addLayout(buttonLayout);
        
        // 注册组件
        registerComponent(componentId, componentType, componentWidget);
        
        // 发出创建信号
        emit componentCreated(componentId, componentType);
        
        qDebug() << "[ImageInteractionServiceImpl]" << componentType << "组件创建成功:" << componentId;
        return componentWidget;
        
    } catch (const std::exception& e) {
        QString error = QString("创建%1组件失败: %2").arg(componentType).arg(e.what());
        setError(error);
        emit interactionError("", error);
        return nullptr;
    }
}

//-----------------------------------------------------------------------------
// 简化的接口实现
//-----------------------------------------------------------------------------

bool ImageInteractionServiceImpl::bindImageToComponent(const QString& componentId, const QString& imageId)
{
    if (!validateComponentId(componentId) || !validateImageId(imageId)) {
        return false;
    }
    
    ComponentInfo* info = getComponentInfoPtr(componentId);
    if (!info) return false;
    
    info->boundImageId = imageId;
    
    // 更新UI显示
    if (info->widget) {
        QLabel* statusLabel = info->widget->findChild<QLabel*>();
        if (statusLabel && statusLabel->text().contains("等待图像绑定")) {
            statusLabel->setText(QString("已绑定图像: %1").arg(imageId));
        }
    }
    
    qDebug() << "[ImageInteractionServiceImpl] 图像绑定成功:" << componentId << "->" << imageId;
    return true;
}

// 其他方法的简化实现
bool ImageInteractionServiceImpl::unbindImageFromComponent(const QString& componentId) { return false; }
QString ImageInteractionServiceImpl::getBoundImageId(const QString& componentId) const { return QString(); }
bool ImageInteractionServiceImpl::setInteractionEnabled(const QString& componentId, bool enabled) { return false; }
bool ImageInteractionServiceImpl::enablePointPicking(const QString& componentId, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return false;
    }
    
    auto& component = m_components[componentId];
    component.parameters["pointPickingEnabled"] = enabled;
    
    qDebug() << "[ImageInteractionServiceImpl] 点选模式已" 
             << (enabled ? "启用" : "禁用") << "for component:" << componentId;
    
    return true;
}

bool ImageInteractionServiceImpl::setPointPickingMode(const QString& componentId, int maxPoints)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId) || maxPoints < 0) {
        return false;
    }
    
    auto& component = m_components[componentId];
    component.parameters["maxPoints"] = maxPoints;
    
    qDebug() << "[ImageInteractionServiceImpl] 设置最大点数:" << maxPoints << "for component:" << componentId;
    
    return true;
}

QString ImageInteractionServiceImpl::addMarkerPoint(const QString& componentId, double x, double y, double z, const QString& label)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return QString();
    }
    
    auto& component = m_components[componentId];
    
    // 检查是否超过最大点数限制
    int maxPoints = component.parameters.value("maxPoints", 10).toInt();
    if (component.markerPoints.size() >= maxPoints) {
        qWarning() << "[ImageInteractionServiceImpl] 已达到最大点数限制:" << maxPoints;
        return QString();
    }
    
    // 生成新的点ID
    QString pointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    // 创建点信息
    QVariantMap pointInfo;
    pointInfo["id"] = pointId;
    pointInfo["x"] = x;
    pointInfo["y"] = y;
    pointInfo["z"] = z;
    pointInfo["label"] = label.isEmpty() ? QString("Point_%1").arg(component.markerPoints.size() + 1) : label;
    pointInfo["timestamp"] = QDateTime::currentDateTime();
    
    // 添加到组件的点列表
    component.markerPoints[pointId] = pointInfo;
    
    // 发送信号
    emit markerPointAdded(componentId, pointId, pointInfo);
    
    qDebug() << "[ImageInteractionServiceImpl] 添加标记点:" << pointId 
             << "位置: (" << x << "," << y << "," << z << ")"
             << "标签:" << pointInfo["label"].toString();
    
    return pointId;
}

bool ImageInteractionServiceImpl::removeMarkerPoint(const QString& componentId, const QString& pointId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId) || pointId.isEmpty()) {
        return false;
    }
    
    auto& component = m_components[componentId];
    
    if (!component.markerPoints.contains(pointId)) {
        qWarning() << "[ImageInteractionServiceImpl] 点不存在:" << pointId;
        return false;
    }
    
    component.markerPoints.remove(pointId);
    
    qDebug() << "[ImageInteractionServiceImpl] 移除标记点:" << pointId;
    
    return true;
}

bool ImageInteractionServiceImpl::clearAllMarkerPoints(const QString& componentId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return false;
    }
    
    auto& component = m_components[componentId];
    int pointCount = component.markerPoints.size();
    component.markerPoints.clear();
    
    qDebug() << "[ImageInteractionServiceImpl] 清除所有标记点，共" << pointCount << "个点";
    
    return true;
}

QList<QVariantMap> ImageInteractionServiceImpl::getMarkerPoints(const QString& componentId) const
{
    QMutexLocker locker(&m_mutex);
    
    QList<QVariantMap> result;
    
    if (!validateComponentId(componentId)) {
        return result;
    }
    
    const auto& component = m_components[componentId];
    
    for (auto it = component.markerPoints.begin(); it != component.markerPoints.end(); ++it) {
        result.append(it.value());
    }
    
    return result;
}
bool ImageInteractionServiceImpl::enableDistanceMeasurement(const QString& componentId, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return false;
    }
    
    auto& component = m_components[componentId];
    component.parameters["distanceMeasurementEnabled"] = enabled;
    
    qDebug() << "[ImageInteractionServiceImpl] 距离测量已" 
             << (enabled ? "启用" : "禁用") << "for component:" << componentId;
    
    return true;
}

bool ImageInteractionServiceImpl::enableAngleMeasurement(const QString& componentId, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return false;
    }
    
    auto& component = m_components[componentId];
    component.parameters["angleMeasurementEnabled"] = enabled;
    
    qDebug() << "[ImageInteractionServiceImpl] 角度测量已" 
             << (enabled ? "启用" : "禁用") << "for component:" << componentId;
    
    return true;
}

bool ImageInteractionServiceImpl::enableVolumeMeasurement(const QString& componentId, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return false;
    }
    
    auto& component = m_components[componentId];
    component.parameters["volumeMeasurementEnabled"] = enabled;
    
    qDebug() << "[ImageInteractionServiceImpl] 体积测量已" 
             << (enabled ? "启用" : "禁用") << "for component:" << componentId;
    
    return true;
}

double ImageInteractionServiceImpl::measureDistance(const QString& componentId, const QList<double>& point1, const QList<double>& point2)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId) || point1.size() < 3 || point2.size() < 3) {
        return -1.0;
    }
    
    // 计算3D欧几里得距离
    double dx = point2[0] - point1[0];
    double dy = point2[1] - point1[1];
    double dz = point2[2] - point1[2];
    
    double distance = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    // 创建测量结果记录
    QString measurementId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap measurementInfo;
    measurementInfo["id"] = measurementId;
    measurementInfo["type"] = "distance";
    measurementInfo["point1"] = QVariantList{point1[0], point1[1], point1[2]};
    measurementInfo["point2"] = QVariantList{point2[0], point2[1], point2[2]};
    measurementInfo["value"] = distance;
    measurementInfo["unit"] = "mm";
    measurementInfo["timestamp"] = QDateTime::currentDateTime();
    
    // 添加到组件的测量列表
    auto& component = m_components[componentId];
    component.measurements.append(measurementInfo);
    
    // 发送测量完成信号
    emit measurementCompleted(componentId, "distance", distance, "mm", measurementInfo);
    
    qDebug() << "[ImageInteractionServiceImpl] 距离测量完成:" << distance << "mm"
             << "从 (" << point1[0] << "," << point1[1] << "," << point1[2] << ")"
             << "到 (" << point2[0] << "," << point2[1] << "," << point2[2] << ")";
    
    return distance;
    
    ComponentInfo* info = getComponentInfoPtr(componentId);
    if (!info || info->boundImageId.isEmpty()) return -1.0;
    
    // 获取图像间距
    QList<double> spacing = m_imageService ? 
        m_imageService->getImageSpacing(info->boundImageId) : 
        QList<double>{1.0, 1.0, 1.0};
    
    return calculateDistance(point1, point2, spacing);
}

double ImageInteractionServiceImpl::measureAngle(const QString& componentId, const QList<double>& vertex, const QList<double>& point1, const QList<double>& point2)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId) || vertex.size() < 3 || point1.size() < 3 || point2.size() < 3) {
        return -1.0;
    }
    
    // 计算向量
    double v1x = point1[0] - vertex[0];
    double v1y = point1[1] - vertex[1]; 
    double v1z = point1[2] - vertex[2];
    
    double v2x = point2[0] - vertex[0];
    double v2y = point2[1] - vertex[1];
    double v2z = point2[2] - vertex[2];
    
    // 计算向量长度
    double len1 = std::sqrt(v1x*v1x + v1y*v1y + v1z*v1z);
    double len2 = std::sqrt(v2x*v2x + v2y*v2y + v2z*v2z);
    
    if (len1 == 0.0 || len2 == 0.0) {
        return -1.0;
    }
    
    // 计算点积
    double dotProduct = v1x*v2x + v1y*v2y + v1z*v2z;
    
    // 计算角度（弧度）
    double cosAngle = dotProduct / (len1 * len2);
    
    // 防止浮点误差导致的定义域错误
    cosAngle = std::max(-1.0, std::min(1.0, cosAngle));
    
    double angleRadians = std::acos(cosAngle);
    double angleDegrees = angleRadians * 180.0 / M_PI;
    
    // 创建测量结果记录
    QString measurementId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap measurementInfo;
    measurementInfo["id"] = measurementId;
    measurementInfo["type"] = "angle";
    measurementInfo["vertex"] = QVariantList{vertex[0], vertex[1], vertex[2]};
    measurementInfo["point1"] = QVariantList{point1[0], point1[1], point1[2]};
    measurementInfo["point2"] = QVariantList{point2[0], point2[1], point2[2]};
    measurementInfo["value"] = angleDegrees;
    measurementInfo["unit"] = "degrees";
    measurementInfo["timestamp"] = QDateTime::currentDateTime();
    
    // 添加到组件的测量列表
    auto& component = m_components[componentId];
    component.measurements.append(measurementInfo);
    
    // 发送测量完成信号
    emit measurementCompleted(componentId, "angle", angleDegrees, "degrees", measurementInfo);
    
    qDebug() << "[ImageInteractionServiceImpl] 角度测量完成:" << angleDegrees << "度"
             << "顶点 (" << vertex[0] << "," << vertex[1] << "," << vertex[2] << ")"
             << "边1 (" << point1[0] << "," << point1[1] << "," << point1[2] << ")"
             << "边2 (" << point2[0] << "," << point2[1] << "," << point2[2] << ")";
    
    return angleDegrees;
}

QList<QVariantMap> ImageInteractionServiceImpl::getMeasurements(const QString& componentId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return QList<QVariantMap>();
    }
    
    const auto& component = m_components[componentId];
    return component.measurements;
}

bool ImageInteractionServiceImpl::clearMeasurements(const QString& componentId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return false;
    }
    
    auto& component = m_components[componentId];
    int measurementCount = component.measurements.size();
    component.measurements.clear();
    
    qDebug() << "[ImageInteractionServiceImpl] 清除所有测量结果，共" << measurementCount << "个测量";
    
    return true;
}
QString ImageInteractionServiceImpl::addTextAnnotation(const QString& componentId, double x, double y, double z, const QString& text, int fontSize, const QString& color)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId) || text.isEmpty()) {
        return QString();
    }
    
    // 生成新的标注ID
    QString annotationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    // 创建标注信息
    QVariantMap annotationInfo;
    annotationInfo["id"] = annotationId;
    annotationInfo["type"] = "text";
    annotationInfo["x"] = x;
    annotationInfo["y"] = y;
    annotationInfo["z"] = z;
    annotationInfo["text"] = text;
    annotationInfo["fontSize"] = fontSize;
    annotationInfo["color"] = color;
    annotationInfo["timestamp"] = QDateTime::currentDateTime();
    
    // 添加到组件的标注列表
    auto& component = m_components[componentId];
    component.annotations.append(annotationInfo);
    
    // 发送标注添加信号
    emit annotationAdded(componentId, annotationId, "text", annotationInfo);
    
    qDebug() << "[ImageInteractionServiceImpl] 添加文本标注:" << annotationId 
             << "位置: (" << x << "," << y << "," << z << ")"
             << "内容:" << text;
    
    return annotationId;
}

QString ImageInteractionServiceImpl::addArrowAnnotation(const QString& componentId, double startX, double startY, double startZ, double endX, double endY, double endZ, const QString& color)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return QString();
    }
    
    // 生成新的标注ID
    QString annotationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    // 创建标注信息
    QVariantMap annotationInfo;
    annotationInfo["id"] = annotationId;
    annotationInfo["type"] = "arrow";
    annotationInfo["startX"] = startX;
    annotationInfo["startY"] = startY;
    annotationInfo["startZ"] = startZ;
    annotationInfo["endX"] = endX;
    annotationInfo["endY"] = endY;
    annotationInfo["endZ"] = endZ;
    annotationInfo["color"] = color;
    annotationInfo["timestamp"] = QDateTime::currentDateTime();
    
    // 添加到组件的标注列表
    auto& component = m_components[componentId];
    component.annotations.append(annotationInfo);
    
    // 发送标注添加信号
    emit annotationAdded(componentId, annotationId, "arrow", annotationInfo);
    
    qDebug() << "[ImageInteractionServiceImpl] 添加箭头标注:" << annotationId 
             << "从 (" << startX << "," << startY << "," << startZ << ")"
             << "到 (" << endX << "," << endY << "," << endZ << ")";
    
    return annotationId;
}

bool ImageInteractionServiceImpl::modifyAnnotation(const QString& componentId, const QString& annotationId, const QVariantMap& properties)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId) || annotationId.isEmpty()) {
        return false;
    }
    
    auto& component = m_components[componentId];
    
    // 查找标注
    for (auto& annotation : component.annotations) {
        if (annotation.value("id").toString() == annotationId) {
            // 更新属性
            for (auto it = properties.begin(); it != properties.end(); ++it) {
                annotation[it.key()] = it.value();
            }
            
            // 发送标注修改信号
            emit annotationModified(componentId, annotationId, annotation);
            
            qDebug() << "[ImageInteractionServiceImpl] 修改标注:" << annotationId;
            return true;
        }
    }
    
    qWarning() << "[ImageInteractionServiceImpl] 标注不存在:" << annotationId;
    return false;
}

bool ImageInteractionServiceImpl::removeAnnotation(const QString& componentId, const QString& annotationId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId) || annotationId.isEmpty()) {
        return false;
    }
    
    auto& component = m_components[componentId];
    
    // 查找并移除标注
    for (int i = 0; i < component.annotations.size(); ++i) {
        if (component.annotations[i].value("id").toString() == annotationId) {
            component.annotations.removeAt(i);
            
            qDebug() << "[ImageInteractionServiceImpl] 移除标注:" << annotationId;
            return true;
        }
    }
    
    qWarning() << "[ImageInteractionServiceImpl] 标注不存在:" << annotationId;
    return false;
}

bool ImageInteractionServiceImpl::clearAllAnnotations(const QString& componentId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return false;
    }
    
    auto& component = m_components[componentId];
    int annotationCount = component.annotations.size();
    component.annotations.clear();
    
    qDebug() << "[ImageInteractionServiceImpl] 清除所有标注，共" << annotationCount << "个标注";
    
    return true;
}
QList<QVariantMap> ImageInteractionServiceImpl::getAnnotations(const QString& componentId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateComponentId(componentId)) {
        return QList<QVariantMap>();
    }
    
    const auto& component = m_components[componentId];
    return component.annotations;
}
QList<double> ImageInteractionServiceImpl::screenToWorldCoordinates(const QString& componentId, double screenX, double screenY) { return QList<double>(); }
QList<double> ImageInteractionServiceImpl::worldToScreenCoordinates(const QString& componentId, double worldX, double worldY, double worldZ) { return QList<double>(); }
QList<double> ImageInteractionServiceImpl::imageToWorldCoordinates(const QString& componentId, int imageX, int imageY, int imageZ) { return QList<double>(); }

QStringList ImageInteractionServiceImpl::getActiveComponents() const
{
    QMutexLocker locker(&m_mutex);
    return m_components.keys();
}

bool ImageInteractionServiceImpl::closeComponent(const QString& componentId) { return false; }
QVariantMap ImageInteractionServiceImpl::getComponentInfo(const QString& componentId) const { return QVariantMap(); }
bool ImageInteractionServiceImpl::setComponentParameters(const QString& componentId, const QVariantMap& parameters) { return false; }

//-----------------------------------------------------------------------------
// UI显示管理实现
//-----------------------------------------------------------------------------

bool ImageInteractionServiceImpl::showInteractionDialog(QWidget* parent)
{
    try {
        QDialog* dialog = new QDialog(parent);
        dialog->setWindowTitle("图像交互工具");
        dialog->setModal(true);
        dialog->resize(600, 400);
        
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        
        QTextEdit* statusText = new QTextEdit();
        QString statusInfo = QString("当前活动交互组件: %1 个\n服务连接状态: %2")
                            .arg(getActiveComponents().size())
                            .arg(m_serviceConnected ? "已连接" : "未连接");
        statusText->setPlainText(statusInfo);
        statusText->setReadOnly(true);
        
        layout->addWidget(statusText);
        
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
        layout->addWidget(buttonBox);
        
        connect(buttonBox, &QDialogButtonBox::clicked, dialog, &QDialog::accept);
        
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool ImageInteractionServiceImpl::showPointPickerDialog(QWidget* parent)
{
    QMessageBox::information(parent, "功能提示", "点选器配置界面即将推出");
    return true;
}

bool ImageInteractionServiceImpl::showMeasurementDialog(QWidget* parent)
{
    QMessageBox::information(parent, "功能提示", "测量工具配置界面即将推出");
    return true;
}

bool ImageInteractionServiceImpl::showAnnotationDialog(QWidget* parent)
{
    QMessageBox::information(parent, "功能提示", "标注编辑器配置界面即将推出");
    return true;
}

//-----------------------------------------------------------------------------
// 私有方法实现
//-----------------------------------------------------------------------------

bool ImageInteractionServiceImpl::validateComponentId(const QString& componentId) const
{
    QMutexLocker locker(&m_mutex);
    return !componentId.isEmpty() && m_components.contains(componentId);
}

bool ImageInteractionServiceImpl::validateImageId(const QString& imageId) const
{
    if (imageId.isEmpty()) return false;
    
    if (m_imageService) {
        QStringList loadedImages = m_imageService->getLoadedImages();
        return loadedImages.contains(imageId);
    }
    
    return false;
}

QString ImageInteractionServiceImpl::generateComponentId() const
{
    return QString("component_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString ImageInteractionServiceImpl::generatePointId() const
{
    return QString("point_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString ImageInteractionServiceImpl::generateMeasurementId() const
{
    return QString("measure_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString ImageInteractionServiceImpl::generateAnnotationId() const
{
    return QString("annotation_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

ImageInteractionServiceImpl::ComponentInfo* ImageInteractionServiceImpl::getComponentInfoPtr(const QString& componentId)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_components.find(componentId);
    return (it != m_components.end()) ? &(*it) : nullptr;
}

const ImageInteractionServiceImpl::ComponentInfo* ImageInteractionServiceImpl::getComponentInfoPtr(const QString& componentId) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_components.find(componentId);
    return (it != m_components.end()) ? &(*it) : nullptr;
}

void ImageInteractionServiceImpl::setError(const QString& error)
{
    QMutexLocker locker(&m_mutex);
    m_lastError = error;
    qWarning() << "[ImageInteractionServiceImpl]" << error;
}

QVariantMap ImageInteractionServiceImpl::getImageInfoFromService(const QString& imageId) const
{
    if (!m_imageService) return QVariantMap();
    
    QVariantMap info;
    info["dimensions"] = QVariant::fromValue(m_imageService->getImageDimensions(imageId));
    info["spacing"] = QVariant::fromValue(m_imageService->getImageSpacing(imageId));
    info["origin"] = QVariant::fromValue(m_imageService->getImageOrigin(imageId));
    info["dataType"] = m_imageService->getImageDataType(imageId);
    info["isValid"] = m_imageService->isValid(imageId);
    info["is3D"] = m_imageService->is3D(imageId);
    
    return info;
}

void ImageInteractionServiceImpl::registerComponent(const QString& componentId, const QString& componentType, QWidget* widget)
{
    QMutexLocker locker(&m_mutex);
    
    ComponentInfo info;
    info.componentId = componentId;
    info.componentType = componentType;
    info.widget = widget;
    info.parameters = m_defaultComponentParameters.value(componentType, QVariantMap());
    
    m_components[componentId] = info;
    
    // 更新计数器
    m_componentCounters[componentType]++;
    
    qDebug() << "[ImageInteractionServiceImpl] 交互组件已注册:" << componentId << "类型:" << componentType;
}

double ImageInteractionServiceImpl::calculateDistance(const QList<double>& point1, const QList<double>& point2, const QList<double>& spacing) const
{
    if (point1.size() < 3 || point2.size() < 3 || spacing.size() < 3) {
        return -1.0;
    }
    
    double dx = (point1[0] - point2[0]) * spacing[0];
    double dy = (point1[1] - point2[1]) * spacing[1];
    double dz = (point1[2] - point2[2]) * spacing[2];
    
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

double ImageInteractionServiceImpl::calculateAngle(const QList<double>& vertex, const QList<double>& point1, const QList<double>& point2) const
{
    if (vertex.size() < 3 || point1.size() < 3 || point2.size() < 3) {
        return -1.0;
    }
    
    // 向量 vertex -> point1
    double v1x = point1[0] - vertex[0];
    double v1y = point1[1] - vertex[1];
    double v1z = point1[2] - vertex[2];
    
    // 向量 vertex -> point2
    double v2x = point2[0] - vertex[0];
    double v2y = point2[1] - vertex[1];
    double v2z = point2[2] - vertex[2];
    
    // 计算点积
    double dotProduct = v1x*v2x + v1y*v2y + v1z*v2z;
    
    // 计算向量长度
    double len1 = std::sqrt(v1x*v1x + v1y*v1y + v1z*v1z);
    double len2 = std::sqrt(v2x*v2x + v2y*v2y + v2z*v2z);
    
    if (len1 == 0 || len2 == 0) return -1.0;
    
    // 计算角度（弧度转度）
    double cosAngle = dotProduct / (len1 * len2);
    cosAngle = std::max(-1.0, std::min(1.0, cosAngle)); // 限制范围
    
    return std::acos(cosAngle) * 180.0 / M_PI;
}

void ImageInteractionServiceImpl::onComponentClosed()
{
    // TODO: 处理组件关闭事件
}

void ImageInteractionServiceImpl::onImageServiceAvailabilityChanged(bool available)
{
    m_serviceConnected = available;
    
    if (available) {
        qDebug() << "[ImageInteractionServiceImpl] 医学图像服务现在可用";
    } else {
        qWarning() << "[ImageInteractionServiceImpl] 医学图像服务不可用";
    }
}

//-----------------------------------------------------------------------------
// 增强的组件内容创建方法
//-----------------------------------------------------------------------------

QWidget* ImageInteractionServiceImpl::createComponentContent(const QString& componentType, const QString& componentId)
{
    QWidget* contentWidget = new QWidget();
    
    if (componentType == "PointPicker") {
        return createPointPickerContent(componentId);
    } else if (componentType == "InteractionPanel") {
        return createInteractionPanelContent(componentId);
    } else if (componentType == "MeasurementToolbar") {
        return createMeasurementToolbarContent(componentId);
    } else if (componentType == "AnnotationEditor") {
        return createAnnotationEditorContent(componentId);
    } else {
        // 通用内容
        QVBoxLayout* layout = new QVBoxLayout(contentWidget);
        QLabel* statusLabel = new QLabel("等待图像绑定...");
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setMinimumHeight(80);
        statusLabel->setStyleSheet("border: 1px solid #ccc; background-color: #fafafa;");
        layout->addWidget(statusLabel);
        
        return contentWidget;
    }
}

QWidget* ImageInteractionServiceImpl::createPointPickerContent(const QString& componentId)
{
    QWidget* content = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(content);
    
    // 点选控制面板
    QGroupBox* controlGroup = new QGroupBox("点选控制");
    QFormLayout* controlLayout = new QFormLayout(controlGroup);
    
    // 最大点数控制
    QSpinBox* maxPointsSpin = new QSpinBox();
    maxPointsSpin->setRange(1, 100);
    maxPointsSpin->setValue(10);
    maxPointsSpin->setToolTip("设置可选择的最大点数");
    controlLayout->addRow("最大点数:", maxPointsSpin);
    
    // 点大小控制
    QDoubleSpinBox* pointSizeSpin = new QDoubleSpinBox();
    pointSizeSpin->setRange(1.0, 20.0);
    pointSizeSpin->setValue(5.0);
    pointSizeSpin->setSuffix(" px");
    pointSizeSpin->setToolTip("设置显示点的大小");
    controlLayout->addRow("点大小:", pointSizeSpin);
    
    // 颜色选择
    QPushButton* colorBtn = new QPushButton();
    colorBtn->setStyleSheet("background-color: #ff0000; min-height: 25px;");
    colorBtn->setToolTip("选择点的颜色");
    controlLayout->addRow("颜色:", colorBtn);
    
    // 启用开关
    QCheckBox* enabledCheck = new QCheckBox("启用3D点选");
    enabledCheck->setChecked(true);
    enabledCheck->setToolTip("启用或禁用3D点选功能");
    controlLayout->addRow("", enabledCheck);
    
    mainLayout->addWidget(controlGroup);
    
    // 选择的点列表
    QGroupBox* pointsGroup = new QGroupBox("已选择的点");
    QVBoxLayout* pointsLayout = new QVBoxLayout(pointsGroup);
    
    QListWidget* pointsList = new QListWidget();
    pointsList->setMaximumHeight(120);
    pointsList->setContextMenuPolicy(Qt::CustomContextMenu);
    pointsList->setToolTip("右键点击可删除选择的点");
    pointsLayout->addWidget(pointsList);
    
    // 点操作按钮
    QHBoxLayout* pointsButtonLayout = new QHBoxLayout();
    QPushButton* clearPointsBtn = new QPushButton("清除所有点");
    QPushButton* exportPointsBtn = new QPushButton("导出坐标");
    QPushButton* importPointsBtn = new QPushButton("导入坐标");
    
    clearPointsBtn->setToolTip("清除所有已选择的点");
    exportPointsBtn->setToolTip("将点坐标导出到文件");
    importPointsBtn->setToolTip("从文件导入点坐标");
    
    pointsButtonLayout->addWidget(clearPointsBtn);
    pointsButtonLayout->addWidget(exportPointsBtn);
    pointsButtonLayout->addWidget(importPointsBtn);
    
    pointsLayout->addLayout(pointsButtonLayout);
    mainLayout->addWidget(pointsGroup);
    
    // 坐标显示
    QGroupBox* coordGroup = new QGroupBox("当前坐标");
    QFormLayout* coordLayout = new QFormLayout(coordGroup);
    
    QLabel* worldCoordLabel = new QLabel("(0.00, 0.00, 0.00)");
    QLabel* imageCoordLabel = new QLabel("(0, 0, 0)");
    QLabel* pixelValueLabel = new QLabel("0");
    
    worldCoordLabel->setStyleSheet("font-family: monospace; color: #2c3e50;");
    imageCoordLabel->setStyleSheet("font-family: monospace; color: #2c3e50;");
    pixelValueLabel->setStyleSheet("font-family: monospace; color: #2c3e50;");
    
    coordLayout->addRow("世界坐标:", worldCoordLabel);
    coordLayout->addRow("图像坐标:", imageCoordLabel);
    coordLayout->addRow("像素值:", pixelValueLabel);
    
    mainLayout->addWidget(coordGroup);
    
    return content;
}

QWidget* ImageInteractionServiceImpl::createInteractionPanelContent(const QString& componentId)
{
    QWidget* content = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(content);
    
    // 交互模式选择
    QGroupBox* modeGroup = new QGroupBox("交互模式");
    QVBoxLayout* modeLayout = new QVBoxLayout(modeGroup);
    
    QRadioButton* noneRadio = new QRadioButton("无交互");
    QRadioButton* pickRadio = new QRadioButton("点选模式");
    QRadioButton* measureRadio = new QRadioButton("测量模式");
    QRadioButton* annotateRadio = new QRadioButton("标注模式");
    
    pickRadio->setChecked(true);
    
    modeLayout->addWidget(noneRadio);
    modeLayout->addWidget(pickRadio);
    modeLayout->addWidget(measureRadio);
    modeLayout->addWidget(annotateRadio);
    
    mainLayout->addWidget(modeGroup);
    
    // 显示设置
    QGroupBox* displayGroup = new QGroupBox("显示设置");
    QFormLayout* displayLayout = new QFormLayout(displayGroup);
    
    QCheckBox* showCoordsCheck = new QCheckBox();
    QCheckBox* showMeasurementsCheck = new QCheckBox();
    QCheckBox* showAnnotationsCheck = new QCheckBox();
    QCheckBox* showCrosshairCheck = new QCheckBox();
    
    showCoordsCheck->setChecked(true);
    showMeasurementsCheck->setChecked(true);
    showAnnotationsCheck->setChecked(true);
    showCrosshairCheck->setChecked(false);
    
    displayLayout->addRow("显示坐标:", showCoordsCheck);
    displayLayout->addRow("显示测量:", showMeasurementsCheck);
    displayLayout->addRow("显示标注:", showAnnotationsCheck);
    displayLayout->addRow("显示十字线:", showCrosshairCheck);
    
    mainLayout->addWidget(displayGroup);
    
    // 快捷键说明
    QGroupBox* shortcutGroup = new QGroupBox("快捷键");
    QVBoxLayout* shortcutLayout = new QVBoxLayout(shortcutGroup);
    
    QLabel* shortcutLabel = new QLabel(
        "左键点击: 选择点\n"
        "右键点击: 取消选择\n"
        "Ctrl+左键: 添加测量点\n"
        "Shift+左键: 添加标注\n"
        "ESC: 取消当前操作\n"
        "Delete: 删除选中项"
    );
    shortcutLabel->setStyleSheet("font-size: 11px; color: #666;");
    shortcutLayout->addWidget(shortcutLabel);
    
    mainLayout->addWidget(shortcutGroup);
    mainLayout->addStretch();
    
    return content;
}

QWidget* ImageInteractionServiceImpl::createMeasurementToolbarContent(const QString& componentId)
{
    QWidget* content = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(content);
    
    // 测量工具按钮
    QGroupBox* toolsGroup = new QGroupBox("测量工具");
    QGridLayout* toolsLayout = new QGridLayout(toolsGroup);
    
    QPushButton* distanceBtn = new QPushButton("距离测量");
    QPushButton* angleBtn = new QPushButton("角度测量");
    QPushButton* areaBtn = new QPushButton("面积测量");
    QPushButton* volumeBtn = new QPushButton("体积测量");
    QPushButton* curveBtn = new QPushButton("曲线测量");
    QPushButton* clearBtn = new QPushButton("清除全部");
    
    distanceBtn->setCheckable(true);
    angleBtn->setCheckable(true);
    areaBtn->setCheckable(true);
    volumeBtn->setCheckable(true);
    curveBtn->setCheckable(true);
    
    distanceBtn->setToolTip("测量两点之间的3D距离");
    angleBtn->setToolTip("测量三点形成的角度");
    areaBtn->setToolTip("测量多边形区域面积");
    volumeBtn->setToolTip("测量3D体积");
    curveBtn->setToolTip("测量沿曲线的长度");
    clearBtn->setToolTip("清除所有测量结果");
    
    toolsLayout->addWidget(distanceBtn, 0, 0);
    toolsLayout->addWidget(angleBtn, 0, 1);
    toolsLayout->addWidget(areaBtn, 1, 0);
    toolsLayout->addWidget(volumeBtn, 1, 1);
    toolsLayout->addWidget(curveBtn, 2, 0);
    toolsLayout->addWidget(clearBtn, 2, 1);
    
    mainLayout->addWidget(toolsGroup);
    
    // 测量设置
    QGroupBox* settingsGroup = new QGroupBox("测量设置");
    QFormLayout* settingsLayout = new QFormLayout(settingsGroup);
    
    QSpinBox* precisionSpin = new QSpinBox();
    precisionSpin->setRange(1, 6);
    precisionSpin->setValue(2);
    precisionSpin->setSuffix(" 位小数");
    settingsLayout->addRow("精度:", precisionSpin);
    
    QComboBox* unitCombo = new QComboBox();
    unitCombo->addItems({"mm", "cm", "inch", "pixel"});
    unitCombo->setCurrentText("mm");
    settingsLayout->addRow("单位:", unitCombo);
    
    QCheckBox* realTimeCheck = new QCheckBox();
    realTimeCheck->setChecked(true);
    realTimeCheck->setToolTip("实时显示测量结果");
    settingsLayout->addRow("实时显示:", realTimeCheck);
    
    mainLayout->addWidget(settingsGroup);
    
    // 测量结果列表
    QGroupBox* resultsGroup = new QGroupBox("测量结果");
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsGroup);
    
    QListWidget* resultsList = new QListWidget();
    resultsList->setMaximumHeight(100);
    resultsList->setContextMenuPolicy(Qt::CustomContextMenu);
    resultsLayout->addWidget(resultsList);
    
    // 结果操作按钮
    QHBoxLayout* resultsButtonLayout = new QHBoxLayout();
    QPushButton* exportResultsBtn = new QPushButton("导出结果");
    QPushButton* copyResultsBtn = new QPushButton("复制到剪贴板");
    
    exportResultsBtn->setToolTip("将测量结果导出到文件");
    copyResultsBtn->setToolTip("复制结果到系统剪贴板");
    
    resultsButtonLayout->addWidget(exportResultsBtn);
    resultsButtonLayout->addWidget(copyResultsBtn);
    
    resultsLayout->addLayout(resultsButtonLayout);
    mainLayout->addWidget(resultsGroup);
    
    return content;
}

QWidget* ImageInteractionServiceImpl::createAnnotationEditorContent(const QString& componentId)
{
    QWidget* content = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(content);
    
    // 标注工具
    QGroupBox* toolsGroup = new QGroupBox("标注工具");
    QGridLayout* toolsLayout = new QGridLayout(toolsGroup);
    
    QPushButton* textBtn = new QPushButton("文本标注");
    QPushButton* arrowBtn = new QPushButton("箭头标注");
    QPushButton* rectBtn = new QPushButton("矩形标注");
    QPushButton* circleBtn = new QPushButton("圆形标注");
    QPushButton* freehandBtn = new QPushButton("自由画笔");
    QPushButton* clearAnnotBtn = new QPushButton("清除标注");
    
    textBtn->setCheckable(true);
    arrowBtn->setCheckable(true);
    rectBtn->setCheckable(true);
    circleBtn->setCheckable(true);
    freehandBtn->setCheckable(true);
    
    textBtn->setToolTip("添加文本标注");
    arrowBtn->setToolTip("添加箭头指向标注");
    rectBtn->setToolTip("添加矩形标注");
    circleBtn->setToolTip("添加圆形标注");
    freehandBtn->setToolTip("自由画笔标注");
    clearAnnotBtn->setToolTip("清除所有标注");
    
    toolsLayout->addWidget(textBtn, 0, 0);
    toolsLayout->addWidget(arrowBtn, 0, 1);
    toolsLayout->addWidget(rectBtn, 1, 0);
    toolsLayout->addWidget(circleBtn, 1, 1);
    toolsLayout->addWidget(freehandBtn, 2, 0);
    toolsLayout->addWidget(clearAnnotBtn, 2, 1);
    
    mainLayout->addWidget(toolsGroup);
    
    // 标注设置
    QGroupBox* settingsGroup = new QGroupBox("标注设置");
    QFormLayout* settingsLayout = new QFormLayout(settingsGroup);
    
    QSpinBox* fontSizeSpin = new QSpinBox();
    fontSizeSpin->setRange(8, 72);
    fontSizeSpin->setValue(12);
    fontSizeSpin->setSuffix(" pt");
    settingsLayout->addRow("字体大小:", fontSizeSpin);
    
    QPushButton* textColorBtn = new QPushButton();
    textColorBtn->setStyleSheet("background-color: #ffffff; min-height: 25px; border: 1px solid #ccc;");
    textColorBtn->setToolTip("选择文本颜色");
    settingsLayout->addRow("文本颜色:", textColorBtn);
    
    QPushButton* bgColorBtn = new QPushButton();
    bgColorBtn->setStyleSheet("background-color: #000000; min-height: 25px; border: 1px solid #ccc;");
    bgColorBtn->setToolTip("选择背景颜色");
    settingsLayout->addRow("背景颜色:", bgColorBtn);
    
    QSpinBox* lineWidthSpin = new QSpinBox();
    lineWidthSpin->setRange(1, 10);
    lineWidthSpin->setValue(2);
    lineWidthSpin->setSuffix(" px");
    settingsLayout->addRow("线条宽度:", lineWidthSpin);
    
    QCheckBox* showLabelsCheck = new QCheckBox();
    showLabelsCheck->setChecked(true);
    showLabelsCheck->setToolTip("显示标注标签");
    settingsLayout->addRow("显示标签:", showLabelsCheck);
    
    mainLayout->addWidget(settingsGroup);
    
    // 标注列表
    QGroupBox* annotationsGroup = new QGroupBox("标注列表");
    QVBoxLayout* annotationsLayout = new QVBoxLayout(annotationsGroup);
    
    QListWidget* annotationsList = new QListWidget();
    annotationsList->setMaximumHeight(100);
    annotationsList->setContextMenuPolicy(Qt::CustomContextMenu);
    annotationsLayout->addWidget(annotationsList);
    
    // 标注操作按钮
    QHBoxLayout* annotationsButtonLayout = new QHBoxLayout();
    QPushButton* editAnnotBtn = new QPushButton("编辑");
    QPushButton* deleteAnnotBtn = new QPushButton("删除");
    QPushButton* exportAnnotBtn = new QPushButton("导出");
    
    editAnnotBtn->setToolTip("编辑选中的标注");
    deleteAnnotBtn->setToolTip("删除选中的标注");
    exportAnnotBtn->setToolTip("导出标注到文件");
    
    annotationsButtonLayout->addWidget(editAnnotBtn);
    annotationsButtonLayout->addWidget(deleteAnnotBtn);
    annotationsButtonLayout->addWidget(exportAnnotBtn);
    
    annotationsLayout->addLayout(annotationsButtonLayout);
    mainLayout->addWidget(annotationsGroup);
    
    return content;
}

//-----------------------------------------------------------------------------
// 服务管理方法实现
//-----------------------------------------------------------------------------

void ImageInteractionServiceImpl::startService()
{
    QMutexLocker locker(&m_mutex);
    if (!m_componentsInitialized) {
        initializeDefaultParameters();
        m_componentsInitialized = true;
    }
    emit serviceStatusChanged(true);
    qDebug() << "[ImageInteractionServiceImpl] 服务已启动";
}

void ImageInteractionServiceImpl::stopService()
{
    QMutexLocker locker(&m_mutex);
    clearPoints();
    m_components.clear();
    emit serviceStatusChanged(false);
    qDebug() << "[ImageInteractionServiceImpl] 服务已停止";
}

QString ImageInteractionServiceImpl::getServiceName() const
{
    return "Image Interaction Service";
}

QString ImageInteractionServiceImpl::getServiceVersion() const
{
    return "1.0.0";
}

bool ImageInteractionServiceImpl::isActive() const
{
    return m_serviceConnected && m_componentsInitialized;
}

bool ImageInteractionServiceImpl::isPointPickingEnabled() const
{
    return !m_components.isEmpty();
}

void ImageInteractionServiceImpl::clearPoints()
{
    QMutexLocker locker(&m_mutex);
    for (auto& component : m_components) {
        component.markerPoints.clear();
    }
    emit allPointsCleared();
    qDebug() << "[ImageInteractionServiceImpl] 所有点已清除";
}

int ImageInteractionServiceImpl::getPointCount() const
{
    QMutexLocker locker(&m_mutex);
    int totalPoints = 0;
    for (const auto& component : m_components) {
        totalPoints += component.markerPoints.size();
    }
    return totalPoints;
}
