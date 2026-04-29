#include "NavigationPage.h"
#include "ui_NavigationPage.h"

#include "Framework/Platform/UiBridge/NavigationPageServiceAccess.h"
#include "Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.h"
#include "Framework/Navigation/navigation_evaluation_service.h"
#include "Plugins/BoneSegmentation/SegmentationService.h"
#include "Plugins/DicomViewer/DicomViewerService.h"
#include "Plugins/FourViewDisplay/FourViewDisplayService.h"
#include "Plugins/InstrumentManagement/InstrumentManagementService.h"
#include "Plugins/OpticalTracking/OpticalTrackingService.h"
#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"
#include "Plugins/PointRegistration/PointRegistrationService.h"
#include "Plugins/PointRegistration/RegistrationWorkflow.h"
#include "UI/Dialogs/InstrumentPreviewDialog.h"

#include <QFileDialog>
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QDir>
#include <QFileInfo>
#include <QTabWidget>
#include <cmath>

// 瀵艰埅3D瑙嗗浘鍜屾ā鎷熷櫒
#include "UI/Widgets/Navigation3DViewWidget.h"
#include "Plugins/OpticalTracking/BoneSurfaceMotionSimulator.h"

#include <vtkSTLWriter.h>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

NavigationPageNew::NavigationPageNew(QWidget* parent, NavigationPageServiceAccess* serviceAccess)
    : BasePage(parent)
    , ui(new Ui::NavigationPage)
    , m_serviceAccess(serviceAccess)
    , m_ownedServiceAdapter(nullptr)
    , m_caseId()
    , m_patientId(-1)
    , m_workflowStage(AnkleWorkflowStage::Preparation)
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
    , m_registrationWorkflow(nullptr)
    , m_fourViewService(nullptr)
    , m_trackingService(nullptr)
    , m_pointRegistrationService(nullptr)
{
    ui->setupUi(this);
    setObjectName("NavigationPage");
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->instrumentTab), QStringLiteral("准备"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->planningTab), QStringLiteral("规划"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->registrationTab), QStringLiteral("配准"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->navigationTab), QStringLiteral("导航"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->evaluationTab), QStringLiteral("评估"));
    setWorkflowStage(AnkleWorkflowStage::Preparation);

    if (!m_serviceAccess) {
        m_ownedServiceAdapter = new LegacyNavigationPageServiceAdapter();
        m_serviceAccess = new NavigationPageServiceAccess(m_ownedServiceAdapter, this);
    }

    // 鍒涘缓瀵艰埅3D瑙嗗浘鍜岃繍鍔ㄦā鎷熷櫒
    m_navigation3DView = new Navigation3DViewWidget(this);
    m_motionSimulator = new BoneSurfaceMotionSimulator();

    // 杩炴帴楠ㄩ妯″瀷鍔犺浇瀹屾垚淇″彿
    connect(m_navigation3DView, &Navigation3DViewWidget::boneModelLoaded,
            this, &NavigationPageNew::onNavigation3DBoneLoaded);

    // 鍒涘缓瀵艰埅鏇存柊瀹氭椂鍣紙30fps锛?
    m_navigationTimer = new QTimer(this);
    m_navigationTimer->setInterval(33);  // 绾?0fps
    connect(m_navigationTimer, &QTimer::timeout,
            this, &NavigationPageNew::onNavigationTimerUpdate);

    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, [this](int) {
                updateFourViewWidgetPlacement();
                if (ui->tabWidget->currentWidget() == ui->registrationTab) {
                    setupRegistration();
                }
            });

    if (m_serviceAccess) {
        connect(m_serviceAccess, &NavigationPageServiceAccess::pointRegistrationPluginAvailable, this, [this]() {
            if (ui && ui->tabWidget && ui->tabWidget->currentWidget() == ui->registrationTab) {
                setupRegistration();
            }
        });
    }

    // 璁剧疆璺熻釜鍣ㄦ洿鏂板畾鏃跺櫒锛?0fps锛?
    m_trackerTimer->setInterval(33);
    connect(m_trackerTimer, &QTimer::timeout, this, &NavigationPageNew::onTrackerDataReceived);

    // 鍒濆鐘舵€?
    updateTrackerStatus(false);

    // 鍒濆鍖栭厤鍑嗗姛鑳?
    setupRegistration();
}

NavigationPageNew::~NavigationPageNew()
{
    m_trackerTimer->stop();
    if (m_navigationTimer) {
        m_navigationTimer->stop();
    }
    cleanupVTKViews();

    // 娓呯悊妯℃嫙鍣?
    delete m_motionSimulator;
    m_motionSimulator = nullptr;

    delete ui;
    delete m_ownedServiceAdapter;
}

void NavigationPageNew::onActivated()
{
    BasePage::onActivated();

    // 鏇存柊鎮ｈ€呬俊鎭樉绀?
    if (!m_patientName.isEmpty()) {
        if (m_caseId.isEmpty()) {
            ui->patientInfoLabel->setText(QStringLiteral("患者：%1").arg(m_patientName));
        } else {
            ui->patientInfoLabel->setText(QStringLiteral("病例：%1 | 患者：%2").arg(m_caseId, m_patientName));
        }
    }

    loadInstruments();
    setupVTKViews();
    updateFourViewWidgetPlacement();

    // 鎭㈠VTK娓叉煋
    if (m_fourViewService) {
        m_fourViewService->resumeRendering();
    }
}

void NavigationPageNew::onDeactivated()
{
    BasePage::onDeactivated();

    // 鍋滄璺熻釜鍣ㄥ畾鏃跺櫒
    m_trackerTimer->stop();

    // 鍋滄瀵艰埅瀹氭椂鍣?
    if (m_navigationTimer) {
        m_navigationTimer->stop();
    }

    // 鏆傚仠妯℃嫙鍣?
    if (m_motionSimulator) {
        m_motionSimulator->setPaused(true);
    }

    // 鍋滄瀵艰埅
    if (m_navigationActive) {
        m_navigationActive = false;
    }

    // 鏆傚仠VTK娓叉煋锛堥槻姝㈤〉闈㈠垏鎹㈡椂闂儊锛?
    if (m_fourViewService) {
        m_fourViewService->pauseRendering();
    }
}

void NavigationPageNew::setCaseContext(const QString& caseId, int patientId, const QString& patientName)
{
    m_caseId = caseId;
    m_patientId = patientId;
    m_patientName = patientName;
    ui->patientInfoLabel->setText(QStringLiteral("病例：%1 | 患者：%2").arg(m_caseId, m_patientName));
    setWorkflowStage(AnkleWorkflowStage::Preparation);
}

