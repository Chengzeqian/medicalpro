#ifndef INSTRUMENT_MANAGEMENT_SERVICE_H
#define INSTRUMENT_MANAGEMENT_SERVICE_H

#include "InstrumentDataStructures.h"
#include <QObject>
#include <QString>
#include <QList>

/**
 * @brief 器械管理服务接口
 * 
 * 提供器械管理的完整服务接口，包括器械CRUD、追踪、校准、统计等功能。
 * 这是一个纯虚接口，遵循CTK服务架构设计原则。
 */
class InstrumentManagementService : public QObject
{
    Q_OBJECT
    
public:
    explicit InstrumentManagementService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~InstrumentManagementService() = default;
    
    // ========== 器械基础管理 ==========
    
    /**
     * @brief 添加新器械
     * @param instrument 器械信息（不需要设置ID）
     * @return 成功返回新器械ID，失败返回-1
     */
    virtual int addInstrument(const InstrumentItem& instrument) = 0;
    
    /**
     * @brief 更新器械信息
     * @param instrument 器械信息（必须包含有效ID）
     * @return 成功返回true，失败返回false
     */
    virtual bool updateInstrument(const InstrumentItem& instrument) = 0;
    
    /**
     * @brief 删除器械（软删除，设置isActive=false）
     * @param instrumentId 器械ID
     * @return 成功返回true，失败返回false
     */
    virtual bool deleteInstrument(int instrumentId) = 0;
    
    /**
     * @brief 物理删除器械（从数据库中永久删除）
     * @param instrumentId 器械ID
     * @return 成功返回true，失败返回false
     */
    virtual bool removeInstrumentPermanently(int instrumentId) = 0;
    
    /**
     * @brief 根据ID获取器械信息
     * @param instrumentId 器械ID
     * @return 器械信息，如果不存在则返回无效的InstrumentItem
     */
    virtual InstrumentItem getInstrument(int instrumentId) = 0;
    
    /**
     * @brief 获取所有器械列表
     * @param includeInactive 是否包含已停用的器械
     * @return 器械列表
     */
    virtual QList<InstrumentItem> getAllInstruments(bool includeInactive = false) = 0;
    
    /**
     * @brief 根据类别获取器械列表
     * @param category 类别名称
     * @return 器械列表
     */
    virtual QList<InstrumentItem> getInstrumentsByCategory(const QString& category) = 0;
    
    /**
     * @brief 根据状态获取器械列表
     * @param status 状态（在库/使用中/维护/报废）
     * @return 器械列表
     */
    virtual QList<InstrumentItem> getInstrumentsByStatus(const QString& status) = 0;
    
    /**
     * @brief 搜索器械（按名称、型号、序列号）
     * @param keyword 搜索关键词
     * @return 器械列表
     */
    virtual QList<InstrumentItem> searchInstruments(const QString& keyword) = 0;
    
    // ========== 3D模型管理 ==========
    
    /**
     * @brief 为器械设置3D模型
     * @param instrumentId 器械ID
     * @param modelFilePath 模型文件路径（相对路径）
     * @return 成功返回true，失败返回false
     */
    virtual bool setInstrumentModel(int instrumentId, const QString& modelFilePath) = 0;
    
    /**
     * @brief 生成器械缩略图（同步方法，会阻塞UI）
     * @param instrumentId 器械ID
     * @param thumbnailSize 缩略图尺寸（默认200x200）
     * @return 成功返回缩略图路径，失败返回空字符串
     * @deprecated 建议使用异步方法避免UI冻结
     */
    virtual QString generateInstrumentThumbnail(int instrumentId, int thumbnailSize = 200) = 0;

    /**
     * @brief 生成器械高质量预览图（同步方法，会阻塞UI）
     * @param instrumentId 器械ID
     * @param previewSize 预览图尺寸（默认800x800）
     * @return 成功返回预览图路径，失败返回空字符串
     * @deprecated 建议使用异步方法避免UI冻结
     */
    virtual QString generateInstrumentPreview(int instrumentId, int previewSize = 800) = 0;

