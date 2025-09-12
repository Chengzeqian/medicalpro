#ifndef OPTICAL_TRACKING_SERVICE_H
#define OPTICAL_TRACKING_SERVICE_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <QMetaType>

/**
 * @brief Optical Tracking Service Interface (完全CTK架构)
 * 
 * 提供光学跟踪系统的标准接口，采用完全CTK架构设计：
 * - 支持多种光学跟踪设备
 * - 所有操作通过CTK服务接口完成
 * - 不直接依赖硬件驱动或外部库
 * - 支持实时跟踪、校准和数据记录
 * - 设备状态管理和配置
 * 
 * 核心设计原则：
 * 1. 设备：设备ID (QString)
 * 2. 跟踪：跟踪会话ID (QString)
 * 3. 配置：设备参数 (QVariantMap)
 * 4. 通信：完全通过CTK服务框架
 */
class OpticalTrackingService : public QObject
{
    Q_OBJECT

public:
    explicit OpticalTrackingService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~OpticalTrackingService() = default;

    // ==================== 设备管理 ====================
    
    /**
     * @brief 扫描可用的光学跟踪设备
     * @return 设备ID列表
     */
    virtual QStringList scanAvailableDevices() = 0;
    
    /**
     * @brief 连接到指定设备
     * @param deviceId 设备ID
     * @return 成功返回true，失败返回false
     */
    virtual bool connectToDevice(const QString& deviceId) = 0;
    
    /**
     * @brief 断开设备连接
     * @param deviceId 设备ID
     * @return 成功返回true，失败返回false
     */
    virtual bool disconnectDevice(const QString& deviceId) = 0;
    
    /**
     * @brief 检查设备连接状态
     * @param deviceId 设备ID
     * @return 是否已连接
     */
    virtual bool isDeviceConnected(const QString& deviceId) const = 0;
    
    /**
     * @brief 获取设备信息
     * @param deviceId 设备ID
     * @return 设备信息映射
     */
    virtual QVariantMap getDeviceInfo(const QString& deviceId) const = 0;
    
    /**
     * @brief 获取所有已连接的设备
     * @return 设备ID列表
     */
    virtual QStringList getConnectedDevices() const = 0;
    
    /**
     * @brief 设置设备参数
     * @param deviceId 设备ID
     * @param parameters 参数映射
     * @return 成功返回true，失败返回false
     */
    virtual bool setDeviceParameters(const QString& deviceId, const QVariantMap& parameters) = 0;
    
    /**
     * @brief 获取设备参数
     * @param deviceId 设备ID
     * @return 参数映射
     */
    virtual QVariantMap getDeviceParameters(const QString& deviceId) const = 0;

    // ==================== 跟踪会话管理 ====================
    
    /**
     * @brief 创建跟踪会话
     * @param deviceId 设备ID
     * @param sessionName 会话名称
     * @return 会话ID，失败返回空字符串
     */
    virtual QString createTrackingSession(const QString& deviceId, const QString& sessionName) = 0;
    
    /**
     * @brief 开始跟踪
     * @param sessionId 会话ID
     * @return 成功返回true，失败返回false
     */
    virtual bool startTracking(const QString& sessionId) = 0;
    
    /**
     * @brief 停止跟踪
     * @param sessionId 会话ID
     * @return 成功返回true，失败返回false
     */
    virtual bool stopTracking(const QString& sessionId) = 0;
    
    /**
     * @brief 暂停/恢复跟踪
     * @param sessionId 会话ID
     * @param paused 是否暂停
     * @return 成功返回true，失败返回false
     */
    virtual bool pauseTracking(const QString& sessionId, bool paused) = 0;
    
    /**
     * @brief 检查跟踪状态
     * @param sessionId 会话ID
     * @return 跟踪状态字符串 ("stopped", "running", "paused", "error")
     */
    virtual QString getTrackingStatus(const QString& sessionId) const = 0;
    
    /**
     * @brief 关闭跟踪会话
     * @param sessionId 会话ID
     * @return 成功返回true，失败返回false
     */
    virtual bool closeTrackingSession(const QString& sessionId) = 0;
    
