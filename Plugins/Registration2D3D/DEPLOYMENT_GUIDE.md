# Registration2D3D 插件部署与集成指南

## 📦 部署清单

### 1. 插件文件
```
medicalpro/
├── plugins/
│   └── Registration2D3D.dll (或 .so)    # 插件库
└── Regi/                                 # Python脚本目录（必需）
    ├── 2D3DRegistration02.py             # 主配准算法
    ├── ProjectorsModule_multi.py         # DRR生成模块
    ├── RigidMotion.py                    # 刚体变换工具
    └── SiddonGpuPy.pyd (或 .so)          # GPU加速库
```

### 2. Python环境要求
```bash
Python >= 3.7
numpy >= 1.19.0
cma >= 3.0.0
matplotlib >= 3.3.0
SimpleITK >= 2.0.0
torch >= 1.7.0 (GPU版本)
```

### 3. 系统依赖
- **CUDA 10.2+**（GPU加速必需）
- **NVIDIA驱动**（最新版本）
- **Visual C++ Redistributable 2019+**（Windows）

---

## 🔧 构建步骤

### 1. 配置CMake

```bash
cd build
cmake .. -DENABLE_PLUGIN_REGISTRATION_2D3D=ON \
         -DPython3_ROOT_DIR=C:/Python39 \
         -DCUDA_TOOLKIT_ROOT_DIR=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.2
```

### 2. 编译插件

```bash
# Windows (Visual Studio)
cmake --build . --config Release --target Registration2D3D

# Linux / macOS
make Registration2D3D -j8
```

### 3. 验证构建

检查输出：
```
build/Release/plugins/Registration2D3D.dll  (Windows)
build/Release/plugins/libRegistration2D3D.so  (Linux)
```

---

## 🚀 部署步骤

### 步骤1：复制插件库

```bash
# Windows
copy build\Release\plugins\Registration2D3D.dll deploy\plugins\

# Linux
cp build/Release/plugins/libRegistration2D3D.so deploy/plugins/
```

### 步骤2：部署Python脚本

```bash
# 创建Regi目录
mkdir deploy/Regi

# 复制Python脚本
copy E:\ANSN\ANSN\ASNS\Release\Regi\*.py deploy\Regi\
copy E:\ANSN\ANSN\ASNS\Release\Regi\*.pyd deploy\Regi\
```

### 步骤3：安装Python依赖

```bash
cd deploy
pip install -r requirements.txt
```

创建 `requirements.txt`：
```txt
numpy>=1.19.0
cma>=3.0.0
matplotlib>=3.3.0
SimpleITK>=2.0.0
torch>=1.7.0+cu110
```

### 步骤4：配置Python路径（可选）

如果使用独立Python环境，在应用启动时设置：

```cpp
// main.cpp
#include <Python.h>

int main(int argc, char *argv[])
{
    // 设置Python主目录
    QString pythonHome = QCoreApplication::applicationDirPath() + "/Python39";
    std::wstring wPythonHome = pythonHome.toStdWString();
    Py_SetPythonHome(const_cast<wchar_t*>(wPythonHome.c_str()));
    
    // 启动应用
    QApplication app(argc, argv);
    // ...
}
```

---

## 🔌 集成到应用

### 方法1：通过CTK服务（推荐）

```cpp
// MainInterfaceWidget.h
#include "Registration2D3DService.h"

class MainInterfaceWidget : public QWidget
{
    Q_OBJECT
public:
    MainInterfaceWidget(ctkPluginContext* context, QWidget* parent = nullptr);
    
private:
    void setupRegistrationService();
    void onRegistrationMenuTriggered();
    
private:
    ctkPluginContext* m_pluginContext;
    Registration2D3DService* m_registrationService;
};

// MainInterfaceWidget.cpp
#include "MainInterfaceWidget.h"
#include <ctkServiceReference.h>

MainInterfaceWidget::MainInterfaceWidget(ctkPluginContext* context, QWidget* parent)
    : QWidget(parent)
    , m_pluginContext(context)
    , m_registrationService(nullptr)
{
    setupRegistrationService();
}

void MainInterfaceWidget::setupRegistrationService()
{
    // 获取服务引用
    ctkServiceReference ref = 
        m_pluginContext->getServiceReference<Registration2D3DService>();
    
    if (ref) {
        m_registrationService = 
            m_pluginContext->getService<Registration2D3DService>(ref);
        
        if (m_registrationService) {
            qDebug() << "[主界面] 2D3D配准服务已连接";
            
            // 初始化Python环境
            QString appDir = QCoreApplication::applicationDirPath();
            QString pythonHome = appDir + "/Python39";
            QString scriptsPath = appDir + "/Regi";
            
            bool pythonOk = m_registrationService->initializePythonEnvironment(
                pythonHome, scriptsPath);
            
            if (pythonOk) {
                qDebug() << "[主界面] Python环境初始化成功";
            } else {
                qWarning() << "[主界面] Python环境初始化失败:" 
                           << m_registrationService->getLastError();
            }
        }
    } else {
        qWarning() << "[主界面] 2D3D配准服务不可用";
    }
}

void MainInterfaceWidget::onRegistrationMenuTriggered()
{
    if (!m_registrationService) {
        QMessageBox::warning(this, "警告", "2D3D配准服务不可用");
        return;
    }
    
    // 创建并显示配准对话框
    auto* registrationDialog = new RegistrationDialog(m_registrationService, this);
    registrationDialog->exec();
}
```

