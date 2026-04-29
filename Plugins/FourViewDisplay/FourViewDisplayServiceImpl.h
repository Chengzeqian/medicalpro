#ifndef FOUR_VIEW_DISPLAY_SERVICE_IMPL_H
#define FOUR_VIEW_DISPLAY_SERVICE_IMPL_H

#include "FourViewDisplayService.h"
#include <QString>
#include <QHash>
#include <QStringList>
#include <QString>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkVolume.h>
#include <vtkSphereSource.h>

#include <atomic>

// 前向声明
class vtkPolyData;
class vtkRenderWindow;

/**
 * @brief 四视图显示服务实现类
 * 
 * 实现FourViewDisplayService接口，提供四视图显示的具体功能。
 */
class FourViewDisplayServiceImpl : public FourViewDisplayService
{
    Q_OBJECT
    Q_INTERFACES(FourViewDisplayService)
    
public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit FourViewDisplayServiceImpl(QObject* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~FourViewDisplayServiceImpl() override;
    
    // ========== 实现FourViewDisplayService接口 ==========
    
    bool loadImageFile(const QString& filePath) override;
    bool warmUpImageIOForFile(const QString& filePath) override;
    vtkImageData* readImageDataFromFile(const QString& filePath) override;
    bool updateViewsWithImageData(vtkImageData* imageData, const QString& filePath) override;
    bool loadImageFiles(const QStringList& filePaths) override;
    bool clearImage() override;
    
    void setWindowLevel(double windowWidth, double windowLevel) override;
    double getWindowWidth() const override;
    double getWindowLevel() const override;

    void set3DRenderMode(const QString& mode);
    void set3DColor(int r, int g, int b);

    QString getCurrentFilePath() const override;
    bool getImageDimensions(int& width, int& height, int& depth) const override;
    bool getImageSpacing(double& spacingX, double& spacingY, double& spacingZ) const override;
    bool isImageLoaded() const override;
    QString getLastError() const override;
    
    // 视图交互
    int getAxialSlice() const override;
    int getSagittalSlice() const override;
    int getCoronalSlice() const override;
    
    void requestVolumeData(double windowWidth = -1.0, double windowLevel = -1.0) override;
    
    // 调试信息
    void printCurrentViewParameters() const override;
    void updateAllViews() override;
    void updateAllViewsProgressive(vtkRenderWindow* renderWindow3D = nullptr) override;

    // Widget创建
    QWidget* createFourViewVTKWidget(QWidget* parent = nullptr) override;

    // 切片控制
    void setAxialSlice(int slice) override;
    void setSagittalSlice(int slice) override;
    void setCoronalSlice(int slice) override;
    void set3DOpacity(double opacity) override;
    void resetViews() override;
    int getAxialSliceMin() const override;
    int getAxialSliceMax() const override;
    int getSagittalSliceMin() const override;
    int getSagittalSliceMax() const override;
    int getCoronalSliceMin() const override;
    int getCoronalSliceMax() const override;

    // VTK渲染控制（防闪烁）
    void pauseRendering() override;
    void resumeRendering() override;

    // 导航工具位置叠加
    void updateToolPosition(double x, double y, double z) override;
    void setToolModelVisible(bool visible) override;
    bool loadToolModel(const QString& modelPath) override;
    void updateToolPose(const QList<double>& position, const QList<double>& orientation) override;

    // Widget 状态管理
    
private:
    /**
     * @brief 确保VTK组件已初始化（延迟初始化）
     */
    void ensureInitialized();
    
    /**
     * @brief 初始化VTK组件
     */
    void initializeVTKComponents();
    
    /**
     * @brief 更新3D视图
     */
    void update3DView();
    
    /**
     * @brief 创建3D表面渲染
     */
    void create3DSurfaceRendering();
    
    /**
     * @brief 创建3D体渲染
     */
    void create3DVolumeRendering();

    // 缓存与磁盘持久化相关
    QString extractPatientId(const QString& filePath) const;
    vtkImageData* getCachedImageClone(const QString& filePath);
    void storeImageInCache(const QString& filePath, vtkImageData* imageData);
    QString cacheDirForPatient(const QString& patientId) const;
    QString cacheFilePathForImage(const QString& filePath) const; // .vti path
    vtkImageData* tryReadDiskCache(const QString& filePath);
    void writeDiskCache(const QString& filePath, vtkImageData* imageData);
    
    // 3D Mesh 缓存相关
    QString meshCacheFilePath(const QString& imagePath) const; // .vtp path
    vtkPolyData* tryReadMeshCache(const QString& imagePath);
    void writeMeshCache(const QString& imagePath, vtkPolyData* polyData);
    
    // 渐进式3D渲染
    void create3DSurfaceRenderingProgressive(bool isPreview = false);
    
private:
    // VTK组件（核心数据，不包含Qt Widget）
    vtkSmartPointer<vtkImageData> m_imageData;           // 影像数据
    vtkSmartPointer<vtkRenderer> m_3dRenderer;           // 3D渲染器
    vtkSmartPointer<vtkActor> m_3dActor;                 // 表面渲染Actor
    vtkSmartPointer<vtkVolume> m_3dVolume;               // 体渲染Volume

    // 状态变量
    QString m_currentFilePath;
    QString m_lastError;
    double m_windowWidth;
    double m_windowLevel;
    QString m_3dRenderMode;  // "surface" 或 "volume"
    double m_3dOpacity;
    int m_3dColorR;
    int m_3dColorG;
    int m_3dColorB;
    bool m_initialized;
    int m_axialSlice;
    int m_sagittalSlice;
    int m_coronalSlice;

    // 按患者维度的简单LRU缓存（容量较小，避免占用过多内存）
    // patientId -> (filePath -> image)
    QHash<QString, QHash<QString, vtkSmartPointer<vtkImageData>>> m_patientToImages;
    // patientId -> order list (most-recent first)
    QHash<QString, QStringList> m_patientCacheOrder;
    int m_cacheCapacityPerPatient = 2;  // 每个患者最多缓存2个影像
    
    // Widget 创建辅助状态

    // 纯VTK Widget（无控制UI）
    class FourViewVTKWidget* m_vtkWidget;

    // 导航工具叠加显示
    vtkSmartPointer<vtkActor> m_toolActor;              // 工具模型Actor
    vtkSmartPointer<vtkPolyData> m_toolPolyData;        // 工具模型数据
    vtkSmartPointer<vtkSphereSource> m_toolSphere;      // 默认球形表示
    bool m_toolModelVisible;
    double m_toolPosition[3];
};

#endif // FOUR_VIEW_DISPLAY_SERVICE_IMPL_H

