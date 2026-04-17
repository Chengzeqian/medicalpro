/**
 * @file USAGE_EXAMPLE.cpp
 * @brief Registration2D3D插件使用示例
 * 
 * 本文件展示如何在您的应用中使用2D3D配准服务
 * 
 * 注意：这是一个示例文件，不会被编译到插件中
 */

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QProgressBar>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <ctkPluginContext.h>
#include <ctkServiceReference.h>
#include "Registration2D3DService.h"

/**
 * @brief 示例1：最简单的配准调用
 */
class SimpleRegistrationExample : public QWidget
{
    Q_OBJECT
public:
    SimpleRegistrationExample(ctkPluginContext* context, QWidget* parent = nullptr)
        : QWidget(parent), m_context(context)
    {
        setupUI();
        setupService();
    }
    
private slots:
    void onStartRegistration()
    {
        // 准备参数
        Registration2D3DParameters params;
        params.ctPath = m_ctPathEdit->text();
        params.xrayApPath = m_xrayApEdit->text();
        params.xrayLatPath = m_xrayLatEdit->text();
        
        // 使用默认参数
        params.initParams = {0, 0, 0, 0, 0, 0};
        params.searchRange = {15, 15, 15, 50, 50, 50};
        params.kdTreeNum = 50;
        
        // 启动配准
        QString regId = m_regService->startRegistration(params);
        if (regId.isEmpty()) {
            QMessageBox::critical(this, "错误", 
                "启动配准失败: " + m_regService->getLastError());
        } else {
            m_logEdit->append("配准已启动，ID: " + regId);
            m_startButton->setEnabled(false);
        }
    }
    
    void onProgressUpdated(const QString& id, const Registration2D3DProgress& progress)
    {
        m_progressBar->setValue(progress.percentage);
        m_logEdit->append(QString("[%1%] %2: %3")
            .arg(progress.percentage)
            .arg(progress.currentPhase)
            .arg(progress.message));
    }
    
    void onRegistrationCompleted(const QString& id, const Registration2D3DResult& result)
    {
        m_progressBar->setValue(100);
        m_startButton->setEnabled(true);
        
        QString resultText = QString(
            "配准完成！\n"
            "耗时: %1 秒\n"
            "\nAP视角结果:\n"
            "  旋转: rx=%2°, ry=%3°, rz=%4°\n"
            "  平移: tx=%5mm, ty=%6mm, tz=%7mm\n"
            "  度量值: %8\n"
            "\nLAT视角结果:\n"
            "  旋转: rx=%9°, ry=%10°, rz=%11°\n"
            "  平移: tx=%12mm, ty=%13mm, tz=%14mm\n"
            "  度量值: %15\n"
        )
        .arg(result.durationSeconds)
        .arg(result.apResult.rx, 0, 'f', 2)
        .arg(result.apResult.ry, 0, 'f', 2)
        .arg(result.apResult.rz, 0, 'f', 2)
        .arg(result.apResult.tx, 0, 'f', 2)
        .arg(result.apResult.ty, 0, 'f', 2)
        .arg(result.apResult.tz, 0, 'f', 2)
        .arg(result.apResult.goMetric, 0, 'f', 4)
        .arg(result.latResult.rx, 0, 'f', 2)
        .arg(result.latResult.ry, 0, 'f', 2)
        .arg(result.latResult.rz, 0, 'f', 2)
        .arg(result.latResult.tx, 0, 'f', 2)
        .arg(result.latResult.ty, 0, 'f', 2)
        .arg(result.latResult.tz, 0, 'f', 2)
        .arg(result.latResult.goMetric, 0, 'f', 4);
        
        QMessageBox::information(this, "配准完成", resultText);
        m_logEdit->append(resultText);
    }
    
