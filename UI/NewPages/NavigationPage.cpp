#include "NavigationPage.h"
#include "ui_NavigationPage.h"

#include "Framework/Platform/UiBridge/NavigationPageServiceAccess.h"
#include "Framework/Platform/LegacyAdapters/LegacyNavigationPageServiceAdapter.h"
#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Navigation/innovation_experiment_batch_runner.h"
#include "Framework/Navigation/ankle_planning_service.h"
#include "Framework/Navigation/navigation_evaluation_service.h"
#include "Framework/VTK/embedded_vtk_view_host.h"
#include "UI/NewPages/Navigation/navigation_evaluation_controller.h"
#include "UI/NewPages/Navigation/navigation_evaluation_summary_formatter.h"
#include "UI/NewPages/Navigation/navigation_registration_snapshot_utils.h"
#include "UI/NewPages/Navigation/navigation_runtime_coordinator.h"
#include "UI/NewPages/Navigation/navigation_runtime_state.h"
#include "UI/NewPages/Navigation/navigation_service_bundle.h"
#include "UI/NewPages/Navigation/navigation_vtk_bridge.h"
#include "UI/NewPages/Navigation/navigation_workspace_application_service.h"
#include "UI/NewPages/Navigation/navigation_workspace_ui_binder.h"
#include "UI/NewPages/Navigation/navigation_workflow_coordinator.h"
#include "UI/NewPages/Navigation/navigation_workflow_context.h"
#include "UI/NewPages/Navigation/preparation_planning_controller.h"
#include "UI/NewPages/Navigation/registration_controller.h"
#include "Plugins/BoneSegmentation/SegmentationService.h"
#include "Plugins/DicomViewer/DicomViewerService.h"
#include "Plugins/FourViewDisplay/FourViewDisplayService.h"
#include "Plugins/InstrumentManagement/InstrumentManagementService.h"
#include "Plugins/OpticalTracking/OpticalTrackingService.h"
#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"
#include "Plugins/PointRegistration/PointRegistrationService.h"
#include "Plugins/PointRegistration/RegistrationWorkflow.h"
#include "UI/Dialogs/InstrumentPreviewDialog.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QTabBar>
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

namespace
{
uint qHash(AnkleWorkflowStage stage, uint seed = 0) noexcept
{
    return ::qHash(static_cast<uint>(stage), seed);
}

QVariantMap planningConstraintContextFromData(const AnklePlanningData& planning)
{
    QVariantMap context;
    if (planning.anatomicalConstraintRegions.isEmpty()) {
        return context;
    }

    QStringList regionKeys;
    QStringList regionBones;
    QStringList regionRoles;
    QString source;
    QString version;

    for (auto it = planning.anatomicalConstraintRegionMetadata.cbegin();
         it != planning.anatomicalConstraintRegionMetadata.cend();
         ++it) {
        regionKeys.append(it.key());
        if (!it.value().boneName.isEmpty()) {
            regionBones.append(it.value().boneName);
        }
        if (!it.value().regionRole.isEmpty()) {
            regionRoles.append(it.value().regionRole);
        }
        if (source.isEmpty() && !it.value().source.isEmpty()) {
            source = it.value().source;
        }
        if (version.isEmpty() && !it.value().version.isEmpty()) {
            version = it.value().version;
        }
    }

    if (regionKeys.isEmpty()) {
        regionKeys = planning.anatomicalConstraintRegions.keys();
    }

    context.insert(QStringLiteral("constraint_region_count"), planning.anatomicalConstraintRegions.size());
    context.insert(QStringLiteral("constraint_region_keys"), regionKeys.join(QStringLiteral("|")));
    context.insert(QStringLiteral("constraint_region_bones"), regionBones.join(QStringLiteral("|")));
    context.insert(QStringLiteral("constraint_region_roles"), regionRoles.join(QStringLiteral("|")));
    context.insert(QStringLiteral("constraint_region_source"), source);
    context.insert(QStringLiteral("constraint_region_version"), version);
    return context;
}

QMatrix4x4 poseMatrixFromTrackingData(const QList<double>& trackingPose)
{
    QMatrix4x4 transform;
    transform.setToIdentity();
    if (trackingPose.size() < 3) {
        return transform;
    }

    const double rx = trackingPose.size() > 3 ? trackingPose[3] : 0.0;
    const double ry = trackingPose.size() > 4 ? trackingPose[4] : 0.0;
    const double rz = trackingPose.size() > 5 ? trackingPose[5] : 0.0;
    const float cx = static_cast<float>(std::cos(rx));
    const float sx = static_cast<float>(std::sin(rx));
    const float cy = static_cast<float>(std::cos(ry));
    const float sy = static_cast<float>(std::sin(ry));
    const float cz = static_cast<float>(std::cos(rz));
    const float sz = static_cast<float>(std::sin(rz));

    transform(0, 0) = cy * cz;
    transform(0, 1) = cy * sz;
    transform(0, 2) = -sy;
    transform(1, 0) = sx * sy * cz - cx * sz;
    transform(1, 1) = sx * sy * sz + cx * cz;
    transform(1, 2) = sx * cy;
    transform(2, 0) = cx * sy * cz + sx * sz;
    transform(2, 1) = cx * sy * sz - sx * cz;
    transform(2, 2) = cx * cy;
    transform(0, 3) = static_cast<float>(trackingPose[0]);
    transform(1, 3) = static_cast<float>(trackingPose[1]);
    transform(2, 3) = static_cast<float>(trackingPose[2]);
    return transform;
}

QMatrix4x4 calibrationMatrixFromResult(const QVariantMap& calibrationResult)
{
    QMatrix4x4 transform;
    transform.setToIdentity();
    const QVariantList tipOffset = calibrationResult.value(QStringLiteral("tipOffset")).toList();
    if (tipOffset.size() >= 3) {
        transform(0, 3) = tipOffset[0].toFloat();
        transform(1, 3) = tipOffset[1].toFloat();
        transform(2, 3) = tipOffset[2].toFloat();
    }
    return transform;
}

QString resolvedCaseAssetPath(const QString& caseRoot, const QString& storedPath)
{
    if (storedPath.isEmpty()) {
        return QString();
    }

    const QString normalizedPath = QDir::fromNativeSeparators(storedPath);
    const QFileInfo pathInfo(normalizedPath);
    return pathInfo.isAbsolute() ? normalizedPath : QDir(caseRoot).filePath(normalizedPath);
}
}

NavigationPageNew::NavigationPageNew(QWidget* parent, NavigationPageServiceAccess* serviceAccess)
    : BasePage(parent)
    , ui(new Ui::NavigationPage)
    , m_serviceAccess(serviceAccess)
    , m_ownedServiceAdapter(nullptr)
    , m_trackerConnected(false)
    , m_navigationActive(false)
    , m_fourViewWidget(nullptr)
    , m_trackerTimer(new QTimer(this))
    , m_modelVisible(false)
    , m_registrationVTKWidget(nullptr)
    , m_selectedPointIndex(-1)
    , m_selectedInstrumentId(-1)
    , m_navigation3DView(nullptr)
    , m_motionSimulator(nullptr)
    , m_navigationTimer(nullptr)
    , m_registrationWorkflow(nullptr)
    , m_activeCalibrationRequiredPoints(0)
    , m_activeCalibrationCollectedPoints(0)
{
    ui->setupUi(this);
    setObjectName("NavigationPage");
    setupNavigationWorkspaceShell();
    hideLegacyPlanningActions();

    if (!m_serviceAccess) {
        m_ownedServiceAdapter = new LegacyNavigationPageServiceAdapter();
        m_serviceAccess = new NavigationPageServiceAccess(m_ownedServiceAdapter, this);
    }

    m_workflowContext = std::make_unique<NavigationWorkflowContext>();
    m_workflowContext->setCasesRoot(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("cases")));
    m_serviceBundle = std::make_unique<NavigationServiceBundle>(m_serviceAccess);
    m_runtimeState = std::make_unique<NavigationRuntimeState>();
    m_runtimeCoordinator = std::make_unique<NavigationRuntimeCoordinator>(m_runtimeState.get());
    m_runtimeCoordinator->setCasesRoot(m_workflowContext->casesRoot());
    m_workspaceApplicationService = std::make_unique<NavigationWorkspaceApplicationService>(
        QFileInfo(m_workflowContext->casesRoot()).dir().absolutePath(),
        m_runtimeState.get());
    m_planningVtkHost = std::make_unique<EmbeddedVtkViewHost>(
        ui->planningViewLayout ? ui->planningViewLayout->parentWidget() : nullptr,
        ui->planningViewLayout,
        ui->planningViewPlaceholder);
    m_navigationVtkHost = std::make_unique<EmbeddedVtkViewHost>(
        ui->fourViewLayout ? ui->fourViewLayout->parentWidget() : nullptr,
        ui->fourViewLayout,
        nullptr,
        EmbeddedVtkViewHostOptions {
            .hideExistingWidgets = true,
            .gridRow = 0,
            .gridColumn = 0,
            .gridRowSpan = 2,
            .gridColumnSpan = 2
        });
    m_registrationVtkHost = std::make_unique<EmbeddedVtkViewHost>(
        ui->registrationViewFrame,
        ui->registrationViewLayout,
        ui->registrationViewPlaceholder);
    m_navigationVtkBridge = std::make_unique<NavigationVtkBridge>(
        m_planningVtkHost.get(),
        m_navigationVtkHost.get(),
        m_registrationVtkHost.get(),
        [this]() { return fourViewDisplayService(); },
        [this]() { return pointRegistrationService(); });
    m_preparationPlanningController = std::make_unique<PreparationPlanningController>(
        PreparationPlanningController::Actions {
            .loadDicom = [this]() { performLoadDicom(); }
        });
    m_registrationController = std::make_unique<RegistrationController>(
        RegistrationController::Actions {
            .computeRegistration = [this]() { performComputeRegistration(); }
        },
        m_runtimeCoordinator.get());
    m_navigationEvaluationController = std::make_unique<NavigationEvaluationController>(
        NavigationEvaluationController::Actions {
            .startNavigation = [this]() { performStartNavigation(); }
        },
        m_runtimeCoordinator.get());
    m_workspaceUiBinder = std::make_unique<NavigationWorkspaceUiBinder>(
        NavigationWorkspaceUiBinder::Bindings {
            .caseStatusLabel = m_navigationCaseStatusLabel,
            .stageSummaryLabel = m_navigationStageSummaryLabel,
            .navigationReadinessLabel = findChild<QLabel*>(QStringLiteral("navigationReadinessLabel")),
            .navigationConfidenceLabel = findChild<QLabel*>(QStringLiteral("navigationConfidenceLabel")),
            .calibrationStatusLabel = findChild<QLabel*>(QStringLiteral("calibrationStatusLabel")),
            .startNavigationButton = ui->startNavigationButton,
            .workflowRailButtons = m_workflowRailButtons,
            .toneApplier = [this](QWidget* widget, const QString& tone) { setStatusTone(widget, tone); },
            .widgetPolisher = [this](QPushButton* button) { polishNavigationWidget(button); }
        });
    m_workflowCoordinator = std::make_unique<NavigationWorkflowCoordinator>(
        m_workflowContext.get(),
        m_preparationPlanningController.get(),
        m_registrationController.get(),
        m_navigationEvaluationController.get(),
        m_runtimeCoordinator.get(),
        [this](AnkleWorkflowStage stage) { setWorkflowStage(stage); },
        m_workspaceApplicationService.get());

    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->instrumentTab), QStringLiteral("准备"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->planningTab), QStringLiteral("规划"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->registrationTab), QStringLiteral("配准"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->navigationTab), QStringLiteral("导航"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->evaluationTab), QStringLiteral("评估"));
    setWorkflowStage(AnkleWorkflowStage::Preparation);

    // 鍒涘缓瀵艰埅3D瑙嗗浘鍜岃繍鍔ㄦā鎷熷櫒
    m_navigation3DView = new Navigation3DViewWidget(this);
    m_motionSimulator = new BoneSurfaceMotionSimulator();

    // 杩炴帴楠ㄩ妯″瀷鍔犺浇瀹屾垚淇″彿
    connect(m_navigation3DView, &Navigation3DViewWidget::boneModelLoaded,
            this, &NavigationPageNew::onNavigation3DBoneLoaded);
    m_navigationVtkBridge->setNavigationViewWidget(m_navigation3DView);

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

    if (ui->opticalRegLayout) {
        auto* calibrationStatusLabel = findChild<QLabel*>(QStringLiteral("calibrationStatusLabel"));
        if (!calibrationStatusLabel) {
            calibrationStatusLabel = new QLabel(this);
            calibrationStatusLabel->setObjectName(QStringLiteral("calibrationStatusLabel"));
            calibrationStatusLabel->setWordWrap(true);
            ui->opticalRegLayout->addWidget(calibrationStatusLabel);
        }

        auto* navigationReadinessLabel = findChild<QLabel*>(QStringLiteral("navigationReadinessLabel"));
        if (!navigationReadinessLabel) {
            navigationReadinessLabel = new QLabel(this);
            navigationReadinessLabel->setObjectName(QStringLiteral("navigationReadinessLabel"));
            navigationReadinessLabel->setWordWrap(true);
            ui->opticalRegLayout->addWidget(navigationReadinessLabel);
        }

        auto* navigationConfidenceLabel = findChild<QLabel*>(QStringLiteral("navigationConfidenceLabel"));
        if (!navigationConfidenceLabel) {
            navigationConfidenceLabel = new QLabel(this);
            navigationConfidenceLabel->setObjectName(QStringLiteral("navigationConfidenceLabel"));
            navigationConfidenceLabel->setWordWrap(true);
            ui->opticalRegLayout->addWidget(navigationConfidenceLabel);
        }

        auto* captureCalibrationPointButton = findChild<QPushButton*>(QStringLiteral("captureCalibrationPointButton"));
        if (!captureCalibrationPointButton) {
            captureCalibrationPointButton = new QPushButton(QStringLiteral("采集标定点"), this);
            captureCalibrationPointButton->setObjectName(QStringLiteral("captureCalibrationPointButton"));
            ui->opticalRegLayout->addWidget(captureCalibrationPointButton);
            connect(captureCalibrationPointButton, &QPushButton::clicked, this, [this]() {
                captureProbeCalibrationPoint();
            });
        }

        auto* finishCalibrationButton = findChild<QPushButton*>(QStringLiteral("finishCalibrationButton"));
        if (!finishCalibrationButton) {
            finishCalibrationButton = new QPushButton(QStringLiteral("完成标定"), this);
            finishCalibrationButton->setObjectName(QStringLiteral("finishCalibrationButton"));
            ui->opticalRegLayout->addWidget(finishCalibrationButton);
            connect(finishCalibrationButton, &QPushButton::clicked, this, [this]() {
                finishProbeCalibration();
            });
        }

        auto* cancelCalibrationButton = findChild<QPushButton*>(QStringLiteral("cancelCalibrationButton"));
        if (!cancelCalibrationButton) {
            cancelCalibrationButton = new QPushButton(QStringLiteral("取消标定"), this);
            cancelCalibrationButton->setObjectName(QStringLiteral("cancelCalibrationButton"));
            ui->opticalRegLayout->addWidget(cancelCalibrationButton);
            connect(cancelCalibrationButton, &QPushButton::clicked, this, [this]() {
                cancelProbeCalibration();
            });
        }
    }

    resetProbeCalibrationState();
    refreshNavigationConfidenceState();
    refreshEvaluationSummary();

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

