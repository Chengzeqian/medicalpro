#include "Registration2D3DServiceImpl.h"
#include "Registration2D3DWidget.h"
#include <QDebug>
#include <QUuid>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QStandardPaths>

#ifdef _WIN32
#include <windows.h>
#endif

// ========================================================================
// Registration2D3DServiceImpl 实现
// ========================================================================

Registration2D3DServiceImpl::Registration2D3DServiceImpl(QObject* parent)
    : Registration2D3DService(parent)
    , m_context(nullptr)
    , m_pythonInitialized(false)
    , m_mainThreadState(nullptr)
    , m_renderingPaused(false)
{
    qDebug() << "[Registration2D3D] Service instance created";

    // 注册元类型，支持跨线程信号槽传递
    qRegisterMetaType<Registration2D3DProgress>("Registration2D3DProgress");
    qRegisterMetaType<Registration2D3DResult>("Registration2D3DResult");

    // 初始化数据库
    initializeDatabase();

    // 设置默认配置
    m_configuration["kdTreeNum"] = 50;
    m_configuration["maxIterations"] = 10;
    m_configuration["tolerance"] = 0.001;
    m_configuration["optimizationMethod"] = "CMA-ES";
}

Registration2D3DServiceImpl::~Registration2D3DServiceImpl()
{
    qDebug() << "[Registration2D3D] Service instance destroyed";
    
    // 取消所有活动任务
    QMutexLocker locker(&m_workerMutex);
    for (auto worker : m_activeWorkers) {
        worker->cancel();
        worker->wait();
        delete worker;
    }
    m_activeWorkers.clear();
    
    // 清理Python环境
    if (m_pythonInitialized) {
        finalizePythonEnvironment();
    }
}

// ========== 配准执行 ==========

QString Registration2D3DServiceImpl::startRegistration(const Registration2D3DParameters& params)
{
    // 验证参数
    QString errorMsg;
    if (!validateParameters(params, errorMsg)) {
        m_lastError = errorMsg;
        qWarning() << "[Registration2D3D] Parameter validation failed:" << errorMsg;
        return QString();
    }

    // 检查Python环境，如果延迟初始化则在此时初始化
    if (!m_pythonInitialized) {
        // 检查是否配置了延迟初始化
        bool deferred = getConfiguration("pythonInitDeferred", false).toBool();
        if (deferred) {
            QString pythonHome = getConfiguration("pythonHome", QString()).toString();
            QString scriptsPath = getConfiguration("scriptsPath", QString()).toString();

            qDebug() << "[Registration2D3D] Running deferred Python environment initialization...";
            if (!initializePythonEnvironment(pythonHome, scriptsPath)) {
                m_lastError = "Python environment initialization failed: " + m_lastError;
                qWarning() << "[Registration2D3D]" << m_lastError;
                return QString();
            }
        } else {
            m_lastError = "Python environment not initialized";
            qWarning() << "[Registration2D3D]" << m_lastError;
            return QString();
        }
    }
    
    // 生成配准ID
    QString registrationId = generateRegistrationId();

    // 创建工作线程（不设置父对象，避免线程问题）
    Registration2D3DWorker* worker = new Registration2D3DWorker(registrationId, params, nullptr);

    // 连接信号（使用 Qt::QueuedConnection 确保跨线程安全）
    connect(worker, &Registration2D3DWorker::progressUpdated,
            this, &Registration2D3DServiceImpl::onWorkerProgressUpdated, Qt::QueuedConnection);
    connect(worker, &Registration2D3DWorker::completed,
            this, &Registration2D3DServiceImpl::onWorkerCompleted, Qt::QueuedConnection);
    connect(worker, &Registration2D3DWorker::failed,
            this, &Registration2D3DServiceImpl::onWorkerFailed, Qt::QueuedConnection);
    connect(worker, &Registration2D3DWorker::finished,
            worker, &QObject::deleteLater);

    // 添加到活动任务列表
    {
        QMutexLocker locker(&m_workerMutex);
        m_activeWorkers[registrationId] = worker;
    }

    // 启动线程
    worker->start();

    qDebug() << "[Registration2D3D] Registration task started:" << registrationId;
    emit registrationStarted(registrationId);
    
    return registrationId;
}

bool Registration2D3DServiceImpl::cancelRegistration(const QString& registrationId)
{
    QMutexLocker locker(&m_workerMutex);
    
    if (!m_activeWorkers.contains(registrationId)) {
        m_lastError = "Registration task not found or already completed";
        return false;
    }
    
    Registration2D3DWorker* worker = m_activeWorkers[registrationId];
    worker->cancel();
    
    qDebug() << "[Registration2D3D] Registration task cancelled:" << registrationId;
    emit registrationCancelled(registrationId);
    
    return true;
}

bool Registration2D3DServiceImpl::executeRegistrationSync(
    const Registration2D3DParameters& params,
    Registration2D3DResult& result)
{
    // 验证参数
    QString errorMsg;
    if (!validateParameters(params, errorMsg)) {
        m_lastError = errorMsg;
        return false;
    }

    // 检查Python环境，如果延迟初始化则在此时初始化
    if (!m_pythonInitialized) {
        bool deferred = getConfiguration("pythonInitDeferred", false).toBool();
        if (deferred) {
            QString pythonHome = getConfiguration("pythonHome", QString()).toString();
            QString scriptsPath = getConfiguration("scriptsPath", QString()).toString();

            qDebug() << "[Registration2D3D] Running deferred Python environment initialization...";
            if (!initializePythonEnvironment(pythonHome, scriptsPath)) {
                m_lastError = "Python environment initialization failed: " + m_lastError;
                return false;
            }
        } else {
            m_lastError = "Python environment not initialized";
            return false;
        }
    }

    // 直接调用Python配准
    return callPythonRegistration(params, result);
}

// ========== 结果查询 ==========

Registration2D3DResult Registration2D3DServiceImpl::getRegistrationResult(const QString& registrationId)
{
    // 先检查缓存
    {
        QMutexLocker locker(&m_workerMutex);
        if (m_resultCache.contains(registrationId)) {
            return m_resultCache[registrationId];
        }
    }
    
    // 从数据库加载
    return loadResultFromDatabase(registrationId);
}

QList<Registration2D3DResult> Registration2D3DServiceImpl::getRegistrationHistory()
{
    QList<Registration2D3DResult> results;
    
    QMutexLocker locker(&m_dbMutex);
    
    QSqlQuery query(m_database);
    query.prepare("SELECT registration_id FROM registration_2d3d ORDER BY start_time DESC");
    
    if (!query.exec()) {
        m_lastError = "Failed to query registration history: " + query.lastError().text();
        return results;
    }
    
    while (query.next()) {
        QString id = query.value(0).toString();
        results.append(loadResultFromDatabase(id));
    }
    
    return results;
}

