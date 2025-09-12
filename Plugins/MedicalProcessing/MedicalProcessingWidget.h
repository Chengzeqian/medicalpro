#ifndef MEDICAL_PROCESSING_WIDGET_H
#define MEDICAL_PROCESSING_WIDGET_H

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
#include <QListWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QSplitter>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

// CTK相关
#include <ctkPluginContext.h>

// 前向声明
class MedicalProcessingService;
class MedicalImageCoreService;
class ctkEventAdmin;

/**
 * @brief 图像处理工作线程
 */
class ImageProcessingWorker : public QObject
{
    Q_OBJECT

public:
    explicit ImageProcessingWorker(QObject* parent = nullptr);

    void setProcessingParameters(const QString& imageId,
                               const QString& processingType,
                               const QVariantMap& parameters,
                               MedicalProcessingService* processingService);

public slots:
    void processImage();

signals:
    void progressUpdated(int percentage);
    void processingCompleted(const QString& operationId, const QString& resultImageId);
    void processingFailed(const QString& operationId, const QString& error);

private:
    QString m_imageId;
    QString m_processingType;
    QVariantMap m_parameters;
    MedicalProcessingService* m_processingService;
    QString m_operationId;
};

/**
 * @brief 医学图像处理操作主界面
 * 
 * 提供完整的医学图像处理功能：
 * - 基础图像处理（滤波、增强、降噪）
 * - 高级图像分割
 * - 配准和融合
 * - 3D重建和处理
 * - 批量处理操作
 * - 处理参数调整和预设
 * - 实时预览和对比
 */
class MedicalProcessingWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit MedicalProcessingWidget(QWidget* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~MedicalProcessingWidget();

    /**
     * @brief 设置CTK插件上下文
     * @param context CTK插件上下文
     */
    void setPluginContext(ctkPluginContext* context);

    /**
     * @brief 加载图像进行处理
     * @param imageId 图像ID
     */
    void loadImageForProcessing(const QString& imageId);

    /**
     * @brief 设置处理类型
     * @param processingType 处理类型
     */
    void setProcessingType(const QString& processingType);



public slots:
    /**
     * @brief 刷新图像列表
     */
    void refreshImageList();
    
    /**
     * @brief 重置所有参数
     */
    void resetAllParameters();
    
    /**
     * @brief 应用处理
     */
    void applyProcessing();
    
    /**
     * @brief 预览处理结果
     */
    void previewProcessing();

private slots:
    // UI控制槽函数
    void onProcessingTypeChanged();
    void onImageSelectionChanged();
    void onParameterChanged();
    void onPresetSelected();
    
    // 基础处理槽函数
    void onBasicFilterChanged();
    void onEnhancementChanged();
    void onNoiseReductionChanged();
    
    // 高级处理槽函数
    void onSegmentationTypeChanged();
    void onRegistrationChanged();
    void onReconstructionChanged();
    
    // 批量处理槽函数
    void onBatchProcessingSetup();
    void onBatchProcessingStart();
    void onBatchProcessingStop();
    
    // 结果管理槽函数
    void onSaveResult();
    void onExportResult();
    void onCompareResults();
    
    // 服务相关槽函数
    void onProcessingServiceAvailable();
    void onImageServiceAvailable();
    void onProcessingCompleted(const QString& operationId, const QString& resultImageId);
    void onProcessingFailed(const QString& operationId, const QString& error);
    void onProcessingProgress(const QString& operationId, int progress);

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
     * @brief 连接信号槽
     */
    void connectSignals();
    




    /**
     * @brief 初始化EventAdmin服务
     */
    void initializeEventAdmin();

    /**
     * @brief 初始化图像处理工作线程
     */
    void initializeProcessingThread();

    /**
     * @brief 初始化服务连接
     */
    void initializeServiceConnections();

    /**
     * @brief 设置图像列表轮询（备用方案）
     */
    void setupImageListPolling();

    /**
     * @brief 创建主工具栏
     */
    void createMainToolBar();
    
    /**
     * @brief 创建处理选择区域
     */
    void createProcessingSelectionArea();
    
    /**
     * @brief 创建图像预览区域
     */
    void createImagePreviewArea();
    
    /**
     * @brief 创建参数控制面板
     */
    void createParameterControlPanels();
    
    /**
     * @brief 创建基础处理面板
     */
    QWidget* createBasicProcessingPanel();
    
    /**
     * @brief 创建图像分割面板
     */
    QWidget* createSegmentationPanel();
    
    /**
     * @brief 创建配准融合面板
     */
    QWidget* createRegistrationPanel();
    
    /**
     * @brief 创建3D重建面板
     */
    QWidget* createReconstructionPanel();
    
    /**
     * @brief 创建批量处理面板
     */
    QWidget* createBatchProcessingPanel();
    
    /**
     * @brief 创建结果管理面板
     */
    QWidget* createResultManagementPanel();
    
    /**
     * @brief 更新预览图像
     */
    void updatePreviewImages();

    /**
     * @brief 加载并显示图像
     * @param imageId 图像ID
     * @param scene 图形场景
     * @param view 图形视图
     * @param label 标签
     * @param labelPrefix 标签前缀
     */
    void loadAndDisplayImage(const QString& imageId,
                           QGraphicsScene* scene,
                           QGraphicsView* view,
                           QLabel* label,
                           const QString& labelPrefix);

    /**
     * @brief 清除图像显示
     * @param scene 图形场景
     * @param label 标签
     * @param message 提示消息
     */
    void clearImageDisplay(QGraphicsScene* scene, QLabel* label, const QString& message);

    /**
     * @brief 更新参数显示
     */
    void updateParameterDisplay();
    
    /**
     * @brief 更新状态信息
     * @param message 状态消息
     */
    void updateStatus(const QString& message);
    
    /**
     * @brief 更新进度显示
     * @param value 进度值(0-100)
     */
    void updateProgress(int value);
    
    /**
     * @brief 更新处理进度
     * @param value 进度值(0-100)
     */
    void updateProcessingProgress(int value);
    
    /**
     * @brief 创建处理参数映射
     * @return 参数映射
     */
    QVariantMap createParameterMap();
    
    /**
     * @brief 验证处理参数
     * @return 是否有效
     */
    bool validateParameters();
    
    /**
     * @brief 获取图像核心服务（通过CTK服务接口）
     * @return 图像核心服务实例（QObject*类型）
     */
    QObject* getImageCoreService() const;

private:
    // CTK相关
    ctkPluginContext* m_pluginContext;
    MedicalProcessingService* m_processingService;
    MedicalImageCoreService* m_imageService;
    bool m_serviceConnected;

    // 多线程处理
    QThread* m_processingThread;
    ImageProcessingWorker* m_processingWorker;

    // CTK EventAdmin（用于发送事件）
    ctkEventAdmin* m_eventAdmin;



    // 主界面布局
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_contentLayout;
    QSplitter* m_mainSplitter;
    QSplitter* m_verticalSplitter;
    
    // 工具栏
    QToolBar* m_mainToolBar;
    QAction* m_previewAction;
    QAction* m_applyAction;
    QAction* m_resetAction;
    QAction* m_saveAction;
    QAction* m_exportAction;
    QAction* m_settingsAction;
    
    // 处理选择区域
    QComboBox* m_processingTypeCombo;
    QComboBox* m_operationComboBox;
    QComboBox* m_imageSelector;
    QComboBox* m_presetCombo;
    QPushButton* m_refreshBtn;
    
    // 图像预览区域
    QSplitter* m_previewSplitter;
    QGraphicsView* m_originalImageView;
    QGraphicsView* m_processedImageView;
    QGraphicsScene* m_originalImageScene;
    QGraphicsScene* m_processedImageScene;
    QLabel* m_originalImageLabel;
    QLabel* m_processedImageLabel;
    
    // 参数控制面板
    QTabWidget* m_parameterTabs;
    QWidget* m_basicProcessingPanel;
    QWidget* m_segmentationPanel;
    QWidget* m_registrationPanel;
    QWidget* m_reconstructionPanel;
    QWidget* m_batchProcessingPanel;
    QWidget* m_resultManagementPanel;
    
    // 基础处理控件
    QComboBox* m_filterTypeCombo;
    QSlider* m_filterStrengthSlider;
    QSlider* m_brightnessSlider;
    QSlider* m_contrastSlider;
    QSlider* m_gammaSlider;
    QCheckBox* m_noiseReductionCheck;
    QSlider* m_noiseReductionSlider;
    
    // 分割控件
    QComboBox* m_segmentationMethodCombo;
    QSlider* m_thresholdSlider;
    QSpinBox* m_thresholdSpinBox;  // 新增：阈值输入框
    QSpinBox* m_seedPointXSpin;
    QSpinBox* m_seedPointYSpin;
    QSlider* m_regionGrowingToleranceSlider;
    QCheckBox* m_morphologyCheck;
    
    // 配准控件
    QComboBox* m_registrationTypeCombo;
    QComboBox* m_referenceImageCombo;
    QComboBox* m_transformTypeCombo;
    QSlider* m_registrationAccuracySlider;
    QCheckBox* m_automaticRegistrationCheck;
    
    // 3D重建控件
    QComboBox* m_reconstructionMethodCombo;
    QSlider* m_isosurfaceValueSlider;
    QSlider* m_smoothingSlider;
    QCheckBox* m_decimationCheck;
    QSlider* m_decimationRatioSlider;
    
    // 批量处理控件
    QListWidget* m_batchImageList;
    QComboBox* m_batchOperationCombo;
    QPushButton* m_addToBatchBtn;
    QPushButton* m_removeFromBatchBtn;
    QPushButton* m_startBatchBtn;
    QPushButton* m_stopBatchBtn;
    QProgressBar* m_batchProgressBar;
    
    // 结果管理控件
    QTableWidget* m_resultTable;
    QPushButton* m_saveResultBtn;
    QPushButton* m_exportResultBtn;
    QPushButton* m_compareResultsBtn;
    QPushButton* m_deleteResultBtn;
    
    // 状态和日志
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QTextEdit* m_logTextEdit;
    
    // 当前状态
    QString m_currentImageId;
    QString m_currentProcessingType;
    QString m_currentOperationId;
    QStringList m_batchImageIds;
    QMap<QString, QString> m_processingResults; // operationId -> resultImageId
};

#endif // MEDICAL_PROCESSING_WIDGET_H