QPushButton* NavigationPageNew::createWorkflowRailButton(const QString& text, AnkleWorkflowStage stage)
{
    auto* button = new QPushButton(text, this);
    button->setCursor(Qt::PointingHandCursor);
    button->setCheckable(true);
    button->setMinimumHeight(44);
    button->setProperty("workflowState", QStringLiteral("idle"));
    connect(button, &QPushButton::clicked, this, [this, stage]() {
        setWorkflowStage(stage);
    });
    m_workflowRailButtons.insert(stage, button);
    return button;
}

QLabel* NavigationPageNew::createNavigationStatusValueLabel(const QString& objectName, const QString& initialText)
{
    auto* label = new QLabel(initialText, this);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setProperty("statusTone", QStringLiteral("warning"));
    return label;
}

QFrame* NavigationPageNew::createNavigationStatusCard(const QString& title, QWidget* valueWidget)
{
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("navigationStatusCard"));
    card->setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("navigationStatusCardTitleLabel"));
    layout->addWidget(titleLabel);
    layout->addWidget(valueWidget);

    return card;
}

void NavigationPageNew::hideLegacyPlanningActions()
{
    const QList<QWidget*> legacyWidgets = {
        ui->autoSegmentButton,
        ui->exportSTLButton,
        ui->loadModelButton,
        ui->toggleModelButton,
        ui->selectProsthesisButton,
        ui->adjustProsthesisButton,
        ui->load2DImageButton,
        ui->start2D3DRegButton,
        ui->importInstrumentButton
    };

    for (QWidget* widget : legacyWidgets) {
        if (widget) {
            widget->hide();
            widget->setEnabled(false);
        }
    }
}

void NavigationPageNew::setupNavigationWorkspaceShell()
{
    if (!ui || !ui->mainLayout || !ui->tabWidget) {
        return;
    }

    setStyleSheet(QString());
    if (ui->tabWidget->tabBar()) {
        ui->tabWidget->tabBar()->hide();
    }
    ui->tabWidget->setDocumentMode(true);

    const QList<QWidget*> legacyStyledWidgets = {
        ui->planningViewFrame,
        ui->planningViewPlaceholder,
        ui->planningControlPanel,
        ui->registrationViewFrame,
        ui->registrationViewPlaceholder,
        ui->registrationControlPanel,
        ui->registrationPointsTable,
        ui->deletePointButton,
        ui->clearAllPointsButton,
        ui->axialViewFrame,
        ui->sagittalViewFrame,
        ui->coronalViewFrame,
        ui->view3DFrame,
        ui->axialLabel,
        ui->sagittalLabel,
        ui->coronalLabel,
        ui->view3DLabel,
        ui->navigationControlPanel,
        ui->trackerStatusLabel,
        ui->connectTrackerButton,
        ui->disconnectTrackerButton,
        ui->xValueLabel,
        ui->yValueLabel,
        ui->zValueLabel,
        ui->accuracyValueLabel,
        ui->regErrorLabel
    };
    for (auto* widget : legacyStyledWidgets) {
        if (widget) {
            widget->setStyleSheet(QString());
        }
    }

    ui->backButton->setText(QStringLiteral("返回病例工作台"));
    ui->titleLabel->setText(QStringLiteral("手术导航工作区"));
    ui->patientInfoLabel->setMinimumWidth(260);
    ui->patientInfoLabel->setWordWrap(true);

    QLayoutItem* headerItem = ui->mainLayout->takeAt(0);
    QLayoutItem* tabItem = ui->mainLayout->takeAt(0);
    auto* headerLayout = headerItem ? qobject_cast<QHBoxLayout*>(headerItem->layout()) : nullptr;
    delete tabItem;

    auto* headerFrame = new QFrame(this);
    headerFrame->setObjectName(QStringLiteral("navigationHeaderFrame"));
    auto* headerFrameLayout = new QHBoxLayout(headerFrame);
    headerFrameLayout->setContentsMargins(24, 20, 24, 20);
    headerFrameLayout->setSpacing(18);

    if (headerItem) {
        headerItem->setAlignment(Qt::Alignment());
    }

    ui->patientInfoLabel->setParent(headerFrame);
    ui->patientInfoLabel->hide();

    ui->backButton->setParent(headerFrame);
    headerFrameLayout->addWidget(ui->backButton, 0, Qt::AlignTop);

    auto* titleContainer = new QWidget(headerFrame);
    auto* titleLayout = new QVBoxLayout(titleContainer);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);

    auto* eyebrowLabel = new QLabel(QStringLiteral("ANKLE NAVIGATION WORKSPACE"), titleContainer);
    eyebrowLabel->setObjectName(QStringLiteral("navigationEyebrowLabel"));
    titleLayout->addWidget(eyebrowLabel);

    ui->titleLabel->setParent(titleContainer);
    titleLayout->addWidget(ui->titleLabel);

    auto* subtitleLabel = new QLabel(
        QStringLiteral("按准备、规划、配准、导航、评估推进当前病例。"),
        titleContainer);
    subtitleLabel->setObjectName(QStringLiteral("navigationSubtitleLabel"));
    subtitleLabel->setWordWrap(true);
    titleLayout->addWidget(subtitleLabel);

    headerFrameLayout->addWidget(titleContainer, 1);

    m_navigationPatientSummaryLabel = createNavigationStatusValueLabel(
        QStringLiteral("navigationPatientSummaryLabel"),
        QStringLiteral("患者：-"));
    m_navigationPatientSummaryLabel->setParent(headerFrame);
    headerFrameLayout->addWidget(m_navigationPatientSummaryLabel, 0, Qt::AlignTop);

    auto* bodyFrame = new QFrame(this);
    bodyFrame->setObjectName(QStringLiteral("navigationBodyFrame"));
    auto* bodyLayout = new QHBoxLayout(bodyFrame);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(18);

    auto* workflowRail = new QFrame(bodyFrame);
    workflowRail->setObjectName(QStringLiteral("navigationWorkflowRailFrame"));
    workflowRail->setMinimumWidth(192);
    workflowRail->setMaximumWidth(224);
    auto* workflowLayout = new QVBoxLayout(workflowRail);
    workflowLayout->setContentsMargins(16, 18, 16, 18);
    workflowLayout->setSpacing(10);

    auto* workflowTitleLabel = new QLabel(QStringLiteral("导航流程"), workflowRail);
    workflowTitleLabel->setObjectName(QStringLiteral("navigationRailTitleLabel"));
    workflowLayout->addWidget(workflowTitleLabel);

    m_navigationStageSummaryLabel = new QLabel(QStringLiteral("当前阶段：准备"), workflowRail);
    m_navigationStageSummaryLabel->setObjectName(QStringLiteral("navigationRailSummaryLabel"));
    m_navigationStageSummaryLabel->setWordWrap(true);
    workflowLayout->addWidget(m_navigationStageSummaryLabel);
    workflowLayout->addSpacing(10);

    workflowLayout->addWidget(createWorkflowRailButton(QStringLiteral("准备"), AnkleWorkflowStage::Preparation));
    workflowLayout->addWidget(createWorkflowRailButton(QStringLiteral("规划"), AnkleWorkflowStage::Planning));
    workflowLayout->addWidget(createWorkflowRailButton(QStringLiteral("配准"), AnkleWorkflowStage::Registration));
    workflowLayout->addWidget(createWorkflowRailButton(QStringLiteral("导航"), AnkleWorkflowStage::Navigation));
    workflowLayout->addWidget(createWorkflowRailButton(QStringLiteral("评估"), AnkleWorkflowStage::Evaluation));
    workflowLayout->addStretch();

    auto* workspaceFrame = new QFrame(bodyFrame);
    workspaceFrame->setObjectName(QStringLiteral("navigationWorkspaceFrame"));
    auto* workspaceLayout = new QVBoxLayout(workspaceFrame);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);
    ui->tabWidget->setParent(workspaceFrame);
    workspaceLayout->addWidget(ui->tabWidget);

    auto* statusRail = new QFrame(bodyFrame);
    statusRail->setObjectName(QStringLiteral("navigationStatusRailFrame"));
    statusRail->setMinimumWidth(264);
    statusRail->setMaximumWidth(320);
    auto* statusLayout = new QVBoxLayout(statusRail);
    statusLayout->setContentsMargins(16, 18, 16, 18);
    statusLayout->setSpacing(12);

    auto* statusTitleLabel = new QLabel(QStringLiteral("运行状态"), statusRail);
    statusTitleLabel->setObjectName(QStringLiteral("navigationStatusTitleLabel"));
    statusLayout->addWidget(statusTitleLabel);

    m_navigationCaseStatusLabel = createNavigationStatusValueLabel(
        QStringLiteral("navigationCaseStatusLabel"),
        QStringLiteral("未选择病例"));
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("当前病例"), m_navigationCaseStatusLabel));
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("追踪器"), ui->trackerStatusLabel));
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("配准误差"), ui->regErrorLabel));

    auto* readinessLabel = findChild<QLabel*>(QStringLiteral("navigationReadinessLabel"));
    if (!readinessLabel) {
        readinessLabel = createNavigationStatusValueLabel(
            QStringLiteral("navigationReadinessLabel"),
            QStringLiteral("导航准入：未就绪"));
    }
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("导航准入"), readinessLabel));

    auto* confidenceLabel = findChild<QLabel*>(QStringLiteral("navigationConfidenceLabel"));
    if (!confidenceLabel) {
        confidenceLabel = createNavigationStatusValueLabel(
            QStringLiteral("navigationConfidenceLabel"),
            QStringLiteral("可信度评分：待评估"));
    }
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("可信度"), confidenceLabel));

    auto* calibrationLabel = findChild<QLabel*>(QStringLiteral("calibrationStatusLabel"));
    if (!calibrationLabel) {
        calibrationLabel = createNavigationStatusValueLabel(
            QStringLiteral("calibrationStatusLabel"),
            QStringLiteral("标定状态：待开始"));
    }
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("探针标定"), calibrationLabel));
    statusLayout->addWidget(createNavigationStatusCard(QStringLiteral("当前精度"), ui->accuracyValueLabel));
    statusLayout->addWidget(ui->accuracyBar);
    statusLayout->addStretch();

    bodyLayout->addWidget(workflowRail);
    bodyLayout->addWidget(workspaceFrame, 1);
    bodyLayout->addWidget(statusRail);

    ui->mainLayout->addWidget(headerFrame);
    ui->mainLayout->addWidget(bodyFrame, 1);
    ui->mainLayout->setContentsMargins(32, 26, 32, 26);
    ui->mainLayout->setSpacing(18);

    syncWorkflowRailState();
    syncNavigationStatusSummary();
}

