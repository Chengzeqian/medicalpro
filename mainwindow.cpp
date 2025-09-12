#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Framework/CTKManager.h"
#include "Framework/CTKEnhancedLogger.h"
#include <ctkServiceReference.h>
#include <ctkPluginContext.h>
#include <ctkCollapsibleGroupBox.h>
#include <ctkDoubleSpinBox.h>
#include <ctkSliderWidget.h>
#include <ctkColorPickerButton.h>
#include <ctkErrorLogWidget.h>
#include "Plugins/MedicalImageCore/MedicalImageCoreService.h"
#include "Plugins/OpticalTracking/OpticalTrackingService.h"
#include "Plugins/PatientManagement/PatientDatabaseService.h"
#include "Plugins/ImageInteraction/ImageInteractionService.h"
#include "Plugins/MedicalViewer/MedicalViewerService.h"
#include "Plugins/MedicalProcessing/MedicalProcessingService.h"
// 遵循CTK架构：不直接包含插件内部UI组件头文件
// 注意：不再直接包含插件内部实现头文件，遵循CTK架构原则
#endif

#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QStandardPaths>
#include <QThreadPool>
#include <QThread>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <process.h>
// 取消Windows宏定义，避免与Qt/CTK冲突
#ifdef ERROR
#undef ERROR
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif
#include <cstdlib>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_uiUpdateTimer(new QTimer(this))
    , m_deviceConnected(false)
    , m_trackingActive(false)
    , m_lastMarkerCount(0)
    , m_totalFrameCount(0)
    , m_validFrameCount(0)
    , m_markerDetectionCount(0)
#ifdef CTK_PLUGIN_FRAMEWORK
    , m_ctkContext(nullptr)
    , m_trackingService(nullptr)
    , m_patientService(nullptr)
    , m_imageService(nullptr)
    , m_imageInteractionService(nullptr)
    , m_medicalViewerService(nullptr)
    , m_medicalProcessingService(nullptr)
    , m_enhancedLogger(nullptr)
    , m_trackingParamsDock(nullptr)
    , m_statusMonitorDock(nullptr)
    , m_errorLogDock(nullptr)
    , m_errorLogWidget(nullptr)
    , m_trackingParamsGroup(nullptr)
    , m_precisionThresholdSpinBox(nullptr)
    , m_frameRateTargetSlider(nullptr)
    , m_markerColorPicker(nullptr)
    , m_tipColorPicker(nullptr)
    , m_statusGroup(nullptr)
    , m_deviceStatusLabel(nullptr)
    , m_trackingStatusLabel(nullptr)
    , m_frameRateStatusLabel(nullptr)
    , m_markerCountStatusLabel(nullptr)
    , m_deviceStatusIndicator(nullptr)
    , m_trackingStatusIndicator(nullptr)
#endif
{
    ui->setupUi(this);
    initializeUI();
    setupConnections();
}

MainWindow::~MainWindow()
{
    qDebug() << "[MainWindow] 析构函数：开始强制清理所有资源...";
    
    // 1. 断开所有信号连接
    disconnect();
    
#ifdef CTK_PLUGIN_FRAMEWORK
    // 2. 清理服务引用
    m_patientService = nullptr;
    m_imageService = nullptr;
    m_imageInteractionService = nullptr;
    m_medicalViewerService = nullptr;
    m_medicalProcessingService = nullptr;
    m_trackingService = nullptr;
    
    // 3. 强制处理待处理事件
    QCoreApplication::processEvents();
    
    // 4. CTK框架会在main.cpp的aboutToQuit信号中停止
    // 这里只需要清理局部资源
    qDebug() << "[MainWindow] CTK框架清理将在应用退出时处理";
#endif
    
    // 5. 清理UI（最后执行）
    delete ui;
    
    // 6. 最后一次强制处理事件
    QCoreApplication::processEvents();
    
    qDebug() << "[MainWindow] 析构函数：资源清理完成";
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "[MainWindow] 关闭事件：开始强制终止所有线程...";
    
    // 1. 显示关闭进度（可选）
    setWindowTitle("正在关闭应用程序，强制终止所有线程...");
    
    // 2. 立即停止所有定时器
    if (m_uiUpdateTimer) {
        m_uiUpdateTimer->stop();
    }
    
    // 3. 强制清理Qt全局线程池（立即执行）
    qDebug() << "[MainWindow] 强制终止Qt全局线程池...";
    QThreadPool::globalInstance()->clear();  // 清空队列
    
    // 4. 限时等待，然后强制终止
    bool threadsCleared = false;
    for (int i = 0; i < 10 && !threadsCleared; ++i) {  // 最多等待1秒
        QCoreApplication::processEvents();
        QThread::msleep(100);
        threadsCleared = QThreadPool::globalInstance()->waitForDone(1);
    }
    
    if (!threadsCleared) {
        qWarning() << "[MainWindow] 线程池清理超时，执行强制终止...";
    }
    
    // 5. 详细调试所有线程（找出"傻逼线程"）
    qDebug() << "[MainWindow] 🕵️ 开始详细调试线程状态...";
    
    // 首先列出所有线程信息
    QList<QThread*> allThreads = findChildren<QThread*>();
    qDebug() << "[MainWindow] 📊 总共发现" << allThreads.size() << "个Qt线程";
    
    // 检查主线程状态
    QThread* mainThread = QApplication::instance()->thread();
    qDebug() << "[MainWindow] 🧵 主线程状态:" 
             << "名称=" << mainThread->objectName()
             << "是否运行=" << mainThread->isRunning() 
             << "是否完成=" << mainThread->isFinished()
             << "线程ID=" << mainThread->currentThreadId();
    
    // 详细分析每个线程
    for (int i = 0; i < allThreads.size(); ++i) {
        QThread* thread = allThreads[i];
        if (thread) {
            QString threadInfo = QString("[线程%1] 名称='%2' 运行中=%3 已完成=%4 线程ID=%5")
                               .arg(i+1)
                               .arg(thread->objectName().isEmpty() ? "未命名" : thread->objectName())
                               .arg(thread->isRunning() ? "是" : "否")
                               .arg(thread->isFinished() ? "是" : "否")
                               .arg(reinterpret_cast<quintptr>(thread));
            
            if (thread->isRunning()) {
                qWarning() << "[MainWindow] 🚨 发现运行中的线程:" << threadInfo;
                
                // 尝试获取更多信息
                if (thread->objectName().isEmpty()) {
                    qWarning() << "[MainWindow] ⚠️  这个线程没有名称，可能是QtConcurrent线程或匿名线程";
                }
                
                // 强制终止逻辑
                qDebug() << "[MainWindow] 🔫 准备强制终止线程:" << thread->objectName();
                thread->quit();
                
                if (!thread->wait(500)) {  // 等待500ms
                    qWarning() << "[MainWindow] 💀 线程拒绝quit()，使用terminate():" << threadInfo;
                    thread->terminate();
                    
                    if (!thread->wait(1000)) {  // 再等1秒
                        qCritical() << "[MainWindow] ☠️  FUCK! 线程拒绝terminate()，这是顽固线程:" << threadInfo;
                    } else {
                        qDebug() << "[MainWindow] ✅ 线程已被terminate()杀死:" << thread->objectName();
                    }
                } else {
                    qDebug() << "[MainWindow] ✅ 线程优雅退出:" << thread->objectName();
                }
            } else {
                qDebug() << "[MainWindow] ✅ 线程已停止:" << threadInfo;
            }
        }
    }
    
    // 检查全局线程池状态
    QThreadPool* globalPool = QThreadPool::globalInstance();
    qDebug() << "[MainWindow] 🏊 全局线程池状态:"
             << "活动线程数=" << globalPool->activeThreadCount()
             << "最大线程数=" << globalPool->maxThreadCount()
             << "是否等待完成=" << (!globalPool->waitForDone(1));
    
    if (globalPool->activeThreadCount() > 0) {
        qWarning() << "[MainWindow] 🚨 全局线程池仍有" << globalPool->activeThreadCount() << "个活动线程!";
    }
    
    // 6. 强制清理CTK插件服务（这是重点！）
#ifdef CTK_PLUGIN_FRAMEWORK
    qDebug() << "[MainWindow] 🔌 强制清理CTK插件服务状态...";
    
    // 强制清理光学追踪服务（Atracsys SDK线程）
    if (m_trackingService) {
        qWarning() << "[MainWindow] 🎯 强制停止光学追踪服务和Atracsys SDK线程...";
        
        // 尝试通过反射调用停止方法
        QMetaObject::invokeMethod(m_trackingService, "stopTracking", Qt::DirectConnection);
        QMetaObject::invokeMethod(m_trackingService, "disconnectDevice", Qt::DirectConnection);
        QMetaObject::invokeMethod(m_trackingService, "cleanup", Qt::DirectConnection);
        
        m_trackingService = nullptr;
        qDebug() << "[MainWindow] ✅ 光学追踪服务已强制清理";
    }
    
    // 强制清理医学图像服务（异步任务）
    if (m_imageService) {
        qWarning() << "[MainWindow] 📷 强制停止医学图像服务的所有异步任务...";
        
        // 强制取消所有任务
        QMetaObject::invokeMethod(m_imageService, "cancelAllTasks", Qt::DirectConnection);
        QMetaObject::invokeMethod(m_imageService, "clearAllImages", Qt::DirectConnection);
        
        m_imageService = nullptr;
        qDebug() << "[MainWindow] ✅ 医学图像服务已强制清理";
    }
    
    // 强制清理医学查看器服务（VTK渲染线程）  
    if (m_medicalViewerService) {
        qWarning() << "[MainWindow] 👁️ 强制停止VTK渲染线程...";
        
        // 强制停止所有渲染器
        QMetaObject::invokeMethod(m_medicalViewerService, "stopAllRendering", Qt::DirectConnection);
        QMetaObject::invokeMethod(m_medicalViewerService, "cleanup", Qt::DirectConnection);
        
        m_medicalViewerService = nullptr;
        qDebug() << "[MainWindow] ✅ 医学查看器服务已强制清理";
    }
    
    // 清理其他服务
    if (m_patientService) {
        QMetaObject::invokeMethod(m_patientService, "closeDatabase", Qt::DirectConnection);
        m_patientService = nullptr;
        qDebug() << "[MainWindow] ✅ 患者管理服务已清理";
    }
    if (m_imageInteractionService) {
        m_imageInteractionService = nullptr;
        qDebug() << "[MainWindow] ✅ 图像交互服务已清理";
    }
    if (m_medicalProcessingService) {
        QMetaObject::invokeMethod(m_medicalProcessingService, "cancelAllProcessing", Qt::DirectConnection);
        m_medicalProcessingService = nullptr;
        qDebug() << "[MainWindow] ✅ 医学处理服务已清理";
    }
    
    // 立即强制CTK框架停止（不等待aboutToQuit）
    qWarning() << "[MainWindow] 🚨 立即强制停止CTK框架...";
    CTKManager* ctkManager = CTKManager::instance();
    if (ctkManager) {
        ctkManager->stopFramework();
        qDebug() << "[MainWindow] ✅ CTK框架已立即停止";
    }
#endif
    
    // 7. 检查应用程序的其他对象
    qDebug() << "[MainWindow] 🏠 检查应用程序根对象状态...";
    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (app) {
        QObjectList appChildren = app->children();
        qDebug() << "[MainWindow] 📝 应用程序有" << appChildren.size() << "个子对象";
        
        for (QObject* child : appChildren) {
            if (child) {
                QString className = child->metaObject()->className();
                QString objectName = child->objectName();
                qDebug() << "[MainWindow] 📦 子对象:" << className 
                         << "名称=" << (objectName.isEmpty() ? "未命名" : objectName);
                
                // 特别关注线程相关的对象
                if (className.contains("Thread") || 
                    className.contains("Future") || 
                    className.contains("Concurrent") ||
                    className.contains("Timer")) {
                    qWarning() << "[MainWindow] ⚠️  可疑对象（可能阻止退出）:" << className << objectName;
                }
            }
        }
    } else {
        qWarning() << "[MainWindow] ⚠️  无法获取QApplication实例";
    }
    
    // 8. 快速线程检测和立即强杀
#ifdef _WIN32
    qWarning() << "[MainWindow] 🔍 快速检测系统线程...";
    
    DWORD currentProcessId = GetCurrentProcessId();
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);
        
        int threadCount = 0;
        if (Thread32First(hSnapshot, &te32)) {
            do {
                if (te32.th32OwnerProcessID == currentProcessId) {
                    threadCount++;
                }
            } while (Thread32Next(hSnapshot, &te32));
        }
        
        CloseHandle(hSnapshot);
        qCritical() << "[MainWindow] 💣 检测到" << threadCount << "个线程，立即ExitProcess强杀！";
        
        // 不管多少线程，直接强杀进程！
        ::ExitProcess(0);
    } else {
        qWarning() << "[MainWindow] ⚠️  无法创建线程快照，直接ExitProcess";
        ::ExitProcess(0);
    }
