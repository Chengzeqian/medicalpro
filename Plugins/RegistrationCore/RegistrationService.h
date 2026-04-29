#ifndef REGISTRATION_SERVICE_H
#define REGISTRATION_SERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>
#include <QMetaType>
#include <vtkSmartPointer.h>

// 前向声明
class vtkMatrix4x4;
class vtkTransform;
class vtkPoints;
class vtkPolyData;

/**
 * @brief 配准核心服务接口 (Registration Core Service Interface)
 *
 * 提供医学影像配准算法，支持：
 * - Landmark 配准（点对点）
 * - ICP 配准（表面配准）
 * - 配准质量评估
 * - 变换矩阵管理
 *
 * 设计原则：
 * 1. 纯算法库：不含 UI，只封装 VTK 配准算法
 * 2. 线程安全：算法可在后台线程执行
 * 3. 标准接口：输入 VTK 数据结构，输出变换矩阵
 *
 * 核心公式：
 * T_CT_to_Tracker = computeRegistration(SourcePoints_Tracker, TargetPoints_CT)
 * ToolWorld = T_CT_to_Tracker^(-1) * ToolTracker
 */
namespace registration_core {

class RegistrationService : public QObject
{
    Q_OBJECT

public:
    explicit RegistrationService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~RegistrationService() = default;

    // ==================== Landmark 配准（点对点配准） ====================

    /**
     * @brief 执行 Landmark 配准
     * @param sourcePoints 源点集（探针采集的点，Tracker 坐标系）
     * @param targetPoints 目标点集（CT 上选的点，CT 坐标系）
     * @param registrationId 配准任务ID（可选）
     * @return 4x4 变换矩阵，失败返回 nullptr
     *
     * 算法：使用 vtkLandmarkTransform 计算 Rigid 变换
     * 要求：sourcePoints 和 targetPoints 数量必须相同（至少 3 个点）
     *
     * 使用示例：
     * @code
     * vtkSmartPointer<vtkPoints> sourcePoints = vtkSmartPointer<vtkPoints>::New();
     * vtkSmartPointer<vtkPoints> targetPoints = vtkSmartPointer<vtkPoints>::New();
     * // 添加点对...
     * vtkSmartPointer<vtkMatrix4x4> transform =
     *     registrationService->performLandmarkRegistration(sourcePoints, targetPoints);
     * @endcode
     */
    virtual vtkSmartPointer<vtkMatrix4x4> performLandmarkRegistration(
        vtkPoints* sourcePoints,
        vtkPoints* targetPoints,
        const QString& registrationId = QString()) = 0;

    /**
     * @brief 执行 Landmark 配准（列表形式输入）
     * @param sourcePoints 源点集 [[x1,y1,z1], [x2,y2,z2], ...]
     * @param targetPoints 目标点集
     * @return 4x4 变换矩阵（16个元素，行优先）
     */
    virtual QList<double> performLandmarkRegistrationList(
        const QList<QList<double>>& sourcePoints,
        const QList<QList<double>>& targetPoints) = 0;

    // ==================== ICP 配准（表面配准） ====================

    /**
     * @brief 执行 ICP 配准
     * @param source 源表面（需要配准的模型）
     * @param target 目标表面（参考模型）
     * @param initialTransform 初始变换矩阵（可选，Landmark 的结果）
     * @param maxIterations 最大迭代次数（默认 100）
     * @param registrationId 配准任务ID（可选）
     * @return 4x4 变换矩阵
     *
     * 算法：使用 vtkIterativeClosestPointTransform
     * 建议：先用 Landmark 粗配准，再用 ICP 精配准
     */
    virtual vtkSmartPointer<vtkMatrix4x4> performICPRegistration(
        vtkPolyData* source,
        vtkPolyData* target,
        vtkMatrix4x4* initialTransform = nullptr,
        int maxIterations = 100,
        const QString& registrationId = QString()) = 0;