void NavigationPageNew::setPatientId(int patientId)
{
    m_patientId = patientId;
}

void NavigationPageNew::setPatientName(const QString& name)
{
    m_patientName = name;
    if (m_caseId.isEmpty()) {
        ui->patientInfoLabel->setText(QStringLiteral("患者：%1").arg(name));
    } else {
        ui->patientInfoLabel->setText(QStringLiteral("病例：%1 | 患者：%2").arg(m_caseId, name));
    }
}

void NavigationPageNew::beginTransitionMask()
{
    // 棰勭暀杩囨浮閬僵閽╁瓙锛屽綋鍓嶄笉鍋氬鐞?
}

void NavigationPageNew::endTransitionMask()
{
    // 棰勭暀杩囨浮閬僵閽╁瓙锛屽綋鍓嶄笉鍋氬鐞?
}

void NavigationPageNew::resetPage()
{
    m_navigationActive = false;
    m_trackerConnected = false;
    m_trackerTimer->stop();
    updateTrackerStatus(false);
    // 娓呯悊鎮ｈ€呮樉绀?
    m_caseId.clear();
    m_patientId = -1;
    m_patientName.clear();
    ui->patientInfoLabel->setText(QStringLiteral("患者：-"));
    setWorkflowStage(AnkleWorkflowStage::Preparation);
}

void NavigationPageNew::setWorkflowStage(AnkleWorkflowStage stage)
{
    m_workflowStage = stage;

    switch (stage) {
    case AnkleWorkflowStage::Preparation:
        ui->tabWidget->setCurrentWidget(ui->instrumentTab);
        break;
    case AnkleWorkflowStage::Planning:
        ui->tabWidget->setCurrentWidget(ui->planningTab);
        break;
    case AnkleWorkflowStage::Registration:
        ui->tabWidget->setCurrentWidget(ui->registrationTab);
        break;
    case AnkleWorkflowStage::Navigation:
        ui->tabWidget->setCurrentWidget(ui->navigationTab);
        break;
    case AnkleWorkflowStage::Evaluation:
        ui->tabWidget->setCurrentWidget(ui->evaluationTab);
        break;
    }
}

QString NavigationPageNew::evaluationCasesRoot() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("cases"));
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
    auto* instrumentService = m_serviceAccess ? m_serviceAccess->instrumentManagementService() : nullptr;
    if (!instrumentService) {
        showWarning("预览", "器械服务不可用");
        return;
    }

    InstrumentItem instrument = instrumentService->getInstrument(instrumentId);
    if (!instrument.isValid()) {
        showWarning("棰勮", "鏃犳硶鑾峰彇鍣ㄦ淇℃伅");
        return;
    }

    // 鍒涘缓骞舵樉绀洪瑙堝璇濇
    InstrumentPreviewDialog* dialog = new InstrumentPreviewDialog(instrumentService, this);
    dialog->setPreviewContent(instrument.name, instrument.modelFilePath, instrument.id);
    dialog->exec();
    delete dialog;

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

    emit backToMainRequested();  // MainInterfaceWidget鏈熸湜鐨勪俊鍙?
    emit navigateTo(toInt(PageIndex::Dashboard));
}

// ========== 鍣ㄦ绠＄悊 ==========

void NavigationPageNew::on_importInstrumentButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "导入器械模型",
        QString(), "STL文件 (*.stl);;OBJ文件 (*.obj);;所有文件 (*)");

    if (filePath.isEmpty()) {
        return;
    }

    auto* instrumentService = m_serviceAccess ? m_serviceAccess->instrumentManagementService() : nullptr;
    if (instrumentService) {
        // The InstrumentManagementService API does not support direct file import here;
        // integrate a real importer when available.
        showInfo("导入", "器械导入服务功能尚未实现。");
    }

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
    auto* instrumentService = m_serviceAccess ? m_serviceAccess->instrumentManagementService() : nullptr;
    if (!instrumentService) {
        showWarning("清除", "器械服务不可用");
        return;
    }

    auto instruments = instrumentService->getAllInstruments();
    if (instruments.isEmpty()) {
        showInfo("娓呴櫎", "鍣ㄦ搴撳凡涓虹┖");
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

    // 閲嶇疆鑷ID
    instrumentService->resetAutoIncrement();

    showInfo("清除", QString("成功删除 %1 个器械").arg(successCount));
    loadInstruments();  // 鍒锋柊鏄剧ず

}

void NavigationPageNew::on_generateThumbnailButton_clicked()
{
    auto* instrumentService = m_serviceAccess ? m_serviceAccess->instrumentManagementService() : nullptr;
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
    loadInstruments();  // 鍒锋柊鏄剧ず

}

