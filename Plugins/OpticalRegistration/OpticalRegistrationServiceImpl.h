#ifndef OPTICAL_REGISTRATION_SERVICE_IMPL_H
#define OPTICAL_REGISTRATION_SERVICE_IMPL_H

/**
 * @file OpticalRegistrationServiceImpl.h
 * @brief 光学配准服务实现
 *
 * 集成Atracsys光学跟踪系统，实现tracker坐标系到影像坐标系的刚体配准
 */

#include "OpticalRegistrationService.h"
#include <QVector>
#include <QPointer>
#include <QMutex>

// CTK框架
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

// 前向声明
class OpticalTrackingService;

#ifdef VTK_FOUND
#include <vtkSmartPointer.h>
class vtkLandmarkTransform;
class vtkPoints;
#endif

/**
 * @brief 光学配准服务实现类
 *
 * 使用VTK的vtkLandmarkTransform实现刚体配准算法，
 * 通过OpticalTrackingService获取实时跟踪数据
 */
class OpticalRegistrationServiceImpl : public OpticalRegistrationService
{
    Q_OBJECT
    Q_INTERFACES(OpticalRegistrationService)

public:
    /**
     * @brief 构造函数
     * @param context CTK插件上下文
     * @param parent 父对象
     */
    explicit OpticalRegistrationServiceImpl(ctkPluginContext* context, QObject* parent = nullptr);
    ~OpticalRegistrationServiceImpl() override;

    // ========== 跟踪上下文配置 ==========
    void setTrackingContext(const QString& sessionId,
                            const QString& referenceToolId,
                            const QString& pointerToolId) override;

    // ========== 配准点管理 ==========
    int addPoint(const QString& name = QString()) override;
    bool setImagePosition(int index, const QVector3D& position) override;
    bool captureTrackerPosition(int index) override;
    void setActivePointIndex(int index) override;
    int getActivePointIndex() const override;
    bool setImagePositionForActivePoint(const QVector3D& position) override;
    void clearPoints() override;
    int pointCount() const override;
    OpticalRegistrationPoint getPoint(int index) const override;
    QVector<OpticalRegistrationPoint> getAllPoints() const override;

    // ========== 配准执行 ==========
    bool canExecuteRegistration() const override;
    OpticalRegistrationResult executeRegistration() override;
    OpticalRegistrationResult getLastResult() const override;
    QMatrix4x4 getRegistrationMatrix() const override;

    // ========== Widget工厂 ==========
    QWidget* createRegistrationWidget(QWidget* parent = nullptr) override;
    QWidget* createVTKWidget(QWidget* parent = nullptr) override;

    // ========== VTK渲染控制 ==========
    void pauseRendering() override;
    void resumeRendering() override;

    // ========== 错误信息 ==========
    QString getLastError() const override;

    // ========== 服务管理 ==========
    void startService();
    void stopService();
    QString getServiceName() const;
    QString getServiceVersion() const;

private slots:
    /**
     * @brief 处理跟踪服务可用性变化
     */
    void onTrackingServiceAvailabilityChanged(bool available);

private:
    /**
     * @brief 初始化可选的服务连接
     */
    void initializeOptionalServiceConnections();

    /**
     * @brief 获取OpticalTrackingService
     */
    OpticalTrackingService* getTrackingService();

    /**
     * @brief 从光学跟踪系统获取指针工具当前位置
     * @return 3D位置，失败返回零向量
     */
    QVector3D getCurrentPointerPosition();

    /**
     * @brief 计算配准误差
     */
    void calculateRegistrationErrors(OpticalRegistrationResult& result);

    /**
     * @brief 设置错误信息
     */
    void setError(const QString& error);

    /**
     * @brief 日志输出
     */
    void logMessage(const QString& level, const QString& message) const;

    /**
     * @brief 清理已销毁的Widget
     */
    void cleanupDestroyedWidgets();

private:
    // CTK插件上下文
    ctkPluginContext* m_pluginContext;

    // 跟踪服务引用
    ctkServiceReference m_trackingServiceRef;
    OpticalTrackingService* m_trackingService;
    bool m_trackingServiceConnected;

    // 跟踪上下文
    QString m_sessionId;
    QString m_referenceToolId;
    QString m_pointerToolId;

    // 配准数据
    QVector<OpticalRegistrationPoint> m_points;
    int m_activePointIndex;
    QMatrix4x4 m_registrationMatrix;
    OpticalRegistrationResult m_lastResult;
    bool m_hasValidResult;

    // 错误信息
    QString m_lastError;
    mutable QMutex m_mutex;

    // Widget管理
    QVector<QPointer<QWidget>> m_createdWidgets;
    QVector<QPointer<QWidget>> m_vtkWidgets;  // 纯VTK Widget列表
    bool m_renderingPaused;

#ifdef VTK_FOUND
    vtkSmartPointer<vtkLandmarkTransform> m_landmarkTransform;
#endif
};

#endif // OPTICAL_REGISTRATION_SERVICE_IMPL_H

