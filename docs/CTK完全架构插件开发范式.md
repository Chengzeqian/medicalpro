# CTK完全架构插件开发范式
## MedicalPro项目标准开发指南

> **基于成功重构的6个核心插件实践总结**  
> 版本: v2.0 (完全CTK架构)  
> 更新时间: 2024年12月  
> 适用插件: MedicalImageCore, MedicalProcessing, MedicalViewer, NrrdViewer, ImageInteraction, OpticalTracking

---

## 🎯 **核心设计原则**

### **1. 完全解耦原则**
```
❌ 传统架构：插件A → 直接依赖 → 插件B
✅ 完全CTK架构：插件A → CTK服务接口 → 插件B
```

### **2. ID驱动数据模式**
```cpp
// ❌ 传统方式：直接传递对象指针
MedicalImageData* processImage(MedicalImageData* inputImage);

// ✅ 完全CTK架构：基于ID的数据引用
QString processImage(const QString& imageId, const QVariantMap& parameters);
```

### **3. 服务优先原则**
```cpp
// 所有功能通过服务接口暴露
Q_DECLARE_INTERFACE(YourService, "medical.YourService")

// 不直接暴露内部实现类
class YourServiceImpl; // 仅在插件内部使用
```

### **4. 配置驱动原则**
```cpp
// 使用QVariantMap传递灵活参数
virtual QString loadImage(const QString& filePath, 
                         const QVariantMap& options = QVariantMap()) = 0;

// 而不是硬编码的结构体
struct ImageLoadOptions { ... }; // ❌ 不够灵活
```

---

## 📁 **标准文件结构模式**

### **每个插件必须包含的文件：**

```
YourPlugin/
├── 📄 YourPluginService.h           # 服务接口定义（核心）
├── 📄 YourPluginServiceImpl.h       # 服务实现头文件
├── 📄 YourPluginServiceImpl.cpp     # 服务实现代码
├── 📄 ServiceInterfaces.h           # 外部服务前向声明
├── 📄 YourPluginActivator.h         # CTK插件激活器
├── 📄 YourPluginActivator.cpp       # CTK插件激活器实现
├── 📄 CMakeLists.txt                # 完全CTK架构配置
├── 📄 YourPlugin.qrc                # 资源文件
└── 📄 MANIFEST.MF                   # CTK插件清单
```

### **禁止包含的文件类型：**
- ❌ 直接的硬件驱动包装类（集成到ServiceImpl中）
- ❌ 其他插件的头文件引用
- ❌ 共享数据结构（除MedicalImageCore作为服务提供者）
- ❌ 跨插件的工具类

---

## 🔧 **服务接口设计标准**

### **接口命名规范：**
```cpp
class [PluginName]Service : public QObject
{
    Q_OBJECT
public:
    explicit [PluginName]Service(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~[PluginName]Service() = default;
    
    // 接口方法...
};

Q_DECLARE_INTERFACE([PluginName]Service, "medical.[PluginName]Service")
```

### **方法签名标准：**

#### **输入参数模式：**
```cpp
// 1. 实体ID引用
const QString& imageId, const QString& sessionId, const QString& deviceId

// 2. 灵活参数配置
const QVariantMap& options = QVariantMap()
const QVariantMap& parameters = QVariantMap()

// 3. 原始数据类型
int width, double threshold, bool enabled
```

#### **返回值模式：**
```cpp
// 1. 新实体ID
QString createXXX(...) → 返回新创建的ID

// 2. 操作结果
bool executeXXX(...) → 返回操作是否成功

// 3. 查询结果
QStringList getXXXList() → 返回ID列表
QVariantMap getXXXInfo(const QString& id) → 返回详细信息

// 4. 组件创建
QWidget* createXXXWidget(QWidget* parent = nullptr) → 返回UI组件
```

#### **必备方法类型：**
```cpp
class YourService {
public:
    // 1. 生命周期管理
    virtual QString createSession/Component/Resource(...) = 0;
    virtual bool closeSession/Component/Resource(const QString& id) = 0;
    virtual QStringList getActiveSessions/Components/Resources() const = 0;
    
    // 2. 状态查询
    virtual bool isValid/Available/Ready(const QString& id) const = 0;
    virtual QString getStatus(const QString& id) const = 0;
    virtual QVariantMap getInfo(const QString& id) const = 0;
    
    // 3. 配置管理
    virtual bool setParameters(const QString& id, const QVariantMap& params) = 0;
    virtual QVariantMap getParameters(const QString& id) const = 0;
    
    // 4. UI显示管理（遵循PatientManagement成功模式）
    virtual bool showMainDialog(QWidget* parent = nullptr) = 0;
    virtual bool showConfigDialog(QWidget* parent = nullptr) = 0;
    
signals:
    // 5. 状态变化信号
    void statusChanged(const QString& id, const QString& status);
    void operationCompleted(const QString& operationId, bool success);
    void errorOccurred(const QString& id, const QString& error);
};
```

