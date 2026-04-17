#ifndef DICOMVIEWERWIDGET_H
#define DICOMVIEWERWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QListWidget>
#include <QSplitter>
#include <QString>
#include <QPixmap>
#include <QScrollArea>

// CTK框架支持
#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkPluginContext.h>
class DicomViewerService;
class ctkEventAdmin;
#endif

/**
 * @brief DICOM影像查看器Widget
 * 
 * 设计理念：
 * - 封装完整的DICOM查看功能（UI + 交互）
 * - 使用DicomViewerService处理数据和算法
 * - 支持多序列浏览、窗宽窗位调整、测量标注
 * - 可独立使用，也可嵌入到任何Page中
 * 
 * 使用场景：
 * - Dashboard中查看患者影像
 * - 手术规划中浏览CT数据
 * - 术后对比分析
 */
class DicomViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DicomViewerWidget(QWidget *parent = nullptr);

    /**
     * @brief 带服务引用的构造函数（推荐使用）
     * @param service DicomViewerService服务引用
     * @param parent 父Widget
     * @note 通过服务工厂方法创建时使用此构造函数
     */
    explicit DicomViewerWidget(DicomViewerService* service, QWidget *parent = nullptr);

    ~DicomViewerWidget();

#ifdef CTK_PLUGIN_FRAMEWORK
    // 初始化 CTK 服务
    void initializeCTKService(ctkPluginContext* context);
#endif

    // ========== 数据加载 ==========
    
    /**
     * @brief 加载患者的所有DICOM数据
     * @param patientId 患者ID
     */
    void loadPatientDicomData(int patientId);
    
    /**
     * @brief 加载指定序列
     * @param seriesId 序列ID
     */
    void loadSeries(int seriesId);
    
    /**
     * @brief 加载单个DICOM图像
     * @param imageId 图像ID
     */
    void loadImage(int imageId);
    
    /**
     * @brief 清除当前显示
     */
    void clear();

    // ========== 窗宽窗位控制 ==========
    
    /**
     * @brief 设置窗宽窗位
     * @param windowWidth 窗宽
     * @param windowLevel 窗位
     */
    void setWindowLevel(int windowWidth, int windowLevel);
    
    /**
     * @brief 应用预设窗宽窗位
     * @param presetName 预设名称（如"骨窗"、"软组织窗"）
     */
    void applyWindowPreset(const QString& presetName);
    
    /**
     * @brief 自动窗宽窗位
     */
    void autoWindowLevel();

    // ========== 视图控制 ==========
    
    /**
     * @brief 缩放图像
     * @param factor 缩放因子
     */
    void zoomImage(double factor);
    
    /**
     * @brief 重置视图
     */
    void resetView();
    
    /**
     * @brief 适应窗口
     */
    void fitToWindow();

    // ========== 测量和标注 ==========
    
    /**
     * @brief 启用距离测量模式
     * @param enabled 是否启用
     */
    void enableDistanceMeasurement(bool enabled);
    
    /**
     * @brief 启用角度测量模式
     * @param enabled 是否启用
     */
    void enableAngleMeasurement(bool enabled);
    
    /**
     * @brief 清除所有标注
     */
    void clearAnnotations();

    // ========== 导出功能 ==========
    
    /**
     * @brief 导出当前图像
     * @param filePath 保存路径
     */
    bool exportCurrentImage(const QString& filePath);
    
    /**
     * @brief 打印当前图像
     */
    void printImage();

signals:
    void imageLoaded(int imageId);
    void imageLoadFailed(const QString& error);
    void seriesChanged(int seriesId);
    void windowLevelChanged(int width, int level);
    void measurementCompleted(const QString& type, double value);
    void annotationAdded(int annotationId);

private slots:
    void onSeriesSelectionChanged();
    void onImageSliceChanged(int value);
    void onWindowWidthChanged(int value);
    void onWindowLevelChanged(int value);
    void onPresetChanged(int index);
    void onZoomInClicked();
    void onZoomOutClicked();
    void onResetViewClicked();
    void onFitToWindowClicked();
    void onMeasureDistanceClicked();
    void onMeasureAngleClicked();
    void onClearAnnotationsClicked();
    void onExportClicked();
    void onPrintClicked();
    void onImageLabelClicked(QMouseEvent* event);

private:
    // UI 初始化
    void setupUI();
    void setupSeriesPanel();
    void setupImageViewerPanel();
    void setupControlPanel();
    void setupToolbar();
    void setupConnections();
    
    // 显示更新
    void updateSeriesList(int patientId);
    void updateImageDisplay();
    void updateImageInfo();
    void updateSliderRange();
    void applyWindowLevelToImage();
    
    // 辅助方法
    QString formatWindowPresetName(const QString& name, int width, int level);
    QPixmap applyWindowLevelToPixmap(const QPixmap& source, int width, int level);
    
private:
    // UI 组件
    QSplitter* m_mainSplitter;
    
    // 左侧：序列列表面板
    QWidget* m_seriesPanel;
    QListWidget* m_seriesList;
    QLabel* m_seriesInfoLabel;
    
    // 中间：图像显示面板
    QWidget* m_imageViewerPanel;
    QScrollArea* m_scrollArea;
    QLabel* m_imageLabel;              // 显示图像
    QLabel* m_imageInfoLabel;          // 显示图像信息（层数、位置等）
    QSlider* m_imageSliceSlider;       // 切片滑块
    QLabel* m_sliceLabel;              // 切片信息
    
    // 右侧：控制面板
    QWidget* m_controlPanel;
    QSlider* m_windowWidthSlider;
    QSlider* m_windowLevelSlider;
    QLabel* m_windowWidthLabel;
    QLabel* m_windowLevelLabel;
    QComboBox* m_presetCombo;
    
    // 工具栏
    QWidget* m_toolbar;
    QPushButton* m_zoomInBtn;
    QPushButton* m_zoomOutBtn;
    QPushButton* m_resetViewBtn;
    QPushButton* m_fitToWindowBtn;
    QPushButton* m_measureDistanceBtn;
    QPushButton* m_measureAngleBtn;
    QPushButton* m_clearAnnotationsBtn;
    QPushButton* m_exportBtn;
    QPushButton* m_printBtn;
    
    // CTK 服务
#ifdef CTK_PLUGIN_FRAMEWORK
    ctkPluginContext* m_ctkContext;
    DicomViewerService* m_dicomService;
    ctkEventAdmin* m_eventAdmin;
#endif
    
    // 状态变量
    int m_currentPatientId;
    int m_currentSeriesId;
    int m_currentImageId;
    int m_currentImageIndex;           // 当前显示的图像索引（在序列中）
    QList<int> m_currentSeriesImages;  // 当前序列的所有图像ID
    
    // 显示参数
    int m_windowWidth;
    int m_windowLevel;
    double m_zoomFactor;
    
    // 测量和标注状态
    bool m_distanceMeasureMode;
    bool m_angleMeasureMode;
    QList<QPoint> m_measurePoints;     // 测量点缓存
    
    // 原始图像缓存
    QPixmap m_originalPixmap;
};

#endif // DICOMVIEWERWIDGET_H