QList<Registration2D3DResult> Registration2D3DServiceImpl::getRegistrationHistoryByPatient(
    const QString& patientId)
{
    QList<Registration2D3DResult> results;
    
    QMutexLocker locker(&m_dbMutex);
    
    QSqlQuery query(m_database);
    query.prepare("SELECT registration_id FROM registration_2d3d WHERE patient_id = ? ORDER BY start_time DESC");
    query.addBindValue(patientId);
    
    if (!query.exec()) {
        m_lastError = "Failed to query patient registration history: " + query.lastError().text();
        return results;
    }
    
    while (query.next()) {
        QString id = query.value(0).toString();
        results.append(loadResultFromDatabase(id));
    }
    
    return results;
}

bool Registration2D3DServiceImpl::deleteRegistration(const QString& registrationId)
{
    QMutexLocker locker(&m_dbMutex);
    
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM registration_2d3d WHERE registration_id = ?");
    query.addBindValue(registrationId);
    
    if (!query.exec()) {
        m_lastError = "Failed to delete registration record: " + query.lastError().text();
        return false;
    }
    
    // 从缓存中移除
    m_resultCache.remove(registrationId);
    
    qDebug() << "[Registration2D3D] Registration record deleted:" << registrationId;
    return true;
}

// ========== 统计信息 ==========

Registration2D3DStatistics Registration2D3DServiceImpl::getStatistics()
{
    Registration2D3DStatistics stats;
    
    QMutexLocker locker(&m_dbMutex);
    
    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT 
            COUNT(*) as total,
            SUM(CASE WHEN status = 'completed' THEN 1 ELSE 0 END) as successful,
            SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed,
            AVG(CASE WHEN status = 'completed' THEN duration_seconds ELSE NULL END) as avg_duration,
            AVG(CASE WHEN status = 'completed' THEN final_metric ELSE NULL END) as avg_metric
        FROM registration_2d3d
    )");
    
    if (query.exec() && query.next()) {
        stats.totalRegistrations = query.value(0).toInt();
        stats.successfulRegistrations = query.value(1).toInt();
        stats.failedRegistrations = query.value(2).toInt();
        stats.averageDuration = query.value(3).toDouble();
        stats.averageMetric = query.value(4).toDouble();
    }
    
    return stats;
}

// ========== Python环境管理 ==========

