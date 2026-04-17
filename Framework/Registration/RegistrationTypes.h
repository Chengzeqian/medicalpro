#ifndef REGISTRATION_TYPES_H
#define REGISTRATION_TYPES_H

#include <QList>
#include <QMatrix4x4>
#include <QString>
#include <QVector3D>

enum class RegistrationTransformType
{
    Translation,
    Rigid,
    Similarity,
    Affine
};

struct RegistrationRequest
{
    QList<QVector3D> fixedLandmarks;
    QList<QVector3D> movingLandmarks;
    RegistrationTransformType transformType = RegistrationTransformType::Rigid;

    bool isValid() const
    {
        return !fixedLandmarks.isEmpty() && fixedLandmarks.size() == movingLandmarks.size();
    }
};

struct RegistrationResult
{
    bool success = false;
    QString message;
    QMatrix4x4 transform;
    double rmsError = 0.0;
};

inline RegistrationResult makeErrorResult(const QString& message)
{
    RegistrationResult result;
    result.success = false;
    result.message = message;
    result.transform.setToIdentity();
    result.rmsError = -1.0;
    return result;
}

/**
 * @brief 影像强度配准请求，面向 BRAINSFit 等外部 CLI
 */
struct ImageRegistrationRequest
{
    QString fixedVolumePath;             ///< 固定影像路径
    QString movingVolumePath;            ///< 浮动影像路径
    QString outputTransformPath;         ///< 输出变换路径（必填，.h5/.tfm）
    QString outputResampledVolumePath;   ///< 可选，输出重采样后的浮动影像
    QString maskVolumePath;              ///< 可选，掩膜
    QString initializedTransformPath;    ///< 可选，初始变换
    QStringList extraOptions;            ///< 额外 CLI 参数

    bool isValid() const
    {
        return !fixedVolumePath.isEmpty() && !movingVolumePath.isEmpty() && !outputTransformPath.isEmpty();
    }
};

/**
 * @brief 外部配准任务执行结果（例如 BRAINSFit）
 */
struct RegistrationProcessResult
{
    bool success = false;
    int exitCode = -1;
    QString message;
    QString standardOutput;
    QString standardError;
};

#endif // REGISTRATION_TYPES_H
