#ifndef PLUGIN_REGISTRATION2D3DWIDGET_H
#define PLUGIN_REGISTRATION2D3DWIDGET_H

/**
 * @file Registration2D3DWidget.h
 * @brief 2D3D配准Widget - 插件内部实现
 *
 * 设计原则：
 * - Widget在插件内部实现，避免主程序链接插件符号
 * - 通过服务接口的createRegistrationWidget()方法创建
 * - 信号/槽连接在插件内部完成
 */

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QGroupBox>
#include <QTableWidget>
#include <QCheckBox>
#include <QTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QTabWidget>
#include <QScrollArea>

#include "Registration2D3DDataStructures.h"

class Registration2D3DService;

/**
 * @brief 2D3D配准Widget（插件内部版本）
 *
 * 功能：
 * - 加载AP/LAT X光图像和CT图像
 * - 高级配准参数配置（初始参数、搜索范围、翻转选项）
 * - 实时配准进度显示
 * - 配准结果验证和可视化（棋盘格、边缘叠加）
 */
class Registration2D3DWidget : public QWidget
{
    Q_OBJECT

public:
    explicit Registration2D3DWidget(Registration2D3DService* service, QWidget* parent = nullptr);
    ~Registration2D3DWidget() override;

    void setPatientId(const QString& patientId);
    void loadXRayImages(const QString& apImagePath, const QString& latImagePath);
    void loadCTImage(const QString& ctPath);
    void loadBoneSegmentation(const QString& bonePath);

signals:
    void registrationStarted();
    void registrationCompleted(const Registration2D3DResult& result);
    void registrationFailed(const QString& errorMessage);

private slots:
    void onLoadAPImage();
    void onLoadLATImage();
    void onLoadCTImage();
    void onLoadBoneImage();
    void onStartRegistration();
    void onCancelRegistration();
    void onClearAll();
    void onResetParameters();
    void onShowResultImage(int index);

    // 服务信号响应
    void onServiceRegistrationStarted(const QString& registrationId);
    void onServiceProgressUpdated(const QString& registrationId, const Registration2D3DProgress& progress);
    void onServiceRegistrationCompleted(const QString& registrationId, const Registration2D3DResult& result);
    void onServiceRegistrationFailed(const QString& registrationId, const QString& errorMessage);

private:
    void setupUI();
    void setupControlPanel();
    void setupParameterPanel();
    void setupImagePanel();
    void setupResultPanel();
    void setupConnections();
    void updateUIState(bool registering);
    void loadResultImages(const Registration2D3DResult& result);
    Registration2D3DParameters collectParameters();

private:
    // 服务引用
    Registration2D3DService* m_service;

    // UI组件
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;

    // 控制按钮
    QPushButton* m_loadAPBtn;
    QPushButton* m_loadLATBtn;
    QPushButton* m_loadCTBtn;
    QPushButton* m_loadBoneBtn;
    QPushButton* m_startBtn;
    QPushButton* m_cancelBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_resetParamsBtn;

    // 基本参数设置
    QGroupBox* m_parameterGroup;
    QSpinBox* m_kdTreeNumSpin;
    QDoubleSpinBox* m_toleranceSpin;

    // 初始参数（6个）
    QDoubleSpinBox* m_initRx;
    QDoubleSpinBox* m_initRy;
    QDoubleSpinBox* m_initRz;
    QDoubleSpinBox* m_initTx;
    QDoubleSpinBox* m_initTy;
    QDoubleSpinBox* m_initTz;

    // 搜索范围（6个）
    QSpinBox* m_rangeRx;
    QSpinBox* m_rangeRy;
    QSpinBox* m_rangeRz;
    QSpinBox* m_rangeTx;
    QSpinBox* m_rangeTy;
    QSpinBox* m_rangeTz;

    // 翻转选项
    QCheckBox* m_apUpDownCheck;
    QCheckBox* m_apHorizontalCheck;
    QCheckBox* m_latUpDownCheck;
    QCheckBox* m_latHorizontalCheck;
    QCheckBox* m_generateDRRCheck;

    // 图像显示
    QLabel* m_apImageView;
    QLabel* m_latImageView;
    QLabel* m_ctImageView;
    QLabel* m_resultImageView;

    // 结果图像选择
    QComboBox* m_resultImageCombo;
    QLabel* m_resultAPDRR;
    QLabel* m_resultAPChecker;
    QLabel* m_resultAPEdge;
    QLabel* m_resultLATDRR;
    QLabel* m_resultLATChecker;
    QLabel* m_resultLATEdge;

    // 进度显示
    QGroupBox* m_progressGroup;      // 配准进度分组框
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_phaseLabel;
    QLabel* m_viewLabel;
    QTextEdit* m_logTextEdit;

    // 结果显示
    QGroupBox* m_resultGroup;        // 配准结果分组框
    QTableWidget* m_resultTable;
    QLabel* m_finalMetricLabel;
    QLabel* m_durationLabel;

    // 数据
    QString m_patientId;
    QString m_apImagePath;
    QString m_latImagePath;
    QString m_ctImagePath;
    QString m_boneImagePath;
    QString m_currentRegistrationId;
    bool m_isRegistering;
    Registration2D3DResult m_lastResult;
};

#endif // PLUGIN_REGISTRATION2D3DWIDGET_H

