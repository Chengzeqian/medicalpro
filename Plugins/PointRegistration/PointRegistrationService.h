#ifndef POINT_REGISTRATION_SERVICE_H
#define POINT_REGISTRATION_SERVICE_H

/**
 * @file PointRegistrationService.h
 * @brief 基于点的配准服务接口
 *
 * 提供基于特征点的医学图像配准服务接口
 * 支持刚体、相似性和仿射变换
 */

#include "PointRegistrationDataStructures.h"
#include <QObject>

class QWidget;
class vtkPolyData;

/**
 * @brief 点配准服务接口
 *
 * 纯虚接口，遵循CTK服务架构设计原则。
 * 支持:
 * - 源点和目标点管理
 * - 多种变换模式配准
 * - 实时精度指标计算
 * - Widget工厂创建
 */
class PointRegistrationService : public QObject
{
    Q_OBJECT

public:
    explicit PointRegistrationService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~PointRegistrationService() = default;

    // ========== 点管理 ==========

    /**
     * @brief 添加配准点
     * @param name 点名称 (可选，自动生成)
     * @return 点索引
     */
    virtual int addPoint(const QString& name = QString()) = 0;

    /**
     * @brief 移除配准点
     * @param index 点索引
     * @return 成功返回true
     */
    virtual bool removePoint(int index) = 0;

    /**
     * @brief 清空所有配准点
     */
    virtual void clearPoints() = 0;

    /**
     * @brief 获取配准点数量
     */
    virtual int pointCount() const = 0;

    /**
     * @brief 获取配准点
     * @param index 点索引
     * @return 配准点
     */
    virtual RegistrationPoint getPoint(int index) const = 0;

    /**
     * @brief 获取所有配准点
     * @return 配准点列表
     */
    virtual QVector<RegistrationPoint> getAllPoints() const = 0;

    /**
     * @brief 设置源点坐标
     * @param index 点索引
     * @param position 源点坐标
     * @return 成功返回true
     */
    virtual bool setSourcePosition(int index, const QVector3D& position) = 0;

    /**
     * @brief 设置目标点坐标
     * @param index 点索引
     * @param position 目标点坐标
     * @return 成功返回true
     */
    virtual bool setTargetPosition(int index, const QVector3D& position) = 0;

    /**
     * @brief 设置点名称
     * @param index 点索引
     * @param name 点名称
     * @return 成功返回true
     */
    virtual bool setPointName(int index, const QString& name) = 0;

    // ========== 配准执行 ==========

    /**
     * @brief 设置变换模式
     * @param mode 变换模式
     */
    virtual void setTransformMode(TransformMode mode) = 0;

    /**
     * @brief 获取当前变换模式
     */
    virtual TransformMode getTransformMode() const = 0;

    /**
     * @brief 执行配准
     * @return 配准结果
     */
    virtual PointRegistrationResult executeRegistration() = 0;

    /**
     * @brief 检查是否可以执行配准
     * @return 至少有3个完整点对返回true
     */
    virtual bool canExecuteRegistration() const = 0;

    /**
     * @brief 获取最后一次配准结果
     */
    virtual PointRegistrationResult getLastResult() const = 0;

    /**
     * @brief 获取变换矩阵
     */
    virtual QMatrix4x4 getTransformMatrix() const = 0;

    /**
     * @brief 应用变换到点
     * @param point 输入点
     * @return 变换后的点
     */
    virtual QVector3D transformPoint(const QVector3D& point) const = 0;

    // ========== 模型加载 ==========

    /**
     * @brief 从分割服务加载模型
     * @param segmentationTaskId 分割任务ID
     * @param bodyPart 身体部位（可选）
     * @return 成功返回true
     */
    virtual bool loadModelFromSegmentation(const QString& segmentationTaskId,
                                           const QString& bodyPart = QString()) = 0;

    /**
     * @brief 直接加载 vtkPolyData 模型
     * @param polyData VTK 数据
     * @param modelName 模型名称
     * @return 成功返回true
     */
    virtual bool loadModelFromPolyData(vtkPolyData* polyData,
                                       const QString& modelName = QString()) = 0;

    /**
     * @brief 加载 STL 文件
     * @param filePath 文件路径
     * @return 成功返回true
     */
    virtual bool loadModelFromFile(const QString& filePath) = 0;

    /**
     * @brief 获取当前模型信息
     * @return 模型信息字符串
     */
    virtual QString getModelInfo() const = 0;

    /**
     * @brief 检查是否已加载模型
     */
    virtual bool hasModel() const = 0;

    // ========== 探针点采集 ==========

    /**
     * @brief 设置探针点数据来源
     * @param source 数据来源
     */
    virtual void setProbePointSource(ProbePointSource source) = 0;

