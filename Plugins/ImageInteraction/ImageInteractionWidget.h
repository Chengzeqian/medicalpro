#ifndef IMAGE_INTERACTION_WIDGET_H
#define IMAGE_INTERACTION_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
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
#include <QListWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QButtonGroup>

// CTK相关
#include <ctkPluginContext.h>

// 前向声明
class ImageInteractionService;
class UnifiedMedicalImageService;

/**
 * @brief 图像交互工具主界面
 * 
 * 提供完整的图像交互功能：
 * - 点选和标记工具
 * - 测量和标注
 * - 感兴趣区域(ROI)绘制
 * - 图像分割辅助
 * - 3D点云交互
 * - 坐标系统管理
 */
class ImageInteractionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageInteractionWidget(QWidget* parent = nullptr);
    ~ImageInteractionWidget();

    void setPluginContext(ctkPluginContext* context);
    void loadImageForInteraction(const QString& imageId);
    void setInteractionMode(const QString& mode);

public slots:
    void refreshImageList();
    void resetAllTools();

private slots:
    // 交互模式控制
    void onInteractionModeChanged();
    void onImageSelectionChanged();
    void onToolSelectionChanged();
    
    // 点选工具
    void onPointSelectionTool();
    void onRegionSelectionTool();
    void onSeedPointTool();
    
    // 测量工具
    void onDistanceMeasurement();
    void onAngleMeasurement();
    void onAreaMeasurement();
    void onVolumeMeasurement();
    
    // 标注工具
    void onTextAnnotation();
    void onArrowAnnotation();
    void onShapeAnnotation();
    
    // ROI工具
    void onRectangleROI();
    void onCircleROI();
    void onPolygonROI();
    void onFreehandROI();
    
    // 3D交互
    void on3DPointSelection();
    void on3DRegionGrowing();
    void on3DCrossSection();
    
    // 数据管理
    void onSaveInteractionData();
    void onExportResults();
    void onClearAll();
    
    // 服务相关
    void onInteractionServiceAvailable();
    void onImageServiceAvailable();

private:
    void initializeUI();
    void setupStyles();
    void connectSignals();
    void initializeServiceConnections();
    
    void createMainToolBar();
    void createInteractionArea();
    void createToolPanels();
    
    QWidget* createPointSelectionPanel();
    QWidget* createMeasurementPanel();
    QWidget* createAnnotationPanel();
    QWidget* createROIPanel();
    QWidget* create3DInteractionPanel();
    QWidget* createDataManagementPanel();
    
    void updateStatus(const QString& message);
    void updateProgress(int value);

private:
    // CTK相关
    ctkPluginContext* m_pluginContext;
    ImageInteractionService* m_interactionService;
    UnifiedMedicalImageService* m_imageService;
    bool m_serviceConnected;

    // 主界面布局
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;
    
    // 工具栏
    QToolBar* m_mainToolBar;
    QComboBox* m_interactionModeCombo;
    QComboBox* m_imageSelector;
    QButtonGroup* m_toolButtonGroup;
    
    // 交互区域
    QSplitter* m_interactionSplitter;
    QGraphicsView* m_imageView;
    QGraphicsScene* m_imageScene;
    
    // 工具面板
    QTabWidget* m_toolTabs;
    QWidget* m_pointSelectionPanel;
    QWidget* m_measurementPanel;
    QWidget* m_annotationPanel;
    QWidget* m_roiPanel;
    QWidget* m_3dInteractionPanel;
    QWidget* m_dataManagementPanel;
    
    // 工具控件
    QPushButton* m_pointToolBtn;
    QPushButton* m_regionToolBtn;
    QPushButton* m_seedToolBtn;
    QPushButton* m_distanceBtn;
    QPushButton* m_angleBtn;
    QPushButton* m_areaBtn;
    QPushButton* m_textAnnotationBtn;
    QPushButton* m_rectangleROIBtn;
    QPushButton* m_circleROIBtn;
    QPushButton* m_polygonROIBtn;
    
    // 参数控件
    QSlider* m_sensitivitySlider;
    QSpinBox* m_toleranceSpin;
    QCheckBox* m_snapToEdgeCheck;
    QCheckBox* m_realTimeUpdateCheck;
    
    // 结果显示
    QTableWidget* m_resultTable;
    QTextEdit* m_coordinateDisplay;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    
    // 当前状态
    QString m_currentImageId;
    QString m_currentMode;
    QString m_currentTool;
    QList<QPointF> m_selectedPoints;
    QList<QVariantMap> m_interactions;
};

#endif // IMAGE_INTERACTION_WIDGET_H
