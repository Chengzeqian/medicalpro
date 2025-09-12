#ifndef MEDICAL_IMAGE_CORE_SERVICE_H
#define MEDICAL_IMAGE_CORE_SERVICE_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>
#include <QMetaType>

/**
 * @brief 图像格式枚举
 */
enum class ImageFormat {
    Unknown = 0,
    DICOM,      // DICOM格式
    NRRD,       // NRRD格式
    NIfTI,      // NIfTI格式
    PNG,        // PNG格式
    JPEG,       // JPEG格式
    TIFF,       // TIFF格式
    BMP,        // BMP格式
    MetaImage   // MetaImage格式
};

/**
 * @brief 图像加载选项
 */
struct ImageLoadOptions {
    bool loadMetadata = true;           // 是否加载元数据
    bool loadPixelData = true;          // 是否加载像素数据
    bool enableProgressReporting = true; // 是否启用进度报告
    int maxImageSize = 1024 * 1024 * 1024; // 最大图像大小 (1GB)
    QString preferredMemoryLayout = "Standard"; // 内存布局偏好
    
    ImageLoadOptions() = default;
};

/**
 * @brief Medical Image Core Service Interface (标准CTK架构)
 * 
 * 提供统一的医学图像处理服务，采用标准CTK架构设计：
 * - 所有操作基于图像ID，不直接暴露数据对象
 * - 支持多格式图像加载和管理
 * - 提供完整的元数据和属性访问接口
 * - 异步加载和进度报告
 * - 内存管理和缓存优化
 * 
 * 核心设计原则：
 * 1. 输入：文件路径 (QString)
 * 2. 输出：图像ID (QString) 
 * 3. 访问：通过图像ID获取属性和数据
 * 4. 通信：完全通过CTK服务框架
 */
class MedicalImageCoreService : public QObject
{
    Q_OBJECT

public:
    explicit MedicalImageCoreService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~MedicalImageCoreService() = default;

    // ==================== 图像加载功能 ====================
    
    /**
     * @brief 获取支持的图像格式列表
     * @return 支持的文件扩展名列表
     */
    virtual QStringList getSupportedFormats() const = 0;
    
    /**
     * @brief 检测图像文件格式
     * @param filePath 图像文件路径
     * @return 图像格式字符串
     */
    virtual QString detectImageFormat(const QString& filePath) const = 0;
    
    /**
     * @brief 同步加载医学图像文件
     * @param filePath 图像文件路径
     * @param options 加载选项（可选）
     * @return 图像ID，失败返回空字符串
     */
    virtual QString loadImage(const QString& filePath, const QVariantMap& options = QVariantMap()) = 0;
    
    /**
     * @brief 异步加载医学图像文件
     * @param filePath 图像文件路径
     * @param options 加载选项（可选）
     * @return 加载任务ID，用于跟踪加载进度
     */
    virtual QString loadImageAsync(const QString& filePath, const QVariantMap& options = QVariantMap()) = 0;
    
    /**
     * @brief 加载DICOM序列
     * @param seriesDirectory DICOM序列目录
     * @param seriesUID 序列UID（可选）
     * @param options 加载选项（可选）
     * @return 图像ID，失败返回空字符串
     */
    virtual QString loadDicomSeries(const QString& seriesDirectory, const QString& seriesUID = QString(), const QVariantMap& options = QVariantMap()) = 0;
    
    /**
     * @brief 异步加载DICOM序列
     * @param seriesDirectory DICOM序列目录
     * @param seriesUID 序列UID（可选）
     * @param options 加载选项（可选）
     * @return 加载任务ID
     */
    virtual QString loadDicomSeriesAsync(const QString& seriesDirectory, const QString& seriesUID = QString(), const QVariantMap& options = QVariantMap()) = 0;
    
    /**
     * @brief 加载多个图像文件
     * @param filePaths 图像文件路径列表
     * @param options 加载选项（可选）
     * @return 批量加载任务ID
     */
    virtual QString loadMultipleImages(const QStringList& filePaths, const QVariantMap& options = QVariantMap()) = 0;

    // ==================== 图像管理功能 ====================
    
    /**
     * @brief 获取已加载的图像列表
     * @return 图像ID列表
     */
    virtual QStringList getLoadedImages() const = 0;
    
    /**
     * @brief 检查图像是否存在
     * @param imageId 图像ID
     * @return 是否存在
     */
    virtual bool hasImage(const QString& imageId) const = 0;
    
    /**
     * @brief 检查图像是否有效
     * @param imageId 图像ID
     * @return 是否有效
     */
    virtual bool isValid(const QString& imageId) const = 0;
    