---

## 🏗️ **CMakeLists.txt配置模式**

### **标准配置模板：**
```cmake
cmake_minimum_required(VERSION 3.16)
project(YourPlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt组件查找
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

# CTK Plugin Framework配置
if(CTK_FOUND)
    # 插件源文件（完全CTK架构）
    set(PLUGIN_SOURCES
        # CTK框架必需文件
        YourPluginActivator.h
        YourPluginActivator.cpp
        
        # 服务接口和实现（完全CTK架构）
        YourPluginService.h
        YourPluginServiceImpl.h
        YourPluginServiceImpl.cpp
        
        # 服务接口声明（完全CTK架构）
        ServiceInterfaces.h
        
        # 资源文件
        YourPlugin.qrc
    )
    
    # 创建插件库
    add_library(YourPlugin SHARED ${PLUGIN_SOURCES})
    
    # 设置插件属性
    set_target_properties(YourPlugin PROPERTIES
        OUTPUT_NAME "YourPlugin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
    )
    
    # 链接库（遵循完全CTK架构）
    target_link_libraries(YourPlugin PRIVATE 
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        ${CTK_CORE_LIBRARY}
        ${CTK_PLUGIN_FRAMEWORK_LIBRARY}
        # 遵循CTK架构：不直接链接其他插件DLL
    )
    
    # 可选的第三方库支持
    if(THIRD_PARTY_LIB_FOUND)
        target_link_libraries(YourPlugin PRIVATE ${THIRD_PARTY_LIBRARIES})
        target_compile_definitions(YourPlugin PRIVATE THIRD_PARTY_LIB_FOUND)
    endif()
    
    # 包含目录（遵循完全CTK架构）
    target_include_directories(YourPlugin PRIVATE 
        ${CTK_INCLUDE_PATH}
        # 注意：完全CTK架构下不包含其他插件或共享数据结构
        # 所有通信通过CTK服务接口完成
    )
    
    # 链接目录
    target_link_directories(YourPlugin PRIVATE ${CTK_LIB_PATH})
    
    # 编译定义
    target_compile_definitions(YourPlugin PRIVATE 
        CTK_PLUGIN_FRAMEWORK
        CTK_PLUGIN_FRAMEWORK_EXPORT
    )
    
    message(STATUS "YourPlugin configured successfully (完全CTK架构)")
else()
    message(WARNING "CTK Plugin Framework not available - YourPlugin will not be built")
endif()
```

### **关键配置要点：**
1. **❌ 禁止直接链接其他插件DLL**
2. **❌ 禁止包含其他插件的头文件路径**
3. **✅ 仅链接Qt和CTK框架库**
4. **✅ 可选的第三方库支持**

---

## 📡 **数据传递方式**

### **ID驱动的数据引用：**
```cpp
// ✅ 推荐方式：通过ID引用数据
class MedicalProcessingService {
public:
    // 输入：图像ID，输出：结果图像ID
    virtual QString gaussianFilter(const QString& imageId, double sigma) = 0;
    
    // 通过UnifiedMedicalImageService获取实际数据
    // QString imageId → MedicalImageData* (内部调用)
};
```

### **参数传递标准：**
```cpp
// ✅ 灵活的参数配置
QVariantMap options;
options["sigma"] = 2.0;
options["kernelSize"] = 5;
options["preserveType"] = true;

QString resultId = processingService->gaussianFilter(imageId, options);
```

### **复杂数据结构传递：**
```cpp
// ✅ 使用QVariantMap序列化复杂结构
QVariantMap calibrationData;
calibrationData["points"] = QVariantList{
    QVariantMap{{"x", 100.0}, {"y", 200.0}, {"z", 300.0}},
    QVariantMap{{"x", 150.0}, {"y", 250.0}, {"z", 350.0}}
};
calibrationData["method"] = "pivot";
calibrationData["precision"] = "high";

QString calibrationId = trackingService->startCalibration(toolId, calibrationData);
```

---

## 🎨 **UI显示管理模式**

### **遵循PatientManagement成功模式：**
```cpp
class YourService {
public:
    // 标准UI显示接口
    virtual bool showMainDialog(QWidget* parent = nullptr) = 0;
    virtual bool showConfigDialog(QWidget* parent = nullptr) = 0;
    virtual bool showAdvancedDialog(QWidget* parent = nullptr) = 0;
    
    // 组件创建接口
    virtual QWidget* createMainWidget(QWidget* parent = nullptr) = 0;
    virtual QWidget* createControlPanel(QWidget* parent = nullptr) = 0;
};
```

