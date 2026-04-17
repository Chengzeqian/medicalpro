#ifndef PROBE_SIMULATOR_H
#define PROBE_SIMULATOR_H

/**
 * @file ProbeSimulator.h
 * @brief 探针模拟器类声明
 *
 * 用于开发测试阶段生成模拟的探针采集数据。
 * 支持多种噪声模型，模拟真实光学跟踪设备的误差特性。
 */

#include <QObject>
#include <QVector3D>
#include <QMatrix4x4>
#include <QRandomGenerator>
#include <QVector>

/**
 * @brief 噪声模型枚举
 */
enum class NoiseModel {
    Gaussian,   ///< 高斯噪声 (正态分布)
    Uniform,    ///< 均匀噪声 (均匀分布)
    Clinical    ///< 临床噪声 (模拟真实临床环境)
};

/**
 * @brief 探针模拟器类
 *
 * 功能：
 * - 根据 CT 点生成模拟的探针采集点
 * - 支持预设变换矩阵（模拟患者体位变化）
 * - 三种噪声模型：Gaussian、Uniform、Clinical
 * - 生成公式：probePoint = T^(-1) * ctPoint + noise
 *
 * 使用示例：
 * @code
 * ProbeSimulator simulator;
 * simulator.setTransformMatrix(patientTransform);
 * simulator.setNoiseLevel(0.5);  // 0.5mm 噪声
 *
 * QVector3D ctPoint(100, 50, 30);
 * QVector3D probePoint = simulator.generateProbePoint(ctPoint);
 * @endcode
 */
class ProbeSimulator : public QObject
{
    Q_OBJECT

public:
    explicit ProbeSimulator(QObject* parent = nullptr);
    ~ProbeSimulator() = default;

    // ========== 变换矩阵设置 ==========

    /**
     * @brief 设置模拟变换矩阵
     * @param transform CT空间到物理空间的变换矩阵
     *
     * 此矩阵模拟患者在手术台上的实际位置。
     * 探针点 = T^(-1) * CT点 + 噪声
     */
    void setTransformMatrix(const QMatrix4x4& transform);

    /**
     * @brief 获取当前变换矩阵
     */
    QMatrix4x4 getTransformMatrix() const;

    /**
     * @brief 设置默认变换矩阵
     *
     * 预设变换：绕 Z 轴旋转 15°，平移 (50, 30, -20) mm
     * 用于快速测试
     */
    void setDefaultTransform();

    /**
     * @brief 重置变换矩阵为单位矩阵
     */
    void resetTransform();

    // ========== 噪声设置 ==========

    /**
     * @brief 设置噪声水平
     * @param level 噪声标准差 (mm)
     *
     * 推荐值：
     * - 低噪声：0.3mm（理想条件）
     * - 中噪声：0.5mm（典型临床）
     * - 高噪声：1.0mm（困难条件）
     */
    void setNoiseLevel(double level);

    /**
     * @brief 获取当前噪声水平
     */
    double getNoiseLevel() const;

    /**
     * @brief 设置噪声模型
     * @param model 噪声模型类型
     */
    void setNoiseModel(NoiseModel model);

    /**
     * @brief 获取当前噪声模型
     */
    NoiseModel getNoiseModel() const;

    // ========== 点生成 ==========

    /**
     * @brief 生成模拟探针点
     * @param ctPoint CT 空间中的点坐标
     * @return 模拟的探针采集点（物理空间）
     *
     * 计算公式：probePoint = T^(-1) * ctPoint + noise
     */
    QVector3D generateProbePoint(const QVector3D& ctPoint);

    /**
     * @brief 批量生成模拟探针点
     * @param ctPoints CT 点列表
     * @return 模拟探针点列表
     */
    QVector<QVector3D> generateProbePoints(const QVector<QVector3D>& ctPoints);

    /**
     * @brief 生成不带噪声的探针点
     * @param ctPoint CT 空间中的点坐标
     * @return 理想探针点（无噪声）
     *
     * 用于计算理论误差
     */
    QVector3D generateIdealProbePoint(const QVector3D& ctPoint);

    // ========== 轨迹模拟 ==========

    /**
     * @brief 生成实时探针位置
     * @param targetPosition 目标位置
     * @param approachSpeed 接近速度 (mm/s)
     * @param deltaTime 时间步长 (s)
     * @return 当前模拟位置
     *
     * 模拟探针逐渐接近目标点的过程
     */
    QVector3D simulateProbeApproach(const QVector3D& targetPosition,
                                     double approachSpeed = 50.0,
                                     double deltaTime = 0.033);

    /**
     * @brief 重置轨迹模拟
     */
    void resetTrajectory();

    /**
     * @brief 设置轨迹起始位置
     * @param position 起始位置
     */
    void setTrajectoryStartPosition(const QVector3D& position);

    // ========== 随机种子 ==========

    /**
     * @brief 设置随机种子（用于可重复测试）
     * @param seed 随机种子
     */
    void setSeed(quint32 seed);

    /**
     * @brief 重置为系统随机种子
     */
    void resetSeed();

    // ========== 统计信息 ==========

    /**
     * @brief 获取噪声模型描述
     * @param model 噪声模型
     * @return 描述字符串
     */
    static QString noiseModelToString(NoiseModel model);

    /**
     * @brief 获取推荐噪声水平
     * @param condition 条件描述 ("ideal", "typical", "difficult")
     * @return 推荐噪声水平 (mm)
     */
    static double getRecommendedNoiseLevel(const QString& condition);

signals:
    /**
     * @brief 轨迹位置更新信号
     * @param position 当前位置
     * @param distanceToTarget 到目标的距离
     */
    void trajectoryPositionUpdated(const QVector3D& position, double distanceToTarget);

    /**
     * @brief 到达目标信号
     * @param finalPosition 最终位置（含噪声）
     */
    void targetReached(const QVector3D& finalPosition);

private:
    /**
     * @brief 生成高斯噪声
     * @return 噪声向量
     */
    QVector3D generateGaussianNoise();

    /**
     * @brief 生成均匀噪声
     * @return 噪声向量
     */
    QVector3D generateUniformNoise();

    /**
     * @brief 生成临床噪声
     * @return 噪声向量
     *
     * 临床噪声模型：
     * - 基础高斯噪声
     * - 偶尔的离群点（模拟遮挡、反射等）
     * - 方向相关性（某些方向误差更大）
     */
    QVector3D generateClinicalNoise();

    /**
     * @brief 使用 Box-Muller 变换生成标准正态随机数
     * @return 标准正态随机数
     */
    double generateStandardNormal();

private:
    QMatrix4x4 m_transformMatrix;       ///< 变换矩阵
    QMatrix4x4 m_inverseTransform;      ///< 变换矩阵的逆
    double m_noiseLevel;                ///< 噪声水平 (mm)
    NoiseModel m_noiseModel;            ///< 噪声模型

    QRandomGenerator m_randomGenerator; ///< 随机数生成器

    // 轨迹模拟状态
    QVector3D m_currentPosition;        ///< 当前位置
    bool m_trajectoryActive;            ///< 轨迹是否激活

    // 临床噪声参数
    double m_outlierProbability;        ///< 离群点概率
    double m_outlierMultiplier;         ///< 离群点放大系数
    QVector3D m_directionalBias;        ///< 方向偏置
};

#endif // PROBE_SIMULATOR_H
