#ifndef INSTRUMENT_DATA_STRUCTURES_H
#define INSTRUMENT_DATA_STRUCTURES_H

/**
 * @file InstrumentDataStructures.h
 * @brief 器械管理插件的扩展数据结构
 * 
 * 注意：基础的 InstrumentItem 定义在 Framework/Core/MedicalDataStructures.h
 * 这里只定义插件特有的扩展结构（如实时追踪状态、校准数据等）
 */

// 引入核心框架的共享数据结构
#include "../../Framework/Core/MedicalDataStructures.h"

#include <QMatrix4x4>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief 器械实时追踪状态（内存数据，不存数据库）
 */
struct InstrumentTrackingState {
    QString instrumentId;        // 器械ID（对应InstrumentItem.id的字符串形式）
    QVector3D position;          // 当前位置（世界坐标系）
    QVector3D rotation;          // 当前旋转（欧拉角，度）
    QMatrix4x4 transformMatrix;  // 4x4变换矩阵
    bool isVisible;              // 是否被追踪到
    double trackingQuality;      // 追踪质量（0.0-1.0）
    QDateTime lastUpdateTime;    // 最后更新时间
    QString statusMessage;       // 状态消息
    
    InstrumentTrackingState()
        : isVisible(false), trackingQuality(0.0)
    {
    }
    
    bool isValid() const { return !instrumentId.isEmpty(); }
};

/**
 * @brief 器械校准数据
 */
struct InstrumentCalibrationData {
    int instrumentId;            // 器械ID
    QString calibrationType;     // 校准类型："pivot"（针尖校准）、"plane"（平面校准）
    QDateTime calibrationTime;   // 校准时间
    QVector3D tipOffset;         // 尖端偏移
    double rmse;                 // 均方根误差（精度指标）
    int pointCount;              // 校准点数量
    QList<QVector3D> calibrationPoints; // 校准点集合
    QString calibrationNote;     // 校准备注
    bool isValid;                // 校准是否有效
    
    InstrumentCalibrationData()
        : instrumentId(-1), rmse(0.0), pointCount(0), isValid(false)
    {
    }
};

// 注意：MarkerGeometry 已移至 Framework/Core/MedicalDataStructures.h
// 这里不再重复定义

/**
 * @brief 器械使用记录
 */
struct InstrumentUsageRecord {
    int id;                      // 记录ID
    int instrumentId;            // 器械ID
    int surgeryId;               // 手术ID
    int patientId;               // 病人ID
    QDateTime startTime;         // 开始使用时间
    QDateTime endTime;           // 结束使用时间
    int durationMinutes;         // 使用时长（分钟）
    QString usageNote;           // 使用备注
    
    InstrumentUsageRecord()
        : id(-1), instrumentId(-1), surgeryId(-1), patientId(-1), durationMinutes(0)
    {
    }
    
    bool isValid() const { return id >= 0 && instrumentId >= 0; }
};

/**
 * @brief 器械维护记录
 */
struct InstrumentMaintenanceRecord {
    int id;                      // 记录ID
    int instrumentId;            // 器械ID
    QString maintenanceType;     // 维护类型：清洁/消毒/维修/校准/检测
    QDateTime maintenanceTime;   // 维护时间
    QString performedBy;         // 执行人
    QString description;         // 维护描述
    QString result;              // 维护结果：合格/不合格/待复检
    double cost;                 // 费用
    
    InstrumentMaintenanceRecord()
        : id(-1), instrumentId(-1), cost(0.0)
    {
    }
    
    bool isValid() const { return id >= 0 && instrumentId >= 0; }
};

// 注意：InstrumentStatistics 已移至 Framework/Core/MedicalDataStructures.h
// 这里不再重复定义

#endif // INSTRUMENT_DATA_STRUCTURES_H