    void onRegistrationFailed(const QString& id, const QString& error)
    {
        m_progressBar->setValue(0);
        m_startButton->setEnabled(true);
        QMessageBox::critical(this, "配准失败", error);
        m_logEdit->append("配准失败: " + error);
    }
    
private:
    void setupUI()
    {
        QVBoxLayout* layout = new QVBoxLayout(this);
        
        // CT路径
        m_ctPathEdit = new QLineEdit(this);
        m_ctPathEdit->setPlaceholderText("CT图像路径 (.nrrd/.mhd/.nii)");
        QPushButton* ctBrowse = new QPushButton("浏览CT...", this);
        connect(ctBrowse, &QPushButton::clicked, [this]() {
            QString path = QFileDialog::getOpenFileName(this, "选择CT图像",
                "", "医学图像 (*.nrrd *.mhd *.nii *.nii.gz)");
            if (!path.isEmpty()) m_ctPathEdit->setText(path);
        });
        
        // AP X射线
        m_xrayApEdit = new QLineEdit(this);
        m_xrayApEdit->setPlaceholderText("AP视角X射线图像");
        QPushButton* apBrowse = new QPushButton("浏览AP...", this);
        connect(apBrowse, &QPushButton::clicked, [this]() {
            QString path = QFileDialog::getOpenFileName(this, "选择AP X射线",
                "", "图像 (*.png *.jpg *.bmp *.tif)");
            if (!path.isEmpty()) m_xrayApEdit->setText(path);
        });
        
        // LAT X射线
        m_xrayLatEdit = new QLineEdit(this);
        m_xrayLatEdit->setPlaceholderText("LAT视角X射线图像");
        QPushButton* latBrowse = new QPushButton("浏览LAT...", this);
        connect(latBrowse, &QPushButton::clicked, [this]() {
            QString path = QFileDialog::getOpenFileName(this, "选择LAT X射线",
                "", "图像 (*.png *.jpg *.bmp *.tif)");
            if (!path.isEmpty()) m_xrayLatEdit->setText(path);
        });
        
        // 进度条
        m_progressBar = new QProgressBar(this);
        m_progressBar->setRange(0, 100);
        
        // 开始按钮
        m_startButton = new QPushButton("开始配准", this);
        connect(m_startButton, &QPushButton::clicked, 
                this, &SimpleRegistrationExample::onStartRegistration);
        
        // 日志
        m_logEdit = new QTextEdit(this);
        m_logEdit->setReadOnly(true);
        
        // 布局
        layout->addWidget(new QLabel("CT图像:"));
        layout->addWidget(m_ctPathEdit);
        layout->addWidget(ctBrowse);
        layout->addWidget(new QLabel("AP X射线:"));
        layout->addWidget(m_xrayApEdit);
        layout->addWidget(apBrowse);
        layout->addWidget(new QLabel("LAT X射线:"));
        layout->addWidget(m_xrayLatEdit);
        layout->addWidget(latBrowse);
        layout->addWidget(m_progressBar);
        layout->addWidget(m_startButton);
        layout->addWidget(new QLabel("日志:"));
        layout->addWidget(m_logEdit);
    }
    
    void setupService()
    {
        // 获取服务
        ctkServiceReference ref = m_context->getServiceReference<Registration2D3DService>();
        m_regService = m_context->getService<Registration2D3DService>(ref);
        
        if (!m_regService) {
            QMessageBox::critical(this, "错误", "无法获取2D3D配准服务");
            return;
        }
        
        // 连接信号
        connect(m_regService, &Registration2D3DService::progressUpdated,
                this, &SimpleRegistrationExample::onProgressUpdated);
        connect(m_regService, &Registration2D3DService::registrationCompleted,
                this, &SimpleRegistrationExample::onRegistrationCompleted);
        connect(m_regService, &Registration2D3DService::registrationFailed,
                this, &SimpleRegistrationExample::onRegistrationFailed);
    }
    
private:
    ctkPluginContext* m_context;
    Registration2D3DService* m_regService;
    
    QLineEdit* m_ctPathEdit;
    QLineEdit* m_xrayApEdit;
    QLineEdit* m_xrayLatEdit;
    QProgressBar* m_progressBar;
    QPushButton* m_startButton;
    QTextEdit* m_logEdit;
};

/**
 * @brief 示例2：高级配准（带参数调整）
 */
