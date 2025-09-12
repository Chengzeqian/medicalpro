#ifndef IMAGE_INTERACTION_SERVICE_IMPL_H
#define IMAGE_INTERACTION_SERVICE_IMPL_H

#include "ImageInteractionService.h"
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QVariant>
#include <QUuid>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QRadioButton>
#include <QListWidget>
#include <QTableWidget>
#include <QProgressBar>
#include <QFrame>

// CTK框架
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>

// 前向声明（遵循完全CTK架构）
class MedicalImageCoreService;

/**
 * @brief Image Interaction Service Implementation (完全CTK架构)
 * 
 * ImageInteractionService接口的具体实现，采用完全CTK架构设计：
 * - 通过CTK服务框架获取MedicalImageCoreService
 * - 专注于图像交互和点选功能
 * - 不直接依赖MedicalImageData类
 * - 支持测量、标注和坐标转换
 * - 完全解耦的插件间通信
 */
class ImageInteractionServiceImpl : public ImageInteractionService
{
    Q_OBJECT
    Q_INTERFACES(ImageInteractionService)

public:
    /**
     * @brief 构造函数
     * @param context CTK插件上下文
     * @param parent 父对象
     */
    explicit ImageInteractionServiceImpl(ctkPluginContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ImageInteractionServiceImpl() override;
    
    /**
     * @brief 设置CTK插件上下文（关键方法，遵循PatientManagement成功模式）
     * @param context CTK插件上下文
     */
    void setPluginContext(ctkPluginContext* context);

    // ==================== 交互组件创建和管理实现 ====================
    
    QWidget* createPointPicker(QWidget* parent = nullptr) override;
    QWidget* createInteractionPanel(QWidget* parent = nullptr) override;
    QWidget* createMeasurementToolbar(QWidget* parent = nullptr) override;
    QWidget* createAnnotationEditor(QWidget* parent = nullptr) override;

    // ==================== 图像交互操作实现 ====================
    
    bool bindImageToComponent(const QString& componentId, const QString& imageId) override;
    bool unbindImageFromComponent(const QString& componentId) override;
    QString getBoundImageId(const QString& componentId) const override;
    bool setInteractionEnabled(const QString& componentId, bool enabled) override;

    // ==================== 点选和坐标操作实现 ====================
    
    bool enablePointPicking(const QString& componentId, bool enabled) override;
    bool setPointPickingMode(const QString& componentId, int maxPoints) override;
    QString addMarkerPoint(const QString& componentId, double x, double y, double z, const QString& label = QString()) override;
    bool removeMarkerPoint(const QString& componentId, const QString& pointId) override;
    bool clearAllMarkerPoints(const QString& componentId) override;
    QList<QVariantMap> getMarkerPoints(const QString& componentId) const override;

    // ==================== 测量功能实现 ====================
    
    bool enableDistanceMeasurement(const QString& componentId, bool enabled) override;
    bool enableAngleMeasurement(const QString& componentId, bool enabled) override;
    bool enableVolumeMeasurement(const QString& componentId, bool enabled) override;
    double measureDistance(const QString& componentId, const QList<double>& point1, const QList<double>& point2) override;
    double measureAngle(const QString& componentId, const QList<double>& vertex, const QList<double>& point1, const QList<double>& point2) override;
    QList<QVariantMap> getMeasurements(const QString& componentId) const override;
    bool clearMeasurements(const QString& componentId) override;

    // ==================== 标注功能实现 ====================
    
    QString addTextAnnotation(const QString& componentId, double x, double y, double z, 
                            const QString& text, int fontSize = 12, const QString& color = "255,255,255") override;
    QString addArrowAnnotation(const QString& componentId, double startX, double startY, double startZ,
                             double endX, double endY, double endZ, const QString& color = "255,255,0") override;
    bool modifyAnnotation(const QString& componentId, const QString& annotationId, const QVariantMap& properties) override;
    bool removeAnnotation(const QString& componentId, const QString& annotationId) override;
    bool clearAllAnnotations(const QString& componentId) override;
    QList<QVariantMap> getAnnotations(const QString& componentId) const override;

    // ==================== 坐标系统和变换实现 ====================
    
    QList<double> screenToWorldCoordinates(const QString& componentId, double screenX, double screenY) override;
    QList<double> worldToScreenCoordinates(const QString& componentId, double worldX, double worldY, double worldZ) override;
    QList<double> imageToWorldCoordinates(const QString& componentId, int imageX, int imageY, int imageZ) override;

    // ==================== 交互组件管理实现 ====================
    
    QStringList getActiveComponents() const override;
    bool closeComponent(const QString& componentId) override;
    QVariantMap getComponentInfo(const QString& componentId) const override;
    bool setComponentParameters(const QString& componentId, const QVariantMap& parameters) override;

    // ==================== UI显示管理（遵循PatientManagement成功模式） ====================
    
    bool showInteractionDialog(QWidget* parent = nullptr) override;
    bool showPointPickerDialog(QWidget* parent = nullptr) override;
    bool showMeasurementDialog(QWidget* parent = nullptr) override;
    bool showAnnotationDialog(QWidget* parent = nullptr) override;
    
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
     * @return 服务名称
     */
    QString getServiceName() const;
    
    /**
     * @brief 获取服务版本
     * @return 服务版本
     */
    QString getServiceVersion() const;
    
    /**
     * @brief 检查服务是否活跃
     * @return 是否活跃
     */
    bool isActive() const;
    
    /**
     * @brief 检查点选功能是否启用
     * @return 是否启用
     */
    bool isPointPickingEnabled() const;
    
    /**
     * @brief 清除所有点
     */
    void clearPoints();
    
    /**
     * @brief 获取点数量
     * @return 点数量
     */
    int getPointCount() const;

signals:
    /**
     * @brief 服务状态变化信号
     * @param active 是否活跃
     */
    void serviceStatusChanged(bool active);
    
    /**
     * @brief 交互事件信号
     * @param eventType 事件类型
     * @param data 事件数据
     */
    void interactionEvent(const QString& eventType, const QVariantMap& data);
    
    /**
     * @brief 点被选中信号
     * @param point 点坐标
     * @param index 点索引
     * @param totalCount 总点数
     */
    void pointPicked(const QVector3D& point, int index, int totalCount);
    
    /**
     * @brief 点被移除信号
     * @param index 点索引
     * @param totalCount 剩余点数
     */
    void pointRemoved(int index, int totalCount);
    
    /**
     * @brief 所有点被清除信号
     */
    void allPointsCleared();
    
    /**
     * @brief 点选模式变化信号
     * @param enabled 是否启用
     */
    void pointPickingModeChanged(bool enabled);

private slots:
    /**
     * @brief 处理组件关闭事件
     */
    void onComponentClosed();
    
    /**
     * @brief 处理服务可用性变化
     * @param available 服务是否可用
     */
    void onImageServiceAvailabilityChanged(bool available);

private:
    /**
     * @brief 交互组件信息结构
     */
    struct ComponentInfo {
        QString componentId;
        QString componentType;
        QWidget* widget;
        QString boundImageId;
        QVariantMap parameters;
        QMap<QString, QVariant> state;
        QMap<QString, QVariantMap> markerPoints;
        QList<QVariantMap> measurements;
        QList<QVariantMap> annotations;
        
        ComponentInfo() : widget(nullptr) {}
    };

    /**
     * @brief 初始化图像服务连接
     */
    void initializeImageServiceConnection();
    
    /**
     * @brief 验证组件ID有效性
     * @param componentId 组件ID
     * @return 是否有效
     */
    bool validateComponentId(const QString& componentId) const;
    
    /**
     * @brief 验证图像ID有效性
     * @param imageId 图像ID
     * @return 是否有效
     */
    bool validateImageId(const QString& imageId) const;
    
    /**
     * @brief 生成组件ID
     * @return 唯一组件ID
     */
    QString generateComponentId() const;
    
    /**
     * @brief 生成点ID
     * @return 唯一点ID
     */
    QString generatePointId() const;
    
    /**
     * @brief 生成测量ID
     * @return 唯一测量ID
     */
    QString generateMeasurementId() const;
    
    /**
     * @brief 生成标注ID
     * @return 唯一标注ID
     */
    QString generateAnnotationId() const;
    
    /**
     * @brief 创建通用交互组件
     * @param componentType 组件类型
     * @param parent 父窗口
     * @return 组件控件指针
     */
    QWidget* createGenericComponent(const QString& componentType, QWidget* parent);
    
    /**
     * @brief 创建组件内容
     * @param componentType 组件类型
     * @param componentId 组件ID
     * @return 组件内容控件
     */
    QWidget* createComponentContent(const QString& componentType, const QString& componentId);
    
    /**
     * @brief 创建点选器内容
     * @param componentId 组件ID
     * @return 点选器控件
     */
    QWidget* createPointPickerContent(const QString& componentId);
    
    /**
     * @brief 创建交互面板内容
     * @param componentId 组件ID
     * @return 交互面板控件
     */
    QWidget* createInteractionPanelContent(const QString& componentId);
    
    /**
     * @brief 创建测量工具栏内容
     * @param componentId 组件ID
     * @return 测量工具栏控件
     */
    QWidget* createMeasurementToolbarContent(const QString& componentId);
    
    /**
     * @brief 创建标注编辑器内容
     * @param componentId 组件ID
     * @return 标注编辑器控件
     */
    QWidget* createAnnotationEditorContent(const QString& componentId);
    
    /**
     * @brief 获取组件信息
     * @param componentId 组件ID
     * @return 组件信息指针
     */
    ComponentInfo* getComponentInfoPtr(const QString& componentId);
    
    /**
     * @brief 获取组件信息指针（只读）
     * @param componentId 组件ID
     * @return 组件信息指针
     */
    const ComponentInfo* getComponentInfoPtr(const QString& componentId) const;
    
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
     * @brief 注册交互组件
     * @param componentId 组件ID
     * @param componentType 组件类型
     * @param widget 组件控件
     */
    void registerComponent(const QString& componentId, const QString& componentType, QWidget* widget);
    
    /**
     * @brief 计算两点间距离
     * @param point1 起点
     * @param point2 终点
     * @param spacing 像素间距
     * @return 距离（毫米）
     */
    double calculateDistance(const QList<double>& point1, const QList<double>& point2, const QList<double>& spacing) const;
    
    /**
     * @brief 计算三点间角度
     * @param vertex 顶点
     * @param point1 第一点
     * @param point2 第二点
     * @return 角度（度）
     */
    double calculateAngle(const QList<double>& vertex, const QList<double>& point1, const QList<double>& point2) const;
    
    /**
     * @brief 初始化默认参数
     */
    void initializeDefaultParameters();

private:
    /// CTK插件上下文
    ctkPluginContext* m_pluginContext;
    
    /// 医学图像服务引用（CTK服务框架）
    ctkServiceReference m_imageServiceRef;
    MedicalImageCoreService* m_imageService;
    
    /// 交互组件注册表
    QMap<QString, ComponentInfo> m_components;
    
    /// 错误信息
    QString m_lastError;
    
    /// 线程安全
    mutable QMutex m_mutex;
    
    /// 服务连接状态
    bool m_serviceConnected;
    
    /// 组件初始化状态
    bool m_componentsInitialized;
    
    /// 默认组件参数
    QMap<QString, QVariantMap> m_defaultComponentParameters;
    
    /// 组件类型计数器
    QMap<QString, int> m_componentCounters;
};

#endif // IMAGE_INTERACTION_SERVICE_IMPL_H
