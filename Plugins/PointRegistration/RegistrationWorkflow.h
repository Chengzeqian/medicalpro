#ifndef REGISTRATION_WORKFLOW_H
#define REGISTRATION_WORKFLOW_H

/**
 * @file RegistrationWorkflow.h
 * @brief 配准工作流协调器
 *
 * 管理配准会话的完整生命周期，协调各服务调用顺序。
 * 提供高层次的工作流控制，简化 UI 层的代码。
 */

#include "PointRegistrationDataStructures.h"
#include "registration_point_strategy_registry.h"
#include <QObject>
#include <QMap>
#include <QVector3D>
#include <QMatrix4x4>

class PointRegistrationService;

/**
 * @brief 配准工作流协调器
 *
 * 功能：
 * - 管理配准会话状态机
 * - 协调服务调用顺序
 * - 发送进度和状态信号
 * - 验证配准精度
 *
 * 使用示例：
 * @code
 * RegistrationWorkflow* workflow = new RegistrationWorkflow(pointRegService, this);
 * connect(workflow, &RegistrationWorkflow::stateChanged, this, &Page::onStateChanged);
 * connect(workflow, &RegistrationWorkflow::registrationCompleted, this, &Page::onCompleted);
 *
 * workflow->startNewSession("patient_001");
 * workflow->loadModel("D:/models/bone.stl");
 * workflow->addCtPoint(QVector3D(10, 20, 30));
 * workflow->generateSimulatedProbePoints(0.5);
 * workflow->executeRegistration();
 * @endcode
 */
class RegistrationWorkflow : public QObject
{
    Q_OBJECT

public:
    explicit RegistrationWorkflow(PointRegistrationService* service, QObject* parent = nullptr);
    ~RegistrationWorkflow() = default;

    // ========== 会话管理 ==========

    /**
     * @brief 开始新的配准会话
     * @param patientId 患者ID（可选）
     * @return 会话ID
     */
    QString startNewSession(const QString& patientId = QString());

    /**
     * @brief 重置当前会话
     */
    void resetSession();

    /**
     * @brief 获取当前会话ID
     */
    QString currentSessionId() const;

    /**
     * @brief 获取当前状态
     */
    RegistrationSessionState currentState() const;

    // ========== 模型加载 ==========

    /**
     * @brief 从文件加载模型
     * @param filePath STL/OBJ 文件路径
     * @return 成功返回true
     */
    bool loadModel(const QString& filePath);

    /**
     * @brief 从分割任务加载模型
     * @param taskId 分割任务ID
     * @param bodyPart 身体部位
     * @return 成功返回true
     */
    bool loadModelFromSegmentation(const QString& taskId, const QString& bodyPart = QString());

    /**
     * @brief 检查是否已加载模型
     */
    bool hasModel() const;

    // ========== CT点管理 ==========

    /**
     * @brief 添加CT点（源点）
     * @param position CT空间坐标
     * @param name 点名称（可选）
     * @return 点索引
     */
    int addCtPoint(const QVector3D& position, const QString& name = QString());

    /**
     * @brief 移除CT点
     * @param index 点索引
     * @return 成功返回true
     */
    bool removeCtPoint(int index);

    /**
     * @brief 清空所有CT点
     */
    void clearCtPoints();

    /**
     * @brief 获取CT点数量
     */
    int ctPointCount() const;

    /**
     * @brief 获取有效点对数量（同时有CT点和探针点）
     */
    int validPairCount() const;

    // ========== 探针点采集 ==========

    /**
     * @brief 设置探针数据来源
     * @param source 数据来源
     */
    void setProbeSource(ProbePointSource source);

    /**
     * @brief 采集探针点
     * @param pointIndex 点索引
     * @return 成功返回true
     *
     * 根据当前 ProbeSource:
     * - Manual: 需要先调用 setProbePosition
     * - Simulated: 自动生成模拟点
     * - OpticalTracking: 从跟踪设备获取
     */
    bool captureProbePoint(int pointIndex);

    /**
     * @brief 手动设置探针点位置
     * @param pointIndex 点索引
     * @param position 探针坐标
     */
    bool setProbePosition(int pointIndex, const QVector3D& position);