void NavigationPageNew::loadInstruments()
{
    // 娓呯┖鐜版湁鍣ㄦ缃戞牸
    QLayoutItem* child;
    while ((child = ui->instrumentGridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    auto* instrumentService = m_serviceAccess ? m_serviceAccess->instrumentManagementService() : nullptr;
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

            // 娣诲姞鐐瑰嚮浜嬩欢鏀寔
            card->setProperty("instrumentId", instrument.id);
            card->installEventFilter(this);
            card->setCursor(Qt::PointingHandCursor);

            QVBoxLayout* cardLayout = new QVBoxLayout(card);
            QLabel* thumbLabel = new QLabel();
            thumbLabel->setFixedSize(150, 120);
            thumbLabel->setStyleSheet("background-color: #0d0d1a; border-radius: 6px; color: #808080;");
            thumbLabel->setAlignment(Qt::AlignCenter);

            if (!instrument.thumbnailPath.isEmpty()) {
                // 灏嗙浉瀵硅矾寰勮浆鎹负缁濆璺緞
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
                    thumbLabel->setText("鍔犺浇澶辫触");
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


    // 娴嬭瘯鍗犱綅绗︼紙鏃燙TK妗嗘灦鏃舵樉绀猴級
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
        thumbLabel->setText(QString("鍣ㄦ %1").arg(i + 1));

        QLabel* nameLabel = new QLabel(QString("宸ュ叿 %1").arg(i + 1));
        nameLabel->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: bold;");
        nameLabel->setAlignment(Qt::AlignCenter);

        cardLayout->addWidget(thumbLabel);
        cardLayout->addWidget(nameLabel);

        ui->instrumentGridLayout->addWidget(card, i / 4, i % 4);
    }
}

// ========== 鏈墠瑙勫垝 ==========

void NavigationPageNew::on_loadDicomButton_clicked()
{
    QString dirPath = QFileDialog::getExistingDirectory(this, "选择DICOM目录");
    if (dirPath.isEmpty()) {
        return;
    }
    m_lastDicomDirPath = dirPath;

    auto* dicomService = m_serviceAccess ? m_serviceAccess->dicomViewerService() : nullptr;
    if (dicomService) {
        const int pid = m_patientId >= 0 ? m_patientId : 0;
        if (dicomService->importDicomDirectory(dirPath, pid)) {
            showInfo("加载DICOM", "DICOM数据加载成功！");
        } else {
            showError("加载DICOM", "DICOM数据加载失败。");
        }
        return;
    }

    showInfo("加载DICOM", "DICOM加载功能将通过CTK服务实现。");
}

void NavigationPageNew::on_autoSegmentButton_clicked()
{
    // 鑾峰彇鍒嗗壊鏈嶅姟锛圫erviceHelper 宸插鐞?CTKManager 澶囩敤鑾峰彇閫昏緫锛?
    auto* segService = m_serviceAccess ? m_serviceAccess->segmentationService() : nullptr;
    if (!segService) {
        showError("鑷姩鍒嗗壊", "鍒嗗壊鏈嶅姟涓嶅彲鐢紝璇锋鏌oneSegmentation鎻掍欢鏄惁姝ｇ‘鍔犺浇");
        return;
    }

    // 鑾峰彇DICOM鏁版嵁璺緞
    QString dicomPath = m_lastDicomDirPath;

    if (dicomPath.isEmpty()) {
        // 閫夋嫨 NIfTI 鏂囦欢鎴?DICOM 鐩綍涓殑浠绘剰鏂囦欢
        // 鏀寔 .nii, .nii.gz 鏂囦欢鐩存帴閫夋嫨锛屾垨閫夋嫨 .dcm 鏂囦欢鍚庤嚜鍔ㄤ娇鐢ㄥ叾鎵€鍦ㄧ洰褰?
        dicomPath = QFileDialog::getOpenFileName(this,
            "閫夋嫨NIfTI鏂囦欢鎴朌ICOM鐩綍涓殑浠绘剰鏂囦欢",
            QString(),
            "鍖诲褰卞儚 (*.nii *.nii.gz *.dcm);;NIfTI鏂囦欢 (*.nii *.nii.gz);;DICOM鏂囦欢 (*.dcm);;鎵€鏈夋枃浠?(*)");

        if (dicomPath.isEmpty()) {
            return;
        }

        // 濡傛灉閫夋嫨鐨勬槸 .dcm 鏂囦欢锛岃嚜鍔ㄨ幏鍙栧叾鎵€鍦ㄧ洰褰曚綔涓?DICOM 鐩綍
        if (dicomPath.endsWith(".dcm", Qt::CaseInsensitive)) {
            dicomPath = QFileInfo(dicomPath).absolutePath();
        }

        m_lastDicomDirPath = dicomPath;
    }

    // 杩炴帴鍒嗗壊鏈嶅姟淇″彿锛堝彧杩炴帴涓€娆★級
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

    // 鍚姩鍒嗗壊浠诲姟
    QString taskName = QString("患者%1_骨骼分割").arg(m_patientId >= 0 ? m_patientId : 0);
    m_currentSegmentationTaskId = segService->runBoneSegmentation(dicomPath, QString(), taskName);

    if (m_currentSegmentationTaskId.isEmpty()) {
        showError("鑷姩鍒嗗壊", "鍚姩鍒嗗壊浠诲姟澶辫触锛岃妫€鏌ython鐜閰嶇疆");
        return;
    }

    // 鏇存柊UI鐘舵€?
    ui->autoSegmentButton->setEnabled(false);
    ui->autoSegmentButton->setText("鍒嗗壊涓?..");
    showInfo("鑷姩鍒嗗壊", "鍒嗗壊浠诲姟宸插惎鍔紝璇风瓑寰匒I澶勭悊...");

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
    auto* segService = m_serviceAccess ? m_serviceAccess->segmentationService() : nullptr;
    if (!segService) {
        showError("导出STL", "分割服务不可用");
        return;
    }

    // 璁╃敤鎴烽€夋嫨 NIfTI 鍒嗗壊缁撴灉鏂囦欢
    QString defaultDir = m_lastSegmentationOutputDir.isEmpty() ? m_lastDicomDirPath : m_lastSegmentationOutputDir;
    QString niftiPath = QFileDialog::getOpenFileName(this,
        "选择分割结果文件（NIfTI格式）",
        defaultDir,
        "NIfTI文件 (*.nii *.nii.gz);;所有文件 (*)");

    if (niftiPath.isEmpty()) {
        return;
    }

    // 閫夋嫨淇濆瓨璺緞
    QString baseName = QFileInfo(niftiPath).completeBaseName();
    if (baseName.endsWith(".nii")) {
        baseName = baseName.left(baseName.length() - 4);  // 鍘绘帀 .nii 鍚庣紑
    }
    QString defaultSavePath = QFileInfo(niftiPath).absolutePath() + "/" + baseName + ".stl";
    QString savePath = QFileDialog::getSaveFileName(this,
        "淇濆瓨STL鏂囦欢",
        defaultSavePath,
        "STL鏂囦欢 (*.stl)");

    if (savePath.isEmpty()) {
        return;
    }

    // 浣跨敤鏈嶅姟鐨?convertMaskToMesh 鍔熻兘杞崲骞跺鍑?
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

}

void NavigationPageNew::on_loadModelButton_clicked()
{
    // 閫夋嫨 STL 鏂囦欢
    QString filePath = QFileDialog::getOpenFileName(this,
        "閫夋嫨3D妯″瀷鏂囦欢",
        QString(),
        "3D妯″瀷 (*.stl *.obj *.ply *.vtk);;STL鏂囦欢 (*.stl);;鎵€鏈夋枃浠?(*)");

    if (filePath.isEmpty()) {
        return;
    }

    bool fourViewSuccess = false;
    bool registrationSuccess = false;

    // 1. 鍔犺浇鍒?FourViewDisplay锛堣鍒掕鍥撅級
    if (m_fourViewService && m_fourViewService->loadToolModel(filePath)) {
        m_fourViewService->setToolModelVisible(true);
        m_modelVisible = true;
        ui->toggleModelButton->setText("闅愯棌妯″瀷");
        fourViewSuccess = true;
    }

    // 2. 鍚屾椂鍔犺浇鍒伴厤鍑哣TK Widget锛堢敤浜庡彇鐐癸級
    if (ensurePointRegistrationService(true)) {
        // Ensure the registration 3D view exists before loading so the service can push the model into it.
        embedRegistrationVTKWidget();
        if (m_pointRegistrationService->loadModelFromFile(filePath)) {
            registrationSuccess = true;
        }
    }

    // 3. 淇濆瓨璺緞渚涘悗缁娇鐢?
    m_lastLoadedModelPath = filePath;

    // 4. 鍙嶉缁撴灉
    if (fourViewSuccess && registrationSuccess) {
        showInfo("加载模型", QString("模型已加载到规划视图和配准视图：%1\n\n提示：切换到\"配准\"Tab后点击模型表面即可选取配准点")
                     .arg(QFileInfo(filePath).fileName()));
    } else if (fourViewSuccess) {
        showInfo("加载模型", QString("模型加载成功：%1").arg(QFileInfo(filePath).fileName()));
    } else {
        showError("加载模型", "模型加载失败，请检查文件格式");
    }

}

void NavigationPageNew::on_toggleModelButton_clicked()
{
    if (!m_fourViewService) {
        showError("鏄剧ず妯″瀷", "鍥涜鍥炬湇鍔′笉鍙敤");
        return;
    }

    m_modelVisible = !m_modelVisible;
    m_fourViewService->setToolModelVisible(m_modelVisible);
    ui->toggleModelButton->setText(m_modelVisible ? "闅愯棌妯″瀷" : "鏄剧ず妯″瀷");

}

// ========== 閰嶅噯 ==========

void NavigationPageNew::on_load2DImageButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "鍔犺浇2D鍥惧儚",
        QString(), "鍥惧儚鏂囦欢 (*.png *.jpg *.bmp *.dcm);;鎵€鏈夋枃浠?(*)");

    if (!filePath.isEmpty()) {
        showInfo("鍔犺浇2D鍥惧儚", QString("宸插姞杞斤細%1").arg(filePath));
    }
}

