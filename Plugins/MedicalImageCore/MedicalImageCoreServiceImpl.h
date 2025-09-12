#ifndef MEDICAL_IMAGE_CORE_SERVICE_IMPL_H
#define MEDICAL_IMAGE_CORE_SERVICE_IMPL_H

#include "MedicalImageCoreService.h"
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QSharedPointer>
#include <QTimer>
#include <QUuid>
#include <QVariant>
#include <QDateTime>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QThreadPool>

// CTK框架
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

// 前向声明
class MedicalImageData;
class ctkEventAdmin;

// 可选服务前向声明（遵循完全CTK架构）
class MedicalProcessingService;
class MedicalViewerService;
class PatientDatabaseService;
class DataStorageService;

/**
 * @brief Medical Image Core Service Implementation (标准CTK架构)
 * 
 * MedicalImageCoreService接口的具体实现，采用标准CTK架构设计：
 * - 所有操作基于图像ID，不直接暴露MedicalImageData对象
 * - 支持多格式图像加载和管理
 * - 提供完整的元数据和属性访问接口
 * - 异步操作和任务管理
 * - 内存管理和缓存优化
 * - 可选的外部服务集成
 */
class MedicalImageCoreServiceImpl : public MedicalImageCoreService
{
    Q_OBJECT
    Q_INTERFACES(MedicalImageCoreService)

public:
    /**
     * @brief 构造函数
     * @param context CTK插件上下文
     * @param parent 父对象
     */
    explicit MedicalImageCoreServiceImpl(ctkPluginContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    virtual ~MedicalImageCoreServiceImpl();

    /**
     * @brief 设置CTK插件上下文
     * @param context 插件上下文
     */
    void setPluginContext(ctkPluginContext* context);

    // ==================== 图像加载功能实现 ====================
    
    QStringList getSupportedFormats() const override;
    QString detectImageFormat(const QString& filePath) const override;
    QString loadImage(const QString& filePath, const QVariantMap& options = QVariantMap()) override;
    QString loadImageAsync(const QString& filePath, const QVariantMap& options = QVariantMap()) override;
    QString loadDicomSeries(const QString& seriesDirectory, const QString& seriesUID = QString(), const QVariantMap& options = QVariantMap()) override;
    QString loadDicomSeriesAsync(const QString& seriesDirectory, const QString& seriesUID = QString(), const QVariantMap& options = QVariantMap()) override;
    QString loadMultipleImages(const QStringList& filePaths, const QVariantMap& options = QVariantMap()) override;

    // ==================== 图像管理功能实现 ====================
    
    QStringList getLoadedImages() const override;
    bool hasImage(const QString& imageId) const override;
    bool isValid(const QString& imageId) const override;
    bool releaseImage(const QString& imageId) override;
    void clearAllImages() override;
    QString getImageSource(const QString& imageId) const override;
    QString duplicateImage(const QString& sourceImageId, const QString& newImageId = QString()) override;

    // ==================== 图像属性和信息查询实现 ====================
    
    QString getImageInfo(const QString& imageId) const override;
    QVariantMap getImageDetails(const QString& imageId) const override;
    QMap<QString, QVariant> getImageMetadata(const QString& imageId) const override;
    bool setImageMetadata(const QString& imageId, const QString& key, const QVariant& value) override;
    QList<int> getImageDimensions(const QString& imageId) const override;
    QList<double> getImageSpacing(const QString& imageId) const override;
    QList<double> getImageOrigin(const QString& imageId) const override;
    QList<double> getImageDirection(const QString& imageId) const override;
    QString getImageDataType(const QString& imageId) const override;
    QString getImageFormat(const QString& imageId) const override;
    bool is3D(const QString& imageId) const override;
    QMap<QString, double> getImageStatistics(const QString& imageId) const override;
    bool calculateImageStatistics(const QString& imageId, bool forceRecalculate = false) override;

    // ==================== 图像数据访问实现 ====================
    
    void* getImagePixelData(const QString& imageId) const override;
    qint64 getImageDataSize(const QString& imageId) const override;
    QVariant getPixelValue(const QString& imageId, int x, int y, int z = 0) const override;
    bool setPixelValue(const QString& imageId, int x, int y, int z, const QVariant& value) override;
    QVariantMap getImageRegion(const QString& imageId, const QVariantMap& region) const override;

    // ==================== 图像变换和坐标系统实现 ====================
    
    QVariantMap getImageTransform(const QString& imageId) const override;
    bool setImageTransform(const QString& imageId, const QVariantMap& transform) override;
    QList<double> imageToWorldCoordinates(const QString& imageId, const QList<int>& imageCoords) const override;
    QList<int> worldToImageCoordinates(const QString& imageId, const QList<double>& worldCoords) const override;

    // ==================== 格式转换功能实现 ====================
    
    QString convertImageFormat(const QString& sourceImageId, const QString& targetFormat, const QVariantMap& options = QVariantMap()) override;
    bool saveImage(const QString& imageId, const QString& filePath, const QString& format = QString(), const QVariantMap& options = QVariantMap()) override;
    QString saveImageAsync(const QString& imageId, const QString& filePath, const QString& format = QString(), const QVariantMap& options = QVariantMap()) override;
    bool exportImage(const QString& imageId, const QString& exportFormat, const QString& filePath, const QVariantMap& exportOptions = QVariantMap()) override;

    // ==================== 缓存和性能管理实现 ====================
    
    void setMemoryCacheLimit(int maxMemoryMB) override;
    int getMemoryCacheLimit() const override;
    QMap<QString, QVariant> getMemoryUsageInfo() const override;
    void clearMemoryCache(int keepRecentImages = 0) override;
    int optimizeMemoryUsage() override;
    int preloadImages(const QStringList& imageIds) override;

    // ==================== 服务状态和配置实现 ====================
    
    QString getServiceStatus() const override;
    QVariantMap getServiceConfiguration() const override;
    bool setServiceConfiguration(const QVariantMap& config) override;
    QString getLastError() const override;
    QStringList getSupportedLoaders() const override;
    QVariantMap getLoaderInfo(const QString& loaderName) const override;

    // ==================== 任务管理实现 ====================
    
    QStringList getActiveTasks() const override;
    QVariantMap getTaskStatus(const QString& taskId) const override;
    bool cancelTask(const QString& taskId) override;
    int cancelAllTasks() override;

    // ==================== UI显示管理实现 ====================
    
    bool showImageManagerDialog(QWidget* parent = nullptr) override;
    bool showImagePropertiesDialog(QWidget* parent = nullptr) override;
    bool showLoaderConfigDialog(QWidget* parent = nullptr) override;
    bool showMemoryManagerDialog(QWidget* parent = nullptr) override;

    // ==================== 便利访问方法（内部使用） ====================
    
    /**
     * @brief 获取图像数据对象（内部使用）
     * @param imageId 图像ID
     * @return 图像数据指针，失败返回nullptr
     */
    MedicalImageData* getImage(const QString& imageId) const;

private slots:
    /**
     * @brief 外部服务可用性变化处理
     * @param available 是否可用
     */
    void onExternalServiceAvailabilityChanged(bool available);

    /**
     * @brief 异步加载任务完成处理
     * @param taskId 任务ID
     * @param imageId 图像ID
     */
    void onAsyncLoadTaskCompleted(const QString& taskId, const QString& imageId);

    /**
     * @brief 异步保存任务完成处理
     * @param taskId 任务ID
     * @param success 是否成功
     */
    void onAsyncSaveTaskCompleted(const QString& taskId, bool success);

private:
    // ==================== 核心功能实现 ====================
    
    /**
     * @brief 初始化服务配置
     */
    void initializeServiceConfiguration();
    
    /**
     * @brief 初始化加载器管理器
     */
    void initializeLoaderManager();
    
    /**
     * @brief 连接外部服务
     */
    void connectExternalServices();

private slots:
    /**
     * @brief 异步图像加载完成槽函数
     */
    void onAsyncLoadFinished();

signals:
    /**
     * @brief 异步加载进度信号
     * @param taskId 任务ID
     * @param progress 进度百分比(0-100)
     * @param message 进度消息
     */
    void asyncLoadProgress(const QString& taskId, int progress, const QString& message);

private:
    /**
     * @brief 异步加载图像的工作函数
     * @param filePath 文件路径
     * @param taskId 任务ID
     * @return 加载结果结构
     */
    struct AsyncLoadResult {
        QString taskId;
        QString filePath;
        MedicalImageData* imageData;
        QString error;
        bool success;
        
        AsyncLoadResult() : imageData(nullptr), success(false) {}
    };
    
    AsyncLoadResult doAsyncImageLoad(const QString& filePath, const QString& taskId);
    
    /**
     * @brief 检查外部服务可用性
     */
    void checkExternalServiceAvailability();
    
    /**
     * @brief 更新内存使用统计
     * @param imageData 图像数据
     * @param add 是否增加内存使用（true=增加，false=减少）
     */
    void updateMemoryUsage(MedicalImageData* imageData, bool add = true);
    
    /**
     * @brief 计算图像数据内存大小(MB)
     * @param imageData 图像数据
     * @return 内存大小(MB)
     */
    qint64 calculateImageMemoryMB(MedicalImageData* imageData) const;
    
    /**
     * @brief 注册图像到映射表
     * @param imageId 图像ID
     * @param imageData 图像数据
     * @param filePath 原始文件路径
     */
    void registerImage(const QString& imageId, MedicalImageData* imageData, const QString& filePath);
    
    /**
     * @brief 生成唯一图像ID
     * @param baseName 基础名称（可选）
     * @return 唯一图像ID
     */
    QString generateImageId(const QString& baseName = QString());
    
    /**
     * @brief 生成唯一任务ID
     * @return 唯一任务ID
     */
    QString generateTaskId();
    
#ifdef ITK_FOUND
    /**
     * @brief ITK图像加载模板函数（同步版本）
     * @param filePath 图像文件路径
     * @param imageData 图像数据对象
     * @param format 图像格式
     * @return 图像ID，如果加载失败返回空字符串
     */
    template<typename TPixel, unsigned int VDimension>
    QString loadITKImage(const QString& filePath, MedicalImageData* imageData, const QString& format);
    
    /**
     * @brief ITK图像加载模板函数（异步版本，仅处理数据不注册）
     * @param filePath 图像文件路径
     * @param imageData 图像数据对象
     * @param format 图像格式
     * @param taskId 任务ID（用于进度报告）
     * @return 是否加载成功
     */
    template<typename TPixel, unsigned int VDimension>
    bool loadITKImageAsync(const QString& filePath, MedicalImageData* imageData, 
                          const QString& format, const QString& taskId);
#endif
    
    /**
     * @brief 验证图像ID格式
     * @param imageId 图像ID
     * @return 是否有效
     */
    bool validateImageId(const QString& imageId) const;
    
    /**
     * @brief 设置错误信息
     * @param error 错误描述
     */
    void setError(const QString& error);

    // ==================== 异步任务管理 ====================
    
    /**
     * @brief 任务信息结构
     */
    struct TaskInfo {
        QString taskId;
        QString operation;
        QString filePath;
        QString imageId;
        QVariantMap options;
        QDateTime startTime;
        QString status;
        int progress;
        QString error;
        
        TaskInfo() : progress(0) {}
    };
    
    /**
     * @brief 添加任务到活动列表
     * @param task 任务信息
     */
    void addActiveTask(const TaskInfo& task);
    
    /**
     * @brief 更新任务状态
     * @param taskId 任务ID
     * @param status 新状态
     * @param progress 进度（可选）
     * @param error 错误信息（可选）
     */
    void updateTaskStatus(const QString& taskId, const QString& status, int progress = -1, const QString& error = QString());
    
    /**
     * @brief 移除完成的任务
     * @param taskId 任务ID
     */
    void removeCompletedTask(const QString& taskId);

    /**
     * @brief 初始化EventAdmin服务
     */
    void initializeEventAdmin();

    /**
     * @brief 发送CTK事件
     * @param topic 事件主题
     * @param properties 事件属性
     */
    void sendEvent(const QString& topic, const QVariantMap& properties);

    // ==================== 数据成员 ====================
    
    // CTK框架相关
    ctkPluginContext* m_pluginContext;
    ctkEventAdmin* m_eventAdmin;
    
    // 核心数据存储
    QMap<QString, MedicalImageData*> m_images;      // 图像数据映射
    QMap<QString, QString> m_imageSources;          // 图像来源映射
    QMap<QString, QVariantMap> m_imageMetadataCache; // 元数据缓存
    
    // 任务管理
    QMap<QString, TaskInfo> m_activeTasks;          // 活动任务
    
    // 外部服务引用（可选集成）
    ctkServiceReference m_processingServiceRef;
    MedicalProcessingService* m_processingService;
    
    ctkServiceReference m_viewerServiceRef;
    MedicalViewerService* m_viewerService;
    
    ctkServiceReference m_patientServiceRef;
    PatientDatabaseService* m_patientService;
    
    ctkServiceReference m_dataStorageServiceRef;
    DataStorageService* m_dataStorageService;
    
    // 服务状态和配置
    QString m_serviceStatus;
    QVariantMap m_serviceConfig;
    QString m_lastError;
    
    // 内存管理
    int m_memoryCacheLimitMB;
    QStringList m_recentImageIds;
    QTimer* m_memoryTimer;
    
    // 异步加载管理
    QFutureWatcher<AsyncLoadResult>* m_loadWatcher;
    QMap<QString, QString> m_pendingLoads;  // 任务ID -> 文件路径
    
    // 内存管理增强
    qint64 m_currentMemoryUsageMB;          // 当前内存使用量(MB)
    static const qint64 MAX_MEMORY_LIMIT_MB = 4096;  // 最大内存限制(4GB)
    static const qint64 LARGE_IMAGE_THRESHOLD_MB = 100;  // 大图像阈值(100MB)
    
    // 加载器管理
    QStringList m_supportedFormats;
    QMap<QString, QVariantMap> m_loaderInfo;
    
    // 线程安全
    mutable QMutex m_mutex;
    mutable QMutex m_taskMutex;
};

#endif // MEDICAL_IMAGE_CORE_SERVICE_IMPL_H