    /**
     * @brief 获取当前探针点数据来源
     */
    virtual ProbePointSource getProbePointSource() const = 0;

    /**
     * @brief 采集当前探针位置作为目标点
     * @param pointIndex 点索引
     * @return 成功返回true
     *
     * 根据 ProbePointSource：
     * - Manual: 使用 setTargetPosition() 手动设置
     * - Simulated: 调用模拟器生成对应位置
     * - OpticalTracking: 从跟踪服务获取当前位置
     */
    virtual bool captureProbePoint(int pointIndex) = 0;

    /**
     * @brief 设置光学跟踪会话
     * @param sessionId 会话ID
     * @param probeToolId 探针工具ID
     */
    virtual void setTrackingSession(const QString& sessionId,
                                    const QString& probeToolId) = 0;

    /**
     * @brief 获取当前探针位置
     * @return 探针位置，无效返回 QVector3D()
     */
    virtual QVector3D getCurrentProbePosition() const = 0;

    // ========== 模拟数据 ==========

    /**
     * @brief 生成模拟探针点
     * @param pointIndex 点索引
     * @param noiseLevel 噪声水平(mm)，默认0.5mm
     * @return 生成的模拟点坐标
     *
     * 根据对应的 CT 点（sourcePosition）生成带噪声的模拟探针点
     */
    virtual QVector3D generateSimulatedProbePoint(int pointIndex, double noiseLevel = 0.5) = 0;

    /**
     * @brief 批量生成所有模拟探针点
     * @param noiseLevel 噪声水平(mm)
     * @return 成功生成的点数
     */
    virtual int generateAllSimulatedProbePoints(double noiseLevel = 0.5) = 0;

    /**
     * @brief 设置模拟变换矩阵
     * @param transform 预设的变换矩阵（用于生成模拟数据）
     */
    virtual void setSimulationTransform(const QMatrix4x4& transform) = 0;

    /**
     * @brief 获取模拟变换矩阵
     */
    virtual QMatrix4x4 getSimulationTransform() const = 0;

    // ========== 配准应用 ==========

    /**
     * @brief 应用配准结果到导航系统
     * @param registrationId 配准ID（用于保存和追踪）
     * @return 成功返回true
     */
    virtual bool applyRegistrationToNavigation(const QString& registrationId) = 0;

    /**
     * @brief 获取当前配准会话
     */
    virtual RegistrationSession getCurrentSession() const = 0;

    // ========== Widget工厂 ==========

    /**
     * @brief 创建配准Widget（包含完整UI，旧接口）
     * @param parent 父Widget
     * @return Widget指针
     * @deprecated 建议使用 createVTKWidget() 并在主程序设计控制UI
     */
    virtual QWidget* createRegistrationWidget(QWidget* parent = nullptr) = 0;

    /**
     * @brief 创建纯VTK 3D视图Widget（推荐）
     * @param parent 父Widget
     * @return QWidget* 指针（实际类型为 PointRegistrationVTKWidget）
     * @note 只包含3D视图和选点功能，不包含控制UI
     * @note 控制UI应在主程序的.ui文件中设计
     */
    virtual QWidget* createVTKWidget(QWidget* parent = nullptr) = 0;

    // ========== VTK渲染控制（防闪烁） ==========

    /**
     * @brief 暂停VTK渲染
     * @note 在页面切换前调用，防止隐藏的VTK Widget继续渲染导致闪烁
     */
    virtual void pauseRendering() = 0;

    /**
     * @brief 恢复VTK渲染
     * @note 在页面切换后调用
     */
    virtual void resumeRendering() = 0;

    // ========== 错误处理 ==========

    /**
     * @brief 获取最后错误信息
     */
    virtual QString getLastError() const = 0;

signals:
    // 点管理信号
    void pointAdded(int index, const QString& name);
    void pointRemoved(int index);
    void pointsCleared();
    void pointUpdated(int index);

    // 配准执行信号
    void registrationStarted();
    void registrationCompleted(const PointRegistrationResult& result);
    void registrationFailed(const QString& error);
    void progressUpdated(int progress, const QString& message);

    // 模型加载信号
    void modelLoaded(bool success, const QString& info);
    void modelCleared();

    // 探针采集信号
    void probePointCaptured(int index, const QVector3D& position);
    void probePositionUpdated(const QVector3D& position);

    // 配准应用信号
    void registrationApplied(const QString& registrationId);

    // 会话状态信号
    void sessionStateChanged(RegistrationSessionState state);
};

// CTK服务接口声明
Q_DECLARE_INTERFACE(PointRegistrationService, "org.medicalpro.PointRegistrationService")

#endif // POINT_REGISTRATION_SERVICE_H

