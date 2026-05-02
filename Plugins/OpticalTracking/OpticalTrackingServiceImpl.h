#ifndef OPTICAL_TRACKING_SERVICE_IMPL_H
#define OPTICAL_TRACKING_SERVICE_IMPL_H

#include "OpticalTrackingService.h"
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QVariant>
#include <QUuid>
#include <QTimer>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QDateTime>
#include <QThread>
#include <QUdpSocket>
#include <QHostAddress>
#include <QRadioButton>
#include <QLibrary>
#include <cstdint>
#include <fstream>
#include <random>

// 必要的Qt包含（在SDK定义之前）
#include <QString>
#include <QList>

// Atracsys SDK - 条件编译支持
#ifdef ATRACSYS_SDK_AVAILABLE
#include <ftkInterface.h>
#else
// 模拟SDK类型定义
typedef void* ftkLibrary;
typedef struct { char data[256]; } ftkBuffer;
typedef enum { FTK_OK = 0, FTK_ERROR = 1 } ftkError;
typedef int ftkDeviceType;
typedef uint64_t uint64;

// 模拟刚体结构定义
struct ftkRigidBody {
    uint64 id;
    QString name;
    QList<double> geometry;
    bool active;
    
    ftkRigidBody() : id(0), active(false) {}
};
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// CTK框架

class PlatformServiceRegistry;

// 前向声明（遵循完全CTK架构）
class UnifiedMedicalImageService;
class ImageInteractionService;

/**
 * @brief Optical Tracking Service Implementation (完全CTK架构)
 * 
 * OpticalTrackingService接口的具体实现，采用完全CTK架构设计：
 * - 通过CTK服务框架进行可选的图像系统集成
 * - 专注于光学跟踪设备管理和数据处理
 * - 支持多种跟踪设备和校准功能
 * - 提供实时数据流和记录回放
 * - 完全解耦的插件间通信
 */
