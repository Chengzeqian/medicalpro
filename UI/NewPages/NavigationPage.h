#ifndef NAVIGATIONPAGE_NEW_H
#define NAVIGATIONPAGE_NEW_H

#include "BasePage.h"
#include "PageIndex.h"

#include <QTimer>
#include <QPointer>
#include <QEvent>
#include <QMatrix4x4>

// 包含配准数据结构（需要 RegistrationSessionState 枚举）
#ifdef CTK_PLUGIN_FRAMEWORK
#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"
#endif

namespace Ui {
class NavigationPage;
}

// 前向声明
class Navigation3DViewWidget;
class BoneSurfaceMotionSimulator;

#ifdef CTK_PLUGIN_FRAMEWORK
class FourViewDisplayService;
class OpticalTrackingService;
class PointRegistrationService;
class RegistrationWorkflow;
#endif

#ifndef CTK_PLUGIN_FRAMEWORK
// 当CTK未启用时，提供空的枚举和结构体定义
enum class RegistrationSessionState { Idle, ModelLoading, PointCollection, Computing, Completed, Failed };

struct PointRegistrationResult {
    bool success = false;
    double rmsError = 0.0;
    double maxError = 0.0;
    double meanError = 0.0;
};
#else
struct PointRegistrationResult;
#endif

/**
 * @brief 手术导航页面
 *
 * 功能：
 * - 器械管理 Tab
 * - 术前规划 Tab（分割、假体规划）
 * - 配准 Tab（2D-3D配准、点配准、光学配准）
 * - 导航 Tab（四视图、跟踪器连接、精度监控）
 */
class NavigationPageNew : public BasePage
{
    Q_OBJECT

public:
    explicit NavigationPageNew(QWidget* parent = nullptr);
    ~NavigationPageNew();

    void onActivated() override;
    void onDeactivated() override;

    void setPatientId(int patientId);
    void setPatientName(const QString& name);

signals:
    void backToMainRequested();  // MainInterfaceWidget期望的信号（返回Dashboard）

private slots:
    // 导航
    void on_backButton_clicked();

    // 器械管理
    void on_importInstrumentButton_clicked();
    void on_deleteInstrumentButton_clicked();
    void on_refreshInstrumentButton_clicked();
    void on_clearAllInstrumentButton_clicked();
    void on_generateThumbnailButton_clicked();

    // 术前规划
    void on_loadDicomButton_clicked();
    void on_autoSegmentButton_clicked();
    void on_manualSegmentButton_clicked();
    void on_exportSTLButton_clicked();
    void on_selectProsthesisButton_clicked();
    void on_adjustProsthesisButton_clicked();
    void on_loadModelButton_clicked();
    void on_toggleModelButton_clicked();

    // 配准
    void on_load2DImageButton_clicked();
    void on_start2D3DRegButton_clicked();
    void on_collectPointButton_clicked();
    void on_computeRegButton_clicked();
    void on_calibrateButton_clicked();
    void on_deletePointButton_clicked();
    void on_clearAllPointsButton_clicked();

    // 导航控制
    void on_connectTrackerButton_clicked();
    void on_disconnectTrackerButton_clicked();
    void on_startNavigationButton_clicked();
    void on_pauseNavigationButton_clicked();
    void on_resetViewButton_clicked();

    // 跟踪器数据更新
    void onTrackerDataReceived();

    // 实时导航定时器回调
    void onNavigationTimerUpdate();

    // 导航3D视图骨骼模型加载完成回调
    void onNavigation3DBoneLoaded(bool success, const QVector3D& center, const QVector3D& size);

    // 器械卡片点击
    void onInstrumentCardClicked(int instrumentId);

    // 分割任务回调
    void onSegmentationProgress(const QString& taskId, int progress, const QString& message);
    void onSegmentationCompleted(const QString& taskId, const QVariantMap& result);
    void onSegmentationFailed(const QString& taskId, const QString& error);

    // 配准工作流回调
    void onRegistrationStateChanged(RegistrationSessionState state);
    void onRegistrationProgressUpdated(int progress, const QString& message);
    void onRegistrationModelLoaded(bool success, const QString& info);
    void onRegistrationPointAdded(int index, const QVector3D& position);
    void onRegistrationProbePointCaptured(int index, const QVector3D& position);
    void onRegistrationCompleted(const PointRegistrationResult& result);
    void onRegistrationFailed(const QString& error);
    void onRegistrationPointPicked(double x, double y, double z);

public:
    // 兼容旧 MainInterfaceWidget 调用的空实现
    void beginTransitionMask();
    void endTransitionMask();
    void resetPage();

protected:
    // 事件过滤器，用于处理器械卡片点击
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void loadInstruments();
    void setupVTKViews();
    void cleanupVTKViews();
    void embedFourViewWidget();
    void updateFourViewWidgetPlacement();
    void updateTrackerStatus(bool connected);
    void updatePositionDisplay(double x, double y, double z);
    void updateAccuracyDisplay(double accuracy);

    // 配准相关
    void setupRegistration();
    void embedRegistrationVTKWidget();
    void updateRegistrationPointsList();
    void updateRegistrationResultDisplay(const PointRegistrationResult& result);

#ifdef CTK_PLUGIN_FRAMEWORK
    bool ensurePointRegistrationService(bool tryStartPlugin = true);
#endif

    Ui::NavigationPage* ui;
    int m_patientId;
    QString m_patientName;
    bool m_trackerConnected;
    bool m_navigationActive;
    QString m_lastDicomDirPath;

    // VTK四视图Widget
    QPointer<QWidget> m_fourViewWidget;

    // 跟踪器更新定时器
    QTimer* m_trackerTimer;

    // 当前分割任务ID
    QString m_currentSegmentationTaskId;

    // 分割结果信息（用于导出STL）
    QString m_lastSegmentationTaskId;
    QString m_lastSegmentationOutputDir;

    // 3D模型显示状态
    bool m_modelVisible;
    QString m_lastLoadedModelPath;  // 规划页面加载的模型路径（用于共享到配准页面）

    // 3D视图相关
    QPointer<QWidget> m_registrationVTKWidget;  // 配准3D视图
    int m_selectedPointIndex;                     // 当前选中的配准点索引

    // 实时导航相关
    Navigation3DViewWidget* m_navigation3DView;      // 导航3D视图
    BoneSurfaceMotionSimulator* m_motionSimulator;   // 模拟探针运动
    QTimer* m_navigationTimer;                        // 导航更新定时器
    QMatrix4x4 m_registrationTransform;              // 配准变换矩阵缓存

#ifdef CTK_PLUGIN_FRAMEWORK
    // 配准工作流
    RegistrationWorkflow* m_registrationWorkflow;
    FourViewDisplayService* m_fourViewService;
    OpticalTrackingService* m_trackingService;
    PointRegistrationService* m_pointRegistrationService;
#endif
};

#endif // NAVIGATIONPAGE_NEW_H
