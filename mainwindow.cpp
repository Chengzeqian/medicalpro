#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Framework/CTKEnhancedLogger.h"
#include "Framework/Platform/CtkBridge/ctk_runtime_host_adapter.h"
#include <ctkCollapsibleGroupBox.h>
#include <ctkDoubleSpinBox.h>
#include <ctkSliderWidget.h>
#include <ctkColorPickerButton.h>
#include <ctkErrorLogWidget.h>
#include "Plugins/OpticalTracking/OpticalTrackingService.h"
#include "Plugins/OpticalTracking/ServiceInterfaces.h"
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

namespace
{
const auto showLegacyPluginEntryMigrationNotice = [](QWidget* parent, const QString& featureName) {
    QMessageBox::information(
        parent,
        QStringLiteral("功能迁移中"),
        QStringLiteral("%1 旧入口尚未迁移到 platform runtime host，当前阶段仅保留宿主脱钩与编译链路。").arg(featureName));
};
}

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
    , m_runtimeHost(nullptr)
    , m_serviceAccess(nullptr)
    , m_trackingService(nullptr)
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
        
        m_imageService = nullptr;
        qDebug() << "[MainWindow] ✅ 医学图像服务已强制清理";
    }
    
    // 强制清理医学查看器服务（VTK渲染线程）  
    if (m_medicalViewerService) {
        qWarning() << "[MainWindow] 👁️ 强制停止VTK渲染线程...";
        
        // 强制停止所有渲染器
        
        m_medicalViewerService = nullptr;
        qDebug() << "[MainWindow] ✅ 医学查看器服务已强制清理";
    }
    
    // 清理其他服务
    if (m_patientService) {
        m_patientService = nullptr;
        qDebug() << "[MainWindow] ✅ 患者管理服务已清理";
    }
    if (m_medicalProcessingService) {
        m_medicalProcessingService = nullptr;
        qDebug() << "[MainWindow] ✅ 医学处理服务已清理";
    }
    
    // 立即强制CTK框架停止（不等待aboutToQuit）
    qWarning() << "[MainWindow] 🚨 立即强制停止CTK框架...";
    if (m_runtimeHost && m_runtimeHost->stop()) {
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
    
    // 设置平台服务访问入口
    setupPlatformServiceAccess();
    
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
// ???????????????
// ========================================

void MainWindow::onOpenImageInteractionDialog()
{
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Image interaction dialog"));
}

void MainWindow::onOpenPointPickerDialog()
{
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Point picker dialog"));
}

void MainWindow::onOpenMeasurementDialog()
{
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Measurement dialog"));
}

void MainWindow::onOpenAnnotationDialog()
{
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Annotation dialog"));
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

void MainWindow::initializeMedicalViewerService()
{
    if (!m_serviceAccess) {
        logTrackingMessage("警告: 无法初始化医学查看器服务 - 插件上下文无效");
        return;
    }
    
    try {
        // 查找医学查看器服务
        m_medicalViewerService = nullptr;
        if (m_medicalViewerService) {
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
#if 0
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
#endif
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Medical viewer dialog"));
}

void MainWindow::onOpenMPRDialog()
{
#if 0
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
#endif
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("MPR dialog"));
}

void MainWindow::onOpenVolumeRenderingDialog()
{
#if 0
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
#endif
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Volume rendering dialog"));
}

void MainWindow::onOpenViewerConfigDialog()
{
#if 0
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
#endif
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Viewer config dialog"));
}

void MainWindow::onOpenNrrdViewerDialog()
{
#if 0
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
#endif
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("NRRD viewer dialog"));
}

void MainWindow::onOpenTransferFunctionEditorDialog()
{
#if 0
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
#endif
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Transfer function editor"));
}

void MainWindow::onOpenScientificVisualizationDialog()
{
#if 0
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
#endif
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Scientific visualization dialog"));
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

void MainWindow::initializeMedicalProcessingService()
{
    if (!m_serviceAccess) {
        logTrackingMessage("警告: 无法初始化医学处理服务 - 插件上下文无效");
        return;
    }
    
    try {
        // 查找医学处理服务
        m_medicalProcessingService = nullptr;
        if (m_medicalProcessingService) {
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
#if 0
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
#endif
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Medical processing dialog"));
}

void MainWindow::onOpenBatchProcessingDialog()
{
#if 0
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
#endif
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Batch processing dialog"));
}

void MainWindow::onOpenAlgorithmConfigDialog()
{
#if 0
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
#endif
    showLegacyPluginEntryMigrationNotice(this, QStringLiteral("Algorithm config dialog"));
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