### 方法2：创建专用页面

```cpp
// RegistrationPage.h
#include <QWidget>
#include "Registration2D3DService.h"

class RegistrationPage : public QWidget
{
    Q_OBJECT
public:
    RegistrationPage(Registration2D3DService* service, QWidget* parent = nullptr);
    
private slots:
    void onBrowseCtClicked();
    void onBrowseXrayApClicked();
    void onBrowseXrayLatClicked();
    void onStartRegistrationClicked();
    void onProgressUpdated(const QString& id, const Registration2D3DProgress& progress);
    void onRegistrationCompleted(const QString& id, const Registration2D3DResult& result);
    
private:
    void setupUI();
    void connectSignals();
    
private:
    Registration2D3DService* m_service;
    
    QLineEdit* m_ctPathEdit;
    QLineEdit* m_xrayApPathEdit;
    QLineEdit* m_xrayLatPathEdit;
    QProgressBar* m_progressBar;
    QPushButton* m_startButton;
    QTextEdit* m_logEdit;
};
```

---

## 🎯 菜单集成

### 添加到主菜单

```cpp
void MainInterfaceWidget::createMenus()
{
    // 工具菜单
    QMenu* toolsMenu = menuBar()->addMenu("工具");
    
    // 2D3D配准菜单项
    QAction* registrationAction = new QAction("2D3D配准", this);
    registrationAction->setIcon(QIcon(":/icons/registration.png"));
    registrationAction->setShortcut(QKeySequence("Ctrl+R"));
    registrationAction->setStatusTip("启动2D-3D医学图像配准");
    
    connect(registrationAction, &QAction::triggered,
            this, &MainInterfaceWidget::onRegistrationMenuTriggered);
    
    toolsMenu->addAction(registrationAction);
    
    // 配准历史菜单项
    QAction* historyAction = new QAction("配准历史", this);
    connect(historyAction, &QAction::triggered,
            this, &MainInterfaceWidget::onShowRegistrationHistory);
    
    toolsMenu->addAction(historyAction);
}
```

### 添加到工具栏

```cpp
void MainInterfaceWidget::createToolBar()
{
    QToolBar* toolbar = addToolBar("主工具栏");
    
    // 配准按钮
    QAction* registrationAction = new QAction(
        QIcon(":/icons/registration.png"), "2D3D配准", this);
    
    connect(registrationAction, &QAction::triggered,
            this, &MainInterfaceWidget::onRegistrationMenuTriggered);
    
    toolbar->addAction(registrationAction);
}
```

---

## 🔍 调试与测试

### 启用调试日志

```cpp
// main.cpp
int main(int argc, char *argv[])
{
    // 设置Qt日志级别
    QLoggingCategory::setFilterRules("*.debug=true\n"
                                     "qt.*.debug=false");
    
    QApplication app(argc, argv);
    // ...
}
```

### 测试Python环境

```cpp
void testPythonEnvironment()
{
    // 测试Python导入
    Py_Initialize();
    
    PyObject* sysModule = PyImport_ImportModule("sys");
    if (sysModule) {
        PyObject* path = PyObject_GetAttrString(sysModule, "path");
        qDebug() << "Python sys.path:" << PyObject_Repr(path);
        Py_DECREF(path);
        Py_DECREF(sysModule);
    }
    
    // 测试2D3D模块
    PyObject* regModule = PyImport_ImportModule("2D3DRegistration02");
    if (regModule) {
        qDebug() << "2D3DRegistration02模块加载成功";
        Py_DECREF(regModule);
    } else {
        PyErr_Print();
        qCritical() << "无法加载2D3DRegistration02模块";
    }
    
    Py_Finalize();
}
```

### 单元测试

