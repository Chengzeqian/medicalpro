#include "NavigationPage.h"
#include "ui_NavigationPage.h"

#include <QFileDialog>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QDir>
#include <QFileInfo>
#include <QTabWidget>
#include <cmath>

// 导航3D视图和模拟器
#include "UI/Widgets/Navigation3DViewWidget.h"
#include "Plugins/OpticalTracking/BoneSurfaceMotionSimulator.h"

#ifdef CTK_PLUGIN_FRAMEWORK
#include "UI/Dialogs/InstrumentPreviewDialog.h"
#include "Framework/CTKManager.h"
#include "Plugins/InstrumentManagement/InstrumentManagementService.h"
#include "Plugins/DicomViewer/DicomViewerService.h"
#include "Plugins/FourViewDisplay/FourViewDisplayService.h"
#include "Plugins/OpticalTracking/OpticalTrackingService.h"
#include "Plugins/BoneSegmentation/SegmentationService.h"
#include "Plugins/PointRegistration/PointRegistrationService.h"
#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"
#include "Plugins/PointRegistration/RegistrationWorkflow.h"
#include <vtkSTLWriter.h>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#endif

NavigationPageNew::NavigationPageNew(QWidget* parent)
    : BasePage(parent)
    , ui(new Ui::NavigationPage)
    , m_patientId(-1)
    , m_trackerConnected(false)
    , m_navigationActive(false)
    , m_fourViewWidget(nullptr)
    , m_trackerTimer(new QTimer(this))
    , m_modelVisible(false)
    , m_registrationVTKWidget(nullptr)
    , m_selectedPointIndex(-1)
    , m_navigation3DView(nullptr)
    , m_motionSimulator(nullptr)
    , m_navigationTimer(nullptr)
#ifdef CTK_PLUGIN_FRAMEWORK
    , m_registrationWorkflow(nullptr)
    , m_fourViewService(nullptr)
    , m_trackingService(nullptr)
    , m_pointRegistrationService(nullptr)
#endif
{
    ui->setupUi(this);
    setObjectName("NavigationPage");

    // 创建导航3D视图和运动模拟器
    m_navigation3DView = new Navigation3DViewWidget(this);
    m_motionSimulator = new BoneSurfaceMotionSimulator();

    // 连接骨骼模型加载完成信号
    connect(m_navigation3DView, &Navigation3DViewWidget::boneModelLoaded,
            this, &NavigationPageNew::onNavigation3DBoneLoaded);

    // 创建导航更新定时器（30fps）
    m_navigationTimer = new QTimer(this);
    m_navigationTimer->setInterval(33);  // 约30fps
    connect(m_navigationTimer, &QTimer::timeout,
            this, &NavigationPageNew::onNavigationTimerUpdate);

    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, [this](int) {
                updateFourViewWidgetPlacement();
#ifdef CTK_PLUGIN_FRAMEWORK
                if (ui->tabWidget->currentWidget() == ui->registrationTab) {
                    setupRegistration();
                }
#endif
            });

#ifdef CTK_PLUGIN_FRAMEWORK
    if (auto* ctk = CTKManager::instance()) {
        connect(ctk, &CTKManager::pluginLoaded, this, [this](const QString& pluginName) {
            if (!pluginName.compare(QStringLiteral("PointRegistration"), Qt::CaseInsensitive)) {
                if (ui && ui->tabWidget && ui->tabWidget->currentWidget() == ui->registrationTab) {
                    setupRegistration();
                }
            }
        });
    }
#endif

    // 设置跟踪器更新定时器（30fps）
    m_trackerTimer->setInterval(33);
    connect(m_trackerTimer, &QTimer::timeout, this, &NavigationPageNew::onTrackerDataReceived);

    // 初始状态
    updateTrackerStatus(false);

    // 初始化配准功能
    setupRegistration();
}

NavigationPageNew::~NavigationPageNew()
{
    m_trackerTimer->stop();
    if (m_navigationTimer) {
        m_navigationTimer->stop();
    }
    cleanupVTKViews();

    // 清理模拟器
    delete m_motionSimulator;
    m_motionSimulator = nullptr;

    delete ui;
}

void NavigationPageNew::onActivated()
{
    BasePage::onActivated();

    // 更新患者信息显示
    if (!m_patientName.isEmpty()) {
        ui->patientInfoLabel->setText(QString("患者：%1").arg(m_patientName));
    }

    loadInstruments();
    setupVTKViews();
    updateFourViewWidgetPlacement();

#ifdef CTK_PLUGIN_FRAMEWORK
    // 恢复VTK渲染
    if (m_fourViewService) {
        m_fourViewService->resumeRendering();
    }
#endif
}

void NavigationPageNew::onDeactivated()
{
    BasePage::onDeactivated();

    // 停止跟踪器定时器
    m_trackerTimer->stop();

    // 停止导航定时器
    if (m_navigationTimer) {
        m_navigationTimer->stop();
    }

    // 暂停模拟器
    if (m_motionSimulator) {
        m_motionSimulator->setPaused(true);
    }

    // 停止导航
    if (m_navigationActive) {
        m_navigationActive = false;
    }

#ifdef CTK_PLUGIN_FRAMEWORK
    // 暂停VTK渲染（防止页面切换时闪烁）
    if (m_fourViewService) {
        m_fourViewService->pauseRendering();
    }
#endif
}

void NavigationPageNew::setPatientId(int patientId)
{
    m_patientId = patientId;
}

void NavigationPageNew::setPatientName(const QString& name)
{
    m_patientName = name;
    ui->patientInfoLabel->setText(QString("患者：%1").arg(name));
}

void NavigationPageNew::beginTransitionMask()
{
    // 预留过渡遮罩钩子，当前不做处理
}

void NavigationPageNew::endTransitionMask()
{
    // 预留过渡遮罩钩子，当前不做处理
}

void NavigationPageNew::resetPage()
{
    m_navigationActive = false;
    m_trackerConnected = false;
    m_trackerTimer->stop();
    updateTrackerStatus(false);
    // 清理患者显示
    m_patientId = -1;
    m_patientName.clear();
    ui->patientInfoLabel->setText("患者：-");
}

bool NavigationPageNew::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QFrame* card = qobject_cast<QFrame*>(obj);
        if (card && card->property("instrumentId").isValid()) {
            int instrumentId = card->property("instrumentId").toInt();
            onInstrumentCardClicked(instrumentId);
            return true;
        }
    }
    return BasePage::eventFilter(obj, event);
}

void NavigationPageNew::onInstrumentCardClicked(int instrumentId)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* instrumentService = CTKManager::instance()->getService<InstrumentManagementService>();
    if (!instrumentService) {
        showWarning("预览", "器械服务不可用");
        return;
    }

    InstrumentItem instrument = instrumentService->getInstrument(instrumentId);
    if (!instrument.isValid()) {
        showWarning("预览", "无法获取器械信息");
        return;
    }

    // 创建并显示预览对话框
    InstrumentPreviewDialog* dialog = new InstrumentPreviewDialog(instrumentService, this);
    dialog->setPreviewContent(instrument.name, instrument.modelFilePath, instrument.id);
    dialog->exec();
    delete dialog;
