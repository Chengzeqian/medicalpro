#ifndef MEDICAL_DATA_STRUCTURES_H
#define MEDICAL_DATA_STRUCTURES_H

/**
 * @file MedicalDataStructures.h
 * @brief 医疗系统核心数据结构定义
 * 
 * 本文件定义了整个医疗系统中共享的核心数据结构，
 * 所有CTK插件和UI模块都可以使用这些数据结构，
 * 避免了插件之间的直接依赖。
 * 
 * @author MedicalPro Team
 * @date 2024
 */

#include <QString>
#include <QDateTime>
#include <QVector3D>
#include <QMetaType>

/**
 * @brief 医疗器械数据结构（完整版）
 * 
 * 用于器械管理、手术规划、光学追踪等多个模块
 */
struct InstrumentItem {
    // ========== 基础信息 ==========
    int id;                     // 主键（数据库ID）
    QString name;               // 器械名称（如 "Probe_Lung"）
    QString model;              // 型号/规格
    QString category;           // 类别（如 "探针", "电刀", "超声探头"）
    QString serial;             // 序列号/资产编号
    QString status;             // 状态：在库/使用中/维护/报废
    QString description;        // 备注说明
    bool isActive;              // 是否有效
    QDateTime createdAt;        // 创建时间
    QDateTime updatedAt;        // 更新时间
    
    // ========== 3D可视化相关 ==========
    QString modelFilePath;      // 3D模型文件路径（.stl/.obj/.vtk）
    QString thumbnailPath;      // 缩略图路径（用于UI显示）
    QString displayColor;       // 显示颜色（RGB格式："255,215,0"）
    double modelScale;          // 模型缩放比例（默认1.0）
    
    // ========== 光学追踪相关 ==========
    QString trackingMarkerId;   // 关联的光学追踪标记ID（如 "10", "40"）
    QString geometryFilePath;   // 追踪标记几何文件路径（.ini）
    bool isCalibrated;          // 是否已校准
    QDateTime calibrationTime;  // 校准时间
    double tipOffsetX;          // 工具尖端偏移X（相对于标记中心，mm）
    double tipOffsetY;          // 工具尖端偏移Y
    double tipOffsetZ;          // 工具尖端偏移Z
    
    // ========== 构造函数 ==========
    InstrumentItem()
        : id(-1)
        , modelScale(1.0)
        , isCalibrated(false)
        , isActive(true)
        , tipOffsetX(0.0)
        , tipOffsetY(0.0)
        , tipOffsetZ(0.0)
        , displayColor("255,215,0")  // 默认金黄色
    {}
    
    // ========== 辅助方法 ==========
    bool isValid() const {
        return id >= 0 && !name.isEmpty();
    }
    
    bool hasModel() const {
        return !modelFilePath.isEmpty();
    }
    
    bool hasTracking() const {
        return !trackingMarkerId.isEmpty() && !geometryFilePath.isEmpty();
    }
    
    bool hasThumbnail() const {
        return !thumbnailPath.isEmpty();
    }
    
    // 获取尖端偏移向量
    QVector3D getTipOffset() const {
        return QVector3D(tipOffsetX, tipOffsetY, tipOffsetZ);
    }
    
    // 设置尖端偏移向量
    void setTipOffset(const QVector3D& offset) {
        tipOffsetX = offset.x();
        tipOffsetY = offset.y();
        tipOffsetZ = offset.z();
    }
};

/**
 * @brief 光学追踪标记几何信息
 * 
 * 从.ini文件解析的标记点几何数据
 */
struct MarkerGeometry {
    QString geometryId;         // 几何ID（如 "geometry10"）
    QString filePath;           // 几何文件路径
    int fiducialCount;          // 标记点数量
    QList<QVector3D> fiducialPositions; // 标记点三维坐标列表
    QDateTime loadTime;         // 加载时间
    
    MarkerGeometry() : fiducialCount(0) {}
    
    bool isValid() const {
        return !geometryId.isEmpty() && fiducialCount >= 3 && fiducialPositions.size() == fiducialCount;
    }
    
    int markerCount() const {
        return fiducialCount;
    }
};

/**
 * @brief 器械统计信息
 * 
 * 用于UI显示统计数据
 */
struct InstrumentStatistics {
    int totalInstruments;        // 总器械数
    int activeInstruments;       // 在用器械数
    int availableInstruments;    // 可用器械数（状态=在库）
    int inUseInstruments;        // 使用中器械数
    int maintenanceInstruments;  // 维护中器械数
    int retiredInstruments;      // 报废器械数
    int calibratedInstruments;   // 已校准器械数
    int withModels;              // 有3D模型的器械数
    int withTracking;            // 有追踪配置的器械数
    int totalUsageTimes;         // 总使用次数
    double averageUsageTime;     // 平均使用时间（分钟）
    
    InstrumentStatistics()
        : totalInstruments(0)
        , activeInstruments(0)
        , availableInstruments(0)
        , inUseInstruments(0)
        , maintenanceInstruments(0)
        , retiredInstruments(0)
        , calibratedInstruments(0)
        , withModels(0)
        , withTracking(0)
        , totalUsageTimes(0)
        , averageUsageTime(0.0)
    {}
};

// 注册Qt元类型，支持信号槽传递
Q_DECLARE_METATYPE(InstrumentItem)
Q_DECLARE_METATYPE(MarkerGeometry)
Q_DECLARE_METATYPE(InstrumentStatistics)

#endif // MEDICAL_DATA_STRUCTURES_H

