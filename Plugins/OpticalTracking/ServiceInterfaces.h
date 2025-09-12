#ifndef OPTICAL_TRACKING_SERVICE_INTERFACES_H
#define OPTICAL_TRACKING_SERVICE_INTERFACES_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>

/**
 * @brief CTK服务接口前向声明文件（OpticalTracking插件）
 * 
 * 此文件包含完全CTK架构所需的服务接口前向声明，
 * 避免直接包含其他插件的头文件，实现真正的解耦。
 * 
 * 注意：OpticalTracking插件主要是独立的硬件接口服务，
 * 通常不需要直接依赖其他医学图像处理插件。
 */

// ==================== 可选的医学图像服务接口声明 ====================
// （如果需要与图像系统集成时使用）

/**
 * @brief 统一医学图像服务接口（前向声明）
 * 
 * 提供基于图像ID的图像操作接口，支持完全CTK架构
 * 注意：此接口在OpticalTracking插件中是可选的，
 * 仅在需要与图像系统进行配准时使用
 */
class UnifiedMedicalImageService : public QObject
{
    Q_OBJECT

public:
    virtual ~UnifiedMedicalImageService() = default;
    
    // 基本图像管理方法（用于图像配准）
    virtual QStringList getLoadedImages() const = 0;
    virtual bool releaseImage(const QString& imageId) = 0;
    
    // 图像信息查询方法（用于坐标系转换）
    virtual QString getImageInfo(const QString& imageId) const = 0;
    virtual QMap<QString, QVariant> getImageMetadata(const QString& imageId) const = 0;
    virtual QList<int> getImageDimensions(const QString& imageId) const = 0;
    virtual QList<double> getImageSpacing(const QString& imageId) const = 0;
    virtual QList<double> getImageOrigin(const QString& imageId) const = 0;
    
    // 图像变换相关（用于坐标配准）
    virtual QVariantMap getImageTransform(const QString& imageId) const = 0;

signals:
    void imageLoadCompleted(const QString& taskId, const QString& imageId);
    void imageLoadFailed(const QString& taskId, const QString& error);
};

// Qt接口声明
Q_DECLARE_INTERFACE(UnifiedMedicalImageService, "medical.UnifiedMedicalImageService")

// ==================== 图像交互服务接口声明 ====================
// （如果需要与交互系统集成时使用）

/**
 * @brief 图像交互服务接口（前向声明）
 * 
 * 提供图像交互功能，支持与光学跟踪的集成
 * 注意：此接口在OpticalTracking插件中是可选的，
 * 仅在需要进行图像引导手术时使用
 */
class ImageInteractionService : public QObject
{
    Q_OBJECT

public:
    virtual ~ImageInteractionService() = default;
    
    // 坐标转换相关方法（用于跟踪与图像的配准）
    virtual QList<double> screenToWorldCoordinates(const QString& componentId, double screenX, double screenY) = 0;
    virtual QList<double> worldToScreenCoordinates(const QString& componentId, double worldX, double worldY, double worldZ) = 0;
    virtual QList<double> imageToWorldCoordinates(const QString& componentId, int imageX, int imageY, int imageZ) = 0;
    
    // 标记点管理（用于配准点）
    virtual QString addMarkerPoint(const QString& componentId, double x, double y, double z, const QString& label = QString()) = 0;
    virtual QList<QVariantMap> getMarkerPoints(const QString& componentId) const = 0;

signals:
    void pointPicked(const QString& componentId, double worldX, double worldY, double worldZ,
                    int imageX, int imageY, int imageZ);
    void markerPointAdded(const QString& componentId, const QString& pointId, const QVariantMap& coordinates);
};

// Qt接口声明
Q_DECLARE_INTERFACE(ImageInteractionService, "medical.ImageInteractionService")

#endif // OPTICAL_TRACKING_SERVICE_INTERFACES_H
