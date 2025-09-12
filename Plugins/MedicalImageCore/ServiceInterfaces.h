#ifndef MEDICAL_IMAGE_CORE_SERVICE_INTERFACES_H
#define MEDICAL_IMAGE_CORE_SERVICE_INTERFACES_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>

/**
 * @brief CTK服务接口前向声明文件（MedicalImageCore插件）
 * 
 * 此文件包含完全CTK架构所需的服务接口前向声明，
 * MedicalImageCore作为核心服务提供者，主要定义对外服务接口，
 * 同时也可能使用其他可选的服务。
 */

// ==================== 可选的外部服务接口声明 ====================

/**
 * @brief 医学图像处理服务接口（前向声明）
 * 
 * MedicalImageCore可能需要与图像处理服务集成
 * 注意：此接口在MedicalImageCore中是可选的，
 * 仅在需要提供内置处理功能时使用
 */
class MedicalProcessingService : public QObject
{
    Q_OBJECT

public:
    virtual ~MedicalProcessingService() = default;
    
    // 基本处理方法（用于内置处理功能）
    virtual QStringList getSupportedOperations() const = 0;
    virtual QString gaussianFilter(const QString& imageId, double sigma) = 0;
    virtual QString thresholdSegmentation(const QString& imageId, double lowerThreshold, double upperThreshold) = 0;

signals:
    void processingCompleted(const QString& operationId, const QString& resultImageId);
    void processingFailed(const QString& operationId, const QString& error);
};

// Qt接口声明
Q_DECLARE_INTERFACE(MedicalProcessingService, "medical.MedicalProcessingService")

// ==================== 可选的图像查看器服务接口声明 ====================

/**
 * @brief 医学图像查看器服务接口（前向声明）
 * 
 * MedicalImageCore可能需要与查看器服务集成
 * 注意：此接口在MedicalImageCore中是可选的，
 * 仅在需要提供内置查看功能时使用
 */
class MedicalViewerService : public QObject
{
    Q_OBJECT

public:
    virtual ~MedicalViewerService() = default;
    
    // 基本查看器方法（用于内置查看功能）
    virtual QStringList getActiveViewers() const = 0;
    virtual bool displayImage(const QString& viewerId, const QString& imageId) = 0;

signals:
    void imageDisplayed(const QString& viewerId, const QString& imageId);
};

// Qt接口声明
Q_DECLARE_INTERFACE(MedicalViewerService, "medical.MedicalViewerService")

// ==================== 其他相关服务接口声明 ====================

/**
 * @brief 患者管理服务接口（前向声明）
 * 
 * MedicalImageCore可能需要与患者管理系统集成
 * 注意：此接口在MedicalImageCore中是可选的，
 * 仅在需要关联患者信息时使用
 */
class PatientDatabaseService : public QObject
{
    Q_OBJECT

public:
    virtual ~PatientDatabaseService() = default;
    
    // 患者信息关联方法
    virtual QString getCurrentPatientId() const = 0;
    virtual QVariantMap getPatientInfo(const QString& patientId) const = 0;

signals:
    void currentPatientChanged(const QString& patientId);
};

// Qt接口声明
Q_DECLARE_INTERFACE(PatientDatabaseService, "medical.PatientDatabaseService")

/**
 * @brief 数据存储服务接口（前向声明）
 * 
 * MedicalImageCore可能需要与数据存储服务集成
 * 注意：此接口是为了支持云存储或数据库存储等高级功能
 */
class DataStorageService : public QObject
{
    Q_OBJECT

public:
    virtual ~DataStorageService() = default;
    
    // 数据存储方法
    virtual QString storeImageData(const QString& imageId, const QVariantMap& metadata) = 0;
    virtual bool retrieveImageData(const QString& storageId, const QString& localPath) = 0;

signals:
    void dataStored(const QString& imageId, const QString& storageId);
    void dataRetrieved(const QString& storageId, const QString& imageId);
};

// Qt接口声明
Q_DECLARE_INTERFACE(DataStorageService, "medical.DataStorageService")

#endif // MEDICAL_IMAGE_CORE_SERVICE_INTERFACES_H