#endif
    
    // 9. 立即发送应用程序退出信号
    qDebug() << "[MainWindow] 发送应用程序退出信号...";
    QCoreApplication::processEvents();
    
    // 10. 接受关闭事件
    event->accept();
    
    // 这些定时器永远不会执行，因为上面已经ExitProcess了
    // 但保留作为备用方案以防万一
    
    qDebug() << "[MainWindow] 关闭事件处理完成，等待应用程序退出...";
}

void MainWindow::initializeUI()
{
    // 旧的患者管理UI初始化已移除，现在使用CTK插件架构
    
    // 设置CTK插件上下文
    setupCTKPluginContext();
    
#ifdef CTK_PLUGIN_FRAMEWORK
    // 初始化CTK增强组件
    setupCTKEnhancedLogging();
    setupCTKTrackingParametersDock();
    setupCTKStatusMonitorDock();
    setupCTKErrorLogDock();
    applyCTKMedicalTheme();
#endif
    
    // Initialize tracking service
    initializeTrackingService();
    
    // Initialize geometry configuration
    initializeGeometryConfig();
    
    // Set status bar message
    statusBar()->showMessage(QString::fromUtf8("就绪"), 2000);
}

void MainWindow::setupConnections()
{
    // 旧的患者管理UI连接已移除，现在使用CTK插件架构
    // 所有患者管理相关的UI连接都已移除
    
    // 基本菜单连接（保留必要的，删除旧患者管理相关）
    connect(ui->exitAction, &QAction::triggered, this, &MainWindow::onExit);
    
#ifdef CTK_PLUGIN_FRAMEWORK
    // 患者管理插件连接 - 检查菜单项是否存在再连接
    if (ui->menubar) {
        // 动态查找或创建患者管理菜单项
        addPatientManagementMenuItem();
        // 动态查找或创建医学图像菜单项
        addMedicalImageMenuItem();
        // 动态查找或创建图像交互菜单项
        addImageInteractionMenuItem();
        // 动态查找或创建医学查看器菜单项
        addMedicalViewerMenuItem();
        // 动态查找或创建医学处理菜单项
        addMedicalProcessingMenuItem();
    }
#endif
    connect(ui->aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    
    // Optical tracking connections
    connect(ui->connectDeviceBtn, &QPushButton::clicked, this, &MainWindow::onConnectDevice);
    connect(ui->startTrackingBtn, &QPushButton::clicked, this, &MainWindow::onStartTracking);
    connect(ui->stopTrackingBtn, &QPushButton::clicked, this, &MainWindow::onStopTracking);
    connect(ui->getStatsBtn, &QPushButton::clicked, this, &MainWindow::onGetStats);
    connect(ui->diagnoseDeviceBtn, &QPushButton::clicked, this, &MainWindow::onDiagnoseDevice);
    connect(ui->geometryComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onGeometryConfigChanged);
    
    // === 增强功能按钮连接 ===
    connect(ui->openEnhancedTrackingBtn, &QPushButton::clicked, this, &MainWindow::onOpenEnhancedTrackingWidget);
    connect(ui->openPivotCalibrationBtn, &QPushButton::clicked, this, &MainWindow::onOpenPivotCalibrationModule);
    connect(ui->openUDPCommunicationBtn, &QPushButton::clicked, this, &MainWindow::onOpenUDPCommunicationModule);
    connect(ui->open3DVisualizationBtn, &QPushButton::clicked, this, &MainWindow::onOpenEnhancedTrackingWidget);  // 3D可视化使用相同界面
    
#ifdef CTK_PLUGIN_FRAMEWORK
    // UI update timer
    connect(m_uiUpdateTimer, &QTimer::timeout, this, &MainWindow::updatePerformanceStats);
    m_uiUpdateTimer->start(1000); // Update every second
#endif
}

// ========================================
// 光学追踪功能实现
// ========================================

void MainWindow::setupCTKPluginContext()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        // 获取CTK管理器和插件上下文
        CTKManager* ctkManager = CTKManager::instance();
        if (ctkManager && ctkManager->isCTKAvailable()) {
            m_ctkContext = ctkManager->getPluginContext();
            if (m_ctkContext) {
                // 遵循CTK架构：不再使用全局访问器，直接通过服务查找
                logTrackingMessage("CTK插件上下文已初始化");
                
                // 初始化患者管理服务
                initializePatientManagementService(m_ctkContext);
                
                // 初始化医学图像服务
                initializeMedicalImageService(m_ctkContext);
                
                // 初始化图像交互服务
                initializeImageInteractionService(m_ctkContext);
                
                // 初始化医学查看器服务
                initializeMedicalViewerService(m_ctkContext);
                
                // 初始化医学处理服务
                initializeMedicalProcessingService(m_ctkContext);
            } else {
                logTrackingMessage("警告: 无法获取CTK插件上下文");
            }
        } else {
            logTrackingMessage("警告: CTK框架不可用");
        }
    } catch (const std::exception& e) {
        logTrackingMessage(QString("设置CTK插件上下文时发生错误: %1").arg(e.what()));
    }
#else
    logTrackingMessage("CTK框架未编译，无法设置插件上下文");
#endif
}

void MainWindow::onOpenNrrdViewer()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        CTKManager* ctkManager = CTKManager::instance();
        if (!ctkManager || !ctkManager->isCTKAvailable()) {
            QMessageBox::warning(this, QString::fromUtf8("警告"),
                                QString::fromUtf8("CTK插件框架不可用！\n"
                                "请确保CTK已正确安装和配置。"));
            return;
        }

        // 获取CTK插件上下文
        auto context = ctkManager->getPluginContext();
        if (!context) {
            QMessageBox::warning(this, QString::fromUtf8("警告"),
                                QString::fromUtf8("无法获取插件上下文！"));
            return;
        }

        // 查找MedicalImageCoreService服务（CTK标准方式）
        ctkServiceReference serviceRef = context->getServiceReference("medical.MedicalImageCoreService");
        if (!serviceRef) {
            // 检查已加载的插件列表以便调试
            QStringList loadedPlugins = ctkManager->getLoadedPlugins();
            QMessageBox::information(this, QString::fromUtf8("提示"),
                                    QString::fromUtf8("MedicalImageService服务未找到。\n"
                                    "请确保NrrdViewer插件已正确加载和注册。\n\n"
                                    "已加载的插件：\n") + loadedPlugins.join("\n") +
                                    QString::fromUtf8("\n\n将直接打开文件选择对话框..."));
            
            // 如果服务不可用，至少可以让用户选择文件
            QString fileName = QFileDialog::getOpenFileName(
                this,
                QString::fromUtf8("选择医学图像文件"),
                "",
                QString::fromUtf8("医学图像文件 (*.nrrd *.nhdr *.nii *.nii.gz *.dcm *.dicom *.ima);;"
                              "NRRD文件 (*.nrrd *.nhdr);;"
                              "NIfTI文件 (*.nii *.nii.gz);;"
                              "DICOM文件 (*.dcm *.dicom *.ima);;"
                              "所有文件 (*.*)")
            );
            
            if (!fileName.isEmpty()) {
                QMessageBox::information(this, QString::fromUtf8("信息"),
                                        QString::fromUtf8("文件已选择：\n") + fileName +
                                        QString::fromUtf8("\n\n但MedicalImageService不可用，无法显示图像。"));
            }
            return;
        }

        // 获取服务实例（CTK标准方式）
        QObject* service = context->getService(serviceRef);
        if (!service) {
            QMessageBox::warning(this, QString::fromUtf8("错误"),
                                QString::fromUtf8("无法获取MedicalImageService服务实例！"));
            return;
        }

        // 显示服务信息（CTK方式）
        QStringList supportedFormats;
        QMetaObject::invokeMethod(service, "getSupportedFormats", 
                                  Qt::DirectConnection, 
                                  Q_RETURN_ARG(QStringList, supportedFormats));
        QString serviceInfo;
        QMetaObject::invokeMethod(service, "getServiceStatus", 
                                  Qt::DirectConnection, 
                                  Q_RETURN_ARG(QString, serviceInfo));
        
        qDebug() << "[MainWindow] MedicalImageService found!";
        qDebug() << "[MainWindow] Supported formats:" << supportedFormats;
        qDebug() << "[MainWindow] Service info:" << serviceInfo;

        // 选择图像文件
        QString fileName = QFileDialog::getOpenFileName(
            this,
            QString::fromUtf8("选择医学图像文件"),
            "",
            QString::fromUtf8("医学图像文件 (*.nrrd *.nhdr *.nii *.nii.gz *.dcm *.dicom *.ima);;"
                          "NRRD文件 (*.nrrd *.nhdr);;"
                          "NIfTI文件 (*.nii *.nii.gz);;"
                          "DICOM文件 (*.dcm *.dicom *.ima);;"
                          "所有文件 (*.*)")
        );
        
        if (fileName.isEmpty()) {
            return; // 用户取消选择
        }

        // 加载图像（CTK方式）
        QString imageId;
        QMetaObject::invokeMethod(service, "loadImage", 
                                  Qt::DirectConnection, 
                                  Q_RETURN_ARG(QString, imageId),
                                  Q_ARG(QString, fileName));
        if (imageId.isEmpty()) {
            QMessageBox::warning(this, QString::fromUtf8("错误"),
                                QString::fromUtf8("无法加载图像文件：\n") + fileName);
            return;
        }

        // 显示图像信息（CTK方式）
        QString imageInfo;
        QMetaObject::invokeMethod(service, "getImageFormat", 
                                  Qt::DirectConnection, 
                                  Q_RETURN_ARG(QString, imageInfo),
                                  Q_ARG(QString, imageId));
        
        QList<int> dimensions;
        QMetaObject::invokeMethod(service, "getImageDimensions", 
                                  Qt::DirectConnection, 
                                  Q_RETURN_ARG(QList<int>, dimensions),
                                  Q_ARG(QString, imageId));
        
        QList<double> spacing;
        QMetaObject::invokeMethod(service, "getImageSpacing", 
                                  Qt::DirectConnection, 
                                  Q_RETURN_ARG(QList<double>, spacing),
                                  Q_ARG(QString, imageId));
        
        // 格式化维度和间距信息
        QString dimensionsStr = QString("%1 x %2").arg(dimensions.value(0, 0)).arg(dimensions.value(1, 0));
        if (dimensions.size() > 2 && dimensions[2] > 1) {
            dimensionsStr += QString(" x %1").arg(dimensions[2]);
        }
        
        QString spacingStr = QString("%1 x %2").arg(spacing.value(0, 0.0)).arg(spacing.value(1, 0.0));
        if (spacing.size() > 2) {
            spacingStr += QString(" x %1").arg(spacing[2]);
        }
        
        QString infoText = QString::fromUtf8("图像已成功加载！\n\n") +
                          QString::fromUtf8("文件：%1\n\n").arg(QFileInfo(fileName).fileName()) +
                          QString::fromUtf8("图像信息：\n%1\n\n").arg(imageInfo) +
                          QString::fromUtf8("维度：%1\n").arg(dimensionsStr) +
                          QString::fromUtf8("间距：%1\n\n").arg(spacingStr) +
                          QString::fromUtf8("是否显示3D图像？");

        int ret = QMessageBox::question(this, QString::fromUtf8("图像已加载"), infoText,
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::Yes);

        if (ret == QMessageBox::Yes) {
            // 图像已成功加载，可以通过医疗查看器插件显示
            QMessageBox::information(this, QString::fromUtf8("提示"),
                                    QString::fromUtf8("图像已加载，图像ID：%1\n可以通过医疗查看器插件显示此图像。").arg(imageId));
        }
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, QString::fromUtf8("错误"),
                            QString::fromUtf8("打开NrrdViewer时发生错误：\n") + e.what());
    } catch (...) {
        QMessageBox::critical(this, QString::fromUtf8("错误"),
                            QString::fromUtf8("打开NrrdViewer时发生未知错误！"));
    }