class AdvancedRegistrationExample : public QWidget
{
    Q_OBJECT
public:
    AdvancedRegistrationExample(ctkPluginContext* context, QWidget* parent = nullptr)
        : QWidget(parent), m_context(context)
    {
        setupService();
        setupUI();
    }
    
private slots:
    void onStartRegistration()
    {
        // 准备高级参数
        Registration2D3DParameters params;
        
        // 文件路径
        params.ctPath = m_ctPath;
        params.xrayApPath = m_apPath;
        params.xrayLatPath = m_latPath;
        params.jingguPath = m_tibiaModelPath;
        
        // 初始配准参数（可以从UI读取）
        params.initParams = {
            m_rxSpin->value(),
            m_rySpin->value(),
            m_rzSpin->value(),
            m_txSpin->value(),
            m_tySpin->value(),
            m_tzSpin->value()
        };
        
        // 搜索范围
        params.searchRange = {
            m_rxRangeSpin->value(),
            m_ryRangeSpin->value(),
            m_rzRangeSpin->value(),
            m_txRangeSpin->value(),
            m_tyRangeSpin->value(),
            m_tzRangeSpin->value()
        };
        
        // 优化参数
        params.kdTreeNum = m_kdTreeSpin->value();
        
        // 图像翻转选项
        params.apUpDown = m_apUDCheck->isChecked();
        params.apHorizontal = m_apHorCheck->isChecked();
        params.latUpDown = m_latUDCheck->isChecked();
        params.latHorizontal = m_latHorCheck->isChecked();
        
        // 输出设置
        params.generateDRR = m_generateDRRCheck->isChecked();
        params.outputDirectory = m_outputDir;
        
        // 验证参数
        QString errorMsg;
        if (!m_regService->validateParameters(params, errorMsg)) {
            QMessageBox::warning(this, "参数错误", errorMsg);
            return;
        }
        
        // 启动配准
        m_currentRegId = m_regService->startRegistration(params);
        if (m_currentRegId.isEmpty()) {
            QMessageBox::critical(this, "错误", 
                "启动配准失败: " + m_regService->getLastError());
        } else {
            m_statusLabel->setText("配准进行中...");
            m_startButton->setEnabled(false);
            m_cancelButton->setEnabled(true);
        }
    }
    
    void onCancelRegistration()
    {
        if (!m_currentRegId.isEmpty()) {
            bool cancelled = m_regService->cancelRegistration(m_currentRegId);
            if (cancelled) {
                m_statusLabel->setText("配准已取消");
                m_startButton->setEnabled(true);
                m_cancelButton->setEnabled(false);
            }
        }
    }
    
    void onViewHistory()
    {
        // 查看配准历史
        QList<Registration2D3DResult> history = m_regService->getRegistrationHistory();
        
        QString historyText;
        for (const auto& result : history) {
            historyText += QString("ID: %1\n")
                .arg(result.registrationId);
            historyText += QString("时间: %1\n")
                .arg(result.startTime.toString("yyyy-MM-dd HH:mm:ss"));
            historyText += QString("状态: %1\n")
                .arg(result.getStatusString());
            historyText += QString("耗时: %1秒\n")
                .arg(result.durationSeconds);
            historyText += "---\n";
        }
        
        QMessageBox::information(this, "配准历史", historyText);
    }
    
    void onViewStatistics()
    {
        // 查看统计信息
        Registration2D3DStatistics stats = m_regService->getStatistics();
        
        QString statsText = QString(
            "总配准次数: %1\n"
            "成功次数: %2\n"
            "失败次数: %3\n"
            "成功率: %4%\n"
            "平均耗时: %5秒\n"
            "平均度量值: %6\n"
        )
        .arg(stats.totalRegistrations)
        .arg(stats.successfulRegistrations)
        .arg(stats.failedRegistrations)
        .arg(stats.totalRegistrations > 0 ? 
             stats.successfulRegistrations * 100.0 / stats.totalRegistrations : 0, 0, 'f', 1)
        .arg(stats.averageDuration, 0, 'f', 1)
        .arg(stats.averageMetric, 0, 'f', 4);
        
        QMessageBox::information(this, "配准统计", statsText);
    }
    
private:
    void setupUI()
    {
        // UI实现略...
        // 包括：参数输入框、范围设置、开关选项等
    }
    
    void setupService()
    {
        ctkServiceReference ref = m_context->getServiceReference<Registration2D3DService>();
        m_regService = m_context->getService<Registration2D3DService>(ref);
        
        if (!m_regService) {
            QMessageBox::critical(this, "错误", "无法获取2D3D配准服务");
            return;
        }
        
        // 连接信号
        connect(m_regService, &Registration2D3DService::progressUpdated,
                this, [this](const QString& id, const Registration2D3DProgress& progress) {
            m_progressBar->setValue(progress.percentage);
            m_statusLabel->setText(progress.message);
        });
        
        connect(m_regService, &Registration2D3DService::registrationCompleted,
                this, [this](const QString& id, const Registration2D3DResult& result) {
            m_startButton->setEnabled(true);
            m_cancelButton->setEnabled(false);
            m_statusLabel->setText("配准完成！");
            
            // 显示结果
            showResult(result);
        });
    }
    