    /**
     * @brief 释放图像内存
     * @param imageId 图像ID
     * @return 是否释放成功
     */
    virtual bool releaseImage(const QString& imageId) = 0;
    
    /**
     * @brief 清理所有图像
     */
    virtual void clearAllImages() = 0;
    
    /**
     * @brief 获取图像加载来源
     * @param imageId 图像ID
     * @return 原始文件路径或来源描述
     */
    virtual QString getImageSource(const QString& imageId) const = 0;
    
    /**
     * @brief 复制图像
     * @param sourceImageId 源图像ID
     * @param newImageId 新图像ID（可选，自动生成）
     * @return 新图像ID，失败返回空字符串
     */
    virtual QString duplicateImage(const QString& sourceImageId, const QString& newImageId = QString()) = 0;

    // ==================== 图像属性和信息查询 ====================
    
    /**
     * @brief 获取图像基本信息
     * @param imageId 图像ID
     * @return 基本信息字符串
     */
    virtual QString getImageInfo(const QString& imageId) const = 0;
    
    /**
     * @brief 获取图像详细信息
     * @param imageId 图像ID
     * @return 详细信息映射
     */
    virtual QVariantMap getImageDetails(const QString& imageId) const = 0;
    
    /**
     * @brief 获取图像元数据
     * @param imageId 图像ID
     * @return 元数据键值对
     */
    virtual QMap<QString, QVariant> getImageMetadata(const QString& imageId) const = 0;
    
    /**
     * @brief 设置图像元数据
     * @param imageId 图像ID
     * @param key 元数据键
     * @param value 元数据值
     * @return 成功返回true，失败返回false
     */
    virtual bool setImageMetadata(const QString& imageId, const QString& key, const QVariant& value) = 0;
    
    /**
     * @brief 获取图像尺寸
     * @param imageId 图像ID
     * @return 尺寸信息 [width, height, depth, ...]
     */
    virtual QList<int> getImageDimensions(const QString& imageId) const = 0;
    
    /**
     * @brief 获取像素间距
     * @param imageId 图像ID
     * @return 像素间距 [x, y, z, ...]
     */
    virtual QList<double> getImageSpacing(const QString& imageId) const = 0;
    
    /**
     * @brief 获取图像原点
     * @param imageId 图像ID
     * @return 原点坐标 [x, y, z, ...]
     */
    virtual QList<double> getImageOrigin(const QString& imageId) const = 0;
    
    /**
     * @brief 获取图像方向矩阵
     * @param imageId 图像ID
     * @return 方向矩阵
     */
    virtual QList<double> getImageDirection(const QString& imageId) const = 0;
    
    /**
     * @brief 获取数据类型
     * @param imageId 图像ID
     * @return 数据类型字符串
     */
    virtual QString getImageDataType(const QString& imageId) const = 0;
    
    /**
     * @brief 获取图像格式
     * @param imageId 图像ID
     * @return 图像格式字符串
     */
    virtual QString getImageFormat(const QString& imageId) const = 0;
    
    /**
     * @brief 检查图像是否为3D
     * @param imageId 图像ID
     * @return 是否为3D图像
     */
    virtual bool is3D(const QString& imageId) const = 0;
    
    /**
     * @brief 获取图像统计信息
     * @param imageId 图像ID
     * @return 统计信息 (min, max, mean, std, etc.)
     */
    virtual QMap<QString, double> getImageStatistics(const QString& imageId) const = 0;
    
    /**
     * @brief 计算图像统计信息
     * @param imageId 图像ID
     * @param forceRecalculate 是否强制重新计算
     * @return 成功返回true，失败返回false
     */
    virtual bool calculateImageStatistics(const QString& imageId, bool forceRecalculate = false) = 0;

    // ==================== 图像数据访问 ====================
    
    /**
     * @brief 获取像素数据指针（用于高性能访问）
     * @param imageId 图像ID
     * @return 像素数据指针，失败返回nullptr
     */
    virtual void* getImagePixelData(const QString& imageId) const = 0;
    
    /**
     * @brief 获取像素数据大小
     * @param imageId 图像ID
     * @return 数据大小（字节），失败返回0
     */
    virtual qint64 getImageDataSize(const QString& imageId) const = 0;
    
    /**
     * @brief 获取指定位置的像素值
     * @param imageId 图像ID
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标（3D图像）
     * @return 像素值，失败返回无效值
     */
    virtual QVariant getPixelValue(const QString& imageId, int x, int y, int z = 0) const = 0;
    
