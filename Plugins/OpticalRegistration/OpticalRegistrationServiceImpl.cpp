#include "OpticalRegistrationServiceImpl.h"
#include "widgets/OpticalRegistrationWidget.h"
#include "internal/OpticalRegistrationVTKWidget.h"

// CTK框架
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

// Qt
#include <QDebug>
#include <QMutexLocker>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QProgressBar>

// VTK
#ifdef VTK_FOUND
#include <vtkLandmarkTransform.h>
#include <vtkPoints.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#endif

// 引入OpticalTrackingService接口（运行时通过CTK获取）
// 注意：这里只需要头文件，不需要链接库
#include "../OpticalTracking/OpticalTrackingService.h"

//-----------------------------------------------------------------------------
OpticalRegistrationServiceImpl::OpticalRegistrationServiceImpl(ctkPluginContext* context, QObject* parent)
    : OpticalRegistrationService(parent)
    , m_pluginContext(context)
    , m_trackingService(nullptr)
    , m_trackingServiceConnected(false)
    , m_hasValidResult(false)
    , m_activePointIndex(-1)
    , m_renderingPaused(false)
{
    logMessage("INFO", "创建光学配准服务实现");

    // 初始化变换矩阵为单位矩阵
    m_registrationMatrix.setToIdentity();

#ifdef VTK_FOUND
    m_landmarkTransform = vtkSmartPointer<vtkLandmarkTransform>::New();
    m_landmarkTransform->SetModeToRigidBody(); // 刚体变换
    logMessage("INFO", "VTK LandmarkTransform 已初始化");
#else
    logMessage("WARNING", "VTK 未找到，配准功能受限");
#endif

    // 延迟初始化服务连接
    initializeOptionalServiceConnections();

    logMessage("INFO", "光学配准服务实现创建完成");
}

