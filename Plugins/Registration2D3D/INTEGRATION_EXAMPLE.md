# Registration2D3D 插件 UI 集成示例

## 概述

本文档展示如何在主界面中集成 Registration2D3D 插件的 UI 组件。

## 集成步骤

### 1. 在主界面头文件中添加声明

在 `UI/MainInterfaceWidget.h` 中：

```cpp
#ifdef CTK_PLUGIN_FRAMEWORK
// 前向声明
class Registration2D3DService;
#endif
```

### 2. 在主界面中创建配准界面

在 `UI/MainInterfaceWidget.cpp` 的 `createSurgicalRegistrationTab()` 函数中：

```cpp
QWidget* MainInterfaceWidget::createSurgicalRegistrationTab(int patientId)
{
    qDebug() << "[MainInterfaceWidget] 创建2D3D配准标签页，患者ID:" << patientId;
    
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_ctkContext) {
        qWarning() << "[MainInterfaceWidget] CTK上下文未初始化";
        return createErrorWidget("CTK框架未初始化");
    }
    
    // 获取Registration2D3DService
    ctkServiceReference registrationRef = m_ctkContext->getServiceReference<Registration2D3DService>();
    if (!registrationRef) {
        qWarning() << "[MainInterfaceWidget] 未找到Registration2D3DService";
        return createErrorWidget("2D3D配准服务未启动\n请检查Registration2D3D插件");
    }
    
    Registration2D3DService* registrationService = m_ctkContext->getService<Registration2D3DService>(registrationRef);
    if (!registrationService) {
        qWarning() << "[MainInterfaceWidget] 获取Registration2D3DService失败";
        return createErrorWidget("2D3D配准服务获取失败");
    }
    
    // 通过服务接口创建配准Widget
    QWidget* registrationWidget = registrationService->createRegistrationWidget(nullptr);
    
    if (!registrationWidget) {
        qWarning() << "[MainInterfaceWidget] 配准Widget创建失败";
        return createErrorWidget("配准Widget创建失败");
    }
    
    // 如果需要，可以设置患者ID
    // 注意：这里需要将 patientId 转换为 QString
    // registrationWidget->setProperty("patientId", QString::number(patientId));
    
    qDebug() << "[MainInterfaceWidget] ✓ 2D3D配准Widget创建成功";
    return registrationWidget;
    
#else
    // 如果CTK框架未启用，返回一个提示Widget
    return createErrorWidget("CTK插件框架未启用\n2D3D配准不可用");
#endif
}

// 辅助函数：创建错误提示Widget
QWidget* MainInterfaceWidget::createErrorWidget(const QString& message)
{
    QWidget* errorWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(errorWidget);
    QLabel* errorLabel = new QLabel(message);
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setStyleSheet("color: #ef4444; font-size: 14px;");
    layout->addWidget(errorLabel);
    return errorWidget;
}
```

### 3. 完整的替换示例

将 `createSurgicalRegistrationTab()` 函数从当前的占位符实现替换为：

