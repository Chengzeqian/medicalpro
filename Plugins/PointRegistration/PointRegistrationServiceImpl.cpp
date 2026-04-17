#include "PointRegistrationServiceImpl.h"
#include "widgets/PointRegistrationWidget.h"
#include "internal/PointRegistrationVTKWidget.h"
#include <QDebug>
#include <QDateTime>
#include <QtMath>
#include <QElapsedTimer>
#include <QUuid>
#include <QFileInfo>

#ifdef VTK_FOUND
#include <vtkLandmarkTransform.h>
#include <vtkPoints.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkPolyData.h>
#include <vtkSTLReader.h>
#endif

PointRegistrationServiceImpl::PointRegistrationServiceImpl(QObject* parent)
    : PointRegistrationService(parent)
    , m_transformMode(TransformMode::RigidBody)
    , m_hasValidResult(false)
    , m_renderingPaused(false)
    , m_probePointSource(ProbePointSource::Manual)
    , m_probeSimulator(nullptr)
    , m_segmentationService(nullptr)
    , m_trackingService(nullptr)
{
#ifdef VTK_FOUND
    m_landmarkTransform = vtkSmartPointer<vtkLandmarkTransform>::New();
#endif
    m_transformMatrix.setToIdentity();

    // 创建探针模拟器
    m_probeSimulator = new ProbeSimulator(this);
    m_probeSimulator->setDefaultTransform();

    // 初始化会话
    m_currentSession.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_currentSession.state = RegistrationSessionState::Idle;

    logMessage("INFO", QString::fromUtf8("点配准服务实例已创建，会话ID: %1").arg(m_currentSession.sessionId));
}

PointRegistrationServiceImpl::~PointRegistrationServiceImpl()
{
    logMessage("INFO", "点配准服务实例销毁中...");
    m_createdWidgets.clear();
    m_vtkWidgets.clear();
    logMessage("INFO", "点配准服务实例已销毁");
}

// ========== 点管理实现 ==========

int PointRegistrationServiceImpl::addPoint(const QString& name)
{
    RegistrationPoint point;
    point.name = name.isEmpty() ? QString("P%1").arg(m_points.size() + 1) : name;
    m_points.append(point);
    int index = m_points.size() - 1;
    
    logMessage("INFO", QString("添加配准点: %1 (索引: %2)").arg(point.name).arg(index));
    emit pointAdded(index, point.name);
    return index;
}

bool PointRegistrationServiceImpl::removePoint(int index)
{
    if (index < 0 || index >= m_points.size()) {
        m_lastError = QString("无效的点索引: %1").arg(index);
        return false;
    }
    
    QString name = m_points[index].name;
    m_points.removeAt(index);
    m_hasValidResult = false;
    
    logMessage("INFO", QString("移除配准点: %1").arg(name));
    emit pointRemoved(index);
    return true;
}

void PointRegistrationServiceImpl::clearPoints()
{
    m_points.clear();
    m_hasValidResult = false;
    m_transformMatrix.setToIdentity();
    
    logMessage("INFO", "清空所有配准点");
    emit pointsCleared();
}

int PointRegistrationServiceImpl::pointCount() const
{
    return m_points.size();
}

RegistrationPoint PointRegistrationServiceImpl::getPoint(int index) const
{
    if (index < 0 || index >= m_points.size()) {
        return RegistrationPoint();
    }
    return m_points[index];
}

QVector<RegistrationPoint> PointRegistrationServiceImpl::getAllPoints() const
{
    return m_points;
}

bool PointRegistrationServiceImpl::setSourcePosition(int index, const QVector3D& position)
{
    if (index < 0 || index >= m_points.size()) {
        m_lastError = QString("无效的点索引: %1").arg(index);
        return false;
    }
    
    m_points[index].sourcePosition = position;
    m_points[index].hasSource = true;
    m_hasValidResult = false;
    
    logMessage("INFO", QString("设置源点 %1: (%2, %3, %4)")
        .arg(m_points[index].name)
        .arg(position.x(), 0, 'f', 2)
        .arg(position.y(), 0, 'f', 2)
        .arg(position.z(), 0, 'f', 2));
    
    emit pointUpdated(index);
    return true;
}

