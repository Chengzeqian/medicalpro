#ifndef IMAGE_INTERACTION_SERVICE_H
#define IMAGE_INTERACTION_SERVICE_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <QMetaType>

/**
 * @brief Image Interaction Service Interface (完全CTK架构)
 * 
 * 提供医学图像交互和点选功能的标准接口，采用完全CTK架构设计：
 * - 通过图像ID引用图像数据
 * - 所有操作通过CTK服务接口完成
 * - 不直接依赖MedicalImageData类
 * - 支持点选、测量、标注等交互功能
 * - 3D图像导航和操作工具
 * 
 * 核心设计原则：
 * 1. 输入：图像ID (QString)
 * 2. 交互：交互组件 (QWidget*)
 * 3. 结果：坐标和测量数据 (QVariantMap)
 * 4. 通信：完全通过CTK服务框架
 */
class ImageInteractionService : public QObject
{
    Q_OBJECT

public:
    explicit ImageInteractionService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ImageInteractionService() = default;

    // ==================== 交互组件创建和管理 ====================
    
    /**
     * @brief 创建3D点选器
     * @param parent 父窗口
     * @return 点选器控件指针，失败返回nullptr
     */
    virtual QWidget* createPointPicker(QWidget* parent = nullptr) = 0;

    /**
     * @brief 创建图像交互控制面板
     * @param parent 父窗口
     * @return 控制面板控件指针，失败返回nullptr
     */
    virtual QWidget* createInteractionPanel(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 创建测量工具栏
     * @param parent 父窗口
     * @return 工具栏控件指针，失败返回nullptr
     */
    virtual QWidget* createMeasurementToolbar(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 创建标注编辑器
     * @param parent 父窗口
     * @return 编辑器控件指针，失败返回nullptr
     */
    virtual QWidget* createAnnotationEditor(QWidget* parent = nullptr) = 0;

    // ==================== 图像交互操作 ====================
    
    /**
     * @brief 绑定图像到交互组件
     * @param componentId 交互组件ID
     * @param imageId 图像ID
     * @return 成功返回true，失败返回false
     */
    virtual bool bindImageToComponent(const QString& componentId, const QString& imageId) = 0;
    
    /**
     * @brief 解除图像绑定
     * @param componentId 交互组件ID
     * @return 成功返回true，失败返回false
     */
    virtual bool unbindImageFromComponent(const QString& componentId) = 0;
    
    /**
     * @brief 获取组件当前绑定的图像ID
     * @param componentId 交互组件ID
     * @return 图像ID，未绑定则返回空字符串
     */
    virtual QString getBoundImageId(const QString& componentId) const = 0;
    
    /**
     * @brief 启用/禁用交互模式
     * @param componentId 交互组件ID
     * @param enabled 是否启用
     * @return 成功返回true，失败返回false
     */
    virtual bool setInteractionEnabled(const QString& componentId, bool enabled) = 0;

    // ==================== 点选和坐标操作 ====================
    
    /**
     * @brief 启用点选模式
     * @param componentId 交互组件ID
     * @param enabled 是否启用
     * @return 成功返回true，失败返回false
     */
    virtual bool enablePointPicking(const QString& componentId, bool enabled) = 0;
    
    /**
     * @brief 设置点选回调
     * @param componentId 交互组件ID
     * @param maxPoints 最大点数（0表示无限制）
     * @return 成功返回true，失败返回false
     */
    virtual bool setPointPickingMode(const QString& componentId, int maxPoints) = 0;
    
    /**
     * @brief 添加标记点
     * @param componentId 交互组件ID
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @param label 点标签
     * @return 点ID，失败返回空字符串
     */
    virtual QString addMarkerPoint(const QString& componentId, double x, double y, double z, const QString& label = QString()) = 0;
    
    /**
     * @brief 移除标记点
     * @param componentId 交互组件ID
     * @param pointId 点ID
     * @return 成功返回true，失败返回false
     */
    virtual bool removeMarkerPoint(const QString& componentId, const QString& pointId) = 0;
    
    /**
     * @brief 清除所有标记点
     * @param componentId 交互组件ID
     * @return 成功返回true，失败返回false
     */
    virtual bool clearAllMarkerPoints(const QString& componentId) = 0;
    
    /**
     * @brief 获取所有标记点
     * @param componentId 交互组件ID
     * @return 点信息列表
     */
    virtual QList<QVariantMap> getMarkerPoints(const QString& componentId) const = 0;

    // ==================== 测量功能 ====================
    
    /**
     * @brief 启用距离测量
     * @param componentId 交互组件ID
     * @param enabled 是否启用
     * @return 成功返回true，失败返回false
     */
    virtual bool enableDistanceMeasurement(const QString& componentId, bool enabled) = 0;
    
    /**
     * @brief 启用角度测量
     * @param componentId 交互组件ID
     * @param enabled 是否启用
     * @return 成功返回true，失败返回false
     */
    virtual bool enableAngleMeasurement(const QString& componentId, bool enabled) = 0;
    
    /**
     * @brief 启用体积测量
     * @param componentId 交互组件ID
     * @param enabled 是否启用
     * @return 成功返回true，失败返回false
     */
    virtual bool enableVolumeMeasurement(const QString& componentId, bool enabled) = 0;
    
    /**
     * @brief 测量两点间距离
     * @param componentId 交互组件ID
     * @param point1 起点坐标 [x, y, z]
     * @param point2 终点坐标 [x, y, z]
     * @return 距离值（毫米），失败返回-1
     */
    virtual double measureDistance(const QString& componentId, const QList<double>& point1, const QList<double>& point2) = 0;
    
    /**
     * @brief 测量三点间角度
     * @param componentId 交互组件ID
     * @param vertex 顶点坐标 [x, y, z]
     * @param point1 第一个点坐标 [x, y, z]
     * @param point2 第二个点坐标 [x, y, z]
     * @return 角度值（度），失败返回-1
     */
    virtual double measureAngle(const QString& componentId, const QList<double>& vertex, const QList<double>& point1, const QList<double>& point2) = 0;
    
    /**
     * @brief 获取所有测量结果
     * @param componentId 交互组件ID
     * @return 测量结果列表
     */
    virtual QList<QVariantMap> getMeasurements(const QString& componentId) const = 0;
    
    /**
     * @brief 清除所有测量结果
     * @param componentId 交互组件ID
     * @return 成功返回true，失败返回false
     */
    virtual bool clearMeasurements(const QString& componentId) = 0;

    // ==================== 标注功能 ====================
    
    /**
     * @brief 添加文本标注
     * @param componentId 交互组件ID
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @param text 标注文本
     * @param fontSize 字体大小
     * @param color 文字颜色（RGB格式："255,0,0"）
     * @return 标注ID，失败返回空字符串
     */
    virtual QString addTextAnnotation(const QString& componentId, double x, double y, double z, 
                                    const QString& text, int fontSize = 12, const QString& color = "255,255,255") = 0;
    
    /**
     * @brief 添加箭头标注
     * @param componentId 交互组件ID
     * @param startX 起点X坐标
     * @param startY 起点Y坐标
     * @param startZ 起点Z坐标
     * @param endX 终点X坐标
     * @param endY 终点Y坐标
     * @param endZ 终点Z坐标
     * @param color 箭头颜色（RGB格式："255,0,0"）
     * @return 标注ID，失败返回空字符串
     */
    virtual QString addArrowAnnotation(const QString& componentId, double startX, double startY, double startZ,
                                     double endX, double endY, double endZ, const QString& color = "255,255,0") = 0;
    
    /**
     * @brief 修改标注
     * @param componentId 交互组件ID
     * @param annotationId 标注ID
     * @param properties 新属性
     * @return 成功返回true，失败返回false
     */
    virtual bool modifyAnnotation(const QString& componentId, const QString& annotationId, const QVariantMap& properties) = 0;
    
    /**
     * @brief 移除标注
     * @param componentId 交互组件ID
     * @param annotationId 标注ID
     * @return 成功返回true，失败返回false
     */
    virtual bool removeAnnotation(const QString& componentId, const QString& annotationId) = 0;
    
    /**
     * @brief 清除所有标注
     * @param componentId 交互组件ID
     * @return 成功返回true，失败返回false
     */
    virtual bool clearAllAnnotations(const QString& componentId) = 0;
    
    /**
     * @brief 获取所有标注
     * @param componentId 交互组件ID
     * @return 标注信息列表
     */
    virtual QList<QVariantMap> getAnnotations(const QString& componentId) const = 0;

    // ==================== 坐标系统和变换 ====================
    
    /**
     * @brief 屏幕坐标转世界坐标
     * @param componentId 交互组件ID
     * @param screenX 屏幕X坐标
     * @param screenY 屏幕Y坐标
     * @return 世界坐标 [x, y, z]，失败返回空列表
     */
    virtual QList<double> screenToWorldCoordinates(const QString& componentId, double screenX, double screenY) = 0;
    
    /**
     * @brief 世界坐标转屏幕坐标
     * @param componentId 交互组件ID
     * @param worldX 世界X坐标
     * @param worldY 世界Y坐标
     * @param worldZ 世界Z坐标
     * @return 屏幕坐标 [x, y]，失败返回空列表
     */
    virtual QList<double> worldToScreenCoordinates(const QString& componentId, double worldX, double worldY, double worldZ) = 0;
    
    /**
     * @brief 图像坐标转世界坐标
     * @param componentId 交互组件ID
     * @param imageX 图像X坐标（像素）
     * @param imageY 图像Y坐标（像素）
     * @param imageZ 图像Z坐标（像素）
     * @return 世界坐标 [x, y, z]，失败返回空列表
     */
    virtual QList<double> imageToWorldCoordinates(const QString& componentId, int imageX, int imageY, int imageZ) = 0;

    // ==================== 交互组件管理 ====================
    
    /**
     * @brief 获取所有活动交互组件ID
     * @return 组件ID列表
     */
    virtual QStringList getActiveComponents() const = 0;
    
    /**
     * @brief 关闭交互组件
     * @param componentId 交互组件ID
     * @return 成功返回true，失败返回false
     */
    virtual bool closeComponent(const QString& componentId) = 0;
    
    /**
     * @brief 获取组件信息
     * @param componentId 交互组件ID
     * @return 组件信息映射
     */
    virtual QVariantMap getComponentInfo(const QString& componentId) const = 0;
    
    /**
     * @brief 设置组件参数
     * @param componentId 交互组件ID
     * @param parameters 参数映射
     * @return 成功返回true，失败返回false
     */
    virtual bool setComponentParameters(const QString& componentId, const QVariantMap& parameters) = 0;

    // ==================== UI显示管理（遵循PatientManagement成功模式） ====================
    
    /**
     * @brief 显示图像交互对话框
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showInteractionDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示点选工具对话框
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showPointPickerDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示测量工具对话框
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showMeasurementDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示标注编辑对话框
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showAnnotationDialog(QWidget* parent = nullptr) = 0;

signals:
    /**
     * @brief 交互组件创建信号
     * @param componentId 组件ID
     * @param componentType 组件类型
     */
    void componentCreated(const QString& componentId, const QString& componentType);
    
    /**
     * @brief 点选完成信号
     * @param componentId 组件ID
     * @param worldX 世界坐标X
     * @param worldY 世界坐标Y
     * @param worldZ 世界坐标Z
     * @param imageX 图像坐标X
     * @param imageY 图像坐标Y
     * @param imageZ 图像坐标Z
     */
    void pointPicked(const QString& componentId, double worldX, double worldY, double worldZ,
                    int imageX, int imageY, int imageZ);
    
    /**
     * @brief 标记点添加信号
     * @param componentId 组件ID
     * @param pointId 点ID
     * @param coordinates 坐标信息
     */
    void markerPointAdded(const QString& componentId, const QString& pointId, const QVariantMap& coordinates);
    
    /**
     * @brief 测量完成信号
     * @param componentId 组件ID
     * @param measurementType 测量类型
     * @param value 测量值
     * @param unit 单位
     * @param details 详细信息
     */
    void measurementCompleted(const QString& componentId, const QString& measurementType, 
                            double value, const QString& unit, const QVariantMap& details);
    
    /**
     * @brief 标注添加信号
     * @param componentId 组件ID
     * @param annotationId 标注ID
     * @param annotationType 标注类型
     * @param properties 标注属性
     */
    void annotationAdded(const QString& componentId, const QString& annotationId, 
                        const QString& annotationType, const QVariantMap& properties);
    
    /**
     * @brief 标注修改信号
     * @param componentId 组件ID
     * @param annotationId 标注ID
     * @param properties 新属性
     */
    void annotationModified(const QString& componentId, const QString& annotationId, const QVariantMap& properties);
    
    /**
     * @brief 交互组件关闭信号
     * @param componentId 组件ID
     */
    void componentClosed(const QString& componentId);
    
    /**
     * @brief 错误信号
     * @param componentId 组件ID
     * @param error 错误信息
     */
    void interactionError(const QString& componentId, const QString& error);
};

// Qt接口声明
Q_DECLARE_INTERFACE(ImageInteractionService, "medical.ImageInteractionService")

#endif // IMAGE_INTERACTION_SERVICE_H