bool Registration2D3DServiceImpl::initializePythonEnvironment(
    const QString& pythonHome, 
    const QString& scriptsPath)
{
    QMutexLocker locker(&m_pythonMutex);
    
    if (m_pythonInitialized) {
        qWarning() << "[Registration2D3D] Python environment already initialized";
        return true;
    }
    
    m_pythonHome = pythonHome;
    m_scriptsPath = scriptsPath;

    try {
        // 【关键】在Python初始化之前设置环境变量
        // 这些必须在任何BLAS/OpenBLAS库加载之前设置
        qDebug() << "[Registration2D3D] Configuring BLAS environment variables in C++ before Python initialization...";
        qputenv("OMP_NUM_THREADS", "1");
        qputenv("OPENBLAS_NUM_THREADS", "1");
        qputenv("MKL_NUM_THREADS", "1");
        qputenv("NUMEXPR_NUM_THREADS", "1");
        qputenv("VECLIB_MAXIMUM_THREADS", "1");
        qputenv("KMP_INIT_AT_FORK", "FALSE");
        qputenv("MPLBACKEND", "Agg");  // matplotlib使用非GUI后端

        auto normalizeForEnv = [](const QString& path) -> QString {
            return QDir::toNativeSeparators(QDir::cleanPath(path));
        };

        auto resolveDefaultPythonHome = [&]() -> QString {
            // 直接使用项目目录下的 Python39
            const QString projectPython39 = "D:/Qtproject/medicalpro/Python39";
            qDebug() << "[Registration2D3D] Using project-local Python39 runtime";
            return projectPython39;
        };

        QString pythonBasePath = resolveDefaultPythonHome();
        pythonBasePath = QDir::fromNativeSeparators(pythonBasePath);
        m_pythonHome = pythonBasePath;

        qDebug() << "[Registration2D3D] Python Home:" << pythonBasePath;

        // 直接检查 python39.dll
        const QString expectedDll = "python39.dll";
        const QString dllPath = pythonBasePath + "/" + expectedDll;
        const bool hasDll = QFileInfo::exists(dllPath);

        // 支持 .pyc 格式的 encodings 模块（项目使用嵌入式Python）
        const QString encodingsInitPyc = pythonBasePath + "/Lib/encodings/__init__.pyc";
        const bool hasEncodings = QFileInfo::exists(encodingsInitPyc);

        if (!hasEncodings || !hasDll) {
            m_lastError = QString(
                "PythonHome is not compatible with this build.\n"
                "Expected: %1 + Lib/encodings.\n"
                "Given PYTHONHOME=%2\n"
                "hasDll=%3, hasEncodings=%4\n"
                "Tip: Use the same Python you linked against (CMake's Python3), or rebuild the plugin with a matching Python.")
                             .arg(expectedDll, pythonBasePath)
                             .arg(hasDll ? "true" : "false")
                             .arg(hasEncodings ? "true" : "false");
            qCritical() << "[Registration2D3D]" << m_lastError;
            return false;
        }

        QString sitePackagesPath = pythonBasePath + "/Lib/site-packages";
        QString cv2Path = sitePackagesPath + "/cv2";

        // Build DLL search paths
        QStringList dllPaths;
        dllPaths << pythonBasePath;
        dllPaths << pythonBasePath + "/bin";
        dllPaths << pythonBasePath + "/DLLs";
        dllPaths << cv2Path;
        dllPaths << sitePackagesPath + "/numpy/.libs";
        dllPaths << sitePackagesPath + "/torch/lib";

        QString currentPath = qEnvironmentVariable("PATH");
        QString newPath = dllPaths.join(";") + ";" + currentPath;
        qputenv("PATH", newPath.toUtf8());
        qDebug() << "[Registration2D3D] PATH updated";

#ifdef _WIN32
        // Use Windows API to add DLL search directories (Python 3.8+)
        for (const QString& dllPath : dllPaths) {
            if (QDir(dllPath).exists()) {
                std::wstring wPath = dllPath.toStdWString();
                std::replace(wPath.begin(), wPath.end(), L'/', L'\\');
                DLL_DIRECTORY_COOKIE cookie = AddDllDirectory(wPath.c_str());
                if (cookie) {
                    qDebug() << "[Registration2D3D] AddDllDirectory ok:" << dllPath;
                } else {
                    qDebug() << "[Registration2D3D] AddDllDirectory failed:" << dllPath << "error" << GetLastError();
                }
            }
        }
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#endif
        qDebug() << "[Registration2D3D] DLL search paths configured";

        // Set PYTHONHOME/PYTHONPATH before Py_Initialize
        qputenv("PYTHONHOME", normalizeForEnv(pythonBasePath).toUtf8());
        QString pythonPath = pythonBasePath + "/Lib;" + pythonBasePath + "/Lib/site-packages;" + pythonBasePath;
        qputenv("PYTHONPATH", pythonPath.toUtf8());
        qDebug() << "[Registration2D3D] PYTHONHOME:" << pythonBasePath;
        qDebug() << "[Registration2D3D] PYTHONPATH:" << pythonPath;

        // 设置Python主目录为项目目录下的嵌入式Python
        std::wstring wPythonHome = pythonBasePath.toStdWString();
        Py_SetPythonHome(const_cast<wchar_t*>(wPythonHome.c_str()));
        qDebug() << "[Registration2D3D] Python Home set to:" << pythonBasePath;

        // 初始化Python解释器
        Py_Initialize();

        if (!Py_IsInitialized()) {
            m_lastError = "Python interpreter initialization failed";
            return false;
        }

        // 初始化线程支持（Python 3.7+ 中这个是自动的，但显式调用也没问题）
        #if PY_VERSION_HEX < 0x03070000
        PyEval_InitThreads();
        #endif

        // 设置Python路径
        if (!setupPythonPath()) {
            m_lastError = "Failed to configure Python path";
            Py_Finalize();
            return false;
        }

        // 【重要】设置环境变量，防止NumPy/BLAS在多线程环境中死锁
        qDebug() << "[Registration2D3D] Setting BLAS threading environment variables...";
        PyRun_SimpleString(
            "import os\n"
            "os.environ['OMP_NUM_THREADS'] = '1'\n"
            "os.environ['OPENBLAS_NUM_THREADS'] = '1'\n"
            "os.environ['MKL_NUM_THREADS'] = '1'\n"
            "os.environ['NUMEXPR_NUM_THREADS'] = '1'\n"
            "os.environ['VECLIB_MAXIMUM_THREADS'] = '1'\n"
            "os.environ['KMP_INIT_AT_FORK'] = 'FALSE'\n"
        );

        // 【关键】在主线程中预加载所有大型Python模块
        // 这样可以避免在worker线程首次导入时发生死锁
        qDebug() << "[Registration2D3D] Preloading Python modules on the main thread (this may take a few seconds)...";
        QElapsedTimer preloadTimer;
        preloadTimer.start();

        // 设置matplotlib使用非GUI后端
        PyRun_SimpleString("import matplotlib; matplotlib.use('Agg')");
        qDebug() << "[Registration2D3D] matplotlib backend configured -" << preloadTimer.elapsed() << "ms";

        // 预加载所有大型模块
        const char* preloadScript =
            "import sys\n"
            "import os\n"
            "print('[Python] 预加载numpy...', flush=True)\n"
            "import numpy\n"
            "print('[Python] 预加载cv2...', flush=True)\n"
            "import cv2\n"
            "print(f'[Python] cv2 {cv2.__version__} 加载成功', flush=True)\n"
            "print('[Python] 预加载itk...', flush=True)\n"
            "import itk\n"
            "print('[Python] 预加载torch...', flush=True)\n"
            "import torch\n"
            "print('[Python] 预加载cma...', flush=True)\n"
            "import cma\n"
            "print('[Python] 预加载matplotlib.pyplot...', flush=True)\n"
            "import matplotlib.pyplot\n"
            "print('[Python] 所有模块预加载完成', flush=True)\n";

        int preloadResult = PyRun_SimpleString(preloadScript);
        if (preloadResult != 0) {
            qWarning() << "[Registration2D3D] Module preload reported an error, continuing startup";
        }
        qDebug() << "[Registration2D3D] Main-thread module preload completed in:" << preloadTimer.elapsed() << "ms";

        // 保存主线程状态并释放GIL，让其他线程可以获取GIL
        m_mainThreadState = PyEval_SaveThread();

        m_pythonInitialized = true;
        qDebug() << "[Registration2D3D] Python environment initialized successfully";
        qDebug() << "  - Python Home:" << m_pythonHome;
        qDebug() << "  - Scripts Path:" << scriptsPath;

        return true;
        
    } catch (const std::exception& e) {
        m_lastError = QString("Python initialization exception: %1").arg(e.what());
        qCritical() << "[Registration2D3D]" << m_lastError;
        return false;
    }
}

bool Registration2D3DServiceImpl::isPythonInitialized()
{
    QMutexLocker locker(&m_pythonMutex);
    return m_pythonInitialized;
}

void Registration2D3DServiceImpl::finalizePythonEnvironment()
{
    QMutexLocker locker(&m_pythonMutex);
    
    if (!m_pythonInitialized) {
        return;
    }
    
    // 注意：不调用Py_Finalize()，因为可能有其他地方在使用Python
    // 在应用程序退出时会自动清理
    
    m_pythonInitialized = false;
    qDebug() << "[Registration2D3D] Python environment cleaned up";
}

// ========== 配置管理 ==========

void Registration2D3DServiceImpl::setConfiguration(const QString& key, const QVariant& value)
{
    QMutexLocker locker(&m_configMutex);
    m_configuration[key] = value;
}

QVariant Registration2D3DServiceImpl::getConfiguration(const QString& key, const QVariant& defaultValue)
{
    QMutexLocker locker(&m_configMutex);
    return m_configuration.value(key, defaultValue);
}

// ========== 工具方法 ==========