#else
    QMessageBox::information(this, QString::fromUtf8("功能不可用"),
                           QString::fromUtf8("3D医学图像查看器功能需要CTK插件框架支持。\n"
                           "当前版本未编译CTK支持。"));
#endif
}

// 所有旧的患者管理私有方法实现已彻底移除，现在使用CTK插件架构

// ========================================
// 旧患者管理方法的空实现 - 避免编译错误
// ========================================

// 所有旧的患者管理方法已彻底移除

// 保留必要的基本方法
void MainWindow::onExit() { close(); }
void MainWindow::onAbout() { 
    QMessageBox::about(this, "关于", "医疗导航系统 v2.0\n现在使用CTK插件架构"); 
}

// ========================================
// 光学追踪功能实现
// ========================================

// setupCTKPluginContext已在第154行定义，删除重复实现

void MainWindow::initializeTrackingService()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    try {
        CTKManager* ctkManager = CTKManager::instance();
        if (!ctkManager || !ctkManager->isCTKAvailable()) {
            logTrackingMessage("CTK框架不可用，无法初始化光学追踪服务");
            return;
        }

        auto context = ctkManager->getPluginContext();
        if (!context) {
            logTrackingMessage("无法获取插件上下文");
            return;
        }

        // 查找OpticalTrackingService服务
        m_trackingServiceRef = context->getServiceReference<OpticalTrackingService>();
        if (!m_trackingServiceRef) {
            logTrackingMessage("OpticalTrackingService服务未找到，请确保OpticalTracking插件已加载");
            return;
        }

        // 获取服务实例
        m_trackingService = context->getService<OpticalTrackingService>(m_trackingServiceRef);
        if (!m_trackingService) {
            logTrackingMessage("无法获取OpticalTrackingService服务实例");
            return;
        }

        // 尝试转换为具体实现类以连接信号
        if (auto* serviceImpl = qobject_cast<QObject*>(m_trackingService)) {
            setupTrackingConnections(serviceImpl);
        }
        
        logTrackingMessage("光学追踪服务初始化成功");
        
        // 初始化增强跟踪界面
        initializeEnhancedTrackingInterface();
        
    } catch (const std::exception& e) {
        logTrackingMessage(QString("初始化光学追踪服务时发生错误: %1").arg(e.what()));
    } catch (...) {
        logTrackingMessage("初始化光学追踪服务时发生未知错误");
    }
#else
    logTrackingMessage("CTK框架未编译，光学追踪功能不可用");
#endif
}

void MainWindow::setupTrackingConnections(QObject* serviceImpl)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!serviceImpl) return;
    
    // 连接具体实现类的信号
    connect(serviceImpl, SIGNAL(deviceConnected()), 
            this, SLOT(onDeviceConnected()));
    connect(serviceImpl, SIGNAL(deviceDisconnected()), 
            this, SLOT(onDeviceDisconnected()));
    connect(serviceImpl, SIGNAL(trackingStateChanged(bool)), 
            this, SLOT(onTrackingStateChanged(bool)));
    connect(serviceImpl, SIGNAL(markersDetected(int, const QList<QVector3D>&)), 
            this, SLOT(onMarkersDetected(int, const QList<QVector3D>&)));
    
    logTrackingMessage("光学追踪服务信号连接完成");
#endif
}

// 光学追踪控制槽函数
void MainWindow::onConnectDevice()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_trackingService) {
        QMessageBox::warning(this, "警告", "光学追踪服务不可用！");
        return;
    }
    
    ui->connectDeviceBtn->setEnabled(false);
    ui->connectDeviceBtn->setText("连接中...");
    
    logTrackingMessage("正在连接光学追踪设备...");
    
    // 异步连接设备
    QTimer::singleShot(100, [this]() {
        QStringList availableDevices = m_trackingService->scanAvailableDevices();
        bool success = false;
        
        if (!availableDevices.isEmpty()) {
            // 尝试连接第一个可用设备
            QString deviceId = availableDevices.first();
            success = m_trackingService->connectToDevice(deviceId);
            if (success) {
                logTrackingMessage(QString("设备连接成功！设备ID: %1").arg(deviceId));
            }
        } else {
            logTrackingMessage("未发现可用的光学追踪设备");
        }
        
        ui->connectDeviceBtn->setEnabled(true);
        
        if (!success) {
            ui->connectDeviceBtn->setText("连接设备");
            QMessageBox::warning(this, "连接失败", "无法连接到光学追踪设备！\n请检查设备连接和网络配置。");
        }
    });
#endif
}

void MainWindow::onStartTracking()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_trackingService) return;
    
    // 创建一个默认的跟踪会话
    QString sessionId = m_trackingService->createTrackingSession("default_device", "主跟踪会话");
    if (!sessionId.isEmpty() && m_trackingService->startTracking(sessionId)) {
        logTrackingMessage("开始实时反光球追踪！");
        ui->startTrackingBtn->setEnabled(false);
        ui->stopTrackingBtn->setEnabled(true);
        ui->getStatsBtn->setEnabled(true);
        // 保存会话ID供后续使用
        m_currentSessionId = sessionId;
    } else {
        QMessageBox::warning(this, "启动失败", "无法启动光学追踪！\n请确保设备已连接。");
    }
#endif
}

void MainWindow::onStopTracking()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_trackingService) return;
    
    if (!m_currentSessionId.isEmpty() && m_trackingService->stopTracking(m_currentSessionId)) {
        logTrackingMessage("停止光学追踪");
        ui->startTrackingBtn->setEnabled(true);
        ui->stopTrackingBtn->setEnabled(false);
        m_currentSessionId.clear();
    } else {
        logTrackingMessage("停止追踪失败");
    }
#endif
}

void MainWindow::onGetStats()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_trackingService) return;
    
    // 使用系统状态报告替代getTrackingStats
    QVariantMap statusReport;
    if (!m_currentSessionId.isEmpty()) {
        statusReport = m_trackingService->getSystemStatusReport(m_currentSessionId);
    }
    
    QString stats = QString("追踪统计信息:\n会话ID: %1\n连接设备数: %2")
                    .arg(m_currentSessionId)
                    .arg(m_trackingService->getConnectedDevices().size());
    
    QMessageBox::information(this, "追踪统计信息", stats);
    logTrackingMessage("显示追踪统计信息");
#endif
}

void MainWindow::onDiagnoseDevice()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_trackingService) {
        QMessageBox::warning(this, "错误", "追踪服务未初始化");
        return;
    }
    
    // 检查是否有连接的设备
    QStringList connectedDevices = m_trackingService->getConnectedDevices();
    if (connectedDevices.isEmpty()) {
        QMessageBox::warning(this, "错误", "设备未连接，请先连接设备");
        return;
    }
    
    // 调用设备诊断功能
    logTrackingMessage("开始设备诊断...");
    
    // 通过现有接口获取设备信息进行诊断
    QString deviceId = connectedDevices.first();
    QVariantMap deviceInfo = m_trackingService->getDeviceInfo(deviceId);
    QVariantMap deviceParams = m_trackingService->getDeviceParameters(deviceId);
    
    QString diagnosticInfo = QString("设备诊断结果:\n设备ID: %1\n设备类型: %2\n连接状态: 正常")
                             .arg(deviceId)
                             .arg(deviceInfo.value("deviceType", "未知").toString());
    
    QMessageBox::information(this, "设备诊断", 
                            diagnosticInfo + "\n\n" +
                            "请查看应用程序输出窗口中的详细诊断信息。\n"
                            "如果检测到图像采集超时问题，请按照建议进行故障排除：\n\n"
                            "1. 检查设备电源连接\n"
                            "2. 检查网络线连接\n" 
                            "3. 重启设备\n"
                            "4. 验证demo.exe是否能正常工作");
    
    logTrackingMessage("设备诊断完成");
#endif
}

// 光学追踪服务信号响应
void MainWindow::onDeviceConnected()
{
    m_deviceConnected = true;
    
    // 获取设备信息
    QString deviceInfo = "";
#ifdef CTK_PLUGIN_FRAMEWORK
    if (m_trackingService) {
        QStringList connectedDevices = m_trackingService->getConnectedDevices();
        if (!connectedDevices.isEmpty()) {
            QVariantMap info = m_trackingService->getDeviceInfo(connectedDevices.first());
            deviceInfo = info.value("deviceName", "未知设备").toString();
        }
    }
#endif
    
    updateDeviceStatus("fusionTrack 500", "", true);
    
    ui->connectDeviceBtn->setText("已连接");
    ui->connectDeviceBtn->setEnabled(false);
    ui->startTrackingBtn->setEnabled(true);
    ui->diagnoseDeviceBtn->setEnabled(true);
    ui->geometryComboBox->setEnabled(true);
    
    logTrackingMessage("设备已连接: " + deviceInfo);
    logTrackingMessage("几何配置已启用，请选择探针类型");
}

void MainWindow::onDeviceDisconnected()
{
    m_deviceConnected = false;
    updateDeviceStatus("未连接", "", false);
    
    ui->connectDeviceBtn->setText("连接设备");
    ui->connectDeviceBtn->setEnabled(true);
    ui->startTrackingBtn->setEnabled(false);
    ui->diagnoseDeviceBtn->setEnabled(false);
    ui->stopTrackingBtn->setEnabled(false);
    ui->getStatsBtn->setEnabled(false);
    ui->geometryComboBox->setEnabled(false);
    
    logTrackingMessage("设备已断开连接");
}

// onDeviceError方法已删除，因为TrackingService没有对应的信号

void MainWindow::onTrackingStateChanged(bool isTracking)
{
    m_trackingActive = isTracking;
    updateTrackingStatus(isTracking);
    
    QString state = isTracking ? "正在追踪" : "已停止";
    logTrackingMessage("追踪状态变更: " + state);
}

void MainWindow::onMarkersDetected(int markerCount, const QList<QVector3D>& positions)
{
    m_lastMarkerCount = markerCount;
    m_markerDetectionCount++;
    
    // 更新计数显示
    ui->markerCountValue->setText(QString::number(markerCount));
    
    // 每100次检测记录一次日志，减少界面刷新负担
    if (m_markerDetectionCount % 100 == 0) {
        logTrackingMessage(QString("检测到 %1 个反光球 (第 %2 次检测)")
                          .arg(markerCount)
                          .arg(m_markerDetectionCount));
    }
}

