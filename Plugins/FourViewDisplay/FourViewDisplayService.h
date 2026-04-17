#ifndef FOUR_VIEW_DISPLAY_SERVICE_H
#define FOUR_VIEW_DISPLAY_SERVICE_H

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "Framework/ImageDataTransfer.h"

// 前向声明
class vtkRenderWindow;
class vtkImageData;

/**
 * @brief 四视图显示服务接口
 * 
 * 提供轴位、矢状位、冠状位和3D视图的显示服务接口。
 * 支持加载和显示nii.gz医学影像文件。
 * 这是一个纯虚接口，遵循CTK服务架构设计原则。
 */
class FourViewDisplayService : public QObject
{
    Q_OBJECT
    
public:
    explicit FourViewDisplayService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~FourViewDisplayService() = default;
    
    // ========== 文件加载 ==========
    
    /**
     * @brief 加载nii.gz医学影像文件
     * @param filePath 文件路径
     * @return 成功返回true，失败返回false
     */
    virtual bool loadImageFile(const QString& filePath) = 0;
    
    /**
     * @brief 预热ITK ImageIO（在主线程调用），只读取头信息，避免首次在后台线程初始化导致的不稳定
     * @param filePath 文件路径
     * @return 成功返回true
     */
    virtual bool warmUpImageIOForFile(const QString& filePath) = 0;
    
    /**
     * @brief 异步读取图像数据（仅ITK读取，不涉及VTK渲染，可在后台线程调用）
     * @param filePath 文件路径
     * @return vtkImageData指针，失败返回nullptr
     * @note 此方法只执行文件读取，不更新VTK视图，线程安全
     */
    virtual vtkImageData* readImageDataFromFile(const QString& filePath) = 0;
    
    /**
     * @brief 使用已读取的图像数据更新VTK视图（必须在主线程调用）
     * @param imageData vtkImageData指针
     * @param filePath 原始文件路径（用于记录）
     * @return 成功返回true，失败返回false
     * @note 此方法涉及OpenGL操作，必须在主线程中调用
     */
    virtual bool updateViewsWithImageData(vtkImageData* imageData, const QString& filePath) = 0;
    
    /**
     * @brief 加载多个nii.gz文件
     * @param filePaths 文件路径列表
     * @return 成功返回true，失败返回false
     */
    virtual bool loadImageFiles(const QStringList& filePaths) = 0;
    
    /**
     * @brief 清除当前加载的影像
     * @return 成功返回true，失败返回false
     */
    virtual bool clearImage() = 0;
    
    // ========== 视图控制 ==========
    
    /**
     * @brief 设置窗宽窗位
     * @param windowWidth 窗宽
     * @param windowLevel 窗位
     */
    virtual void setWindowLevel(double windowWidth, double windowLevel) = 0;
    
    /**
     * @brief 获取当前窗宽
     * @return 窗宽值
     */
    virtual double getWindowWidth() const = 0;
    
    /**
     * @brief 获取当前窗位
     * @return 窗位值
     */
    virtual double getWindowLevel() const = 0;
    
    // ========== 数据请求 ==========

    /**
     * @brief 请求当前加载的体数据
     * @param windowWidth 可选的窗宽覆盖（<0 表示使用当前值）
     * @param windowLevel 可选的窗位覆盖（<0 表示使用当前值）
     */
    virtual void requestVolumeData(double windowWidth = -1.0, double windowLevel = -1.0) = 0;

    // ========== 信息查询 ==========
    
    /**
     * @brief 获取当前加载的文件路径
     * @return 文件路径
     */
    virtual QString getCurrentFilePath() const = 0;
    
    /**
     * @brief 获取影像尺寸信息
     * @param width 宽度（输出）
     * @param height 高度（输出）
     * @param depth 深度（输出）
     * @return 成功返回true，失败返回false
     */
    virtual bool getImageDimensions(int& width, int& height, int& depth) const = 0;
    
    /**
     * @brief 获取影像间距信息
     * @param spacingX X方向间距（输出）
     * @param spacingY Y方向间距（输出）
     * @param spacingZ Z方向间距（输出）
     * @return 成功返回true，失败返回false
     */
    virtual bool getImageSpacing(double& spacingX, double& spacingY, double& spacingZ) const = 0;
    
    /**
     * @brief 是否已加载影像
     * @return 已加载返回true，未加载返回false
     */
    virtual bool isImageLoaded() const = 0;
    
    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息字符串
     */
    virtual QString getLastError() const = 0;
    
    // ========== 视图交互 ==========
    
    /**
     * @brief 获取当前轴位切片索引
     * @return 切片索引
     */
    virtual int getAxialSlice() const = 0;
    virtual int getSagittalSlice() const = 0;
    virtual int getCoronalSlice() const = 0;

