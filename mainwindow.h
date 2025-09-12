#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QDateEdit>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDate>
#include <QTimer>
#include <QVector3D>
#include <QCloseEvent>

#ifdef CTK_PLUGIN_FRAMEWORK
#include <ctkServiceReference.h>
#endif



QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

#ifdef CTK_PLUGIN_FRAMEWORK
class OpticalTrackingService;
class PatientDatabaseService;
class MedicalImageCoreService;
class ImageInteractionService;
class MedicalViewerService;
class MedicalProcessingService;
class CTKEnhancedLogger;
class ctkCollapsibleGroupBox;
class ctkDoubleSpinBox; 
class ctkSliderWidget;
class ctkColorPickerButton;
class ctkErrorLogWidget;
// 遵循CTK架构：主应用只通过服务接口访问插件功能，不直接创建插件UI组件
#endif

class QDockWidget;
class QGroupBox;
class QLabel;

// 旧的Patient和DiagnosisRecord结构体已移除
// 现在使用CTK插件架构中的PatientManagement插件定义的数据结构

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;  // 处理窗口关闭事件

private slots:
    // 基本应用程序操作
    void onExit();
    void onAbout();
    
    // 医学图像相关槽函数
    void onOpenNrrdViewer();
    
    // 光学追踪相关槽函数
    void onConnectDevice();
    void onStartTracking();
    void onStopTracking();
    void onGetStats();
    void onGeometryConfigChanged();
    void onDiagnoseDevice();
    
    // === 增强功能槽函数 ===
    void onOpenEnhancedTrackingWidget();      // 打开增强跟踪界面
    void onOpenPivotCalibrationModule();      // 打开针尖校准模块
    void onOpenUDPCommunicationModule();      // 打开UDP通信模块
    void onOpticalTrackingServiceAvailable(bool available);  // 服务可用性变化
    
    // === 患者管理插件槽函数 ===
    void onOpenPatientManagement();           // 打开患者管理插件
    void onOpenPatientInfoEntry();            // 打开患者信息录入
    void onOpenPatientListView();             // 打开患者列表查看
    void onPatientDatabaseServiceAvailable(bool available);  // 患者数据库服务可用性
    
    // === 医学图像管理插件槽函数 ===
    void onOpenMedicalImageManager();         // 打开医学图像管理界面
    void onOpenImagePropertiesViewer();       // 打开图像属性查看器
    void onOpenImageLoaderConfig();           // 打开图像加载器配置
    void onMedicalImageServiceAvailable(bool available);  // 医学图像服务可用性
    
    // === 图像交互插件槽函数 ===
    void onOpenImageInteractionDialog();      // 打开图像交互主界面
    void onOpenPointPickerDialog();           // 打开点拾取界面
    void onOpenMeasurementDialog();           // 打开测量工具界面
    void onOpenAnnotationDialog();            // 打开标注工具界面
    void onImageInteractionServiceAvailable(bool available);  // 图像交互服务可用性
    
    // === 医学查看器插件槽函数 ===
    void onOpenMedicalViewerDialog();         // 打开医学查看器主界面
    void onOpenMPRDialog();                   // 打开MPR多平面重建界面
    void onOpenVolumeRenderingDialog();       // 打开体绘制界面
    void onOpenViewerConfigDialog();          // 打开查看器配置界面
    void onOpenNrrdViewerDialog();            // 打开NRRD查看器界面
    void onOpenTransferFunctionEditorDialog();// 打开传输函数编辑器
    void onOpenScientificVisualizationDialog();// 打开科学可视化界面
    void onMedicalViewerServiceAvailable(bool available);  // 医学查看器服务可用性
    
    // === 医学处理插件槽函数 ===
    void onOpenMedicalProcessingDialog();     // 打开医学处理主界面
    void onOpenBatchProcessingDialog();       // 打开批量处理界面
    void onOpenAlgorithmConfigDialog();       // 打开算法配置界面
    void onMedicalProcessingServiceAvailable(bool available);  // 医学处理服务可用性
    
    // 光学追踪服务信号响应
    void onDeviceConnected();
    void onDeviceDisconnected();
    void onTrackingStateChanged(bool isTracking);
    void onMarkersDetected(int markerCount, const QList<QVector3D>& positions);