#else
    Q_UNUSED(instrumentId);
    showInfo("预览", "器械预览功能需要CTK框架支持");
#endif
}

void NavigationPageNew::on_backButton_clicked()
{
    if (m_navigationActive) {
        if (!showConfirm("退出导航", "导航正在进行中。确定要退出吗？")) {
            return;
        }
        m_navigationActive = false;
        m_trackerTimer->stop();
    }

    emit backToMainRequested();  // MainInterfaceWidget期望的信号
    emit navigateTo(toInt(PageIndex::Dashboard));
}

// ========== 器械管理 ==========

void NavigationPageNew::on_importInstrumentButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "导入器械模型",
        QString(), "STL文件 (*.stl);;OBJ文件 (*.obj);;所有文件 (*)");

    if (filePath.isEmpty()) {
        return;
    }

#ifdef CTK_PLUGIN_FRAMEWORK
    auto* instrumentService = CTKManager::instance()->getService<InstrumentManagementService>();
    if (instrumentService) {
        // The InstrumentManagementService API does not support direct file import here;
        // integrate a real importer when available.
        showInfo("导入", "器械导入服务功能尚未实现。");
    }
#endif

    showInfo("导入", "器械导入功能将通过CTK服务实现。");
}

void NavigationPageNew::on_deleteInstrumentButton_clicked()
{
    showInfo("删除", "请选择要删除的器械。");
}

void NavigationPageNew::on_refreshInstrumentButton_clicked()
{
    loadInstruments();
}

void NavigationPageNew::on_clearAllInstrumentButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* instrumentService = CTKManager::instance()->getService<InstrumentManagementService>();
    if (!instrumentService) {
        showWarning("清除", "器械服务不可用");
        return;
    }

    auto instruments = instrumentService->getAllInstruments();
    if (instruments.isEmpty()) {
        showInfo("清除", "器械库已为空");
        return;
    }

    if (!showConfirm("全部清除", QString("确定要删除全部 %1 个器械吗？此操作不可恢复！").arg(instruments.size()))) {
        return;
    }

    int successCount = 0;
    for (const auto& instrument : instruments) {
        if (instrumentService->removeInstrumentPermanently(instrument.id)) {
            successCount++;
        }
    }

    // 重置自增ID
    instrumentService->resetAutoIncrement();

    showInfo("清除", QString("成功删除 %1 个器械").arg(successCount));
    loadInstruments();  // 刷新显示
#else
    showInfo("清除", "器械管理功能需要CTK框架支持");
#endif
}

void NavigationPageNew::on_generateThumbnailButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* instrumentService = CTKManager::instance()->getService<InstrumentManagementService>();
    if (!instrumentService) {
        showWarning("生成缩略图", "器械服务不可用");
        return;
    }

    auto instruments = instrumentService->getAllInstruments();
    if (instruments.isEmpty()) {
        showInfo("生成缩略图", "器械库为空，请先导入器械");
        return;
    }

    int successCount = 0;
    int failCount = 0;

    for (const auto& instrument : instruments) {
        if (instrument.modelFilePath.isEmpty()) {
            failCount++;
            continue;
        }

        QString thumbnailPath = instrumentService->generateInstrumentThumbnail(instrument.id, 200);
        if (!thumbnailPath.isEmpty()) {
            successCount++;
        } else {
            failCount++;
        }
    }

    showInfo("生成缩略图", QString("完成！成功: %1, 失败: %2").arg(successCount).arg(failCount));
    loadInstruments();  // 刷新显示
#else
    showInfo("生成缩略图", "器械管理功能需要CTK框架支持");
#endif
}