void NavigationPageNew::on_start2D3DRegButton_clicked()
{
    showInfo("2D-3D配准", "2D-3D配准功能将在此实现。");
}

void NavigationPageNew::on_collectPointButton_clicked()
{
    if (!m_registrationWorkflow) {
        setupRegistration();
    }
    if (!m_registrationWorkflow) {
        showWarning("采集点", "配准服务未就绪，请稍后再试");
        return;
    }

    // Ensure the registration 3D view exists before collecting points/loading models.
    embedRegistrationVTKWidget();

    // 妫€鏌ユ槸鍚﹀凡鍔犺浇妯″瀷
    if (!m_pointRegistrationService->hasModel()) {
        // 鎻愮ず鐢ㄦ埛鍏堝姞杞絊TL妯″瀷
        QString filePath = QFileDialog::getOpenFileName(this,
            "閫夋嫨STL妯″瀷鏂囦欢锛堢敤浜庨厤鍑嗗彇鐐癸級",
            m_lastSegmentationOutputDir.isEmpty() ? m_lastDicomDirPath : m_lastSegmentationOutputDir,
            "3D妯″瀷 (*.stl *.obj *.ply);;STL鏂囦欢 (*.stl);;鎵€鏈夋枃浠?(*)");

        if (filePath.isEmpty()) {
            return;
        }

        // 鍔犺浇妯″瀷鍒伴厤鍑哣TK Widget
        embedRegistrationVTKWidget();
        if (!m_pointRegistrationService->loadModelFromFile(filePath)) {
            showError("鍔犺浇妯″瀷", m_pointRegistrationService->getLastError());
            return;
        }
        showInfo("加载模型", QString("模型已加载，请在3D视图中点击选取配准点"));
        return;
    }

    // 浣跨敤妯℃嫙鏁版嵁鐢熸垚鎺㈤拡鐐?
    m_registrationWorkflow->setProbeSource(ProbePointSource::Simulated);

    int generatedCount = m_registrationWorkflow->generateSimulatedProbePoints(0.5);

    if (generatedCount > 0) {
        showInfo("采集点", QString("已生成 %1 个模拟探针点（噪声: 0.5mm）").arg(generatedCount));
        updateRegistrationPointsList();
    } else {
        showWarning("采集点", "请先在3D模型上选择CT点（点击3D视图中的模型表面）");
    }

}

void NavigationPageNew::on_computeRegButton_clicked()
{
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

    // 鎵ц閰嶅噯
    if (!m_registrationWorkflow->executeRegistration()) {
        showError("璁＄畻閰嶅噯", m_pointRegistrationService ? m_pointRegistrationService->getLastError()
                                                          : "閰嶅噯璁＄畻澶辫触");
    }
    // 鎴愬姛鏃剁敱淇″彿鍥炶皟澶勭悊

}

void NavigationPageNew::on_calibrateButton_clicked()
{
    if (!m_registrationWorkflow) {
        setupRegistration();
    }
    if (!m_registrationWorkflow) {
        showWarning("校准", "配准服务未就绪，请稍后再试");
        return;
    }

    // 搴旂敤閰嶅噯缁撴灉鍒板鑸?
    if (m_registrationWorkflow->applyToNavigation()) {
        showInfo("应用配准", "配准结果已应用到导航系统！");
    } else {
        showWarning("应用配准", "请先完成配准计算");
    }

}

// ========== 瀵艰埅鎺у埗 ==========

void NavigationPageNew::on_connectTrackerButton_clicked()
{
    m_trackingService = m_serviceAccess ? m_serviceAccess->opticalTrackingService() : nullptr;

    // 妯℃嫙杩炴帴
    updateTrackerStatus(true);
    showInfo("追踪器", m_trackingService ? "追踪器服务已连接。" : "追踪器已连接（模拟）。");
}

void NavigationPageNew::on_disconnectTrackerButton_clicked()
{
    m_trackingService = nullptr;


    m_trackerTimer->stop();
    updateTrackerStatus(false);
}

