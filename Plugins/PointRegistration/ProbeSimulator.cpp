#include "ProbeSimulator.h"

#include <QtMath>
#include <QDebug>

ProbeSimulator::ProbeSimulator(QObject* parent)
    : QObject(parent)
    , m_noiseLevel(0.5)
    , m_noiseModel(NoiseModel::Gaussian)
    , m_randomGenerator(QRandomGenerator::global()->generate())
    , m_trajectoryActive(false)
    , m_outlierProbability(0.02)
    , m_outlierMultiplier(5.0)
    , m_directionalBias(1.0, 1.0, 1.2)
{
    m_transformMatrix.setToIdentity();
    m_inverseTransform.setToIdentity();
}

// ========== 变换矩阵设置 ==========

void ProbeSimulator::setTransformMatrix(const QMatrix4x4& transform)
{
    m_transformMatrix = transform;

    bool invertible = false;
    m_inverseTransform = transform.inverted(&invertible);

    if (!invertible) {
        qWarning() << "ProbeSimulator: Transform matrix is not invertible, using identity";
        m_inverseTransform.setToIdentity();
    }
}

QMatrix4x4 ProbeSimulator::getTransformMatrix() const
{
    return m_transformMatrix;
}

void ProbeSimulator::setDefaultTransform()
{
    m_transformMatrix.setToIdentity();

    // 绕 Z 轴旋转 15°
    m_transformMatrix.rotate(15.0f, 0.0f, 0.0f, 1.0f);

    // 平移 (50, 30, -20) mm
    m_transformMatrix.translate(50.0f, 30.0f, -20.0f);

    bool invertible = false;
    m_inverseTransform = m_transformMatrix.inverted(&invertible);

    if (!invertible) {
        qWarning() << "ProbeSimulator: Default transform matrix is not invertible";
        m_inverseTransform.setToIdentity();
    }

    qDebug() << "ProbeSimulator: Default transform set (rotate Z 15°, translate (50, 30, -20))";
}

void ProbeSimulator::resetTransform()
{
    m_transformMatrix.setToIdentity();
    m_inverseTransform.setToIdentity();
}

// ========== 噪声设置 ==========

void ProbeSimulator::setNoiseLevel(double level)
{
    m_noiseLevel = qMax(0.0, level);
}

double ProbeSimulator::getNoiseLevel() const
{
    return m_noiseLevel;
}

void ProbeSimulator::setNoiseModel(NoiseModel model)
{
    m_noiseModel = model;
}

NoiseModel ProbeSimulator::getNoiseModel() const
{
    return m_noiseModel;
}

// ========== 点生成 ==========

QVector3D ProbeSimulator::generateProbePoint(const QVector3D& ctPoint)
{
    // 应用逆变换：probePoint = T^(-1) * ctPoint
    QVector3D transformedPoint = m_inverseTransform.map(ctPoint);

    // 添加噪声
    QVector3D noise;
    switch (m_noiseModel) {
        case NoiseModel::Gaussian:
            noise = generateGaussianNoise();
            break;
        case NoiseModel::Uniform:
            noise = generateUniformNoise();
            break;
        case NoiseModel::Clinical:
            noise = generateClinicalNoise();
            break;
    }

    return transformedPoint + noise;
}

QVector<QVector3D> ProbeSimulator::generateProbePoints(const QVector<QVector3D>& ctPoints)
{
    QVector<QVector3D> probePoints;
    probePoints.reserve(ctPoints.size());

    for (const auto& ctPoint : ctPoints) {
        probePoints.append(generateProbePoint(ctPoint));
    }

    return probePoints;
}

QVector3D ProbeSimulator::generateIdealProbePoint(const QVector3D& ctPoint)
{
    return m_inverseTransform.map(ctPoint);
}

// ========== 轨迹模拟 ==========