// UI更新方法
void MainWindow::updateDeviceStatus(const QString& deviceType, const QString& serialNumber, bool connected)
{
    m_deviceConnected = connected;
    
    ui->deviceTypeValue->setText(deviceType);
    
    if (connected) {
        ui->connectionStatusValue->setText("已连接");
        ui->connectionStatusValue->setStyleSheet("font-weight: bold; color: green;");
    } else {
        ui->connectionStatusValue->setText("未连接");
        ui->connectionStatusValue->setStyleSheet("font-weight: bold; color: red;");
    }
    
#ifdef CTK_PLUGIN_FRAMEWORK
    // 更新CTK状态指示器
    updateCTKStatusIndicators();
    
    // CTK增强日志记录
    CTK_TRACKING_LOG("设备状态变更", (QVariantMap{
        {"device_type", deviceType},
        {"serial_number", serialNumber},
        {"connected", connected}
    }));
#endif
}

void MainWindow::updateTrackingStatus(bool isTracking)
{
    m_trackingActive = isTracking;
    
    if (!isTracking) {
        ui->markerCountValue->setText("0");
    }
    
#ifdef CTK_PLUGIN_FRAMEWORK
    // 更新CTK状态指示器
    updateCTKStatusIndicators();
    
    // CTK增强日志记录
    CTK_TRACKING_LOG("追踪状态变更", (QVariantMap{{"tracking_active", isTracking}}));
#endif
}

void MainWindow::updateMarkerTable(const QList<QVector3D>& positions)
{
    // 调用高级表格更新方法
    updateMarkersTableAdvanced(positions.size(), positions);
}

void MainWindow::updateMarkersTableAdvanced(int markerCount, const QList<QVector3D>& positions, 
                                           const QList<double>& errors, 
                                           const QList<double>& qualities)
{
    // 更新标记数量显示
    ui->markerCountValue->setText(QString::number(markerCount));
    
    // 更新表格内容
    if (ui->markersTableWidget) {
        ui->markersTableWidget->setRowCount(markerCount);
        
        for (int i = 0; i < markerCount && i < positions.size(); ++i) {
            const QVector3D& pos = positions[i];
            
            // ID列
            auto idItem = new QTableWidgetItem(QString::number(i));
            idItem->setTextAlignment(Qt::AlignCenter);
            ui->markersTableWidget->setItem(i, 0, idItem);
            
            // X坐标
            auto xItem = new QTableWidgetItem(QString::number(pos.x(), 'f', 2));
            xItem->setTextAlignment(Qt::AlignCenter);
            ui->markersTableWidget->setItem(i, 1, xItem);
            
            // Y坐标
            auto yItem = new QTableWidgetItem(QString::number(pos.y(), 'f', 2));
            yItem->setTextAlignment(Qt::AlignCenter);
            ui->markersTableWidget->setItem(i, 2, yItem);
            
            // Z坐标
            auto zItem = new QTableWidgetItem(QString::number(pos.z(), 'f', 2));
            zItem->setTextAlignment(Qt::AlignCenter);
            ui->markersTableWidget->setItem(i, 3, zItem);
            
            // 误差列
            QString errorText = "N/A";
            if (i < errors.size()) {
                errorText = QString::number(errors[i], 'f', 3);
            }
            auto errorItem = new QTableWidgetItem(errorText);
            errorItem->setTextAlignment(Qt::AlignCenter);
            ui->markersTableWidget->setItem(i, 4, errorItem);
            
            // 质量列
            QString qualityText = "N/A";
            if (i < qualities.size()) {
                qualityText = QString::number(qualities[i], 'f', 2);
            }
            auto qualityItem = new QTableWidgetItem(qualityText);
            qualityItem->setTextAlignment(Qt::AlignCenter);
            ui->markersTableWidget->setItem(i, 5, qualityItem);
            
            // 状态列
            auto statusItem = new QTableWidgetItem("活跃");
            statusItem->setTextAlignment(Qt::AlignCenter);
            statusItem->setForeground(QBrush(QColor(76, 175, 80))); // 绿色
            ui->markersTableWidget->setItem(i, 6, statusItem);
            
            // 根据质量设置行颜色
            QColor rowColor = Qt::white;
            if (i < qualities.size()) {
                if (qualities[i] > 0.8) {
                    rowColor = QColor(232, 245, 233); // 浅绿色
                } else if (qualities[i] > 0.5) {
                    rowColor = QColor(255, 248, 225); // 浅黄色
                } else {
                    rowColor = QColor(255, 235, 238); // 浅红色
                }
            }
            
            for (int col = 0; col < 7; ++col) {
                if (ui->markersTableWidget->item(i, col)) {
                    ui->markersTableWidget->item(i, col)->setBackground(rowColor);
                }
            }
        }
    }
    
#ifdef CTK_PLUGIN_FRAMEWORK
    // 更新CTK标记物数量显示
    if (m_markerCountStatusLabel) {
        m_markerCountStatusLabel->setText(QString("%1 个").arg(markerCount));
        
        // 根据标记物数量设置颜色
        QString color = "#2d3748";  // 默认颜色
        if (markerCount >= 3) {
            color = "#48bb78";  // 绿色 - 良好
        } else if (markerCount >= 1) {
            color = "#ed8936";  // 橙色 - 一般
        } else {
            color = "#f56565";  // 红色 - 无标记物
        }
        
        m_markerCountStatusLabel->setStyleSheet(
            QString("QLabel { font-family: 'Consolas', monospace; color: %1; font-weight: bold; }")
            .arg(color));
    }
    
    // 记录标记物检测日志
    CTK_TRACKING_LOG("标记物检测", (QVariantMap{{"count", markerCount}}));
#endif
}

void MainWindow::updateDetectionRate(double rate)
{
    if (ui->detectionRateValue) {
        ui->detectionRateValue->setText(QString::number(rate, 'f', 1) + "%");
        
        // 根据检测率设置颜色
        QString colorStyle;
        if (rate > 80.0) {
            colorStyle = "font-weight: bold; color: #4CAF50;"; // 绿色
        } else if (rate > 50.0) {
            colorStyle = "font-weight: bold; color: #FF9800;"; // 橙色
        } else {
            colorStyle = "font-weight: bold; color: #F44336;"; // 红色
        }
        ui->detectionRateValue->setStyleSheet(colorStyle);
    }
}

void MainWindow::updatePerformanceStats()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_trackingService || !m_trackingActive) return;
    
    // 简化版UI没有详细统计显示，只更新计数器
    m_totalFrameCount += 20; // 20 FPS
    if (m_lastMarkerCount > 0) {
        m_validFrameCount += 20;
    }
#endif
}

void MainWindow::logTrackingMessage(const QString& message)
{
    qDebug() << "[MainWindow-Tracking]" << message;
    statusBar()->showMessage(message, 3000);
}

void MainWindow::initializeGeometryConfig()
{
    // 扫描几何配置文件
    QString geometryPath = QCoreApplication::applicationDirPath() + 
                          "/../../../ThirdParty/Atracsys/fusionTrack SDK x64/data";
    
    QDir geometryDir(geometryPath);
    if (!geometryDir.exists()) {
        logTrackingMessage("几何配置文件夹不存在，无法加载探针配置");
        return;
    }
    
    // 获取所有几何配置文件
    QStringList filters;
    filters << "*.ini";
    QStringList geometryFiles = geometryDir.entryList(filters, QDir::Files, QDir::Name);
    
    ui->geometryComboBox->clear();
    ui->geometryComboBox->addItem("自动检测 (Markerless)", "");
    ui->geometryComboBox->addItem("强制无几何模式 (推荐)", "FORCE_MARKERLESS");
    
    // 添加特殊的探针配置
    for (const QString& file : geometryFiles) {
        QString displayName = getGeometryDisplayName(file);
        QString fullPath = geometryDir.absoluteFilePath(file);
        ui->geometryComboBox->addItem(displayName, fullPath);
    }
    
    // 默认选择第一个配置
    ui->geometryComboBox->setCurrentIndex(0);
    
    logTrackingMessage(QString("已加载 %1 个几何配置文件").arg(geometryFiles.size()));
}

QString MainWindow::getGeometryDisplayName(const QString& filename)
{
    if (filename == "stylus.ini") return "NDI 被动探针 (4点)";
    if (filename == "geometry20.ini") return "通用探针 #20 (4点)";
    if (filename == "geometry60.ini") return "通用探针 #60 (4点)";
    if (filename == "geometry011.ini") return "Cascination 钛标记 (3点)";
    if (filename == "geometry101.ini") return "探针 #101";
    if (filename.startsWith("geometry")) {
        QString number = filename.mid(8, filename.length() - 12);
        return QString("探针 #%1").arg(number);
    }
    return filename;
}

void MainWindow::onGeometryConfigChanged()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_trackingService) return;
    
    int currentIndex = ui->geometryComboBox->currentIndex();
    QString selectedPath = ui->geometryComboBox->itemData(currentIndex).toString();
    
    // 通过设备参数来配置几何路径
    QStringList connectedDevices = m_trackingService->getConnectedDevices();
    if (!connectedDevices.isEmpty()) {
        QVariantMap params;
        params["geometryConfigPath"] = selectedPath;
        m_trackingService->setDeviceParameters(connectedDevices.first(), params);
    }
    
    if (selectedPath.isEmpty()) {
        ui->geometryInfoValue->setText("自动检测模式");
        logTrackingMessage("已切换到自动检测模式 (Markerless)");
    } else if (selectedPath == "FORCE_MARKERLESS") {
        ui->geometryInfoValue->setText("强制无几何模式");
        logTrackingMessage("已切换到强制无几何模式 (推荐用于故障排除)");
    } else {
        QFileInfo fileInfo(selectedPath);
        ui->geometryInfoValue->setText(fileInfo.baseName());
        logTrackingMessage(QString("已选择几何配置: %1").arg(ui->geometryComboBox->currentText()));
    }
    
    // 如果当前正在追踪，提示用户重新开始
    if (m_trackingActive) {
        QMessageBox::information(this, "提示", 
                                "几何配置已更改，请停止追踪后重新开始以应用新配置。");
    }
#endif
}

void MainWindow::initializeEnhancedTrackingInterface()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    // 遵循CTK架构：插件UI应该由插件自己管理
    // 主应用程序只需要提供基本的服务接口访问
    if (m_trackingService) {
        logTrackingMessage("光学追踪服务已可用，增强功能可通过按钮访问");
    } else {
        logTrackingMessage("光学追踪服务不可用");
    }
#else
    logTrackingMessage("CTK框架未编译，增强光学追踪界面不可用");
#endif
}

// === 增强功能槽函数实现 ===

void MainWindow::onOpenEnhancedTrackingWidget()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (m_trackingService) {
        QMessageBox::information(this, "增强跟踪界面", 
                               "增强跟踪功能已通过光学追踪服务提供。\n"
                               "请使用光学追踪标签页中的功能按钮来访问：\n"
                               "- 连接设备\n"
                               "- 开始追踪\n"
                               "- 几何配置\n"
                               "- 设备诊断");
        logTrackingMessage("增强跟踪功能通过服务接口可用");
    } else {
        QMessageBox::warning(this, "警告", 
                           "光学追踪服务不可用。\n请确保OpticalTracking插件已正确加载。");
        logTrackingMessage("光学追踪服务不可用");
    }
#else
    QMessageBox::information(this, "信息", 
                           "增强跟踪功能需要CTK插件框架支持。\n当前版本未编译CTK框架。");
#endif
}