    // ========== 调试信息 ==========
    
    /**
     * @brief 打印当前所有视图的显示参数（调试用）
     */
    virtual void printCurrentViewParameters() const = 0;
    
    /**
     * @brief 更新所有视图
     * @details 重新渲染所有四个视图，应用当前的相机参数和窗位窗宽
     */
    virtual void updateAllViews() = 0;
    
    /**
     * @brief 渐进式更新所有视图（先显示低质量预览，后台生成高质量版本）
     * @param renderWindow3D 3D视图的渲染窗口（用于刷新）
     */
    virtual void updateAllViewsProgressive(vtkRenderWindow* renderWindow3D = nullptr) = 0;

    // ========== Widget创建 ==========

    /**
     * @brief 创建纯VTK渲染Widget（不包含控制UI，无闪烁）
     * @param parent 父Widget
     * @return 创建的Widget指针
     * @note 这个Widget只包含4个VTK渲染窗口，控制UI需要在调用方实现
     *       这种设计可以避免VTK渲染时触发整个插件Widget的重绘
     */
    virtual QWidget* createFourViewVTKWidget(QWidget* parent = nullptr) = 0;

    // ========== 切片控制（用于外部控制UI） ==========

    /**
     * @brief 设置轴位切片
     * @param slice 切片索引
     */
    virtual void setAxialSlice(int slice) = 0;

    /**
     * @brief 设置矢状位切片
     * @param slice 切片索引
     */
    virtual void setSagittalSlice(int slice) = 0;

    /**
     * @brief 设置冠状位切片
     * @param slice 切片索引
     */
    virtual void setCoronalSlice(int slice) = 0;

    /**
     * @brief 设置3D透明度
     * @param opacity 透明度值 (0.0-1.0)
     */
    virtual void set3DOpacity(double opacity) = 0;

    /**
     * @brief 重置所有视图
     */
    virtual void resetViews() = 0;

    /**
     * @brief 获取切片范围
     */
    virtual int getAxialSliceMin() const = 0;
    virtual int getAxialSliceMax() const = 0;
    virtual int getSagittalSliceMin() const = 0;
    virtual int getSagittalSliceMax() const = 0;
    virtual int getCoronalSliceMin() const = 0;
    virtual int getCoronalSliceMax() const = 0;

    // ========== VTK渲染控制（防闪烁） ==========

    /**
     * @brief 暂停VTK渲染
     * @note 在页面切换前调用，防止隐藏的VTK Widget继续渲染导致闪烁
     */
    virtual void pauseRendering() = 0;

    /**
     * @brief 恢复VTK渲染
     * @note 在页面切换后调用
     */
    virtual void resumeRendering() = 0;

    // ========== 导航工具位置叠加 ==========

    /**
     * @brief 更新工具位置（用于术中导航叠加显示）
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @note 在四视图上显示工具的实时位置
     */
    virtual void updateToolPosition(double x, double y, double z) = 0;

    /**
     * @brief 设置工具模型可见性
     * @param visible 是否可见
     */
    virtual void setToolModelVisible(bool visible) = 0;

    /**
     * @brief 加载工具3D模型
     * @param modelPath STL模型文件路径
     * @return 成功返回true
     */
    virtual bool loadToolModel(const QString& modelPath) = 0;

    /**
     * @brief 更新工具位姿（包含旋转）
     * @param position 位置 [x, y, z]
     * @param orientation 方向四元数 [qw, qx, qy, qz]
     */
    virtual void updateToolPose(const QList<double>& position, const QList<double>& orientation) = 0;

signals:
    /**
     * @brief 影像加载成功信号
     * @param filePath 文件路径
     */
    void imageLoaded(const QString& filePath);
    
    /**
     * @brief 影像加载失败信号
     * @param filePath 文件路径
     * @param errorMessage 错误消息
     */
    void imageLoadFailed(const QString& filePath, const QString& errorMessage);
    
    /**
     * @brief 视图更新信号
     */
    void viewsUpdated();
    
    /**
     * @brief 窗宽窗位改变信号
     * @param windowWidth 窗宽
     * @param windowLevel 窗位
     */
    void windowLevelChanged(double windowWidth, double windowLevel);
    
    /**
     * @brief 服务错误信号
     * @param errorMessage 错误消息
     */
    void serviceError(const QString& errorMessage);
    
    /**
     * @brief 体数据就绪信号
     */
    void volumeDataReady(const ImageData& data);
};

// 声明为CTK服务接口
Q_DECLARE_INTERFACE(FourViewDisplayService, "com.medicalpro.FourViewDisplayService")

#endif // FOUR_VIEW_DISPLAY_SERVICE_H