//-----------------------------------------------------------------------------
OpticalRegistrationServiceImpl::~OpticalRegistrationServiceImpl()
{
    QMutexLocker locker(&m_mutex);

    // 清理Widget引用
    cleanupDestroyedWidgets();

    logMessage("INFO", "光学配准服务实现已销毁");
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::initializeOptionalServiceConnections()
{
    if (!m_pluginContext) {
        logMessage("WARNING", "插件上下文为空，无法连接跟踪服务");
        return;
    }

    // 尝试获取OpticalTrackingService
    m_trackingServiceRef = m_pluginContext->getServiceReference<OpticalTrackingService>();
    if (m_trackingServiceRef) {
        m_trackingService = m_pluginContext->getService<OpticalTrackingService>(m_trackingServiceRef);
        if (m_trackingService) {
            m_trackingServiceConnected = true;
            logMessage("INFO", "已连接到OpticalTrackingService");
        }
    } else {
        logMessage("INFO", "OpticalTrackingService暂不可用，将在运行时动态获取");
    }
}

//-----------------------------------------------------------------------------
OpticalTrackingService* OpticalRegistrationServiceImpl::getTrackingService()
{
    if (m_trackingService && m_trackingServiceConnected) {
        return m_trackingService;
    }

    // 动态获取服务
    if (m_pluginContext) {
        m_trackingServiceRef = m_pluginContext->getServiceReference<OpticalTrackingService>();
        if (m_trackingServiceRef) {
            m_trackingService = m_pluginContext->getService<OpticalTrackingService>(m_trackingServiceRef);
            if (m_trackingService) {
                m_trackingServiceConnected = true;
                logMessage("INFO", "动态获取OpticalTrackingService成功");
                return m_trackingService;
            }
        }
    }

    logMessage("WARNING", "无法获取OpticalTrackingService");
    return nullptr;
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::setTrackingContext(const QString& sessionId,
                                                         const QString& referenceToolId,
                                                         const QString& pointerToolId)
{
    QMutexLocker locker(&m_mutex);

    m_sessionId = sessionId;
    m_referenceToolId = referenceToolId;
    m_pointerToolId = pointerToolId;

    logMessage("INFO", QString("设置跟踪上下文: session=%1, ref=%2, pointer=%3")
               .arg(sessionId).arg(referenceToolId).arg(pointerToolId));
}

//-----------------------------------------------------------------------------
int OpticalRegistrationServiceImpl::addPoint(const QString& name)
{
    QMutexLocker locker(&m_mutex);

    OpticalRegistrationPoint point;
    point.name = name.isEmpty() ? QString("Point_%1").arg(m_points.size() + 1) : name;
    point.hasImagePosition = false;
    point.hasTrackerPosition = false;

    int index = m_points.size();
    m_points.append(point);

    logMessage("INFO", QString("添加配准点: %1 (索引 %2)").arg(point.name).arg(index));

    // 重要：在发射信号前释放互斥锁，避免槽函数中再次访问服务导致死锁
    locker.unlock();
    emit pointUpdated(index);
    return index;
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::setActivePointIndex(int index)
{
    QMutexLocker locker(&m_mutex);
    m_activePointIndex = index;
}

//-----------------------------------------------------------------------------
int OpticalRegistrationServiceImpl::getActivePointIndex() const
{
    QMutexLocker locker(&m_mutex);
    return m_activePointIndex;
}

//-----------------------------------------------------------------------------
bool OpticalRegistrationServiceImpl::setImagePositionForActivePoint(const QVector3D& position)
{
    QMutexLocker locker(&m_mutex);

    if (m_activePointIndex < 0 || m_activePointIndex >= m_points.size()) {
        setError("当前未选择有效的配准点");
        return false;
    }

    int index = m_activePointIndex;

    m_points[index].imagePosition = position;
    m_points[index].hasImagePosition = true;

    logMessage("INFO", QString("为活动点[%1]设置影像位置: (%2, %3, %4)")
               .arg(index)
               .arg(position.x()).arg(position.y()).arg(position.z()));

    locker.unlock();
    emit pointUpdated(index);
    return true;
}

//-----------------------------------------------------------------------------
bool OpticalRegistrationServiceImpl::setImagePosition(int index, const QVector3D& position)
{
    QMutexLocker locker(&m_mutex);

    if (index < 0 || index >= m_points.size()) {
        setError(QString("无效的点索引: %1").arg(index));
        return false;
    }

    m_points[index].imagePosition = position;
    m_points[index].hasImagePosition = true;

    logMessage("INFO", QString("设置影像位置[%1]: (%2, %3, %4)")
               .arg(index).arg(position.x()).arg(position.y()).arg(position.z()));

    locker.unlock();
    emit pointUpdated(index);
    return true;
}

//-----------------------------------------------------------------------------
bool OpticalRegistrationServiceImpl::captureTrackerPosition(int index)
{
    QMutexLocker locker(&m_mutex);

    if (index < 0 || index >= m_points.size()) {
        setError(QString("无效的点索引: %1").arg(index));
        return false;
    }

    locker.unlock();

    // 从跟踪服务获取当前指针位置
    QVector3D position = getCurrentPointerPosition();

    locker.relock();

    if (position.isNull()) {
        setError("无法获取指针工具位置");
        return false;
    }

    m_points[index].trackerPosition = position;
    m_points[index].hasTrackerPosition = true;

    logMessage("INFO", QString("采集跟踪位置[%1]: (%2, %3, %4)")
               .arg(index).arg(position.x()).arg(position.y()).arg(position.z()));

    locker.unlock();
    emit pointUpdated(index);
    return true;
}

//-----------------------------------------------------------------------------
QVector3D OpticalRegistrationServiceImpl::getCurrentPointerPosition()
{
    OpticalTrackingService* trackingService = getTrackingService();
    if (!trackingService) {
        logMessage("WARNING", "跟踪服务不可用");
        return QVector3D();
    }

    if (m_sessionId.isEmpty() || m_pointerToolId.isEmpty()) {
        logMessage("WARNING", "跟踪上下文未设置");
        return QVector3D();
    }

    // 获取工具位置 [x, y, z, rx, ry, rz]
    QList<double> position = trackingService->getToolPosition(m_sessionId, m_pointerToolId);

    if (position.size() < 3) {
        logMessage("WARNING", "获取的位置数据无效");
        return QVector3D();
    }

    // 如果设置了参考工具，转换到参考坐标系
    if (!m_referenceToolId.isEmpty()) {
        position = trackingService->transformPoint(m_sessionId, position.mid(0, 3),
                                                    m_pointerToolId, m_referenceToolId);
        if (position.size() < 3) {
            return QVector3D();
        }
    }

    return QVector3D(position[0], position[1], position[2]);
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::clearPoints()
{
    QMutexLocker locker(&m_mutex);

    m_points.clear();
    m_hasValidResult = false;
    m_registrationMatrix.setToIdentity();

    logMessage("INFO", "已清空所有配准点");

    locker.unlock();
    emit pointsCleared();
}

//-----------------------------------------------------------------------------
int OpticalRegistrationServiceImpl::pointCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_points.size();
}

//-----------------------------------------------------------------------------
OpticalRegistrationPoint OpticalRegistrationServiceImpl::getPoint(int index) const
{
    QMutexLocker locker(&m_mutex);

    if (index >= 0 && index < m_points.size()) {
        return m_points[index];
    }
    return OpticalRegistrationPoint();
}

//-----------------------------------------------------------------------------
QVector<OpticalRegistrationPoint> OpticalRegistrationServiceImpl::getAllPoints() const
{
    QMutexLocker locker(&m_mutex);
    return m_points;
}

//-----------------------------------------------------------------------------
bool OpticalRegistrationServiceImpl::canExecuteRegistration() const
{
    QMutexLocker locker(&m_mutex);

    int completePoints = 0;
    for (const auto& point : m_points) {
        if (point.hasImagePosition && point.hasTrackerPosition) {
            completePoints++;
        }
    }

    return completePoints >= 3; // 刚体配准至少需要3个点
}

//-----------------------------------------------------------------------------
OpticalRegistrationResult OpticalRegistrationServiceImpl::executeRegistration()
{
    QMutexLocker locker(&m_mutex);

    OpticalRegistrationResult result;
    result.success = false;

    emit registrationStarted();
    emit progressUpdated(0, "开始配准...");

    // 收集完整的点对
    QVector<QVector3D> imagePoints;
    QVector<QVector3D> trackerPoints;

    for (const auto& point : m_points) {
        if (point.hasImagePosition && point.hasTrackerPosition) {
            imagePoints.append(point.imagePosition);
            trackerPoints.append(point.trackerPosition);
        }
    }

    result.pointCount = imagePoints.size();

    if (result.pointCount < 3) {
        result.message = "至少需要3个完整的配准点对";
        setError(result.message);
        emit registrationFailed(result.message);
        return result;
    }

    emit progressUpdated(30, QString("使用 %1 个点进行配准...").arg(result.pointCount));

#ifdef VTK_FOUND
    // 使用VTK进行配准
    vtkNew<vtkPoints> sourcePoints; // tracker坐标
    vtkNew<vtkPoints> targetPoints; // image坐标

    for (int i = 0; i < result.pointCount; i++) {
        sourcePoints->InsertNextPoint(trackerPoints[i].x(), trackerPoints[i].y(), trackerPoints[i].z());
        targetPoints->InsertNextPoint(imagePoints[i].x(), imagePoints[i].y(), imagePoints[i].z());
    }

    m_landmarkTransform->SetSourceLandmarks(sourcePoints);
    m_landmarkTransform->SetTargetLandmarks(targetPoints);
    m_landmarkTransform->SetModeToRigidBody();
    m_landmarkTransform->Update();

    emit progressUpdated(70, "计算变换矩阵...");

    // 获取变换矩阵
    vtkMatrix4x4* vtkMatrix = m_landmarkTransform->GetMatrix();

    // 转换为QMatrix4x4
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            m_registrationMatrix(row, col) = vtkMatrix->GetElement(row, col);
        }
    }

    result.transform = m_registrationMatrix;
    result.success = true;

#else
    // 无VTK时使用简化算法（仅用于测试）
    logMessage("WARNING", "VTK不可用，使用简化配准算法");

    // 计算质心
    QVector3D imageCentroid, trackerCentroid;
    for (int i = 0; i < result.pointCount; i++) {
        imageCentroid += imagePoints[i];
        trackerCentroid += trackerPoints[i];
    }
    imageCentroid /= result.pointCount;
    trackerCentroid /= result.pointCount;

    // 简单平移（无旋转）
    QVector3D translation = imageCentroid - trackerCentroid;
    m_registrationMatrix.setToIdentity();
    m_registrationMatrix.translate(translation);

    result.transform = m_registrationMatrix;
    result.success = true;
#endif

    emit progressUpdated(90, "计算配准误差...");

    // 计算误差
    calculateRegistrationErrors(result);

    m_lastResult = result;
    m_hasValidResult = result.success;

    emit progressUpdated(100, "配准完成");

    if (result.success) {
        result.message = QString("配准成功，RMS误差: %1 mm").arg(result.rmsError, 0, 'f', 3);
        logMessage("INFO", result.message);
        locker.unlock();
        emit registrationCompleted(result);
    } else {
        locker.unlock();
        emit registrationFailed(result.message);
    }

    return result;
}


//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::calculateRegistrationErrors(OpticalRegistrationResult& result)
{
    if (result.pointCount == 0) {
        result.rmsError = 0.0;
        result.maxError = 0.0;
        return;
    }

    double sumSquaredError = 0.0;
    result.maxError = 0.0;

    for (const auto& point : m_points) {
        if (!point.hasImagePosition || !point.hasTrackerPosition) {
            continue;
        }

        // 变换tracker点到image空间
        QVector3D transformedPoint = result.transform.map(point.trackerPosition);

        // 计算与image点的距离
        double error = (transformedPoint - point.imagePosition).length();

        sumSquaredError += error * error;
        if (error > result.maxError) {
            result.maxError = error;
        }
    }

    result.rmsError = std::sqrt(sumSquaredError / result.pointCount);
}

//-----------------------------------------------------------------------------
OpticalRegistrationResult OpticalRegistrationServiceImpl::getLastResult() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastResult;
}