void MainWindow::onOpenPivotCalibrationModule()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (m_trackingService) {
        QString info = "针尖校准功能说明：\n\n"
                      "针尖校准算法已集成在光学追踪服务中。\n"
                      "使用方法：\n"
                      "1. 确保设备已连接并开始追踪\n"
                      "2. 将探针针尖固定在某一点\n"
                      "3. 转动探针收集多个位姿数据\n"
                      "4. 通过追踪服务调用校准算法\n\n"
                      "详细操作请参考用户手册。";
        
        QMessageBox::information(this, "针尖校准", info);
        logTrackingMessage("针尖校准功能说明已显示");
    } else {
        QMessageBox::warning(this, "警告", 
                           "光学追踪服务不可用。\n请确保OpticalTracking插件已正确加载。");
        logTrackingMessage("光学追踪服务不可用");
    }
#else
    QMessageBox::information(this, "信息", 
                           "针尖校准功能需要CTK插件框架支持。");
#endif
}

void MainWindow::onOpenUDPCommunicationModule()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (m_trackingService) {
        // 获取当前状态信息
        QString serviceStatus = m_trackingService ? "激活" : "未激活";
        QString trackingStatus = (!m_currentSessionId.isEmpty()) ? "追踪中" : "已停止";
        QStringList connectedDevices = m_trackingService->getConnectedDevices();
        QString deviceStatus = (!connectedDevices.isEmpty()) ? "已连接" : "未连接";
        
        QString info = QString("UDP通信功能说明：\n\n"
                      "UDP通信功能已集成在光学追踪服务中。\n"
                      "当前状态：\n"
                      "- 服务状态: %1\n"
                      "- 追踪状态: %2\n"
                      "- 设备状态: %3\n\n"
                      "UDP数据接收功能在开始追踪时自动启用。")
                      .arg(serviceStatus)
                      .arg(trackingStatus)
                      .arg(deviceStatus);
        
        QMessageBox::information(this, "UDP通信模块", info);
        logTrackingMessage("UDP通信模块状态已显示");
        } else {
        QMessageBox::warning(this, "警告", 
                           "光学追踪服务不可用。\n请确保OpticalTracking插件已正确加载。");
        logTrackingMessage("光学追踪服务不可用");
    }
#else
    QMessageBox::information(this, "信息", 
                           "UDP通信功能需要CTK插件框架支持。");
#endif
}

void MainWindow::onOpticalTrackingServiceAvailable(bool available)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    QString message = available ? 
        "OpticalTracking服务已可用" : 
        "OpticalTracking服务不可用";
    
    logTrackingMessage(message);
    
    // 遵循CTK架构：服务状态变化时只需要记录，UI由服务本身管理
    if (available) {
        logTrackingMessage("可以通过光学追踪标签页访问增强功能");
    }
#else
    // 非CTK模式下的处理
    logTrackingMessage("CTK框架未编译，无法使用服务可用性监听");
#endif
}

// ============================================================================
// CTK增强组件实现
// ============================================================================

#ifdef CTK_PLUGIN_FRAMEWORK

void MainWindow::setupCTKEnhancedLogging()
{
    // 初始化增强日志系统
    m_enhancedLogger = CTKEnhancedLogger::instance();
    
    // 配置日志级别和文件
    m_enhancedLogger->setLogLevel(CTKEnhancedLogger::INFO);
    
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QString logFile = logDir + "/medicalpro_enhanced.log";
    m_enhancedLogger->setLogFile(logFile);
    
    // 配置日志轮转
    m_enhancedLogger->setMaxFileSize(50 * 1024 * 1024);  // 50MB
    m_enhancedLogger->setMaxFiles(5);
    m_enhancedLogger->enableFileRotation(true);
    m_enhancedLogger->enableConsoleOutput(true);
    
    // 记录系统启动
    CTK_AUDIT("SYSTEM_START", "", qgetenv("USERNAME"));
    CTK_INFO("增强日志系统已初始化");
    
    logTrackingMessage("CTK增强日志系统已初始化");
}

void MainWindow::setupCTKTrackingParametersDock()
{
    // 创建追踪参数停靠窗口
    m_trackingParamsDock = new QDockWidget("光学追踪参数", this);
    
    // 创建主容器
    QWidget* paramsWidget = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(paramsWidget);
    
    // 创建可折叠的参数组
    m_trackingParamsGroup = new ctkCollapsibleGroupBox("追踪参数");
    m_trackingParamsGroup->setCollapsed(false);
    
    QGridLayout* paramsLayout = new QGridLayout();
    
    // 精度阈值设置
    paramsLayout->addWidget(new QLabel("检测精度阈值:"), 0, 0);
    m_precisionThresholdSpinBox = new ctkDoubleSpinBox();
    m_precisionThresholdSpinBox->setDecimals(3);
    m_precisionThresholdSpinBox->setSuffix(" mm");
    m_precisionThresholdSpinBox->setRange(0.001, 10.0);
    m_precisionThresholdSpinBox->setValue(0.1);
    paramsLayout->addWidget(m_precisionThresholdSpinBox, 0, 1);
    
    // 目标帧率设置
    paramsLayout->addWidget(new QLabel("目标帧率:"), 1, 0);
    m_frameRateTargetSlider = new ctkSliderWidget();
    m_frameRateTargetSlider->setRange(10.0, 60.0);
    m_frameRateTargetSlider->setValue(30.0);
    m_frameRateTargetSlider->setSuffix(" fps");
    paramsLayout->addWidget(m_frameRateTargetSlider, 1, 1);
    
    // 标记物颜色
    paramsLayout->addWidget(new QLabel("标记物颜色:"), 2, 0);
    m_markerColorPicker = new ctkColorPickerButton();
    m_markerColorPicker->setColor(Qt::red);
    m_markerColorPicker->setDisplayColorName(true);
    paramsLayout->addWidget(m_markerColorPicker, 2, 1);
    
    // 针尖颜色
    paramsLayout->addWidget(new QLabel("针尖颜色:"), 3, 0);
    m_tipColorPicker = new ctkColorPickerButton();
    m_tipColorPicker->setColor(Qt::green);
    m_tipColorPicker->setDisplayColorName(true);
    paramsLayout->addWidget(m_tipColorPicker, 3, 1);
    
    m_trackingParamsGroup->setLayout(paramsLayout);
    mainLayout->addWidget(m_trackingParamsGroup);
    mainLayout->addStretch();
    
    // 连接信号
    connect(m_precisionThresholdSpinBox, QOverload<double>::of(&ctkDoubleSpinBox::valueChanged),
            this, &MainWindow::onCTKPrecisionThresholdChanged);
    connect(m_frameRateTargetSlider, QOverload<double>::of(&ctkSliderWidget::valueChanged),
            this, &MainWindow::onCTKFrameRateTargetChanged);
    connect(m_markerColorPicker, &ctkColorPickerButton::colorChanged,
            this, &MainWindow::onCTKMarkerColorChanged);
    connect(m_tipColorPicker, &ctkColorPickerButton::colorChanged,
            this, &MainWindow::onCTKTipColorChanged);
    
    // 设置停靠窗口
    m_trackingParamsDock->setWidget(paramsWidget);
    m_trackingParamsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_trackingParamsDock->setFeatures(QDockWidget::DockWidgetMovable | 
                                     QDockWidget::DockWidgetFloatable |
                                     QDockWidget::DockWidgetClosable);
    
    addDockWidget(Qt::RightDockWidgetArea, m_trackingParamsDock);
    
    // 创建或获取视图菜单
    QMenu* viewMenu = nullptr;
    // 查找现有的视图菜单
    for (QAction* action : ui->menubar->actions()) {
        if (action->menu() && action->text().contains("视图")) {
            viewMenu = action->menu();
            break;
        }
    }
    // 如果没有视图菜单则创建
    if (!viewMenu) {
        viewMenu = ui->menubar->addMenu("视图(&V)");
    }
    // 添加到视图菜单
    viewMenu->addAction(m_trackingParamsDock->toggleViewAction());
    
    CTK_INFO("追踪参数停靠窗口已创建");
}

void MainWindow::setupCTKStatusMonitorDock()
{
    // 创建状态监控停靠窗口
    m_statusMonitorDock = new QDockWidget("实时状态监控", this);
    
    // 创建主容器
    QWidget* statusWidget = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(statusWidget);
    
    // 创建状态组
    m_statusGroup = new ctkCollapsibleGroupBox("系统状态");
    m_statusGroup->setCollapsed(false);
    
    QGridLayout* statusLayout = new QGridLayout();
    
    // 设备连接状态
    statusLayout->addWidget(new QLabel("设备状态:"), 0, 0);
    m_deviceStatusIndicator = new ctkColorPickerButton();
    m_deviceStatusIndicator->setColor(Qt::red);  // 默认未连接
    m_deviceStatusIndicator->setEnabled(false);
    statusLayout->addWidget(m_deviceStatusIndicator, 0, 1);
    m_deviceStatusLabel = new QLabel("未连接");
    statusLayout->addWidget(m_deviceStatusLabel, 0, 2);
    
    // 追踪状态
    statusLayout->addWidget(new QLabel("追踪状态:"), 1, 0);
    m_trackingStatusIndicator = new ctkColorPickerButton();
    m_trackingStatusIndicator->setColor(Qt::gray);  // 默认未追踪
    m_trackingStatusIndicator->setEnabled(false);
    statusLayout->addWidget(m_trackingStatusIndicator, 1, 1);
    m_trackingStatusLabel = new QLabel("未追踪");
    statusLayout->addWidget(m_trackingStatusLabel, 1, 2);
    
    // 帧率显示
    statusLayout->addWidget(new QLabel("当前帧率:"), 2, 0);
    m_frameRateStatusLabel = new QLabel("0.0 fps");
    m_frameRateStatusLabel->setStyleSheet("QLabel { font-family: 'Consolas', monospace; }");
    statusLayout->addWidget(m_frameRateStatusLabel, 2, 1, 1, 2);
    
    // 标记物数量
    statusLayout->addWidget(new QLabel("检测标记物:"), 3, 0);
    m_markerCountStatusLabel = new QLabel("0 个");
    m_markerCountStatusLabel->setStyleSheet("QLabel { font-family: 'Consolas', monospace; }");
    statusLayout->addWidget(m_markerCountStatusLabel, 3, 1, 1, 2);
    
    m_statusGroup->setLayout(statusLayout);
    mainLayout->addWidget(m_statusGroup);
    mainLayout->addStretch();
    
    // 设置停靠窗口
    m_statusMonitorDock->setWidget(statusWidget);
    m_statusMonitorDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_statusMonitorDock->setFeatures(QDockWidget::DockWidgetMovable | 
                                    QDockWidget::DockWidgetFloatable |
                                    QDockWidget::DockWidgetClosable);
    
    addDockWidget(Qt::BottomDockWidgetArea, m_statusMonitorDock);
    
    // 获取视图菜单（应该已经在追踪参数设置时创建了）
    QMenu* viewMenu = nullptr;
    for (QAction* action : ui->menubar->actions()) {
        if (action->menu() && action->text().contains("视图")) {
            viewMenu = action->menu();
            break;
        }
    }
    if (!viewMenu) {
        viewMenu = ui->menubar->addMenu("视图(&V)");
    }
    // 添加到视图菜单
    viewMenu->addAction(m_statusMonitorDock->toggleViewAction());
    
    CTK_INFO("状态监控停靠窗口已创建");
}