void NavigationPageNew::on_startNavigationButton_clicked()
{
    if (!m_trackerConnected) {
        showWarning("导航", "请先连接追踪器。");
        return;
    }

    // 妫€鏌ユ槸鍚﹀畬鎴愰厤鍑?
    if (!m_pointRegistrationService) {
        showWarning("导航", "配准服务不可用，请先完成配准。");
        return;
    }

    // 鑾峰彇閰嶅噯鍙樻崲鐭╅樀
    m_registrationTransform = m_pointRegistrationService->getTransformMatrix();
    if (m_registrationTransform.isIdentity()) {
        showWarning("导航", "尚未完成配准，请先在配准Tab中完成点配准。");
        return;
    }

    const PointRegistrationResult registrationResult =
        m_registrationWorkflow ? m_registrationWorkflow->getLastResult() : PointRegistrationResult();

    NavigationConfidenceInputs inputs;
    inputs.fre = registrationResult.rmsError;
    inputs.targetTre = registrationResult.targetRegionTre;
    inputs.coverageScore = registrationResult.coverageScore;
    inputs.surfaceResidual = registrationResult.metrics.value(QStringLiteral("refined_rms")).toDouble();

    QVariantMap trackingQuality;
    if (m_trackingService) {
        trackingQuality = m_trackingService->checkTrackingQuality(QString(), QString());
    }
    if (trackingQuality.isEmpty()) {
        trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.4);
        trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 1.0);
    }

    inputs.trackingJitter = trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble();
    inputs.visibleFrameRatio = trackingQuality.value(QStringLiteral("visible_frame_ratio")).toDouble();

    m_lastConfidence = m_confidenceEvaluator.evaluate(inputs);
    if (!m_lastConfidence.allowNavigation) {
        const QString warningText = m_lastConfidence.recommendations.isEmpty()
            ? QStringLiteral("当前导航准入条件不足。")
            : m_lastConfidence.recommendations.join(QStringLiteral("；"));
        showWarning(QStringLiteral("导航准入"), warningText);
        return;
    }


    // 鍔犺浇楠ㄩ妯″瀷鍒板鑸?D瑙嗗浘
    if (m_navigation3DView && !m_lastLoadedModelPath.isEmpty()) {
        m_navigation3DView->loadBoneModel(m_lastLoadedModelPath);
    } else if (m_navigation3DView) {
        // 濡傛灉娌℃湁棰勫姞杞界殑妯″瀷锛屾彁绀虹敤鎴烽€夋嫨
        QString modelPath = QFileDialog::getOpenFileName(this,
            "閫夋嫨楠ㄩ妯″瀷鐢ㄤ簬瀵艰埅鏄剧ず",
            m_lastSegmentationOutputDir.isEmpty() ? QString() : m_lastSegmentationOutputDir,
            "3D妯″瀷 (*.stl *.obj);;STL鏂囦欢 (*.stl);;鎵€鏈夋枃浠?(*)");
        if (!modelPath.isEmpty()) {
            m_lastLoadedModelPath = modelPath;
            m_navigation3DView->loadBoneModel(modelPath);
        }
    }

    // 灏嗗鑸?D瑙嗗浘宓屽叆鍒板鑸猅ab锛堝鏋滆繕娌℃湁宓屽叆锛?
    if (m_navigation3DView && ui->fourViewLayout) {
        // 妫€鏌ユ槸鍚﹀凡缁忓湪甯冨眬涓?
        bool alreadyInLayout = false;
        for (int i = 0; i < ui->fourViewLayout->count(); ++i) {
            if (ui->fourViewLayout->itemAt(i)->widget() == m_navigation3DView) {
                alreadyInLayout = true;
                break;
            }
        }

        if (!alreadyInLayout) {
            // 闅愯棌鍥涜鍥網idget锛堝鏋滄湁鐨勮瘽锛夛紝鏄剧ず瀵艰埅3D瑙嗗浘
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

    // 閲嶇疆骞跺惎鍔ㄦā鎷熷櫒
    if (m_motionSimulator) {
        m_motionSimulator->reset();
    }

    // 鍚姩瀵艰埅瀹氭椂鍣?
    if (m_navigationTimer) {
        m_navigationTimer->start();
    }

    // 鍚姩璺熻釜鍣ㄦ暟鎹洿鏂帮紙鏃х殑瀹氭椂鍣紝淇濈暀鍏煎锛?
    m_trackerTimer->start();

    showInfo("导航", QStringLiteral("实时导航已开始。当前准入评分：%1")
                           .arg(m_lastConfidence.score, 0, 'f', 2));
}

void NavigationPageNew::on_pauseNavigationButton_clicked()
{
    m_navigationActive = false;
    m_trackerTimer->stop();

    // 鍋滄瀵艰埅瀹氭椂鍣?
    if (m_navigationTimer) {
        m_navigationTimer->stop();
    }

    // 鏆傚仠妯℃嫙鍣?
    if (m_motionSimulator) {
        m_motionSimulator->setPaused(true);
    }

    // 闅愯棌鎺㈤拡
    if (m_navigation3DView) {
        m_navigation3DView->setProbeVisible(false);
    }

    ui->startNavigationButton->setEnabled(true);
    ui->pauseNavigationButton->setEnabled(false);

    if (!m_caseId.isEmpty()) {
        NavigationEvaluationService evaluationService(evaluationCasesRoot());

        AnkleNavigationRunRecord run;
        run.caseId = m_caseId;
        run.navigationMode = QStringLiteral("replay");
        run.confidenceScore = m_lastConfidence.score;
        run.warnings = m_lastConfidence.recommendations;
        evaluationService.saveNavigationRun(run);

        AnkleEvaluationReport report;
        report.caseId = m_caseId;
        report.translationErrorMm = m_registrationWorkflow ? m_registrationWorkflow->getLastResult().targetRegionTre : 0.0;
        report.rotationErrorDeg = 0.0;
        report.allowNavigation = m_lastConfidence.allowNavigation;
        evaluationService.saveEvaluationReport(report);
        evaluationService.exportMetricsCsv(m_caseId);
    }
}

void NavigationPageNew::on_resetViewButton_clicked()
{
    if (m_fourViewService) {
        m_fourViewService->resetViews();
        return;
    }

    showInfo("重置视图", "视图已重置为默认状态。");
}

void NavigationPageNew::onTrackerDataReceived()
{
    if (!m_navigationActive) {
        return;
    }

    // 妯℃嫙鏁版嵁鏇存柊
    static double simX = 0, simY = 0, simZ = 0;
    simX += 0.1;
    simY = 10.0 * sin(simX * 0.1);
    simZ = 5.0 * cos(simX * 0.1);
    updatePositionDisplay(simX, simY, simZ);
    updateAccuracyDisplay(0.5 + 0.3 * sin(simX * 0.2));
}

void NavigationPageNew::setupVTKViews()
{
    // 濡傛灉宸茬粡宓屽叆浜哣TK Widget锛岀洿鎺ヨ繑鍥?
    if (m_fourViewWidget) {
        updateFourViewWidgetPlacement();
        return;
    }

    embedFourViewWidget();
}

void NavigationPageNew::embedFourViewWidget()
{
    m_fourViewService = m_serviceAccess ? m_serviceAccess->fourViewDisplayService() : nullptr;
    if (!m_fourViewService) {
        qWarning() << "[NavigationPage] FourViewDisplayService not available";
        return;
    }

    if (!m_fourViewWidget) {
        // 浣跨敤鏈嶅姟鍒涘缓绾疺TK鍥涜鍥網idget
        m_fourViewWidget = m_fourViewService->createFourViewVTKWidget(this);
        if (!m_fourViewWidget) {
            qWarning() << "[NavigationPage] Failed to create FourView widget";
            return;
        }
    }

    updateFourViewWidgetPlacement();

}

void NavigationPageNew::updateFourViewWidgetPlacement()
{
    if (!ui || !m_fourViewWidget) {
        return;
    }

    // 鍏堜粠鍙兘鐨勫竷灞€涓Щ闄わ紙閬垮厤涓€涓獁idget鍚屾椂琚涓猯ayout绠＄悊锛?
    if (ui->fourViewLayout) {
        ui->fourViewLayout->removeWidget(m_fourViewWidget);
    }
    if (ui->planningViewLayout) {
        ui->planningViewLayout->removeWidget(m_fourViewWidget);
    }

    QWidget* currentTab = ui->tabWidget ? ui->tabWidget->currentWidget() : nullptr;

    // 瑙勫垝Tab锛氭浛鎹⑩€?D瑙勫垝瑙嗗浘鈥濆崰浣嶆帶浠?
    if (currentTab == ui->planningTab && ui->planningViewLayout) {
        if (ui->planningViewPlaceholder) {
            ui->planningViewPlaceholder->hide();
        }
        ui->planningViewLayout->addWidget(m_fourViewWidget);
        m_fourViewWidget->show();
        qDebug() << "[NavigationPage] FourView VTK widget embedded in planning view";
    }
    // 瀵艰埅Tab锛氬祵鍏ュ洓瑙嗗浘鍖哄煙
    else if (currentTab == ui->navigationTab && ui->fourViewLayout) {
        // 娓呯┖鐜版湁鐨刦ourViewLayout鍐呭锛堥殣钘忓崰浣岶rame/Label锛?
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
    // 鍏朵粬Tab锛氶粯璁ゆ斁鍒拌鍒掑尯锛堥殣钘忕姸鎬佷笅涔熶笉褰卞搷锛?
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

}

void NavigationPageNew::cleanupVTKViews()
{
    if (m_fourViewWidget) {
        m_fourViewWidget->hide();
        // 涓嶅垹闄idget锛岀敱鏈嶅姟绠＄悊
        m_fourViewWidget = nullptr;
    }

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

    // 鏍规嵁绮惧害璁剧疆棰滆壊
    if (accuracy <= 1.0) {
        ui->accuracyValueLabel->setStyleSheet("color: #27ae60; font-weight: bold; font-size: 16px;");
    } else if (accuracy <= 2.0) {
        ui->accuracyValueLabel->setStyleSheet("color: #f39c12; font-weight: bold; font-size: 16px;");
    } else {
        ui->accuracyValueLabel->setStyleSheet("color: #e74c3c; font-weight: bold; font-size: 16px;");
    }

    // 鏇存柊杩涘害鏉★紙鍋囪5mm鏄渶澶ц宸級
    int progress = qBound(0, static_cast<int>((5.0 - accuracy) / 5.0 * 100), 100);
    ui->accuracyBar->setValue(progress);
}

// ========== 鍒嗗壊浠诲姟鍥炶皟 ==========

void NavigationPageNew::onSegmentationProgress(const QString& taskId, int progress, const QString& message)
{
    // 鍙鐞嗗綋鍓嶄换鍔＄殑杩涘害
    if (taskId != m_currentSegmentationTaskId) {
        return;
    }

    qDebug() << "[NavigationPage] Segmentation progress:" << progress << "%" << message;

    // 鏇存柊鎸夐挳鏂囧瓧鏄剧ず杩涘害
    ui->autoSegmentButton->setText(QString("鍒嗗壊涓?.. %1%").arg(progress));
}

void NavigationPageNew::onSegmentationCompleted(const QString& taskId, const QVariantMap& result)
{
    // 鍙鐞嗗綋鍓嶄换鍔?
    if (taskId != m_currentSegmentationTaskId) {
        return;
    }

    qDebug() << "[NavigationPage] Segmentation completed:" << taskId;

    // 鎭㈠鎸夐挳鐘舵€?
    ui->autoSegmentButton->setEnabled(true);
    ui->autoSegmentButton->setText("鑷姩鍒嗗壊");
    m_currentSegmentationTaskId.clear();

    // 鑾峰彇缁撴灉鏂囦欢鍒楄〃
    const QString outputDir = result.value("outputDir").toString();

    // 淇濆瓨鍒嗗壊缁撴灉淇℃伅锛屼緵瀵煎嚭STL鍔熻兘浣跨敤
    m_lastSegmentationTaskId = taskId;
    m_lastSegmentationOutputDir = outputDir;

    // 鍚敤瀵煎嚭STL鎸夐挳
    ui->exportSTLButton->setEnabled(true);

    showInfo("自动分割", outputDir.isEmpty()
                             ? QString("分割完成！")
                             : QString("分割完成！\n输出目录：%1").arg(outputDir));

    // 灏濊瘯瀵煎嚭骞舵樉绀哄垎鍓茬粨鏋?
    auto* segService = m_serviceAccess ? m_serviceAccess->segmentationService() : nullptr;
    if (m_fourViewService && segService && !outputDir.isEmpty()) {
        const QString stlPath = QDir(outputDir).filePath("segmentation_mesh.stl");
        if (segService->exportSegmentation(taskId, stlPath, "stl")
            && m_fourViewService->loadToolModel(stlPath)) {
            m_fourViewService->setToolModelVisible(true);
            m_modelVisible = true;
            ui->toggleModelButton->setText("闅愯棌妯″瀷");
            qDebug() << "[NavigationPage] Loaded segmentation mesh STL:" << stlPath;
        } else {
            qWarning() << "[NavigationPage] Failed to export/load segmentation STL:" << stlPath;
        }
    }

}

void NavigationPageNew::onSegmentationFailed(const QString& taskId, const QString& error)
{
    // 鍙鐞嗗綋鍓嶄换鍔?
    if (taskId != m_currentSegmentationTaskId) {
        return;
    }

    qWarning() << "[NavigationPage] Segmentation failed:" << taskId << error;

    // 鎭㈠鎸夐挳鐘舵€?
    ui->autoSegmentButton->setEnabled(true);
    ui->autoSegmentButton->setText("鑷姩鍒嗗壊");
    m_currentSegmentationTaskId.clear();

    showError("自动分割", QString("分割失败：%1").arg(error));
}

// ========== 閰嶅噯鍔熻兘瀹炵幇 ==========

bool NavigationPageNew::ensurePointRegistrationService(bool tryStartPlugin)
{
    if (m_pointRegistrationService) {
        return true;
    }

    auto* serviceAccess = m_serviceAccess;
    if (!serviceAccess || !serviceAccess->isPointRegistrationFrameworkReady()) {
        if (ui && ui->registrationViewPlaceholder) {
            ui->registrationViewPlaceholder->setText(QStringLiteral("CTK 妗嗘灦鏈氨缁紝閰嶅噯鏈嶅姟灏氫笉鍙敤"));
        }
        return false;
    }

    m_pointRegistrationService = serviceAccess->pointRegistrationService(tryStartPlugin);
    if (!m_pointRegistrationService) {
        const QString state = serviceAccess->pointRegistrationPluginState();
        qWarning() << "[NavigationPage] PointRegistrationService not available (plugin state:" << state << ")";
        if (ui && ui->registrationViewPlaceholder) {
            ui->registrationViewPlaceholder->setText(
                QStringLiteral("配准服务未就绪（PointRegistration: %1）").arg(state));
        }
        return false;
    }

    return true;
}


void NavigationPageNew::setupRegistration()
{
    // Idempotent: if already initialized, just ensure the view is embedded.
    if (m_registrationWorkflow) {
        ensurePointRegistrationService(true);
        embedRegistrationVTKWidget();
        return;
    }

    if (!ensurePointRegistrationService(true)) {
        return;
    }

    // 鍒涘缓閰嶅噯宸ヤ綔娴?
    m_registrationWorkflow = new RegistrationWorkflow(m_pointRegistrationService, this);

    // 杩炴帴宸ヤ綔娴佷俊鍙?
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
                showError("閰嶅噯閿欒", error);
            });

    // 杩炴帴Service鐨勭偣鏇存柊淇″彿浠ュ埛鏂癠I鍜?D鏄剧ず
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

    // 鍚姩鏂颁細璇?
    QString patientIdStr = m_patientId >= 0 ? QString::number(m_patientId) : "";
    m_registrationWorkflow->startNewSession(patientIdStr);

    // 绔嬪嵆宓屽叆閰嶅噯VTK Widget锛堢幇鍦ㄦ彃浠跺凡鏀逛负deferred鍔犺浇锛屾湇鍔″簲璇ュ彲鐢級
    embedRegistrationVTKWidget();

    // 濡傛灉鐢ㄦ埛鍦ㄨ鍒掗〉闈㈠厛鍔犺浇浜嗘ā鍨嬶紝浣嗗綋鏃堕厤鍑嗘湇鍔?瑙嗗浘鏈氨缁紝鍒欐澶勮ˉ鍔犺浇
    if (!m_lastLoadedModelPath.isEmpty() && !m_pointRegistrationService->hasModel()) {
        m_pointRegistrationService->loadModelFromFile(m_lastLoadedModelPath);
    }

    qDebug() << "[NavigationPage] Registration workflow initialized";

}