private:
    Ui::MainWindow *ui;
    
    // 旧的数据存储成员变量已移除，现在使用CTK插件架构
    
    // 私有方法
    void initializeUI();
    void setupConnections();
    // 旧患者管理方法已移除，现在使用CTK插件架构
    
    // 光学追踪相关方法
    void setupCTKPluginContext();               // 设置CTK插件上下文
    void initializeTrackingService();
    void initializePatientManagementService(ctkPluginContext* context);  // 初始化患者管理服务
    void initializeMedicalImageService(ctkPluginContext* context);      // 初始化医学图像服务
    void initializeImageInteractionService(ctkPluginContext* context);  // 初始化图像交互服务
    void initializeMedicalViewerService(ctkPluginContext* context);     // 初始化医学查看器服务
    void initializeMedicalProcessingService(ctkPluginContext* context); // 初始化医学处理服务
    void addPatientManagementMenuItem();                               // 添加患者管理菜单项
    void addMedicalImageMenuItem();                                     // 添加医学图像菜单项
    void addImageInteractionMenuItem();                                // 添加图像交互菜单项
    void addMedicalViewerMenuItem();                                   // 添加医学查看器菜单项
    void addMedicalProcessingMenuItem();                               // 添加医学处理菜单项
    void setupTrackingConnections(QObject* serviceImpl);
    void initializeEnhancedTrackingInterface();  // 初始化增强界面
    void initializeGeometryConfig();
    QString getGeometryDisplayName(const QString& filename);
    
#ifdef CTK_PLUGIN_FRAMEWORK
    // CTK增强组件方法
    void setupCTKEnhancedLogging();             // 设置增强日志系统
    void setupCTKTrackingParametersDock();      // 创建追踪参数停靠窗口
    void setupCTKStatusMonitorDock();           // 创建状态监控停靠窗口
    void setupCTKErrorLogDock();                // 创建错误日志停靠窗口
    void applyCTKMedicalTheme();                // 应用医疗软件主题
    void updateCTKStatusIndicators();           // 更新状态指示器
    void updateCTKFrameRateDisplay(double frameRate); // 更新帧率显示
    
    // CTK组件事件处理
    void onCTKPrecisionThresholdChanged(double value);
    void onCTKFrameRateTargetChanged(double value);
    void onCTKMarkerColorChanged(const QColor& color);
    void onCTKTipColorChanged(const QColor& color);
    void onCTKLogLevelChanged(int level);
#endif
    void updateDeviceStatus(const QString& deviceType, const QString& serialNumber, bool connected);
    void updateTrackingStatus(bool isTracking);
    void updateMarkerTable(const QList<QVector3D>& positions);
    void updateMarkersTableAdvanced(int markerCount, const QList<QVector3D>& positions, 
                                   const QList<double>& errors = QList<double>(), 
                                   const QList<double>& qualities = QList<double>());
    void updateDetectionRate(double rate);
    void updatePerformanceStats();
    void logTrackingMessage(const QString& message);

    // 光学追踪相关成员变量（无论是否有CTK都需要）
    QTimer* m_uiUpdateTimer;
    
    // 状态变量
    bool m_deviceConnected;
    bool m_trackingActive;
    int m_lastMarkerCount;
    int m_totalFrameCount;
    int m_validFrameCount;
    int m_markerDetectionCount;

#ifdef CTK_PLUGIN_FRAMEWORK
    // CTK插件上下文
    ctkPluginContext* m_ctkContext;
    
    // CTK特定的成员 - 只通过服务接口访问插件
    ctkServiceReference m_trackingServiceRef;
    OpticalTrackingService* m_trackingService;
    QString m_currentSessionId;
    
    // 患者管理插件服务
    ctkServiceReference m_patientServiceRef;
    PatientDatabaseService* m_patientService;
    
    // 医学图像管理插件服务
    ctkServiceReference m_imageServiceRef;
    MedicalImageCoreService* m_imageService;
    
    // 图像交互插件服务
    ctkServiceReference m_imageInteractionServiceRef;
    ImageInteractionService* m_imageInteractionService;
    
    // 医学查看器插件服务
    ctkServiceReference m_medicalViewerServiceRef;
    MedicalViewerService* m_medicalViewerService;
    
    // 医学处理插件服务
    ctkServiceReference m_medicalProcessingServiceRef;
    MedicalProcessingService* m_medicalProcessingService;
    
    // CTK增强组件
    CTKEnhancedLogger* m_enhancedLogger;
    
    // CTK UI停靠窗口
    QDockWidget* m_trackingParamsDock;
    QDockWidget* m_statusMonitorDock; 
    QDockWidget* m_errorLogDock;
    
    // CTK错误日志组件
    ctkErrorLogWidget* m_errorLogWidget;
    
    // CTK追踪参数控制组件
    ctkCollapsibleGroupBox* m_trackingParamsGroup;
    ctkDoubleSpinBox* m_precisionThresholdSpinBox;
    ctkSliderWidget* m_frameRateTargetSlider;
    ctkColorPickerButton* m_markerColorPicker;
    ctkColorPickerButton* m_tipColorPicker;
    
    // CTK状态监控组件
    ctkCollapsibleGroupBox* m_statusGroup;
    QLabel* m_deviceStatusLabel;
    QLabel* m_trackingStatusLabel;
    QLabel* m_frameRateStatusLabel;
    QLabel* m_markerCountStatusLabel;
    ctkColorPickerButton* m_deviceStatusIndicator;
    ctkColorPickerButton* m_trackingStatusIndicator;
#endif
};
#endif // MAINWINDOW_H