    /**
     * @brief 获取所有活动会话
     * @return 会话ID列表
     */
    virtual QStringList getActiveSessions() const = 0;
    
    /**
     * @brief 获取会话信息
     * @param sessionId 会话ID
     * @return 会话信息映射
     */
    virtual QVariantMap getSessionInfo(const QString& sessionId) const = 0;

    // ==================== 工具和标记管理 ====================
    
    /**
     * @brief 添加跟踪工具
     * @param sessionId 会话ID
     * @param toolName 工具名称
     * @param toolConfig 工具配置参数
     * @return 工具ID，失败返回空字符串
     */
    virtual QString addTrackingTool(const QString& sessionId, const QString& toolName, const QVariantMap& toolConfig) = 0;
    
    /**
     * @brief 移除跟踪工具
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @return 成功返回true，失败返回false
     */
    virtual bool removeTrackingTool(const QString& sessionId, const QString& toolId) = 0;
    
    /**
     * @brief 获取工具列表
     * @param sessionId 会话ID
     * @return 工具ID列表
     */
    virtual QStringList getTrackingTools(const QString& sessionId) const = 0;
    
    /**
     * @brief 获取工具当前位置
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @return 位置信息 [x, y, z, rx, ry, rz]，失败返回空列表
     */
    virtual QList<double> getToolPosition(const QString& sessionId, const QString& toolId) = 0;
    
    /**
     * @brief 获取工具状态
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @return 状态信息映射
     */
    virtual QVariantMap getToolStatus(const QString& sessionId, const QString& toolId) const = 0;
    
    /**
     * @brief 设置工具参数
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @param parameters 参数映射
     * @return 成功返回true，失败返回false
     */
    virtual bool setToolParameters(const QString& sessionId, const QString& toolId, const QVariantMap& parameters) = 0;

    // ==================== 校准功能 ====================
    
    /**
     * @brief 开始工具校准
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @param calibrationType 校准类型 ("pivot", "surface", "point")
     * @return 校准ID，失败返回空字符串
     */
    virtual QString startToolCalibration(const QString& sessionId, const QString& toolId, const QString& calibrationType) = 0;
    
    /**
     * @brief 添加校准点
     * @param calibrationId 校准ID
     * @return 成功返回true，失败返回false
     */
    virtual bool addCalibrationPoint(const QString& calibrationId) = 0;
    
    /**
     * @brief 完成校准
     * @param calibrationId 校准ID
     * @return 校准结果映射，失败返回空映射
     */
    virtual QVariantMap finishCalibration(const QString& calibrationId) = 0;
    
    /**
     * @brief 取消校准
     * @param calibrationId 校准ID
     * @return 成功返回true，失败返回false
     */
    virtual bool cancelCalibration(const QString& calibrationId) = 0;
    
    /**
     * @brief 获取校准状态
     * @param calibrationId 校准ID
     * @return 校准状态信息
     */
    virtual QVariantMap getCalibrationStatus(const QString& calibrationId) const = 0;
    
    /**
     * @brief 应用校准结果
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @param calibrationResult 校准结果
     * @return 成功返回true，失败返回false
     */
    virtual bool applyCalibrationResult(const QString& sessionId, const QString& toolId, const QVariantMap& calibrationResult) = 0;

    // ==================== 数据记录和回放 ====================
    
    /**
     * @brief 开始数据记录
     * @param sessionId 会话ID
     * @param recordingName 记录名称
     * @param filePath 保存路径
     * @return 记录ID，失败返回空字符串
     */
    virtual QString startDataRecording(const QString& sessionId, const QString& recordingName, const QString& filePath) = 0;
    
    /**
     * @brief 停止数据记录
     * @param recordingId 记录ID
     * @return 成功返回true，失败返回false
     */
    virtual bool stopDataRecording(const QString& recordingId) = 0;
    
    /**
     * @brief 暂停/恢复数据记录
     * @param recordingId 记录ID
     * @param paused 是否暂停
     * @return 成功返回true，失败返回false
     */
    virtual bool pauseDataRecording(const QString& recordingId, bool paused) = 0;
    
