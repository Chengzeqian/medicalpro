#ifndef MEDICAL_VIEWER_SERVICE_IMPL_H
#define MEDICAL_VIEWER_SERVICE_IMPL_H

#include "MedicalViewerService.h"
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QVariant>
#include <QUuid>
#include <QTimer>
#include <QWidget>
#include <QHBoxLayout>
#include <QDateTime>

// CTK框架
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

// 前向声明（遵循完全CTK架构）
class MedicalImageCoreService;

/**
 * @brief Medical Viewer Service Implementation (完全CTK架构)
 * 
 * MedicalViewerService接口的具体实现，采用完全CTK架构设计：
 * - 通过CTK服务框架获取MedicalImageCoreService
 * - 所有图像操作通过服务接口完成
 * - 不直接依赖MedicalImageData类
 * - 支持多种查看器类型和显示模式
 * - 完全解耦的插件间通信
 */
class MedicalViewerServiceImpl : public MedicalViewerService
{
    Q_OBJECT
    Q_INTERFACES(MedicalViewerService)

public:
    /**
     * @brief 构造函数
     * @param context CTK插件上下文
     * @param parent 父对象
     */
    explicit MedicalViewerServiceImpl(ctkPluginContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MedicalViewerServiceImpl() override;
    
    /**
     * @brief 设置CTK插件上下文（关键方法，遵循PatientManagement成功模式）
     * @param context CTK插件上下文
     */
    void setPluginContext(ctkPluginContext* context);

    // ==================== 查看器创建和管理实现 ====================
    
    QWidget* create2DImageViewer(QWidget* parent = nullptr) override;
    QWidget* create3DImageViewer(QWidget* parent = nullptr) override;
    QWidget* createMPRViewer(QWidget* parent = nullptr) override;
    QWidget* createVolumeRenderer(QWidget* parent = nullptr) override;
    
    // 新增：体绘制功能（整合自NrrdViewer）
    QWidget* createAdvancedVolumeRenderer(QWidget* parent = nullptr) override;
    QWidget* createNrrdViewer(QWidget* parent = nullptr) override;
    QWidget* createScientificVisualizationViewer(QWidget* parent = nullptr) override;

    // ==================== 图像显示操作实现 ====================
    
    bool displayImage(const QString& viewerId, const QString& imageId) override;
    bool displayMultipleImages(const QString& viewerId, const QStringList& imageIds) override;
    bool clearViewer(const QString& viewerId) override;
    QString getCurrentImageId(const QString& viewerId) const override;
    QStringList getDisplayedImageIds(const QString& viewerId) const override;

    // ==================== 窗口/级别调整实现 ====================
    
    bool setWindowLevel(const QString& viewerId, double window, double level) override;
    QVariantMap getWindowLevel(const QString& viewerId) const override;
    bool autoAdjustWindowLevel(const QString& viewerId) override;
    bool resetWindowLevel(const QString& viewerId) override;

    // ==================== 图像导航和操作实现 ====================
    
    bool setCurrentSlice(const QString& viewerId, int sliceIndex) override;
    int getCurrentSlice(const QString& viewerId) const override;
    int getSliceCount(const QString& viewerId) const override;
    bool zoomImage(const QString& viewerId, double zoomFactor) override;
    bool panImage(const QString& viewerId, double deltaX, double deltaY) override;
    bool resetView(const QString& viewerId) override;

    // ==================== 测量和标注实现 ====================
    
    bool enableDistanceMeasurement(const QString& viewerId, bool enabled) override;
    bool enableAngleMeasurement(const QString& viewerId, bool enabled) override;
    QString addTextAnnotation(const QString& viewerId, double x, double y, const QString& text) override;
    bool removeAnnotation(const QString& viewerId, const QString& annotationId) override;
    bool clearAllAnnotations(const QString& viewerId) override;

    // ==================== 高级功能实现 ====================
    
    bool setViewerLayout(const QString& viewerId, int rows, int cols) override;
    bool setImageOrientation(const QString& viewerId, const QString& orientation) override;
    bool enableSynchronizedScrolling(const QStringList& viewerIds, bool enabled) override;
    bool captureViewerImage(const QString& viewerId, const QString& filePath) override;

    // ==================== 查看器管理实现 ====================
    
    QStringList getAvailableViewerTypes() const override;
    QStringList getActiveViewers() const override;
    bool closeViewer(const QString& viewerId) override;
    QVariantMap getViewerInfo(const QString& viewerId) const override;
    bool setViewerParameters(const QString& viewerId, const QVariantMap& parameters) override;

    // ==================== 专业体绘制功能实现（整合自NrrdViewer） ====================
    
    bool isNrrdImage(const QString& imageId) const override;
    bool setVolumeOpacity(const QString& viewerId, double opacity) override;
    bool setTransferFunction(const QString& viewerId, const QVariantMap& transferFunction) override;
    bool setVolumeLightingParameters(const QString& viewerId, double ambient, double diffuse, double specular) override;
    bool setVolumeMaterialProperties(const QString& viewerId, const QVariantMap& material) override;
    bool setVolumeCameraPosition(const QString& viewerId, double x, double y, double z) override;
    bool setVolumeCameraFocalPoint(const QString& viewerId, double x, double y, double z) override;
    bool resetVolumeCamera(const QString& viewerId) override;
    bool setVolumeClippingPlane(const QString& viewerId, const QVariantMap& plane) override;
    bool setVolumeSampleDistance(const QString& viewerId, double distance) override;
    bool setVolumeGradientShading(const QString& viewerId, bool enabled) override;
    bool setVolumeInteractionEnabled(const QString& viewerId, bool enabled) override;

    // ==================== 科研级可视化功能实现（整合自NrrdViewer） ====================
    
    QVariantMap getVolumeStatistics(const QString& viewerId, const QString& imageId) override;
    QVariantMap analyzeVolumeDistribution(const QString& viewerId, const QString& imageId) override;
    bool setAdvancedRenderingAlgorithm(const QString& viewerId, const QString& algorithm) override;
    bool setRenderingQuality(const QString& viewerId, const QString& quality) override;
    bool exportVolumeRendering(const QString& viewerId, const QString& filePath, 
                               const QString& format, const QVariantMap& options = QVariantMap()) override;

    // ==================== UI显示管理（遵循PatientManagement成功模式） ====================
    
    bool showViewerDialog(QWidget* parent = nullptr) override;
    bool showMPRDialog(QWidget* parent = nullptr) override;
    bool showViewerConfigDialog(QWidget* parent = nullptr) override;
    
    // 新增：体绘制UI（整合自NrrdViewer）
    bool showVolumeRenderingDialog(QWidget* parent = nullptr) override;
    bool showNrrdViewerDialog(QWidget* parent = nullptr) override;
    bool showTransferFunctionEditorDialog(QWidget* parent = nullptr) override;
    bool showScientificVisualizationDialog(QWidget* parent = nullptr) override;

private slots:
    /**
     * @brief 处理查看器关闭事件
     */
    void onViewerClosed();
    
    /**
     * @brief 处理服务可用性变化
     * @param available 服务是否可用
     */
    void onImageServiceAvailabilityChanged(bool available);

private:
    /**
     * @brief 查看器信息结构
     */
    struct ViewerInfo {
        QString viewerId;
        QString viewerType;
        QString status;
        QWidget* widget;
        QString currentImageId;
        QStringList displayedImageIds;
        QVariantMap parameters;
        QMap<QString, QVariant> state;
        bool isActive;
        QDateTime creationTime;
        
        ViewerInfo() : widget(nullptr), isActive(false) {}
    };

    /**
     * @brief 初始化图像服务连接
     */
    void initializeImageServiceConnection();
    
    /**
     * @brief 验证查看器ID有效性
     * @param viewerId 查看器ID
     * @return 是否有效
     */
    bool validateViewerId(const QString& viewerId) const;
    
    /**
     * @brief 验证图像ID有效性
     * @param imageId 图像ID
     * @return 是否有效
     */
    bool validateImageId(const QString& imageId) const;
    
    /**
     * @brief 生成查看器ID
     * @return 唯一查看器ID
     */
    QString generateViewerId() const;
    
    /**
     * @brief 创建通用查看器
     * @param viewerType 查看器类型
     * @param parent 父窗口
     * @return 查看器控件指针
     */
    QWidget* createGenericViewer(const QString& viewerType, QWidget* parent);
    
    /**
     * @brief 获取查看器信息
     * @param viewerId 查看器ID
     * @return 查看器信息指针
     */
    ViewerInfo* getViewerInfo(const QString& viewerId);
    
    /**
     * @brief 获取查看器信息（只读）
     * @param viewerId 查看器ID
     * @return 查看器信息指针
     */
    const ViewerInfo* getViewerInfoPtr(const QString& viewerId) const;
    
    /**
     * @brief 设置错误信息
     * @param error 错误描述
     */
    void setError(const QString& error);
    
    /**
     * @brief 通过图像服务获取图像信息
     * @param imageId 图像ID
     * @return 图像信息映射
     */
    QVariantMap getImageInfoFromService(const QString& imageId) const;
    
    /**
     * @brief 注册查看器
     * @param viewerId 查看器ID
     * @param viewerType 查看器类型
     * @param widget 查看器控件
     */
    void registerViewer(const QString& viewerId, const QString& viewerType, QWidget* widget);
    
    /**
     * @brief 注销查看器
     * @param viewerId 查看器ID
     */
    void unregisterViewer(const QString& viewerId);
    
    /**
     * @brief 创建查看器控件
     * @param viewerType 查看器类型
     * @param parent 父窗口
     * @return 查看器控件指针
     */
    QWidget* createViewerWidget(const QString& viewerType, QWidget* parent);
    
    /**
     * @brief 创建2D查看器控件
     * @param parent 父窗口
     * @return 查看器控件指针
     */
    QWidget* create2DViewerWidget(QWidget* parent);
    
    /**
     * @brief 创建3D查看器控件
     * @param parent 父窗口
     * @return 查看器控件指针
     */
    QWidget* create3DViewerWidget(QWidget* parent);
    
    /**
     * @brief 创建MPR查看器控件
     * @param parent 父窗口
     * @return 查看器控件指针
     */
    QWidget* createMPRViewerWidget(QWidget* parent);
    
    /**
     * @brief 创建体绘制查看器控件
     * @param parent 父窗口
     * @return 查看器控件指针
     */
    QWidget* createVolumeRendererWidget(QWidget* parent);
    
    /**
     * @brief 创建高级体绘制查看器控件
     * @param parent 父窗口
     * @return 查看器控件指针
     */
    QWidget* createAdvancedVolumeRendererWidget(QWidget* parent);
    
    /**
     * @brief 创建NRRD查看器控件
     * @param parent 父窗口
     * @return 查看器控件指针
     */
    QWidget* createNrrdViewerWidget(QWidget* parent);
    
    /**
     * @brief 创建科研级可视化查看器控件
     * @param parent 父窗口
     * @return 查看器控件指针
     */
    QWidget* createScientificVisualizationWidget(QWidget* parent);
    
    /**
     * @brief 创建查看器工具栏
     * @param viewerType 查看器类型
     * @return 工具栏布局
     */
    QHBoxLayout* createViewerToolbar(const QString& viewerType);
    
    /**
     * @brief 创建显示区域
     * @param viewerType 查看器类型
     * @param viewerId 查看器ID
     * @return 显示区域控件
     */
    QWidget* createDisplayArea(const QString& viewerType, const QString& viewerId);
    
    /**
     * @brief 配置查看器特定功能
     * @param viewerId 查看器ID
     * @param viewerType 查看器类型
     * @param viewerWidget 查看器控件
     */
    void configureViewerSpecificFeatures(const QString& viewerId, const QString& viewerType, QWidget* viewerWidget);

private:
    /// CTK插件上下文
    ctkPluginContext* m_pluginContext;
    
    /// 医学图像服务引用（CTK服务框架）
    ctkServiceReference m_imageServiceRef;
    MedicalImageCoreService* m_imageService;
    
    /// 查看器注册表
    QMap<QString, ViewerInfo> m_viewers;
    
    /// 同步滚动组
    QMap<QString, QStringList> m_syncGroups;
    
    /// 错误信息
    QString m_lastError;
    
    /// 线程安全
    mutable QMutex m_mutex;
    
    /// 服务连接状态
    bool m_serviceConnected;
    
    /// 组件初始化状态
    bool m_componentsInitialized;
    
    /// 默认查看器参数
    QMap<QString, QVariantMap> m_defaultViewerParameters;
    
    /// 查看器类型计数器
    QMap<QString, int> m_viewerCounters;
};

#endif // MEDICAL_VIEWER_SERVICE_IMPL_H