    /**
     * @brief 执行 ICP 配准（高级参数）
     * @param source 源表面
     * @param target 目标表面
     * @param parameters ICP 参数映射
     * @return 4x4 变换矩阵
     *
     * parameters 支持的键：
     * - "maxIterations": int (默认 100)
     * - "maxLandmarks": int (默认 200, 用于加速)
     * - "startByMatchingCentroids": bool (默认 true)
     * - "checkMeanDistance": bool (默认 false)
     * - "maxMeanDistance": double
     */
    virtual vtkSmartPointer<vtkMatrix4x4> performICPRegistrationAdvanced(
        vtkPolyData* source,
        vtkPolyData* target,
        const QVariantMap& parameters) = 0;

    // ==================== 配准质量评估 ====================

    /**
     * @brief 计算配准误差（RMS）
     * @param sourcePoints 源点集
     * @param targetPoints 目标点集（已变换）
     * @return RMS 误差（mm）
     */
    virtual double computeRegistrationError(vtkPoints* sourcePoints,
                                           vtkPoints* targetPoints) = 0;

    /**
     * @brief 计算配准误差（列表形式）
     * @param sourcePoints 源点集
     * @param targetPoints 目标点集
     * @param transform 变换矩阵（16个元素）
     * @return RMS 误差
     */
    virtual double computeRegistrationErrorList(
        const QList<QList<double>>& sourcePoints,
        const QList<QList<double>>& targetPoints,
        const QList<double>& transform) = 0;

    /**
     * @brief 计算 Fiducial Registration Error (FRE)
     * @param registrationId 配准任务ID
     * @return FRE 值（mm）
     */
    virtual double computeFRE(const QString& registrationId) = 0;

    /**
     * @brief 计算 Target Registration Error (TRE)
     * @param registrationId 配准任务ID
     * @param targetPoint 目标点
     * @return TRE 值（mm）
     */
    virtual double computeTRE(const QString& registrationId,
                             const QList<double>& targetPoint) = 0;

    /**
     * @brief 评估配准质量
     * @param registrationId 配准任务ID
     * @return 质量报告映射
     *
     * 返回内容：
     * - "fre": Fiducial Registration Error
     * - "tre_mean": 平均 TRE
     * - "tre_max": 最大 TRE
     * - "quality": "excellent" | "good" | "acceptable" | "poor"
     * - "recommendation": 建议信息
     */
    virtual QVariantMap evaluateRegistrationQuality(const QString& registrationId) = 0;

    // ==================== 变换矩阵操作 ====================

    /**
     * @brief 保存配准结果
     * @param registrationId 配准任务ID
     * @param transform 变换矩阵
     * @param metadata 元数据（可选）
     * @return 成功返回 true
     */
    virtual bool saveRegistrationResult(const QString& registrationId,
                                       vtkMatrix4x4* transform,
                                       const QVariantMap& metadata = QVariantMap()) = 0;

    /**
     * @brief 加载配准结果
     * @param registrationId 配准任务ID
     * @return 变换矩阵
     */
    virtual vtkSmartPointer<vtkMatrix4x4> loadRegistrationResult(const QString& registrationId) = 0;

    /**
     * @brief 获取配准结果列表
     * @return 配准ID列表
     */
    virtual QStringList getRegistrationList() const = 0;

    /**
     * @brief 获取配准信息
     * @param registrationId 配准任务ID
     * @return 配准信息映射
     */
    virtual QVariantMap getRegistrationInfo(const QString& registrationId) const = 0;

    /**
     * @brief 删除配准结果
     * @param registrationId 配准任务ID
     * @return 成功返回 true
     */
    virtual bool deleteRegistration(const QString& registrationId) = 0;

    /**
     * @brief 矩阵求逆
     * @param matrix 输入矩阵
     * @return 逆矩阵
     */
    virtual vtkSmartPointer<vtkMatrix4x4> invertMatrix(vtkMatrix4x4* matrix) = 0;