    /**
     * @brief 获取记录状态
     * @param recordingId 记录ID
     * @return 记录状态信息
     */
    virtual QVariantMap getRecordingStatus(const QString& recordingId) const = 0;
    
    /**
     * @brief 加载记录数据
     * @param filePath 文件路径
     * @return 回放会话ID，失败返回空字符串
     */
    virtual QString loadRecordedData(const QString& filePath) = 0;
    
    /**
     * @brief 回放记录数据
     * @param playbackId 回放ID
     * @param timestamp 时间戳（毫秒）
     * @return 成功返回true，失败返回false
     */
    virtual bool playbackData(const QString& playbackId, qint64 timestamp) = 0;

    // ==================== 坐标系统和变换 ====================
    
    /**
     * @brief 设置参考坐标系
     * @param sessionId 会话ID
     * @param referenceToolId 参考工具ID
     * @return 成功返回true，失败返回false
     */
    virtual bool setReferenceCoordinateSystem(const QString& sessionId, const QString& referenceToolId) = 0;
    
    /**
     * @brief 获取坐标变换矩阵
     * @param sessionId 会话ID
     * @param fromToolId 源工具ID
     * @param toToolId 目标工具ID
     * @return 4x4变换矩阵（16个元素），失败返回空列表
     */
    virtual QList<double> getTransformMatrix(const QString& sessionId, const QString& fromToolId, const QString& toToolId) = 0;
    
    /**
     * @brief 转换坐标点
     * @param sessionId 会话ID
     * @param point 输入点 [x, y, z]
     * @param fromToolId 源坐标系工具ID
     * @param toToolId 目标坐标系工具ID
     * @return 转换后的点 [x, y, z]，失败返回空列表
     */
    virtual QList<double> transformPoint(const QString& sessionId, const QList<double>& point, 
                                       const QString& fromToolId, const QString& toToolId) = 0;

    // ==================== 实时数据流 ====================
    
    /**
     * @brief 启用实时数据流
     * @param sessionId 会话ID
     * @param frequency 数据频率（Hz）
     * @return 成功返回true，失败返回false
     */
    virtual bool enableRealTimeStreaming(const QString& sessionId, double frequency) = 0;
    
    /**
     * @brief 禁用实时数据流
     * @param sessionId 会话ID
     * @return 成功返回true，失败返回false
     */
    virtual bool disableRealTimeStreaming(const QString& sessionId) = 0;
    
    /**
     * @brief 获取实时数据
     * @param sessionId 会话ID
     * @return 所有工具的当前位置数据
     */
    virtual QMap<QString, QList<double>> getRealTimeData(const QString& sessionId) = 0;

    // ==================== 质量控制和验证 ====================
    
    /**
     * @brief 检查跟踪质量
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @return 质量指标映射
     */
    virtual QVariantMap checkTrackingQuality(const QString& sessionId, const QString& toolId) = 0;
    
    /**
     * @brief 验证工具精度
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @param referencePoints 参考点列表
     * @return 精度验证结果
     */
    virtual QVariantMap validateToolAccuracy(const QString& sessionId, const QString& toolId, const QList<QList<double>>& referencePoints) = 0;
    
    /**
     * @brief 获取系统状态报告
     * @param sessionId 会话ID
     * @return 状态报告映射
     */
    virtual QVariantMap getSystemStatusReport(const QString& sessionId) = 0;

    // ==================== UI显示管理（遵循PatientManagement成功模式） ====================
    
