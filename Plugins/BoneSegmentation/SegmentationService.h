#ifndef SEGMENTATION_SERVICE_H
#define SEGMENTATION_SERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>
#include <QMetaType>
#include <vtkSmartPointer.h>

// 前向声明
class vtkPolyData;
class vtkImageData;

/**
 * @brief 骨骼分割服务接口 (Segmentation Service Interface)
 *
 * 提供医学影像骨骼分割功能，支持：
 * - 集成 TotalSegmentator (Python AI)
 * - DICOM/NIfTI 输入
 * - 多种输出格式 (STL, VTK PolyData)
 * - 异步处理（不阻塞主线程）
 * - 进度回调
 *
 * 设计原则：
 * 1. 进程隔离：Python 环境通过 QProcess 调用
 * 2. 异步优先：所有耗时操作返回任务ID，通过信号通知完成
 * 3. 标准接口：输入路径，输出 VTK 数据结构
 */
class SegmentationService : public QObject
{
    Q_OBJECT

public:
    explicit SegmentationService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~SegmentationService() = default;

    // ==================== 分割任务管理 ====================

    /**
     * @brief 运行骨骼分割（异步）
     * @param inputPath DICOM 文件夹路径或 NIfTI 文件路径
     * @param outputDir 输出目录（可选，默认临时目录）
     * @param taskName 任务名称（可选）
     * @return 任务ID，失败返回空字符串
     *
     * 使用示例：
     * @code
     * QString taskId = segService->runBoneSegmentation("D:/data/dicom_series");
     * // 监听信号 segmentationCompleted(taskId, result)
     * @endcode
     */
    virtual QString runBoneSegmentation(const QString& inputPath,
                                       const QString& outputDir = QString(),
                                       const QString& taskName = QString()) = 0;

    /**
     * @brief 运行指定部位分割
     * @param inputPath 输入路径
     * @param bodyPart 身体部位 ("femur", "tibia", "pelvis", "all")
     * @param outputDir 输出目录
     * @param taskName 任务名称
     * @return 任务ID
     */
    virtual QString runSegmentation(const QString& inputPath,
                                   const QString& bodyPart,
                                   const QString& outputDir = QString(),
                                   const QString& taskName = QString()) = 0;

    /**
     * @brief 取消正在运行的任务
     * @param taskId 任务ID
     * @return 成功返回 true
     */
    virtual bool cancelTask(const QString& taskId) = 0;

    /**
     * @brief 获取任务状态
     * @param taskId 任务ID
     * @return 状态字符串 ("pending", "running", "completed", "failed", "cancelled")
     */
    virtual QString getTaskStatus(const QString& taskId) const = 0;

    /**
     * @brief 获取任务进度
     * @param taskId 任务ID
     * @return 进度百分比 (0-100)，失败返回 -1
     */
    virtual int getTaskProgress(const QString& taskId) const = 0;

    /**
     * @brief 获取所有活动任务
     * @return 任务ID列表
     */
    virtual QStringList getActiveTasks() const = 0;

    /**
     * @brief 获取任务信息
     * @param taskId 任务ID
     * @return 任务信息映射
     */
    virtual QVariantMap getTaskInfo(const QString& taskId) const = 0;

    // ==================== 结果获取 ====================

    /**
     * @brief 获取分割结果（VTK PolyData）
     * @param taskId 任务ID
     * @param bodyPart 身体部位（可选，默认返回所有）
     * @return VTK PolyData 智能指针，失败返回 nullptr
     *
     * 注意：返回的是智能指针，自动管理内存
     */
    virtual vtkSmartPointer<vtkPolyData> getSegmentationMesh(const QString& taskId,
                                                             const QString& bodyPart = QString()) = 0;

    /**
     * @brief 获取分割结果（Image Data）
     * @param taskId 任务ID
     * @return VTK ImageData 智能指针
     */
    virtual vtkSmartPointer<vtkImageData> getSegmentationMask(const QString& taskId) = 0;

    /**
     * @brief 获取分割结果文件路径
     * @param taskId 任务ID
     * @param format 格式 ("stl", "nii", "vtk")
     * @return 文件路径列表
     */
    virtual QStringList getSegmentationFiles(const QString& taskId,
                                            const QString& format = "stl") = 0;

