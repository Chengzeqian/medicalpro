#ifndef MEDICAL_VIEWER_WIDGET_H
#define MEDICAL_VIEWER_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QToolBar>
#include <QAction>
#include <QProgressBar>
#include <QTextEdit>
#include <QCheckBox>
#include <QFrame>
#include <QStackedWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QTimer>
#include <QEvent>
#include <QGraphicsSceneMouseEvent>
#include <cmath>
#include <QMutex>

// Qt多线程相关
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

// CTK相关
#include <ctkPluginContext.h>

// 前向声明（完全CTK架构）
class MedicalViewerService;
class ctkEventAdmin;

#ifdef VTK_FOUND
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include <vtkPiecewiseFunction.h>
#include <vtkColorTransferFunction.h>
#include <vtkDataSet.h>
#include <vtkDataObject.h>
#include <vtkAbstractVolumeMapper.h>
#endif
class MedicalImageCoreService;

/**
 * @brief 多模式医学图像查看器主界面
 * 
 * 提供完整的医学图像显示功能：
 * - 2D图像查看（轴状面、冠状面、矢状面）
 * - 3D图像显示和交互
 * - MPR（多平面重建）
 * - 高级体绘制功能（整合自NrrdViewer）
 * - 科研级可视化工具
 * - 实时参数调整
 */
class MedicalViewerWidget : public QWidget
{
    Q_OBJECT

public:
    // MPR切片控制结构
    struct MPRViewInfo {
        QString imageId;
        QString viewType;       // "axial", "coronal", "sagittal"
        QLabel* imageLabel;     // 显示图像的标签
        QSlider* sliceSlider;   // 切片滑块
        QSpinBox* sliceSpinBox; // 切片数值框
        int totalSlices;        // 总切片数
        int currentSlice;       // 当前切片索引
        QFutureWatcher<QPixmap>* currentWatcher; // 当前异步任务监听器
        
