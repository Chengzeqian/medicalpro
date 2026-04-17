#include "BoneSurfaceMotionSimulator.h"
#include <QtMath>

BoneSurfaceMotionSimulator::BoneSurfaceMotionSimulator()
    : m_center(0, 0, 0)
    , m_radii(50, 50, 100)  // 默认椭球尺寸
    , m_noise(0.0, 2.0)     // 2mm标准差噪声
    , m_angularSpeed(0.5)   // 0.5 rad/s，约12秒转一圈
    , m_verticalSpeed(0.2)  // 垂直振荡速度
    , m_noiseStdDev(2.0)
    , m_paused(false)
    , m_pausedTime(0)
    , m_accumulatedPauseTime(0)
{
    // 使用随机设备初始化随机数生成器
    m_rng.seed(std::random_device{}());
    m_timer.start();
}

void BoneSurfaceMotionSimulator::setEllipsoidParameters(
    const QVector3D& center, const QVector3D& radii)
{
    m_center = center;
    m_radii = radii;
}

void BoneSurfaceMotionSimulator::setSpeedParameters(double angularSpeed, double verticalSpeed)
{
    m_angularSpeed = angularSpeed;
    m_verticalSpeed = verticalSpeed;
}

void BoneSurfaceMotionSimulator::setNoiseLevel(double stdDev)
{
    m_noiseStdDev = stdDev;
    m_noise = std::normal_distribution<double>(0.0, stdDev);
}

QVector3D BoneSurfaceMotionSimulator::getCurrentPosition()
{
    if (m_paused) {
        // 暂停时返回最后的位置（使用暂停时的时间计算）
        double t = (m_pausedTime - m_accumulatedPauseTime) / 1000.0;

        double theta = m_angularSpeed * t;
        double phi = qSin(m_verticalSpeed * t) * M_PI / 3 + M_PI / 2;

        double x = m_radii.x() * qSin(phi) * qCos(theta);
        double y = m_radii.y() * qSin(phi) * qSin(theta);
        double z = m_radii.z() * qCos(phi);

        return m_center + QVector3D(x, y, z);
    }

    // 获取有效时间（减去累计暂停时间）
    double t = (m_timer.elapsed() - m_accumulatedPauseTime) / 1000.0;  // 转换为秒

    // 螺旋参数
    // theta: 水平角，随时间线性增加，形成圆周运动
    double theta = m_angularSpeed * t;

    // phi: 垂直角，使用正弦函数产生上下振荡
    // 范围: [PI/2 - PI/3, PI/2 + PI/3] = [60°, 120°]
    // 这样探针主要在椭球的"赤道"附近运动
    double phi = qSin(m_verticalSpeed * t) * M_PI / 3 + M_PI / 2;

    // 椭球面参数方程
    // x = rx * sin(phi) * cos(theta)
    // y = ry * sin(phi) * sin(theta)
    // z = rz * cos(phi)
    double x = m_radii.x() * qSin(phi) * qCos(theta);
    double y = m_radii.y() * qSin(phi) * qSin(theta);
    double z = m_radii.z() * qCos(phi);

    // 添加高斯噪声，模拟真实跟踪器的测量误差
    if (m_noiseStdDev > 0) {
        x += m_noise(m_rng);
        y += m_noise(m_rng);
        z += m_noise(m_rng);
    }

    return m_center + QVector3D(static_cast<float>(x),
                                 static_cast<float>(y),
                                 static_cast<float>(z));
}

void BoneSurfaceMotionSimulator::reset()
{
    m_timer.restart();
    m_paused = false;
    m_pausedTime = 0;
    m_accumulatedPauseTime = 0;
}

void BoneSurfaceMotionSimulator::setPaused(bool paused)
{
    if (paused == m_paused) {
        return;
    }

    if (paused) {
        // 开始暂停，记录当前时间
        m_pausedTime = m_timer.elapsed();
    } else {
        // 恢复运动，累加暂停时间
        m_accumulatedPauseTime += m_timer.elapsed() - m_pausedTime;
    }

    m_paused = paused;
}
