# CTK组件集成增强方案

## 当前CTK配置状态

### 已配置组件
- **CTKCore**: 核心功能库
- **CTKWidgets**: 基础UI组件
- **CTKPluginFramework**: 插件框架

### 可增强集成的CTK组件

## 1. DICOM数据处理增强

### ctkDICOMCore + ctkDICOMWidgets
**适用场景**: 医学影像数据管理和显示
```cmake
# CMakeLists.txt 中添加
set(CTK_LIBRARIES 
    CTKCore 
    CTKWidgets 
    CTKPluginFramework
    CTKDICOMCore          # 新增：DICOM数据处理
    CTKDICOMWidgets       # 新增：DICOM UI组件
)
```

**功能增强**:
- DICOM文件导入/导出
- DICOM数据库管理
- 患者信息提取
- 系列和图像组织
- DICOM标签编辑

### 实现示例：DICOM管理插件
```cpp
// DicomManagerServiceImpl.h
#include <ctkDICOMDatabase.h>
#include <ctkDICOMModel.h>
#include <ctkDICOMBrowser.h>

class DicomManagerServiceImpl : public QObject
{
    Q_OBJECT
public:
    void initializeDicomDatabase();
    void importDicomData(const QString& directory);
    QStringList getPatientStudies(const QString& patientID);
    
private:
    ctkDICOMDatabase* m_dicomDB;
    ctkDICOMModel* m_dicomModel;
};
```

## 2. VTK集成增强

### ctkVTKWidgets
**当前状态**: 项目中使用原生 QVTKOpenGLNativeWidget
**增强方案**: 使用 CTK 的 VTK 集成组件

```cmake
set(CTK_LIBRARIES 
    # ... 现有组件
    CTKVisualizationVTKCore      # VTK核心集成
    CTKVisualizationVTKWidgets   # VTK UI组件
)
```

**功能优势**:
- 更好的VTK生命周期管理
- 内置的相机控制
- 标准化的3D交互
- 性能优化的渲染管道

### 实现示例
```cpp
// EnhancedTrackingWidget 升级
#include <ctkVTKRenderView.h>
#include <ctkVTKVolumePropertyWidget.h>

class EnhancedTrackingWidget : public QWidget
{
private:
    ctkVTKRenderView* m_renderView;           // 替代 QVTKOpenGLNativeWidget
    ctkVTKVolumePropertyWidget* m_volProperty; // 体渲染属性控制
};
```

## 3. 日志和错误管理系统

### ctkLogger + ctkErrorLogWidget
```cmake
set(CTK_LIBRARIES 
    # ... 现有组件
    CTKCore              # 包含日志功能
    CTKWidgets           # 包含错误显示组件
)
```

**集成到MainWindow**:
```cpp
// mainwindow.h
#include <ctkErrorLogWidget.h>
#include <ctkLogger.h>

class MainWindow : public QMainWindow
{
private:
    ctkErrorLogWidget* m_errorLogWidget;
    
    void setupAdvancedLogging();
    void setupErrorLogDockWidget();
};

// mainwindow.cpp
void MainWindow::setupAdvancedLogging()
{
    // 替代现有的简单日志记录
    ctkLogger::instance()->setLevel(ctkLogger::DEBUG);
    
    // 设置日志文件
    QString logPath = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/logs";
    QDir().mkpath(logPath);
    ctkLogger::instance()->setLogFile(logPath + "/medicalpro.log");
}

void MainWindow::setupErrorLogDockWidget()
{
    // 创建错误日志停靠窗口
    m_errorLogWidget = new ctkErrorLogWidget();
    
    QDockWidget* errorLogDock = new QDockWidget("错误日志", this);
    errorLogDock->setWidget(m_errorLogWidget);
    errorLogDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    
    addDockWidget(Qt::BottomDockWidgetArea, errorLogDock);
    errorLogDock->hide(); // 默认隐藏，出错时显示
}
```

## 4. 设置管理系统升级

### ctkSettings
**当前问题**: 使用QSettings进行简单配置
**增强方案**: 使用CTK的高级设置管理

```cpp
// 新建 SettingsManager.h
#include <ctkSettings.h>

class SettingsManager : public QObject
{
    Q_OBJECT
public:
    static SettingsManager* instance();
    
    // 光学追踪设置
    void saveTrackingSettings(const TrackingConfig& config);
    TrackingConfig loadTrackingSettings();
    
    // DICOM设置
    void saveDicomSettings(const DicomConfig& config);
    DicomConfig loadDicomSettings();
    
    // UI设置
    void saveWindowGeometry(const QString& windowName, const QByteArray& geometry);
    QByteArray loadWindowGeometry(const QString& windowName);

private:
    ctkSettings* m_settings;
};
```

## 5. 高级UI组件集成

### ctkWidgets 扩展使用
当前已配置，但可以更充分利用：