    /**
     * @brief 矩阵乘法
     * @param matrix1 矩阵1
     * @param matrix2 矩阵2
     * @return 结果矩阵 (matrix1 * matrix2)
     */
    virtual vtkSmartPointer<vtkMatrix4x4> multiplyMatrix(vtkMatrix4x4* matrix1,
                                                         vtkMatrix4x4* matrix2) = 0;

    /**
     * @brief 转换点
     * @param point 输入点 [x, y, z]
     * @param transform 变换矩阵
     * @return 输出点 [x, y, z]
     */
    virtual QList<double> transformPoint(const QList<double>& point,
                                        vtkMatrix4x4* transform) = 0;

    /**
     * @brief 转换点集
     * @param points 输入点集
     * @param transform 变换矩阵
     * @return 输出点集
     */
    virtual vtkSmartPointer<vtkPoints> transformPoints(vtkPoints* points,
                                                       vtkMatrix4x4* transform) = 0;

    // ==================== 2D-3D 配准支持 ====================

    /**
     * @brief 执行 2D-3D 配准
     * @param image2D 2D 图像路径（X-ray）
     * @param model3D 3D 模型
     * @param initialTransform 初始变换
     * @param parameters 配准参数
     * @return 4x4 变换矩阵
     *
     * 注意：这是高级功能，需要依赖 Registration2D3D 插件
     */
    virtual vtkSmartPointer<vtkMatrix4x4> perform2D3DRegistration(
        const QString& image2D,
        vtkPolyData* model3D,
        vtkMatrix4x4* initialTransform = nullptr,
        const QVariantMap& parameters = QVariantMap()) = 0;

    // ==================== 工具方法 ====================

    /**
     * @brief 矩阵转列表
     * @param matrix VTK 矩阵
     * @return 16个元素的列表（行优先）
     */
    virtual QList<double> matrixToList(vtkMatrix4x4* matrix) = 0;

    /**
     * @brief 列表转矩阵
     * @param list 16个元素的列表
     * @return VTK 矩阵
     */
    virtual vtkSmartPointer<vtkMatrix4x4> listToMatrix(const QList<double>& list) = 0;

    /**
     * @brief 导出矩阵到文件
     * @param matrix 矩阵
     * @param filePath 文件路径
     * @param format 格式 ("txt", "json", "xml")
     * @return 成功返回 true
     */
    virtual bool exportMatrix(vtkMatrix4x4* matrix,
                             const QString& filePath,
                             const QString& format = "txt") = 0;

    /**
     * @brief 从文件导入矩阵
     * @param filePath 文件路径
     * @return 矩阵
     */
    virtual vtkSmartPointer<vtkMatrix4x4> importMatrix(const QString& filePath) = 0;

    /**
     * @brief 获取最后错误信息
     * @return 错误信息字符串
     */
    virtual QString getLastError() const = 0;

signals:
    /**
     * @brief 配准开始信号
     * @param registrationId 配准任务ID
     * @param type 配准类型
     */
    void registrationStarted(const QString& registrationId, const QString& type);

    /**
     * @brief 配准进度信号
     * @param registrationId 配准任务ID
     * @param progress 进度百分比
     * @param message 进度消息
     */
    void registrationProgress(const QString& registrationId, int progress, const QString& message);

    /**
     * @brief 配准完成信号
     * @param registrationId 配准任务ID
     * @param result 结果信息
     */
    void registrationCompleted(const QString& registrationId, const QVariantMap& result);

    /**
     * @brief 配准失败信号
     * @param registrationId 配准任务ID
     * @param error 错误信息
     */
    void registrationFailed(const QString& registrationId, const QString& error);

    /**
     * @brief 配准质量警告信号
     * @param registrationId 配准任务ID
     * @param quality 质量信息
     * @param warning 警告信息
     */
    void registrationQualityWarning(const QString& registrationId,
                                   const QVariantMap& quality,
                                   const QString& warning);
};

} // namespace registration_core

// Qt 接口声明
Q_DECLARE_INTERFACE(registration_core::RegistrationService, "medical.RegistrationService")

#endif // REGISTRATION_SERVICE_H