bool Registration2D3DServiceImpl::validateParameters(
    const Registration2D3DParameters& params, 
    QString& errorMessage)
{
    // 检查CT路径
    if (params.ctPath.isEmpty()) {
        errorMessage = "CT图像路径为空";
        return false;
    }
    if (!QFile::exists(params.ctPath)) {
        errorMessage = "CT图像文件不存在: " + params.ctPath;
        return false;
    }
    
    // 检查X射线图像路径
    if (params.xrayApPath.isEmpty()) {
        errorMessage = "AP视角X射线图像路径为空";
        return false;
    }
    if (!QFile::exists(params.xrayApPath)) {
        errorMessage = "AP视角X射线图像文件不存在: " + params.xrayApPath;
        return false;
    }
    
    if (params.xrayLatPath.isEmpty()) {
        errorMessage = "LAT视角X射线图像路径为空";
        return false;
    }
    if (!QFile::exists(params.xrayLatPath)) {
        errorMessage = "LAT视角X射线图像文件不存在: " + params.xrayLatPath;
        return false;
    }
    
    // 检查初始参数
    if (params.initParams.size() != 6) {
        errorMessage = "初始参数数量错误（应为6个）";
        return false;
    }
    
    // 检查搜索范围
    if (params.searchRange.size() != 6) {
        errorMessage = "搜索范围数量错误（应为6个）";
        return false;
    }
    
    // 检查K-d树数量
    if (params.kdTreeNum <= 0 || params.kdTreeNum > 200) {
        errorMessage = "K-d树数量无效（应在1-200之间）";
        return false;
    }
    
    return true;
}

bool Registration2D3DServiceImpl::generateDRRPreview(
    const QString& ctPath, 
    const QVector<double>& params,
    const QString& view,
    const QString& outputPath)
{
    // TODO: 实现DRR预览生成
    // 这需要调用Python的DRR生成函数
    m_lastError = "DRR preview generation is not implemented yet";
    return false;
}

QString Registration2D3DServiceImpl::getLastError() const
{
    return m_lastError;
}

// ========== 私有方法 ==========

QString Registration2D3DServiceImpl::generateRegistrationId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool Registration2D3DServiceImpl::initializeDatabase()
{
    QMutexLocker locker(&m_dbMutex);
    
    // 使用主数据库
    m_database = QSqlDatabase::database();
    
    if (!m_database.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }
    
    // 创建配准表
    QSqlQuery query(m_database);
    bool success = query.exec(R"(
        CREATE TABLE IF NOT EXISTS registration_2d3d (
            registration_id TEXT PRIMARY KEY,
            patient_id TEXT,
            start_time TEXT,
            end_time TEXT,
            duration_seconds INTEGER,
            status TEXT,
            error_message TEXT,
            ct_path TEXT,
            xray_ap_path TEXT,
            xray_lat_path TEXT,
            init_params TEXT,
            search_range TEXT,
            kd_tree_num INTEGER,
            ap_rx REAL, ap_ry REAL, ap_rz REAL,
            ap_tx REAL, ap_ty REAL, ap_tz REAL,
            ap_metric REAL,
            lat_rx REAL, lat_ry REAL, lat_rz REAL,
            lat_tx REAL, lat_ty REAL, lat_tz REAL,
            lat_metric REAL,
            final_metric REAL,
            total_iterations INTEGER
        )
    )");
    
    if (!success) {
        m_lastError = "Failed to create registration table: " + query.lastError().text();
        return false;
    }
    
    qDebug() << "[Registration2D3D] Database tables initialized";
    return true;
}

bool Registration2D3DServiceImpl::saveResultToDatabase(const Registration2D3DResult& result)
{
    QMutexLocker locker(&m_dbMutex);
    
    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT OR REPLACE INTO registration_2d3d (
            registration_id, start_time, end_time, duration_seconds,
            status, error_message,
            ct_path, xray_ap_path, xray_lat_path,
            init_params, search_range, kd_tree_num,
            ap_rx, ap_ry, ap_rz, ap_tx, ap_ty, ap_tz, ap_metric,
            lat_rx, lat_ry, lat_rz, lat_tx, lat_ty, lat_tz, lat_metric,
            final_metric, total_iterations
        ) VALUES (
            ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
            ?, ?, ?, ?, ?, ?, ?,
            ?, ?, ?, ?, ?, ?, ?,
            ?, ?
        )
    )");
    
    query.addBindValue(result.registrationId);
    query.addBindValue(result.startTime.toString(Qt::ISODate));
    query.addBindValue(result.endTime.toString(Qt::ISODate));
    query.addBindValue(result.durationSeconds);
    query.addBindValue(result.getStatusString());
    query.addBindValue(result.errorMessage);
    query.addBindValue(result.parameters.ctPath);
    query.addBindValue(result.parameters.xrayApPath);
    query.addBindValue(result.parameters.xrayLatPath);
    
    // 序列化数组
    QString initParamsStr;
    for (double v : result.parameters.initParams) {
        initParamsStr += QString::number(v) + ",";
    }
    query.addBindValue(initParamsStr);
    
    QString searchRangeStr;
    for (int v : result.parameters.searchRange) {
        searchRangeStr += QString::number(v) + ",";
    }
    query.addBindValue(searchRangeStr);
    
    query.addBindValue(result.parameters.kdTreeNum);
    
    // AP结果
    query.addBindValue(result.apResult.rx);
    query.addBindValue(result.apResult.ry);
    query.addBindValue(result.apResult.rz);
    query.addBindValue(result.apResult.tx);
    query.addBindValue(result.apResult.ty);
    query.addBindValue(result.apResult.tz);
    query.addBindValue(result.apResult.goMetric);
    
    // LAT结果
    query.addBindValue(result.latResult.rx);
    query.addBindValue(result.latResult.ry);
    query.addBindValue(result.latResult.rz);
    query.addBindValue(result.latResult.tx);
    query.addBindValue(result.latResult.ty);
    query.addBindValue(result.latResult.tz);
    query.addBindValue(result.latResult.goMetric);
    
    query.addBindValue(result.finalMetric);
    query.addBindValue(result.totalIterations);
    
    if (!query.exec()) {
        m_lastError = "Failed to save registration result: " + query.lastError().text();
        return false;
    }
    
    return true;
}