void MainWindow::setupCTKErrorLogDock()
{
    // 创建错误日志停靠窗口
    m_errorLogDock = new QDockWidget("错误日志", this);
    
    // 创建错误日志组件
    m_errorLogWidget = new ctkErrorLogWidget();
    m_errorLogWidget->setVisible(true);
    
    // 配置错误日志显示
    m_errorLogWidget->resize(800, 200);
    
    // 连接增强日志系统到错误日志组件
    connect(m_enhancedLogger, &CTKEnhancedLogger::logMessageEmitted,
            this, [this](CTKEnhancedLogger::LogLevel level, 
                         CTKEnhancedLogger::LogCategory category,
                         const QString& message, const QString& timestamp) {
        // 如果是错误级别，自动显示错误日志窗口
        if (level >= CTKEnhancedLogger::ERROR) {
            m_errorLogDock->show();
            m_errorLogDock->raise();
        }
    });
    
    // 设置停靠窗口
    m_errorLogDock->setWidget(m_errorLogWidget);
    m_errorLogDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    m_errorLogDock->setFeatures(QDockWidget::DockWidgetMovable | 
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetClosable);
    
    addDockWidget(Qt::BottomDockWidgetArea, m_errorLogDock);
    m_errorLogDock->hide();  // 默认隐藏，出错时显示
    
    // 添加到视图菜单
    QAction* showErrorLogAction = m_errorLogDock->toggleViewAction();
    showErrorLogAction->setText("错误日志");
    
    // 获取视图菜单
    QMenu* viewMenu = nullptr;
    for (QAction* action : ui->menubar->actions()) {
        if (action->menu() && action->text().contains("视图")) {
            viewMenu = action->menu();
            break;
        }
    }
    if (!viewMenu) {
        viewMenu = ui->menubar->addMenu("视图(&V)");
    }
    viewMenu->addAction(showErrorLogAction);
    
    CTK_INFO("错误日志停靠窗口已创建");
}

void MainWindow::applyCTKMedicalTheme()
{
    // 应用医疗软件专用样式表
    QString medicalStyleSheet = R"(
        /* CTK医疗软件主题样式 */
        ctkCollapsibleGroupBox {
            background-color: #f8f9fa;
            border: 1px solid #dee2e6;
            border-radius: 6px;
            font-weight: bold;
            margin: 2px;
            padding: 8px;
        }
        
        ctkCollapsibleGroupBox::title {
            color: #2d3748;
            background-color: #e2e8f0;
            padding: 6px 12px;
            border-radius: 4px;
            font-size: 11pt;
        }
        
        ctkDoubleSpinBox {
            background-color: white;
            border: 1px solid #cbd5e0;
            border-radius: 4px;
            padding: 4px;
            font-family: "Consolas", monospace;
            font-size: 10pt;
        }
        
        ctkDoubleSpinBox:focus {
            border-color: #4299e1;
            outline: none;
        }
        
        ctkSliderWidget {
            background-color: #f7fafc;
            padding: 8px;
            border-radius: 4px;
        }
        
        ctkColorPickerButton {
            border: 2px solid #a0aec0;
            border-radius: 6px;
            min-width: 40px;
            min-height: 25px;
        }
        
        ctkColorPickerButton:hover {
            border-color: #4299e1;
        }
        
        /* 状态指示器颜色 */
        .status-connected {
            background-color: #48bb78;
        }
        
        .status-disconnected {
            background-color: #f56565;
        }
        
        .status-tracking {
            background-color: #4299e1;
        }
        
        .status-idle {
            background-color: #a0aec0;
        }
        
        /* 停靠窗口标题栏 */
        QDockWidget::title {
            background-color: #edf2f7;
            color: #2d3748;
            padding: 4px 8px;
            text-align: left;
            font-weight: bold;
        }
        
        QDockWidget::close-button, QDockWidget::float-button {
            background-color: #cbd5e0;
            border: 1px solid #a0aec0;
            border-radius: 3px;
        }
        
        QDockWidget::close-button:hover, QDockWidget::float-button:hover {
            background-color: #a0aec0;
        }
    )";
    
    setStyleSheet(styleSheet() + medicalStyleSheet);
    
    CTK_INFO("医疗软件专用主题已应用");
}

void MainWindow::updateCTKStatusIndicators()
{
    if (!m_deviceStatusIndicator || !m_trackingStatusIndicator) return;
    
    // 更新设备连接状态指示器
    if (m_deviceConnected) {
        m_deviceStatusIndicator->setColor(QColor("#48bb78")); // 绿色
        m_deviceStatusLabel->setText("已连接");
    } else {
        m_deviceStatusIndicator->setColor(QColor("#f56565")); // 红色
        m_deviceStatusLabel->setText("未连接");
    }
    
    // 更新追踪状态指示器
    if (m_trackingActive) {
        m_trackingStatusIndicator->setColor(QColor("#4299e1")); // 蓝色
        m_trackingStatusLabel->setText("追踪中");
    } else {
        m_trackingStatusIndicator->setColor(QColor("#a0aec0")); // 灰色
        m_trackingStatusLabel->setText(m_deviceConnected ? "已停止" : "未追踪");
    }
    
    CTK_TRACKING_LOG("状态指示器已更新", (QVariantMap{
        {"device_connected", m_deviceConnected},
        {"tracking_active", m_trackingActive}
    }));
}

void MainWindow::updateCTKFrameRateDisplay(double frameRate)
{
    if (!m_frameRateStatusLabel) return;
    
    m_frameRateStatusLabel->setText(QString("%1 fps").arg(frameRate, 0, 'f', 1));
    
    // 根据帧率设置文本颜色
    QString color;
    if (frameRate >= 25.0) {
        color = "#48bb78";  // 绿色 - 良好
    } else if (frameRate >= 15.0) {
        color = "#ed8936";  // 橙色 - 一般
    } else {
        color = "#f56565";  // 红色 - 较差
    }
    
    m_frameRateStatusLabel->setStyleSheet(
        QString("QLabel { font-family: 'Consolas', monospace; color: %1; font-weight: bold; }")
        .arg(color));
}

// CTK组件事件处理
void MainWindow::onCTKPrecisionThresholdChanged(double value)
{
    CTK_TRACKING_LOG("精度阈值变更", (QVariantMap{{"threshold_mm", value}}));
    
    if (m_trackingService) {
        // 如果追踪服务有设置精度的方法，在这里调用
        logTrackingMessage(QString("精度阈值设置为: %1 mm").arg(value, 0, 'f', 3));
    }
}

void MainWindow::onCTKFrameRateTargetChanged(double value)
{
    CTK_TRACKING_LOG("目标帧率变更", (QVariantMap{{"target_fps", value}}));
    
    if (m_trackingService) {
        // 如果追踪服务有设置帧率的方法，在这里调用
        logTrackingMessage(QString("目标帧率设置为: %1 fps").arg(value, 0, 'f', 1));
    }
}

void MainWindow::onCTKMarkerColorChanged(const QColor& color)
{
    CTK_TRACKING_LOG("标记物颜色变更", (QVariantMap{{"color", color.name()}}));
    logTrackingMessage(QString("标记物颜色设置为: %1").arg(color.name()));
    
    // 这里可以通知3D渲染系统更新标记物颜色
}

void MainWindow::onCTKTipColorChanged(const QColor& color)
{
    CTK_TRACKING_LOG("针尖颜色变更", (QVariantMap{{"color", color.name()}}));
    logTrackingMessage(QString("针尖颜色设置为: %1").arg(color.name()));
    
    // 这里可以通知3D渲染系统更新针尖颜色
}

void MainWindow::onCTKLogLevelChanged(int level)
{
    if (m_enhancedLogger) {
        m_enhancedLogger->setLogLevel(static_cast<CTKEnhancedLogger::LogLevel>(level));
        CTK_INFO(QString("日志级别已设置为: %1").arg(level));
    }
}

// ========================================
// 患者管理插件功能实现
// ========================================

void MainWindow::initializePatientManagementService(ctkPluginContext* context)
{
    if (!context) {
        logTrackingMessage("警告: 无法初始化患者管理服务 - 插件上下文无效");
        return;
    }
    
    try {
        // 查找患者数据库服务
        m_patientServiceRef = context->getServiceReference<PatientDatabaseService>();
        if (m_patientServiceRef) {
            m_patientService = context->getService<PatientDatabaseService>(m_patientServiceRef);
            if (m_patientService) {
                logTrackingMessage("患者管理服务已成功连接");
                onPatientDatabaseServiceAvailable(true);
            } else {
                logTrackingMessage("警告: 无法获取患者数据库服务实例");
            }
        } else {
            logTrackingMessage("提示: 患者管理插件尚未加载或不可用");
        }
    } catch (const std::exception& e) {
        logTrackingMessage(QString("初始化患者管理服务时发生错误: %1").arg(e.what()));
    }
}

void MainWindow::onOpenPatientManagement()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_patientService) {
        QMessageBox::information(this, "患者管理", 
            "患者管理插件尚未加载，请确保插件正确安装。\n\n"
            "插件位置: plugins/Patient_Management.dll");
        return;
    }
    
    // 显示患者管理选择对话框
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("患者管理系统");
    msgBox.setText("请选择要进行的操作：");
    
    QPushButton* listButton = msgBox.addButton("患者列表管理", QMessageBox::ActionRole);
    QPushButton* addButton = msgBox.addButton("新增患者信息", QMessageBox::ActionRole);
    QPushButton* cancelButton = msgBox.addButton("取消", QMessageBox::RejectRole);
    
    msgBox.setDefaultButton(listButton);
    msgBox.exec();
    
    if (msgBox.clickedButton() == listButton) {
        onOpenPatientListView();
    } else if (msgBox.clickedButton() == addButton) {
        onOpenPatientInfoEntry();
    }
#else
    QMessageBox::information(this, "功能不可用", "此功能需要CTK插件框架支持");
#endif
}

void MainWindow::onOpenPatientInfoEntry()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_patientService) {
        QMessageBox::warning(this, "服务不可用", "患者管理服务未连接，无法打开患者信息录入界面");
        return;
    }
    
    // 遵循CTK架构原则：通过服务接口请求显示UI
    if (m_patientService->showPatientInfoDialog(this)) {
        statusBar()->showMessage("患者信息录入界面已打开", 3000);
        logTrackingMessage("患者信息录入界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开患者信息录入界面");
    }
#endif
}

void MainWindow::onOpenPatientListView()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_patientService) {
        QMessageBox::warning(this, "服务不可用", "患者管理服务未连接，无法打开患者列表");
        return;
    }
    
    // 遵循CTK架构原则：通过服务接口请求显示UI
    if (m_patientService->showPatientListDialog(this)) {
        statusBar()->showMessage("患者列表管理界面已打开", 3000);
        logTrackingMessage("患者列表管理界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开患者列表管理界面");
    }
#endif
}

void MainWindow::onPatientDatabaseServiceAvailable(bool available)
{
    if (available) {
        logTrackingMessage("患者数据库服务已可用");
        // 可以在这里启用相关的UI元素
    } else {
        logTrackingMessage("患者数据库服务不可用");
        // 可以在这里禁用相关的UI元素
    }
}

void MainWindow::addPatientManagementMenuItem()
{
    // 查找或创建患者管理菜单
    QMenu* patientMenu = nullptr;
    
    // 查找现有的患者管理相关菜单
    QList<QMenu*> menus = ui->menubar->findChildren<QMenu*>();
    for (QMenu* menu : menus) {
        if (menu->title().contains("患者") || menu->title().contains("Patient")) {
            patientMenu = menu;
            break;
        }
    }
    
    // 如果没有找到，创建新的患者管理菜单
    if (!patientMenu) {
        patientMenu = ui->menubar->addMenu("患者管理(&P)");
    }
    
    // 添加患者管理插件相关的菜单项
    if (patientMenu) {
        // 添加分隔符（如果菜单不为空）
        if (!patientMenu->actions().isEmpty()) {
            patientMenu->addSeparator();
        }
        
        // 患者管理系统主入口
        QAction* patientMgmtAction = patientMenu->addAction("患者管理系统");
        patientMgmtAction->setShortcut(QKeySequence("Ctrl+P"));
        patientMgmtAction->setToolTip("打开患者管理系统（CTK插件）");
        connect(patientMgmtAction, &QAction::triggered, this, &MainWindow::onOpenPatientManagement);
        
        // 快速录入患者信息
        QAction* patientEntryAction = patientMenu->addAction("新增患者信息");
        patientEntryAction->setShortcut(QKeySequence("Ctrl+Shift+P"));
        patientEntryAction->setToolTip("快速录入新患者信息");
        connect(patientEntryAction, &QAction::triggered, this, &MainWindow::onOpenPatientInfoEntry);
        
        // 患者列表查看
        QAction* patientListAction = patientMenu->addAction("患者列表管理");
        patientListAction->setShortcut(QKeySequence("Ctrl+L"));
        patientListAction->setToolTip("查看和管理患者列表");
        connect(patientListAction, &QAction::triggered, this, &MainWindow::onOpenPatientListView);
        
        logTrackingMessage("患者管理菜单项已添加");
    }
}