    /**
     * @brief 生成所有模拟探针点
     * @param noiseLevel 噪声水平 (mm)
     * @return 生成的点数
     */
    int generateSimulatedProbePoints(double noiseLevel = 0.5);

    void setExecutionOptions(const PointRegistrationExecutionOptions& options);
    PointRegistrationExecutionOptions executionOptions() const;

    // ========== 配准执行 ==========

    /**
     * @brief 检查是否可以执行配准
     * @return 至少有3个有效点对返回true
     */
    bool canExecute() const;

    /**
     * @brief 执行配准
     * @return 成功返回true
     */
    bool executeRegistration();
    void setTargetRegistrationRegion(const TargetRegistrationRegion& region);
    void setPlanningConstraintContext(const QVariantMap& context);
    void setPlanningConstraintRegions(const QMap<QString, QList<QVector3D>>& regions);
    QList<RecommendedRegistrationPoint> recommendRegistrationPoints(
        const QList<CandidateRegistrationPoint>& candidates) const;
    QList<RecommendedRegistrationPoint> recommendRegistrationPoints(
        const QList<CandidateRegistrationPoint>& candidates,
        const QString& strategyId) const;

    /**
     * @brief 获取最后配准结果
     */
    PointRegistrationResult getLastResult() const;

    /**
     * @brief 获取变换矩阵
     */
    QMatrix4x4 getTransformMatrix() const;

    // ========== 精度评估 ==========

    /**
     * @brief 评估配准质量
     * @return 质量等级 (0-3: 差/可接受/良好/优秀)
     */
    int evaluateQuality() const;

    /**
     * @brief 获取质量描述
     * @return 描述字符串
     */
    QString getQualityDescription() const;

    /**
     * @brief 获取改进建议
     * @return 建议列表
     */
    QStringList getImprovementSuggestions() const;

    // ========== 配准应用 ==========

    /**
     * @brief 应用配准结果到导航
     * @return 成功返回true
     */
    bool applyToNavigation();

signals:
    /**
     * @brief 会话状态变化信号
     * @param state 新状态
     */
    void stateChanged(RegistrationSessionState state);

    /**
     * @brief 进度更新信号
     * @param progress 进度百分比 (0-100)
     * @param message 进度消息
     */
    void progressUpdated(int progress, const QString& message);

    /**
     * @brief 模型加载完成信号
     * @param success 是否成功
     * @param info 模型信息
     */
    void modelLoaded(bool success, const QString& info);

    /**
     * @brief CT点添加信号
     * @param index 点索引
     * @param position 点位置
     */
    void ctPointAdded(int index, const QVector3D& position);

    /**
     * @brief 探针点采集信号
     * @param index 点索引
     * @param position 探针位置
     */
    void probePointCaptured(int index, const QVector3D& position);

    /**
     * @brief 配准完成信号
     * @param result 配准结果
     */
    void registrationCompleted(const PointRegistrationResult& result);

    /**
     * @brief 配准失败信号
     * @param error 错误信息
     */
    void registrationFailed(const QString& error);

    /**
     * @brief 配准应用完成信号
     * @param registrationId 配准ID
     */
    void registrationApplied(const QString& registrationId);

    /**
     * @brief 错误发生信号
     * @param error 错误信息
     */
    void errorOccurred(const QString& error);

private:
    void decorateRegistrationResult(PointRegistrationResult& result) const;
    void invalidateLastResult();
    void setState(RegistrationSessionState state);
    void connectServiceSignals();

private:
    PointRegistrationService* m_service;
    QString m_sessionId;
    RegistrationSessionState m_state;
    ProbePointSource m_probeSource;
    PointRegistrationExecutionOptions m_executionOptions;
    PointRegistrationResult m_lastResult;
    TargetRegistrationRegion m_targetRegion;
    QMap<QString, QList<QVector3D>> m_planningConstraintRegions;
    QVariantMap m_planningConstraintContext;
    RegistrationPointStrategyRegistry m_strategyRegistry;
};

#endif // REGISTRATION_WORKFLOW_H
