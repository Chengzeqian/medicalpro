#ifndef REGISTRATION2D3D_SERVICE_IMPL_H
#define REGISTRATION2D3D_SERVICE_IMPL_H

/**
 * @file Registration2D3DServiceImpl.h
 * @brief 2D3D配准服务实现类
 * 
 * 实现Registration2D3DService接口的所有功能
 * 集成Python配准算法、多线程任务管理、结果存储
 */

#include "Registration2D3DService.h"
#include <ctkPluginContext.h>

// 解决 Qt 和 Python 的宏冲突
// Qt 定义了 slots/signals 等宏，Python.h 中使用了 slots 作为变量名
#ifdef slots
#undef slots
#endif
#ifdef signals
#undef signals
#endif

#include <Python.h>

// 恢复 Qt 宏定义
#define slots Q_SLOTS
#define signals Q_SIGNALS

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QMap>
#include <QSqlDatabase>

// 前向声明
class Registration2D3DWorker;

/**
 * @brief 2D3D配准服务实现类
 */
class Registration2D3DServiceImpl : public Registration2D3DService
{
    Q_OBJECT
    Q_INTERFACES(Registration2D3DService)
    
public:
    explicit Registration2D3DServiceImpl(QObject* parent = nullptr);
    virtual ~Registration2D3DServiceImpl();
    
    // ========== 配准执行 ==========
    QString startRegistration(const Registration2D3DParameters& params) override;
    bool cancelRegistration(const QString& registrationId) override;
    bool executeRegistrationSync(const Registration2D3DParameters& params, 
                                 Registration2D3DResult& result) override;
    
    // ========== 结果查询 ==========
    Registration2D3DResult getRegistrationResult(const QString& registrationId) override;
    QList<Registration2D3DResult> getRegistrationHistory() override;
    QList<Registration2D3DResult> getRegistrationHistoryByPatient(const QString& patientId) override;
    bool deleteRegistration(const QString& registrationId) override;
    
    // ========== 统计信息 ==========
    Registration2D3DStatistics getStatistics() override;
    
    // ========== Python环境管理 ==========
    bool initializePythonEnvironment(const QString& pythonHome, 
                                    const QString& scriptsPath) override;
    bool isPythonInitialized() override;
    void finalizePythonEnvironment() override;
    
    // ========== 配置管理 ==========
    void setConfiguration(const QString& key, const QVariant& value) override;
    QVariant getConfiguration(const QString& key, const QVariant& defaultValue = QVariant()) override;
    
    // ========== 工具方法 ==========
    bool validateParameters(const Registration2D3DParameters& params, 
                           QString& errorMessage) override;
    bool generateDRRPreview(const QString& ctPath, 
                           const QVector<double>& params,
                           const QString& view,
                           const QString& outputPath) override;
    QString getLastError() const override;
    
    // ========== UI 组件工厂实现 ==========
    QWidget* createRegistrationWidget(QWidget* parent = nullptr) override;

    // ========== VTK渲染控制 ==========
    void pauseRendering() override;
    void resumeRendering() override;

    // ========== CTK Context 管理 ==========
    void setContext(ctkPluginContext* context) { m_context = context; }
    
private slots:
    // 工作线程信号处理
    void onWorkerProgressUpdated(const QString& registrationId, 
                                const Registration2D3DProgress& progress);
    void onWorkerCompleted(const QString& registrationId, 
                          const Registration2D3DResult& result);
    void onWorkerFailed(const QString& registrationId, 
                       const QString& errorMessage);
    
private:
    // CTK Context
    ctkPluginContext* m_context;
    
    // Python环境
    bool m_pythonInitialized;
    QString m_pythonHome;
    QString m_scriptsPath;
    mutable QMutex m_pythonMutex;
    PyThreadState* m_mainThreadState;  // 主线程Python状态（用于多线程GIL管理）
    
    // 数据库
    QSqlDatabase m_database;
    mutable QMutex m_dbMutex;
    
    // 配准任务管理
    QMap<QString, Registration2D3DWorker*> m_activeWorkers;
    QMap<QString, Registration2D3DResult> m_resultCache;
    mutable QMutex m_workerMutex;
    
    // 配置
    QMap<QString, QVariant> m_configuration;
    mutable QMutex m_configMutex;
    
    // 错误信息
    mutable QString m_lastError;

    // VTK渲染控制状态
    bool m_renderingPaused;
    QList<QWidget*> m_createdWidgets;

    // 辅助方法
    QString generateRegistrationId() const;
    bool initializeDatabase();
    bool saveResultToDatabase(const Registration2D3DResult& result);
    Registration2D3DResult loadResultFromDatabase(const QString& registrationId);
    void logMessage(const QString& level, const QString& message) const;
    QString getProjectPath() const;
    
    // Python调用核心方法
    bool callPythonRegistration(const Registration2D3DParameters& params,
                               Registration2D3DResult& result);
    bool setupPythonPath();
};

/**
 * @brief 配准工作线程
 * 
 * 在后台线程执行Python配准算法
 */
class Registration2D3DWorker : public QThread
{
    Q_OBJECT
    
public:
    explicit Registration2D3DWorker(const QString& registrationId,
                                    const Registration2D3DParameters& params,
                                    QObject* parent = nullptr);
    ~Registration2D3DWorker() override;
    
    void run() override;
    void cancel();
    
    Registration2D3DResult getResult() const { return m_result; }
    
signals:
    void progressUpdated(const QString& registrationId, 
                        const Registration2D3DProgress& progress);
    void completed(const QString& registrationId, 
                  const Registration2D3DResult& result);
    void failed(const QString& registrationId, 
               const QString& errorMessage);
    
private:
    QString m_registrationId;
    Registration2D3DParameters m_parameters;
    Registration2D3DResult m_result;
    bool m_cancelled;
    mutable QMutex m_mutex;
    
    // Python C API调用封装
    bool executePythonRegistration();
    void emitProgress(const QString& phase, int percentage, const QString& message);
    
    // Python全局锁管理
    class PyThreadStateLock {
    public:
        PyThreadStateLock() {
            m_state = PyGILState_Ensure();
        }
        ~PyThreadStateLock() {
            PyGILState_Release(m_state);
        }
    private:
        PyGILState_STATE m_state;
    };
};

#endif // REGISTRATION2D3D_SERVICE_IMPL_H