void NavigationPageNew::loadInstruments()
{
    // 清空现有器械网格
    QLayoutItem* child;
    while ((child = ui->instrumentGridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

#ifdef CTK_PLUGIN_FRAMEWORK
    auto* instrumentService = CTKManager::instance()->getService<InstrumentManagementService>();
    if (instrumentService) {
        auto instruments = instrumentService->getAllInstruments();
        int col = 0, row = 0;
        const int maxCols = 4;

        for (const auto& instrument : instruments) {
            QFrame* card = new QFrame();
            card->setStyleSheet(
                "QFrame { background-color: #1e1e2e; "
                "border-radius: 10px; border: 1px solid rgba(255,255,255,0.2); }"
                "QFrame:hover { border-color: #e94560; background-color: #252538; }"
            );
            card->setMinimumSize(180, 200);
            card->setMaximumSize(180, 200);

            // 添加点击事件支持
            card->setProperty("instrumentId", instrument.id);
            card->installEventFilter(this);
            card->setCursor(Qt::PointingHandCursor);

            QVBoxLayout* cardLayout = new QVBoxLayout(card);
            QLabel* thumbLabel = new QLabel();
            thumbLabel->setFixedSize(150, 120);
            thumbLabel->setStyleSheet("background-color: #0d0d1a; border-radius: 6px; color: #808080;");
            thumbLabel->setAlignment(Qt::AlignCenter);

            if (!instrument.thumbnailPath.isEmpty()) {
                // 将相对路径转换为绝对路径
                QString absolutePath = instrument.thumbnailPath;
                if (!QFileInfo(absolutePath).isAbsolute()) {
                    QString projectPath = instrumentService->getProjectPath();
                    absolutePath = QDir(projectPath).filePath(instrument.thumbnailPath);
                }

                QPixmap thumb(absolutePath);
                if (!thumb.isNull()) {
                    thumbLabel->setPixmap(thumb.scaled(140, 110, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                } else {
                    qWarning() << "[NavigationPage] Failed to load thumbnail:" << absolutePath;
                    thumbLabel->setText("加载失败");
                }
            } else {
                thumbLabel->setText("无预览");
            }

            QLabel* nameLabel = new QLabel(instrument.name);
            nameLabel->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: bold;");
            nameLabel->setAlignment(Qt::AlignCenter);
            nameLabel->setWordWrap(true);

            cardLayout->addWidget(thumbLabel);
            cardLayout->addWidget(nameLabel);

            ui->instrumentGridLayout->addWidget(card, row, col);

            col++;
            if (col >= maxCols) {
                col = 0;
                row++;
            }
        }
        return;
    }
#endif

    // 测试占位符（无CTK框架时显示）
    for (int i = 0; i < 4; ++i) {
        QFrame* card = new QFrame();
        card->setStyleSheet(
            "QFrame { background-color: #1e1e2e; "
            "border-radius: 10px; border: 1px solid rgba(255,255,255,0.2); }"
            "QFrame:hover { border-color: #e94560; background-color: #252538; }"
        );
        card->setMinimumSize(180, 200);
        card->setMaximumSize(180, 200);
        card->setCursor(Qt::PointingHandCursor);

        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        QLabel* thumbLabel = new QLabel();
        thumbLabel->setFixedSize(150, 120);
        thumbLabel->setStyleSheet("background-color: #0d0d1a; border-radius: 6px; color: #808080;");
        thumbLabel->setAlignment(Qt::AlignCenter);
        thumbLabel->setText(QString("器械 %1").arg(i + 1));

        QLabel* nameLabel = new QLabel(QString("工具 %1").arg(i + 1));
        nameLabel->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: bold;");
        nameLabel->setAlignment(Qt::AlignCenter);

        cardLayout->addWidget(thumbLabel);
        cardLayout->addWidget(nameLabel);

        ui->instrumentGridLayout->addWidget(card, i / 4, i % 4);
    }
}

// ========== 术前规划 ==========

void NavigationPageNew::on_loadDicomButton_clicked()
{
    QString dirPath = QFileDialog::getExistingDirectory(this, "选择DICOM目录");
    if (dirPath.isEmpty()) {
        return;
    }
    m_lastDicomDirPath = dirPath;

#ifdef CTK_PLUGIN_FRAMEWORK
    auto* dicomService = CTKManager::instance()->getService<DicomViewerService>();
    if (dicomService) {
        const int pid = m_patientId >= 0 ? m_patientId : 0;
        if (dicomService->importDicomDirectory(dirPath, pid)) {
            showInfo("加载DICOM", "DICOM数据加载成功！");
        } else {
            showError("加载DICOM", "DICOM数据加载失败。");
        }
        return;
    }
#endif

    showInfo("加载DICOM", "DICOM加载功能将通过CTK服务实现。");
}

void NavigationPageNew::on_autoSegmentButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    // 获取分割服务（ServiceHelper 已处理 CTKManager 备用获取逻辑）
    auto* segService = CTKManager::instance()->getService<SegmentationService>();
    if (!segService) {
        showError("自动分割", "分割服务不可用，请检查BoneSegmentation插件是否正确加载");
        return;
    }

    // 获取DICOM数据路径
    QString dicomPath = m_lastDicomDirPath;

    if (dicomPath.isEmpty()) {
        // 选择 NIfTI 文件或 DICOM 目录中的任意文件
        // 支持 .nii, .nii.gz 文件直接选择，或选择 .dcm 文件后自动使用其所在目录
        dicomPath = QFileDialog::getOpenFileName(this,
            "选择NIfTI文件或DICOM目录中的任意文件",
            QString(),
            "医学影像 (*.nii *.nii.gz *.dcm);;NIfTI文件 (*.nii *.nii.gz);;DICOM文件 (*.dcm);;所有文件 (*)");

        if (dicomPath.isEmpty()) {
            return;
        }

        // 如果选择的是 .dcm 文件，自动获取其所在目录作为 DICOM 目录
        if (dicomPath.endsWith(".dcm", Qt::CaseInsensitive)) {
            dicomPath = QFileInfo(dicomPath).absolutePath();
        }

        m_lastDicomDirPath = dicomPath;
    }

    // 连接分割服务信号（只连接一次）
    static bool segSignalsConnected = false;
    if (!segSignalsConnected) {
        connect(segService, SIGNAL(segmentationProgress(QString,int,QString)),
                this, SLOT(onSegmentationProgress(QString,int,QString)));
        connect(segService, SIGNAL(segmentationCompleted(QString,QVariantMap)),
                this, SLOT(onSegmentationCompleted(QString,QVariantMap)));
        connect(segService, SIGNAL(segmentationFailed(QString,QString)),
                this, SLOT(onSegmentationFailed(QString,QString)));
        segSignalsConnected = true;
    }

    // 启动分割任务
    QString taskName = QString("患者%1_骨骼分割").arg(m_patientId >= 0 ? m_patientId : 0);
    m_currentSegmentationTaskId = segService->runBoneSegmentation(dicomPath, QString(), taskName);

    if (m_currentSegmentationTaskId.isEmpty()) {
        showError("自动分割", "启动分割任务失败，请检查Python环境配置");
        return;
    }

    // 更新UI状态
    ui->autoSegmentButton->setEnabled(false);
    ui->autoSegmentButton->setText("分割中...");
    showInfo("自动分割", "分割任务已启动，请等待AI处理...");
#else
    showInfo("自动分割", "需要CTK框架支持");
#endif
}

void NavigationPageNew::on_manualSegmentButton_clicked()
{
    showInfo("手动分割", "手动分割编辑器将在此实现。");
}

void NavigationPageNew::on_selectProsthesisButton_clicked()
{
    showInfo("选择假体", "假体选择对话框将在此实现。");
}

void NavigationPageNew::on_adjustProsthesisButton_clicked()
{
    showInfo("调整假体", "假体位置调整功能将在此实现。");
}

void NavigationPageNew::on_exportSTLButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* segService = CTKManager::instance()->getService<SegmentationService>();
    if (!segService) {
        showError("导出STL", "分割服务不可用");
        return;
    }

    // 让用户选择 NIfTI 分割结果文件
    QString defaultDir = m_lastSegmentationOutputDir.isEmpty() ? m_lastDicomDirPath : m_lastSegmentationOutputDir;
    QString niftiPath = QFileDialog::getOpenFileName(this,
        "选择分割结果文件（NIfTI格式）",
        defaultDir,
        "NIfTI文件 (*.nii *.nii.gz);;所有文件 (*)");

    if (niftiPath.isEmpty()) {
        return;
    }

    // 选择保存路径
    QString baseName = QFileInfo(niftiPath).completeBaseName();
    if (baseName.endsWith(".nii")) {
        baseName = baseName.left(baseName.length() - 4);  // 去掉 .nii 后缀
    }
    QString defaultSavePath = QFileInfo(niftiPath).absolutePath() + "/" + baseName + ".stl";
    QString savePath = QFileDialog::getSaveFileName(this,
        "保存STL文件",
        defaultSavePath,
        "STL文件 (*.stl)");

    if (savePath.isEmpty()) {
        return;
    }

    // 使用服务的 convertMaskToMesh 功能转换并导出
    auto mesh = segService->convertNiftiToMeshAuto(niftiPath);
    if (mesh) {
        vtkSmartPointer<vtkSTLWriter> writer = vtkSmartPointer<vtkSTLWriter>::New();
        writer->SetFileName(savePath.toStdString().c_str());
        writer->SetInputData(mesh.GetPointer());
        writer->Write();
        showInfo("导出STL", QString("导出成功：%1").arg(savePath));
    } else {
        showError("导出STL", QString("转换失败：%1").arg(segService->getLastError()));
    }
#else
    showInfo("导出STL", "需要CTK框架支持");
#endif
}

void NavigationPageNew::on_loadModelButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    // 选择 STL 文件
    QString filePath = QFileDialog::getOpenFileName(this,
        "选择3D模型文件",
        QString(),
        "3D模型 (*.stl *.obj *.ply *.vtk);;STL文件 (*.stl);;所有文件 (*)");

    if (filePath.isEmpty()) {
        return;
    }

    bool fourViewSuccess = false;
    bool registrationSuccess = false;

    // 1. 加载到 FourViewDisplay（规划视图）
    if (m_fourViewService && m_fourViewService->loadToolModel(filePath)) {
        m_fourViewService->setToolModelVisible(true);
        m_modelVisible = true;
        ui->toggleModelButton->setText("隐藏模型");
        fourViewSuccess = true;
    }

    // 2. 同时加载到配准VTK Widget（用于取点）
    if (ensurePointRegistrationService(true)) {
        // Ensure the registration 3D view exists before loading so the service can push the model into it.
        embedRegistrationVTKWidget();
        if (m_pointRegistrationService->loadModelFromFile(filePath)) {
            registrationSuccess = true;
        }
    }

    // 3. 保存路径供后续使用
    m_lastLoadedModelPath = filePath;

    // 4. 反馈结果
    if (fourViewSuccess && registrationSuccess) {
        showInfo("加载模型", QString("模型已加载到规划视图和配准视图：%1\n\n提示：切换到\"配准\"Tab后点击模型表面即可选取配准点")
                     .arg(QFileInfo(filePath).fileName()));
    } else if (fourViewSuccess) {
        showInfo("加载模型", QString("模型加载成功：%1").arg(QFileInfo(filePath).fileName()));
    } else {
        showError("加载模型", "模型加载失败，请检查文件格式");
    }
#else
    showInfo("加载模型", "需要CTK框架支持");
#endif
}

void NavigationPageNew::on_toggleModelButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_fourViewService) {
        showError("显示模型", "四视图服务不可用");
        return;
    }

    m_modelVisible = !m_modelVisible;
    m_fourViewService->setToolModelVisible(m_modelVisible);
    ui->toggleModelButton->setText(m_modelVisible ? "隐藏模型" : "显示模型");