void NavigationPageNew::embedRegistrationVTKWidget()
{
    if (m_registrationVTKWidget) {
        return;
    }

    // 鍒涘缓閰嶅噯VTK Widget
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

    // 杩炴帴鐐瑰嚮淇″彿
    connect(m_registrationVTKWidget, SIGNAL(pointPicked(double,double,double)),
            this, SLOT(onRegistrationPointPicked(double,double,double)));

    // 宓屽叆鍒伴厤鍑員ab鐨勫竷灞€涓?
    if (ui->registrationViewLayout) {
        ui->registrationViewLayout->setContentsMargins(0, 0, 0, 0);
        ui->registrationViewLayout->setSpacing(0);
        // 闅愯棌鍗犱綅绗?
        if (ui->registrationViewPlaceholder) {
            ui->registrationViewPlaceholder->hide();
        }
        // 娣诲姞VTK Widget
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

}

void NavigationPageNew::updateRegistrationPointsList()
{
    if (!m_pointRegistrationService || !ui->registrationPointsTable) return;

    auto points = m_pointRegistrationService->getAllPoints();
    qDebug() << "[NavigationPage] Registration points count:" << points.size();

    ui->registrationPointsTable->setRowCount(points.size());

    for (int i = 0; i < points.size(); ++i) {
        const auto& pt = points[i];

        // 搴忓彿
        QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        ui->registrationPointsTable->setItem(i, 0, indexItem);

        // 鍚嶇О
        ui->registrationPointsTable->setItem(i, 1, new QTableWidgetItem(pt.name));

        // 鍧愭爣
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

        // 鐘舵€?
        QString status;
        QColor statusColor;
        if (pt.hasSource && pt.hasTarget) {
            status = "瀹屾垚";
            statusColor = QColor(39, 174, 96);  // 缁胯壊
        } else if (pt.hasSource) {
            status = "CT点";
            statusColor = QColor(52, 152, 219);  // 钃濊壊
        } else {
            status = "待采集";
            statusColor = QColor(149, 165, 166);  // 鐏拌壊
        }
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(statusColor);
        ui->registrationPointsTable->setItem(i, 5, statusItem);
    }

    // 璋冩暣鍒楀
    ui->registrationPointsTable->resizeColumnsToContents();
    ui->registrationPointsTable->horizontalHeader()->setStretchLastSection(true);

}

void NavigationPageNew::on_deletePointButton_clicked()
{
    if (!m_pointRegistrationService || !ui->registrationPointsTable) return;

    int row = ui->registrationPointsTable->currentRow();
    if (row < 0) {
        showWarning("删除点", "请先在列表中选择要删除的点");
        return;
    }

    m_pointRegistrationService->removePoint(row);
    updateRegistrationPointsList();

    // 鍒锋柊3D瑙嗗浘
    if (m_registrationVTKWidget) {
        QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
    }

    qDebug() << "[NavigationPage] Deleted point at index:" << row;

}

void NavigationPageNew::on_clearAllPointsButton_clicked()
{
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

    // 鍒锋柊3D瑙嗗浘
    if (m_registrationVTKWidget) {
        QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
    }

    qDebug() << "[NavigationPage] Cleared all registration points";

}

void NavigationPageNew::updateRegistrationResultDisplay(const PointRegistrationResult& result)
{
    if (!result.success) {
        ui->regErrorLabel->setText("--");
        return;
    }

    // 鏇存柊閰嶅噯璇樊鏄剧ず
    ui->regErrorLabel->setText(QString("%1 mm").arg(result.rmsError, 0, 'f', 2));

    // 鏍规嵁璇樊璁剧疆棰滆壊
    QString color;
    if (result.rmsError < 1.0) {
        color = "#27ae60";  // 缁胯壊 - 浼樼
    } else if (result.rmsError < 2.0) {
        color = "#f39c12";  // 榛勮壊 - 鑹ソ
    } else {
        color = "#e74c3c";  // 绾㈣壊 - 杈冨樊
    }
    ui->regErrorLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));
}