void NavigationPageNew::polishNavigationWidget(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void NavigationPageNew::setStatusTone(QWidget* widget, const QString& tone)
{
    if (!widget) {
        return;
    }
    widget->setProperty("statusTone", tone);
    polishNavigationWidget(widget);
}

void NavigationPageNew::syncWorkflowRailState()
{
    const AnkleWorkflowStage currentStage = m_workflowContext
        ? m_workflowContext->currentStage()
        : AnkleWorkflowStage::Preparation;
    const QHash<AnkleWorkflowStage, QString> stageNames = {
        { AnkleWorkflowStage::Preparation, QStringLiteral("准备") },
        { AnkleWorkflowStage::Planning, QStringLiteral("规划") },
        { AnkleWorkflowStage::Registration, QStringLiteral("配准") },
        { AnkleWorkflowStage::Navigation, QStringLiteral("导航") },
        { AnkleWorkflowStage::Evaluation, QStringLiteral("评估") }
    };

    for (auto it = m_workflowRailButtons.begin(); it != m_workflowRailButtons.end(); ++it) {
        auto* button = it.value().data();
        if (!button) {
            continue;
        }
        const bool active = it.key() == currentStage;
        button->setChecked(active);
        button->setProperty("workflowState", active ? QStringLiteral("active") : QStringLiteral("idle"));
        polishNavigationWidget(button);
    }

    if (m_navigationStageSummaryLabel) {
        m_navigationStageSummaryLabel->setText(
            QStringLiteral("当前阶段：%1").arg(stageNames.value(currentStage)));
    }
}

void NavigationPageNew::syncNavigationStatusSummary()
{
    if (!m_workflowContext) {
        return;
    }

    const QString summary = m_workflowContext->patientSummary();
    if (ui && ui->patientInfoLabel) {
        ui->patientInfoLabel->setText(summary);
    }
    if (m_navigationPatientSummaryLabel) {
        m_navigationPatientSummaryLabel->setText(summary);
    }
}

void NavigationPageNew::onActivated()
{
    BasePage::onActivated();

    refreshPatientInfoLabel();
    refreshStageGateUi();

    loadInstruments();
    setupVTKViews();
    updateFourViewWidgetPlacement();

    if (m_navigationVtkBridge) {
        m_navigationVtkBridge->resumeFourView();
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

    if (m_navigationVtkBridge) {
        m_navigationVtkBridge->pauseFourView();
    }
}

void NavigationPageNew::setCaseContext(const QString& caseId, int patientId, const QString& patientName)
{
    m_workflowContext->setCaseIdentity(caseId, patientId, patientName);
    if (m_runtimeState) {
        m_runtimeState->setCaseContext(caseId, QString(), QString());
    }
    m_registrationTransform = QMatrix4x4();
    if (m_runtimeCoordinator) {
        m_runtimeCoordinator->clearPoseTrackingState();
        m_runtimeCoordinator->clearRegistrationTransform();
    }
    if (m_workspaceApplicationService) {
        m_workspaceApplicationService->loadWorkspace(
            caseId,
            QString::number(patientId),
            patientName);
        m_workspaceApplicationService->persistSnapshot();
    }
    refreshPatientInfoLabel();
    setWorkflowStage(AnkleWorkflowStage::Preparation);
    restoreRegistrationSnapshotState();
    restoreNavigationSnapshotState();
    updateProbeCalibrationUi();
    refreshStageGateUi();
    refreshEvaluationSummary();
}

void NavigationPageNew::setPatientId(int patientId)
{
    m_workflowContext->setPatientId(patientId);
}

void NavigationPageNew::setPatientName(const QString& name)
{
    m_workflowContext->setPatientName(name);
    refreshPatientInfoLabel();
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
    clearTrackingRuntimeState(true);
    m_navigationActive = false;
    m_trackerTimer->stop();
    updateTrackerStatus(false);
    m_selectedInstrumentId = -1;
    // 娓呯悊鎮ｈ€呮樉绀?
    m_workflowContext->clearCaseIdentity();
    refreshPatientInfoLabel();
    setWorkflowStage(AnkleWorkflowStage::Preparation);
    refreshEvaluationSummary();
}

void NavigationPageNew::setWorkflowStage(AnkleWorkflowStage stage)
{
    m_workflowContext->setCurrentStage(stage);

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
    refreshStageGateUi();
    syncWorkflowRailState();
}

QString NavigationPageNew::evaluationCasesRoot() const
{
    return m_workflowContext->casesRoot();
}

void NavigationPageNew::refreshEvaluationSummary()
{
    if (!ui || !ui->evaluationTabLayout) {
        return;
    }

    auto ensureSectionLabel = [this](const QString& groupName, const QString& labelName, const QString& title) {
        auto* label = findChild<QLabel*>(labelName);
        if (label) {
            return label;
        }

        auto* group = new QGroupBox(title, ui->evaluationTab);
        group->setObjectName(groupName);
        auto* layout = new QVBoxLayout(group);
        layout->setContentsMargins(12, 12, 12, 12);

        label = new QLabel(group);
        label->setObjectName(labelName);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(label);
        ui->evaluationTabLayout->addWidget(group);
        return label;
    };

    auto* headerLabel = findChild<QLabel*>(QStringLiteral("evaluationHeaderLabel"));
    if (!headerLabel) {
        headerLabel = new QLabel(ui->evaluationTab);
        headerLabel->setObjectName(QStringLiteral("evaluationHeaderLabel"));
        headerLabel->setWordWrap(true);
        headerLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        ui->evaluationTabLayout->insertWidget(0, headerLabel);
    }

    auto* registrationLabel = ensureSectionLabel(
        QStringLiteral("evaluationRegistrationSummaryGroup"),
        QStringLiteral("evaluationRegistrationSummaryLabel"),
        QStringLiteral("配准结果"));
    auto* constraintLabel = ensureSectionLabel(
        QStringLiteral("evaluationConstraintSummaryGroup"),
        QStringLiteral("evaluationConstraintSummaryLabel"),
        QStringLiteral("目标区与约束区"));
    auto* trackingLabel = ensureSectionLabel(
        QStringLiteral("evaluationTrackingSummaryGroup"),
        QStringLiteral("evaluationTrackingSummaryLabel"),
        QStringLiteral("跟踪质量"));
    auto* gateLabel = ensureSectionLabel(
        QStringLiteral("evaluationGateSummaryGroup"),
        QStringLiteral("evaluationGateSummaryLabel"),
        QStringLiteral("导航准入"));

    const QString caseId = m_workflowContext ? m_workflowContext->caseId() : QString();
    NavigationEvaluationSummary summary;
    if (!caseId.isEmpty()) {
        if (m_workspaceApplicationService) {
            summary = buildNavigationEvaluationSummary(m_workspaceApplicationService->currentSnapshot());
        } else {
            NavigationEvaluationService evaluationService(evaluationCasesRoot());
            summary = buildNavigationEvaluationSummary(evaluationService.loadEvaluationSnapshot(caseId));
        }
    } else {
        summary = buildNavigationEvaluationSummary(AnkleEvaluationSnapshot());
    }

    if (ui->evaluationPlaceholderLabel) {
        ui->evaluationPlaceholderLabel->setVisible(!summary.hasData);
        if (!summary.hasData) {
            ui->evaluationPlaceholderLabel->setText(QStringLiteral("当前病例暂无评估结果，完成配准与导航后将在此汇总。"));
        }
    }

    headerLabel->setText(summary.headerText);
    registrationLabel->setText(summary.registrationText);
    constraintLabel->setText(summary.constraintText);
    trackingLabel->setText(summary.trackingText);
    gateLabel->setText(summary.gateText);
}

void NavigationPageNew::refreshPatientInfoLabel()
{
    if (!ui || !ui->patientInfoLabel) {
        return;
    }

    ui->patientInfoLabel->setText(m_workflowContext->patientSummary());
    syncNavigationStatusSummary();
}

InstrumentManagementService* NavigationPageNew::instrumentManagementService() const
{
    return m_serviceBundle ? m_serviceBundle->instrumentManagementService() : nullptr;
}

DicomViewerService* NavigationPageNew::dicomViewerService() const
{
    return m_serviceBundle ? m_serviceBundle->dicomViewerService() : nullptr;
}

BoneSegmentationService* NavigationPageNew::segmentationService() const
{
    return m_serviceBundle ? m_serviceBundle->segmentationService() : nullptr;
}

FourViewDisplayService* NavigationPageNew::fourViewDisplayService() const
{
    return m_serviceBundle ? m_serviceBundle->fourViewDisplayService() : nullptr;
}

OpticalTrackingService* NavigationPageNew::opticalTrackingService() const
{
    return m_serviceBundle ? m_serviceBundle->opticalTrackingService() : nullptr;
}

PointRegistrationService* NavigationPageNew::pointRegistrationService(bool tryStartPlugin) const
{
    return m_serviceBundle ? m_serviceBundle->pointRegistrationService(tryStartPlugin) : nullptr;
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
    auto* instrumentService = instrumentManagementService();
    if (!instrumentService) {
        showWarning("预览", "器械服务不可用");
        return;
    }

    InstrumentItem instrument = instrumentService->getInstrument(instrumentId);
    if (!instrument.isValid()) {
        showWarning("棰勮", "鏃犳硶鑾峰彇鍣ㄦ淇℃伅");
        return;
    }

    m_selectedInstrumentId = instrumentId;

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
    showWarning("准备", "当前版本只允许使用病例工作包中已绑定的器械和几何文件。");
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
    auto* instrumentService = instrumentManagementService();
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
    auto* instrumentService = instrumentManagementService();
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

    auto* instrumentService = instrumentManagementService();
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
    if (m_workflowCoordinator) {
        m_workflowCoordinator->handleLoadDicom();
    }
}

void NavigationPageNew::performLoadDicom()
{
    QString dirPath = QFileDialog::getExistingDirectory(this, "选择DICOM目录");
    if (dirPath.isEmpty()) {
        return;
    }
    m_lastDicomDirPath = dirPath;

    auto* dicomService = dicomViewerService();
    if (dicomService) {
        const int pid = m_workflowContext->patientId() >= 0 ? m_workflowContext->patientId() : 0;
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
    auto* segService = segmentationService();
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
    const int patientId = m_workflowContext->patientId() >= 0 ? m_workflowContext->patientId() : 0;
    QString taskName = QString("患者%1_骨骼分割").arg(patientId);
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
    auto* segService = segmentationService();
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
    auto* fourViewService = fourViewDisplayService();

    // 1. 鍔犺浇鍒?FourViewDisplay锛堣鍒掕鍥撅級
    if (fourViewService && fourViewService->loadToolModel(filePath)) {
        fourViewService->setToolModelVisible(true);
        m_modelVisible = true;
        ui->toggleModelButton->setText("闅愯棌妯″瀷");
        fourViewSuccess = true;
    }

    // 2. 鍚屾椂鍔犺浇鍒伴厤鍑哣TK Widget锛堢敤浜庡彇鐐癸級
    if (ensurePointRegistrationService(true)) {
        // Ensure the registration 3D view exists before loading so the service can push the model into it.
        embedRegistrationVTKWidget();
        if (auto* registrationService = pointRegistrationService()) {
            if (registrationService->loadModelFromFile(filePath)) {
                registrationSuccess = true;
            }
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
    auto* fourViewService = fourViewDisplayService();
    if (!fourViewService) {
        showError("鏄剧ず妯″瀷", "鍥涜鍥炬湇鍔′笉鍙敤");
        return;
    }

    m_modelVisible = !m_modelVisible;
    fourViewService->setToolModelVisible(m_modelVisible);
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
    auto* registrationService = pointRegistrationService();
    if (!registrationService || !registrationService->hasModel()) {
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
        registrationService = pointRegistrationService();
        if (!registrationService || !registrationService->loadModelFromFile(filePath)) {
            showError("鍔犺浇妯″瀷", registrationService ? registrationService->getLastError() : QStringLiteral("配准服务未就绪"));
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
    if (m_workflowCoordinator) {
        m_workflowCoordinator->handleComputeRegistration();
    }
}

void NavigationPageNew::performComputeRegistration()
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
        auto* registrationService = pointRegistrationService();
        showError("璁＄畻閰嶅噯", registrationService ? registrationService->getLastError()
                                                       : "閰嶅噯璁＄畻澶辫触");
    }
    // 鎴愬姛鏃剁敱淇″彿鍥炶皟澶勭悊

}

void NavigationPageNew::on_calibrateButton_clicked()
{
    startProbeCalibration();
}

void NavigationPageNew::startProbeCalibration()
{
    if (!m_registrationWorkflow) {
        setupRegistration();
    }
    if (!m_registrationWorkflow) {
        showWarning("校准", "配准服务未就绪，请稍后再试");
        return;
    }
    if (m_trackingSessionId.isEmpty() || m_navigationToolId.isEmpty()) {
        showWarning("校准", "请先连接已配置追踪器械的追踪会话。");
        return;
    }

    auto* trackingService = opticalTrackingService();
    if (!trackingService) {
        showWarning("校准", "追踪器服务不可用。");
        return;
    }

    if (!m_activeCalibrationId.isEmpty()) {
        showWarning("校准", "当前已有进行中的标定，请继续采点、完成或取消。");
        updateProbeCalibrationUi();
        return;
    }

    // 搴旂敤閰嶅噯缁撴灉鍒板鑸?
    if (m_registrationWorkflow->applyToNavigation()) {
        showInfo("应用配准", "配准结果已应用到导航系统！");
    } else {
        showWarning("应用配准", "请先完成配准计算");
        return;
    }

    const QString calibrationId = trackingService->startToolCalibration(m_trackingSessionId, m_navigationToolId, QStringLiteral("pivot"));
    if (calibrationId.isEmpty()) {
        showError("校准", trackingService->getLastError());
        return;
    }

    const QVariantMap calibrationStatus = trackingService->getCalibrationStatus(calibrationId);
    const int requiredPoints = calibrationStatus.value(QStringLiteral("requiredPoints")).toInt();
    if (requiredPoints <= 0) {
        trackingService->cancelCalibration(calibrationId);
        showError("校准", QStringLiteral("校准会话未返回有效采样要求。"));
        return;
    }

    m_activeCalibrationId = calibrationId;
    m_activeCalibrationRequiredPoints = requiredPoints;
    m_activeCalibrationCollectedPoints = calibrationStatus.value(QStringLiteral("pointCount")).toInt();
    updateProbeCalibrationUi();

    showInfo("校准", QStringLiteral("探针标定已开始，请围绕枢轴稳定旋转并逐点采集。\n当前进度：%1/%2")
                           .arg(m_activeCalibrationCollectedPoints)
                           .arg(m_activeCalibrationRequiredPoints));
}

void NavigationPageNew::captureProbeCalibrationPoint()
{
    if (m_activeCalibrationId.isEmpty()) {
        showWarning("校准", "当前没有进行中的标定会话。");
        return;
    }

    auto* trackingService = opticalTrackingService();
    if (!trackingService) {
        showWarning("校准", "追踪器服务不可用。");
        return;
    }

    const QString calibrationId = m_activeCalibrationId;
    if (!trackingService->addCalibrationPoint(calibrationId)) {
        showError("校准", trackingService->getLastError());
        return;
    }

    const QVariantMap calibrationStatus = trackingService->getCalibrationStatus(calibrationId);
    m_activeCalibrationRequiredPoints = calibrationStatus.value(QStringLiteral("requiredPoints")).toInt();
    m_activeCalibrationCollectedPoints = calibrationStatus.value(QStringLiteral("pointCount")).toInt();
    updateProbeCalibrationUi();

    if (m_activeCalibrationCollectedPoints >= m_activeCalibrationRequiredPoints) {
        showInfo("校准", QStringLiteral("标定采样已满足要求，可以执行完成标定。"));
    }
}

void NavigationPageNew::finishProbeCalibration()
{
    if (m_activeCalibrationId.isEmpty()) {
        showWarning("校准", "当前没有进行中的标定会话。");
        return;
    }

    auto* trackingService = opticalTrackingService();
    if (!trackingService) {
        showWarning("校准", "追踪器服务不可用。");
        return;
    }

    if (m_activeCalibrationCollectedPoints < m_activeCalibrationRequiredPoints) {
        showWarning("校准", QStringLiteral("标定采样不足，当前为 %1/%2。")
                                .arg(m_activeCalibrationCollectedPoints)
                                .arg(m_activeCalibrationRequiredPoints));
        updateProbeCalibrationUi();
        return;
    }

    const QString calibrationId = m_activeCalibrationId;
    const QVariantMap calibrationResult = trackingService->finishCalibration(calibrationId);
    if (!calibrationResult.value(QStringLiteral("success")).toBool()) {
        showError("校准", calibrationResult.value(QStringLiteral("error")).toString());
        return;
    }

    if (!trackingService->applyCalibrationResult(m_trackingSessionId, m_navigationToolId, calibrationResult)) {
        showError("校准", trackingService->getLastError());
        return;
    }

    if (m_runtimeCoordinator) {
        m_runtimeCoordinator->handleCalibrationTransform(calibrationMatrixFromResult(calibrationResult));
    }

    const double calibrationAccuracy = calibrationResult.value(QStringLiteral("accuracy")).toDouble();
    const int completedCalibrationPoints = m_activeCalibrationRequiredPoints;
    resetProbeCalibrationState();
    if (m_runtimeCoordinator) {
        QVariantMap trackingQuality;
        trackingQuality.insert(QStringLiteral("calibrated"), true);
        trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), calibrationAccuracy);
        m_runtimeCoordinator->handleCalibrationCompleted(trackingQuality);
    }
    if (m_workspaceApplicationService) {
        NavigationWorkspaceCalibrationState calibrationState;
        calibrationState.trackingReady = !m_trackingSessionId.isEmpty() && !m_navigationToolId.isEmpty();
        calibrationState.started = true;
        calibrationState.collectedPoints = completedCalibrationPoints;
        calibrationState.requiredPoints = completedCalibrationPoints;
        calibrationState.completed = true;
        calibrationState.accuracy = calibrationAccuracy;
        m_workspaceApplicationService->recordCalibrationState(calibrationState);
        m_workspaceApplicationService->persistSnapshot();
    }
    refreshNavigationConfidenceState();
    persistEvaluationReportSnapshot();
    showInfo("校准", QStringLiteral("探针标定完成，已应用到当前导航器械。\n精度：%1 mm")
                           .arg(calibrationAccuracy, 0, 'f', 3));
}

void NavigationPageNew::cancelProbeCalibration()
{
    if (m_activeCalibrationId.isEmpty()) {
        resetProbeCalibrationState();
        return;
    }

    auto* trackingService = opticalTrackingService();
    const QString calibrationId = m_activeCalibrationId;
    if (trackingService && !trackingService->cancelCalibration(calibrationId)) {
        showError("校准", trackingService->getLastError());
        return;
    }

    resetProbeCalibrationState();
    if (m_workspaceApplicationService) {
        NavigationWorkspaceCalibrationState calibrationState;
        calibrationState.trackingReady = !m_trackingSessionId.isEmpty() && !m_navigationToolId.isEmpty();
        m_workspaceApplicationService->recordCalibrationState(calibrationState);
        m_workspaceApplicationService->persistSnapshot();
    }
    showInfo("校准", "当前探针标定已取消。");
}

// ========== 瀵艰埅鎺у埗 ==========

void NavigationPageNew::on_connectTrackerButton_clicked()
{
    auto* trackingService = opticalTrackingService();
    auto* instrumentService = instrumentManagementService();
    if (!trackingService) {
        showWarning("追踪器", "追踪器服务不可用。");
        return;
    }
    if (!instrumentService) {
        showWarning("追踪器", "器械服务不可用。");
        return;
    }
    if (m_selectedInstrumentId < 0) {
        showWarning("追踪器", "请先选择用于导航的器械。");
        return;
    }

    InstrumentItem instrument = instrumentService->getInstrument(m_selectedInstrumentId);
    if (!instrument.isValid()) {
        showWarning("追踪器", "当前器械无效，请重新选择。");
        return;
    }
    if (!instrument.hasTracking()) {
        showWarning("追踪器", "当前器械未配置追踪标记和几何文件。");
        return;
    }

    clearTrackingRuntimeState(true);

    const QStringList devices = trackingService->scanAvailableDevices();
    if (devices.isEmpty()) {
        showWarning("追踪器", "未发现可用追踪设备。");
        return;
    }

    const QString deviceId = devices.first();
    if (!trackingService->connectToDevice(deviceId)) {
        showError("追踪器", trackingService->getLastError());
        return;
    }

    const QString sessionName = QStringLiteral("ankle_navigation_%1").arg(
        m_workflowContext && !m_workflowContext->caseId().isEmpty()
            ? m_workflowContext->caseId()
            : QStringLiteral("default"));
    const QString sessionId = trackingService->createTrackingSession(deviceId, sessionName);
    if (sessionId.isEmpty()) {
        showError("追踪器", trackingService->getLastError());
        trackingService->disconnectDevice(deviceId);
        return;
    }

    const QString geometryPath = QDir::fromNativeSeparators(instrument.geometryFilePath.trimmed());
    QFileInfo geometryInfo(geometryPath);
    QString geometryFile = geometryPath;
    if (!geometryInfo.isAbsolute()) {
        const QString projectPath = instrumentService->getProjectPath();
        const QString projectGeometryFile = QDir(projectPath).filePath(geometryPath);
        if (QFileInfo::exists(projectGeometryFile)) {
            geometryFile = QDir::fromNativeSeparators(projectGeometryFile);
            geometryInfo.setFile(geometryFile);
        } else if (!geometryInfo.fileName().isEmpty()) {
            geometryFile = geometryInfo.fileName();
            geometryInfo.setFile(geometryFile);
        }
    }

    QVariantMap toolConfig;
    toolConfig[QStringLiteral("name")] = instrument.name;
    toolConfig[QStringLiteral("type")] = QStringLiteral("probe");
    toolConfig[QStringLiteral("markerId")] = instrument.trackingMarkerId;
    toolConfig[QStringLiteral("trackingMarkerId")] = instrument.trackingMarkerId;
    toolConfig[QStringLiteral("geometryFile")] = QDir::fromNativeSeparators(geometryFile);
    toolConfig[QStringLiteral("geometryId")] = geometryInfo.completeBaseName().remove(QStringLiteral("geometry"), Qt::CaseInsensitive);

    const QString toolId = trackingService->addTrackingTool(sessionId, instrument.name, toolConfig);
    if (toolId.isEmpty()) {
        showError("追踪器", trackingService->getLastError());
        trackingService->closeTrackingSession(sessionId);
        trackingService->disconnectDevice(deviceId);
        return;
    }

    if (!trackingService->startTracking(sessionId)) {
        showError("追踪器", trackingService->getLastError());
        trackingService->removeTrackingTool(sessionId, toolId);
        trackingService->closeTrackingSession(sessionId);
        trackingService->disconnectDevice(deviceId);
        return;
    }

    m_trackingSessionId = sessionId;
    m_navigationToolId = toolId;
    if (m_runtimeState) {
        m_runtimeState->setCaseContext(
            m_workflowContext ? m_workflowContext->caseId() : QString(),
            m_trackingSessionId,
            m_navigationToolId);
    }
    updateTrackerStatus(true);
    showInfo("追踪器", QStringLiteral("追踪器已连接，当前器械已接入导航会话。"));
}

void NavigationPageNew::on_disconnectTrackerButton_clicked()
{
    clearTrackingRuntimeState(true);
    m_trackerTimer->stop();
    updateTrackerStatus(false);
    if (m_workspaceApplicationService) {
        NavigationWorkspaceNavigationState navigationState = m_workspaceApplicationService->currentSnapshot().navigationState;
        navigationState.trackerConnected = false;
        navigationState.running = false;
        navigationState.allowNavigation = false;
        navigationState.summaryText = QStringLiteral("导航未就绪");
        m_workspaceApplicationService->recordNavigationState(navigationState);
        m_workspaceApplicationService->persistSnapshot();
    }
    refreshEvaluationSummary();
}

void NavigationPageNew::on_startNavigationButton_clicked()
{
    if (m_workflowCoordinator) {
        m_workflowCoordinator->handleStartNavigation();
    }
}

void NavigationPageNew::performStartNavigation()
{
    if (!m_trackerConnected) {
        showWarning("导航", "请先连接追踪器。");
        return;
    }
    if (m_trackingSessionId.isEmpty() || m_navigationToolId.isEmpty()) {
        showWarning("导航", "当前导航器械尚未接入追踪会话。");
        return;
    }

    auto* registrationService = pointRegistrationService();
    const PointRegistrationResult registrationResult = m_runtimeState->hasRegistrationResult()
        ? m_runtimeState->registrationResult()
        : (m_registrationWorkflow ? m_registrationWorkflow->getLastResult() : PointRegistrationResult());
    const QMatrix4x4 registrationTransform = m_registrationTransform.isIdentity()
        ? (registrationService ? registrationService->getTransformMatrix() : QMatrix4x4())
        : m_registrationTransform;
    if (registrationTransform.isIdentity() || !registrationResult.success) {
        showWarning("导航", "尚未完成配准，请先在配准Tab中完成点配准。");
        return;
    }
    m_registrationTransform = registrationTransform;

    refreshNavigationConfidenceState(true);
    if (!m_lastConfidence.allowNavigation) {
        return;
    }

    setWorkflowStage(AnkleWorkflowStage::Navigation);

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

    m_navigationActive = true;
    if (m_workspaceApplicationService) {
        NavigationWorkspaceNavigationState navigationState = m_workspaceApplicationService->currentSnapshot().navigationState;
        navigationState.trackerConnected = m_trackerConnected;
        navigationState.running = true;
        navigationState.confidence = m_lastConfidence.score;
        navigationState.allowNavigation = m_lastConfidence.allowNavigation;
        navigationState.blockReasons = m_lastConfidence.recommendations;
        m_workspaceApplicationService->recordNavigationState(navigationState);
        m_workspaceApplicationService->persistSnapshot();
    }
    updateFourViewWidgetPlacement();
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

    updateFourViewWidgetPlacement();
    ui->startNavigationButton->setEnabled(true);
    ui->pauseNavigationButton->setEnabled(false);

    const QString caseId = m_workflowContext->caseId();
    if (!caseId.isEmpty()) {
        NavigationEvaluationService evaluationService(evaluationCasesRoot());

        AnkleNavigationRunRecord run;
        run.caseId = caseId;
        run.navigationMode = QStringLiteral("replay");
        run.confidenceScore = m_lastConfidence.score;
        run.warnings = m_lastConfidence.recommendations;
        QVariantMap trackingQuality;
        if (auto* trackingService = opticalTrackingService()) {
            trackingQuality = trackingService->checkTrackingQuality(m_trackingSessionId, m_navigationToolId);
        }

        if (m_registrationWorkflow) {
            const PointRegistrationResult registrationResult = m_registrationWorkflow->getLastResult();
            run.metrics = registrationResult.metrics;
            run.metrics.insert(QStringLiteral("registration_mode"), registrationResult.metrics.value(QStringLiteral("registration_mode")));
            run.metrics.insert(QStringLiteral("target_region_tre_mm"), registrationResult.targetRegionTre);
            run.metrics.insert(QStringLiteral("coverage_score"), registrationResult.coverageScore);
        }

        run.metrics.insert(QStringLiteral("tracking_jitter_mm"), trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble());
        run.metrics.insert(QStringLiteral("visible_frame_ratio"), trackingQuality.value(QStringLiteral("visible_frame_ratio")).toDouble());
        run.metrics.insert(QStringLiteral("tracking_latency_ms"), trackingQuality.value(QStringLiteral("tracking_latency_ms")).toDouble());
        run.metrics.insert(QStringLiteral("tracking_profile"), trackingQuality.value(QStringLiteral("tracking_profile")).toString());
        run.metrics.insert(QStringLiteral("tracking_confidence_score"), trackingQuality.value(QStringLiteral("tracking_confidence_score")).toDouble());
        evaluationService.saveNavigationRun(run);
        if (m_workspaceApplicationService) {
            NavigationWorkspaceNavigationState navigationState = m_workspaceApplicationService->currentSnapshot().navigationState;
            navigationState.trackerConnected = m_trackerConnected;
            navigationState.running = false;
            navigationState.confidence = m_lastConfidence.score;
            navigationState.allowNavigation = m_lastConfidence.allowNavigation;
            navigationState.blockReasons = m_lastConfidence.recommendations;
            navigationState.hasRunRecord = true;
            m_workspaceApplicationService->recordNavigationState(navigationState);
            m_workspaceApplicationService->persistSnapshot();
        }

        persistEvaluationReportSnapshot(true);
    }

    refreshNavigationConfidenceState();
    refreshEvaluationSummary();
}

void NavigationPageNew::on_exportEvaluationSummaryButton_clicked()
{
    NavigationEvaluationService evaluationService(evaluationCasesRoot());
    const QStringList caseIds = evaluationService.discoverExportableCaseIds();
    if (caseIds.isEmpty()) {
        showWarning(QStringLiteral("评估汇总"), QStringLiteral("当前没有可导出的病例评估记录。"));
        return;
    }

    if (!evaluationService.exportBatchSummaryCsv(caseIds)) {
        showError(QStringLiteral("评估汇总"), QStringLiteral("病例评估汇总导出失败。"));
        return;
    }

    InnovationExperimentBatchRunner runner;
    InnovationBatchInput input;
    input.caseIds = caseIds;
    input.caseDataRoot = QFileInfo(evaluationCasesRoot()).dir().absolutePath();
    const InnovationBatchOutput output = runner.run(input);

    const QStringList requiredSummaryFiles = {
        QStringLiteral("innovation_1_summary.csv"),
        QStringLiteral("innovation_2_summary.csv"),
        QStringLiteral("innovation_3_summary.csv")
    };
    for (const QString& summaryFile : requiredSummaryFiles) {
        if (!output.summaryFiles.contains(summaryFile)) {
            showError(QStringLiteral("评估汇总"), QStringLiteral("创新实验汇总导出失败：%1").arg(summaryFile));
            return;
        }
    }

    showInfo(
        QStringLiteral("评估汇总"),
        QStringLiteral("已导出 %1 个病例的评估汇总 CSV，并生成 innovation_1_summary.csv、innovation_2_summary.csv、innovation_3_summary.csv。")
            .arg(caseIds.size()));
}

void NavigationPageNew::on_resetViewButton_clicked()
{
    if (auto* fourViewService = fourViewDisplayService()) {
        fourViewService->resetViews();
        return;
    }

    showInfo("重置视图", "视图已重置为默认状态。");
}

void NavigationPageNew::onTrackerDataReceived()
{
    if (!m_navigationActive) {
        return;
    }
}

void NavigationPageNew::setupVTKViews()
{
    embedFourViewWidget();
}

void NavigationPageNew::embedFourViewWidget()
{
    if (!m_navigationVtkBridge) {
        return;
    }

    m_fourViewWidget = m_navigationVtkBridge->ensureFourViewWidget(this);
    if (!m_fourViewWidget) {
        qWarning() << "[NavigationPage] Failed to prepare FourView widget";
        return;
    }

    updateFourViewWidgetPlacement();
}

void NavigationPageNew::updateFourViewWidgetPlacement()
{
    if (!ui || !m_navigationVtkBridge) {
        return;
    }

    QWidget* currentTab = ui->tabWidget ? ui->tabWidget->currentWidget() : nullptr;

    if (currentTab == ui->planningTab) {
        m_fourViewWidget = m_navigationVtkBridge->showFourViewInPlanning(this);
        m_navigationVtkBridge->resumeFourView();
        qDebug() << "[NavigationPage] FourView VTK widget embedded in planning view";
    } else if (currentTab == ui->navigationTab) {
        if (m_navigationActive && m_navigation3DView) {
            m_navigationVtkBridge->pauseFourView();
            m_navigationVtkBridge->showNavigationContent(m_navigation3DView);
            qDebug() << "[NavigationPage] Navigation 3D view embedded in navigation view";
        } else {
            m_fourViewWidget = m_navigationVtkBridge->showFourViewInNavigation(this);
            m_navigationVtkBridge->resumeFourView();
            qDebug() << "[NavigationPage] FourView VTK widget embedded in navigation view";
        }
    } else {
        m_navigationVtkBridge->detachFourView();
        m_navigationVtkBridge->detachNavigationContent();
        m_navigationVtkBridge->pauseFourView();
    }
}

void NavigationPageNew::cleanupVTKViews()
{
    if (m_navigationVtkBridge) {
        m_navigationVtkBridge->detachAll();
    }

    if (m_fourViewWidget) {
        m_fourViewWidget->hide();
        m_fourViewWidget = nullptr;
    }

    if (m_registrationVTKWidget) {
        m_registrationVTKWidget->hide();
        m_registrationVTKWidget = nullptr;
    }
}

void NavigationPageNew::updateTrackerStatus(bool connected)
{
    m_trackerConnected = connected;

    if (connected) {
        ui->trackerStatusLabel->setText("已连接");
        ui->connectTrackerButton->setEnabled(false);
        ui->disconnectTrackerButton->setEnabled(true);
    } else {
        ui->trackerStatusLabel->setText("未连接");
        ui->connectTrackerButton->setEnabled(true);
        ui->disconnectTrackerButton->setEnabled(false);

        m_navigationActive = false;
        ui->startNavigationButton->setEnabled(true);
        ui->pauseNavigationButton->setEnabled(false);
    }

    setStatusTone(ui->trackerStatusLabel, connected ? QStringLiteral("ok") : QStringLiteral("danger"));
    updateProbeCalibrationUi();
    refreshNavigationConfidenceState();
}

void NavigationPageNew::clearTrackingRuntimeState(bool disconnectDevice)
{
    auto* trackingService = opticalTrackingService();
    if (trackingService && !m_activeCalibrationId.isEmpty()) {
        trackingService->cancelCalibration(m_activeCalibrationId);
    }
    if (trackingService && !m_trackingSessionId.isEmpty()) {
        trackingService->stopTracking(m_trackingSessionId);
        if (!m_navigationToolId.isEmpty()) {
            trackingService->removeTrackingTool(m_trackingSessionId, m_navigationToolId);
        }

        const QVariantMap sessionInfo = trackingService->getSessionInfo(m_trackingSessionId);
        const QString deviceId = sessionInfo.value(QStringLiteral("deviceId")).toString();
        trackingService->closeTrackingSession(m_trackingSessionId);
        if (disconnectDevice && !deviceId.isEmpty()) {
            trackingService->disconnectDevice(deviceId);
        }
    }

    m_trackingSessionId.clear();
    m_navigationToolId.clear();
    if (m_runtimeCoordinator) {
        m_runtimeCoordinator->clearPoseTrackingState();
    }
    if (m_runtimeState) {
        m_runtimeState->setCaseContext(
            m_workflowContext ? m_workflowContext->caseId() : QString(),
            m_trackingSessionId,
            m_navigationToolId);
    }
    resetProbeCalibrationState();
}

void NavigationPageNew::resetProbeCalibrationState()
{
    m_activeCalibrationId.clear();
    m_activeCalibrationRequiredPoints = 0;
    m_activeCalibrationCollectedPoints = 0;
    updateProbeCalibrationUi();
}

void NavigationPageNew::refreshNavigationConfidenceState(bool showWarnings)
{
    const auto resetGateUi = [this](bool enableStartButton) {
        if (m_runtimeState) {
            m_runtimeState->clearConfidenceResult();
        }

        if (m_workspaceUiBinder) {
            NavigationStageGate gate;
            gate.requestedStage = AnkleWorkflowStage::Navigation;
            gate.allowed = false;
            gate.reasonCode = QStringLiteral("runtime_not_ready");
            gate.reasonText = QStringLiteral("未就绪");
            gate.severity = QStringLiteral("warning");
            m_workspaceUiBinder->applyStageGate(gate);

            NavigationWorkspaceNavigationState navigationState;
            navigationState.trackerConnected = m_trackerConnected;
            navigationState.allowNavigation = false;
            if (m_workspaceApplicationService) {
                m_workspaceApplicationService->recordNavigationState(navigationState);
                m_workspaceApplicationService->persistSnapshot();
            }
            m_workspaceUiBinder->applyNavigationConfidence(navigationState, gate);
        }
        if (ui && ui->startNavigationButton && !m_navigationActive) {
            ui->startNavigationButton->setEnabled(enableStartButton);
        }
        refreshStageGateUi();
    };

    if (!m_runtimeCoordinator || !m_runtimeState || !m_trackerConnected || m_trackingSessionId.isEmpty() || m_navigationToolId.isEmpty()) {
        m_lastConfidence = NavigationConfidenceResult();
        resetGateUi(m_trackerConnected);
        return;
    }

    auto* registrationService = pointRegistrationService();
    const PointRegistrationResult registrationResult = m_runtimeState->hasRegistrationResult()
        ? m_runtimeState->registrationResult()
        : (m_registrationWorkflow ? m_registrationWorkflow->getLastResult() : PointRegistrationResult());
    const QMatrix4x4 registrationTransform = m_registrationTransform.isIdentity()
        ? (registrationService ? registrationService->getTransformMatrix() : QMatrix4x4())
        : m_registrationTransform;
    if (!registrationService && !m_runtimeState->hasRegistrationResult()) {
        m_lastConfidence = NavigationConfidenceResult();
        m_runtimeState->clearRegistrationResult();
        resetGateUi(false);
        return;
    }
    if (registrationTransform.isIdentity() || !registrationResult.success) {
        m_lastConfidence = NavigationConfidenceResult();
        m_runtimeState->clearRegistrationResult();
        resetGateUi(false);
        return;
    }

    QVariantMap trackingQuality;
    if (auto* trackingService = opticalTrackingService()) {
        trackingService->getRealTimeData(m_trackingSessionId);
        trackingQuality = trackingService->checkTrackingQuality(m_trackingSessionId, m_navigationToolId);
    }

    if (trackingQuality.isEmpty()) {
        trackingQuality.insert(QStringLiteral("tracking_jitter_mm"), 0.4);
        trackingQuality.insert(QStringLiteral("visible_frame_ratio"), 1.0);
        trackingQuality.insert(QStringLiteral("calibrated"), false);
        trackingQuality.insert(QStringLiteral("calibration_accuracy_mm"), 0.0);
    }

    m_runtimeCoordinator->handleRegistrationResult(registrationResult);
    m_runtimeCoordinator->handleTrackingQuality(trackingQuality);
    m_runtimeCoordinator->recomputeConfidence();

    if (!m_runtimeCoordinator->runtimeState()->hasConfidenceResult()) {
        m_lastConfidence = NavigationConfidenceResult();
        return;
    }

    m_lastConfidence = m_runtimeCoordinator->runtimeState()->confidenceResult();

    if (m_workspaceUiBinder) {
        NavigationStageGate gate;
        gate.requestedStage = AnkleWorkflowStage::Navigation;
        gate.allowed = m_lastConfidence.allowNavigation;
        gate.reasonCode = m_lastConfidence.allowNavigation
            ? QStringLiteral("ok")
            : QStringLiteral("navigation_confidence_blocked");
        gate.reasonText = m_lastConfidence.allowNavigation
            ? QStringLiteral("允许进入导航")
            : QStringLiteral("暂不允许进入导航");
        gate.severity = m_lastConfidence.allowNavigation ? QStringLiteral("ok") : QStringLiteral("danger");
        m_workspaceUiBinder->applyStageGate(gate);

        NavigationWorkspaceNavigationState navigationState;
        navigationState.confidence = m_lastConfidence.score;
        navigationState.allowNavigation = m_lastConfidence.allowNavigation;
        navigationState.blockReasons = m_lastConfidence.recommendations;
        navigationState.trackerConnected = m_trackerConnected;
        if (m_workspaceApplicationService) {
            m_workspaceApplicationService->recordNavigationState(navigationState);
            m_workspaceApplicationService->persistSnapshot();
        }
        m_workspaceUiBinder->applyNavigationConfidence(
            navigationState,
            m_workspaceApplicationService
                ? m_workspaceApplicationService->currentSnapshot().stageGate
                : gate,
            false);
    }

    if (ui && ui->startNavigationButton && !m_navigationActive) {
        ui->startNavigationButton->setEnabled(m_lastConfidence.allowNavigation);
    }

    refreshStageGateUi();

    if (showWarnings && !m_lastConfidence.allowNavigation) {
        const QString warningText = m_lastConfidence.recommendations.isEmpty()
            ? QStringLiteral("当前导航准入条件不足。")
            : m_lastConfidence.recommendations.join(QStringLiteral("；"));
        showWarning(QStringLiteral("导航准入"), warningText);
    }
}

void NavigationPageNew::refreshStageGateUi()
{
    if (!m_workspaceApplicationService || !m_workflowContext) {
        return;
    }

    const QString caseId = m_workflowContext->caseId();
    if (caseId.isEmpty()) {
        return;
    }

    const NavigationWorkspaceSnapshot snapshot = m_workspaceApplicationService->loadWorkspace(
        caseId,
        m_workflowContext->patientId() >= 0 ? QString::number(m_workflowContext->patientId()) : QString(),
        m_workflowContext->patientName());
    Q_UNUSED(snapshot);
    m_workspaceApplicationService->evaluateStageGate(m_workflowContext->currentStage());
    m_workspaceApplicationService->persistSnapshot();

    if (m_workspaceUiBinder) {
        m_workspaceUiBinder->refreshFromSnapshot(m_workspaceApplicationService->currentSnapshot());
    }
    refreshEvaluationSummary();
}

void NavigationPageNew::persistEvaluationReportSnapshot(bool exportMetricsCsv)
{
    if (!m_runtimeCoordinator) {
        refreshEvaluationSummary();
        return;
    }

    m_runtimeCoordinator->persistEvaluationReportSnapshot(exportMetricsCsv);
    refreshEvaluationSummary();
}

void NavigationPageNew::updateProbeCalibrationUi()
{
    auto* captureCalibrationPointButton = findChild<QPushButton*>(QStringLiteral("captureCalibrationPointButton"));
    auto* finishCalibrationButton = findChild<QPushButton*>(QStringLiteral("finishCalibrationButton"));
    auto* cancelCalibrationButton = findChild<QPushButton*>(QStringLiteral("cancelCalibrationButton"));

    bool hasActiveCalibration = !m_activeCalibrationId.isEmpty();
    QString calibrationStateText = QStringLiteral("待开始");

    if (hasActiveCalibration) {
        if (auto* trackingService = opticalTrackingService()) {
            const QVariantMap calibrationStatus = trackingService->getCalibrationStatus(m_activeCalibrationId);
            if (calibrationStatus.value(QStringLiteral("valid")).toBool()) {
                m_activeCalibrationRequiredPoints = calibrationStatus.value(QStringLiteral("requiredPoints")).toInt();
                m_activeCalibrationCollectedPoints = calibrationStatus.value(QStringLiteral("pointCount")).toInt();
                calibrationStateText = calibrationStatus.value(QStringLiteral("status")).toString();
            } else {
                hasActiveCalibration = false;
                m_activeCalibrationId.clear();
                m_activeCalibrationRequiredPoints = 0;
                m_activeCalibrationCollectedPoints = 0;
            }
        } else {
            hasActiveCalibration = false;
        }
    }

    const bool trackingReady = !m_trackingSessionId.isEmpty() && !m_navigationToolId.isEmpty();
    const bool canCapturePoint = hasActiveCalibration && m_activeCalibrationCollectedPoints < m_activeCalibrationRequiredPoints;
    const bool canFinishCalibration = hasActiveCalibration
        && m_activeCalibrationRequiredPoints > 0
        && m_activeCalibrationCollectedPoints >= m_activeCalibrationRequiredPoints;

    if (ui->calibrateButton) {
        ui->calibrateButton->setEnabled(trackingReady && !hasActiveCalibration);
    }
    if (captureCalibrationPointButton) {
        captureCalibrationPointButton->setEnabled(canCapturePoint);
    }
    if (finishCalibrationButton) {
        finishCalibrationButton->setEnabled(canFinishCalibration);
    }
    if (cancelCalibrationButton) {
        cancelCalibrationButton->setEnabled(hasActiveCalibration);
    }

    if (!m_workspaceUiBinder) {
        return;
    }

    const NavigationWorkspaceCalibrationState snapshotCalibrationState = m_workspaceApplicationService
        ? m_workspaceApplicationService->currentSnapshot().calibrationState
        : NavigationWorkspaceCalibrationState();
    NavigationWorkspaceCalibrationState calibrationState;
    calibrationState.trackingReady = trackingReady;

    if (!trackingReady && snapshotCalibrationState.completed) {
        calibrationState = snapshotCalibrationState;
        m_workspaceUiBinder->applyCalibrationSummary(calibrationState);
        return;
    }

    if (!trackingReady) {
        if (m_workspaceApplicationService) {
            m_workspaceApplicationService->recordCalibrationState(calibrationState);
            m_workspaceApplicationService->persistSnapshot();
        }
        m_workspaceUiBinder->applyCalibrationSummary(calibrationState);
        return;
    }

    if (!hasActiveCalibration && snapshotCalibrationState.completed) {
        calibrationState = snapshotCalibrationState;
        calibrationState.trackingReady = trackingReady;
        if (m_workspaceApplicationService) {
            m_workspaceApplicationService->recordCalibrationState(calibrationState);
            m_workspaceApplicationService->persistSnapshot();
        }
        m_workspaceUiBinder->applyCalibrationSummary(calibrationState);
        return;
    }

    if (!hasActiveCalibration) {
        if (m_workspaceApplicationService) {
            m_workspaceApplicationService->recordCalibrationState(calibrationState);
            m_workspaceApplicationService->persistSnapshot();
        }
        m_workspaceUiBinder->applyCalibrationSummary(calibrationState);
        return;
    }

    calibrationState.started = hasActiveCalibration;
    calibrationState.collectedPoints = m_activeCalibrationCollectedPoints;
    calibrationState.requiredPoints = m_activeCalibrationRequiredPoints;
    calibrationState.completed = canFinishCalibration;
    calibrationState.statusText = calibrationStateText;
    if (m_workspaceApplicationService) {
        m_workspaceApplicationService->recordCalibrationState(calibrationState);
        m_workspaceApplicationService->persistSnapshot();
    }
    m_workspaceUiBinder->applyCalibrationSummary(calibrationState);
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

    // 鏇存柊杩涘害鏉★紙鍋囪5mm鏄渶澶ц宸級
    int progress = qBound(0, static_cast<int>((5.0 - accuracy) / 5.0 * 100), 100);
    ui->accuracyBar->setValue(progress);
    setStatusTone(
        ui->accuracyValueLabel,
        accuracy < 1.0 ? QStringLiteral("ok") : (accuracy < 2.0 ? QStringLiteral("warning") : QStringLiteral("danger")));
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
    auto* segService = segmentationService();
    auto* fourViewService = fourViewDisplayService();
    if (fourViewService && segService && !outputDir.isEmpty()) {
        const QString stlPath = QDir(outputDir).filePath("segmentation_mesh.stl");
        if (segService->exportSegmentation(taskId, stlPath, "stl")
            && fourViewService->loadToolModel(stlPath)) {
            fourViewService->setToolModelVisible(true);
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
    if (pointRegistrationService()) {
        return true;
    }

    if (!m_serviceBundle || !m_serviceBundle->isPointRegistrationFrameworkReady()) {
        if (ui && ui->registrationViewPlaceholder) {
            ui->registrationViewPlaceholder->setText(QStringLiteral("CTK 妗嗘灦鏈氨缁紝閰嶅噯鏈嶅姟灏氫笉鍙敤"));
        }
        return false;
    }

    if (!pointRegistrationService(tryStartPlugin)) {
        const QString state = m_serviceBundle->pointRegistrationPluginState();
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

    auto* registrationService = pointRegistrationService();
    auto* trackingService = opticalTrackingService();

    // 鍒涘缓閰嶅噯宸ヤ綔娴?
    m_registrationWorkflow = new RegistrationWorkflow(registrationService, this);

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
    connect(registrationService, &PointRegistrationService::pointAdded,
            this, [this](int, const QString&) {
                updateRegistrationPointsList();
                refreshNavigationConfidenceState();
                if (m_registrationVTKWidget) {
                    QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
                }
            });
    connect(registrationService, &PointRegistrationService::pointRemoved,
            this, [this](int) {
                updateRegistrationPointsList();
                refreshNavigationConfidenceState();
                if (m_registrationVTKWidget) {
                    QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
                }
            });
    connect(registrationService, &PointRegistrationService::pointsCleared,
            this, [this]() {
                updateRegistrationPointsList();
                refreshNavigationConfidenceState();
                if (m_registrationVTKWidget) {
                    QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
                }
            });
    connect(registrationService, &PointRegistrationService::pointUpdated,
            this, [this](int) {
                updateRegistrationPointsList();
                refreshNavigationConfidenceState();
                if (m_registrationVTKWidget) {
                    QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
                }
            }, Qt::UniqueConnection);
    connect(registrationService, &PointRegistrationService::registrationApplied,
            this, [this](const QString&) {
                if (auto* service = pointRegistrationService()) {
                    m_registrationTransform = service->getTransformMatrix();
                }
                refreshNavigationConfidenceState();
            }, Qt::UniqueConnection);
    connect(registrationService, &PointRegistrationService::sessionStateChanged,
            this, [this](RegistrationSessionState) {
                refreshNavigationConfidenceState();
            }, Qt::UniqueConnection);

    if (trackingService) {
        connect(trackingService, &OpticalTrackingService::toolStatusChanged,
                this, [this](const QString& sessionId, const QString& toolId, const QVariantMap&) {
                    if (sessionId != m_trackingSessionId || toolId != m_navigationToolId) {
                        return;
                    }
                    updateProbeCalibrationUi();
                    refreshNavigationConfidenceState();
                }, Qt::UniqueConnection);
        connect(trackingService, &OpticalTrackingService::calibrationCompleted,
                this, [this](const QString& calibrationId, const QVariantMap&) {
                    if (!m_activeCalibrationId.isEmpty() && calibrationId != m_activeCalibrationId) {
                        return;
                    }
                    updateProbeCalibrationUi();
                    refreshNavigationConfidenceState();
                }, Qt::UniqueConnection);
    }

    // 鍚姩鏂颁細璇?
    QString patientIdStr = m_workflowContext->patientId() >= 0 ? QString::number(m_workflowContext->patientId()) : "";
    m_registrationWorkflow->startNewSession(patientIdStr);

    const QString caseId = m_workflowContext ? m_workflowContext->caseId() : QString();
    const QString casesRoot = evaluationCasesRoot();
    if (!caseId.isEmpty() && !casesRoot.isEmpty()) {
        const QString dataRoot = QFileInfo(casesRoot).dir().absolutePath();
        const AnkleCaseWorkspaceRepository repository(dataRoot);
        const AnklePlanningService planningService(repository);
        const AnklePlanningData planning = planningService.loadPlanning(caseId);

        if (planning.targetRegionRadiusMm > 0.0) {
            TargetRegistrationRegion region;
            region.origin = planning.targetRegionCenter;
            region.primaryAxis = QVector3D(0.0f, 0.0f, 1.0f);
            region.radiusMm = planning.targetRegionRadiusMm;
            m_registrationWorkflow->setTargetRegistrationRegion(region);
        }

        m_registrationWorkflow->setPlanningConstraintContext(planningConstraintContextFromData(planning));
        m_registrationWorkflow->setPlanningConstraintRegions(planning.anatomicalConstraintRegions);
    }

    // 绔嬪嵆宓屽叆閰嶅噯VTK Widget锛堢幇鍦ㄦ彃浠跺凡鏀逛负deferred鍔犺浇锛屾湇鍔″簲璇ュ彲鐢級
    embedRegistrationVTKWidget();

    // 濡傛灉鐢ㄦ埛鍦ㄨ鍒掗〉闈㈠厛鍔犺浇浜嗘ā鍨嬶紝浣嗗綋鏃堕厤鍑嗘湇鍔?瑙嗗浘鏈氨缁紝鍒欐澶勮ˉ鍔犺浇
    if (!m_lastLoadedModelPath.isEmpty() && !registrationService->hasModel()) {
        registrationService->loadModelFromFile(m_lastLoadedModelPath);
    }

    qDebug() << "[NavigationPage] Registration workflow initialized";

}

void NavigationPageNew::embedRegistrationVTKWidget()
{
    if (m_registrationVTKWidget) {
        if (m_navigationVtkBridge) {
            m_navigationVtkBridge->ensureRegistrationWidget(ui && ui->registrationViewFrame
                ? static_cast<QWidget*>(ui->registrationViewFrame)
                : this);
        }
        return;
    }

    if (!ensurePointRegistrationService(true)) {
        return;
    }

    QWidget* viewParent = ui && ui->registrationViewFrame ? static_cast<QWidget*>(ui->registrationViewFrame) : this;
    m_registrationVTKWidget = m_navigationVtkBridge
        ? m_navigationVtkBridge->ensureRegistrationWidget(viewParent)
        : nullptr;
    if (!m_registrationVTKWidget) {
        qWarning() << "[NavigationPage] Failed to create registration VTK widget";
        return;
    }

    m_registrationVTKWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(m_registrationVTKWidget, SIGNAL(pointPicked(double,double,double)),
            this, SLOT(onRegistrationPointPicked(double,double,double)));

    qDebug() << "[NavigationPage] Registration VTK widget embedded";
}

void NavigationPageNew::updateRegistrationPointsList()
{
    auto* registrationService = pointRegistrationService();
    if (!registrationService || !ui->registrationPointsTable) return;

    auto points = registrationService->getAllPoints();
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
    auto* registrationService = pointRegistrationService();
    if (!registrationService || !ui->registrationPointsTable) return;

    int row = ui->registrationPointsTable->currentRow();
    if (row < 0) {
        showWarning("删除点", "请先在列表中选择要删除的点");
        return;
    }

    registrationService->removePoint(row);
    updateRegistrationPointsList();

    // 鍒锋柊3D瑙嗗浘
    if (m_registrationVTKWidget) {
        QMetaObject::invokeMethod(m_registrationVTKWidget, "updatePointMarkers");
    }

    qDebug() << "[NavigationPage] Deleted point at index:" << row;

}

void NavigationPageNew::on_clearAllPointsButton_clicked()
{
    auto* registrationService = pointRegistrationService();
    if (!registrationService) return;

    if (registrationService->pointCount() == 0) {
        showInfo("清空点", "当前没有配准点");
        return;
    }

    if (!showConfirm("清空点", "确定要清空所有配准点吗？此操作不可撤销。")) {
        return;
    }

    registrationService->clearPoints();
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
        setStatusTone(ui->regErrorLabel, QStringLiteral("warning"));
        return;
    }

    m_registrationTransform = result.transformMatrix;

    // 鏇存柊閰嶅噯璇樊鏄剧ず
    ui->regErrorLabel->setText(QString("%1 mm").arg(result.rmsError, 0, 'f', 2));

    setStatusTone(
        ui->regErrorLabel,
        result.rmsError < 1.0 ? QStringLiteral("ok") : (result.rmsError < 2.0 ? QStringLiteral("warning") : QStringLiteral("danger")));
}

void NavigationPageNew::restoreRegistrationSnapshotState()
{
    if (!m_workspaceApplicationService) {
        return;
    }

    const NavigationWorkspaceRegistrationState registrationState =
        m_workspaceApplicationService->currentSnapshot().registrationState;
    if (!registrationState.success) {
        return;
    }

    const PointRegistrationResult registrationResult =
        buildRegistrationResultFromWorkspaceState(registrationState);
    m_registrationTransform = registrationResult.transformMatrix;

    updateRegistrationResultDisplay(registrationResult);
    if (m_runtimeState) {
        m_runtimeState->setRegistrationResult(registrationResult);
    }
    if (m_runtimeCoordinator) {
        m_runtimeCoordinator->handleRegistrationTransform(m_registrationTransform);
    }
}

void NavigationPageNew::restoreNavigationSnapshotState()
{
    if (!m_workspaceApplicationService) {
        return;
    }

    const NavigationWorkspaceNavigationState navigationState =
        m_workspaceApplicationService->currentSnapshot().navigationState;
    m_navigationActive = navigationState.running;

    if (m_navigationTimer) {
        m_navigationTimer->stop();
    }
    if (m_motionSimulator) {
        m_motionSimulator->setPaused(!navigationState.running);
    }
    if (m_navigation3DView) {
        m_navigation3DView->setProbeVisible(navigationState.toolVisible || navigationState.running);
    }

    if (ui->startNavigationButton) {
        ui->startNavigationButton->setEnabled(!navigationState.running);
    }
    if (ui->pauseNavigationButton) {
        ui->pauseNavigationButton->setEnabled(navigationState.running);
    }

    updateFourViewWidgetPlacement();
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
    if (m_registrationController) {
        m_registrationController->handleRegistrationCompleted(result);
    }
    if (m_runtimeCoordinator) {
        m_runtimeCoordinator->handleRegistrationTransform(result.transformMatrix);
    }
    refreshNavigationConfidenceState();
    if (!m_registrationWorkflow) {
        return;
    }

    const QString caseId = m_workflowContext->caseId();
    if (!caseId.isEmpty()) {
        if (m_workspaceApplicationService) {
            NavigationWorkspaceRegistrationState registrationState;
            registrationState.pointCount = result.metrics.value(QStringLiteral("point_count")).toInt();
            registrationState.success = result.success;
            registrationState.fre = result.rmsError;
            registrationState.targetTre = result.targetRegionTre;
            registrationState.coverageScore = result.coverageScore;
            registrationState.translationX = result.translationX;
            registrationState.translationY = result.translationY;
            registrationState.translationZ = result.translationZ;
            registrationState.rotationX = result.rotationX;
            registrationState.rotationY = result.rotationY;
            registrationState.rotationZ = result.rotationZ;
            registrationState.transformMatrix = summarizeRegistrationTransform(result);
            m_workspaceApplicationService->recordRegistrationState(registrationState);
            m_workspaceApplicationService->persistSnapshot();
        }
        NavigationEvaluationService evaluationService(evaluationCasesRoot());
        AnkleRegistrationRecord record;
        record.caseId = caseId;
        record.registrationMode = result.metrics.value(QStringLiteral("registration_mode")).toString();
        record.fre = result.rmsError;
        record.targetTre = result.targetRegionTre;
        record.coverageScore = result.coverageScore;
        record.metrics = result.metrics;
        evaluationService.saveRegistrationRecord(record);
        persistEvaluationReportSnapshot();
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
    m_registrationTransform = QMatrix4x4();
    if (m_runtimeCoordinator) {
        m_runtimeCoordinator->clearRegistrationTransform();
    }
    refreshNavigationConfidenceState();
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
    if (!m_navigationActive || !m_navigation3DView) {
        return;
    }

    refreshNavigationConfidenceState(true);
    if (!m_lastConfidence.allowNavigation) {
        on_pauseNavigationButton_clicked();
        return;
    }

    QVector3D trackingPos;
    QList<double> trackingPose;
    bool hasLiveTracking = false;

    if (auto* trackingService = opticalTrackingService();
        trackingService && !m_trackingSessionId.isEmpty() && !m_navigationToolId.isEmpty()) {
        const QMap<QString, QList<double>> realTimeData = trackingService->getRealTimeData(m_trackingSessionId);
        if (realTimeData.contains(m_navigationToolId)) {
            trackingPose = realTimeData.value(m_navigationToolId);
            if (trackingPose.size() >= 3) {
                trackingPos = QVector3D(
                    static_cast<float>(trackingPose[0]),
                    static_cast<float>(trackingPose[1]),
                    static_cast<float>(trackingPose[2]));
                hasLiveTracking = true;

                const QVariantMap trackingQuality =
                    trackingService->checkTrackingQuality(m_trackingSessionId, m_navigationToolId);
                updateAccuracyDisplay(trackingQuality.value(QStringLiteral("tracking_jitter_mm")).toDouble());
            }
        }
    }

    if (!hasLiveTracking) {
        if (!m_motionSimulator) {
            return;
        }

        trackingPos = m_motionSimulator->getCurrentPosition();
        trackingPose = QList<double> {
            trackingPos.x(),
            trackingPos.y(),
            trackingPos.z(),
            0.0,
            0.0,
            0.0
        };
        static double simAccuracy = 0;
        static int frameCount = 0;
        frameCount++;
        simAccuracy = 0.8 + 0.3 * std::sin(frameCount * 0.05);
        updateAccuracyDisplay(simAccuracy);
    }

    const QVector3D bonePos = m_registrationTransform.map(trackingPos);
    updatePositionDisplay(bonePos.x(), bonePos.y(), bonePos.z());
    refreshNavigationConfidenceState();
    pushSimulatedPoseFrameToRuntime(trackingPose);
    refreshRealtimeDigitalTwin();
}

void NavigationPageNew::pushSimulatedPoseFrameToRuntime(const QList<double>& trackingPose)
{
    if (!m_runtimeCoordinator || !m_runtimeState) {
        return;
    }
    if (trackingPose.size() < 3) {
        m_runtimeCoordinator->clearPoseTrackingState();
        return;
    }

    NavigationPoseFrame frame;
    frame.sourceId = QStringLiteral("simulator");
    frame.toolId = m_navigationToolId;
    frame.timestamp = QDateTime::currentDateTimeUtc();
    frame.trackingVisible = true;
    frame.trackingConfidence = 1.0;
    frame.trackingToMarker = poseMatrixFromTrackingData(trackingPose);

    m_runtimeCoordinator->handlePoseFrame(frame);
}

void NavigationPageNew::refreshRealtimeDigitalTwin()
{
    if (!m_runtimeCoordinator || !m_navigationVtkBridge || !m_runtimeState) {
        return;
    }

    const NavigationDisplayState displayState =
        m_runtimeCoordinator->buildDisplayState(activeBoneModelPaths(), activeInstrumentModelPath());

    m_navigationVtkBridge->loadBoneModels(displayState.boneModelPaths);
    m_navigationVtkBridge->loadInstrumentModel(displayState.activeToolId, displayState.activeToolModelPath);
    m_navigationVtkBridge->updateInstrumentPose(displayState.activeToolId, displayState.vtkToolTransform);
    m_navigationVtkBridge->setInstrumentVisible(displayState.activeToolId, displayState.toolVisible);
}

QStringList NavigationPageNew::activeBoneModelPaths() const
{
    QStringList paths;
    if (!m_workflowContext) {
        return paths;
    }

    const QString caseId = m_workflowContext->caseId();
    const QString casesRoot = evaluationCasesRoot();
    if (caseId.isEmpty() || casesRoot.isEmpty()) {
        return paths;
    }

    const QString dataRoot = QFileInfo(casesRoot).dir().absolutePath();
    const AnkleCaseWorkspaceRepository repository(dataRoot);
    const AnkleCaseManifest manifest = repository.loadManifest(caseId);
    const AnkleCaseAssetBindings bindings = repository.loadCaseAssetBindings(caseId);
    const QString caseRoot = repository.caseRoot(caseId);

    for (const AnkleModelAsset& asset : manifest.modelAssets) {
        const QString assetBoneId = QStringLiteral("bone:%1").arg(asset.boneName);
        if (bindings.activeBoneAssetIds.contains(assetBoneId) && !asset.normalizedPath.isEmpty()) {
            paths.append(resolvedCaseAssetPath(caseRoot, QDir(caseRoot).filePath(asset.normalizedPath)));
        }
    }

    if (paths.isEmpty() && !m_lastLoadedModelPath.isEmpty()) {
        paths.append(m_lastLoadedModelPath);
    }

    return paths;
}

QString NavigationPageNew::activeInstrumentModelPath() const
{
    if (!m_workflowContext || m_selectedInstrumentId < 0) {
        return QString();
    }
    auto* instrumentService = instrumentManagementService();
    if (!instrumentService) {
        return QString();
    }

    const InstrumentItem instrument = instrumentService->getInstrument(m_selectedInstrumentId);
    return instrument.modelFilePath;
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