#else
    showInfo("显示模型", "需要CTK框架支持");
#endif
}

// ========== 配准 ==========

void NavigationPageNew::on_load2DImageButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "加载2D图像",
        QString(), "图像文件 (*.png *.jpg *.bmp *.dcm);;所有文件 (*)");

    if (!filePath.isEmpty()) {
        showInfo("加载2D图像", QString("已加载：%1").arg(filePath));
    }
}

void NavigationPageNew::on_start2D3DRegButton_clicked()
{
    showInfo("2D-3D配准", "2D-3D配准功能将在此实现。");
}

void NavigationPageNew::on_collectPointButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_registrationWorkflow) {
        setupRegistration();
    }
    if (!m_registrationWorkflow) {
        showWarning("采集点", "配准服务未就绪，请稍后再试");
        return;
    }

    // Ensure the registration 3D view exists before collecting points/loading models.
    embedRegistrationVTKWidget();

    // 检查是否已加载模型
    if (!m_pointRegistrationService->hasModel()) {
        // 提示用户先加载STL模型
        QString filePath = QFileDialog::getOpenFileName(this,
            "选择STL模型文件（用于配准取点）",
            m_lastSegmentationOutputDir.isEmpty() ? m_lastDicomDirPath : m_lastSegmentationOutputDir,
            "3D模型 (*.stl *.obj *.ply);;STL文件 (*.stl);;所有文件 (*)");

        if (filePath.isEmpty()) {
            return;
        }

        // 加载模型到配准VTK Widget
        embedRegistrationVTKWidget();
        if (!m_pointRegistrationService->loadModelFromFile(filePath)) {
            showError("加载模型", m_pointRegistrationService->getLastError());
            return;
        }
        showInfo("加载模型", QString("模型已加载，请在3D视图中点击选取配准点"));
        return;
    }

    // 使用模拟数据生成探针点
    m_registrationWorkflow->setProbeSource(ProbePointSource::Simulated);

    int generatedCount = m_registrationWorkflow->generateSimulatedProbePoints(0.5);

    if (generatedCount > 0) {
        showInfo("采集点", QString("已生成 %1 个模拟探针点（噪声: 0.5mm）").arg(generatedCount));
        updateRegistrationPointsList();
    } else {
        showWarning("采集点", "请先在3D模型上选择CT点（点击3D视图中的模型表面）");
    }
#else
    showInfo("采集点", "点采集模式已激活。");
#endif
}

void NavigationPageNew::on_computeRegButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_registrationWorkflow) {
        setupRegistration();
    }
    if (!m_registrationWorkflow) {
        showWarning("计算配准", "配准服务未就绪，请稍后再试");
        return;
    }

    if (!m_registrationWorkflow->canExecute()) {
        showWarning("计算配准", QString("有效点对不足，至少需要3对点（当前: %1）")
                                    .arg(m_registrationWorkflow->validPairCount()));
        return;
    }

    // 执行配准
    if (!m_registrationWorkflow->executeRegistration()) {
        showError("计算配准", m_pointRegistrationService ? m_pointRegistrationService->getLastError()
                                                          : "配准计算失败");
    }
    // 成功时由信号回调处理
#else
    // 模拟配准计算
    ui->regErrorLabel->setText("1.23 mm");
    showInfo("计算配准", "配准计算成功！");
#endif
}

void NavigationPageNew::on_calibrateButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_registrationWorkflow) {
        setupRegistration();
    }
    if (!m_registrationWorkflow) {
        showWarning("校准", "配准服务未就绪，请稍后再试");
        return;
    }

    // 应用配准结果到导航
    if (m_registrationWorkflow->applyToNavigation()) {
        showInfo("应用配准", "配准结果已应用到导航系统！");
    } else {
        showWarning("应用配准", "请先完成配准计算");
    }
#else
    showInfo("校准", "光学校准功能将在此实现。");
#endif
}

// ========== 导航控制 ==========

void NavigationPageNew::on_connectTrackerButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    // Tracking service integration not wired yet; fall back to simulation.
    m_trackingService = nullptr;
#endif

    // 模拟连接
    updateTrackerStatus(true);
    showInfo("追踪器", "追踪器已连接（模拟）。");
}

void NavigationPageNew::on_disconnectTrackerButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    m_trackingService = nullptr;
#endif

    m_trackerTimer->stop();
    updateTrackerStatus(false);
}