Registration2D3DResult Registration2D3DServiceImpl::loadResultFromDatabase(const QString& registrationId)
{
    Registration2D3DResult result;
    result.registrationId = registrationId;
    
    QMutexLocker locker(&m_dbMutex);
    
    QSqlQuery query(m_database);
    query.prepare("SELECT * FROM registration_2d3d WHERE registration_id = ?");
    query.addBindValue(registrationId);
    
    if (!query.exec() || !query.next()) {
        m_lastError = "Failed to load registration result";
        return result;
    }
    
    result.startTime = QDateTime::fromString(query.value("start_time").toString(), Qt::ISODate);
    result.endTime = QDateTime::fromString(query.value("end_time").toString(), Qt::ISODate);
    result.durationSeconds = query.value("duration_seconds").toInt();
    
    QString statusStr = query.value("status").toString();
    if (statusStr == "完成") result.status = Registration2D3DResult::Completed;
    else if (statusStr == "失败") result.status = Registration2D3DResult::Failed;
    else if (statusStr == "运行中") result.status = Registration2D3DResult::Running;
    else if (statusStr == "取消") result.status = Registration2D3DResult::Cancelled;
    
    result.errorMessage = query.value("error_message").toString();
    
    // 加载参数
    result.parameters.ctPath = query.value("ct_path").toString();
    result.parameters.xrayApPath = query.value("xray_ap_path").toString();
    result.parameters.xrayLatPath = query.value("xray_lat_path").toString();
    result.parameters.kdTreeNum = query.value("kd_tree_num").toInt();
    
    // 加载AP结果
    result.apResult.viewName = "AP";
    result.apResult.rx = query.value("ap_rx").toDouble();
    result.apResult.ry = query.value("ap_ry").toDouble();
    result.apResult.rz = query.value("ap_rz").toDouble();
    result.apResult.tx = query.value("ap_tx").toDouble();
    result.apResult.ty = query.value("ap_ty").toDouble();
    result.apResult.tz = query.value("ap_tz").toDouble();
    result.apResult.goMetric = query.value("ap_metric").toDouble();
    
    // 加载LAT结果
    result.latResult.viewName = "LAT";
    result.latResult.rx = query.value("lat_rx").toDouble();
    result.latResult.ry = query.value("lat_ry").toDouble();
    result.latResult.rz = query.value("lat_rz").toDouble();
    result.latResult.tx = query.value("lat_tx").toDouble();
    result.latResult.ty = query.value("lat_ty").toDouble();
    result.latResult.tz = query.value("lat_tz").toDouble();
    result.latResult.goMetric = query.value("lat_metric").toDouble();
    
    result.finalMetric = query.value("final_metric").toDouble();
    result.totalIterations = query.value("total_iterations").toInt();
    
    return result;
}

void Registration2D3DServiceImpl::logMessage(const QString& level, const QString& message) const
{
    QString logMsg = QString("[Registration2D3D][%1] %2").arg(level).arg(message);
    if (level == "ERROR") {
        qCritical() << logMsg;
    } else if (level == "WARNING") {
        qWarning() << logMsg;
    } else {
        qDebug() << logMsg;
    }
}

QString Registration2D3DServiceImpl::getProjectPath() const
{
    return QCoreApplication::applicationDirPath();
}

bool Registration2D3DServiceImpl::setupPythonPath()
{
    if (m_scriptsPath.isEmpty()) {
        return true; // 使用默认路径
    }
    
    try {
        PyObject* sysPath = PySys_GetObject("path");
        if (!sysPath) {
            return false;
        }
        
        PyObject* path = PyUnicode_FromString(m_scriptsPath.toUtf8().constData());
        if (!path) {
            return false;
        }
        
        PyList_Insert(sysPath, 0, path);
        Py_DECREF(path);
        
        qDebug() << "[Registration2D3D] Python search path added:" << m_scriptsPath;
        return true;
        
    } catch (...) {
        return false;
    }
}

bool Registration2D3DServiceImpl::callPythonRegistration(
    const Registration2D3DParameters& params,
    Registration2D3DResult& result)
{
    QMutexLocker locker(&m_pythonMutex);
    
    try {
        // 获取GIL
        PyGILState_STATE gstate = PyGILState_Ensure();
        
        // 导入模块
        PyObject* pModule = PyImport_ImportModule("2D3DRegistration02");
        if (!pModule) {
            PyErr_Print();
            m_lastError = "Failed to import Python module: 2D3DRegistration02";
            PyGILState_Release(gstate);
            return false;
        }
        
        // 获取函数
        PyObject* pFunc = PyObject_GetAttrString(pModule, "changedTest");
        if (!pFunc || !PyCallable_Check(pFunc)) {
            m_lastError = "Failed to find Python function: changedTest";
            Py_DECREF(pModule);
            PyGILState_Release(gstate);
            return false;
        }
        
        // 准备参数
        PyObject* pArgs = PyTuple_New(12);
        
        // 文件路径
        PyTuple_SetItem(pArgs, 0, PyUnicode_FromString(params.ctPath.toUtf8().constData()));
        PyTuple_SetItem(pArgs, 1, PyUnicode_FromString(params.xrayApPath.toUtf8().constData()));
        PyTuple_SetItem(pArgs, 2, PyUnicode_FromString(params.xrayLatPath.toUtf8().constData()));
        
        // 初始参数
        PyObject* initList = PyList_New(6);
        for (int i = 0; i < 6; i++) {
            PyList_SetItem(initList, i, PyFloat_FromDouble(params.initParams[i]));
        }
        PyTuple_SetItem(pArgs, 3, initList);
        
        // 搜索范围
        PyObject* rangeList = PyList_New(6);
        for (int i = 0; i < 6; i++) {
            PyList_SetItem(rangeList, i, PyLong_FromLong(params.searchRange[i]));
        }
        PyTuple_SetItem(pArgs, 4, rangeList);
        
        // K-d树数量
        PyTuple_SetItem(pArgs, 5, PyLong_FromLong(params.kdTreeNum));
        
        // 胫骨路径
        PyTuple_SetItem(pArgs, 6, PyUnicode_FromString(params.jingguPath.toUtf8().constData()));
        
        // 翻转标志
        PyTuple_SetItem(pArgs, 7, params.apUpDown ? Py_True : Py_False);
        PyTuple_SetItem(pArgs, 8, params.apHorizontal ? Py_True : Py_False);
        PyTuple_SetItem(pArgs, 9, params.latUpDown ? Py_True : Py_False);
        PyTuple_SetItem(pArgs, 10, params.latHorizontal ? Py_True : Py_False);
        PyTuple_SetItem(pArgs, 11, params.generateDRR ? Py_True : Py_False);
        
        // 调用函数
        PyObject* pReturn = PyObject_CallObject(pFunc, pArgs);
        
        if (!pReturn) {
            PyErr_Print();
            m_lastError = "Python function call failed";
            Py_DECREF(pArgs);
            Py_DECREF(pFunc);
            Py_DECREF(pModule);
            PyGILState_Release(gstate);
            return false;
        }
        
        // 解析返回结果（12个浮点数）
        if (!PyList_Check(pReturn) || PyList_Size(pReturn) != 12) {
            m_lastError = "Python returned data in an unexpected format";
            Py_DECREF(pReturn);
            Py_DECREF(pArgs);
            Py_DECREF(pFunc);
            Py_DECREF(pModule);
            PyGILState_Release(gstate);
            return false;
        }
        
        // AP结果
        result.apResult.viewName = "AP";
        result.apResult.rx = PyFloat_AsDouble(PyList_GetItem(pReturn, 0));
        result.apResult.ry = PyFloat_AsDouble(PyList_GetItem(pReturn, 1));
        result.apResult.rz = PyFloat_AsDouble(PyList_GetItem(pReturn, 2));
        result.apResult.tx = PyFloat_AsDouble(PyList_GetItem(pReturn, 3));
        result.apResult.ty = PyFloat_AsDouble(PyList_GetItem(pReturn, 4));
        result.apResult.tz = PyFloat_AsDouble(PyList_GetItem(pReturn, 5));
        
        // LAT结果
        result.latResult.viewName = "LAT";
        result.latResult.rx = PyFloat_AsDouble(PyList_GetItem(pReturn, 6));
        result.latResult.ry = PyFloat_AsDouble(PyList_GetItem(pReturn, 7));
        result.latResult.rz = PyFloat_AsDouble(PyList_GetItem(pReturn, 8));
        result.latResult.tx = PyFloat_AsDouble(PyList_GetItem(pReturn, 9));
        result.latResult.ty = PyFloat_AsDouble(PyList_GetItem(pReturn, 10));
        result.latResult.tz = PyFloat_AsDouble(PyList_GetItem(pReturn, 11));
        
        // 清理
        Py_DECREF(pReturn);
        Py_DECREF(pArgs);
        Py_DECREF(pFunc);
        Py_DECREF(pModule);
        
        // 释放GIL
        PyGILState_Release(gstate);
        
        qDebug() << "[Registration2D3D] Python registration completed";
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = QString("Python call exception: %1").arg(e.what());
        return false;
    }
}