    /**
     * @brief 设置指定位置的像素值
     * @param imageId 图像ID
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标（3D图像）
     * @param value 像素值
     * @return 成功返回true，失败返回false
     */
    virtual bool setPixelValue(const QString& imageId, int x, int y, int z, const QVariant& value) = 0;
    
    /**
     * @brief 获取图像区域数据
     * @param imageId 图像ID
     * @param region 区域定义 {"x", "y", "z", "width", "height", "depth"}
     * @return 区域数据，失败返回空映射
     */
    virtual QVariantMap getImageRegion(const QString& imageId, const QVariantMap& region) const = 0;

    // ==================== 图像变换和坐标系统 ====================
    
    /**
     * @brief 获取图像变换矩阵
     * @param imageId 图像ID
     * @return 4x4变换矩阵
     */
    virtual QVariantMap getImageTransform(const QString& imageId) const = 0;
    
    /**
     * @brief 设置图像变换矩阵
     * @param imageId 图像ID
     * @param transform 变换矩阵
     * @return 成功返回true，失败返回false
     */
    virtual bool setImageTransform(const QString& imageId, const QVariantMap& transform) = 0;
    
    /**
     * @brief 图像坐标到世界坐标转换
     * @param imageId 图像ID
     * @param imageCoords 图像坐标 [x, y, z]
     * @return 世界坐标 [x, y, z]
     */
    virtual QList<double> imageToWorldCoordinates(const QString& imageId, const QList<int>& imageCoords) const = 0;
    
    /**
     * @brief 世界坐标到图像坐标转换
     * @param imageId 图像ID
     * @param worldCoords 世界坐标 [x, y, z]
     * @return 图像坐标 [x, y, z]
     */
    virtual QList<int> worldToImageCoordinates(const QString& imageId, const QList<double>& worldCoords) const = 0;

    // ==================== 格式转换功能 ====================
    
    /**
     * @brief 转换图像格式
     * @param sourceImageId 源图像ID
     * @param targetFormat 目标格式
     * @param options 转换选项（可选）
     * @return 转换后的图像ID，失败返回空字符串
     */
    virtual QString convertImageFormat(const QString& sourceImageId, const QString& targetFormat, const QVariantMap& options = QVariantMap()) = 0;
    
    /**
     * @brief 保存图像到文件
     * @param imageId 图像ID
     * @param filePath 保存路径
     * @param format 保存格式（可选，自动检测）
     * @param options 保存选项（可选）
     * @return 是否保存成功
     */
    virtual bool saveImage(const QString& imageId, const QString& filePath, const QString& format = QString(), const QVariantMap& options = QVariantMap()) = 0;
    
    /**
     * @brief 异步保存图像到文件
     * @param imageId 图像ID
     * @param filePath 保存路径
     * @param format 保存格式（可选）
     * @param options 保存选项（可选）
     * @return 保存任务ID
     */
    virtual QString saveImageAsync(const QString& imageId, const QString& filePath, const QString& format = QString(), const QVariantMap& options = QVariantMap()) = 0;
    
    /**
     * @brief 导出图像为标准格式
     * @param imageId 图像ID
     * @param exportFormat 导出格式 ("PNG", "JPEG", "TIFF")
     * @param filePath 保存路径
     * @param exportOptions 导出选项
     * @return 成功返回true，失败返回false
     */
    virtual bool exportImage(const QString& imageId, const QString& exportFormat, const QString& filePath, const QVariantMap& exportOptions = QVariantMap()) = 0;

    // ==================== 缓存和性能管理 ====================
    
    /**
     * @brief 设置内存缓存大小限制
     * @param maxMemoryMB 最大内存使用量（MB）
     */
    virtual void setMemoryCacheLimit(int maxMemoryMB) = 0;
    
    /**
     * @brief 获取内存缓存限制
     * @return 内存限制（MB）
     */
    virtual int getMemoryCacheLimit() const = 0;
    
    /**
     * @brief 获取当前内存使用情况
     * @return 内存使用情况信息
     */
    virtual QMap<QString, QVariant> getMemoryUsageInfo() const = 0;
    
    /**
     * @brief 清理内存缓存
     * @param keepRecentImages 保留最近使用的图像数量
     */
    virtual void clearMemoryCache(int keepRecentImages = 0) = 0;
    
    /**
     * @brief 优化内存使用
     * @return 释放的内存大小（MB）
     */
    virtual int optimizeMemoryUsage() = 0;
    
    /**
     * @brief 预加载图像到缓存
     * @param imageIds 图像ID列表
     * @return 成功预加载的图像数量
     */
    virtual int preloadImages(const QStringList& imageIds) = 0;

    // ==================== 服务状态和配置 ====================
    