    /**
     * @brief 从模型文件路径生成预览图（同步方法，会阻塞UI）
     * @param modelFilePath 模型文件路径
     * @param outputPath 输出路径
     * @param previewSize 预览图尺寸（默认800x800）
     * @return 成功返回true，失败返回false
     * @deprecated 建议使用异步方法避免UI冻结
     */
    virtual bool generatePreviewFromModel(const QString& modelFilePath, const QString& outputPath, int previewSize = 800) = 0;

    /**
     * @brief 异步生成缩略图（不阻塞UI）
     * @param modelFilePath 模型文件路径
     * @param outputPath 输出路径
     * @param size 图片尺寸
     * @return 成功返回true，失败返回false
     * @note 此方法在后台线程执行，线程安全
     */
    virtual bool generateThumbnailAsync(const QString& modelFilePath, const QString& outputPath, int size) = 0;

    /**
     * @brief 异步生成预览图（不阻塞UI）
     * @param modelFilePath 模型文件路径
     * @param outputPath 输出路径
     * @param size 图片尺寸
     * @return 成功返回true，失败返回false
     * @note 此方法在后台线程执行，线程安全
     */
    virtual bool generatePreviewAsync(const QString& modelFilePath, const QString& outputPath, int size) = 0;

    /**
     * @brief 获取所有可用的模型文件列表
     * @return 模型文件路径列表
     */
    virtual QStringList getAvailableModelFiles() = 0;
    
    /**
     * @brief 从plustoolkitModels文件夹扫描并导入器械
     * @return 导入的器械数量
     */
    virtual int importInstrumentsFromModels() = 0;
    
    // ========== 光学追踪配置 ==========
    
    /**
     * @brief 设置器械的追踪标记
     * @param instrumentId 器械ID
     * @param markerId 标记ID
     * @param geometryFilePath 几何文件路径
     * @return 成功返回true，失败返回false
     */
    virtual bool setInstrumentTracking(int instrumentId, const QString& markerId, const QString& geometryFilePath) = 0;
    
    /**
     * @brief 加载标记几何信息
     * @param geometryFilePath 几何文件路径
     * @return 标记几何信息
     */
    virtual MarkerGeometry loadMarkerGeometry(const QString& geometryFilePath) = 0;
    
    /**
     * @brief 获取所有可用的几何文件列表
     * @return 几何文件路径列表
     */
    virtual QStringList getAvailableGeometryFiles() = 0;
    
    // ========== 器械校准 ==========
    
    /**
     * @brief 保存器械校准数据
     * @param calibrationData 校准数据
     * @return 成功返回true，失败返回false
     */
    virtual bool saveCalibrationData(const InstrumentCalibrationData& calibrationData) = 0;
    
    /**
     * @brief 获取器械的校准数据
     * @param instrumentId 器械ID
     * @return 校准数据列表（按时间倒序）
     */
    virtual QList<InstrumentCalibrationData> getCalibrationHistory(int instrumentId) = 0;
    
    /**
     * @brief 获取器械最新的校准数据
     * @param instrumentId 器械ID
     * @return 最新的校准数据
     */
    virtual InstrumentCalibrationData getLatestCalibration(int instrumentId) = 0;
    
    /**
     * @brief 应用校准数据到器械
     * @param instrumentId 器械ID
     * @param calibrationData 校准数据
     * @return 成功返回true，失败返回false
     */
    virtual bool applyCalibrationToInstrument(int instrumentId, const InstrumentCalibrationData& calibrationData) = 0;
    
    // ========== 器械使用记录 ==========
    
    /**
     * @brief 记录器械使用
     * @param record 使用记录
     * @return 成功返回记录ID，失败返回-1
     */
    virtual int recordInstrumentUsage(const InstrumentUsageRecord& record) = 0;
    
    /**
     * @brief 获取器械使用历史
     * @param instrumentId 器械ID
     * @return 使用记录列表
     */
    virtual QList<InstrumentUsageRecord> getUsageHistory(int instrumentId) = 0;
    
    // ========== 器械维护记录 ==========
    
