#ifndef POINT_REGISTRATION_DATA_STRUCTURES_H
#define POINT_REGISTRATION_DATA_STRUCTURES_H

/**
 * @file PointRegistrationDataStructures.h
 * @brief 基于点的配准插件数据结构定义
 *
 * 定义配准点、配准参数、配准结果等数据结构
 */

#include <QString>
#include <QVector>
#include <QVector3D>
#include <QMatrix4x4>
#include <QDateTime>
#include <QVariant>

/**
 * @brief 配准点结构
 * 表示源点和目标点对
 */
struct RegistrationPoint {
    QString name;              ///< 点名称
    QVector3D sourcePosition;  ///< 源点坐标 (CT/模型空间)
    QVector3D targetPosition;  ///< 目标点坐标 (物理/跟踪空间)
    bool hasSource;            ///< 是否设置了源点
    bool hasTarget;            ///< 是否设置了目标点
    
    RegistrationPoint()
        : hasSource(false)
        , hasTarget(false)
    {
    }
    
    bool isComplete() const {
        return hasSource && hasTarget;
    }
};

/**
 * @brief 变换模式枚举
 */
enum class TransformMode {
    RigidBody,   ///< 刚体变换 (6自由度: 旋转+平移)
    Similarity,  ///< 相似性变换 (7自由度: 旋转+平移+均匀缩放)
    Affine       ///< 仿射变换 (12自由度)
};

/**
 * @brief 探针点数据来源枚举
 */
enum class ProbePointSource {
    Manual,          ///< 手动输入坐标
    Simulated,       ///< 模拟数据生成
    OpticalTracking  ///< 光学跟踪设备采集
};

/**
 * @brief 配准会话状态枚举
 */
enum class RegistrationSessionState {
    Idle,            ///< 空闲状态
    ModelLoading,    ///< 模型加载中
    PointCollection, ///< 点采集中
    Computing,       ///< 配准计算中
    Completed,       ///< 配准完成
    Failed           ///< 配准失败
};

/**
 * @brief 配准参数结构
 */
struct PointRegistrationParameters {
    TransformMode transformMode;   ///< 变换模式
    double convergenceThreshold;   ///< 收敛阈值 (mm)
    int maxIterations;             ///< 最大迭代次数
    
    PointRegistrationParameters()
        : transformMode(TransformMode::RigidBody)
        , convergenceThreshold(0.001)
        , maxIterations(100)
    {
    }
};

/**
 * @brief 配准结果结构
 */
struct PointRegistrationResult {
    bool success;                  ///< 配准是否成功
    QString errorMessage;          ///< 错误信息
    
    // 变换参数
    QMatrix4x4 transformMatrix;    ///< 4x4变换矩阵
    double translationX;           ///< X方向平移 (mm)
    double translationY;           ///< Y方向平移 (mm)
    double translationZ;           ///< Z方向平移 (mm)
    double rotationX;              ///< X轴旋转角度 (度)
    double rotationY;              ///< Y轴旋转角度 (度)
    double rotationZ;              ///< Z轴旋转角度 (度)
    double scale;                  ///< 缩放因子 (仅相似性变换)
    
    // 精度指标
    double rmsError;               ///< RMS误差 (mm)
    double maxError;               ///< 最大误差 (mm)
    double meanError;              ///< 平均误差 (mm)
    QVector<double> pointErrors;   ///< 每个点的误差
    
    // 统计信息
    int pointCount;                ///< 有效点对数量
    QDateTime timestamp;           ///< 配准时间戳
    double durationMs;             ///< 配准耗时 (毫秒)
    
    PointRegistrationResult()
        : success(false)
        , translationX(0.0), translationY(0.0), translationZ(0.0)
        , rotationX(0.0), rotationY(0.0), rotationZ(0.0)
        , scale(1.0)
        , rmsError(0.0), maxError(0.0), meanError(0.0)
        , pointCount(0), durationMs(0.0)
    {
        transformMatrix.setToIdentity();
        timestamp = QDateTime::currentDateTime();
    }
};

/**
 * @brief 配准会话信息结构
 */
struct RegistrationSession {
    QString sessionId;                  ///< 会话ID
    QString patientId;                  ///< 患者ID
    QString modelSource;                ///< 模型来源（分割任务ID或文件路径）
    RegistrationSessionState state;     ///< 当前状态
    ProbePointSource probeSource;       ///< 探针数据来源
    QString trackingSessionId;          ///< 跟踪会话ID（如果使用跟踪）
    QString probeToolId;                ///< 探针工具ID
    QMatrix4x4 registrationMatrix;      ///< 配准变换矩阵
    PointRegistrationResult result;     ///< 配准结果
    QDateTime createdAt;                ///< 创建时间
    QDateTime completedAt;              ///< 完成时间

    RegistrationSession()
        : state(RegistrationSessionState::Idle)
        , probeSource(ProbePointSource::Manual)
    {
        createdAt = QDateTime::currentDateTime();
        registrationMatrix.setToIdentity();
    }

    bool isActive() const {
        return state == RegistrationSessionState::ModelLoading ||
               state == RegistrationSessionState::PointCollection ||
               state == RegistrationSessionState::Computing;
    }

    bool isCompleted() const {
        return state == RegistrationSessionState::Completed;
    }
};

// 声明为Qt元类型，支持跨线程信号槽传递
Q_DECLARE_METATYPE(RegistrationPoint)
Q_DECLARE_METATYPE(PointRegistrationParameters)
Q_DECLARE_METATYPE(PointRegistrationResult)
Q_DECLARE_METATYPE(ProbePointSource)
Q_DECLARE_METATYPE(RegistrationSessionState)
Q_DECLARE_METATYPE(RegistrationSession)

/**
 * @brief 获取变换模式的字符串描述
 */
inline QString transformModeToString(TransformMode mode) {
    switch (mode) {
        case TransformMode::RigidBody: return "刚体变换";
        case TransformMode::Similarity: return "相似性变换";
        case TransformMode::Affine: return "仿射变换";
        default: return "未知";
    }
}

/**
 * @brief 获取探针数据来源的字符串描述
 */
inline QString probeSourceToString(ProbePointSource source) {
    switch (source) {
        case ProbePointSource::Manual: return "手动输入";
        case ProbePointSource::Simulated: return "模拟数据";
        case ProbePointSource::OpticalTracking: return "光学跟踪";
        default: return "未知";
    }
}

/**
 * @brief 获取配准会话状态的字符串描述
 */
inline QString sessionStateToString(RegistrationSessionState state) {
    switch (state) {
        case RegistrationSessionState::Idle: return "空闲";
        case RegistrationSessionState::ModelLoading: return "模型加载中";
        case RegistrationSessionState::PointCollection: return "点采集中";
        case RegistrationSessionState::Computing: return "配准计算中";
        case RegistrationSessionState::Completed: return "已完成";
        case RegistrationSessionState::Failed: return "失败";
        default: return "未知";
    }
}

#endif // POINT_REGISTRATION_DATA_STRUCTURES_H