void NavigationPageNew::on_startNavigationButton_clicked()
{
    if (!m_trackerConnected) {
        showWarning("导航", "请先连接追踪器。");
        return;
    }

#ifdef CTK_PLUGIN_FRAMEWORK
    // 检查是否完成配准
    if (!m_pointRegistrationService) {
        showWarning("导航", "配准服务不可用，请先完成配准。");
        return;
    }

    // 获取配准变换矩阵
    m_registrationTransform = m_pointRegistrationService->getTransformMatrix();
    if (m_registrationTransform.isIdentity()) {
        showWarning("导航", "尚未完成配准，请先在配准Tab中完成点配准。");
        return;
    }
#endif

    // 加载骨骼模型到导航3D视图
    if (m_navigation3DView && !m_lastLoadedModelPath.isEmpty()) {
        m_navigation3DView->loadBoneModel(m_lastLoadedModelPath);
    } else if (m_navigation3DView) {
        // 如果没有预加载的模型，提示用户选择
        QString modelPath = QFileDialog::getOpenFileName(this,
            "选择骨骼模型用于导航显示",
            m_lastSegmentationOutputDir.isEmpty() ? QString() : m_lastSegmentationOutputDir,
            "3D模型 (*.stl *.obj);;STL文件 (*.stl);;所有文件 (*)");
        if (!modelPath.isEmpty()) {
            m_lastLoadedModelPath = modelPath;
            m_navigation3DView->loadBoneModel(modelPath);
        }
    }

    // 将导航3D视图嵌入到导航Tab（如果还没有嵌入）
    if (m_navigation3DView && ui->fourViewLayout) {
        // 检查是否已经在布局中
        bool alreadyInLayout = false;
        for (int i = 0; i < ui->fourViewLayout->count(); ++i) {
            if (ui->fourViewLayout->itemAt(i)->widget() == m_navigation3DView) {
                alreadyInLayout = true;
                break;
            }
        }

        if (!alreadyInLayout) {
            // 隐藏四视图Widget（如果有的话），显示导航3D视图
            if (m_fourViewWidget) {
                m_fourViewWidget->hide();
            }
            ui->fourViewLayout->addWidget(m_navigation3DView, 0, 0, 2, 2);
        }
        m_navigation3DView->show();
    }

    m_navigationActive = true;
    ui->startNavigationButton->setEnabled(false);
    ui->pauseNavigationButton->setEnabled(true);

    // 重置并启动模拟器
    if (m_motionSimulator) {
        m_motionSimulator->reset();
    }

    // 启动导航定时器
    if (m_navigationTimer) {
        m_navigationTimer->start();
    }

    // 启动跟踪器数据更新（旧的定时器，保留兼容）
    m_trackerTimer->start();

    showInfo("导航", "实时导航已开始。探针位置将实时显示在3D视图中。");
}

void NavigationPageNew::on_pauseNavigationButton_clicked()
{
    m_navigationActive = false;
    m_trackerTimer->stop();

    // 停止导航定时器
    if (m_navigationTimer) {
        m_navigationTimer->stop();
    }

    // 暂停模拟器
    if (m_motionSimulator) {
        m_motionSimulator->setPaused(true);
    }

    // 隐藏探针
    if (m_navigation3DView) {
        m_navigation3DView->setProbeVisible(false);
    }

    ui->startNavigationButton->setEnabled(true);
    ui->pauseNavigationButton->setEnabled(false);
}

void NavigationPageNew::on_resetViewButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (m_fourViewService) {
        m_fourViewService->resetViews();
        return;
    }
#endif
    showInfo("重置视图", "视图已重置为默认状态。");
}

void NavigationPageNew::onTrackerDataReceived()
{
    if (!m_navigationActive) {
        return;
    }

    // 模拟数据更新
    static double simX = 0, simY = 0, simZ = 0;
    simX += 0.1;
    simY = 10.0 * sin(simX * 0.1);
    simZ = 5.0 * cos(simX * 0.1);
    updatePositionDisplay(simX, simY, simZ);
    updateAccuracyDisplay(0.5 + 0.3 * sin(simX * 0.2));
}

void NavigationPageNew::setupVTKViews()
{
    // 如果已经嵌入了VTK Widget，直接返回
    if (m_fourViewWidget) {
        updateFourViewWidgetPlacement();
        return;
    }

    embedFourViewWidget();
}

void NavigationPageNew::embedFourViewWidget()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    m_fourViewService = CTKManager::instance()->getService<FourViewDisplayService>();
    if (!m_fourViewService) {
        qWarning() << "[NavigationPage] FourViewDisplayService not available";
        return;
    }

    if (!m_fourViewWidget) {
        // 使用服务创建纯VTK四视图Widget
        m_fourViewWidget = m_fourViewService->createFourViewVTKWidget(this);
        if (!m_fourViewWidget) {
            qWarning() << "[NavigationPage] Failed to create FourView widget";
            return;
        }
    }

    updateFourViewWidgetPlacement();
#endif
}

void NavigationPageNew::updateFourViewWidgetPlacement()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!ui || !m_fourViewWidget) {
        return;
    }

    // 先从可能的布局中移除（避免一个widget同时被多个layout管理）
    if (ui->fourViewLayout) {
        ui->fourViewLayout->removeWidget(m_fourViewWidget);
    }
    if (ui->planningViewLayout) {
        ui->planningViewLayout->removeWidget(m_fourViewWidget);
    }

    QWidget* currentTab = ui->tabWidget ? ui->tabWidget->currentWidget() : nullptr;

    // 规划Tab：替换“3D规划视图”占位控件
    if (currentTab == ui->planningTab && ui->planningViewLayout) {
        if (ui->planningViewPlaceholder) {
            ui->planningViewPlaceholder->hide();
        }
        ui->planningViewLayout->addWidget(m_fourViewWidget);
        m_fourViewWidget->show();
        qDebug() << "[NavigationPage] FourView VTK widget embedded in planning view";
    }
    // 导航Tab：嵌入四视图区域
    else if (currentTab == ui->navigationTab && ui->fourViewLayout) {
        // 清空现有的fourViewLayout内容（隐藏占位Frame/Label）
        QLayoutItem* child;
        while ((child = ui->fourViewLayout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                child->widget()->hide();
            }
        }
        ui->fourViewLayout->addWidget(m_fourViewWidget, 0, 0, 2, 2);
        m_fourViewWidget->show();
        qDebug() << "[NavigationPage] FourView VTK widget embedded in navigation view";
    }
    // 其他Tab：默认放到规划区（隐藏状态下也不影响）
    else if (ui->planningViewLayout) {
        if (ui->planningViewPlaceholder) {
            ui->planningViewPlaceholder->hide();
        }
        ui->planningViewLayout->addWidget(m_fourViewWidget);
        m_fourViewWidget->hide();
    }

    if (m_fourViewService) {
        m_fourViewService->resumeRendering();
    }
#endif
}

void NavigationPageNew::cleanupVTKViews()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (m_fourViewWidget) {
        m_fourViewWidget->hide();
        // 不删除Widget，由服务管理
        m_fourViewWidget = nullptr;
    }
#endif
}

void NavigationPageNew::updateTrackerStatus(bool connected)
{
    m_trackerConnected = connected;

    if (connected) {
        ui->trackerStatusLabel->setText("已连接");
        ui->trackerStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
        ui->connectTrackerButton->setEnabled(false);
        ui->disconnectTrackerButton->setEnabled(true);
    } else {
        ui->trackerStatusLabel->setText("未连接");
        ui->trackerStatusLabel->setStyleSheet("color: #c0392b; font-weight: bold;");
        ui->connectTrackerButton->setEnabled(true);
        ui->disconnectTrackerButton->setEnabled(false);

        m_navigationActive = false;
        ui->startNavigationButton->setEnabled(true);
        ui->pauseNavigationButton->setEnabled(false);
    }
}

