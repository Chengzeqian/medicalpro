#ifndef SEGMENTATION_SERVICE_IMPL_H
#define SEGMENTATION_SERVICE_IMPL_H

#include "SegmentationService.h"
#include <QObject>
#include <QProcess>
#include <QHash>
#include <QMutex>
#include <QTimer>
#include <vtkSmartPointer.h>

class vtkPolyData;
class vtkImageData;
class vtkNIFTIImageReader;
class vtkMarchingCubes;
class vtkSmoothPolyDataFilter;

/**
 * @brief 任务状态
 */
struct SegmentationTask {
    QString taskId;
    QString taskName;
    QString inputPath;
    QString outputDir;
    QString logFilePath;
    QString bodyPart;
    QString status; // "pending", "running", "completed", "failed", "cancelled"
    int progress;
    QString errorMessage;
    QProcess* process;
    QProcess* consoleProcess;
    QVariantMap result;
    qint64 startTime;
    qint64 endTime;

    SegmentationTask()
        : progress(0), process(nullptr), consoleProcess(nullptr), startTime(0), endTime(0)
    {}
};

/**
 * @brief 骨骼分割服务实现
 *
 * 实现细节：
 * 1. 使用 QProcess 调用 TotalSegmentator
 * 2. 使用 vtkNIFTIImageReader 读取 NIfTI 文件
 * 3. 使用 vtkMarchingCubes 生成 Mesh
 * 4. 异步任务管理，不阻塞主线程
 */
class SegmentationServiceImpl : public SegmentationService
{
    Q_OBJECT
    Q_INTERFACES(SegmentationService)

public:
    explicit SegmentationServiceImpl(QObject* parent = nullptr);
    virtual ~SegmentationServiceImpl();

    // ==================== SegmentationService 接口实现 ====================

    QString runBoneSegmentation(const QString& inputPath,
                               const QString& outputDir = QString(),
                               const QString& taskName = QString()) override;

    QString runSegmentation(const QString& inputPath,
                           const QString& bodyPart,
                           const QString& outputDir = QString(),
                           const QString& taskName = QString()) override;

    bool cancelTask(const QString& taskId) override;
    QString getTaskStatus(const QString& taskId) const override;
    int getTaskProgress(const QString& taskId) const override;
    QStringList getActiveTasks() const override;
    QVariantMap getTaskInfo(const QString& taskId) const override;

    vtkSmartPointer<vtkPolyData> getSegmentationMesh(const QString& taskId,
                                                     const QString& bodyPart = QString()) override;

    vtkSmartPointer<vtkImageData> getSegmentationMask(const QString& taskId) override;

    QStringList getSegmentationFiles(const QString& taskId,
                                    const QString& format = "stl") override;

    bool exportSegmentation(const QString& taskId,
                           const QString& exportPath,
                           const QString& format = "stl") override;

    bool setPythonEnvironment(const QString& pythonPath) override;
    bool checkPythonEnvironment() override;
    bool setSegmentationParameters(const QVariantMap& parameters) override;
    QVariantMap getSegmentationParameters() const override;
    QStringList getSupportedBodyParts() const override;

    bool convertDicomToNifti(const QString& dicomPath, const QString& outputPath) override;

    vtkSmartPointer<vtkPolyData> convertMaskToMesh(const QString& niftiPath,
                                                   double threshold = 0.5) override;

    vtkSmartPointer<vtkPolyData> convertNiftiToMeshAuto(const QString& niftiPath) override;

    bool cleanupTempFiles(const QString& taskId = QString()) override;
    QString getLastError() const override;

private slots:
    /**
     * @brief QProcess 输出处理
     */
    void handleProcessOutput();

    /**
     * @brief QProcess 错误输出处理
     */
    void handleProcessError();

    /**
     * @brief QProcess 完成处理
     */
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    /**
     * @brief 进度监控定时器
     */
    void monitorProgress();

private:
    /**
     * @brief 生成唯一任务ID
     */
    QString generateTaskId();

    /**
     * @brief 创建临时输出目录
     */
    QString createTempOutputDir();

    /**
     * @brief 构建 TotalSegmentator 命令
     */
    QStringList buildSegmentationCommand(const SegmentationTask& task);

    /**
     * @brief 启动分割进程（线程安全包装）
     *
     * 如果从非主线程调用，会自动将实际创建过程调度到主线程执行
     */
    bool startSegmentationProcess(SegmentationTask& task);

    /**
     * @brief 启动分割进程（内部实现）
     *
     * 必须在主线程中调用，创建 QProcess 并启动分割任务
     */
    bool startSegmentationProcessInternal(SegmentationTask& task);

    /**
     * @brief 解析输出文件
     */
    bool parseSegmentationOutput(SegmentationTask& task);

    /**
     * @brief 加载 NIfTI Mask 为 vtkImageData
     */
    vtkSmartPointer<vtkImageData> loadNiftiMask(const QString& filePath);

    /**
     * @brief 生成 Mesh from ImageData
     */
    vtkSmartPointer<vtkPolyData> generateMeshFromMask(vtkImageData* mask, double threshold);

    vtkSmartPointer<vtkPolyData> generateMeshFromIntensity(vtkImageData* volume, double isoValue);

    bool isLikelyLabelMap(vtkImageData* image, const double range[2]) const;

    /**
     * @brief 查找任务
     */
    SegmentationTask* findTask(const QString& taskId);
    const SegmentationTask* findTask(const QString& taskId) const;

    /**
     * @brief 查找任务（通过 QProcess）
     */
    QString findTaskIdByProcess(QProcess* process);

    /**
     * @brief 更新任务进度
     */
    void updateTaskProgress(const QString& taskId, int progress, const QString& message);

    /**
     * @brief 完成任务
     */
    void completeTask(const QString& taskId, const QVariantMap& result);

    /**
     * @brief 任务失败
     */
    void failTask(const QString& taskId, const QString& error);

    /**
     * @brief 加载配置文件
     */
    void loadConfiguration();

    /**
     * @brief 保存配置文件
     * @return 成功返回 true
     */
    bool saveConfiguration();

    /**
     * @brief 获取配置文件路径
     */
    QString getConfigFilePath() const;

    /**
     * @brief 自动检测 Python 环境
     */
    void autoDetectPythonEnvironment();

private:
    mutable QMutex m_mutex;
    QHash<QString, SegmentationTask> m_tasks;
    QTimer* m_progressTimer;

    QString m_pythonPath;
    QString m_totalSegmentatorPath;
    QVariantMap m_parameters;
    QString m_lastError;

    // 缓存的分割结果
    QHash<QString, vtkSmartPointer<vtkPolyData>> m_meshCache;
    QHash<QString, vtkSmartPointer<vtkImageData>> m_maskCache;
};

#endif // SEGMENTATION_SERVICE_IMPL_H