// ========== 閰嶅噯宸ヤ綔娴佸洖璋?==========

void NavigationPageNew::onRegistrationStateChanged(RegistrationSessionState state)
{
    qDebug() << "[NavigationPage] Registration state changed:" << sessionStateToString(state);

    // 鏍规嵁鐘舵€佹洿鏂癠I
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

}

void NavigationPageNew::onRegistrationProgressUpdated(int progress, const QString& message)
{
    qDebug() << "[NavigationPage] Registration progress:" << progress << "%" << message;
    // TODO: 鏇存柊杩涘害鏉I锛堝鏋滄湁鐨勮瘽锛?
}

void NavigationPageNew::onRegistrationModelLoaded(bool success, const QString& info)
{
    if (success) {
        showInfo("鍔犺浇妯″瀷", info);
        // 宓屽叆VTK Widget锛堝鏋滆繕娌℃湁锛?
        embedRegistrationVTKWidget();
    } else {
        showError("鍔犺浇妯″瀷", info);
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
    updateRegistrationResultDisplay(result);
    if (!m_registrationWorkflow) {
        return;
    }

    if (!m_caseId.isEmpty()) {
        NavigationEvaluationService evaluationService(evaluationCasesRoot());
        AnkleRegistrationRecord record;
        record.caseId = m_caseId;
        record.registrationMode = result.metrics.value(QStringLiteral("registration_mode")).toString();
        record.fre = result.rmsError;
        record.targetTre = result.targetRegionTre;
        record.coverageScore = result.coverageScore;
        record.metrics = result.metrics;
        evaluationService.saveRegistrationRecord(record);
    }

    // 鑾峰彇璐ㄩ噺鎻忚堪
    QString qualityDesc = m_registrationWorkflow->getQualityDescription();
    QStringList suggestions = m_registrationWorkflow->getImprovementSuggestions();

    QString message = qualityDesc + "\n\n建议:\n";
    for (const auto& suggestion : suggestions) {
        message += "- " + suggestion + "\n";
    }

    showInfo("配准完成", message);

}

void NavigationPageNew::onRegistrationFailed(const QString& error)
{
    showError("配准失败", error);
    ui->regErrorLabel->setText("--");
}

void NavigationPageNew::onRegistrationPointPicked(double x, double y, double z)
{
    if (!m_registrationWorkflow) return;

    QVector3D position(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));

    // 娣诲姞CT鐐?
    int index = m_registrationWorkflow->addCtPoint(position);

    if (index >= 0) {
        showInfo("选点", QString("已添加CT点 P%1: (%2, %3, %4)")
                             .arg(index + 1)
                             .arg(x, 0, 'f', 1)
                             .arg(y, 0, 'f', 1)
                             .arg(z, 0, 'f', 1));

        // 绔嬪嵆鏇存柊鐐瑰垪琛ㄥ拰3D鏄剧ず
        updateRegistrationPointsList();
        if (m_registrationVTKWidget) {
            QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
        }
    }

}