void NavigationPageNew::updatePositionDisplay(double x, double y, double z)
{
    ui->xValueLabel->setText(QString("%1 mm").arg(x, 0, 'f', 2));
    ui->yValueLabel->setText(QString("%1 mm").arg(y, 0, 'f', 2));
    ui->zValueLabel->setText(QString("%1 mm").arg(z, 0, 'f', 2));
}

void NavigationPageNew::updateAccuracyDisplay(double accuracy)
{
    ui->accuracyValueLabel->setText(QString("%1 mm").arg(accuracy, 0, 'f', 2));

    // 根据精度设置颜色
    if (accuracy <= 1.0) {
        ui->accuracyValueLabel->setStyleSheet("color: #27ae60; font-weight: bold; font-size: 16px;");
    } else if (accuracy <= 2.0) {
        ui->accuracyValueLabel->setStyleSheet("color: #f39c12; font-weight: bold; font-size: 16px;");
    } else {
        ui->accuracyValueLabel->setStyleSheet("color: #e74c3c; font-weight: bold; font-size: 16px;");
    }

    // 更新进度条（假设5mm是最大误差）
    int progress = qBound(0, static_cast<int>((5.0 - accuracy) / 5.0 * 100), 100);
    ui->accuracyBar->setValue(progress);
}

// ========== 分割任务回调 ==========

void NavigationPageNew::onSegmentationProgress(const QString& taskId, int progress, const QString& message)
{
    // 只处理当前任务的进度
    if (taskId != m_currentSegmentationTaskId) {
        return;
    }

    qDebug() << "[NavigationPage] Segmentation progress:" << progress << "%" << message;

    // 更新按钮文字显示进度
    ui->autoSegmentButton->setText(QString("分割中... %1%").arg(progress));
}

void NavigationPageNew::onSegmentationCompleted(const QString& taskId, const QVariantMap& result)
{
    // 只处理当前任务
    if (taskId != m_currentSegmentationTaskId) {
        return;
    }

    qDebug() << "[NavigationPage] Segmentation completed:" << taskId;

    // 恢复按钮状态
    ui->autoSegmentButton->setEnabled(true);
    ui->autoSegmentButton->setText("自动分割");
    m_currentSegmentationTaskId.clear();

    // 获取结果文件列表
    const QString outputDir = result.value("outputDir").toString();

    // 保存分割结果信息，供导出STL功能使用
    m_lastSegmentationTaskId = taskId;
    m_lastSegmentationOutputDir = outputDir;

    // 启用导出STL按钮
    ui->exportSTLButton->setEnabled(true);

    showInfo("自动分割", outputDir.isEmpty()
                             ? QString("分割完成！")
                             : QString("分割完成！\n输出目录：%1").arg(outputDir));

#ifdef CTK_PLUGIN_FRAMEWORK
    // 尝试导出并显示分割结果
    auto* segService = CTKManager::instance()->getService<SegmentationService>();
    if (m_fourViewService && segService && !outputDir.isEmpty()) {
        const QString stlPath = QDir(outputDir).filePath("segmentation_mesh.stl");
        if (segService->exportSegmentation(taskId, stlPath, "stl")
            && m_fourViewService->loadToolModel(stlPath)) {
            m_fourViewService->setToolModelVisible(true);
            m_modelVisible = true;
            ui->toggleModelButton->setText("隐藏模型");
            qDebug() << "[NavigationPage] Loaded segmentation mesh STL:" << stlPath;
        } else {
            qWarning() << "[NavigationPage] Failed to export/load segmentation STL:" << stlPath;
        }
    }
#endif
}

void NavigationPageNew::onSegmentationFailed(const QString& taskId, const QString& error)
{
    // 只处理当前任务
    if (taskId != m_currentSegmentationTaskId) {
        return;
    }

    qWarning() << "[NavigationPage] Segmentation failed:" << taskId << error;

    // 恢复按钮状态
    ui->autoSegmentButton->setEnabled(true);
    ui->autoSegmentButton->setText("自动分割");
    m_currentSegmentationTaskId.clear();

    showError("自动分割", QString("分割失败：%1").arg(error));
}

// ========== 配准功能实现 ==========

#ifdef CTK_PLUGIN_FRAMEWORK
bool NavigationPageNew::ensurePointRegistrationService(bool tryStartPlugin)
{
    if (m_pointRegistrationService) {
        return true;
    }

    CTKManager* ctk = CTKManager::instance();
    if (!ctk || !ctk->isCTKAvailable()) {
        if (ui && ui->registrationViewPlaceholder) {
            ui->registrationViewPlaceholder->setText(QStringLiteral("CTK 框架未就绪，配准服务尚不可用"));
        }
        return false;
    }

    if (tryStartPlugin && !ctk->isPluginStarted(QStringLiteral("PointRegistration"))) {
        ctk->startPlugin(QStringLiteral("PointRegistration"));
    }

    m_pointRegistrationService = ctk->getService<PointRegistrationService>();
    if (!m_pointRegistrationService) {
        const QString state = ctk->getPluginState(QStringLiteral("PointRegistration"));
        qWarning() << "[NavigationPage] PointRegistrationService not available (plugin state:" << state << ")";
        if (ui && ui->registrationViewPlaceholder) {
            ui->registrationViewPlaceholder->setText(
                QStringLiteral("配准服务未就绪（PointRegistration: %1）").arg(state));
        }
        return false;
    }

    return true;
}
#endif