    /**
     * @brief 导出分割结果
     * @param taskId 任务ID
     * @param exportPath 导出路径
     * @param format 导出格式 ("stl", "obj", "ply", "vtk")
     * @return 成功返回 true
     */
    virtual bool exportSegmentation(const QString& taskId,
                                   const QString& exportPath,
                                   const QString& format = "stl") = 0;

    // ==================== 配置管理 ====================

    /**
     * @brief 设置 Python 环境路径
     * @param pythonPath Python 解释器路径
     * @return 成功返回 true
     */
    virtual bool setPythonEnvironment(const QString& pythonPath) = 0;

    /**
     * @brief 检查 Python 环境
     * @return 环境是否可用
     */
    virtual bool checkPythonEnvironment() = 0;

    /**
     * @brief 设置 TotalSegmentator 参数
     * @param parameters 参数映射
     * @return 成功返回 true
     */
    virtual bool setSegmentationParameters(const QVariantMap& parameters) = 0;

    /**
     * @brief 获取当前参数
     * @return 参数映射
     */
    virtual QVariantMap getSegmentationParameters() const = 0;

    /**
     * @brief 获取支持的身体部位列表
     * @return 身体部位列表
     */
    virtual QStringList getSupportedBodyParts() const = 0;

    // ==================== 辅助功能 ====================

    /**
     * @brief 转换 DICOM 系列到 NIfTI
     * @param dicomPath DICOM 文件夹路径
     * @param outputPath 输出 NIfTI 路径
     * @return 成功返回 true
     */
    virtual bool convertDicomToNifti(const QString& dicomPath, const QString& outputPath) = 0;

    /**
     * @brief 转换 NIfTI Mask 到 VTK Mesh
     * @param niftiPath NIfTI 文件路径
     * @param threshold 阈值
     * @return VTK PolyData
     */
    virtual vtkSmartPointer<vtkPolyData> convertMaskToMesh(const QString& niftiPath,
                                                           double threshold = 0.5) = 0;

    /**
     * @brief 自动判断 NIfTI 类型并生成 Mesh
     *
     * - 若为标签图（label map，值为 0..N），按“非零为前景”生成表面
     * - 若为强度图（例如 CT HU，或掩码后的 HU 强度图），按 HU 阈值生成表面
     *
     * HU 阈值从参数 `hu_threshold` 读取（默认 200）。
     */
    virtual vtkSmartPointer<vtkPolyData> convertNiftiToMeshAuto(const QString& niftiPath) = 0;

    /**
     * @brief 清理临时文件
     * @param taskId 任务ID（可选，默认清理所有）
     * @return 成功返回 true
     */
    virtual bool cleanupTempFiles(const QString& taskId = QString()) = 0;

    /**
     * @brief 获取最后错误信息
     * @return 错误信息字符串
     */
    virtual QString getLastError() const = 0;

signals:
    /**
     * @brief 分割任务开始信号
     * @param taskId 任务ID
     * @param taskName 任务名称
     */
    void segmentationStarted(const QString& taskId, const QString& taskName);

    /**
     * @brief 分割进度更新信号
     * @param taskId 任务ID
     * @param progress 进度百分比 (0-100)
     * @param message 进度消息
     */
    void segmentationProgress(const QString& taskId, int progress, const QString& message);

    /**
     * @brief 分割任务完成信号
     * @param taskId 任务ID
     * @param result 结果信息映射
     */
    void segmentationCompleted(const QString& taskId, const QVariantMap& result);

    /**
     * @brief 分割任务失败信号
     * @param taskId 任务ID
     * @param error 错误信息
     */
    void segmentationFailed(const QString& taskId, const QString& error);

    /**
     * @brief 分割任务取消信号
     * @param taskId 任务ID
     */
    void segmentationCancelled(const QString& taskId);

    /**
     * @brief Python 环境状态变化信号
     * @param available 是否可用
     * @param message 状态消息
     */
    void pythonEnvironmentChanged(bool available, const QString& message);
};

// Qt 接口声明
Q_DECLARE_INTERFACE(SegmentationService, "medical.SegmentationService")

#endif // SEGMENTATION_SERVICE_H