    void showResult(const Registration2D3DResult& result)
    {
        // 显示详细结果，包括验证图像等
        // 实现略...
    }
    
private:
    ctkPluginContext* m_context;
    Registration2D3DService* m_regService;
    QString m_currentRegId;
    
    // UI控件略...
    QString m_ctPath, m_apPath, m_latPath, m_tibiaModelPath, m_outputDir;
    QDoubleSpinBox *m_rxSpin, *m_rySpin, *m_rzSpin;
    QDoubleSpinBox *m_txSpin, *m_tySpin, *m_tzSpin;
    QSpinBox *m_rxRangeSpin, *m_ryRangeSpin, *m_rzRangeSpin;
    QSpinBox *m_txRangeSpin, *m_tyRangeSpin, *m_tzRangeSpin;
    QSpinBox* m_kdTreeSpin;
    QCheckBox *m_apUDCheck, *m_apHorCheck, *m_latUDCheck, *m_latHorCheck;
    QCheckBox* m_generateDRRCheck;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QPushButton *m_startButton, *m_cancelButton;
};

/**
 * @brief 示例3：批量配准
 */
class BatchRegistrationExample
{
public:
    BatchRegistrationExample(Registration2D3DService* service)
        : m_service(service)
    {
    }
    
    void processBatch(const QStringList& patientIds)
    {
        for (const QString& patientId : patientIds) {
            // 构建参数
            Registration2D3DParameters params;
            params.ctPath = QString("D:/data/%1/ct.nrrd").arg(patientId);
            params.xrayApPath = QString("D:/data/%1/xray_ap.png").arg(patientId);
            params.xrayLatPath = QString("D:/data/%1/xray_lat.png").arg(patientId);
            
            // 使用默认参数
            params.initParams = {0, 0, 0, 0, 0, 0};
            params.searchRange = {15, 15, 15, 50, 50, 50};
            params.kdTreeNum = 50;
            
            // 同步执行（批处理模式）
            Registration2D3DResult result;
            bool success = m_service->executeRegistrationSync(params, result);
            
            if (success) {
                qDebug() << "患者" << patientId << "配准成功";
                // 保存结果
                saveResultToFile(patientId, result);
            } else {
                qWarning() << "患者" << patientId << "配准失败:" 
                           << m_service->getLastError();
            }
        }
    }
    
private:
    void saveResultToFile(const QString& patientId, 
                         const Registration2D3DResult& result)
    {
        // 保存结果到CSV或JSON文件
        // 实现略...
    }
    
private:
    Registration2D3DService* m_service;
};

/**
 * @brief 示例4：如何在主窗口中集成
 */
void integrateIntoMainWindow()
{
    // 在MainInterfaceWidget或其他主界面类中：
    /*
    
    // 1. 添加成员变量
    Registration2D3DService* m_registrationService;
    
    // 2. 在构造函数中获取服务
    void MainInterfaceWidget::setupServices()
    {
        ctkServiceReference ref = 
            m_pluginContext->getServiceReference<Registration2D3DService>();
        m_registrationService = 
            m_pluginContext->getService<Registration2D3DService>(ref);
        
        if (m_registrationService) {
            qDebug() << "2D3D配准服务已连接";
        }
    }
    
    // 3. 创建菜单项或按钮
    void MainInterfaceWidget::createRegistrationMenu()
    {
        QAction* regAction = new QAction("2D3D配准", this);
        connect(regAction, &QAction::triggered, this, [this]() {
            // 显示配准对话框
            SimpleRegistrationExample* dialog = 
                new SimpleRegistrationExample(m_pluginContext, this);
            dialog->show();
        });
        
        // 添加到工具菜单
        m_toolsMenu->addAction(regAction);
    }
    
    // 4. 或者直接调用
    void MainInterfaceWidget::onRegistrationButtonClicked()
    {
        if (!m_registrationService) {
            QMessageBox::warning(this, "警告", "配准服务不可用");
            return;
        }
        
        // 准备参数并调用
        Registration2D3DParameters params;
        // ... 设置参数 ...
        
        QString regId = m_registrationService->startRegistration(params);
        // ... 处理结果 ...
    }
    
    */
}

#include "USAGE_EXAMPLE.moc"