bool PointRegistrationServiceImpl::setTargetPosition(int index, const QVector3D& position)
{
    if (index < 0 || index >= m_points.size()) {
        m_lastError = QString("无效的点索引: %1").arg(index);
        return false;
    }
    
    m_points[index].targetPosition = position;
    m_points[index].hasTarget = true;
    m_hasValidResult = false;
    
    logMessage("INFO", QString("设置目标点 %1: (%2, %3, %4)")
        .arg(m_points[index].name)
        .arg(position.x(), 0, 'f', 2)
        .arg(position.y(), 0, 'f', 2)
        .arg(position.z(), 0, 'f', 2));
    
    emit pointUpdated(index);
    return true;
}

bool PointRegistrationServiceImpl::setPointName(int index, const QString& name)
{
    if (index < 0 || index >= m_points.size()) {
        m_lastError = QString("无效的点索引: %1").arg(index);
        return false;
    }
    
    m_points[index].name = name;
    emit pointUpdated(index);
    return true;
}

// ========== 配准执行实现 ==========

void PointRegistrationServiceImpl::setTransformMode(TransformMode mode)
{
    m_transformMode = mode;
    m_hasValidResult = false;
    logMessage("INFO", QString("设置变换模式: %1").arg(transformModeToString(mode)));
}

TransformMode PointRegistrationServiceImpl::getTransformMode() const
{
    return m_transformMode;
}

bool PointRegistrationServiceImpl::canExecuteRegistration() const
{
    int validCount = 0;
    for (const auto& point : m_points) {
        if (point.isComplete()) {
            validCount++;
        }
    }
    return validCount >= 3;
}

PointRegistrationResult PointRegistrationServiceImpl::executeRegistration()
{
    QElapsedTimer timer;
    timer.start();

    PointRegistrationResult result;
    result.timestamp = QDateTime::currentDateTime();

    emit registrationStarted();
    emit progressUpdated(0, "开始配准...");

    // 检查点数量
    int validCount = 0;
    for (const auto& point : m_points) {
        if (point.isComplete()) {
            validCount++;
        }
    }

    if (validCount < 3) {
        result.errorMessage = QString("有效点对数量不足: %1 (至少需要3个)").arg(validCount);
        m_lastError = result.errorMessage;
        logMessage("ERROR", result.errorMessage);
        emit registrationFailed(result.errorMessage);
        return result;
    }

    result.pointCount = validCount;
    emit progressUpdated(20, QString("找到 %1 个有效点对").arg(validCount));

#ifdef VTK_FOUND
    // 创建VTK点集
    vtkSmartPointer<vtkPoints> sourcePoints = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkPoints> targetPoints = vtkSmartPointer<vtkPoints>::New();

    for (const auto& point : m_points) {
        if (point.isComplete()) {
            sourcePoints->InsertNextPoint(
                point.sourcePosition.x(),
                point.sourcePosition.y(),
                point.sourcePosition.z());
            targetPoints->InsertNextPoint(
                point.targetPosition.x(),
                point.targetPosition.y(),
                point.targetPosition.z());
        }
    }

    emit progressUpdated(40, "设置变换参数...");

    // 配置变换模式
    m_landmarkTransform->SetSourceLandmarks(sourcePoints);
    m_landmarkTransform->SetTargetLandmarks(targetPoints);

    switch (m_transformMode) {
        case TransformMode::RigidBody:
            m_landmarkTransform->SetModeToRigidBody();
            break;
        case TransformMode::Similarity:
            m_landmarkTransform->SetModeToSimilarity();
            break;
        case TransformMode::Affine:
            m_landmarkTransform->SetModeToAffine();
            break;
    }

    emit progressUpdated(60, "执行配准计算...");

    // 执行配准
    m_landmarkTransform->Update();

    // 获取变换矩阵
    vtkMatrix4x4* vtkMatrix = m_landmarkTransform->GetMatrix();

    // 转换为QMatrix4x4
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m_transformMatrix(i, j) = static_cast<float>(vtkMatrix->GetElement(i, j));
        }
    }
    result.transformMatrix = m_transformMatrix;

    // 提取平移分量
    result.translationX = vtkMatrix->GetElement(0, 3);
    result.translationY = vtkMatrix->GetElement(1, 3);
    result.translationZ = vtkMatrix->GetElement(2, 3);

    // 计算欧拉角
    calculateEulerAngles(m_transformMatrix, result.rotationX, result.rotationY, result.rotationZ);

    emit progressUpdated(80, "计算配准精度...");

    // 计算配准误差
    double sumSquaredError = 0.0;
    result.maxError = 0.0;
    double sumError = 0.0;

    for (const auto& point : m_points) {
        if (point.isComplete()) {
            double error = calculatePointError(point.sourcePosition, point.targetPosition, m_transformMatrix);
            result.pointErrors.append(error);
            sumSquaredError += error * error;
            sumError += error;
            if (error > result.maxError) {
                result.maxError = error;
            }
        }
    }

    result.rmsError = qSqrt(sumSquaredError / validCount);
    result.meanError = sumError / validCount;
    result.success = true;
    m_hasValidResult = true;

    result.durationMs = timer.elapsed();

    logMessage("INFO", QString("配准成功: RMS=%.3f mm, MaxErr=%.3f mm, 耗时=%.1f ms")
        .arg(result.rmsError).arg(result.maxError).arg(result.durationMs));