### **UI实现标准：**
```cpp
bool YourServiceImpl::showMainDialog(QWidget* parent) {
    try {
        QDialog* dialog = new QDialog(parent);
        dialog->setWindowTitle("Your Plugin Main Interface");
        dialog->setModal(true);
        dialog->resize(800, 600);
        
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        
        // 添加主要内容
        // ...
        
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
        layout->addWidget(buttonBox);
        
        connect(buttonBox, &QDialogButtonBox::clicked, dialog, &QDialog::accept);
        
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}
```

---

## 🔗 **插件间通信模式**

### **服务发现和连接：**
```cpp
// ServiceInterfaces.h - 外部服务前向声明
class UnifiedMedicalImageService : public QObject {
    Q_OBJECT
public:
    virtual ~UnifiedMedicalImageService() = default;
    virtual QStringList getLoadedImages() const = 0;
    virtual bool isValid(const QString& imageId) const = 0;
};

Q_DECLARE_INTERFACE(UnifiedMedicalImageService, "medical.UnifiedMedicalImageService")
```

### **服务连接实现：**
```cpp
// YourServiceImpl.cpp
void YourServiceImpl::initializeExternalServiceConnections() {
    if (!m_pluginContext) return;
    
    try {
        // 查找外部服务（可选）
        m_imageServiceRef = m_pluginContext->getServiceReference<UnifiedMedicalImageService>();
        if (m_imageServiceRef) {
            m_imageService = m_pluginContext->getService<UnifiedMedicalImageService>(m_imageServiceRef);
            if (m_imageService) {
                m_imageServiceConnected = true;
                qDebug() << "成功连接到医学图像服务";
            }
        } else {
            qDebug() << "未找到医学图像服务（可选）";
        }
    } catch (const std::exception& e) {
        qCritical() << "服务连接失败:" << e.what();
    }
}
```

### **跨插件操作示例：**
```cpp
QString YourServiceImpl::processImageData(const QString& imageId) {
    // 1. 验证图像存在
    if (!m_imageService || !m_imageService->isValid(imageId)) {
        setError("图像不存在或无效");
        return QString();
    }
    
    // 2. 获取图像信息（不直接访问数据）
    QList<int> dimensions = m_imageService->getImageDimensions(imageId);
    QList<double> spacing = m_imageService->getImageSpacing(imageId);
    
    // 3. 执行处理逻辑
    QString resultId = performProcessing(imageId, dimensions, spacing);
    
    // 4. 返回结果ID
    return resultId;
}
```

---

## ⚠️ **错误处理和状态管理**

### **统一错误处理模式：**
```cpp
class YourServiceImpl {
private:
    QString m_lastError;
    mutable QMutex m_mutex;
    
    void setError(const QString& error) {
        QMutexLocker locker(&m_mutex);
        m_lastError = error;
        qWarning() << "[YourServiceImpl]" << error;
        emit errorOccurred("", error);  // 发出错误信号
    }
    
public:
    QString getLastError() const override {
        QMutexLocker locker(&m_mutex);
        return m_lastError;
    }
};
```

### **状态管理模式：**
```cpp
class YourServiceImpl {
private:
    enum class ServiceState {
        Uninitialized,
        Initializing,
        Ready,
        Error
    };
    
    ServiceState m_serviceState;
    QMap<QString, QString> m_componentStates;  // ID → 状态
    
public:
    QString getServiceStatus() const override {
        switch (m_serviceState) {
            case ServiceState::Uninitialized: return "未初始化";
            case ServiceState::Initializing: return "初始化中";
            case ServiceState::Ready: return "就绪";
            case ServiceState::Error: return "错误";
            default: return "未知";
        }
    }
    
    QString getComponentStatus(const QString& componentId) const override {
        return m_componentStates.value(componentId, "未知");
    }
};
```

---

## 🔄 **异步操作管理**

