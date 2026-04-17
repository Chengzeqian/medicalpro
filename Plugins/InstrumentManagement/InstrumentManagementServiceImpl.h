#ifndef INSTRUMENT_MANAGEMENT_SERVICE_IMPL_H
#define INSTRUMENT_MANAGEMENT_SERVICE_IMPL_H

#include "InstrumentManagementService.h"
#include <QSqlDatabase>
#include <QMutex>

/**
 * @brief 器械管理服务实现类
 * 
 * 实现InstrumentManagementService接口的所有功能
 */
class InstrumentManagementServiceImpl : public InstrumentManagementService
{
    Q_OBJECT
    Q_INTERFACES(InstrumentManagementService)
    
public:
    explicit InstrumentManagementServiceImpl(QObject* parent = nullptr);
    virtual ~InstrumentManagementServiceImpl();
    
    // ========== 器械基础管理 ==========
    int addInstrument(const InstrumentItem& instrument) override;
    bool updateInstrument(const InstrumentItem& instrument) override;
    bool deleteInstrument(int instrumentId) override;
    bool removeInstrumentPermanently(int instrumentId) override;
    InstrumentItem getInstrument(int instrumentId) override;
    QList<InstrumentItem> getAllInstruments(bool includeInactive = false) override;
    QList<InstrumentItem> getInstrumentsByCategory(const QString& category) override;
    QList<InstrumentItem> getInstrumentsByStatus(const QString& status) override;
    QList<InstrumentItem> searchInstruments(const QString& keyword) override;
    
    // ========== 3D模型管理 ==========
    bool setInstrumentModel(int instrumentId, const QString& modelFilePath) override;
    QString generateInstrumentThumbnail(int instrumentId, int thumbnailSize = 200) override;
    QString generateInstrumentPreview(int instrumentId, int previewSize = 800) override;
    bool generatePreviewFromModel(const QString& modelFilePath, const QString& outputPath, int previewSize = 800) override;
    bool generateThumbnailAsync(const QString& modelFilePath, const QString& outputPath, int size) override;
    bool generatePreviewAsync(const QString& modelFilePath, const QString& outputPath, int size) override;
    QStringList getAvailableModelFiles() override;
    int importInstrumentsFromModels() override;
    
    // ========== 光学追踪配置 ==========
    bool setInstrumentTracking(int instrumentId, const QString& markerId, const QString& geometryFilePath) override;
    MarkerGeometry loadMarkerGeometry(const QString& geometryFilePath) override;
    QStringList getAvailableGeometryFiles() override;
    
    // ========== 器械校准 ==========
    bool saveCalibrationData(const InstrumentCalibrationData& calibrationData) override;
    QList<InstrumentCalibrationData> getCalibrationHistory(int instrumentId) override;
    InstrumentCalibrationData getLatestCalibration(int instrumentId) override;
    bool applyCalibrationToInstrument(int instrumentId, const InstrumentCalibrationData& calibrationData) override;
    
    // ========== 器械使用记录 ==========
    int recordInstrumentUsage(const InstrumentUsageRecord& record) override;
    QList<InstrumentUsageRecord> getUsageHistory(int instrumentId) override;
    
    // ========== 器械维护记录 ==========
    int addMaintenanceRecord(const InstrumentMaintenanceRecord& record) override;
    QList<InstrumentMaintenanceRecord> getMaintenanceHistory(int instrumentId) override;
    
    // ========== 器械与手术关联 ==========
    bool linkInstrumentToSurgery(int instrumentId, int surgeryId) override;
    bool unlinkInstrumentFromSurgery(int instrumentId, int surgeryId) override;
    QList<InstrumentItem> getInstrumentsBySurgery(int surgeryId) override;
    QList<int> getSurgeriesByInstrument(int instrumentId) override;
    
    // ========== 统计信息 ==========
    InstrumentStatistics getStatistics() override;
    QList<InstrumentItem> getUncalibratedInstruments() override;
    QList<InstrumentItem> getInstrumentsNeedingMaintenance() override;
    
    // ========== 数据库管理 ==========
    bool initializeDatabase() override;
    bool isDatabaseConnected() override;
    QString getLastError() const override;
    int cleanupDuplicateInstruments() override;
    bool resetAutoIncrement() override;
    QString getProjectPath() const override;

private:
    // 数据库相关
    QSqlDatabase m_database;
    mutable QMutex m_mutex;
    mutable QString m_lastError;
    
    // 项目路径
    QString m_projectPath;
    QString m_modelsPath;        // plustoolkitModels路径
    QString m_geometryPath;      // geometry路径
    QString m_thumbnailsPath;    // 缩略图路径
    
    // 辅助方法
    bool checkDatabaseConnection() const;
    bool executeQuery(QSqlQuery& query);
    void logMessage(const QString& level, const QString& message) const;
    void ensureDirectoriesExist();
    
    // 缩略图生成（使用VTK）
    bool generateThumbnailVTK(const QString& modelPath, const QString& outputPath, int size);

    // 高质量预览图生成（使用VTK，带金属材质）
    bool generatePreviewVTK(const QString& modelPath, const QString& outputPath, int size);

    // INI文件解析
    MarkerGeometry parseGeometryIni(const QString& filePath) const;
};

#endif // INSTRUMENT_MANAGEMENT_SERVICE_IMPL_H