```cpp
QWidget* MainInterfaceWidget::createSurgicalRegistrationTab(int patientId)
{
    qDebug() << "[MainInterfaceWidget] 创建手术注册标签页...";
    
#ifdef CTK_PLUGIN_FRAMEWORK
    if (!m_ctkContext) {
        QWidget* errorWidget = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(errorWidget);
        QLabel* errorLabel = new QLabel("CTK框架未初始化");
        errorLabel->setAlignment(Qt::AlignCenter);
        errorLabel->setStyleSheet("color: #ef4444; font-size: 14px;");
        layout->addWidget(errorLabel);
        return errorWidget;
    }
    
    // 创建标签页容器
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    
    // 顶部按钮栏
    QHBoxLayout* topButtonLayout = new QHBoxLayout();
    
    QPushButton* config2D3DBtn = new QPushButton("2D-3D配准");
    QPushButton* opticalBtn = new QPushButton("光学配准");
    QPushButton* imageProcessBtn = new QPushButton("图片处理");
    
    config2D3DBtn->setCheckable(true);
    opticalBtn->setCheckable(true);
    imageProcessBtn->setCheckable(true);
    config2D3DBtn->setChecked(true);
    
    QString regButtonStyle = 
        "QPushButton { "
        "  background: rgba(59,130,246,0.7); "
        "  color: #fff; "
        "  border: 2px solid transparent; "
        "  border-radius: 8px; "
        "  padding: 10px 20px; "
        "  font-weight: bold; "
        "  font-size: 13px; "
        "  min-width: 100px; "
        "}"
        "QPushButton:checked { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "    stop:0 #1d4ed8, stop:1 #3b82f6); "
        "  border: 2px solid #fbbf24; "
        "}";
    
    config2D3DBtn->setStyleSheet(regButtonStyle);
    opticalBtn->setStyleSheet(regButtonStyle);
    imageProcessBtn->setStyleSheet(regButtonStyle);
    
    topButtonLayout->addWidget(config2D3DBtn);
    topButtonLayout->addWidget(opticalBtn);
    topButtonLayout->addWidget(imageProcessBtn);
    topButtonLayout->addStretch();
    
    layout->addLayout(topButtonLayout);
    
    // 创建内容堆栈
    QStackedWidget* contentStack = new QStackedWidget();
    
    // 获取Registration2D3DService并创建Widget
    ctkServiceReference registrationRef = m_ctkContext->getServiceReference<Registration2D3DService>();
    if (registrationRef) {
        Registration2D3DService* registrationService = m_ctkContext->getService<Registration2D3DService>(registrationRef);
        if (registrationService) {
            QWidget* registrationWidget = registrationService->createRegistrationWidget(nullptr);
            if (registrationWidget) {
                contentStack->addWidget(registrationWidget);  // 2D-3D配准页面
                qDebug() << "[MainInterfaceWidget] ✓ 2D3D配准Widget已添加";
            } else {
                contentStack->addWidget(new QLabel("配准Widget创建失败"));
            }
        } else {
            contentStack->addWidget(new QLabel("配准服务获取失败"));
        }
    } else {
        contentStack->addWidget(new QLabel("配准服务未找到"));
    }
    
    // 其他页面（占位符）
    contentStack->addWidget(new QLabel("光学配准 - 开发中"));
    contentStack->addWidget(new QLabel("图片处理 - 开发中"));
    
    layout->addWidget(contentStack);
    
    // 按钮切换
    QButtonGroup* buttonGroup = new QButtonGroup();
    buttonGroup->addButton(config2D3DBtn, 0);
    buttonGroup->addButton(opticalBtn, 1);
    buttonGroup->addButton(imageProcessBtn, 2);
    
    connect(buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), 
            [contentStack](int id) {
        contentStack->setCurrentIndex(id);
    });
    
    return tab;
    
#else
    QWidget* errorWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(errorWidget);
    QLabel* errorLabel = new QLabel("CTK插件框架未启用\n2D3D配准不可用");
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setStyleSheet("color: #ef4444; font-size: 14px;");
    layout->addWidget(errorLabel);
    return errorWidget;
#endif
}
```

## 使用方法

1. 编译 Registration2D3D 插件
2. 在主界面的 `initializeServicesDelayed()` 中不需要特殊处理（服务会自动注册）
3. 程序运行时，进入手术导航界面的"手术注册"标签
4. 2D3D配准UI 会自动显示

## 功能特性

创建的 Widget 包含：

- **顶部控制栏**：加载图像、开始配准、取消配准等按钮
- **左侧参数区**：配准算法选择、参数设置、进度显示、结果展示
- **右侧图像区**：AP视图、LAT视图、CT视图、配准结果（2x2布局）

## API 示例

### 设置患者ID

```cpp
Registration2D3DWidget* widget = qobject_cast<Registration2D3DWidget*>(registrationWidget);
if (widget) {
    widget->setPatientId(QString::number(patientId));
}
```

### 加载图像

```cpp
widget->loadXRayImages("/path/to/ap.jpg", "/path/to/lat.jpg");
widget->loadCTImage("/path/to/ct/dicom/dir");
```

### 监听信号

```cpp
connect(widget, &Registration2D3DWidget::registrationCompleted,
        this, [](const Registration2D3DResult& result) {
    qDebug() << "配准完成，相似度:" << result.finalSimilarityScore;
});
```

## 注意事项

1. **Python 依赖**：确保 `python39.dll` 在程序目录中
2. **服务检查**：始终检查服务是否可用再创建 Widget
3. **生命周期**：Widget 由父容器管理，无需手动删除
4. **线程安全**：UI 操作必须在主线程

## 完整示例项目

参考 `FourViewDisplay` 插件的集成方式，它使用了相同的架构模式。