// ========================================
// 医学图像管理插件功能实现
// ========================================

void MainWindow::initializeMedicalImageService(ctkPluginContext* context)
{
    if (!context) {
        logTrackingMessage("警告: 无法初始化医学图像服务 - 插件上下文无效");
        return;
    }
    
    try {
        // 查找医学图像服务（CTK标准方式）
        m_imageServiceRef = context->getServiceReference("medical.MedicalImageCoreService");
        if (m_imageServiceRef) {
            m_imageService = qobject_cast<MedicalImageCoreService*>(context->getService(m_imageServiceRef));
            if (m_imageService) {
                logTrackingMessage("医学图像服务已成功连接");
                onMedicalImageServiceAvailable(true);
            } else {
                logTrackingMessage("警告: 无法获取医学图像服务实例");
            }
        } else {
            logTrackingMessage("提示: 医学图像管理插件尚未加载或不可用");
        }
    } catch (const std::exception& e) {
        logTrackingMessage(QString("初始化医学图像服务时发生异常: %1").arg(e.what()));
    }
}

void MainWindow::onOpenMedicalImageManager()
{
    if (!m_imageService) {
        QMessageBox::information(this, "医学图像管理", 
            "医学图像管理插件尚未加载，请确保插件正确安装。\n\n"
            "插件位置: plugins/MedicalImageCore.dll");
        return;
    }
    
    // 遵循CTK架构原则：通过服务接口请求显示UI（与其他插件保持一致的直接调用方式）
    if (m_imageService->showImageManagerDialog(this)) {
        statusBar()->showMessage("医学图像管理界面已打开", 3000);
        logTrackingMessage("医学图像管理界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开医学图像管理界面");
    }
}

void MainWindow::onOpenImagePropertiesViewer()
{
    if (!m_imageService) {
        QMessageBox::warning(this, "服务不可用", "医学图像服务未连接，无法打开图像属性查看器");
        return;
    }
    
    // 遵循CTK架构原则：通过服务接口请求显示UI（与其他插件保持一致的直接调用方式）
    if (m_imageService->showImagePropertiesDialog(this)) {
        statusBar()->showMessage("图像属性查看器已打开", 3000);
        logTrackingMessage("图像属性查看器已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开图像属性查看器");
    }
}

void MainWindow::onOpenImageLoaderConfig()
{
    if (!m_imageService) {
        QMessageBox::warning(this, "服务不可用", "医学图像服务未连接，无法打开加载器配置");
        return;
    }
    
    // 遵循CTK架构原则：通过服务接口请求显示UI（与其他插件保持一致的直接调用方式）
    if (m_imageService->showLoaderConfigDialog(this)) {
        statusBar()->showMessage("图像加载器配置界面已打开", 3000);
        logTrackingMessage("图像加载器配置界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开图像加载器配置界面");
    }
}

void MainWindow::onMedicalImageServiceAvailable(bool available)
{
    if (available) {
        logTrackingMessage("医学图像服务现在可用");
        // 可以在这里启用相关的UI元素
    } else {
        logTrackingMessage("医学图像服务不可用");
        // 可以在这里禁用相关的UI元素
    }
}

void MainWindow::addMedicalImageMenuItem()
{
    // 查找或创建医学图像菜单
    QMenu* imageMenu = nullptr;
    
    // 查找现有的医学图像相关菜单
    QList<QMenu*> menus = ui->menubar->findChildren<QMenu*>();
    for (QMenu* menu : menus) {
        if (menu->title().contains("图像") || menu->title().contains("Image")) {
            imageMenu = menu;
            break;
        }
    }
    
    // 如果没有找到，创建新的医学图像菜单
    if (!imageMenu) {
        imageMenu = ui->menubar->addMenu("医学图像(&I)");
    }
    
    // 添加医学图像管理插件相关的菜单项
    if (imageMenu) {
        // 添加分隔符（如果菜单不为空）
        if (!imageMenu->actions().isEmpty()) {
            imageMenu->addSeparator();
        }
        
        // 医学图像管理系统主入口
        QAction* imageMgmtAction = imageMenu->addAction("医学图像管理");
        imageMgmtAction->setShortcut(QKeySequence("Ctrl+I"));
        imageMgmtAction->setToolTip("打开医学图像管理系统（CTK插件）");
        connect(imageMgmtAction, &QAction::triggered, this, &MainWindow::onOpenMedicalImageManager);
        
        // 图像属性查看器
        QAction* imagePropsAction = imageMenu->addAction("图像属性查看");
        imagePropsAction->setShortcut(QKeySequence("Ctrl+Shift+I"));
        imagePropsAction->setToolTip("查看医学图像属性信息");
        connect(imagePropsAction, &QAction::triggered, this, &MainWindow::onOpenImagePropertiesViewer);
        
        // 图像加载器配置
        QAction* loaderConfigAction = imageMenu->addAction("加载器配置");
        loaderConfigAction->setShortcut(QKeySequence("Ctrl+Alt+I"));
        loaderConfigAction->setToolTip("配置医学图像加载器");
        connect(loaderConfigAction, &QAction::triggered, this, &MainWindow::onOpenImageLoaderConfig);
        
        logTrackingMessage("医学图像管理菜单项已添加");
    }
}

// ========================================
// 图像交互插件功能实现
// ========================================

void MainWindow::initializeImageInteractionService(ctkPluginContext* context)
{
    if (!context) {
        logTrackingMessage("警告: 无法初始化图像交互服务 - 插件上下文无效");
        return;
    }
    
    try {
        // 查找图像交互服务
        m_imageInteractionServiceRef = context->getServiceReference<ImageInteractionService>();
        if (m_imageInteractionServiceRef) {
            m_imageInteractionService = context->getService<ImageInteractionService>(m_imageInteractionServiceRef);
            if (m_imageInteractionService) {
                logTrackingMessage("图像交互服务已成功连接");
                onImageInteractionServiceAvailable(true);
            } else {
                logTrackingMessage("警告: 无法获取图像交互服务实例");
            }
        } else {
            logTrackingMessage("提示: 图像交互插件尚未加载或不可用");
        }
    } catch (const std::exception& e) {
        logTrackingMessage(QString("初始化图像交互服务时发生异常: %1").arg(e.what()));
    }
}

void MainWindow::onOpenImageInteractionDialog()
{
    if (!m_imageInteractionService) {
        QMessageBox::information(this, "图像交互", 
            "图像交互插件尚未加载，请确保插件正确安装。\n\n"
            "插件位置: plugins/ImageInteraction.dll");
        return;
    }
    
    if (m_imageInteractionService->showInteractionDialog(this)) {
        statusBar()->showMessage("图像交互界面已打开", 3000);
        logTrackingMessage("图像交互界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开图像交互界面");
    }
}

void MainWindow::onOpenPointPickerDialog()
{
    if (!m_imageInteractionService) {
        QMessageBox::warning(this, "服务不可用", "图像交互服务未连接，无法打开点拾取界面");
        return;
    }
    
    if (m_imageInteractionService->showPointPickerDialog(this)) {
        statusBar()->showMessage("点拾取界面已打开", 3000);
        logTrackingMessage("点拾取界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开点拾取界面");
    }
}

void MainWindow::onOpenMeasurementDialog()
{
    if (!m_imageInteractionService) {
        QMessageBox::warning(this, "服务不可用", "图像交互服务未连接，无法打开测量工具界面");
        return;
    }
    
    if (m_imageInteractionService->showMeasurementDialog(this)) {
        statusBar()->showMessage("测量工具界面已打开", 3000);
        logTrackingMessage("测量工具界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开测量工具界面");
    }
}

void MainWindow::onOpenAnnotationDialog()
{
    if (!m_imageInteractionService) {
        QMessageBox::warning(this, "服务不可用", "图像交互服务未连接，无法打开标注工具界面");
        return;
    }
    
    if (m_imageInteractionService->showAnnotationDialog(this)) {
        statusBar()->showMessage("标注工具界面已打开", 3000);
        logTrackingMessage("标注工具界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开标注工具界面");
    }
}

void MainWindow::onImageInteractionServiceAvailable(bool available)
{
    if (available) {
        logTrackingMessage("图像交互服务现在可用");
    } else {
        logTrackingMessage("图像交互服务不可用");
    }
}

void MainWindow::addImageInteractionMenuItem()
{
    // 查找或创建图像交互菜单
    QMenu* interactionMenu = nullptr;
    
    // 查找现有的图像交互相关菜单
    QList<QMenu*> menus = ui->menubar->findChildren<QMenu*>();
    for (QMenu* menu : menus) {
        if (menu->title().contains("交互") || menu->title().contains("Interaction")) {
            interactionMenu = menu;
            break;
        }
    }
    
    // 如果没有找到，创建新的图像交互菜单
    if (!interactionMenu) {
        interactionMenu = ui->menubar->addMenu("图像交互(&T)");
    }
    
    // 添加图像交互插件相关的菜单项
    if (interactionMenu) {
        // 添加分隔符（如果菜单不为空）
        if (!interactionMenu->actions().isEmpty()) {
            interactionMenu->addSeparator();
        }
        
        // 图像交互主入口
        QAction* interactionAction = interactionMenu->addAction("图像交互工具");
        interactionAction->setShortcut(QKeySequence("Ctrl+T"));
        interactionAction->setToolTip("打开图像交互工具（CTK插件）");
        connect(interactionAction, &QAction::triggered, this, &MainWindow::onOpenImageInteractionDialog);
        
        // 点拾取工具
        QAction* pointPickerAction = interactionMenu->addAction("点拾取工具");
        pointPickerAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
        pointPickerAction->setToolTip("打开点拾取工具");
        connect(pointPickerAction, &QAction::triggered, this, &MainWindow::onOpenPointPickerDialog);
        
        // 测量工具
        QAction* measurementAction = interactionMenu->addAction("测量工具");
        measurementAction->setShortcut(QKeySequence("Ctrl+M"));
        measurementAction->setToolTip("打开测量工具");
        connect(measurementAction, &QAction::triggered, this, &MainWindow::onOpenMeasurementDialog);
        
        // 标注工具
        QAction* annotationAction = interactionMenu->addAction("标注工具");
        annotationAction->setShortcut(QKeySequence("Ctrl+A"));
        annotationAction->setToolTip("打开标注工具");
        connect(annotationAction, &QAction::triggered, this, &MainWindow::onOpenAnnotationDialog);
        
        logTrackingMessage("图像交互菜单项已添加");
    }
}

// ========================================
// 医学查看器插件功能实现
// ========================================

void MainWindow::initializeMedicalViewerService(ctkPluginContext* context)
{
    if (!context) {
        logTrackingMessage("警告: 无法初始化医学查看器服务 - 插件上下文无效");
        return;
    }
    
    try {
        // 查找医学查看器服务
        m_medicalViewerServiceRef = context->getServiceReference<MedicalViewerService>();
        if (m_medicalViewerServiceRef) {
            m_medicalViewerService = context->getService<MedicalViewerService>(m_medicalViewerServiceRef);
            if (m_medicalViewerService) {
                logTrackingMessage("医学查看器服务已成功连接");
                onMedicalViewerServiceAvailable(true);
            } else {
                logTrackingMessage("警告: 无法获取医学查看器服务实例");
            }
        } else {
            logTrackingMessage("提示: 医学查看器插件尚未加载或不可用");
        }
    } catch (const std::exception& e) {
        logTrackingMessage(QString("初始化医学查看器服务时发生异常: %1").arg(e.what()));
    }
}