        // 任务队列和同步控制
        QTimer* debounceTimer;   // 防抖动定时器
        QMutex* taskMutex;       // 任务互斥锁
        bool taskRunning;        // 任务运行标志
        int pendingSlice;        // 待处理的切片索引
    };
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit MedicalViewerWidget(QWidget* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~MedicalViewerWidget();

    /**
     * @brief 设置CTK插件上下文
     * @param context CTK插件上下文
     */
    void setPluginContext(ctkPluginContext* context);

    /**
     * @brief 显示图像
     * @param imageId 图像ID
     */
    void displayImage(const QString& imageId);
    
    /**
     * @brief 异步显示图像（避免界面卡死）
     * @param imageId 图像ID
     */
    void displayImageAsync(const QString& imageId);

    /**
     * @brief 切换显示模式
     * @param mode 显示模式（"2D", "3D", "MPR", "Volume", "Advanced"）
     */
    void setDisplayMode(const QString& mode);

public slots:
    /**
     * @brief 重置所有查看器
     */
    void resetAllViewers();
    
    /**
     * @brief 刷新图像列表（公共方法）
     */
    void refreshImageList();
    
    /**
     * @brief 处理加载显示按钮点击
     */
    void onLoadDisplayClicked();

private slots:
    // UI控制槽函数
    void onDisplayModeChanged();
    void onImageSelectionChanged();
    void onWindowLevelChanged();
    void onSliceChanged();
    void onZoomChanged();
    
    // MPR切片控制槽函数
    void onMPRSliceChanged(int sliceIndex);
    void regenerateMPRSlice(MedicalViewerWidget::MPRViewInfo* viewInfo);
    void clearMPRViews();
    
    // 体绘制控制槽函数
    void onOpacityChanged();
    void onTransferFunctionChanged();
    void onLightingChanged();
    void onRenderingQualityChanged();
    void onCameraReset();
    
    // 工具操作槽函数
    void onMeasurementToggled();
    void onAnnotationToggled();
    void onScreenshot();
    void onExportData();
    
    // 服务相关槽函数
    void onViewerServiceAvailable();
    void onImageServiceAvailable();
    void onImageLoadedFromService(const QString& imageId, const QString& filePath);

private:
    /**
     * @brief 初始化UI界面
     */
    void initializeUI();
    
    /**
     * @brief 设置现代化样式
     */
    void setupStyles();
    
    /**
     * @brief 渲染2D图像
     * @param imageId 图像ID
     * @param infoText 图像信息
     */
    void render2DImage(const QString& imageId, const QString& infoText);
    
    /**
     * @brief 渲染3D图像
     * @param imageId 图像ID
     * @param infoText 图像信息
     */
    void render3DImage(const QString& imageId, const QString& infoText);
    
    /**
     * @brief 渲染MPR图像
     * @param imageId 图像ID
     * @param infoText 图像信息
     */
    void renderMPRImage(const QString& imageId, const QString& infoText);
    
    /**
     * @brief 渲染体绘制图像
     * @param imageId 图像ID
     * @param infoText 图像信息
     */
    void renderVolumeImage(const QString& imageId, const QString& infoText);
    
    /**
     * @brief 渲染高级可视化
     * @param imageId 图像ID
     * @param infoText 图像信息
     */
    void renderAdvancedVisualization(const QString& imageId, const QString& infoText);
    
    /**
     * @brief 应用窗口/级别调整到图像
     * @param originalPixmap 原始图像
     * @param windowCenter 窗口中心 (级别)
     * @param windowWidth 窗口宽度
     * @return 调整后的图像
     */
    QPixmap applyWindowLevel(const QPixmap& originalPixmap, double windowCenter, double windowWidth);
    
    /**
     * @brief 创建窗口/级别控制面板
     * @param imageId 图像ID
     * @return 控制面板widget
     */
    QWidget* createWindowLevelControls(const QString& imageId);
    
    /**
     * @brief 创建3D渲染控制面板
     * @param imageId 图像ID
     * @return 控制面板widget
     */
    QWidget* create3DControls(const QString& imageId);
    
    /**
     * @brief 创建MPR视图组件
     * @param title 视图标题
     * @param imageId 图像ID
     * @param viewType 视图类型 (axial/coronal/sagittal)
     * @param viewWidth 视图宽度
     * @param viewHeight 视图高度
     * @param sliceCount 切片数量
     * @return MPR视图widget
     */
    QWidget* createMPRViewWidget(const QString& title, const QString& imageId, const QString& viewType, int viewWidth, int viewHeight, int sliceCount);
    
    /**
     * @brief 创建MPR概览组件
     * @param title 标题
     * @param imageId 图像ID
     * @param imgWidth 图像宽度
     * @param imgHeight 图像高度
     * @param imgDepth 图像深度
     * @return 概览widget
     */
    QWidget* createMPROverviewWidget(const QString& title, const QString& imageId, int imgWidth, int imgHeight, int imgDepth);
    
    /**
     * @brief 创建MPR控制面板
     * @param imageId 图像ID
     * @return 控制面板widget
     */
    QWidget* createMPRControls(const QString& imageId);
    
    /**
     * @brief 在2D查看器中启用测量工具
     * @param imageView 图像视图
     * @param measureType 测量类型 (distance/angle)
     */
    void enableMeasurement(QGraphicsView* imageView, const QString& measureType);
    
    /**
     * @brief 处理测量工具的鼠标点击事件
     * @param event 鼠标事件
     * @param scenePos 场景坐标
     */
    bool handleMeasurementClick(QPointF scenePos);
    
    /**
     * @brief 完成当前测量并显示结果
     */
    void finalizeMeasurement();
    
    /**
     * @brief 计算两点间距离
     * @param p1 点1
     * @param p2 点2
     * @return 距离（像素）
     */
    double calculateDistance(const QPointF& p1, const QPointF& p2);
    
    /**
     * @brief 计算三点组成的角度
     * @param p1 起始点
     * @param p2 顶点
     * @param p3 结束点
     * @return 角度（度）
     */
    double calculateAngle(const QPointF& p1, const QPointF& p2, const QPointF& p3);
    
    /**
     * @brief 设置3D相机视角
     * @param viewType 视角类型 ("front", "side", "top", "reset")
     */
    void setCamera3DView(const QString& viewType);
    
    /**
     * @brief 应用传输函数预设
     * @param presetIndex 预设索引
     * @param dataRange 数据范围 [min, max]
     */
    void applyTransferFunctionPreset(int presetIndex, double dataRange[2]);

protected:
    /**
     * @brief 事件过滤器，处理测量工具的鼠标事件
     * @param obj 事件对象
     * @param event 事件
     * @return 是否处理了事件
     */
    bool eventFilter(QObject* obj, QEvent* event) override;
    
    /**
     * @brief 连接信号槽
     */
    void connectSignals();
    
    /**
     * @brief 初始化服务连接
     */
    void initializeServiceConnections();
    
    /**
     * @brief 创建主工具栏
     */
    void createMainToolBar();
    
    /**
     * @brief 创建显示区域
     */
    void createDisplayArea();
    
    /**
     * @brief 创建控制面板
     */
    void createControlPanels();
    
    /**
     * @brief 创建2D查看器界面
     */
    QWidget* create2DViewerTab();
    
    /**
     * @brief 创建3D查看器界面
     */
    QWidget* create3DViewerTab();
    
    /**
     * @brief 创建MPR查看器界面
     */
    QWidget* createMPRViewerTab();
    
    /**
     * @brief 创建体绘制界面
     */
    QWidget* createVolumeRenderingTab();
    
    /**
     * @brief 创建科研级可视化界面
     */
    QWidget* createAdvancedVisualizationTab();
    
    /**
     * @brief 创建基本控制面板
     */
    QWidget* createBasicControlPanel();
    
    /**
     * @brief 创建体绘制控制面板
     */
    QWidget* createVolumeControlPanel();
    
    /**
     * @brief 创建测量工具面板
     */
    QWidget* createMeasurementPanel();
    
    /**
     * @brief 创建状态面板
     */
    QWidget* createStatusPanel();
    
    /**
     * @brief 更新状态信息
     * @param message 状态消息
     */
    void updateStatus(const QString& message);
    
    /**
     * @brief 获取MedicalImageCoreService实例（处理类型转换）
     * @return MedicalImageCoreService指针，失败返回nullptr
     */
    MedicalImageCoreService* getImageCoreService() const;
    
    /**
     * @brief 更新进度显示
     * @param value 进度值(0-100)
     */
    void updateProgress(int value);

    /**
     * @brief 初始化EventAdmin服务
     */
    void initializeEventAdmin();

    /**
     * @brief 处理图像事件
     * @param eventType 事件类型
     * @param imageId 图像ID
     */
    void handleImageEvent(const QString& eventType, const QString& imageId);

private:
    // CTK相关
    ctkPluginContext* m_pluginContext;
    MedicalViewerService* m_viewerService;
    QObject* m_imageService; // CTK服务对象，通过服务框架获取
    bool m_serviceConnected;
    ctkEventAdmin* m_eventAdmin; // 事件管理服务
    
    // 窗口/级别调整相关
    QPixmap m_originalPixmap;           // 原始图像数据
    QGraphicsPixmapItem* m_currentPixmapItem; // 当前显示的图像项
    QGraphicsView* m_currentImageView; // 当前图像视图
    double m_windowCenter;              // 窗口中心 (级别)
    double m_windowWidth;               // 窗口宽度
    double m_defaultWindowCenter;       // 默认窗口中心
    double m_defaultWindowWidth;        // 默认窗口宽度

    // 主界面布局
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_contentLayout;
    QSplitter* m_mainSplitter;
    
    // 工具栏
    QToolBar* m_mainToolBar;
    QAction* m_resetAction;
    QAction* m_screenshotAction;
    QAction* m_exportAction;
    QAction* m_settingsAction;
    
    // 显示区域
    QTabWidget* m_displayTabs;
    QWidget* m_2dViewerTab;
    QWidget* m_3dViewerTab;
    QWidget* m_mprViewerTab;
    QWidget* m_volumeRenderingTab;
    QWidget* m_advancedVisualizationTab;
    
    // 控制面板（优化后只保留状态面板）
    QWidget* m_controlTabs;        // 现在直接指向状态面板
    QWidget* m_statusPanel;
    
    // 保留必要的顶部工具栏组件
    QComboBox* m_displayModeCombo;    // 显示模式选择（顶部工具栏）
    QComboBox* m_imageSelector;       // 图像选择器（顶部工具栏）
    
    // 注意：原来的右侧控制面板组件已移除，功能整合到各查看器标签页内
    
#ifdef VTK_FOUND
    // VTK 3D渲染相关组件
    vtkSmartPointer<vtkRenderer> m_vtkRenderer;
    vtkSmartPointer<vtkVolume> m_vtkVolume;
    vtkSmartPointer<vtkVolumeProperty> m_vtkVolumeProperty;
    vtkSmartPointer<vtkPiecewiseFunction> m_vtkOpacityFunction;
    vtkSmartPointer<vtkColorTransferFunction> m_vtkColorFunction;
    QVTKOpenGLNativeWidget* m_vtkWidget;
#endif
    
    // 状态组件
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QTextEdit* m_logTextEdit;
    
    // 测量工具状态
    enum MeasurementMode {
        NoMeasurement,
        DistanceMeasurement,
        AngleMeasurement,
        AreaMeasurement
    };
    MeasurementMode m_currentMeasurementMode;
    QList<QPointF> m_measurementPoints;  // 存储测量点
    QGraphicsView* m_currentMeasurementView;  // 当前测量的视图
    
    // 当前状态
    QString m_currentImageId;
    QString m_currentDisplayMode;
    QStringList m_activeViewerIds;
    
    QList<MedicalViewerWidget::MPRViewInfo*> m_mprViews;  // MPR视图信息列表
    
    // 查看器复用管理
    QMap<QString, QWidget*> m_createdViewers;  // 模式 -> 查看器控件的映射
};

#endif // MEDICAL_VIEWER_WIDGET_H