### **任务管理模式：**
```cpp
class YourServiceImpl {
private:
    struct TaskInfo {
        QString taskId;
        QString taskType;
        QString status;  // "running", "completed", "failed", "cancelled"
        QVariantMap parameters;
        QVariantMap result;
        qint64 startTime;
        int progress;
    };
    
    QMap<QString, TaskInfo> m_tasks;
    
public:
    QString startAsyncOperation(const QString& operation, const QVariantMap& params) {
        QString taskId = generateTaskId();
        
        TaskInfo info;
        info.taskId = taskId;
        info.taskType = operation;
        info.status = "running";
        info.parameters = params;
        info.startTime = QDateTime::currentMSecsSinceEpoch();
        info.progress = 0;
        
        m_tasks[taskId] = info;
        
        // 启动异步操作
        QTimer::singleShot(0, this, [this, taskId]() {
            executeAsyncTask(taskId);
        });
        
        emit operationStarted(taskId);
        return taskId;
    }
    
    QVariantMap getTaskStatus(const QString& taskId) const override {
        auto it = m_tasks.find(taskId);
        if (it == m_tasks.end()) return QVariantMap();
        
        QVariantMap status;
        status["taskId"] = it->taskId;
        status["taskType"] = it->taskType;
        status["status"] = it->status;
        status["progress"] = it->progress;
        status["startTime"] = it->startTime;
        
        return status;
    }
};
```

---

## 📋 **最佳实践指南**

### **✅ 推荐做法：**

1. **服务接口优先设计**
   ```cpp
   // 先设计接口，再实现具体功能
   class YourService { /* 纯虚接口 */ };
   class YourServiceImpl : public YourService { /* 具体实现 */ };
   ```

2. **ID驱动的数据管理**
   ```cpp
   // 使用ID引用，避免直接传递对象指针
   QString processData(const QString& dataId, const QVariantMap& options);
   ```

3. **可选的服务集成**
   ```cpp
   // 通过服务可用性检查实现可选集成
   if (m_imageService && m_imageService->isValid(imageId)) {
       // 使用图像服务
   } else {
       // 降级处理或提示
   }
   ```

4. **信号驱动的状态通知**
   ```cpp
   // 使用Qt信号通知状态变化
   emit statusChanged(componentId, newStatus);
   emit operationCompleted(taskId, result);
   ```

5. **线程安全的数据访问**
   ```cpp
   // 使用互斥锁保护共享数据
   QMutexLocker locker(&m_mutex);
   return m_sharedData.value(key);
   ```

### **❌ 避免做法：**

1. **直接链接其他插件DLL**
   ```cmake
   # ❌ 错误做法
   target_link_libraries(YourPlugin PRIVATE OtherPlugin)
   ```

2. **直接包含其他插件头文件**
   ```cpp
   // ❌ 错误做法
   #include "../OtherPlugin/OtherPluginClass.h"
   ```

3. **暴露内部数据结构**
   ```cpp
   // ❌ 错误做法
   virtual MedicalImageData* getImageData(const QString& id) = 0;
   ```

4. **硬编码的参数结构**
   ```cpp
   // ❌ 不够灵活
   struct FixedOptions { int param1; bool param2; };
   
   // ✅ 推荐方式
   QVariantMap flexibleOptions;
   ```

5. **阻塞式的长时间操作**
   ```cpp
   // ❌ 阻塞UI线程
   void longOperation() { /* 长时间同步操作 */ }
   
   // ✅ 异步操作
   QString startLongOperation() { /* 返回任务ID */ }
   ```

---

## 🏆 **架构优势对比**

| 特性 | 传统架构 | **完全CTK架构** |
|------|----------|----------------|
| **编译依赖** | 复杂依赖链 | ✅ **零依赖** |
| **运行时耦合** | 紧耦合 | ✅ **完全解耦** |
| **数据传递** | 对象指针 | ✅ **ID引用** |
| **参数配置** | 固定结构 | ✅ **灵活映射** |
| **错误传播** | 异常抛出 | ✅ **信号通知** |
| **插件加载** | 顺序依赖 | ✅ **动态发现** |
| **功能扩展** | 修改接口 | ✅ **向后兼容** |
| **测试难度** | 需要完整环境 | ✅ **独立测试** |
| **部署灵活性** | 全量部署 | ✅ **按需部署** |
| **开发效率** | 复杂调试 | ✅ **独立开发** |

---

## 🎯 **总结**

**完全CTK架构插件开发范式的核心价值：**

1. **🔧 零依赖编译** - 每个插件可独立编译和测试
2. **🔄 动态服务发现** - 运行时自动发现和连接服务
3. **📡 ID驱动通信** - 轻量级、类型安全的数据传递
4. **⚙️ 灵活参数配置** - QVariantMap支持任意参数组合
5. **🎨 标准化UI模式** - 统一的用户界面展示方式
6. **🛡️ 健壮错误处理** - 完整的错误传播和状态管理
7. **🚀 高性能异步** - 非阻塞的任务执行模式
8. **🔒 线程安全设计** - 多线程环境下的数据保护

**这套范式已在6个核心插件中验证成功，为医学图像处理软件的现代化架构提供了最佳实践标准。**

---

*本文档基于MedicalPro项目的实际重构经验编写，包含了从MedicalImageCore到OpticalTracking等所有核心插件的成功实践案例。*