// ========== 槽函数 ==========

void Registration2D3DServiceImpl::onWorkerProgressUpdated(
    const QString& registrationId, 
    const Registration2D3DProgress& progress)
{
    emit progressUpdated(registrationId, progress);
}

void Registration2D3DServiceImpl::onWorkerCompleted(
    const QString& registrationId, 
    const Registration2D3DResult& result)
{
    // 保存结果
    saveResultToDatabase(result);
    
    // 缓存结果
    {
        QMutexLocker locker(&m_workerMutex);
        m_resultCache[registrationId] = result;
        m_activeWorkers.remove(registrationId);
    }
    
    emit registrationCompleted(registrationId, result);
}

void Registration2D3DServiceImpl::onWorkerFailed(
    const QString& registrationId, 
    const QString& errorMessage)
{
    // 从活动列表移除
    {
        QMutexLocker locker(&m_workerMutex);
        m_activeWorkers.remove(registrationId);
    }
    
    emit registrationFailed(registrationId, errorMessage);
}

// ========================================================================
// Registration2D3DWorker 实现
// ========================================================================

Registration2D3DWorker::Registration2D3DWorker(
    const QString& registrationId,
    const Registration2D3DParameters& params,
    QObject* parent)
    : QThread(parent)
    , m_registrationId(registrationId)
    , m_parameters(params)
    , m_cancelled(false)
{
    m_result.registrationId = registrationId;
    m_result.parameters = params;
    m_result.status = Registration2D3DResult::Running;
}

Registration2D3DWorker::~Registration2D3DWorker()
{
}

void Registration2D3DWorker::run()
{
    m_result.startTime = QDateTime::currentDateTime();
    
    emitProgress("初始化", 0, "开始2D3D配准");
    
    try {
        // 执行配准
        bool success = executePythonRegistration();
        
        m_result.endTime = QDateTime::currentDateTime();
        m_result.durationSeconds = m_result.startTime.secsTo(m_result.endTime);
        
        if (success) {
            m_result.status = Registration2D3DResult::Completed;
            m_result.finalMetric = (m_result.apResult.goMetric + m_result.latResult.goMetric) / 2.0;
            
            emitProgress("完成", 100, "配准成功完成");
            emit completed(m_registrationId, m_result);
        } else {
            m_result.status = Registration2D3DResult::Failed;
            emit failed(m_registrationId, m_result.errorMessage);
        }
        
    } catch (const std::exception& e) {
        m_result.status = Registration2D3DResult::Failed;
        m_result.errorMessage = QString("Registration exception: %1").arg(e.what());
        emit failed(m_registrationId, m_result.errorMessage);
    }
}

void Registration2D3DWorker::cancel()
{
    QMutexLocker locker(&m_mutex);
    m_cancelled = true;
}