    /**
     * @brief 添加维护记录
     * @param record 维护记录
     * @return 成功返回记录ID，失败返回-1
     */
    virtual int addMaintenanceRecord(const InstrumentMaintenanceRecord& record) = 0;
    
    /**
     * @brief 获取器械维护历史
     * @param instrumentId 器械ID
     * @return 维护记录列表
     */
    virtual QList<InstrumentMaintenanceRecord> getMaintenanceHistory(int instrumentId) = 0;
    
    // ========== 器械与手术关联 ==========
    
    /**
     * @brief 关联器械与手术类型
     * @param instrumentId 器械ID
     * @param surgeryId 手术类型ID
     * @return 成功返回true，失败返回false
     */
    virtual bool linkInstrumentToSurgery(int instrumentId, int surgeryId) = 0;
    
    /**
     * @brief 取消器械与手术类型的关联
     * @param instrumentId 器械ID
     * @param surgeryId 手术类型ID
     * @return 成功返回true，失败返回false
     */
    virtual bool unlinkInstrumentFromSurgery(int instrumentId, int surgeryId) = 0;
    
    /**
     * @brief 获取手术类型关联的器械列表
     * @param surgeryId 手术类型ID
     * @return 器械列表
     */
    virtual QList<InstrumentItem> getInstrumentsBySurgery(int surgeryId) = 0;
    
    /**
     * @brief 获取器械关联的手术类型列表
     * @param instrumentId 器械ID
     * @return 手术类型ID列表
     */
    virtual QList<int> getSurgeriesByInstrument(int instrumentId) = 0;
    
    // ========== 统计信息 ==========
    
    /**
     * @brief 获取器械统计信息
     * @return 统计信息
     */
    virtual InstrumentStatistics getStatistics() = 0;
    
    /**
     * @brief 获取未校准器械列表
     * @return 器械列表
     */
    virtual QList<InstrumentItem> getUncalibratedInstruments() = 0;
    
    /**
     * @brief 获取需要维护的器械列表
     * @return 器械列表
     */
    virtual QList<InstrumentItem> getInstrumentsNeedingMaintenance() = 0;
    
    // ========== 数据库管理 ==========

    /**
     * @brief 初始化数据库表
     * @return 成功返回true，失败返回false
     */
    virtual bool initializeDatabase() = 0;

    /**
     * @brief 检查数据库连接状态
     * @return 已连接返回true，未连接返回false
     */
    virtual bool isDatabaseConnected() = 0;

    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息字符串
     */
    virtual QString getLastError() const = 0;

    /**
     * @brief 清理重复的器械记录（根据模型文件路径去重）
     * @return 删除的重复记录数量
     */
    virtual int cleanupDuplicateInstruments() = 0;

    /**
     * @brief 重置数据库自增ID（删除所有数据后调用，使下次插入ID从1开始）
     * @return 成功返回true，失败返回false
     */
    virtual bool resetAutoIncrement() = 0;

    /**
     * @brief 获取项目根目录路径
     * @return 项目根目录的绝对路径
     */
    virtual QString getProjectPath() const = 0;

signals:
    /**
     * @brief 器械添加信号
     * @param instrumentId 新添加的器械ID
     */
    void instrumentAdded(int instrumentId);
    
    /**
     * @brief 器械更新信号
     * @param instrumentId 更新的器械ID
     */
    void instrumentUpdated(int instrumentId);
    
    /**
     * @brief 器械删除信号
     * @param instrumentId 删除的器械ID
     */
    void instrumentDeleted(int instrumentId);
    
    /**
     * @brief 器械校准完成信号
     * @param instrumentId 校准的器械ID
     * @param rmse 校准精度（均方根误差）
     */
    void instrumentCalibrated(int instrumentId, double rmse);
    
    /**
     * @brief 数据库错误信号
     * @param errorMessage 错误消息
     */
    void databaseError(const QString& errorMessage);
};

// CTK服务接口声明（必需）
Q_DECLARE_INTERFACE(InstrumentManagementService, "com.medicalpro.InstrumentManagementService")

#endif // INSTRUMENT_MANAGEMENT_SERVICE_H