    /**
     * @brief 获取服务状态
     * @return 服务状态信息
     */
    virtual QString getServiceStatus() const = 0;
    
    /**
     * @brief 获取服务配置
     * @return 配置信息映射
     */
    virtual QVariantMap getServiceConfiguration() const = 0;
    
    /**
     * @brief 设置服务配置
     * @param config 配置信息
     * @return 成功返回true，失败返回false
     */
    virtual bool setServiceConfiguration(const QVariantMap& config) = 0;
    
    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息
     */
    virtual QString getLastError() const = 0;
    
    /**
     * @brief 获取支持的加载器列表
     * @return 加载器信息列表
     */
    virtual QStringList getSupportedLoaders() const = 0;
    
    /**
     * @brief 获取加载器信息
     * @param loaderName 加载器名称
     * @return 加载器详细信息
     */
    virtual QVariantMap getLoaderInfo(const QString& loaderName) const = 0;

    // ==================== 任务管理 ====================
    
    /**
     * @brief 获取活动任务列表
     * @return 任务ID列表
     */
    virtual QStringList getActiveTasks() const = 0;
    
    /**
     * @brief 获取任务状态
     * @param taskId 任务ID
     * @return 任务状态信息
     */
    virtual QVariantMap getTaskStatus(const QString& taskId) const = 0;
    
    /**
     * @brief 取消任务
     * @param taskId 任务ID
     * @return 成功返回true，失败返回false
     */
    virtual bool cancelTask(const QString& taskId) = 0;
    
    /**
     * @brief 取消所有任务
     * @return 取消的任务数量
     */
    virtual int cancelAllTasks() = 0;

    // ==================== UI显示管理（遵循PatientManagement成功模式） ====================
    
    /**
     * @brief 显示图像管理界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showImageManagerDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示图像属性查看界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showImagePropertiesDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示图像加载器配置界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showLoaderConfigDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示内存管理界面
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showMemoryManagerDialog(QWidget* parent = nullptr) = 0;

signals:
    /**
     * @brief 图像加载开始信号
     * @param taskId 任务ID
     * @param filePath 文件路径
     */
    void imageLoadStarted(const QString& taskId, const QString& filePath);
    
    /**
     * @brief 图像加载进度信号
     * @param taskId 任务ID
     * @param progress 进度百分比 (0-100)
     */
    void imageLoadProgress(const QString& taskId, int progress);
    
    /**
     * @brief 图像加载完成信号
     * @param taskId 任务ID
     * @param imageId 加载的图像ID
     */
    void imageLoadCompleted(const QString& taskId, const QString& imageId);
    
    /**
     * @brief 图像加载完成信号（简化版）
     * @param imageId 加载的图像ID
     * @param filePath 文件路径
     */
    void imageLoaded(const QString& imageId, const QString& filePath);
    
    /**
     * @brief 图像加载失败信号
     * @param taskId 任务ID
     * @param error 错误信息
     */
    void imageLoadFailed(const QString& taskId, const QString& error);
    
    /**
     * @brief 批量加载完成信号
     * @param taskId 任务ID
     * @param imageIds 加载的图像ID列表
     */
    void batchLoadCompleted(const QString& taskId, const QStringList& imageIds);
    
    /**
     * @brief 图像释放信号
     * @param imageId 图像ID
     */
    void imageReleased(const QString& imageId);
    
    /**
     * @brief 图像保存开始信号
     * @param taskId 任务ID
     * @param imageId 图像ID
     * @param filePath 保存路径
     */
    void imageSaveStarted(const QString& taskId, const QString& imageId, const QString& filePath);
    
    /**
     * @brief 图像保存完成信号
     * @param taskId 任务ID
     * @param success 是否成功
     */
    void imageSaveCompleted(const QString& taskId, bool success);
    
    /**
     * @brief 内存使用量变化信号
     * @param usedMemoryMB 已使用内存（MB）
     * @param totalMemoryMB 总内存限制（MB）
     */
    void memoryUsageChanged(int usedMemoryMB, int totalMemoryMB);
    
    /**
     * @brief 服务错误信号
     * @param error 错误信息
     */
    void serviceError(const QString& error);
    
    /**
     * @brief 服务状态变化信号
     * @param status 新状态
     */
    void serviceStatusChanged(const QString& status);
    
    /**
     * @brief 任务状态变化信号
     * @param taskId 任务ID
     * @param status 任务状态
     */
    void taskStatusChanged(const QString& taskId, const QString& status);
};

// Qt接口声明
Q_DECLARE_INTERFACE(MedicalImageCoreService, "medical.MedicalImageCoreService")

#endif // MEDICAL_IMAGE_CORE_SERVICE_H