bool Registration2D3DWorker::executePythonRegistration()
{
    qDebug() << "[Registration2D3DWorker] Starting Python registration...";
    qDebug() << "[Registration2D3DWorker] Current thread:" << QThread::currentThread();

    // 检查Python是否已初始化
    if (!Py_IsInitialized()) {
        m_result.errorMessage = "Python environment not initialized";
        qWarning() << "[Registration2D3DWorker]" << m_result.errorMessage;
        return false;
    }
    qDebug() << "[Registration2D3DWorker] Python runtime ready";

    // 获取GIL
    qDebug() << "[Registration2D3DWorker] Attempting to acquire GIL...";
    PyThreadStateLock lock;
    qDebug() << "[Registration2D3DWorker] GIL acquired";

    emitProgress("加载模块", 5, "正在加载Python配准模块（首次加载可能需要30-60秒）...");

    try {
        qDebug() << "[Registration2D3DWorker] Starting staged module imports...";
        QElapsedTimer importTimer;
        importTimer.start();

        // 设置环境变量，防止BLAS/OpenBLAS/MKL在嵌入式Python中死锁
        qDebug() << "[Registration2D3DWorker] 0. Setting BLAS environment variables to avoid deadlock...";
        PyRun_SimpleString(
            "import os\n"
            "os.environ['OMP_NUM_THREADS'] = '1'\n"
            "os.environ['OPENBLAS_NUM_THREADS'] = '1'\n"
            "os.environ['MKL_NUM_THREADS'] = '1'\n"
            "os.environ['NUMEXPR_NUM_THREADS'] = '1'\n"
            "os.environ['VECLIB_MAXIMUM_THREADS'] = '1'\n"
        );
        qDebug() << "[Registration2D3DWorker] Environment variables configured -" << importTimer.elapsed() << "ms";

        // 分步导入，便于诊断哪个模块卡住
        qDebug() << "[Registration2D3DWorker] 1. Importing sys...";
        PyObject* pSys = PyImport_ImportModule("sys");
        if (!pSys) { PyErr_Print(); qWarning() << "Failed to import sys"; }
        else { Py_DECREF(pSys); qDebug() << "[Registration2D3DWorker] sys OK -" << importTimer.elapsed() << "ms"; }

        qDebug() << "[Registration2D3DWorker] 2. Importing numpy...";
        PyObject* pNumpy = PyImport_ImportModule("numpy");
        if (!pNumpy) { PyErr_Print(); qWarning() << "Failed to import numpy"; }
        else { Py_DECREF(pNumpy); qDebug() << "[Registration2D3DWorker] numpy OK -" << importTimer.elapsed() << "ms"; }

        qDebug() << "[Registration2D3DWorker] 3. Importing cv2...";
        PyObject* pCv2 = PyImport_ImportModule("cv2");
        if (!pCv2) { PyErr_Print(); qWarning() << "Failed to import cv2"; }
        else { Py_DECREF(pCv2); qDebug() << "[Registration2D3DWorker] cv2 OK -" << importTimer.elapsed() << "ms"; }

        qDebug() << "[Registration2D3DWorker] 4. Importing itk...";
        PyObject* pItk = PyImport_ImportModule("itk");
        if (!pItk) { PyErr_Print(); qWarning() << "Failed to import itk"; }
        else { Py_DECREF(pItk); qDebug() << "[Registration2D3DWorker] itk OK -" << importTimer.elapsed() << "ms"; }

        qDebug() << "[Registration2D3DWorker] 5. Importing matplotlib (Agg backend avoids GUI conflicts)...";
        // 设置matplotlib使用非GUI后端，避免与Qt冲突
        PyRun_SimpleString("import matplotlib; matplotlib.use('Agg')");
        PyObject* pPlt = PyImport_ImportModule("matplotlib.pyplot");
        if (!pPlt) { PyErr_Print(); qWarning() << "Failed to import matplotlib"; }
        else { Py_DECREF(pPlt); qDebug() << "[Registration2D3DWorker] matplotlib OK -" << importTimer.elapsed() << "ms"; }

        qDebug() << "[Registration2D3DWorker] 6. Importing torch...";
        PyObject* pTorch = PyImport_ImportModule("torch");
        if (!pTorch) { PyErr_Print(); qWarning() << "Failed to import torch"; }
        else { Py_DECREF(pTorch); qDebug() << "[Registration2D3DWorker] torch OK -" << importTimer.elapsed() << "ms"; }

        qDebug() << "[Registration2D3DWorker] 7. Importing cma...";
        PyObject* pCma = PyImport_ImportModule("cma");
        if (!pCma) { PyErr_Print(); qWarning() << "Failed to import cma"; }
        else { Py_DECREF(pCma); qDebug() << "[Registration2D3DWorker] cma OK -" << importTimer.elapsed() << "ms"; }

        qDebug() << "[Registration2D3DWorker] 8. Importing SiddonGpuPy...";
        PyObject* pSiddon = PyImport_ImportModule("SiddonGpuPy");
        if (!pSiddon) { PyErr_Print(); qWarning() << "Failed to import SiddonGpuPy (possible CUDA issue)"; }
        else { Py_DECREF(pSiddon); qDebug() << "[Registration2D3DWorker] SiddonGpuPy OK -" << importTimer.elapsed() << "ms"; }

        qDebug() << "[Registration2D3DWorker] 9. Importing main module: 2D3DRegistration02...";
        // 导入主模块
        PyObject* pModule = PyImport_ImportModule("2D3DRegistration02");

        qDebug() << "[Registration2D3DWorker] Total module import time:" << importTimer.elapsed() << "ms";

        qDebug() << "[Registration2D3DWorker] Module import time:" << importTimer.elapsed() << "ms";

        if (!pModule) {
            PyErr_Print();
            // 获取Python错误信息
            PyObject *pType, *pValue, *pTraceback;
            PyErr_Fetch(&pType, &pValue, &pTraceback);
            if (pValue) {
                PyObject* pStr = PyObject_Str(pValue);
                if (pStr) {
                    const char* errStr = PyUnicode_AsUTF8(pStr);
                    m_result.errorMessage = QString("Failed to import Python module: %1").arg(errStr);
                    Py_DECREF(pStr);
                }
                Py_XDECREF(pType);
                Py_XDECREF(pValue);
                Py_XDECREF(pTraceback);
            } else {
                m_result.errorMessage = "Failed to import Python module (unknown error)";
            }
            qWarning() << "[Registration2D3DWorker]" << m_result.errorMessage;
            return false;
        }
        qDebug() << "[Registration2D3DWorker] Module import completed";
        
        emitProgress("准备参数", 10, "准备配准参数");
        
        // 获取函数
        PyObject* pFunc = PyObject_GetAttrString(pModule, "changedTest");
        if (!pFunc || !PyCallable_Check(pFunc)) {
            m_result.errorMessage = "Failed to find registration function";
            Py_DECREF(pModule);
            return false;
        }
        
        // 准备参数（与ServiceImpl中相同）
        PyObject* pArgs = PyTuple_New(12);
        
        PyTuple_SetItem(pArgs, 0, PyUnicode_FromString(m_parameters.ctPath.toUtf8().constData()));
        PyTuple_SetItem(pArgs, 1, PyUnicode_FromString(m_parameters.xrayApPath.toUtf8().constData()));
        PyTuple_SetItem(pArgs, 2, PyUnicode_FromString(m_parameters.xrayLatPath.toUtf8().constData()));
        
        PyObject* initList = PyList_New(6);
        for (int i = 0; i < 6; i++) {
            PyList_SetItem(initList, i, PyFloat_FromDouble(m_parameters.initParams[i]));
        }
        PyTuple_SetItem(pArgs, 3, initList);
        
        PyObject* rangeList = PyList_New(6);
        for (int i = 0; i < 6; i++) {
            PyList_SetItem(rangeList, i, PyLong_FromLong(m_parameters.searchRange[i]));
        }
        PyTuple_SetItem(pArgs, 4, rangeList);
        
        PyTuple_SetItem(pArgs, 5, PyLong_FromLong(m_parameters.kdTreeNum));
        PyTuple_SetItem(pArgs, 6, PyUnicode_FromString(m_parameters.jingguPath.toUtf8().constData()));
        PyTuple_SetItem(pArgs, 7, m_parameters.apUpDown ? Py_True : Py_False);
        PyTuple_SetItem(pArgs, 8, m_parameters.apHorizontal ? Py_True : Py_False);
        PyTuple_SetItem(pArgs, 9, m_parameters.latUpDown ? Py_True : Py_False);
        PyTuple_SetItem(pArgs, 10, m_parameters.latHorizontal ? Py_True : Py_False);
        PyTuple_SetItem(pArgs, 11, m_parameters.generateDRR ? Py_True : Py_False);
        
        emitProgress("AP视角配准", 20, "正在进行AP视角配准");
        
        // 打印参数用于调试
        qDebug() << "[Registration2D3DWorker] changedTest arguments:";
        qDebug() << "  CT path:" << m_parameters.ctPath;
        qDebug() << "  AP X-ray path:" << m_parameters.xrayApPath;
        qDebug() << "  LAT X-ray path:" << m_parameters.xrayLatPath;
        qDebug() << "  Tibia path:" << m_parameters.jingguPath;

        // 调用函数
        PyObject* pReturn = PyObject_CallObject(pFunc, pArgs);

        if (!pReturn) {
            // 获取详细的Python错误信息
            PyObject *pType, *pValue, *pTraceback;
            PyErr_Fetch(&pType, &pValue, &pTraceback);
            PyErr_NormalizeException(&pType, &pValue, &pTraceback);

            QString errorMsg = "Python function call failed";
            if (pValue) {
                PyObject* pStr = PyObject_Str(pValue);
                if (pStr) {
                    errorMsg = QString("Python error: %1").arg(PyUnicode_AsUTF8(pStr));
                    Py_DECREF(pStr);
                }
            }

            // 打印完整traceback
            if (pTraceback) {
                PyObject* tbModule = PyImport_ImportModule("traceback");
                if (tbModule) {
                    PyObject* formatFunc = PyObject_GetAttrString(tbModule, "format_exception");
                    if (formatFunc) {
                        PyObject* tbList = PyObject_CallFunctionObjArgs(formatFunc, pType, pValue, pTraceback, NULL);
                        if (tbList) {
                            PyObject* tbStr = PyUnicode_Join(PyUnicode_FromString(""), tbList);
                            if (tbStr) {
                                qWarning() << "[Registration2D3DWorker] Python Traceback:\n" << PyUnicode_AsUTF8(tbStr);
                                Py_DECREF(tbStr);
                            }
                            Py_DECREF(tbList);
                        }
                        Py_DECREF(formatFunc);
                    }
                    Py_DECREF(tbModule);
                }
            }

            Py_XDECREF(pType);
            Py_XDECREF(pValue);
            Py_XDECREF(pTraceback);

            m_result.errorMessage = errorMsg;
            qWarning() << "[Registration2D3DWorker]" << errorMsg;
            Py_DECREF(pArgs);
            Py_DECREF(pFunc);
            Py_DECREF(pModule);
            return false;
        }
        
        emitProgress("LAT视角配准", 60, "正在进行LAT视角配准");
        
        // 解析结果
        if (!PyList_Check(pReturn) || PyList_Size(pReturn) != 12) {
            m_result.errorMessage = "Python returned data in an unexpected format";
            Py_DECREF(pReturn);
            Py_DECREF(pArgs);
            Py_DECREF(pFunc);
            Py_DECREF(pModule);
            return false;
        }
        
        emitProgress("解析结果", 90, "解析配准结果");
        
        // AP结果
        m_result.apResult.viewName = "AP";
        m_result.apResult.rx = PyFloat_AsDouble(PyList_GetItem(pReturn, 0));
        m_result.apResult.ry = PyFloat_AsDouble(PyList_GetItem(pReturn, 1));
        m_result.apResult.rz = PyFloat_AsDouble(PyList_GetItem(pReturn, 2));
        m_result.apResult.tx = PyFloat_AsDouble(PyList_GetItem(pReturn, 3));
        m_result.apResult.ty = PyFloat_AsDouble(PyList_GetItem(pReturn, 4));
        m_result.apResult.tz = PyFloat_AsDouble(PyList_GetItem(pReturn, 5));
        
        // LAT结果
        m_result.latResult.viewName = "LAT";
        m_result.latResult.rx = PyFloat_AsDouble(PyList_GetItem(pReturn, 6));
        m_result.latResult.ry = PyFloat_AsDouble(PyList_GetItem(pReturn, 7));
        m_result.latResult.rz = PyFloat_AsDouble(PyList_GetItem(pReturn, 8));
        m_result.latResult.tx = PyFloat_AsDouble(PyList_GetItem(pReturn, 9));
        m_result.latResult.ty = PyFloat_AsDouble(PyList_GetItem(pReturn, 10));
        m_result.latResult.tz = PyFloat_AsDouble(PyList_GetItem(pReturn, 11));
        
        // 清理
        Py_DECREF(pReturn);
        Py_DECREF(pArgs);
        Py_DECREF(pFunc);
        Py_DECREF(pModule);
        
        return true;
        
    } catch (const std::exception& e) {
        m_result.errorMessage = QString("Registration execution exception: %1").arg(e.what());
        return false;
    }
}