void MainWindow::onOpenMedicalViewerDialog()
{
    if (!m_medicalViewerService) {
        QMessageBox::information(this, "医学查看器", 
            "医学查看器插件尚未加载，请确保插件正确安装。\n\n"
            "插件位置: plugins/MedicalViewer.dll");
        return;
    }
    
    if (m_medicalViewerService->showViewerDialog(this)) {
        statusBar()->showMessage("医学查看器界面已打开", 3000);
        logTrackingMessage("医学查看器界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开医学查看器界面");
    }
}

void MainWindow::onOpenMPRDialog()
{
    if (!m_medicalViewerService) {
        QMessageBox::warning(this, "服务不可用", "医学查看器服务未连接，无法打开MPR界面");
        return;
    }
    
    if (m_medicalViewerService->showMPRDialog(this)) {
        statusBar()->showMessage("MPR多平面重建界面已打开", 3000);
        logTrackingMessage("MPR多平面重建界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开MPR界面");
    }
}

void MainWindow::onOpenVolumeRenderingDialog()
{
    if (!m_medicalViewerService) {
        QMessageBox::warning(this, "服务不可用", "医学查看器服务未连接，无法打开体绘制界面");
        return;
    }
    
    if (m_medicalViewerService->showVolumeRenderingDialog(this)) {
        statusBar()->showMessage("体绘制界面已打开", 3000);
        logTrackingMessage("体绘制界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开体绘制界面");
    }
}

void MainWindow::onOpenViewerConfigDialog()
{
    if (!m_medicalViewerService) {
        QMessageBox::warning(this, "服务不可用", "医学查看器服务未连接，无法打开查看器配置界面");
        return;
    }
    
    if (m_medicalViewerService->showViewerConfigDialog(this)) {
        statusBar()->showMessage("查看器配置界面已打开", 3000);
        logTrackingMessage("查看器配置界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开查看器配置界面");
    }
}

void MainWindow::onOpenNrrdViewerDialog()
{
    if (!m_medicalViewerService) {
        QMessageBox::warning(this, "服务不可用", "医学查看器服务未连接，无法打开NRRD查看器界面");
        return;
    }
    
    if (m_medicalViewerService->showNrrdViewerDialog(this)) {
        statusBar()->showMessage("NRRD查看器界面已打开", 3000);
        logTrackingMessage("NRRD查看器界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开NRRD查看器界面");
    }
}

void MainWindow::onOpenTransferFunctionEditorDialog()
{
    if (!m_medicalViewerService) {
        QMessageBox::warning(this, "服务不可用", "医学查看器服务未连接，无法打开传输函数编辑器");
        return;
    }
    
    if (m_medicalViewerService->showTransferFunctionEditorDialog(this)) {
        statusBar()->showMessage("传输函数编辑器已打开", 3000);
        logTrackingMessage("传输函数编辑器已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开传输函数编辑器");
    }
}

void MainWindow::onOpenScientificVisualizationDialog()
{
    if (!m_medicalViewerService) {
        QMessageBox::warning(this, "服务不可用", "医学查看器服务未连接，无法打开科学可视化界面");
        return;
    }
    
    if (m_medicalViewerService->showScientificVisualizationDialog(this)) {
        statusBar()->showMessage("科学可视化界面已打开", 3000);
        logTrackingMessage("科学可视化界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开科学可视化界面");
    }
}

void MainWindow::onMedicalViewerServiceAvailable(bool available)
{
    if (available) {
        logTrackingMessage("医学查看器服务现在可用");
    } else {
        logTrackingMessage("医学查看器服务不可用");
    }
}

void MainWindow::addMedicalViewerMenuItem()
{
    // 查找或创建医学查看器菜单
    QMenu* viewerMenu = nullptr;
    
    // 查找现有的医学查看器相关菜单
    QList<QMenu*> menus = ui->menubar->findChildren<QMenu*>();
    for (QMenu* menu : menus) {
        if (menu->title().contains("查看器") || menu->title().contains("Viewer")) {
            viewerMenu = menu;
            break;
        }
    }
    
    // 如果没有找到，创建新的医学查看器菜单
    if (!viewerMenu) {
        viewerMenu = ui->menubar->addMenu("医学查看器(&V)");
    }
    
    // 添加医学查看器插件相关的菜单项
    if (viewerMenu) {
        // 添加分隔符（如果菜单不为空）
        if (!viewerMenu->actions().isEmpty()) {
            viewerMenu->addSeparator();
        }
        
        // 医学查看器主入口
        QAction* viewerAction = viewerMenu->addAction("医学查看器");
        viewerAction->setShortcut(QKeySequence("Ctrl+V"));
        viewerAction->setToolTip("打开医学查看器（CTK插件）");
        connect(viewerAction, &QAction::triggered, this, &MainWindow::onOpenMedicalViewerDialog);
        
        // MPR多平面重建
        QAction* mprAction = viewerMenu->addAction("MPR多平面重建");
        mprAction->setShortcut(QKeySequence("Ctrl+Shift+V"));
        mprAction->setToolTip("打开MPR多平面重建界面");
        connect(mprAction, &QAction::triggered, this, &MainWindow::onOpenMPRDialog);
        
        // 体绘制
        QAction* volumeAction = viewerMenu->addAction("体绘制");
        volumeAction->setShortcut(QKeySequence("Ctrl+U"));
        volumeAction->setToolTip("打开体绘制界面");
        connect(volumeAction, &QAction::triggered, this, &MainWindow::onOpenVolumeRenderingDialog);
        
        // NRRD查看器
        QAction* nrrdAction = viewerMenu->addAction("NRRD查看器");
        nrrdAction->setShortcut(QKeySequence("Ctrl+N"));
        nrrdAction->setToolTip("打开NRRD查看器");
        connect(nrrdAction, &QAction::triggered, this, &MainWindow::onOpenNrrdViewerDialog);
        
        // 查看器配置
        QAction* configAction = viewerMenu->addAction("查看器配置");
        configAction->setToolTip("配置医学查看器");
        connect(configAction, &QAction::triggered, this, &MainWindow::onOpenViewerConfigDialog);
        
        // 传输函数编辑器
        QAction* transferAction = viewerMenu->addAction("传输函数编辑器");
        transferAction->setToolTip("打开传输函数编辑器");
        connect(transferAction, &QAction::triggered, this, &MainWindow::onOpenTransferFunctionEditorDialog);
        
        // 科学可视化
        QAction* sciVisAction = viewerMenu->addAction("科学可视化");
        sciVisAction->setToolTip("打开科学可视化界面");
        connect(sciVisAction, &QAction::triggered, this, &MainWindow::onOpenScientificVisualizationDialog);
        
        logTrackingMessage("医学查看器菜单项已添加");
    }
}

// ========================================
// 医学处理插件功能实现
// ========================================

void MainWindow::initializeMedicalProcessingService(ctkPluginContext* context)
{
    if (!context) {
        logTrackingMessage("警告: 无法初始化医学处理服务 - 插件上下文无效");
        return;
    }
    
    try {
        // 查找医学处理服务
        m_medicalProcessingServiceRef = context->getServiceReference<MedicalProcessingService>();
        if (m_medicalProcessingServiceRef) {
            m_medicalProcessingService = context->getService<MedicalProcessingService>(m_medicalProcessingServiceRef);
            if (m_medicalProcessingService) {
                logTrackingMessage("医学处理服务已成功连接");
                onMedicalProcessingServiceAvailable(true);
            } else {
                logTrackingMessage("警告: 无法获取医学处理服务实例");
            }
        } else {
            logTrackingMessage("提示: 医学处理插件尚未加载或不可用");
        }
    } catch (const std::exception& e) {
        logTrackingMessage(QString("初始化医学处理服务时发生异常: %1").arg(e.what()));
    }
}

void MainWindow::onOpenMedicalProcessingDialog()
{
    if (!m_medicalProcessingService) {
        QMessageBox::information(this, "医学处理", 
            "医学处理插件尚未加载，请确保插件正确安装。\n\n"
            "插件位置: plugins/MedicalProcessing.dll");
        return;
    }
    
    if (m_medicalProcessingService->showProcessingDialog(this)) {
        statusBar()->showMessage("医学处理界面已打开", 3000);
        logTrackingMessage("医学处理界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开医学处理界面");
    }
}

void MainWindow::onOpenBatchProcessingDialog()
{
    if (!m_medicalProcessingService) {
        QMessageBox::warning(this, "服务不可用", "医学处理服务未连接，无法打开批量处理界面");
        return;
    }
    
    if (m_medicalProcessingService->showBatchProcessingDialog(this)) {
        statusBar()->showMessage("批量处理界面已打开", 3000);
        logTrackingMessage("批量处理界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开批量处理界面");
    }
}

void MainWindow::onOpenAlgorithmConfigDialog()
{
    if (!m_medicalProcessingService) {
        QMessageBox::warning(this, "服务不可用", "医学处理服务未连接，无法打开算法配置界面");
        return;
    }
    
    if (m_medicalProcessingService->showAlgorithmConfigDialog(this)) {
        statusBar()->showMessage("算法配置界面已打开", 3000);
        logTrackingMessage("算法配置界面已打开");
    } else {
        QMessageBox::warning(this, "界面错误", "无法打开算法配置界面");
    }
}

void MainWindow::onMedicalProcessingServiceAvailable(bool available)
{
    if (available) {
        logTrackingMessage("医学处理服务现在可用");
    } else {
        logTrackingMessage("医学处理服务不可用");
    }
}

void MainWindow::addMedicalProcessingMenuItem()
{
    // 查找或创建医学处理菜单
    QMenu* processingMenu = nullptr;
    
    // 查找现有的医学处理相关菜单
    QList<QMenu*> menus = ui->menubar->findChildren<QMenu*>();
    for (QMenu* menu : menus) {
        if (menu->title().contains("处理") || menu->title().contains("Processing")) {
            processingMenu = menu;
            break;
        }
    }
    
    // 如果没有找到，创建新的医学处理菜单
    if (!processingMenu) {
        processingMenu = ui->menubar->addMenu("医学处理(&R)");
    }
    
    // 添加医学处理插件相关的菜单项
    if (processingMenu) {
        // 添加分隔符（如果菜单不为空）
        if (!processingMenu->actions().isEmpty()) {
            processingMenu->addSeparator();
        }
        
        // 医学处理主入口
        QAction* processingAction = processingMenu->addAction("医学处理工具");
        processingAction->setShortcut(QKeySequence("Ctrl+R"));
        processingAction->setToolTip("打开医学处理工具（CTK插件）");
        connect(processingAction, &QAction::triggered, this, &MainWindow::onOpenMedicalProcessingDialog);
        
        // 批量处理
        QAction* batchAction = processingMenu->addAction("批量处理");
        batchAction->setShortcut(QKeySequence("Ctrl+Shift+R"));
        batchAction->setToolTip("打开批量处理界面");
        connect(batchAction, &QAction::triggered, this, &MainWindow::onOpenBatchProcessingDialog);
        
        // 算法配置
        QAction* configAction = processingMenu->addAction("算法配置");
        configAction->setShortcut(QKeySequence("Ctrl+Alt+R"));
        configAction->setToolTip("配置图像处理算法");
        connect(configAction, &QAction::triggered, this, &MainWindow::onOpenAlgorithmConfigDialog);
        
        logTrackingMessage("医学处理菜单项已添加");
    }
}

#endif // CTK_PLUGIN_FRAMEWORK
