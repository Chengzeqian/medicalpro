#ifndef SERVICE_INTERFACES_H
#define SERVICE_INTERFACES_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>

/**
 * @brief CTK服务接口前向声明文件
 * 
 * 此文件包含完全CTK架构所需的服务接口前向声明，
 * 避免直接包含其他插件的头文件，实现真正的解耦。
 */

// ==================== UnifiedMedicalImageService 接口声明 ====================

/**
 * @brief 统一医学图像服务接口（前向声明）
 * 
 * 提供基于图像ID的图像操作接口，支持完全CTK架构
 */
class UnifiedMedicalImageService : public QObject
{
    Q_OBJECT

public:
    virtual ~UnifiedMedicalImageService() = default;
    
    // 基本图像管理方法
    virtual QStringList getLoadedImages() const = 0;
    virtual bool hasImage(const QString& imageId) const = 0;
    virtual bool releaseImage(const QString& imageId) = 0;
    virtual void clearAllImages() = 0;
    
    // 图像信息查询方法
    virtual QString getImageInfo(const QString& imageId) const = 0;
    virtual QMap<QString, QVariant> getImageMetadata(const QString& imageId) const = 0;
    virtual QList<int> getImageDimensions(const QString& imageId) const = 0;
    virtual QList<double> getImageSpacing(const QString& imageId) const = 0;
    virtual QString getImageDataType(const QString& imageId) const = 0;
    virtual void* getImagePixelData(const QString& imageId) const = 0;
    
    // 图像创建和存储方法（用于处理结果）
    virtual QString createEmptyImage(const QList<int>& dimensions, 
                                   const QList<double>& spacing = QList<double>(),
                                   const QList<double>& origin = QList<double>()) = 0;
    virtual bool storeProcessedImage(const QString& imageId, 
                                   const QString& sourceImageId,
                                   const QString& operation,
                                   const QVariantMap& parameters) = 0;

signals:
    void imageLoadCompleted(const QString& taskId, const QString& imageId);
    void imageLoadFailed(const QString& taskId, const QString& error);
    void imageLoaded(const QString& imageId, const QString& filePath);
};

// Qt接口声明
Q_DECLARE_INTERFACE(UnifiedMedicalImageService, "medical.UnifiedMedicalImageService")

#endif // SERVICE_INTERFACES_H