QVector3D ProbeSimulator::simulateProbeApproach(const QVector3D& targetPosition,
                                                  double approachSpeed,
                                                  double deltaTime)
{
    if (!m_trajectoryActive) {
        m_trajectoryActive = true;
        if (m_currentPosition.isNull()) {
            // 从目标点上方 100mm 处开始
            m_currentPosition = targetPosition + QVector3D(0, 0, 100);
        }
    }

    // 计算到目标的方向和距离
    QVector3D direction = targetPosition - m_currentPosition;
    double distance = direction.length();

    if (distance < 0.1) {
        // 已到达目标，添加噪声
        QVector3D finalPosition = targetPosition + generateGaussianNoise();
        emit targetReached(finalPosition);
        m_trajectoryActive = false;
        return finalPosition;
    }

    // 归一化方向
    direction.normalize();

    // 计算移动距离
    double moveDistance = qMin(approachSpeed * deltaTime, distance);

    // 更新位置
    m_currentPosition += direction * static_cast<float>(moveDistance);

    // 添加轻微抖动噪声（模拟手抖）
    QVector3D jitter = generateGaussianNoise() * 0.1f;
    QVector3D displayPosition = m_currentPosition + jitter;

    emit trajectoryPositionUpdated(displayPosition, distance - moveDistance);

    return displayPosition;
}

void ProbeSimulator::resetTrajectory()
{
    m_trajectoryActive = false;
    m_currentPosition = QVector3D();
}

void ProbeSimulator::setTrajectoryStartPosition(const QVector3D& position)
{
    m_currentPosition = position;
}

// ========== 随机种子 ==========

void ProbeSimulator::setSeed(quint32 seed)
{
    m_randomGenerator.seed(seed);
}

void ProbeSimulator::resetSeed()
{
    m_randomGenerator.seed(QRandomGenerator::global()->generate());
}

// ========== 统计信息 ==========

QString ProbeSimulator::noiseModelToString(NoiseModel model)
{
    switch (model) {
        case NoiseModel::Gaussian:
            return QString::fromUtf8("高斯噪声");
        case NoiseModel::Uniform:
            return QString::fromUtf8("均匀噪声");
        case NoiseModel::Clinical:
            return QString::fromUtf8("临床噪声");
        default:
            return QString::fromUtf8("未知");
    }
}

double ProbeSimulator::getRecommendedNoiseLevel(const QString& condition)
{
    if (condition == "ideal" || condition == "low") {
        return 0.3;
    } else if (condition == "typical" || condition == "medium") {
        return 0.5;
    } else if (condition == "difficult" || condition == "high") {
        return 1.0;
    }
    return 0.5; // 默认中等
}

// ========== 私有方法 ==========

QVector3D ProbeSimulator::generateGaussianNoise()
{
    return QVector3D(
        static_cast<float>(generateStandardNormal() * m_noiseLevel),
        static_cast<float>(generateStandardNormal() * m_noiseLevel),
        static_cast<float>(generateStandardNormal() * m_noiseLevel)
    );
}

QVector3D ProbeSimulator::generateUniformNoise()
{
    // 均匀分布在 [-noiseLevel, noiseLevel] 范围内
    double range = m_noiseLevel * 2.0;
    return QVector3D(
        static_cast<float>(m_randomGenerator.generateDouble() * range - m_noiseLevel),
        static_cast<float>(m_randomGenerator.generateDouble() * range - m_noiseLevel),
        static_cast<float>(m_randomGenerator.generateDouble() * range - m_noiseLevel)
    );
}

QVector3D ProbeSimulator::generateClinicalNoise()
{
    // 基础高斯噪声
    QVector3D noise = generateGaussianNoise();

    // 应用方向偏置（某些方向误差更大，例如 Z 方向）
    noise.setX(noise.x() * static_cast<float>(m_directionalBias.x()));
    noise.setY(noise.y() * static_cast<float>(m_directionalBias.y()));
    noise.setZ(noise.z() * static_cast<float>(m_directionalBias.z()));

    // 偶尔的离群点（模拟遮挡、反射等）
    if (m_randomGenerator.generateDouble() < m_outlierProbability) {
        noise *= static_cast<float>(m_outlierMultiplier);
        qDebug() << "ProbeSimulator: Outlier noise generated";
    }

    return noise;
}

double ProbeSimulator::generateStandardNormal()
{
    // Box-Muller 变换生成标准正态分布
    double u1 = m_randomGenerator.generateDouble();
    double u2 = m_randomGenerator.generateDouble();

    // 避免 log(0)
    while (u1 <= 1e-10) {
        u1 = m_randomGenerator.generateDouble();
    }

    double z0 = qSqrt(-2.0 * qLn(u1)) * qCos(2.0 * M_PI * u2);
    return z0;
}