```cpp
// test_registration2d3d.cpp
#include <QTest>
#include "Registration2D3DService.h"

class TestRegistration2D3D : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void testParameterValidation();
    void testRegistrationSync();
    void testRegistrationAsync();
    void cleanupTestCase();
    
private:
    Registration2D3DService* m_service;
};

void TestRegistration2D3D::initTestCase()
{
    // 初始化服务
    m_service = new Registration2D3DServiceImpl();
    m_service->initializePythonEnvironment("", "");
}

void TestRegistration2D3D::testParameterValidation()
{
    Registration2D3DParameters params;
    QString error;
    
    // 测试空参数
    QVERIFY(!m_service->validateParameters(params, error));
    QVERIFY(!error.isEmpty());
    
    // 测试有效参数
    params.ctPath = "test.nrrd";
    params.xrayApPath = "test_ap.png";
    params.xrayLatPath = "test_lat.png";
    params.initParams = {0, 0, 0, 0, 0, 0};
    params.searchRange = {15, 15, 15, 50, 50, 50};
    
    // 注意：如果文件不存在，验证会失败
    // 实际测试需要提供真实文件
}

QTEST_MAIN(TestRegistration2D3D)
#include "test_registration2d3d.moc"
```

---

## 📊 性能优化

### 1. Python预加载

```cpp
// 应用启动时预加载Python模块
void preloadPythonModules()
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    
    // 预加载模块
    PyImport_ImportModule("numpy");
    PyImport_ImportModule("cma");
    PyImport_ImportModule("SimpleITK");
    PyImport_ImportModule("2D3DRegistration02");
    
    PyGILState_Release(gstate);
}
```

### 2. 线程池管理

```cpp
// 限制并发配准任务数
class Registration2D3DManager
{
public:
    static constexpr int MAX_CONCURRENT_TASKS = 2;
    
    void enqueueRegistration(const Registration2D3DParameters& params)
    {
        if (m_activeTasks.size() >= MAX_CONCURRENT_TASKS) {
            m_queuedTasks.enqueue(params);
        } else {
            startRegistration(params);
        }
    }
    
private:
    QQueue<Registration2D3DParameters> m_queuedTasks;
    QVector<QString> m_activeTasks;
};
```

### 3. 结果缓存

```cpp
// 缓存最近的配准结果
class ResultCache
{
public:
    static constexpr int MAX_CACHE_SIZE = 100;
    
    void addResult(const Registration2D3DResult& result)
    {
        m_cache.insert(result.registrationId, result);
        if (m_cache.size() > MAX_CACHE_SIZE) {
            // 移除最旧的
            auto it = m_cache.begin();
            m_cache.erase(it);
        }
    }
    
private:
    QMap<QString, Registration2D3DResult> m_cache;
};
```

---

## 🛠️ 故障排查

### 问题1：插件未加载

**检查步骤**：
```bash
# 1. 检查插件文件
ls plugins/Registration2D3D.dll

# 2. 检查依赖库
ldd plugins/Registration2D3D.dll  # Linux
dumpbin /dependents plugins\Registration2D3D.dll  # Windows

# 3. 查看CTK日志
medicalpro.exe --ctk-debug
```

### 问题2：Python导入失败

**检查步骤**：
```python
# 测试Python脚本
cd Regi
python -c "import 2D3DRegistration02"

# 检查依赖
python -c "import numpy; import cma; import SimpleITK; import torch"
```

### 问题3：GPU加速不可用

**检查步骤**：
```bash
# 检查CUDA
nvidia-smi

# 检查PyTorch GPU
python -c "import torch; print(torch.cuda.is_available())"

# 测试SiddonGpuPy
python -c "import SiddonGpuPy; print('OK')"
```

### 问题4：配准速度慢

**优化建议**：
- 减少K-d树数量：`kdTreeNum = 30`
- 缩小搜索范围
- 升级GPU硬件
- 检查CPU占用和内存使用

---

## 📝 版本更新

### v1.0.0 (2024-11-03)
- ✅ 初始发布
- ✅ CTK插件架构
- ✅ Python集成
- ✅ 双视角配准
- ✅ GPU加速DRR
- ✅ CMA-ES优化
- ✅ 数据库存储

### 后续计划
- ⏳ 多GPU并行支持
- ⏳ 实时配准预览
- ⏳ 配准质量自动评估
- ⏳ 配准参数自动推荐

---

## 📞 技术支持

- **文档**: `README.md`
- **示例**: `USAGE_EXAMPLE.cpp`
- **邮箱**: support@medicalpro.com
- **问题反馈**: 通过管理控制台报告

---

## 📄 许可证

Copyright (c) 2024 MedicalPro. All rights reserved.