// ========== 瀹炴椂瀵艰埅鍔熻兘 ==========

void NavigationPageNew::onNavigationTimerUpdate()
{
    if (!m_navigationActive || !m_motionSimulator || !m_navigation3DView) {
        return;
    }

    // 1. 鑾峰彇妯℃嫙鎺㈤拡浣嶇疆锛堣窡韪┖闂达級
    QVector3D trackingPos = m_motionSimulator->getCurrentPosition();

    // 2. 搴旂敤閰嶅噯鍙樻崲鐭╅樀锛岃浆鎹㈠埌楠ㄩ绌洪棿
    QVector3D bonePos = m_registrationTransform.map(trackingPos);

    // 3. 鏇存柊瀵艰埅3D瑙嗗浘涓殑鎺㈤拡浣嶇疆
    m_navigation3DView->updateProbePosition(bonePos);

    // 4. 鏇存柊浣嶇疆鏁板€兼樉绀?
    updatePositionDisplay(bonePos.x(), bonePos.y(), bonePos.z());

    // 5. 璁＄畻骞舵洿鏂扮簿搴︽樉绀猴紙妯℃嫙绮惧害鍊硷級
    // 瀹為檯搴旂敤涓紝杩欓噷搴旇璁＄畻鐪熷疄鐨勫鑸簿搴?
    static double simAccuracy = 0;
    static int frameCount = 0;
    frameCount++;
    // 妯℃嫙绮惧害鍦?.5-1.5mm涔嬮棿娉㈠姩
    simAccuracy = 0.8 + 0.3 * std::sin(frameCount * 0.05);
    updateAccuracyDisplay(simAccuracy);
}

void NavigationPageNew::onNavigation3DBoneLoaded(bool success, const QVector3D& center, const QVector3D& size)
{
    if (!success) {
        showWarning("鍔犺浇妯″瀷", "楠ㄩ妯″瀷鍔犺浇澶辫触");
        return;
    }

    qDebug() << "[NavigationPage] Bone model loaded for navigation, center:" << center << "size:" << size;

    // 鏍规嵁楠ㄩ杈圭晫妗嗚缃ā鎷熷櫒鐨勬き鐞冨弬鏁?
    if (m_motionSimulator) {
        // 浣跨敤楠ㄩ灏哄鐨勪竴鍗婁綔涓烘き鐞冨崐杞达紙绋嶅井鏀惧ぇ涓€鐐癸紝璁╂帰閽堝湪琛ㄩ潰闄勮繎杩愬姩锛?
        QVector3D radii = size * 0.55f;

        // 纭繚鍗婅酱鏈夊悎鐞嗙殑鏈€灏忓€?
        radii.setX(qMax(radii.x(), 20.0f));
        radii.setY(qMax(radii.y(), 20.0f));
        radii.setZ(qMax(radii.z(), 30.0f));

        m_motionSimulator->setEllipsoidParameters(center, radii);
        qDebug() << "[NavigationPage] Motion simulator configured, center:" << center << "radii:" << radii;
    }
}