    /**
     * @brief 显示跟踪控制面板
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showTrackingControlPanel(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示设备配置对话框
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showDeviceConfigDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示校准向导对话框
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showCalibrationWizardDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示数据记录对话框
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showDataRecordingDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 创建追踪控制界面
     * @param parent 父窗口（可选）
     * @return 控制界面Widget指针，失败返回nullptr
     */
    virtual QWidget* createTrackingControlInterface(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 导出记录数据
     * @param recordingId 记录ID
     * @param exportPath 导出路径
     * @param exportFormat 导出格式
     * @return 导出ID，失败返回空字符串
     */
    virtual QString exportRecordingData(const QString& recordingId, const QString& exportPath, const QString& exportFormat) = 0;

signals:
    /**
     * @brief 设备连接状态变化信号
     * @param deviceId 设备ID
     * @param connected 是否已连接
     */
    void deviceConnectionChanged(const QString& deviceId, bool connected);
    
    /**
     * @brief 跟踪状态变化信号
     * @param sessionId 会话ID
     * @param status 新状态
     */
    void trackingStatusChanged(const QString& sessionId, const QString& status);
    
    /**
     * @brief 工具位置更新信号
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @param position 位置信息 [x, y, z, rx, ry, rz]
     * @param timestamp 时间戳
     */
    void toolPositionUpdated(const QString& sessionId, const QString& toolId, 
                           const QList<double>& position, qint64 timestamp);
    
    /**
     * @brief 工具状态变化信号
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @param status 状态信息
     */
    void toolStatusChanged(const QString& sessionId, const QString& toolId, const QVariantMap& status);
    
    /**
     * @brief 校准进度信号
     * @param calibrationId 校准ID
     * @param progress 进度百分比
     * @param message 进度消息
     */
    void calibrationProgress(const QString& calibrationId, int progress, const QString& message);
    
    /**
     * @brief 校准完成信号
     * @param calibrationId 校准ID
     * @param result 校准结果
     */
    void calibrationCompleted(const QString& calibrationId, const QVariantMap& result);
    
    /**
     * @brief 数据记录状态变化信号
     * @param recordingId 记录ID
     * @param status 记录状态
     */
    void recordingStatusChanged(const QString& recordingId, const QString& status);
    
    /**
     * @brief 跟踪质量警告信号
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @param quality 质量指标
     * @param warning 警告信息
     */
    void trackingQualityWarning(const QString& sessionId, const QString& toolId, 
                              const QVariantMap& quality, const QString& warning);
    
    /**
     * @brief 错误信号
     * @param sessionId 会话ID
     * @param error 错误信息
     */
    void trackingError(const QString& sessionId, const QString& error);
    
    /**
     * @brief 追踪界面创建信号
     * @param interfaceId 界面ID
     */
    void trackingInterfaceCreated(const QString& interfaceId);
    
    /**
     * @brief 界面错误信号
     * @param error 错误信息
     */
    void interfaceError(const QString& error);
    
    /**
     * @brief 回放数据可用信号
     * @param playbackId 回放ID
     * @param frameData 帧数据
     */
    void playbackDataAvailable(const QString& playbackId, const QVariantMap& frameData);
    
    /**
     * @brief 导出完成信号
     * @param exportId 导出ID
     * @param exportPath 导出路径
     */
    void exportCompleted(const QString& exportId, const QString& exportPath);
    
    /**
     * @brief 导出失败信号
     * @param exportId 导出ID
     * @param error 错误信息
     */
    void exportFailed(const QString& exportId, const QString& error);
    
    /**
     * @brief 设备连接进度信号
     * @param deviceId 设备ID
     * @param progress 进度百分比
     */
    void deviceConnectionProgress(const QString& deviceId, int progress);
    
    /**
     * @brief 设备错误信号
     * @param deviceId 设备ID
     * @param error 错误信息
     */
    void deviceError(const QString& deviceId, const QString& error);
    
    /**
     * @brief 工具检测信号
     * @param sessionId 会话ID
     * @param toolId 工具ID
     * @param toolProperties 工具属性
     */
    void toolDetected(const QString& sessionId, const QString& toolId, const QVariantMap& toolProperties);
    
    /**
     * @brief 校准开始信号
     * @param calibrationId 校准ID
     * @param sessionId 会话ID
     * @param toolId 工具ID
     */
    void calibrationStarted(const QString& calibrationId, const QString& sessionId, const QString& toolId);
    
    /**
     * @brief 校准点添加信号
     * @param calibrationId 校准ID
     * @param pointCount 当前点数
     * @param requiredPoints 所需点数
     */
    void calibrationPointAdded(const QString& calibrationId, int pointCount, int requiredPoints);
};

// Qt接口声明
Q_DECLARE_INTERFACE(OpticalTrackingService, "medical.OpticalTrackingService")

#endif // OPTICAL_TRACKING_SERVICE_H
