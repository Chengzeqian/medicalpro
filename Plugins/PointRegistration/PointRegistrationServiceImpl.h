#ifndef POINT_REGISTRATION_SERVICE_IMPL_H
#define POINT_REGISTRATION_SERVICE_IMPL_H

/**
 * @file PointRegistrationServiceImpl.h
 * @brief 基于点的配准服务实现
 */

#include "PointRegistrationService.h"
#include "ProbeSimulator.h"
#include <QVector>
#include <QPointer>

#ifdef VTK_FOUND
#include <vtkSmartPointer.h>
class vtkLandmarkTransform;
class vtkPolyData;
class vtkSTLReader;
#endif

// 前向声明服务依赖
class SegmentationService;
class OpticalTrackingService;

/**
 * @brief 点配准服务实现类
 *
 * 使用VTK的vtkLandmarkTransform实现配准算法
 */
class PointRegistrationServiceImpl : public PointRegistrationService
{
    Q_OBJECT
    Q_INTERFACES(PointRegistrationService)

public:
    explicit PointRegistrationServiceImpl(QObject* parent = nullptr);
    ~PointRegistrationServiceImpl() override;

    // ========== 点管理实现 ==========
    int addPoint(const QString& name = QString()) override;
    bool removePoint(int index) override;
    void clearPoints() override;
    int pointCount() const override;
    RegistrationPoint getPoint(int index) const override;
    QVector<RegistrationPoint> getAllPoints() const override;
    bool setSourcePosition(int index, const QVector3D& position) override;
    bool setTargetPosition(int index, const QVector3D& position) override;
    bool setPointName(int index, const QString& name) override;

    // ========== 配准执行实现 ==========
    void setTransformMode(TransformMode mode) override;
    TransformMode getTransformMode() const override;
    PointRegistrationResult executeRegistration() override;
    bool canExecuteRegistration() const override;
    PointRegistrationResult getLastResult() const override;
    QMatrix4x4 getTransformMatrix() const override;
    QVector3D transformPoint(const QVector3D& point) const override;

    // ========== 模型加载实现 ==========
    bool loadModelFromSegmentation(const QString& segmentationTaskId,
                                   const QString& bodyPart = QString()) override;
    bool loadModelFromPolyData(vtkPolyData* polyData,
                               const QString& modelName = QString()) override;
    bool loadModelFromFile(const QString& filePath) override;
    QString getModelInfo() const override;
    bool hasModel() const override;

    // ========== 探针点采集实现 ==========
    void setProbePointSource(ProbePointSource source) override;
    ProbePointSource getProbePointSource() const override;
    bool captureProbePoint(int pointIndex) override;
    void setTrackingSession(const QString& sessionId,
                            const QString& probeToolId) override;
    QVector3D getCurrentProbePosition() const override;

    // ========== 模拟数据实现 ==========
    QVector3D generateSimulatedProbePoint(int pointIndex, double noiseLevel = 0.5) override;
    int generateAllSimulatedProbePoints(double noiseLevel = 0.5) override;
    void setSimulationTransform(const QMatrix4x4& transform) override;
    QMatrix4x4 getSimulationTransform() const override;

    // ========== 配准应用实现 ==========
    bool applyRegistrationToNavigation(const QString& registrationId) override;
    RegistrationSession getCurrentSession() const override;

    // ========== Widget工厂实现 ==========
    QWidget* createRegistrationWidget(QWidget* parent = nullptr) override;
    QWidget* createVTKWidget(QWidget* parent = nullptr) override;

    // ========== VTK渲染控制实现 ==========
    void pauseRendering() override;
    void resumeRendering() override;

    // ========== 错误处理实现 ==========
    QString getLastError() const override;

    // ========== 服务依赖注入 ==========
    void setSegmentationService(SegmentationService* service);
    void setTrackingService(OpticalTrackingService* service);

private:
    /**
     * @brief 计算两点之间的误差
     */
    double calculatePointError(const QVector3D& source, const QVector3D& target, 
                               const QMatrix4x4& transform) const;
    
    /**
     * @brief 从变换矩阵提取欧拉角
     */
    void calculateEulerAngles(const QMatrix4x4& matrix, 
                              double& rx, double& ry, double& rz) const;
    
    /**
     * @brief 清理已销毁的Widget
     */
    void cleanupDestroyedWidgets();
    
    /**
     * @brief 日志输出
     */
    void logMessage(const QString& level, const QString& message) const;

private:
    QVector<RegistrationPoint> m_points;           ///< 配准点列表
    TransformMode m_transformMode;                 ///< 当前变换模式
    QMatrix4x4 m_transformMatrix;                  ///< 当前变换矩阵
    PointRegistrationResult m_lastResult;          ///< 最后配准结果
    bool m_hasValidResult;                         ///< 是否有有效结果
    QString m_lastError;                           ///< 最后错误信息
    QVector<QPointer<QWidget>> m_createdWidgets;   ///< 已创建的Widget
    QVector<QPointer<QWidget>> m_vtkWidgets;       ///< 已创建的纯VTK Widget
    bool m_renderingPaused;                        ///< 渲染是否暂停

    // 会话和探针状态
    RegistrationSession m_currentSession;          ///< 当前配准会话
    ProbePointSource m_probePointSource;           ///< 探针点数据来源
    QString m_trackingSessionId;                   ///< 跟踪会话ID
    QString m_probeToolId;                         ///< 探针工具ID
    QVector3D m_currentProbePosition;              ///< 当前探针位置

    // 模型信息
    QString m_modelName;                           ///< 当前模型名称
    QString m_modelSource;                         ///< 模型来源（任务ID或文件路径）

    // 探针模拟器
    ProbeSimulator* m_probeSimulator;              ///< 探针模拟器

    // 服务依赖
    SegmentationService* m_segmentationService;    ///< 分割服务（可选）
    OpticalTrackingService* m_trackingService;     ///< 跟踪服务（可选）

#ifdef VTK_FOUND
    vtkSmartPointer<vtkLandmarkTransform> m_landmarkTransform;  ///< VTK配准对象
    vtkSmartPointer<vtkPolyData> m_modelPolyData;               ///< 当前加载的模型
#endif
};

#endif // POINT_REGISTRATION_SERVICE_IMPL_H