void NavigationPageNew::setupRegistration()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    // Idempotent: if already initialized, just ensure the view is embedded.
    if (m_registrationWorkflow) {
        ensurePointRegistrationService(true);
        embedRegistrationVTKWidget();
        return;
    }

    if (!ensurePointRegistrationService(true)) {
        return;
    }

    // 创建配准工作流
    m_registrationWorkflow = new RegistrationWorkflow(m_pointRegistrationService, this);

    // 连接工作流信号
    connect(m_registrationWorkflow, &RegistrationWorkflow::stateChanged,
            this, &NavigationPageNew::onRegistrationStateChanged);
    connect(m_registrationWorkflow, &RegistrationWorkflow::progressUpdated,
            this, &NavigationPageNew::onRegistrationProgressUpdated);
    connect(m_registrationWorkflow, &RegistrationWorkflow::modelLoaded,
            this, &NavigationPageNew::onRegistrationModelLoaded);
    connect(m_registrationWorkflow, &RegistrationWorkflow::ctPointAdded,
            this, &NavigationPageNew::onRegistrationPointAdded);
    connect(m_registrationWorkflow, &RegistrationWorkflow::probePointCaptured,
            this, &NavigationPageNew::onRegistrationProbePointCaptured);
    connect(m_registrationWorkflow, &RegistrationWorkflow::registrationCompleted,
            this, &NavigationPageNew::onRegistrationCompleted);
    connect(m_registrationWorkflow, &RegistrationWorkflow::registrationFailed,
            this, &NavigationPageNew::onRegistrationFailed);
    connect(m_registrationWorkflow, &RegistrationWorkflow::errorOccurred,
            this, [this](const QString& error) {
                showError("配准错误", error);
            });

    // 连接Service的点更新信号以刷新UI和3D显示
    connect(m_pointRegistrationService, &PointRegistrationService::pointAdded,
            this, [this](int, const QString&) {
                updateRegistrationPointsList();
                if (m_registrationVTKWidget) {
                    QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
                }
            });
    connect(m_pointRegistrationService, &PointRegistrationService::pointRemoved,
            this, [this](int) {
                updateRegistrationPointsList();
                if (m_registrationVTKWidget) {
                    QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
                }
            });
    connect(m_pointRegistrationService, &PointRegistrationService::pointsCleared,
            this, [this]() {
                updateRegistrationPointsList();
                if (m_registrationVTKWidget) {
                    QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
                }
            });
    connect(m_pointRegistrationService, &PointRegistrationService::pointUpdated,
            this, [this](int) {
                updateRegistrationPointsList();
                if (m_registrationVTKWidget) {
                    QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
                }
            });

    // 启动新会话
    QString patientIdStr = m_patientId >= 0 ? QString::number(m_patientId) : "";
    m_registrationWorkflow->startNewSession(patientIdStr);

    // 立即嵌入配准VTK Widget（现在插件已改为deferred加载，服务应该可用）
    embedRegistrationVTKWidget();

    // 如果用户在规划页面先加载了模型，但当时配准服务/视图未就绪，则此处补加载
    if (!m_lastLoadedModelPath.isEmpty() && !m_pointRegistrationService->hasModel()) {
        m_pointRegistrationService->loadModelFromFile(m_lastLoadedModelPath);
    }

    qDebug() << "[NavigationPage] Registration workflow initialized";
#endif
}

void NavigationPageNew::embedRegistrationVTKWidget()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (m_registrationVTKWidget) {
        return;
    }

    // 创建配准VTK Widget
    if (!ensurePointRegistrationService(true)) {
        return;
    }

    QWidget* viewParent = ui && ui->registrationViewFrame ? static_cast<QWidget*>(ui->registrationViewFrame) : this;
    m_registrationVTKWidget = m_pointRegistrationService->createVTKWidget(viewParent);
    if (!m_registrationVTKWidget) {
        qWarning() << "[NavigationPage] Failed to create registration VTK widget";
        return;
    }

    m_registrationVTKWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 连接点击信号
    connect(m_registrationVTKWidget, SIGNAL(pointPicked(double,double,double)),
            this, SLOT(onRegistrationPointPicked(double,double,double)));

    // 嵌入到配准Tab的布局中
    if (ui->registrationViewLayout) {
        ui->registrationViewLayout->setContentsMargins(0, 0, 0, 0);
        ui->registrationViewLayout->setSpacing(0);
        // 隐藏占位符
        if (ui->registrationViewPlaceholder) {
            ui->registrationViewPlaceholder->hide();
        }
        // 添加VTK Widget
        ui->registrationViewLayout->addWidget(m_registrationVTKWidget, /*stretch*/ 1);
        m_registrationVTKWidget->show();
        qDebug() << "[NavigationPage] Registration VTK widget embedded in registration view";
    } else if (ui && ui->registrationViewFrame) {
        // Fallback: ensure there is a layout on the frame.
        auto* layout = new QVBoxLayout(ui->registrationViewFrame);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        if (ui->registrationViewPlaceholder) {
            ui->registrationViewPlaceholder->hide();
        }
        layout->addWidget(m_registrationVTKWidget);
        m_registrationVTKWidget->show();
    }

    qDebug() << "[NavigationPage] Registration VTK widget embedded";
#endif
}

void NavigationPageNew::updateRegistrationPointsList()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_pointRegistrationService || !ui->registrationPointsTable) return;

    auto points = m_pointRegistrationService->getAllPoints();
    qDebug() << "[NavigationPage] Registration points count:" << points.size();

    ui->registrationPointsTable->setRowCount(points.size());

    for (int i = 0; i < points.size(); ++i) {
        const auto& pt = points[i];

        // 序号
        QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        ui->registrationPointsTable->setItem(i, 0, indexItem);

        // 名称
        ui->registrationPointsTable->setItem(i, 1, new QTableWidgetItem(pt.name));

        // 坐标
        if (pt.hasSource) {
            QTableWidgetItem* xItem = new QTableWidgetItem(QString::number(pt.sourcePosition.x(), 'f', 1));
            QTableWidgetItem* yItem = new QTableWidgetItem(QString::number(pt.sourcePosition.y(), 'f', 1));
            QTableWidgetItem* zItem = new QTableWidgetItem(QString::number(pt.sourcePosition.z(), 'f', 1));
            xItem->setTextAlignment(Qt::AlignCenter);
            yItem->setTextAlignment(Qt::AlignCenter);
            zItem->setTextAlignment(Qt::AlignCenter);
            ui->registrationPointsTable->setItem(i, 2, xItem);
            ui->registrationPointsTable->setItem(i, 3, yItem);
            ui->registrationPointsTable->setItem(i, 4, zItem);
        } else {
            ui->registrationPointsTable->setItem(i, 2, new QTableWidgetItem("-"));
            ui->registrationPointsTable->setItem(i, 3, new QTableWidgetItem("-"));
            ui->registrationPointsTable->setItem(i, 4, new QTableWidgetItem("-"));
        }

        // 状态
        QString status;
        QColor statusColor;
        if (pt.hasSource && pt.hasTarget) {
            status = "完成";
            statusColor = QColor(39, 174, 96);  // 绿色
        } else if (pt.hasSource) {
            status = "CT点";
            statusColor = QColor(52, 152, 219);  // 蓝色
        } else {
            status = "待采集";
            statusColor = QColor(149, 165, 166);  // 灰色
        }
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(statusColor);
        ui->registrationPointsTable->setItem(i, 5, statusItem);
    }

    // 调整列宽
    ui->registrationPointsTable->resizeColumnsToContents();
    ui->registrationPointsTable->horizontalHeader()->setStretchLastSection(true);
#endif
}

void NavigationPageNew::on_deletePointButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_pointRegistrationService || !ui->registrationPointsTable) return;

    int row = ui->registrationPointsTable->currentRow();
    if (row < 0) {
        showWarning("删除点", "请先在列表中选择要删除的点");
        return;
    }

    m_pointRegistrationService->removePoint(row);
    updateRegistrationPointsList();

    // 刷新3D视图
    if (m_registrationVTKWidget) {
        QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
    }

    qDebug() << "[NavigationPage] Deleted point at index:" << row;