class OpticalTrackingServiceImpl : public OpticalTrackingService
{
    Q_OBJECT
    Q_INTERFACES(OpticalTrackingService)
    friend class OpticalTrackingProbeCalibrationLoadSmokeTest;
    friend class OpticalTrackingProbeCalibrationInitializationTest;
    friend class OpticalTrackingProbeCalibrationToolGeometrySelectionTest;

public:
    /**
     * @brief 构造函数
     * @param context CTK插件上下文
     * @param parent 父对象
     */
    explicit OpticalTrackingServiceImpl(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~OpticalTrackingServiceImpl() override;
    
    /**
     * @brief 设置CTK插件上下文（关键方法，遵循PatientManagement成功模式）
     * @param context CTK插件上下文
     */
    void setServiceRegistry(PlatformServiceRegistry* serviceRegistry);

    // ==================== 设备管理实现 ====================
    
    QStringList scanAvailableDevices() override;
    bool connectToDevice(const QString& deviceId) override;
    bool disconnectDevice(const QString& deviceId) override;
    bool isDeviceConnected(const QString& deviceId) const override;
    QVariantMap getDeviceInfo(const QString& deviceId) const override;
    QStringList getConnectedDevices() const override;
    bool setDeviceParameters(const QString& deviceId, const QVariantMap& parameters) override;
    QVariantMap getDeviceParameters(const QString& deviceId) const override;

    // ==================== 跟踪会话管理实现 ====================
    
    QString createTrackingSession(const QString& deviceId, const QString& sessionName) override;
    bool startTracking(const QString& sessionId) override;
    bool stopTracking(const QString& sessionId) override;
    bool pauseTracking(const QString& sessionId, bool paused) override;
    QString getTrackingStatus(const QString& sessionId) const override;
    bool closeTrackingSession(const QString& sessionId) override;
    QStringList getActiveSessions() const override;
    QVariantMap getSessionInfo(const QString& sessionId) const override;

    // ==================== 工具和标记管理实现 ====================
    
    QString addTrackingTool(const QString& sessionId, const QString& toolName, const QVariantMap& toolConfig) override;
    bool removeTrackingTool(const QString& sessionId, const QString& toolId) override;
    QStringList getTrackingTools(const QString& sessionId) const override;
    QList<double> getToolPosition(const QString& sessionId, const QString& toolId) override;
    QVariantMap getToolStatus(const QString& sessionId, const QString& toolId) const override;
    bool setToolParameters(const QString& sessionId, const QString& toolId, const QVariantMap& parameters) override;

    // ==================== 校准功能实现 ====================
    
    QString startToolCalibration(const QString& sessionId, const QString& toolId, const QString& calibrationType) override;
    bool addCalibrationPoint(const QString& calibrationId) override;
    QVariantMap finishCalibration(const QString& calibrationId) override;
    bool cancelCalibration(const QString& calibrationId) override;
    QVariantMap getCalibrationStatus(const QString& calibrationId) const override;
    bool applyCalibrationResult(const QString& sessionId, const QString& toolId, const QVariantMap& calibrationResult) override;

    // ==================== 数据记录和回放实现 ====================
    
    QString startDataRecording(const QString& sessionId, const QString& recordingName, const QString& filePath) override;
    bool stopDataRecording(const QString& recordingId) override;
    bool pauseDataRecording(const QString& recordingId, bool paused) override;
    QVariantMap getRecordingStatus(const QString& recordingId) const override;
    QString loadRecordedData(const QString& filePath) override;
    bool playbackData(const QString& playbackId, qint64 timestamp) override;
    QString exportRecordingData(const QString& recordingId, const QString& exportPath, const QString& exportFormat) override;

    // ==================== 坐标系统和变换实现 ====================
    
    bool setReferenceCoordinateSystem(const QString& sessionId, const QString& referenceToolId) override;
    QList<double> getTransformMatrix(const QString& sessionId, const QString& fromToolId, const QString& toToolId) override;
    QList<double> transformPoint(const QString& sessionId, const QList<double>& point, 
                               const QString& fromToolId, const QString& toToolId) override;

    // ==================== 实时数据流实现 ====================
    
    bool enableRealTimeStreaming(const QString& sessionId, double frequency) override;
    bool disableRealTimeStreaming(const QString& sessionId) override;
    QMap<QString, QList<double>> getRealTimeData(const QString& sessionId) override;

    // ==================== 质量控制和验证实现 ====================
    
    QVariantMap checkTrackingQuality(const QString& sessionId, const QString& toolId) override;
    QVariantMap validateToolAccuracy(const QString& sessionId, const QString& toolId, const QList<QList<double>>& referencePoints) override;
    QVariantMap getSystemStatusReport(const QString& sessionId) override;

    // ==================== VTK渲染控制 ====================

    void pauseRendering() override;
    void resumeRendering() override;

    // ==================== 错误处理 ====================

    QString getLastError() const override;

    // ==================== UI显示管理（遵循PatientManagement成功模式） ====================
    
    QWidget* createTrackingWidget(QWidget* parent = nullptr) override;
    bool showTrackingControlPanel(QWidget* parent = nullptr) override;
    bool showDeviceConfigDialog(QWidget* parent = nullptr) override;
    bool showCalibrationWizardDialog(QWidget* parent = nullptr) override;
    bool showDataRecordingDialog(QWidget* parent = nullptr) override;
    
    // ==================== 服务管理方法 ====================
    
    /**
     * @brief 启动服务
     */
    void startService();
    
    /**
     * @brief 停止服务
     */
    void stopService();
    
    /**
     * @brief 获取服务名称
     */
    QString getServiceName() const;
    
    /**
     * @brief 获取服务版本
     */
    QString getServiceVersion() const;

private slots:
    /**
     * @brief 处理实时数据更新定时器
     */
    void onRealTimeDataUpdate();
    
    /**
     * @brief 处理服务可用性变化
     * @param available 服务是否可用
     */
    void onImageServiceAvailabilityChanged(bool available);

private:
    /**
     * @brief 设备信息结构
     */
    struct DeviceInfo {
        QString deviceId;
        QString deviceType;
        QString deviceName;
        bool connected;
        QVariantMap parameters;
        QMap<QString, QVariant> state;
        
        DeviceInfo() : connected(false) {}
    };
    
    /**
     * @brief 跟踪会话信息结构
     */
    struct SessionInfo {
        QString sessionId;
        QString sessionName;
        QString deviceId;
        QString status; // "stopped", "running", "paused", "error"
        QMap<QString, QVariantMap> tools;
        QStringList toolIds;
        QMap<QString, QVariantMap> toolConfigurations;
        QString referenceToolId;
        qint64 startTime;
        QVariantMap parameters;
        
        SessionInfo() : startTime(0) {}
    };
    
    /**
     * @brief 校准信息结构
     */
    struct CalibrationInfo {
        QString calibrationId;
        QString sessionId;
        QString toolId;
        QString calibrationType;
        QList<QList<double>> points;
        QList<QList<double>> calibrationPoints;
        QList<qint64> timeStamps;
        int pointCount;
        int requiredPoints;
        qint64 startTime;
        qint64 endTime;
        QString status; // "active", "completed", "cancelled"
        QVariantMap result;
        QVariantMap parameters; // 校准参数
        
        CalibrationInfo() : pointCount(0), requiredPoints(10), startTime(0), endTime(0) {}
    };
    
    /**
     * @brief 记录信息结构
     */
    struct RecordingInfo {
        QString recordingId;
        QString sessionId;
        QString recordingName;
        QString filePath;
        QString status; // "recording", "paused", "stopped"
        qint64 startTime;
        qint64 endTime;
        qint64 duration;
        qint64 pauseTime;
        qint64 totalPauseTime;
        int frameCount;
        qint64 dataSize;
        QString format;
        
        RecordingInfo() : startTime(0), endTime(0), duration(0), pauseTime(0), totalPauseTime(0), frameCount(0), dataSize(0) {}
    };
    
    /**
     * @brief 回放信息结构
     */
    struct PlaybackInfo {
        QString playbackId;
        QString filePath;
        QString format;
        QString status; // "loaded", "playing", "paused", "stopped"
        int currentFrame;
        int totalFrames;
        qint64 currentTimestamp;
        QVariantMap state;
        
        PlaybackInfo() : currentFrame(0), totalFrames(0), currentTimestamp(0) {}
    };
    
    /**
     * @brief 导出信息结构
     */
    struct ExportInfo {
        QString exportId;
        QString recordingId;
        QString exportPath;
        QString exportFormat;
        QString status; // "exporting", "completed", "failed"
        qint64 startTime;
        qint64 endTime;
        int progress; // 0-100
        QVariantMap state;
        
        ExportInfo() : startTime(0), endTime(0), progress(0) {}
    };

private:
    /**
     * @brief 初始化可选的图像服务连接
     */
    void initializeOptionalServiceConnections();
    
    /**
     * @brief 验证设备ID有效性
     * @param deviceId 设备ID
     * @return 是否有效
     */
    bool validateDeviceId(const QString& deviceId) const;
    
    /**
     * @brief 验证会话ID有效性
     * @param sessionId 会话ID
     * @return 是否有效
     */
    bool validateSessionId(const QString& sessionId) const;
    
    /**
     * @brief 生成设备ID
     * @return 唯一设备ID
     */
    QString generateDeviceId() const;
    
    /**
     * @brief 生成会话ID
     * @return 唯一会话ID
     */
    QString generateSessionId() const;
    
    /**
     * @brief 生成工具ID
     * @return 唯一工具ID
     */
    QString generateToolId() const;
    
    /**
     * @brief 生成校准ID
     * @return 唯一校准ID
     */
    QString generateCalibrationId() const;
    
    /**
     * @brief 生成记录ID
     * @return 唯一记录ID
     */
    QString generateRecordingId() const;
    
    /**
     * @brief 获取设备信息（内部使用）
     * @param deviceId 设备ID
     * @return 设备信息指针
     */
    DeviceInfo* getDeviceInfoPtr(const QString& deviceId);
    
    /**
     * @brief 获取设备信息（只读，内部使用）
     * @param deviceId 设备ID
     * @return 设备信息指针
     */
    const DeviceInfo* getDeviceInfoPtr(const QString& deviceId) const;
    
    /**
     * @brief 获取会话信息（内部使用）
     * @param sessionId 会话ID
     * @return 会话信息指针
     */
    SessionInfo* getSessionInfoPtr(const QString& sessionId);
    
    /**
     * @brief 获取会话信息（只读，内部使用）
     * @param sessionId 会话ID
     * @return 会话信息指针
     */
    const SessionInfo* getSessionInfoPtr(const QString& sessionId) const;
    
    /**
     * @brief 设置错误信息
     * @param error 错误描述
     */
    void setError(const QString& error);
    
    // 校准相关的内部方法
    bool validateCalibrationId(const QString& calibrationId) const;
    CalibrationInfo* getCalibrationInfo(const QString& calibrationId);
    const CalibrationInfo* getCalibrationInfo(const QString& calibrationId) const;
    bool validatePositionQuality(const QList<double>& position) const;
    bool validatePositionQuality(const QList<double>& position, const QString& calibrationType) const;
    void updatePivotCalibrationProgress(const QString& calibrationId, int progress);
    void updatePivotCalibrationProgress(CalibrationInfo& calibrationInfo);
    void updateSurfaceCalibrationProgress(const QString& calibrationId, int progress);
    void updateSurfaceCalibrationProgress(CalibrationInfo& calibrationInfo);
    void calibrationPointAdded(const QString& calibrationId, int pointCount, int requiredPoints);
    
    // 校准算法实现
    QVariantMap performPivotCalibration(const QString& calibrationId, CalibrationInfo& calibrationInfo);
    QVariantMap performSurfaceCalibration(const QString& calibrationId, CalibrationInfo& calibrationInfo);
    QVariantMap performPointCalibration(const QString& calibrationId, CalibrationInfo& calibrationInfo);
    double calculatePivotCalibrationAccuracy(const CalibrationInfo& calibrationInfo);
    QVariantMap calculateCalibrationStatistics(const CalibrationInfo& calibrationInfo);
    
    // 数学工具方法
    QList<double> eulerToRotationMatrix(double rx, double ry, double rz);
    QList<double> transformPoint3D(const QList<double>& point, const QList<double>& matrix);
    QList<double> transformPoint3D(const QList<double>& point, const QList<double>& rotationMatrix, const QList<double>& translation);
    QList<double> solveLeastSquares(const QList<QList<double>>& A, const QList<double>& b);
    QList<double> solveLinearSystem(const QList<QList<double>>& A, const QList<double>& b);
    double calculatePlaneFitQuality(const QList<QList<double>>& points);
    
    // 校准初始化
    int getRequiredCalibrationPoints(const QString& calibrationType) const;
    int getRequiredCalibrationPoints(const QString& calibrationType);
    void initializePivotCalibration(CalibrationInfo& calibrationInfo);
    void initializeSurfaceCalibration(CalibrationInfo& calibrationInfo);
    void initializePointCalibration(CalibrationInfo& calibrationInfo);
    
    /**
     * @brief 模拟设备扫描
     * @return 发现的设备列表
     */
    QStringList simulateDeviceScan();
    
    /**
     * @brief 模拟工具位置数据
     * @param toolId 工具ID
     * @return 位置数据 [x, y, z, rx, ry, rz]
     */
    QList<double> simulateToolPosition(const QString& toolId);
    
    /**
     * @brief 计算坐标变换矩阵
     * @param fromTool 源工具信息
     * @param toTool 目标工具信息
     * @return 4x4变换矩阵
     */
    QList<double> calculateTransformMatrix(const DeviceInfo& fromTool, const DeviceInfo& toTool);
    
    // 增强的设备连接和管理方法
    bool performDeviceInitialization(const QString& deviceId);
    bool initializePolarisDevice(const QString& deviceId);
    bool initializeAuroraDevice(const QString& deviceId);
    bool initializeSimulatorDevice(const QString& deviceId);
    
    bool configureDeviceParameters(const QString& deviceId);
    void configurePolarisParameters(const QString& deviceId);
    void configureAuroraParameters(const QString& deviceId);
    void configureSimulatorParameters(const QString& deviceId);
    
    bool validateDeviceStatus(const QString& deviceId);
    bool checkHardwareConnection(const QString& deviceId);
    bool checkCalibrationStatus(const QString& deviceId);
    bool checkWorkingParameters(const QString& deviceId);
    bool performSelfTest(const QString& deviceId);
    
    void initializeToolDetection(const QString& deviceId);
    void createSimulatedTools(const QString& deviceId);
    QVariantMap getSimulatedToolProperties(const QString& toolId);
    
    QString getDeviceFirmwareVersion(const QString& deviceId);
    double getDeviceTemperature(const QString& deviceId);
    
    // 实时追踪数据采集和处理方法
    QList<double> generateRealTimeToolData(const QString& sessionId, const QString& toolId);
    QList<double> generateSimulatedTrackingData(const QString& sessionId, const QString& toolId);
    QList<double> generateProbeMotion(qint64 timestamp);
    QList<double> generatePointerMotion(qint64 timestamp);
    QList<double> generateReferenceMotion(qint64 timestamp);
    QList<double> generateDefaultMotion(qint64 timestamp);
    
    void addRealisticNoise(QList<double>& data, const QString& deviceType);
    double generateGaussianNoise(double mean, double stddev);
    bool shouldSimulateOcclusion(qint64 timestamp);
    QString getToolMotionPattern(const QString& sessionId, const QString& toolId);
    
    // 数据质量和验证
    bool validateRealTimeData(const QList<double>& data);
    double calculateDataQuality(const QList<double>& data, const QString& deviceType);
    double calculateMarkerVisibilityQuality(const QList<double>& data);
    double calculateElectromagneticQuality(const QList<double>& data);
    
    // 延迟补偿和滤波
    QList<double> applyDelayCompensation(const QString& sessionId, const QString& toolId, const QList<double>& rawData);
    QList<double> applyNoiseFiltering(const QString& sessionId, const QString& toolId, const QList<double>& rawData);
    QList<double> applyLowPassFilter(const QString& sessionId, const QString& toolId, const QList<double>& rawData);
    QList<double> applyKalmanFilter(const QString& sessionId, const QString& toolId, const QList<double>& rawData);
    QList<double> applyMedianFilter(const QString& sessionId, const QString& toolId, const QList<double>& rawData);
    
    // 设备特定数据获取
    QList<double> acquirePolarisTrackingData(const QString& sessionId, const QString& toolId);
    QList<double> acquireAuroraTrackingData(const QString& sessionId, const QString& toolId);
    
    // 辅助方法
    double getSystemDelay(const QString& sessionId);
    bool hasHistoricalData(const QString& sessionId, const QString& toolId);
    QList<double> calculateToolVelocity(const QString& sessionId, const QString& toolId);
    QString getFilterType(const QString& sessionId, const QString& toolId);
    double getLowPassFilterAlpha(const QString& sessionId, const QString& toolId);
    bool hasFilterHistory(const QString& sessionId, const QString& toolId);
    QList<double> getFilterHistory(const QString& sessionId, const QString& toolId);
    void updateFilterHistory(const QString& sessionId, const QString& toolId, const QList<double>& data);
    
    // 数据记录和导出方法
    QString determineFileFormat(const QString& filePath);
    bool createRecordingFile(RecordingInfo& recordingInfo);
    bool createBinaryRecordingFile(RecordingInfo& recordingInfo);
    bool createCSVRecordingFile(RecordingInfo& recordingInfo);
    bool createXMLRecordingFile(RecordingInfo& recordingInfo);
    bool createJSONRecordingFile(RecordingInfo& recordingInfo);
    bool writeRecordingHeader(RecordingInfo& recordingInfo);
    void initializeRecordingBuffer(RecordingInfo& recordingInfo);
    void flushRecordingBuffer(RecordingInfo& recordingInfo);
    bool writeRecordingFooter(RecordingInfo& recordingInfo);
    void closeRecordingFile(RecordingInfo& recordingInfo);
    void generateRecordingSummary(RecordingInfo& recordingInfo);
    QString calculateDataChecksum(const RecordingInfo& recordingInfo);
    double calculateCompressionRatio(const RecordingInfo& recordingInfo);
    
    // 记录管理辅助方法
    bool validateRecordingId(const QString& recordingId);
    RecordingInfo* getRecordingInfo(const QString& recordingId);
    const RecordingInfo* getRecordingInfo(const QString& recordingId) const;
    QString generatePlaybackId() const;
    QString generateExportId() const;
    PlaybackInfo* getPlaybackInfo(const QString& playbackId);
    bool parseRecordedFile(PlaybackInfo& playbackInfo);
    bool validateRecordedFile(PlaybackInfo& playbackInfo);
    int findFrameByTimestamp(const PlaybackInfo& playbackInfo, qint64 timestamp);
    QVariantMap getFrameData(const PlaybackInfo& playbackInfo, int frameIndex);
    
    // 导出方法
    bool exportToCSV(ExportInfo& exportInfo, const RecordingInfo& recordingInfo);
    bool exportToXML(ExportInfo& exportInfo, const RecordingInfo& recordingInfo);
    bool exportToJSON(ExportInfo& exportInfo, const RecordingInfo& recordingInfo);
    bool exportToMatlab(ExportInfo& exportInfo, const RecordingInfo& recordingInfo);
    bool exportToHDF5(ExportInfo& exportInfo, const RecordingInfo& recordingInfo);
    
    // 现代化追踪控制界面方法
    QWidget* createTrackingControlInterface(QWidget* parent = nullptr);
    QHBoxLayout* createControlInterfaceTitleBar(const QString& interfaceId);
    QWidget* createControlPanel(const QString& interfaceId);
    QWidget* createVisualizationPanel(const QString& interfaceId);
    
    // Atracsys SDK 集成方法
    bool initializeAtracsysSDK();
    bool cleanupAtracsysSDK();
    bool scanAtracsysDevices();
    bool connectToAtracsysDevice(const QString& deviceId);
    bool disconnectFromAtracsysDevice(const QString& deviceId);
    bool loadRigidBodyGeometry(const QString& sessionId, const QString& geometryFile);
    bool startAtracsysTracking(const QString& sessionId);
    bool stopAtracsysTracking(const QString& sessionId);
    QMap<QString, QList<double>> getAtracsysFrameData(const QString& sessionId);
    
    // 几何体和配置管理
    QString resolveProbeCalibrationGeometry(const QString& sessionId, const QString& toolId);
    QString findGeometryFile(const QString& geometryId);
    bool validateGeometryFile(const QString& filePath);
    QVariantMap parseGeometryInfo(const QString& filePath);
    
    // 数据处理和转换
    QList<double> convertAtracsysToStandardPose(const float translation[3], const float rotation[3][3]);
    QMatrix4x4 createTransformMatrix(const QList<double>& translation, const QList<double>& rotation);
    QList<double> calculateRelativeTransform(const QList<double>& pose1, const QList<double>& pose2);
    
    // UDP 服务器支持（与参考项目兼容）
    bool startUDPServer(quint16 port = 8888);
    bool stopUDPServer();
    void broadcastTrackingData(const QMap<QString, QList<double>>& trackingData);

    // UDP 命令处理
    void processUDPCommand(const QByteArray& datagram, const QHostAddress& sender, quint16 senderPort);
    void handleTextCommand(const QString& command, const QHostAddress& sender, quint16 senderPort);
    void sendUDPBroadcast(const QString& message, quint16 port);
    void sendDeviceList(const QHostAddress& sender, quint16 senderPort);
    void sendSessionList(const QHostAddress& sender, quint16 senderPort);
    void sendToolStatus(const QHostAddress& sender, quint16 senderPort);
    void sendAcknowledge(const QHostAddress& sender, quint16 senderPort, quint8 commandType, bool success);
    void sendPong(const QHostAddress& sender, quint16 senderPort);
    
    // 设备管理辅助方法
    void addSimulatedDevices();
    bool validateDeviceId(const QString& deviceId);

    // 会话管理辅助方法
    QString createTrackingSession(const QString& deviceId);

    // CTK插件上下文
    PlatformServiceRegistry* m_serviceRegistry;
    
    // 可选的医学图像服务引用（CTK服务框架）
    UnifiedMedicalImageService* m_imageService;
    
    // 可选的图像交互服务引用（CTK服务框架）
    ImageInteractionService* m_interactionService;
    
    // Atracsys SDK 相关成员变量
    ftkLibrary m_atracsysLibrary;           // ftkLibrary handle
    uint64 m_currentDeviceSerial;           // 当前设备序列号
    bool m_deviceInitialized;               // 设备是否已初始化
    QMap<QString, ftkRigidBody> m_rigidBodies; // 刚体几何体映射
    QMap<QString, QString> m_deviceTypes; // 设备类型映射
    void* m_currentFrame;              // ftkFrameQuery handle
    
    // UDP 服务器相关（用于与参考项目兼容）
    QUdpSocket* m_udpSocket;
    bool m_udpServerEnabled;
    quint16 m_udpPort;
    
    // 设备注册表
    QMap<QString, DeviceInfo> m_devices;
    
    // 会话注册表
    QMap<QString, SessionInfo> m_sessions;
    
    // 校准注册表
    QMap<QString, CalibrationInfo> m_calibrations;
    
    // 记录注册表
    QMap<QString, RecordingInfo> m_recordings;
    
    // 回放信息注册表
    QMap<QString, PlaybackInfo> m_playbacks;
    
    // 导出信息注册表 
    QMap<QString, ExportInfo> m_exports;
    
    // 服务状态
    mutable QMutex m_mutex;
    QString m_lastError;
    bool m_imageServiceConnected;
    bool m_interactionServiceConnected;
    bool m_componentsInitialized;

    // 定时器
    QTimer* m_realTimeTimer;

    // 默认设备参数
    QMap<QString, QVariantMap> m_defaultDeviceParameters;

    // 模拟数据生成器
    QMap<QString, QVariantMap> m_simulationState;

    // 控制界面注册表
    QMap<QString, QWidget*> m_controlInterfaces;

    // ==================== 新增：工具位置历史数据 ====================
    struct ToolTrackingData {
        QList<double> currentPosition;      // [x, y, z, rx, ry, rz]
        QList<double> previousPosition;     // 上一帧位置
        QList<double> velocity;             // 速度 [vx, vy, vz, vrx, vry, vrz]
        QList<QList<double>> positionHistory; // 历史位置（用于滤波）
        qint64 lastUpdateTime;
        bool visible;
        double quality;                     // 0.0 - 1.0
        QString motionPattern;              // "probe", "pointer", "reference", "custom"

        // 滤波器状态
        QList<double> kalmanState;          // 卡尔曼滤波状态
        QList<double> kalmanCovariance;     // 卡尔曼协方差

        // 校准偏移
        QList<double> calibrationOffset;    // 校准后的工具尖端偏移

        ToolTrackingData() : lastUpdateTime(0), visible(false), quality(0.0) {
            currentPosition = {0, 0, 0, 0, 0, 0};
            previousPosition = {0, 0, 0, 0, 0, 0};
            velocity = {0, 0, 0, 0, 0, 0};
            calibrationOffset = {0, 0, 0};
        }
    };

    // 工具跟踪数据缓存 (sessionId -> toolId -> data)
    QMap<QString, QMap<QString, ToolTrackingData>> m_toolTrackingData;

    // 实时流配置
    struct StreamingConfig {
        bool enabled;
        double frequency;           // Hz
        qint64 lastBroadcastTime;

        StreamingConfig() : enabled(false), frequency(30.0), lastBroadcastTime(0) {}
    };
    QMap<QString, StreamingConfig> m_streamingConfigs;

    // 记录文件句柄
    QMap<QString, std::ofstream*> m_recordingFiles;

    // 回放数据缓存
    struct PlaybackData {
        QList<qint64> timestamps;
        QList<QMap<QString, QList<double>>> frames;  // toolId -> position per frame
    };
    QMap<QString, PlaybackData> m_playbackDataCache;

    // 随机数生成器（用于模拟）
    std::mt19937 m_randomGenerator;

    // VTK渲染控制状态
    bool m_renderingPaused;
    QList<QWidget*> m_vtkWidgets;  // 跟踪所有创建的VTK Widget

    // ==================== ProbeCalibration DLL ====================

    // DLL 函数指针类型（probe_calibration_c_api.h）
    using PC_CreatePipelineFn = void* (*)();
    using PC_DestroyPipelineFn = void (*)(void*);
    using PC_InitializePipelineFn = int (*)(void*, const char*);
    using PC_IsInitializedFn = int (*)(void*);
    using PC_ShutdownPipelineFn = void (*)(void*);
    using PC_StartCalibrationFn = int (*)(void*);
    using PC_FinishCalibrationFn = int (*)(void*);
    using PC_SaveCalibrationFn = int (*)(void*, const char*);
    using PC_LoadCalibrationFn = int (*)(void*, const char*);
    using PC_IsCalibrated = int (*)(void*);
    using PC_GetLastErrorFn = const char* (*)(void*);
    // Collector API
    using PC_CollectorResetFn = int (*)(void*);
    using PC_CollectorAddPointFn = int (*)(void*, float, float, float, uint64_t);
    using PC_CollectorGetSuperPointCountFn = int (*)(void*, uint32_t*);
    using PC_CollectorExportFn = int (*)(void*, float*, float*, float*, uint32_t, uint32_t*);

    bool loadProbeCalibrationDLL(const QString& dllPath = QString());
    bool initializeProbeCalibrationPipeline(const QString& geometrySelector = QStringLiteral("072"));
    QVariantMap performPivotCalibrationDLL(const QString& calibrationId, CalibrationInfo& calibInfo);

    QLibrary m_pcLib;
    void* m_pcPipeline = nullptr;
    bool m_pcLoaded = false;

    // 函数指针
    PC_CreatePipelineFn m_pcCreate = nullptr;
    PC_DestroyPipelineFn m_pcDestroy = nullptr;
    PC_InitializePipelineFn m_pcInitialize = nullptr;
    PC_IsInitializedFn m_pcIsInitialized = nullptr;
    PC_ShutdownPipelineFn m_pcShutdown = nullptr;
    PC_StartCalibrationFn m_pcStartCalibration = nullptr;
    PC_FinishCalibrationFn m_pcFinishCalibration = nullptr;
    PC_SaveCalibrationFn m_pcSaveCalibration = nullptr;
    PC_LoadCalibrationFn m_pcLoadCalibration = nullptr;
    PC_IsCalibrated m_pcIsCalibrated = nullptr;
    PC_GetLastErrorFn m_pcGetLastError = nullptr;
    PC_CollectorResetFn m_pcCollectorReset = nullptr;
    PC_CollectorAddPointFn m_pcCollectorAddPoint = nullptr;
    PC_CollectorGetSuperPointCountFn m_pcCollectorGetSuperPointCount = nullptr;
    PC_CollectorExportFn m_pcCollectorExport = nullptr;
};

#endif // OPTICAL_TRACKING_SERVICE_IMPL_H