//-----------------------------------------------------------------------------
QMatrix4x4 OpticalRegistrationServiceImpl::getRegistrationMatrix() const
{
    QMutexLocker locker(&m_mutex);
    return m_registrationMatrix;
}

//-----------------------------------------------------------------------------
QWidget* OpticalRegistrationServiceImpl::createRegistrationWidget(QWidget* parent)
{
    logMessage("INFO", "创建配准Widget...");
    cleanupDestroyedWidgets();

    try {
        // 使用专门的OpticalRegistrationWidget
        OpticalRegistrationWidget* widget = new OpticalRegistrationWidget(this, parent);
        m_createdWidgets.append(QPointer<QWidget>(widget));

        logMessage("INFO", QString("Widget创建成功，当前跟踪 %1 个Widget").arg(m_createdWidgets.size()));
        return widget;
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("创建Widget异常: %1").arg(e.what()));
        return nullptr;
    } catch (...) {
        logMessage("ERROR", "创建Widget时发生未知异常");
        return nullptr;
    }
}

//-----------------------------------------------------------------------------
QWidget* OpticalRegistrationServiceImpl::createVTKWidget(QWidget* parent)
{
    logMessage("INFO", "创建纯VTK Widget...");

    // 清理已销毁的widget引用
    m_vtkWidgets.erase(
        std::remove_if(m_vtkWidgets.begin(), m_vtkWidgets.end(),
                       [](const QPointer<QWidget>& w) { return w.isNull(); }),
        m_vtkWidgets.end());

    try {
        OpticalRegistrationVTKWidget* widget = new OpticalRegistrationVTKWidget(this, parent);
        m_vtkWidgets.append(QPointer<QWidget>(widget));

        // 如果当前处于暂停状态，立即暂停新创建的widget
        if (m_renderingPaused) {
            widget->pauseRendering();
        }

        logMessage("INFO", QString("纯VTK Widget创建成功，当前跟踪 %1 个VTK Widget").arg(m_vtkWidgets.size()));
        return widget;
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("创建VTK Widget异常: %1").arg(e.what()));
        return nullptr;
    } catch (...) {
        logMessage("ERROR", "创建VTK Widget时发生未知异常");
        return nullptr;
    }
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::pauseRendering()
{
    m_renderingPaused = true;
    logMessage("INFO", "暂停VTK渲染");

    for (auto& widgetPtr : m_vtkWidgets) {
        if (widgetPtr) {
            OpticalRegistrationVTKWidget* vtkWidget = qobject_cast<OpticalRegistrationVTKWidget*>(widgetPtr.data());
            if (vtkWidget) {
                vtkWidget->pauseRendering();
            }
        }
    }
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::resumeRendering()
{
    m_renderingPaused = false;
    logMessage("INFO", "恢复VTK渲染");

    for (auto& widgetPtr : m_vtkWidgets) {
        if (widgetPtr) {
            OpticalRegistrationVTKWidget* vtkWidget = qobject_cast<OpticalRegistrationVTKWidget*>(widgetPtr.data());
            if (vtkWidget) {
                vtkWidget->resumeRendering();
            }
        }
    }
}

//-----------------------------------------------------------------------------
QString OpticalRegistrationServiceImpl::getLastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::startService()
{
    logMessage("INFO", "光学配准服务已启动");
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::stopService()
{
    logMessage("INFO", "光学配准服务已停止");
}

//-----------------------------------------------------------------------------
QString OpticalRegistrationServiceImpl::getServiceName() const
{
    return "OpticalRegistrationService";
}

//-----------------------------------------------------------------------------
QString OpticalRegistrationServiceImpl::getServiceVersion() const
{
    return "1.0.0";
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::onTrackingServiceAvailabilityChanged(bool available)
{
    m_trackingServiceConnected = available;
    logMessage("INFO", QString("跟踪服务可用性变化: %1").arg(available ? "可用" : "不可用"));
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::setError(const QString& error)
{
    m_lastError = error;
    logMessage("ERROR", error);
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::logMessage(const QString& level, const QString& message) const
{
    QString formattedMessage = QString("[OpticalRegistrationService] [%1] %2").arg(level).arg(message);

    if (level == "ERROR") {
        qCritical() << formattedMessage;
    } else if (level == "WARNING") {
        qWarning() << formattedMessage;
    } else {
        qDebug() << formattedMessage;
    }
}

//-----------------------------------------------------------------------------
void OpticalRegistrationServiceImpl::cleanupDestroyedWidgets()
{
    m_createdWidgets.erase(
        std::remove_if(m_createdWidgets.begin(), m_createdWidgets.end(),
                       [](const QPointer<QWidget>& w) { return w.isNull(); }),
        m_createdWidgets.end());
}