#endif
}

void NavigationPageNew::on_clearAllPointsButton_clicked()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_pointRegistrationService) return;

    if (m_pointRegistrationService->pointCount() == 0) {
        showInfo("清空点", "当前没有配准点");
        return;
    }

    if (!showConfirm("清空点", "确定要清空所有配准点吗？此操作不可撤销。")) {
        return;
    }

    m_pointRegistrationService->clearPoints();
    updateRegistrationPointsList();

    // 刷新3D视图
    if (m_registrationVTKWidget) {
        QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
    }

    qDebug() << "[NavigationPage] Cleared all registration points";
#endif
}

void NavigationPageNew::updateRegistrationResultDisplay(const PointRegistrationResult& result)
{
    if (!result.success) {
        ui->regErrorLabel->setText("--");
        return;
    }

    // 更新配准误差显示
    ui->regErrorLabel->setText(QString("%1 mm").arg(result.rmsError, 0, 'f', 2));

    // 根据误差设置颜色
    QString color;
    if (result.rmsError < 1.0) {
        color = "#27ae60";  // 绿色 - 优秀
    } else if (result.rmsError < 2.0) {
        color = "#f39c12";  // 黄色 - 良好
    } else {
        color = "#e74c3c";  // 红色 - 较差
    }
    ui->regErrorLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));
}

// ========== 配准工作流回调 ==========

void NavigationPageNew::onRegistrationStateChanged(RegistrationSessionState state)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    qDebug() << "[NavigationPage] Registration state changed:" << sessionStateToString(state);

    // 根据状态更新UI
    switch (state) {
        case RegistrationSessionState::Idle:
            ui->collectPointButton->setEnabled(true);
            ui->computeRegButton->setEnabled(false);
            break;
        case RegistrationSessionState::PointCollection:
            ui->collectPointButton->setEnabled(true);
            ui->computeRegButton->setEnabled(true);
            break;
        case RegistrationSessionState::Computing:
            ui->collectPointButton->setEnabled(false);
            ui->computeRegButton->setEnabled(false);
            break;
        case RegistrationSessionState::Completed:
            ui->collectPointButton->setEnabled(true);
            ui->computeRegButton->setEnabled(true);
            ui->calibrateButton->setEnabled(true);
            break;
        case RegistrationSessionState::Failed:
            ui->collectPointButton->setEnabled(true);
            ui->computeRegButton->setEnabled(true);
            break;
        default:
            break;
    }
#else
    Q_UNUSED(state);
#endif
}

void NavigationPageNew::onRegistrationProgressUpdated(int progress, const QString& message)
{
    qDebug() << "[NavigationPage] Registration progress:" << progress << "%" << message;
    // TODO: 更新进度条UI（如果有的话）
}

void NavigationPageNew::onRegistrationModelLoaded(bool success, const QString& info)
{
    if (success) {
        showInfo("加载模型", info);
        // 嵌入VTK Widget（如果还没有）
        embedRegistrationVTKWidget();
    } else {
        showError("加载模型", info);
    }
}

void NavigationPageNew::onRegistrationPointAdded(int index, const QVector3D& position)
{
    qDebug() << "[NavigationPage] CT point added:" << index << position;
    updateRegistrationPointsList();
}

void NavigationPageNew::onRegistrationProbePointCaptured(int index, const QVector3D& position)
{
    qDebug() << "[NavigationPage] Probe point captured:" << index << position;
    updateRegistrationPointsList();
}

void NavigationPageNew::onRegistrationCompleted(const PointRegistrationResult& result)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    updateRegistrationResultDisplay(result);

    // 获取质量描述
    QString qualityDesc = m_registrationWorkflow->getQualityDescription();
    QStringList suggestions = m_registrationWorkflow->getImprovementSuggestions();

    QString message = qualityDesc + "\n\n建议:\n";
    for (const auto& suggestion : suggestions) {
        message += "- " + suggestion + "\n";
    }

    showInfo("配准完成", message);
#else
    Q_UNUSED(result);
#endif
}

void NavigationPageNew::onRegistrationFailed(const QString& error)
{
    showError("配准失败", error);
    ui->regErrorLabel->setText("--");
}

void NavigationPageNew::onRegistrationPointPicked(double x, double y, double z)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_registrationWorkflow) return;

    QVector3D position(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));

    // 添加CT点
    int index = m_registrationWorkflow->addCtPoint(position);

    if (index >= 0) {
        showInfo("选点", QString("已添加CT点 P%1: (%2, %3, %4)")
                             .arg(index + 1)
                             .arg(x, 0, 'f', 1)
                             .arg(y, 0, 'f', 1)
                             .arg(z, 0, 'f', 1));

        // 立即更新点列表和3D显示
        updateRegistrationPointsList();
        if (m_registrationVTKWidget) {
            QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
        }
    }
#else
    Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(z);
#endif
}

// ========== 实时导航功能 ==========

void NavigationPageNew::onNavigationTimerUpdate()
{
    if (!m_navigationActive || !m_motionSimulator || !m_navigation3DView) {
        return;
    }

    // 1. 获取模拟探针位置（跟踪空间）
    QVector3D trackingPos = m_motionSimulator->getCurrentPosition();

    // 2. 应用配准变换矩阵，转换到骨骼空间
    QVector3D bonePos = m_registrationTransform.map(trackingPos);

    // 3. 更新导航3D视图中的探针位置
    m_navigation3DView->updateProbePosition(bonePos);

    // 4. 更新位置数值显示
    updatePositionDisplay(bonePos.x(), bonePos.y(), bonePos.z());

    // 5. 计算并更新精度显示（模拟精度值）
    // 实际应用中，这里应该计算真实的导航精度
    static double simAccuracy = 0;
    static int frameCount = 0;
    frameCount++;
    // 模拟精度在0.5-1.5mm之间波动
    simAccuracy = 0.8 + 0.3 * std::sin(frameCount * 0.05);
    updateAccuracyDisplay(simAccuracy);
}

void NavigationPageNew::onNavigation3DBoneLoaded(bool success, const QVector3D& center, const QVector3D& size)
{
    if (!success) {
        showWarning("加载模型", "骨骼模型加载失败");
        return;
    }

    qDebug() << "[NavigationPage] Bone model loaded for navigation, center:" << center << "size:" << size;

    // 根据骨骼边界框设置模拟器的椭球参数
    if (m_motionSimulator) {
        // 使用骨骼尺寸的一半作为椭球半轴（稍微放大一点，让探针在表面附近运动）
        QVector3D radii = size * 0.55f;

        // 确保半轴有合理的最小值
        radii.setX(qMax(radii.x(), 20.0f));
        radii.setY(qMax(radii.y(), 20.0f));
        radii.setZ(qMax(radii.z(), 30.0f));

        m_motionSimulator->setEllipsoidParameters(center, radii);
        qDebug() << "[NavigationPage] Motion simulator configured, center:" << center << "radii:" << radii;
    }
}
