#ifndef BONESURFACEMOTIONSIMULATOR_H
#define BONESURFACEMOTIONSIMULATOR_H

#include <QVector3D>
#include <QElapsedTimer>
#include <random>

/**
 * @brief 骨骼表面运动模拟器
 *
 * 生成在椭球面上的螺旋运动轨迹，用于模拟探针在骨骼表面附近的运动。
 * 轨迹包含：
 * - 椭球面参数化运动
 * - 螺旋路径
 * - 高斯噪声扰动
 */
class BoneSurfaceMotionSimulator
{
public:
    BoneSurfaceMotionSimulator();

    /**
     * @brief 设置椭球参数（基于骨骼边界框）
     * @param center 椭球中心点
     * @param radii 椭球半轴 (rx, ry, rz)
     */
    void setEllipsoidParameters(const QVector3D& center, const QVector3D& radii);

    /**
     * @brief 获取当前模拟位置（跟踪空间坐标）
     * @return 当前位置
     */
    QVector3D getCurrentPosition();

    /**
     * @brief 设置运动速度参数
     * @param angularSpeed 水平角速度 (rad/s)
     * @param verticalSpeed 垂直振荡速度
     */
    void setSpeedParameters(double angularSpeed, double verticalSpeed);

    /**
     * @brief 设置噪声标准差
     * @param stdDev 高斯噪声标准差 (mm)
     */
    void setNoiseLevel(double stdDev);

    /**
     * @brief 重置模拟器（重新开始计时）
     */
    void reset();

    /**
     * @brief 暂停/恢复模拟
     */
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

private:
    QVector3D m_center;      // 椭球中心
    QVector3D m_radii;       // 椭球半轴 (rx, ry, rz)
    QElapsedTimer m_timer;   // 计时器
    std::mt19937 m_rng;      // 随机数生成器
    std::normal_distribution<double> m_noise;  // 高斯噪声

    double m_angularSpeed;   // 水平角速度 (rad/s)
    double m_verticalSpeed;  // 垂直振荡速度
    double m_noiseStdDev;    // 噪声标准差

    bool m_paused;           // 暂停状态
    qint64 m_pausedTime;     // 暂停时的时间
    qint64 m_accumulatedPauseTime;  // 累计暂停时间
};

#endif // BONESURFACEMOTIONSIMULATOR_H
