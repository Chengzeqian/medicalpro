#ifndef REGISTRATION2D3D_DATA_STRUCTURES_H
#define REGISTRATION2D3D_DATA_STRUCTURES_H

/**
 * @file Registration2D3DDataStructures.h
 * @brief 2D3D配准插件的数据结构定义
 * 
 * 定义配准相关的参数、结果和状态数据结构
 */

#include <QString>
#include <QVector>
#include <QDateTime>
#include <QVariant>

/**
 * @brief 配准参数
 * 包含配准所需的所有输入参数
 */
struct Registration2D3DParameters {
    // 文件路径
    QString ctPath;              // CT图像路径
    QString xrayApPath;          // AP视角X射线图像路径
    QString xrayLatPath;         // LAT视角X射线图像路径
    QString jingguPath;          // 胫骨模型路径（用于边缘叠加验证）
    
    // 初始配准参数 [rx, ry, rz, tx, ty, tz]
    QVector<double> initParams;  // 初始参数（6个）
    QVector<int> searchRange;    // 搜索范围（6个）
    
    // 优化参数
    int kdTreeNum;               // K-d树划分数量（默认50）
    
    // 翻转标志（用于图像预处理）
    bool apUpDown;               // AP视角上下翻转
    bool apHorizontal;           // AP视角水平翻转
    bool latUpDown;              // LAT视角上下翻转
    bool latHorizontal;          // LAT视角水平翻转
    
    // 输出设置
    bool generateDRR;            // 是否生成验证DRR图像
    QString outputDirectory;     // 输出目录（保存验证图像）
    
    Registration2D3DParameters()
        : kdTreeNum(50)
        , apUpDown(false)
        , apHorizontal(false)
        , latUpDown(false)
        , latHorizontal(false)
        , generateDRR(true)
    {
        // 默认初始参数 [0, 0, 0, 0, 0, 0]
        initParams = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        // 默认搜索范围
        searchRange = {15, 15, 15, 50, 50, 50};
    }
    
    bool isValid() const {
        return !ctPath.isEmpty() && 
               !xrayApPath.isEmpty() && 
               !xrayLatPath.isEmpty() &&
               initParams.size() == 6 &&
               searchRange.size() == 6;
    }
};

/**
 * @brief 单视角配准结果
 */
struct SingleViewRegistrationResult {
    QString viewName;            // 视角名称（"AP" 或 "LAT"）
    
    // 配准参数
    double rx;                   // X轴旋转角度（度）
    double ry;                   // Y轴旋转角度（度）
    double rz;                   // Z轴旋转角度（度）
    double tx;                   // X轴平移（mm）
    double ty;                   // Y轴平移（mm）
    double tz;                   // Z轴平移（mm）
    
    // 度量值
    double goMetric;             // GO（梯度方向）度量值
    int iterationCount;          // 迭代次数
    
    // 验证图像路径
    QString drrImagePath;        // DRR图像
    QString checkerboardPath;    // 棋盘格验证图像
    QString edgeOverlayPath;     // 边缘叠加图像
    
    SingleViewRegistrationResult()
        : rx(0.0), ry(0.0), rz(0.0)
        , tx(0.0), ty(0.0), tz(0.0)
        , goMetric(0.0)
        , iterationCount(0)
    {
    }
    
    QVector<double> toVector() const {
        return {rx, ry, rz, tx, ty, tz};
    }
};

/**
 * @brief 2D3D配准完整结果
 */
struct Registration2D3DResult {
    // 基本信息
    QString registrationId;      // 配准ID（唯一标识）
    QDateTime startTime;         // 开始时间
    QDateTime endTime;           // 结束时间
    int durationSeconds;         // 耗时（秒）
    
    // 配准状态
    enum Status {
        NotStarted,              // 未开始
        Running,                 // 运行中
        Completed,               // 完成
        Failed,                  // 失败
        Cancelled                // 取消
    };
    Status status;
    QString errorMessage;        // 错误消息（如果失败）
    
    // 配准参数
    Registration2D3DParameters parameters;
    
    // 配准结果
    SingleViewRegistrationResult apResult;   // AP视角结果
    SingleViewRegistrationResult latResult;  // LAT视角结果
    
    // 统计信息
    int totalIterations;         // 总迭代次数
    double finalMetric;          // 最终度量值（AP和LAT的平均值）
    
    Registration2D3DResult()
        : durationSeconds(0)
        , status(NotStarted)
        , totalIterations(0)
        , finalMetric(0.0)
    {
    }
    
    bool isSuccess() const {
        return status == Completed;
    }
    
    QString getStatusString() const {
        switch (status) {
            case NotStarted: return "未开始";
            case Running: return "运行中";
            case Completed: return "完成";
            case Failed: return "失败";
            case Cancelled: return "取消";
            default: return "未知";
        }
    }
};

// 声明为Qt元类型，支持跨线程信号槽传递
Q_DECLARE_METATYPE(Registration2D3DResult)

/**
 * @brief 配准进度信息
 */
struct Registration2D3DProgress {
    QString currentView;         // 当前处理的视角（"AP" 或 "LAT"）
    QString currentPhase;        // 当前阶段（"初始化"/"空间划分"/"优化"/"精细优化"/"生成验证图像"）
    int percentage;              // 进度百分比（0-100）
    QString message;             // 进度消息
    
    Registration2D3DProgress()
        : percentage(0)
    {
    }
};

// 声明为Qt元类型，支持跨线程信号槽传递
Q_DECLARE_METATYPE(Registration2D3DProgress)

/**
 * @brief 配准统计信息
 */
struct Registration2D3DStatistics {
    int totalRegistrations;      // 总配准次数
    int successfulRegistrations; // 成功次数
    int failedRegistrations;     // 失败次数
    double averageDuration;      // 平均耗时（秒）
    double averageMetric;        // 平均度量值

    Registration2D3DStatistics()
        : totalRegistrations(0)
        , successfulRegistrations(0)
        , failedRegistrations(0)
        , averageDuration(0.0)
        , averageMetric(0.0)
    {
    }
};

#endif // REGISTRATION2D3D_DATA_STRUCTURES_H

