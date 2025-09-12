#ifndef MEDICAL_VIEWER_SERVICE_H
#define MEDICAL_VIEWER_SERVICE_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <QMetaType>

/**
 * @brief Medical Viewer Service Interface (完全CTK架构)
 * 
 * 提供医学图像显示和可视化的标准接口，采用完全CTK架构设计：
 * - 通过图像ID引用图像数据
 * - 所有操作通过CTK服务接口完成
 * - 不直接依赖MedicalImageData类
 * - 支持多种显示模式和交互功能
 * 
 * 核心设计原则：
 * 1. 输入：图像ID (QString)
 * 2. 显示：查看器窗口 (QWidget*)
 * 3. 配置：显示参数 (QVariantMap)
 * 4. 通信：完全通过CTK服务框架
 */
class MedicalViewerService : public QObject
{
    Q_OBJECT

public:
    explicit MedicalViewerService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~MedicalViewerService() = default;

    // ==================== 查看器创建和管理 ====================
    
    /**
     * @brief 创建2D图像查看器
     * @param parent 父窗口
     * @return 查看器控件指针，失败返回nullptr
     */
    virtual QWidget* create2DImageViewer(QWidget* parent = nullptr) = 0;

    /**
     * @brief 创建3D图像查看器
     * @param parent 父窗口
     * @return 查看器控件指针，失败返回nullptr
     */
    virtual QWidget* create3DImageViewer(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 创建MPR（多平面重建）查看器
     * @param parent 父窗口
     * @return 查看器控件指针，失败返回nullptr
     */
    virtual QWidget* createMPRViewer(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 创建体绘制查看器
     * @param parent 父窗口
     * @return 查看器控件指针，失败返回nullptr
     */
    virtual QWidget* createVolumeRenderer(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 创建高级体绘制查看器（整合自NrrdViewer）
     * @param parent 父窗口
     * @return 查看器控件指针，失败返回nullptr
     */
    virtual QWidget* createAdvancedVolumeRenderer(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 创建NRRD专用查看器（整合自NrrdViewer）
     * @param parent 父窗口
     * @return 查看器控件指针，失败返回nullptr
     */
    virtual QWidget* createNrrdViewer(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 创建科研级可视化查看器（整合自NrrdViewer）
     * @param parent 父窗口
     * @return 查看器控件指针，失败返回nullptr
     */
    virtual QWidget* createScientificVisualizationViewer(QWidget* parent = nullptr) = 0;

    // ==================== 图像显示操作 ====================
    
    /**
     * @brief 在查看器中显示图像
     * @param viewerId 查看器ID
     * @param imageId 图像ID
     * @return 成功返回true，失败返回false
     */
    virtual bool displayImage(const QString& viewerId, const QString& imageId) = 0;
    
    /**
     * @brief 在查看器中显示多幅图像
     * @param viewerId 查看器ID
     * @param imageIds 图像ID列表
     * @return 成功返回true，失败返回false
     */
    virtual bool displayMultipleImages(const QString& viewerId, const QStringList& imageIds) = 0;
    
    /**
     * @brief 清除查看器中的图像
     * @param viewerId 查看器ID
     * @return 成功返回true，失败返回false
     */
    virtual bool clearViewer(const QString& viewerId) = 0;
    
    /**
     * @brief 获取查看器中当前显示的图像ID
     * @param viewerId 查看器ID
     * @return 当前图像ID，未显示则返回空字符串
     */
    virtual QString getCurrentImageId(const QString& viewerId) const = 0;
    
    /**
     * @brief 获取查看器中显示的所有图像ID
     * @param viewerId 查看器ID
     * @return 图像ID列表
     */
    virtual QStringList getDisplayedImageIds(const QString& viewerId) const = 0;

    // ==================== 窗口/级别调整 ====================
    
    /**
     * @brief 设置窗口/级别
     * @param viewerId 查看器ID
     * @param window 窗口宽度
     * @param level 窗口中心
     * @return 成功返回true，失败返回false
     */
    virtual bool setWindowLevel(const QString& viewerId, double window, double level) = 0;
    
    /**
     * @brief 获取当前窗口/级别
     * @param viewerId 查看器ID
     * @return 包含window和level的映射
     */
    virtual QVariantMap getWindowLevel(const QString& viewerId) const = 0;
    
    /**
     * @brief 自动调整窗口/级别
     * @param viewerId 查看器ID
     * @return 成功返回true，失败返回false
     */
    virtual bool autoAdjustWindowLevel(const QString& viewerId) = 0;
    
    /**
     * @brief 重置窗口/级别到默认值
     * @param viewerId 查看器ID
     * @return 成功返回true，失败返回false
     */
    virtual bool resetWindowLevel(const QString& viewerId) = 0;

    // ==================== 图像导航和操作 ====================
    
    /**
     * @brief 设置当前切片（用于3D图像）
     * @param viewerId 查看器ID
     * @param sliceIndex 切片索引
     * @return 成功返回true，失败返回false
     */
    virtual bool setCurrentSlice(const QString& viewerId, int sliceIndex) = 0;
    
    /**
     * @brief 获取当前切片索引
     * @param viewerId 查看器ID
     * @return 切片索引，失败返回-1
     */
    virtual int getCurrentSlice(const QString& viewerId) const = 0;
    
    /**
     * @brief 获取切片总数
     * @param viewerId 查看器ID
     * @return 切片总数，失败返回0
     */
    virtual int getSliceCount(const QString& viewerId) const = 0;
    
    /**
     * @brief 缩放图像
     * @param viewerId 查看器ID
     * @param zoomFactor 缩放因子
     * @return 成功返回true，失败返回false
     */
    virtual bool zoomImage(const QString& viewerId, double zoomFactor) = 0;
    
    /**
     * @brief 平移图像
     * @param viewerId 查看器ID
     * @param deltaX X方向偏移
     * @param deltaY Y方向偏移
     * @return 成功返回true，失败返回false
     */
    virtual bool panImage(const QString& viewerId, double deltaX, double deltaY) = 0;
    
    /**
     * @brief 重置图像视图到初始状态
     * @param viewerId 查看器ID
     * @return 成功返回true，失败返回false
     */
    virtual bool resetView(const QString& viewerId) = 0;

    // ==================== 测量和标注 ====================
    
    /**
     * @brief 启用距离测量
     * @param viewerId 查看器ID
     * @param enabled 是否启用
     * @return 成功返回true，失败返回false
     */
    virtual bool enableDistanceMeasurement(const QString& viewerId, bool enabled) = 0;
    
    /**
     * @brief 启用角度测量
     * @param viewerId 查看器ID
     * @param enabled 是否启用
     * @return 成功返回true，失败返回false
     */
    virtual bool enableAngleMeasurement(const QString& viewerId, bool enabled) = 0;
    
    /**
     * @brief 添加文本标注
     * @param viewerId 查看器ID
     * @param x X坐标
     * @param y Y坐标
     * @param text 标注文本
     * @return 标注ID，失败返回空字符串
     */
    virtual QString addTextAnnotation(const QString& viewerId, double x, double y, const QString& text) = 0;
    
    /**
     * @brief 移除标注
     * @param viewerId 查看器ID
     * @param annotationId 标注ID
     * @return 成功返回true，失败返回false
     */
    virtual bool removeAnnotation(const QString& viewerId, const QString& annotationId) = 0;
    
    /**
     * @brief 清除所有标注
     * @param viewerId 查看器ID
     * @return 成功返回true，失败返回false
     */
    virtual bool clearAllAnnotations(const QString& viewerId) = 0;

    // ==================== 高级功能 ====================
    
    /**
     * @brief 设置查看器布局（用于多图像显示）
     * @param viewerId 查看器ID
     * @param rows 行数
     * @param cols 列数
     * @return 成功返回true，失败返回false
     */
    virtual bool setViewerLayout(const QString& viewerId, int rows, int cols) = 0;
    
    /**
     * @brief 设置显示方向（轴状面、冠状面、矢状面）
     * @param viewerId 查看器ID
     * @param orientation 方向名称 ("axial", "coronal", "sagittal")
     * @return 成功返回true，失败返回false
     */
    virtual bool setImageOrientation(const QString& viewerId, const QString& orientation) = 0;
    
    /**
     * @brief 启用同步滚动（多查看器同步）
     * @param viewerIds 查看器ID列表
     * @param enabled 是否启用
     * @return 成功返回true，失败返回false
     */
    virtual bool enableSynchronizedScrolling(const QStringList& viewerIds, bool enabled) = 0;
    
    /**
     * @brief 截屏查看器内容
     * @param viewerId 查看器ID
     * @param filePath 保存路径
     * @return 成功返回true，失败返回false
     */
    virtual bool captureViewerImage(const QString& viewerId, const QString& filePath) = 0;

    // ==================== 专业体绘制功能（整合自NrrdViewer） ====================
    
    /**
     * @brief 检查图像是否为NRRD格式
     * @param imageId 图像ID
     * @return 是否为NRRD格式
     */
    virtual bool isNrrdImage(const QString& imageId) const = 0;
    
    /**
     * @brief 设置体绘制不透明度
     * @param viewerId 查看器ID
     * @param opacity 不透明度 (0.0-1.0)
     * @return 成功返回true，失败返回false
     */
    virtual bool setVolumeOpacity(const QString& viewerId, double opacity) = 0;
    
    /**
     * @brief 设置传输函数
     * @param viewerId 查看器ID
     * @param transferFunction 传输函数参数
     * @return 成功返回true，失败返回false
     */
    virtual bool setTransferFunction(const QString& viewerId, const QVariantMap& transferFunction) = 0;
    
    /**
     * @brief 设置体绘制光照参数
     * @param viewerId 查看器ID
     * @param ambient 环境光强度
     * @param diffuse 漫反射强度
     * @param specular 镜面反射强度
     * @return 成功返回true，失败返回false
     */
    virtual bool setVolumeLightingParameters(const QString& viewerId, double ambient, double diffuse, double specular) = 0;
    
    /**
     * @brief 设置体绘制材质属性
     * @param viewerId 查看器ID
     * @param material 材质参数映射
     * @return 成功返回true，失败返回false
     */
    virtual bool setVolumeMaterialProperties(const QString& viewerId, const QVariantMap& material) = 0;
    
    /**
     * @brief 设置体绘制相机位置
     * @param viewerId 查看器ID
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @return 成功返回true，失败返回false
     */
    virtual bool setVolumeCameraPosition(const QString& viewerId, double x, double y, double z) = 0;
    
    /**
     * @brief 设置体绘制相机焦点
     * @param viewerId 查看器ID
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @return 成功返回true，失败返回false
     */
    virtual bool setVolumeCameraFocalPoint(const QString& viewerId, double x, double y, double z) = 0;
    
    /**
     * @brief 重置体绘制相机视角
     * @param viewerId 查看器ID
     * @return 成功返回true，失败返回false
     */
    virtual bool resetVolumeCamera(const QString& viewerId) = 0;
    
    /**
     * @brief 设置体绘制裁剪平面
     * @param viewerId 查看器ID
     * @param plane 裁剪平面参数
     * @return 成功返回true，失败返回false
     */
    virtual bool setVolumeClippingPlane(const QString& viewerId, const QVariantMap& plane) = 0;
    
    /**
     * @brief 设置体绘制采样距离
     * @param viewerId 查看器ID
     * @param distance 采样距离
     * @return 成功返回true，失败返回false
     */
    virtual bool setVolumeSampleDistance(const QString& viewerId, double distance) = 0;
    
    /**
     * @brief 启用体绘制渐变阴影
     * @param viewerId 查看器ID
     * @param enabled 是否启用
     * @return 成功返回true，失败返回false
     */
    virtual bool setVolumeGradientShading(const QString& viewerId, bool enabled) = 0;
    
    /**
     * @brief 启用/禁用体绘制交互
     * @param viewerId 查看器ID
     * @param enabled 是否启用
     * @return 成功返回true，失败返回false
     */
    virtual bool setVolumeInteractionEnabled(const QString& viewerId, bool enabled) = 0;

    // ==================== 科研级可视化功能（整合自NrrdViewer） ====================
    
    /**
     * @brief 获取体数据统计信息
     * @param viewerId 查看器ID
     * @param imageId 图像ID
     * @return 统计信息映射
     */
    virtual QVariantMap getVolumeStatistics(const QString& viewerId, const QString& imageId) = 0;
    
    /**
     * @brief 分析体数据分布
     * @param viewerId 查看器ID
     * @param imageId 图像ID
     * @return 分析结果映射
     */
    virtual QVariantMap analyzeVolumeDistribution(const QString& viewerId, const QString& imageId) = 0;
    
    /**
     * @brief 设置高级渲染算法
     * @param viewerId 查看器ID
     * @param algorithm 算法名称 ("ray_casting", "texture_mapping", "shear_warp")
     * @return 成功返回true，失败返回false
     */
    virtual bool setAdvancedRenderingAlgorithm(const QString& viewerId, const QString& algorithm) = 0;
    
    /**
     * @brief 设置渲染质量等级
     * @param viewerId 查看器ID
     * @param quality 质量等级 ("low", "medium", "high", "ultra")
     * @return 成功返回true，失败返回false
     */
    virtual bool setRenderingQuality(const QString& viewerId, const QString& quality) = 0;
    
    /**
     * @brief 导出体绘制结果
     * @param viewerId 查看器ID
     * @param filePath 导出路径
     * @param format 导出格式 ("png", "jpg", "tiff", "vtk")
     * @param options 导出选项
     * @return 成功返回true，失败返回false
     */
    virtual bool exportVolumeRendering(const QString& viewerId, const QString& filePath, 
                                     const QString& format, const QVariantMap& options = QVariantMap()) = 0;

    // ==================== 查看器管理 ====================
    
    /**
     * @brief 获取可用的查看器类型
     * @return 查看器类型列表
     */
    virtual QStringList getAvailableViewerTypes() const = 0;
    
    /**
     * @brief 获取所有活动查看器ID
     * @return 查看器ID列表
     */
    virtual QStringList getActiveViewers() const = 0;
    
    /**
     * @brief 关闭查看器
     * @param viewerId 查看器ID
     * @return 成功返回true，失败返回false
     */
    virtual bool closeViewer(const QString& viewerId) = 0;
    
    /**
     * @brief 获取查看器信息
     * @param viewerId 查看器ID
     * @return 查看器信息映射
     */
    virtual QVariantMap getViewerInfo(const QString& viewerId) const = 0;
    
    /**
     * @brief 设置查看器参数
     * @param viewerId 查看器ID
     * @param parameters 参数映射
     * @return 成功返回true，失败返回false
     */
    virtual bool setViewerParameters(const QString& viewerId, const QVariantMap& parameters) = 0;

    // ==================== UI显示管理（遵循PatientManagement成功模式） ====================
    
    /**
     * @brief 显示图像查看器对话框
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showViewerDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示多平面重建对话框
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showMPRDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示查看器配置对话框
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showViewerConfigDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示体绘制配置对话框（整合自NrrdViewer）
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showVolumeRenderingDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示NRRD专用查看器对话框（整合自NrrdViewer）
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showNrrdViewerDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示传输函数编辑器对话框（整合自NrrdViewer）
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showTransferFunctionEditorDialog(QWidget* parent = nullptr) = 0;
    
    /**
     * @brief 显示科研级可视化配置对话框（整合自NrrdViewer）
     * @param parent 父窗口（可选）
     * @return 成功返回true，失败返回false
     */
    virtual bool showScientificVisualizationDialog(QWidget* parent = nullptr) = 0;

signals:
    /**
     * @brief 查看器创建信号
     * @param viewerId 查看器ID
     * @param viewerType 查看器类型
     */
    void viewerCreated(const QString& viewerId, const QString& viewerType);
    
    /**
     * @brief 图像显示完成信号
     * @param viewerId 查看器ID
     * @param imageId 图像ID
     */
    void imageDisplayed(const QString& viewerId, const QString& imageId);
    
    /**
     * @brief 窗口/级别变化信号
     * @param viewerId 查看器ID
     * @param window 窗口宽度
     * @param level 窗口中心
     */
    void windowLevelChanged(const QString& viewerId, double window, double level);
    
    /**
     * @brief 切片变化信号
     * @param viewerId 查看器ID
     * @param sliceIndex 切片索引
     */
    void sliceChanged(const QString& viewerId, int sliceIndex);
    
    /**
     * @brief 鼠标点击信号
     * @param viewerId 查看器ID
     * @param x X坐标
     * @param y Y坐标
     * @param worldX 世界坐标X
     * @param worldY 世界坐标Y
     * @param worldZ 世界坐标Z
     */
    void mouseClicked(const QString& viewerId, double x, double y, double worldX, double worldY, double worldZ);
    
    /**
     * @brief 测量完成信号
     * @param viewerId 查看器ID
     * @param measurementType 测量类型
     * @param value 测量值
     * @param unit 单位
     */
    void measurementCompleted(const QString& viewerId, const QString& measurementType, double value, const QString& unit);
    
    /**
     * @brief 查看器关闭信号
     * @param viewerId 查看器ID
     */
    void viewerClosed(const QString& viewerId);
    
    // ==================== 体绘制相关信号（整合自NrrdViewer） ====================
    
    /**
     * @brief NRRD图像显示完成信号
     * @param viewerId 查看器ID
     * @param imageId 图像ID
     */
    void nrrdImageDisplayed(const QString& viewerId, const QString& imageId);
    
    /**
     * @brief 体绘制参数变化信号
     * @param viewerId 查看器ID
     * @param parameters 参数映射
     */
    void volumeRenderingParametersChanged(const QString& viewerId, const QVariantMap& parameters);
    
    /**
     * @brief 传输函数变化信号
     * @param viewerId 查看器ID
     * @param transferFunction 传输函数参数
     */
    void transferFunctionChanged(const QString& viewerId, const QVariantMap& transferFunction);
    
    /**
     * @brief 体绘制相机位置变化信号
     * @param viewerId 查看器ID
     * @param position 相机位置
     * @param focalPoint 焦点位置
     */
    void volumeCameraChanged(const QString& viewerId, const QVariantMap& position, const QVariantMap& focalPoint);
    
    /**
     * @brief 体数据分析完成信号
     * @param viewerId 查看器ID
     * @param imageId 图像ID
     * @param statistics 统计结果
     */
    void volumeAnalysisCompleted(const QString& viewerId, const QString& imageId, const QVariantMap& statistics);
    
    /**
     * @brief 体绘制导出完成信号
     * @param viewerId 查看器ID
     * @param filePath 导出路径
     * @param success 是否成功
     */
    void volumeExportCompleted(const QString& viewerId, const QString& filePath, bool success);
    
    /**
     * @brief 错误信号
     * @param viewerId 查看器ID
     * @param error 错误信息
     */
    void viewerError(const QString& viewerId, const QString& error);
};

// Qt接口声明
Q_DECLARE_INTERFACE(MedicalViewerService, "medical.MedicalViewerService")

#endif // MEDICAL_VIEWER_SERVICE_H