void Registration2D3DWorker::emitProgress(const QString& phase, int percentage, const QString& message)
{
    Registration2D3DProgress progress;
    progress.currentPhase = phase;
    progress.percentage = percentage;
    progress.message = message;
    
    emit progressUpdated(m_registrationId, progress);
}

// ========================================================================
// UI 组件工厂实现
// ========================================================================

QWidget* Registration2D3DServiceImpl::createRegistrationWidget(QWidget* parent)
{
    qDebug() << "[Registration2D3DServiceImpl] Creating Registration2D3DWidget";

    Registration2D3DWidget* widget = new Registration2D3DWidget(this, parent);

    // 跟踪创建的Widget用于渲染控制
    m_createdWidgets.append(widget);

    return widget;
}

// ========================================================================
// VTK渲染控制实现
// ========================================================================

void Registration2D3DServiceImpl::pauseRendering()
{
    m_renderingPaused = true;

    // 暂停所有创建的Widget的渲染
    for (QWidget* widget : m_createdWidgets) {
        if (widget && widget->isVisible()) {
            widget->setUpdatesEnabled(false);
        }
    }

    qDebug() << "[Registration2D3DService] Rendering paused";
}

void Registration2D3DServiceImpl::resumeRendering()
{
    m_renderingPaused = false;

    // 恢复所有创建的Widget的渲染
    for (QWidget* widget : m_createdWidgets) {
        if (widget) {
            widget->setUpdatesEnabled(true);
            widget->update();
        }
    }

    qDebug() << "[Registration2D3DService] Rendering resumed";
}