#else
    result.errorMessage = "VTK未启用，无法执行配准";
    m_lastError = result.errorMessage;
    logMessage("ERROR", result.errorMessage);
    emit registrationFailed(result.errorMessage);
    return result;
#endif

    m_lastResult = result;
    emit progressUpdated(100, "配准完成");
    emit registrationCompleted(result);
    return result;
}

PointRegistrationResult PointRegistrationServiceImpl::getLastResult() const
{
    return m_lastResult;
}

QMatrix4x4 PointRegistrationServiceImpl::getTransformMatrix() const
{
    return m_transformMatrix;
}

QVector3D PointRegistrationServiceImpl::transformPoint(const QVector3D& point) const
{
    if (!m_hasValidResult) {
        return point;
    }
    return m_transformMatrix.map(point);
}

// ========== Widget工厂实现 ==========

QWidget* PointRegistrationServiceImpl::createRegistrationWidget(QWidget* parent)
{
    logMessage("INFO", "创建配准Widget...");
    cleanupDestroyedWidgets();

    try {
        PointRegistrationWidget* widget = new PointRegistrationWidget(this, parent);
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

// ========== 纯VTK Widget工厂实现 ==========

QWidget* PointRegistrationServiceImpl::createVTKWidget(QWidget* parent)
{
    logMessage("INFO", "创建纯VTK Widget...");

    // 清理已销毁的Widget
    m_vtkWidgets.removeAll(QPointer<QWidget>());

    try {
        PointRegistrationVTKWidget* widget = new PointRegistrationVTKWidget(this, parent);
        m_vtkWidgets.append(QPointer<QWidget>(widget));

#ifdef VTK_FOUND
        // If a model is already loaded before the view is created (e.g. loaded from planning tab),
        // push it into the newly created VTK widget so the user can immediately pick points.
        if (m_modelPolyData && m_modelPolyData->GetNumberOfPoints() > 0) {
            widget->loadModel(m_modelPolyData, m_modelName);
        }
#endif

        logMessage("INFO", QString("VTK Widget创建成功，当前跟踪 %1 个VTK Widget").arg(m_vtkWidgets.size()));
        return widget;
    } catch (const std::exception& e) {
        logMessage("ERROR", QString("创建VTK Widget异常: %1").arg(e.what()));
        return nullptr;
    } catch (...) {
        logMessage("ERROR", "创建VTK Widget时发生未知异常");
        return nullptr;
    }
}

// ========== VTK渲染控制实现 ==========

void PointRegistrationServiceImpl::pauseRendering()
{
    m_renderingPaused = true;

    // 暂停所有VTK Widget
    for (auto& widgetPtr : m_vtkWidgets) {
        if (widgetPtr) {
            PointRegistrationVTKWidget* vtkWidget = qobject_cast<PointRegistrationVTKWidget*>(widgetPtr.data());
            if (vtkWidget) {
                vtkWidget->pauseRendering();
            }
        }
    }

    // 也暂停旧版Widget
    for (auto& widgetPtr : m_createdWidgets) {
        if (widgetPtr) {
            widgetPtr->setUpdatesEnabled(false);
        }
    }

    logMessage("INFO", "VTK渲染已暂停");
}

void PointRegistrationServiceImpl::resumeRendering()
{
    m_renderingPaused = false;

    // 恢复所有VTK Widget
    for (auto& widgetPtr : m_vtkWidgets) {
        if (widgetPtr) {
            PointRegistrationVTKWidget* vtkWidget = qobject_cast<PointRegistrationVTKWidget*>(widgetPtr.data());
            if (vtkWidget) {
                vtkWidget->resumeRendering();
            }
        }
    }

    // 也恢复旧版Widget
    for (auto& widgetPtr : m_createdWidgets) {
        if (widgetPtr) {
            widgetPtr->setUpdatesEnabled(true);
            widgetPtr->update();
        }
    }

    logMessage("INFO", "VTK渲染已恢复");
}

// ========== 模型加载实现 ==========

bool PointRegistrationServiceImpl::loadModelFromSegmentation(const QString& segmentationTaskId,
                                                              const QString& bodyPart)
{
#ifdef VTK_FOUND
    if (!m_segmentationService) {
        m_lastError = QString::fromUtf8("分割服务未设置");
        logMessage("ERROR", m_lastError);
        emit modelLoaded(false, m_lastError);
        return false;
    }

    m_currentSession.state = RegistrationSessionState::ModelLoading;
    emit sessionStateChanged(m_currentSession.state);

    // 注意：此处需要根据实际的 SegmentationService 接口调用
    // vtkSmartPointer<vtkPolyData> polyData = m_segmentationService->getSegmentationMesh(segmentationTaskId, bodyPart);
    // 暂时使用占位实现
    m_lastError = QString::fromUtf8("分割服务集成待实现");
    logMessage("WARNING", m_lastError);

    m_modelSource = segmentationTaskId;
    m_modelName = bodyPart.isEmpty() ? QString::fromUtf8("分割模型") : bodyPart;

    // TODO: 实际加载实现
    emit modelLoaded(false, m_lastError);
    return false;
#else
    m_lastError = QString::fromUtf8("VTK未启用");
    emit modelLoaded(false, m_lastError);
    return false;
#endif
}

bool PointRegistrationServiceImpl::loadModelFromPolyData(vtkPolyData* polyData,
                                                          const QString& modelName)
{
#ifdef VTK_FOUND
    if (!polyData) {
        m_lastError = QString::fromUtf8("无效的 vtkPolyData 指针");
        logMessage("ERROR", m_lastError);
        emit modelLoaded(false, m_lastError);
        return false;
    }

    m_currentSession.state = RegistrationSessionState::ModelLoading;
    emit sessionStateChanged(m_currentSession.state);

    // 复制数据
    m_modelPolyData = vtkSmartPointer<vtkPolyData>::New();
    m_modelPolyData->DeepCopy(polyData);

    m_modelName = modelName.isEmpty() ? QString::fromUtf8("外部模型") : modelName;
    m_modelSource = QString::fromUtf8("PolyData");

    int numPoints = m_modelPolyData->GetNumberOfPoints();
    int numCells = m_modelPolyData->GetNumberOfCells();

    QString info = QString::fromUtf8("模型加载成功: %1 (%2 点, %3 面)")
                       .arg(m_modelName)
                       .arg(numPoints)
                       .arg(numCells);
    logMessage("INFO", info);

    // 更新所有 VTK Widget
    for (auto& widgetPtr : m_vtkWidgets) {
        if (widgetPtr) {
            PointRegistrationVTKWidget* vtkWidget = qobject_cast<PointRegistrationVTKWidget*>(widgetPtr.data());
            if (vtkWidget) {
                vtkWidget->loadModel(m_modelPolyData);
            }
        }
    }

    m_currentSession.state = RegistrationSessionState::PointCollection;
    m_currentSession.modelSource = m_modelSource;
    emit sessionStateChanged(m_currentSession.state);
    emit modelLoaded(true, info);
    return true;
#else
    m_lastError = QString::fromUtf8("VTK未启用");
    emit modelLoaded(false, m_lastError);
    return false;
#endif
}

bool PointRegistrationServiceImpl::loadModelFromFile(const QString& filePath)
{
#ifdef VTK_FOUND
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        m_lastError = QString::fromUtf8("文件不存在: %1").arg(filePath);
        logMessage("ERROR", m_lastError);
        emit modelLoaded(false, m_lastError);
        return false;
    }

    m_currentSession.state = RegistrationSessionState::ModelLoading;
    emit sessionStateChanged(m_currentSession.state);

    QString suffix = fileInfo.suffix().toLower();

    if (suffix == "stl") {
        vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
        reader->SetFileName(filePath.toStdString().c_str());
        reader->Update();

        m_modelPolyData = reader->GetOutput();

        if (!m_modelPolyData || m_modelPolyData->GetNumberOfPoints() == 0) {
            m_lastError = QString::fromUtf8("STL文件读取失败或为空");
            logMessage("ERROR", m_lastError);
            m_currentSession.state = RegistrationSessionState::Failed;
            emit sessionStateChanged(m_currentSession.state);
            emit modelLoaded(false, m_lastError);
            return false;
        }

        m_modelName = fileInfo.baseName();
        m_modelSource = filePath;

        int numPoints = m_modelPolyData->GetNumberOfPoints();
        int numCells = m_modelPolyData->GetNumberOfCells();

        QString info = QString::fromUtf8("STL模型加载成功: %1 (%2 点, %3 面)")
                           .arg(m_modelName)
                           .arg(numPoints)
                           .arg(numCells);
        logMessage("INFO", info);

        // 更新所有 VTK Widget
        for (auto& widgetPtr : m_vtkWidgets) {
            if (widgetPtr) {
                PointRegistrationVTKWidget* vtkWidget = qobject_cast<PointRegistrationVTKWidget*>(widgetPtr.data());
                if (vtkWidget) {
                    vtkWidget->loadModel(m_modelPolyData);
                }
            }
        }

        m_currentSession.state = RegistrationSessionState::PointCollection;
        m_currentSession.modelSource = filePath;
        emit sessionStateChanged(m_currentSession.state);
        emit modelLoaded(true, info);
        return true;
    } else {
        m_lastError = QString::fromUtf8("不支持的文件格式: %1").arg(suffix);
        logMessage("ERROR", m_lastError);
        emit modelLoaded(false, m_lastError);
        return false;
    }
#else
    m_lastError = QString::fromUtf8("VTK未启用");
    emit modelLoaded(false, m_lastError);
    return false;
#endif
}

QString PointRegistrationServiceImpl::getModelInfo() const
{
#ifdef VTK_FOUND
    if (!m_modelPolyData) {
        return QString::fromUtf8("未加载模型");
    }

    return QString::fromUtf8("模型: %1\n来源: %2\n点数: %3\n面数: %4")
               .arg(m_modelName)
               .arg(m_modelSource)
               .arg(m_modelPolyData->GetNumberOfPoints())
               .arg(m_modelPolyData->GetNumberOfCells());
#else
    return QString::fromUtf8("VTK未启用");
#endif
}

bool PointRegistrationServiceImpl::hasModel() const
{
#ifdef VTK_FOUND
    return m_modelPolyData != nullptr && m_modelPolyData->GetNumberOfPoints() > 0;
#else
    return false;
#endif
}

// ========== 探针点采集实现 ==========

void PointRegistrationServiceImpl::setProbePointSource(ProbePointSource source)
{
    m_probePointSource = source;
    m_currentSession.probeSource = source;
    logMessage("INFO", QString::fromUtf8("设置探针数据来源: %1").arg(probeSourceToString(source)));
}

ProbePointSource PointRegistrationServiceImpl::getProbePointSource() const
{
    return m_probePointSource;
}

bool PointRegistrationServiceImpl::captureProbePoint(int pointIndex)
{
    if (pointIndex < 0 || pointIndex >= m_points.size()) {
        m_lastError = QString::fromUtf8("无效的点索引: %1").arg(pointIndex);
        return false;
    }

    QVector3D probePosition;

    switch (m_probePointSource) {
        case ProbePointSource::Manual:
            // 手动模式：使用已设置的 targetPosition
            if (m_points[pointIndex].hasTarget) {
                probePosition = m_points[pointIndex].targetPosition;
            } else {
                m_lastError = QString::fromUtf8("手动模式下需要先设置目标点坐标");
                return false;
            }
            break;

        case ProbePointSource::Simulated:
            // 模拟模式：根据 CT 点生成模拟探针点
            if (!m_points[pointIndex].hasSource) {
                m_lastError = QString::fromUtf8("模拟模式需要先设置源点（CT点）");
                return false;
            }
            probePosition = m_probeSimulator->generateProbePoint(m_points[pointIndex].sourcePosition);
            setTargetPosition(pointIndex, probePosition);
            break;

        case ProbePointSource::OpticalTracking:
            // 光学跟踪模式：从跟踪服务获取当前位置
            probePosition = getCurrentProbePosition();
            if (probePosition.isNull()) {
                m_lastError = QString::fromUtf8("无法获取探针当前位置");
                return false;
            }
            setTargetPosition(pointIndex, probePosition);
            break;
    }

    logMessage("INFO", QString::fromUtf8("采集探针点 %1: (%2, %3, %4)")
                   .arg(m_points[pointIndex].name)
                   .arg(probePosition.x(), 0, 'f', 2)
                   .arg(probePosition.y(), 0, 'f', 2)
                   .arg(probePosition.z(), 0, 'f', 2));

    emit probePointCaptured(pointIndex, probePosition);
    return true;
}

void PointRegistrationServiceImpl::setTrackingSession(const QString& sessionId,
                                                       const QString& probeToolId)
{
    m_trackingSessionId = sessionId;
    m_probeToolId = probeToolId;
    m_currentSession.trackingSessionId = sessionId;
    m_currentSession.probeToolId = probeToolId;

    logMessage("INFO", QString::fromUtf8("设置跟踪会话: %1, 探针工具: %2")
                   .arg(sessionId, probeToolId));
}

QVector3D PointRegistrationServiceImpl::getCurrentProbePosition() const
{
    if (m_probePointSource == ProbePointSource::OpticalTracking) {
        // TODO: 从 OpticalTrackingService 获取实时位置
        // if (m_trackingService) {
        //     return m_trackingService->getToolPosition(m_probeToolId);
        // }
        return QVector3D();
    }

    return m_currentProbePosition;
}

// ========== 模拟数据实现 ==========

QVector3D PointRegistrationServiceImpl::generateSimulatedProbePoint(int pointIndex, double noiseLevel)
{
    if (pointIndex < 0 || pointIndex >= m_points.size()) {
        m_lastError = QString::fromUtf8("无效的点索引: %1").arg(pointIndex);
        return QVector3D();
    }

    if (!m_points[pointIndex].hasSource) {
        m_lastError = QString::fromUtf8("点 %1 未设置源点坐标").arg(m_points[pointIndex].name);
        return QVector3D();
    }

    m_probeSimulator->setNoiseLevel(noiseLevel);
    QVector3D probePoint = m_probeSimulator->generateProbePoint(m_points[pointIndex].sourcePosition);

    // 自动设置目标点
    setTargetPosition(pointIndex, probePoint);

    logMessage("INFO", QString::fromUtf8("生成模拟探针点 %1: (%2, %3, %4) 噪声水平: %5mm")
                   .arg(m_points[pointIndex].name)
                   .arg(probePoint.x(), 0, 'f', 2)
                   .arg(probePoint.y(), 0, 'f', 2)
                   .arg(probePoint.z(), 0, 'f', 2)
                   .arg(noiseLevel, 0, 'f', 2));

    emit probePointCaptured(pointIndex, probePoint);
    return probePoint;
}

int PointRegistrationServiceImpl::generateAllSimulatedProbePoints(double noiseLevel)
{
    int generatedCount = 0;

    m_probeSimulator->setNoiseLevel(noiseLevel);

    for (int i = 0; i < m_points.size(); ++i) {
        if (m_points[i].hasSource) {
            QVector3D probePoint = m_probeSimulator->generateProbePoint(m_points[i].sourcePosition);
            setTargetPosition(i, probePoint);
            emit probePointCaptured(i, probePoint);
            generatedCount++;
        }
    }

    logMessage("INFO", QString::fromUtf8("批量生成模拟探针点: %1/%2 个, 噪声水平: %3mm")
                   .arg(generatedCount)
                   .arg(m_points.size())
                   .arg(noiseLevel, 0, 'f', 2));

    return generatedCount;
}

void PointRegistrationServiceImpl::setSimulationTransform(const QMatrix4x4& transform)
{
    m_probeSimulator->setTransformMatrix(transform);
    logMessage("INFO", QString::fromUtf8("设置模拟变换矩阵"));
}

QMatrix4x4 PointRegistrationServiceImpl::getSimulationTransform() const
{
    return m_probeSimulator->getTransformMatrix();
}

// ========== 配准应用实现 ==========

bool PointRegistrationServiceImpl::applyRegistrationToNavigation(const QString& registrationId)
{
    if (!m_hasValidResult) {
        m_lastError = QString::fromUtf8("没有有效的配准结果可应用");
        return false;
    }

    // 更新会话状态
    m_currentSession.registrationMatrix = m_transformMatrix;
    m_currentSession.result = m_lastResult;
    m_currentSession.completedAt = QDateTime::currentDateTime();
    m_currentSession.state = RegistrationSessionState::Completed;

    logMessage("INFO", QString::fromUtf8("配准结果已应用，配准ID: %1, RMS误差: %2mm")
                   .arg(registrationId)
                   .arg(m_lastResult.rmsError, 0, 'f', 3));

    emit sessionStateChanged(m_currentSession.state);
    emit registrationApplied(registrationId);
    return true;
}

RegistrationSession PointRegistrationServiceImpl::getCurrentSession() const
{
    return m_currentSession;
}

// ========== 服务依赖注入 ==========

void PointRegistrationServiceImpl::setSegmentationService(SegmentationService* service)
{
    m_segmentationService = service;
    logMessage("INFO", QString::fromUtf8("分割服务已设置: %1")
                   .arg(service ? "有效" : "空"));
}

void PointRegistrationServiceImpl::setTrackingService(OpticalTrackingService* service)
{
    m_trackingService = service;
    logMessage("INFO", QString::fromUtf8("跟踪服务已设置: %1")
                   .arg(service ? "有效" : "空"));
}

// ========== 错误处理实现 ==========

QString PointRegistrationServiceImpl::getLastError() const
{
    return m_lastError;
}

// ========== 私有方法 ==========

double PointRegistrationServiceImpl::calculatePointError(
    const QVector3D& source, const QVector3D& target, const QMatrix4x4& transform) const
{
    QVector3D transformed = transform.map(source);
    return (transformed - target).length();
}

void PointRegistrationServiceImpl::calculateEulerAngles(
    const QMatrix4x4& matrix, double& rx, double& ry, double& rz) const
{
    // 从旋转矩阵提取ZYX欧拉角
    double r00 = matrix(0, 0);
    double r10 = matrix(1, 0);
    double r20 = matrix(2, 0);
    double r21 = matrix(2, 1);
    double r22 = matrix(2, 2);

    // 计算Y轴旋转角
    ry = qAsin(-r20);

    // 检查是否在奇异点附近
    if (qAbs(qCos(ry)) > 1e-6) {
        rx = qAtan2(r21, r22);
        rz = qAtan2(r10, r00);
    } else {
        // 奇异点处理
        rx = qAtan2(-matrix(1, 2), matrix(1, 1));
        rz = 0;
    }

    // 转换为度
    rx = qRadiansToDegrees(rx);
    ry = qRadiansToDegrees(ry);
    rz = qRadiansToDegrees(rz);
}

void PointRegistrationServiceImpl::cleanupDestroyedWidgets()
{
    m_createdWidgets.removeAll(QPointer<QWidget>());
}

void PointRegistrationServiceImpl::logMessage(const QString& level, const QString& message) const
{
    QString formattedMsg = QString("[PointRegistrationService][%1] %2").arg(level, message);

    if (level == "ERROR") {
        qCritical().noquote() << formattedMsg;
    } else if (level == "WARNING") {
        qWarning().noquote() << formattedMsg;
    } else {
        qDebug().noquote() << formattedMsg;
    }
}