```cpp
// 在 mainwindow.cpp 中使用更多CTK组件
#include <ctkCollapsibleButton.h>
#include <ctkCollapsibleGroupBox.h>
#include <ctkDoubleSpinBox.h>
#include <ctkSliderWidget.h>
#include <ctkRangeWidget.h>
#include <ctkCheckableHeaderView.h>
#include <ctkTreeComboBox.h>

void MainWindow::setupAdvancedUI()
{
    // 可折叠的参数面板
    ctkCollapsibleGroupBox* trackingParams = 
        new ctkCollapsibleGroupBox("光学追踪参数");
    trackingParams->setCollapsed(false);
    
    // 精确的双精度输入
    ctkDoubleSpinBox* precisionSpinBox = new ctkDoubleSpinBox();
    precisionSpinBox->setDecimals(6);  // 6位小数精度
    precisionSpinBox->setSuffix(" mm");
    
    // 范围滑块控制
    ctkSliderWidget* thresholdSlider = new ctkSliderWidget();
    thresholdSlider->setRange(0.0, 10.0);
    thresholdSlider->setSingleStep(0.1);
    
    // 范围选择器
    ctkRangeWidget* calibrationRange = new ctkRangeWidget();
    calibrationRange->setRange(0.0, 100.0);
    calibrationRange->setValues(10.0, 90.0);
}
```

## 6. 命令行参数处理

### ctkCommandLineParser
**适用场景**: 支持批量处理、自动化测试
```cpp
// main.cpp 中添加
#include <ctkCommandLineParser.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    ctkCommandLineParser parser;
    parser.setArgumentPrefix("--", "-");
    
    // 添加命令行选项
    parser.addArgument("config", "c", QVariant::String, 
                      "指定配置文件路径");
    parser.addArgument("patient-data", "p", QVariant::String,
                      "指定患者数据目录"); 
    parser.addArgument("auto-connect", "", QVariant::Bool,
                      "自动连接光学追踪设备");
    parser.addArgument("log-level", "l", QVariant::String,
                      "设置日志级别 (DEBUG/INFO/WARN/ERROR)");
                      
    bool ok = false;
    QHash<QString, QVariant> arguments = parser.parseArguments(argc, argv, &ok);
    
    if (!ok) {
        qCritical() << parser.errorString();
        return -1;
    }
    
    // 根据命令行参数配置应用
    if (arguments.contains("auto-connect")) {
        // 设置自动连接标志
    }
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
```

## 7. 工作流管理 (可选高级功能)

### ctkWorkflow
**适用场景**: 标准化的医疗操作流程
```cpp
// 新建 MedicalWorkflowManager.h
#include <ctkWorkflow.h>
#include <ctkWorkflowStep.h>
#include <ctkWorkflowWidget.h>

class MedicalWorkflowManager : public QObject
{
    Q_OBJECT
public:
    void setupDiagnosisWorkflow();
    void setupCalibrationWorkflow();
    
private:
    ctkWorkflow* m_diagnosisWorkflow;
    ctkWorkflow* m_calibrationWorkflow;
    
    // 工作流步骤
    void createPatientRegistrationStep();
    void createImageAcquisitionStep();
    void createCalibrationStep();
    void createDiagnosisStep();
    void createReportGenerationStep();
};
```

## CMakeLists.txt 完整配置

```cmake
# 在主项目的 CMakeLists.txt 中
if(CTK_FOUND)
    # 扩展CTK组件列表
    set(CTK_LIBRARIES 
        CTKCore 
        CTKWidgets 
        CTKPluginFramework
        CTKDICOMCore                    # DICOM处理
        CTKDICOMWidgets                 # DICOM UI
        CTKVisualizationVTKCore         # VTK集成
        CTKVisualizationVTKWidgets      # VTK UI
        # 如果需要工作流功能：
        # CTKWorkflow                   
    )
    
    # 链接到主程序
    target_link_libraries(medicalpro PRIVATE ${CTK_LIBRARIES})
    
    # 添加必要的编译定义
    add_definitions(-DCTK_DICOM_SUPPORT)
    add_definitions(-DCTK_VTK_SUPPORT)
endif()
```

## 实施优先级建议

### 第一阶段 (立即可实施)
1. **日志系统增强** - ctkLogger + ctkErrorLogWidget
2. **设置管理升级** - ctkSettings
3. **UI组件丰富** - 更多ctkWidgets组件

### 第二阶段 (中期实施)
1. **DICOM集成** - ctkDICOMCore + ctkDICOMWidgets
2. **VTK增强** - ctkVTKWidgets替代当前方案
3. **命令行支持** - ctkCommandLineParser

### 第三阶段 (长期规划)
1. **工作流管理** - ctkWorkflow (如果需要标准化流程)
2. **更多专业医疗组件** - 根据具体需求

## 预期收益

1. **更专业的医疗软件架构**
2. **标准化的DICOM数据处理**
3. **更丰富的用户界面交互**
4. **完善的日志和错误处理**
5. **更好的系统可维护性**
6. **符合医疗软件规范**

这些CTK组件的集成将显著提升你的医疗软件的专业性和功能完整性！
