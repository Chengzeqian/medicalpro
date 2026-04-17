#include "InstrumentPreviewDialog.h"
#include "Plugins/InstrumentManagement/InstrumentManagementService.h"
#include "UI/Widgets/Instrument3DPreviewWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QPixmap>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QFileInfo>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QProgressDialog>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QTimer>

InstrumentPreviewDialog::InstrumentPreviewDialog(InstrumentManagementService* service, QWidget *parent)
    : QDialog(parent)
    , m_titleLabel(nullptr)
    , m_imageLabel(nullptr)
    , m_loadingLabel(nullptr)
    , m_resetViewBtn(nullptr)
    , m_projectionBtn(nullptr)
    , m_lightingBtn(nullptr)
    , m_switchModeBtn(nullptr)
    , m_controlBar(nullptr)
    , m_instrumentService(service)
    , m_currentMode(Interactive3DMode)  // 默认使用3D模式
    , m_instrumentId(-1)
#ifdef VTK_FOUND
    , m_3dPreviewWidget(nullptr)
    , m_isOrthographic(false)
    , m_isThreePointLighting(true)
#endif
{
    setWindowTitle("器械预览");
    // 使用真正的顶层对话框窗口，避免继承父窗口的任何透明 / 特效属性
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    setModal(true);
    setFixedSize(900, 1000);

    // 设置对话框属性
    setAttribute(Qt::WA_DeleteOnClose);
    setAutoFillBackground(true);

    // 设置深色背景
    QPalette dialogPalette = palette();
    dialogPalette.setColor(QPalette::Window, QColor(30, 41, 59));  // #1e293b
    setPalette(dialogPalette);

    // 🔥 样式表层面强制所有子控件也使用不透明背景，覆盖任何全局 "background: transparent"
    setStyleSheet(
        "QDialog, QDialog * {"
        "    background-color: #1e293b;"
        "}"
        "QWidget#previewContainer {"
        "    background-color: #1e293b;"
        "}"
        "Instrument3DPreviewWidget {"
        "    background-color: #191926;"
        "}"
    );

    qDebug() << "[InstrumentPreviewDialog] 🔥 Dialog不透明属性已设置（包括所有子控件）";

    setupUI();
}

InstrumentPreviewDialog::~InstrumentPreviewDialog()
{
    qDebug() << "[InstrumentPreviewDialog] 销毁预览对话框";
}

void InstrumentPreviewDialog::paintEvent(QPaintEvent* event)
{
    // 强制填充整个对话框背景，彻底阻止任何“透明洞”效果
    QPainter painter(this);
    QColor bg = palette().color(QPalette::Window);
    painter.fillRect(rect(), bg);

    // 让基类继续绘制子控件等
    QDialog::paintEvent(event);
}

void InstrumentPreviewDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(20);

    // 标题栏
    QWidget* titleBar = new QWidget();
    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(15);

    m_titleLabel = new QLabel("器械预览");
    m_titleLabel->setStyleSheet(
        "color: #f0f9ff;"
        "font-size: 24px;"
        "font-weight: bold;"
        "background: transparent;"
    );


    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();

    mainLayout->addWidget(titleBar);

    // 静态图片显示区域
    m_imageLabel = new QLabel();
    m_imageLabel->setFixedSize(840, 840);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet(
        "background: rgba(15,23,42,0.5);"
        "border: 1px solid rgba(100,116,139,0.3);"
        "border-radius: 12px;"
    );
    m_imageLabel->hide();  // 默认隐藏

    mainLayout->addWidget(m_imageLabel, 0, Qt::AlignCenter);

#ifdef VTK_FOUND
    // 3D交互预览区域：使用 Instrument3DPreviewWidget（内部封装全部 VTK 逻辑）
    qDebug() << "[InstrumentPreviewDialog] 创建 Instrument3DPreviewWidget 作为3D预览区域";
    m_3dPreviewWidget = new Instrument3DPreviewWidget(this);
    if (!m_3dPreviewWidget) {
        qCritical() << "[InstrumentPreviewDialog] ❌ 创建 Instrument3DPreviewWidget 失败，3D预览将不可用";
    } else {
        m_3dPreviewWidget->setMinimumSize(840, 840);
        m_3dPreviewWidget->hide();  // 默认隐藏，等待加载模型后显示
        mainLayout->addWidget(m_3dPreviewWidget, 0, Qt::AlignCenter);
    }

    // 3D控制按钮栏
    m_controlBar = new QWidget();
    QHBoxLayout* controlLayout = new QHBoxLayout(m_controlBar);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(15);

    QString btnStyle =
        "QPushButton {"
        "    background: rgba(59,130,246,0.7);"
        "    color: white;"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 10px 20px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background: rgba(37,99,235,0.9);"
        "}"
        "QPushButton:pressed {"
        "    background: rgba(29,78,216,1.0);"
        "}";

    m_resetViewBtn = new QPushButton("");
    m_resetViewBtn->setCursor(Qt::PointingHandCursor);
    m_resetViewBtn->setStyleSheet(btnStyle);
    m_resetViewBtn->setText("重置视角");

    connect(m_resetViewBtn, &QPushButton::clicked, this, &InstrumentPreviewDialog::onResetView);

    m_projectionBtn = new QPushButton("");
    m_projectionBtn->setCursor(Qt::PointingHandCursor);
    m_projectionBtn->setStyleSheet(btnStyle);
    m_projectionBtn->setText("透视投影");

    connect(m_projectionBtn, &QPushButton::clicked, this, &InstrumentPreviewDialog::onToggleProjection);

    m_lightingBtn = new QPushButton("三点照明");
    m_lightingBtn->setCursor(Qt::PointingHandCursor);
    m_lightingBtn->setStyleSheet(btnStyle);
    connect(m_lightingBtn, &QPushButton::clicked, this, &InstrumentPreviewDialog::onToggleLighting);

    controlLayout->addStretch();
    controlLayout->addWidget(m_resetViewBtn);
    controlLayout->addWidget(m_projectionBtn);
    controlLayout->addWidget(m_lightingBtn);
    controlLayout->addStretch();

    m_controlBar->hide();  // 默认隐藏
    mainLayout->addWidget(m_controlBar);

    // 操作提示
    QLabel* hintLabel = new QLabel(
        "💡 操作提示：左键旋转 | 右键平移 | 滚轮缩放"
    );
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setStyleSheet(
        "color: #94a3b8;"
        "font-size: 13px;"
        "background: transparent;"
    );
    hintLabel->hide();  // 默认隐藏
    hintLabel->setObjectName("hintLabel");
    mainLayout->addWidget(hintLabel);
#endif

    // 模式切换按钮（放在底部）
    QString switchBtnStyle =
        "QPushButton {"
        "    background: rgba(16,185,129,0.7);"
        "    color: white;"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px 24px;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background: rgba(5,150,105,0.9);"
        "}"
        "QPushButton:pressed {"
        "    background: rgba(4,120,87,1.0);"
        "}";

    m_switchModeBtn = new QPushButton("切换到静态预览");
    m_switchModeBtn->setCursor(Qt::PointingHandCursor);
    m_switchModeBtn->setStyleSheet(switchBtnStyle);
    m_switchModeBtn->setFixedHeight(50);
    connect(m_switchModeBtn, &QPushButton::clicked, this, &InstrumentPreviewDialog::onSwitchViewMode);

    mainLayout->addWidget(m_switchModeBtn, 0, Qt::AlignCenter);
}

void InstrumentPreviewDialog::setPreviewContent(const QString& instrumentName, const QString& modelFilePath, int instrumentId)
{
    qDebug() << "[InstrumentPreviewDialog] ========== 设置预览内容 ==========";
    qDebug() << "[InstrumentPreviewDialog] 器械名称:" << instrumentName;
    qDebug() << "[InstrumentPreviewDialog] 模型文件路径:" << modelFilePath;
    qDebug() << "[InstrumentPreviewDialog] 器械ID:" << instrumentId;
    qDebug() << "[InstrumentPreviewDialog] 文件是否存在:" << QFile::exists(modelFilePath);

    // 保存数据
    m_modelFilePath = modelFilePath;
    m_instrumentId = instrumentId;

    // 更新标题（名称 + ID）
    m_titleLabel->setText(QString("器械预览 - %1（ID: %2）").arg(instrumentName).arg(instrumentId));

#ifdef VTK_FOUND
    // 检查文件是否存在
    if (!QFile::exists(modelFilePath)) {
        qCritical() << "[InstrumentPreviewDialog] ❌ 模型文件不存在:" << modelFilePath;
        QMessageBox::warning(this, "错误", "模型文件不存在:\n" + modelFilePath);
        return;
    }

    // 检查文件大小
    QFileInfo fileInfo(modelFilePath);
    qDebug() << "[InstrumentPreviewDialog] 文件大小:" << fileInfo.size() << "字节";
    qDebug() << "[InstrumentPreviewDialog] 文件扩展名:" << fileInfo.suffix();

    // 把模型路径传递给 3D 预览组件
    if (m_3dPreviewWidget) {
        qDebug() << "[InstrumentPreviewDialog] 将模型路径传递给 Instrument3DPreviewWidget";
        m_3dPreviewWidget->setModelFilePath(modelFilePath);
    } else {
        qWarning() << "[InstrumentPreviewDialog] ⚠️ m_3dPreviewWidget 为空，无法进行3D预览";
    }

    // 默认启动静态预览模式
    qDebug() << "[InstrumentPreviewDialog] 准备切换到静态预览模式...";
    switchToStaticMode();
#else
    QMessageBox::warning(this, "错误", "VTK未启用，无法显示3D模型");
#endif
}

void InstrumentPreviewDialog::keyPressEvent(QKeyEvent* event)
{
    // ESC键关闭对话框
    if (event->key() == Qt::Key_Escape) {
        reject();
    } else {
        QDialog::keyPressEvent(event);
    }
}

#ifdef VTK_FOUND
#if 0  // Legacy VTK渲染管线（已由 Instrument3DPreviewWidget 接管），保留以备参考

void InstrumentPreviewDialog::setupVTKPipeline(const QString& modelFilePath)
{
    qDebug() << "[InstrumentPreviewDialog] ========== 开始设置VTK渲染管线 ==========";
    qDebug() << "[InstrumentPreviewDialog] 模型文件路径:" << modelFilePath;
    qDebug() << "[InstrumentPreviewDialog] 文件是否存在:" << QFile::exists(modelFilePath);

    // ✅ 诊断：检查VTK Widget状态
    qDebug() << "[InstrumentPreviewDialog] VTK Widget尺寸:" << m_vtkWidget->width() << "x" << m_vtkWidget->height();
    qDebug() << "[InstrumentPreviewDialog] VTK Widget可见性:" << m_vtkWidget->isVisible();
    qDebug() << "[InstrumentPreviewDialog] 对话框尺寸:" << this->width() << "x" << this->height();
    qDebug() << "[InstrumentPreviewDialog] 对话框可见性:" << this->isVisible();

    // 1. 读取STL模型
    m_stlReader = vtkSTLReader::New();
    m_stlReader->SetFileName(modelFilePath.toStdString().c_str());
    m_stlReader->Update();

    // ✅ 验证STL读取是否成功
    vtkPolyData* polyData = m_stlReader->GetOutput();
    if (!polyData || polyData->GetNumberOfPoints() == 0) {
        qCritical() << "[InstrumentPreviewDialog] ❌ STL文件读取失败或模型为空！";
        QMessageBox::critical(this, "错误", "无法读取STL模型文件，请检查文件格式。");
        return;
    }

    qDebug() << "[InstrumentPreviewDialog] ✅ STL读取成功";
    qDebug() << "[InstrumentPreviewDialog] 模型点数:" << polyData->GetNumberOfPoints();
    qDebug() << "[InstrumentPreviewDialog] 模型单元数:" << polyData->GetNumberOfCells();

    double bounds[6];
    polyData->GetBounds(bounds);
    qDebug() << "[InstrumentPreviewDialog] 模型边界框: X[" << bounds[0] << "," << bounds[1] << "]"
             << "Y[" << bounds[2] << "," << bounds[3] << "]"
             << "Z[" << bounds[4] << "," << bounds[5] << "]";

    // 2. 数据预处理
    vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputConnection(m_stlReader->GetOutputPort());

    vtkSmartPointer<vtkTriangleFilter> triangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
    triangleFilter->SetInputConnection(cleaner->GetOutputPort());

    vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
    normals->SetInputConnection(triangleFilter->GetOutputPort());
    normals->ComputePointNormalsOn();
    normals->ComputeCellNormalsOn();
    normals->SplittingOff();
    normals->ConsistencyOn();
    normals->AutoOrientNormalsOn();
    normals->Update();

    qDebug() << "[InstrumentPreviewDialog] ✅ 数据预处理完成";

    // 3. 创建Mapper
    m_mapper = vtkPolyDataMapper::New();
    m_mapper->SetInputConnection(normals->GetOutputPort());
    qDebug() << "[InstrumentPreviewDialog] ✅ Mapper创建完成";

    // 4. 创建Actor并应用金属材质
    m_actor = vtkActor::New();
    m_actor->SetMapper(m_mapper);
    applyMetallicMaterial();

    // ✅ 验证Actor边界框
    double actorBounds[6];
    m_actor->GetBounds(actorBounds);
    qDebug() << "[InstrumentPreviewDialog] Actor边界框: X[" << actorBounds[0] << "," << actorBounds[1] << "]"
             << "Y[" << actorBounds[2] << "," << actorBounds[3] << "]"
             << "Z[" << actorBounds[4] << "," << actorBounds[5] << "]";

    // 5. 创建Renderer
    m_renderer = vtkRenderer::New();
    m_renderer->AddActor(m_actor);

    // 为了方便肉眼观察，使用非常显眼的背景色（洋红色），便于区分是否真的在绘制VTK内容
    m_renderer->SetBackground(1.0, 0.0, 1.0);  // 亮洋红色背景
    m_renderer->SetBackgroundAlpha(1.0);       // 完全不透明
    qDebug() << "[InstrumentPreviewDialog] ✅ Renderer创建完成，背景色:"
             << m_renderer->GetBackground()[0] << m_renderer->GetBackground()[1] << m_renderer->GetBackground()[2];

    // === 调试: 添加一个简单的红色球体，验证渲染管线是否真正输出 ===
    vtkSmartPointer<vtkSphereSource> debugSphere = vtkSmartPointer<vtkSphereSource>::New();
    debugSphere->SetCenter(0.0, 0.0, 0.0);
    debugSphere->SetRadius(10.0);
    debugSphere->SetThetaResolution(32);
    debugSphere->SetPhiResolution(32);
    debugSphere->Update();

    vtkSmartPointer<vtkPolyDataMapper> debugMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    debugMapper->SetInputConnection(debugSphere->GetOutputPort());

    vtkSmartPointer<vtkActor> debugActor = vtkSmartPointer<vtkActor>::New();
    debugActor->SetMapper(debugMapper);
    debugActor->GetProperty()->SetColor(1.0, 0.0, 0.0);  // 纯红色
    debugActor->GetProperty()->SetOpacity(1.0);
    m_renderer->AddActor(debugActor);

    qDebug() << "[InstrumentPreviewDialog] 调试: 已添加红色测试球体Actor";

    // 6. 设置三点照明
    setupThreePointLighting();

    // 7. 获取或创建RenderWindow（优先使用VTKWidgetFactory为QVTKWidget配置的RenderWindow）
    vtkRenderWindow* renderWindow = m_vtkWidget ? m_vtkWidget->renderWindow() : nullptr;
    if (!renderWindow) {
        qWarning() << "[InstrumentPreviewDialog] ⚠️ VTK Widget当前没有RenderWindow，创建新的vtkGenericOpenGLRenderWindow";
        vtkSmartPointer<vtkGenericOpenGLRenderWindow> newRenderWindow =
            vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        m_vtkWidget->setRenderWindow(newRenderWindow);
        renderWindow = m_vtkWidget->renderWindow();
    }

    m_renderWindow = renderWindow;
    renderWindow->AddRenderer(m_renderer);
    renderWindow->SetAlphaBitPlanes(0);  // 禁用Alpha通道
    renderWindow->SetMultiSamples(8);    // 8x抗锯齿

    // ✅ 设置渲染窗口尺寸（与VTK Widget尺寸一致）
    int widgetWidth = m_vtkWidget->width();
    int widgetHeight = m_vtkWidget->height();
    renderWindow->SetSize(widgetWidth, widgetHeight);
    renderWindow->SetOffScreenRendering(0);  // 确保交互式模式禁用离屏渲染
    qDebug() << "[InstrumentPreviewDialog] ✅ RenderWindow准备完成，尺寸:" << widgetWidth << "x" << widgetHeight;
    qDebug() << "[InstrumentPreviewDialog] RenderWindow::GetOffScreenRendering =" << renderWindow->GetOffScreenRendering();

    // ✅ 确保VTK Widget可见
    if (!m_vtkWidget->isVisible()) {
        qWarning() << "[InstrumentPreviewDialog] ⚠️ VTK Widget当前不可见，强制显示";
        m_vtkWidget->show();
    }

    // 8. 设置交互器
    m_interactor = renderWindow->GetInteractor();
    qDebug() << "[InstrumentPreviewDialog] ✅ 交互器获取完成";

    // 9. 设置交互样式（轨迹球相机）
    vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
        vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    m_interactor->SetInteractorStyle(style);
    qDebug() << "[InstrumentPreviewDialog] ✅ 交互样式设置完成";

    // 10. 重置相机以适应模型
    m_renderer->ResetCamera();

    vtkCamera* camera = m_renderer->GetActiveCamera();
    double* position = camera->GetPosition();
    double* focalPoint = camera->GetFocalPoint();
    qDebug() << "[InstrumentPreviewDialog] 相机位置:" << position[0] << position[1] << position[2];
    qDebug() << "[InstrumentPreviewDialog] 相机焦点:" << focalPoint[0] << focalPoint[1] << focalPoint[2];

    // 11. 调整相机距离，确保模型可见
    camera->Zoom(1.2);  // 稍微拉近一点
    qDebug() << "[InstrumentPreviewDialog] ✅ 相机已重置并缩放";

    // 12. 初始化交互器（重要！）
    m_interactor->Initialize();
    qDebug() << "[InstrumentPreviewDialog] ✅ 交互器已初始化";

    // 13. 启动渲染
    renderWindow->Render();
    qDebug() << "[InstrumentPreviewDialog] ✅ 首次渲染完成";

    // 14. 刷新VTK Widget
    m_vtkWidget->update();
    qDebug() << "[InstrumentPreviewDialog] ✅ VTK Widget已刷新";

    // ✅ 诊断：检查OpenGL上下文
    qDebug() << "[InstrumentPreviewDialog] === OpenGL上下文诊断 ===";
    qDebug() << "[InstrumentPreviewDialog] RenderWindow是否已初始化:" << (renderWindow && renderWindow->GetInteractor() != nullptr);
    qDebug() << "[InstrumentPreviewDialog] VTK Widget是否有效:" << m_vtkWidget->isValid();
    qDebug() << "[InstrumentPreviewDialog] VTK Widget是否可见:" << m_vtkWidget->isVisible();

    // 使用项目提供的VTKContextValidator进一步验证OpenGL上下文
    auto validationResult = VTKContextValidator::validateVTKWidget(m_vtkWidget);
    qDebug() << "[InstrumentPreviewDialog] OpenGL上下文是否有效:" << validationResult.isValid;
    if (!validationResult.errorMessage.isEmpty()) {
        qDebug() << "[InstrumentPreviewDialog] OpenGL错误信息:" << validationResult.errorMessage;
    }
    qDebug() << "[InstrumentPreviewDialog] OpenGL版本:" << validationResult.openGLMajorVersion << "." << validationResult.openGLMinorVersion;
    qDebug() << "[InstrumentPreviewDialog] OpenGL供应商:" << validationResult.vendor;
    qDebug() << "[InstrumentPreviewDialog] OpenGL渲染器:" << validationResult.renderer;

    // ✅ 再次强制渲染（确保OpenGL上下文已激活）
    qDebug() << "[InstrumentPreviewDialog] 再次强制渲染...";
    m_vtkWidget->renderWindow()->Render();
    m_vtkWidget->update();
    qDebug() << "[InstrumentPreviewDialog] ✅ 再次渲染完成";

    qDebug() << "[InstrumentPreviewDialog] ========== VTK渲染管线设置完成 ==========";
    qDebug() << "[InstrumentPreviewDialog] Actor可见性:" << m_actor->GetVisibility();
    qDebug() << "[InstrumentPreviewDialog] Actor数量:" << m_renderer->GetActors()->GetNumberOfItems();
    qDebug() << "[InstrumentPreviewDialog] 光源数量:" << m_renderer->GetLights()->GetNumberOfItems();
}

void InstrumentPreviewDialog::applyMetallicMaterial()
{
    if (!m_actor) return;

    vtkProperty* property = m_actor->GetProperty();

    // 金属材质（钢铁效果）
    property->SetColor(0.75, 0.75, 0.78);      // 银灰色
    property->SetMetallic(0.9);                 // 高金属度
    property->SetRoughness(0.2);                // 低粗糙度（光滑）
    property->SetSpecular(0.8);                 // 高镜面反射
    property->SetSpecularPower(100);            // 高镜面强度
    property->SetAmbient(0.15);                 // 环境光
    property->SetDiffuse(0.7);                  // 漫反射
    property->SetOpacity(1.0);                  // 完全不透明

    // 确保Actor可见
    m_actor->SetVisibility(1);
    m_actor->VisibilityOn();

    qDebug() << "[InstrumentPreviewDialog] 金属材质已应用";
}

void InstrumentPreviewDialog::setupThreePointLighting()
{
    if (!m_renderer) return;

    // 移除所有现有光源
    m_renderer->RemoveAllLights();

    // 主光源（从右上方）
    vtkSmartPointer<vtkLight> mainLight = vtkSmartPointer<vtkLight>::New();
    mainLight->SetPosition(1.0, 1.0, 1.0);
    mainLight->SetFocalPoint(0.0, 0.0, 0.0);
    mainLight->SetColor(1.0, 1.0, 1.0);
    mainLight->SetIntensity(1.2);
    m_renderer->AddLight(mainLight);

    // 补光（从左侧）
    vtkSmartPointer<vtkLight> fillLight = vtkSmartPointer<vtkLight>::New();
    fillLight->SetPosition(-1.0, 0.5, 0.5);
    fillLight->SetFocalPoint(0.0, 0.0, 0.0);
    fillLight->SetColor(0.9, 0.95, 1.0);
    fillLight->SetIntensity(0.5);
    m_renderer->AddLight(fillLight);

    // 背光（从后方）
    vtkSmartPointer<vtkLight> backLight = vtkSmartPointer<vtkLight>::New();
    backLight->SetPosition(0.0, -1.0, -0.5);
    backLight->SetFocalPoint(0.0, 0.0, 0.0);
    backLight->SetColor(0.8, 0.85, 0.9);
    backLight->SetIntensity(0.3);
    m_renderer->AddLight(backLight);

    m_isThreePointLighting = true;

    qDebug() << "[InstrumentPreviewDialog] 三点照明已设置";
}

void InstrumentPreviewDialog::setupSingleLighting()
{
    if (!m_renderer) return;

    // 移除所有现有光源
    m_renderer->RemoveAllLights();

    // 单一主光源
    vtkSmartPointer<vtkLight> mainLight = vtkSmartPointer<vtkLight>::New();
    mainLight->SetPosition(1.0, 1.0, 1.0);
    mainLight->SetFocalPoint(0.0, 0.0, 0.0);
    mainLight->SetColor(1.0, 1.0, 1.0);
    mainLight->SetIntensity(1.0);
    m_renderer->AddLight(mainLight);

    m_isThreePointLighting = false;

    qDebug() << "[InstrumentPreviewDialog] 单点照明已设置";
}

#endif  // 0


void InstrumentPreviewDialog::onResetView()
{
    qDebug() << "[InstrumentPreviewDialog] 重置视角";

    if (m_3dPreviewWidget) {
        m_3dPreviewWidget->resetView();
    }
}

void InstrumentPreviewDialog::onToggleProjection()
{
    qDebug() << "[InstrumentPreviewDialog] 切换投影模式";

    if (!m_3dPreviewWidget)
        return;

    m_isOrthographic = !m_isOrthographic;

    if (m_isOrthographic) {
        m_projectionBtn->setText("正交投影");
        qDebug() << "[InstrumentPreviewDialog] 切换到正交投影";
    } else {
        m_projectionBtn->setText("透视投影");
        qDebug() << "[InstrumentPreviewDialog] 切换到透视投影";
    }

    m_3dPreviewWidget->setParallelProjection(m_isOrthographic);
}

void InstrumentPreviewDialog::onToggleLighting()
{
    qDebug() << "[InstrumentPreviewDialog] 切换照明模式";

    if (!m_3dPreviewWidget)
        return;

    if (m_isThreePointLighting) {
        m_isThreePointLighting = false;
        m_lightingBtn->setText("单点照明");
    } else {
        m_isThreePointLighting = true;
        m_lightingBtn->setText("三点照明");
    }

    m_3dPreviewWidget->setThreePointLighting(m_isThreePointLighting);
}

void InstrumentPreviewDialog::switchTo3DMode()
{
    qDebug() << "[InstrumentPreviewDialog] ========== 切换到3D模式 (Instrument3DPreviewWidget) ==========";
    qDebug() << "[InstrumentPreviewDialog] 模型路径:" << m_modelFilePath;

    m_currentMode = Interactive3DMode;

    // 隐藏静态图片
    if (m_imageLabel) {
        m_imageLabel->hide();
    }

    // 显示 3D 预览组件
    if (m_3dPreviewWidget) {
        m_3dPreviewWidget->show();
        m_3dPreviewWidget->raise();

        qDebug() << "[InstrumentPreviewDialog] Instrument3DPreviewWidget 尺寸:" << m_3dPreviewWidget->width() << "x" << m_3dPreviewWidget->height();
        qDebug() << "[InstrumentPreviewDialog] VTK 已在构造函数中初始化完成";

        m_3dPreviewWidget->update();
    } else {
        qWarning() << "[InstrumentPreviewDialog] ⚠️ m_3dPreviewWidget 为空，无法显示3D预览";
    }

    // 更新3D控制按钮文本
    if (m_projectionBtn) {
        m_projectionBtn->setText(m_isOrthographic ? "正交投影" : "透视投影");
    }
    if (m_lightingBtn) {
        m_lightingBtn->setText(m_isThreePointLighting ? "三点照明" : "单点照明");
    }

    if (m_controlBar) {
        m_controlBar->show();
    }

    // 显示操作提示
    QLabel* hintLabel = findChild<QLabel*>("hintLabel");
    if (hintLabel) {
        hintLabel->show();
    }

    // 更新切换按钮
    if (m_switchModeBtn) {
        m_switchModeBtn->setText("切换到静态预览");
    }

    qDebug() << "[InstrumentPreviewDialog] ========== 3D模式切换完成 ==========";
}

void InstrumentPreviewDialog::switchToStaticMode()
{
    qDebug() << "[InstrumentPreviewDialog] 切换到静态预览模式";

    m_currentMode = StaticImageMode;

    // 隐藏 3D 预览组件
    if (m_3dPreviewWidget) {
        m_3dPreviewWidget->hide();
    }
    if (m_controlBar) {
        m_controlBar->hide();
    }

    // 隐藏操作提示
    QLabel* hintLabel = findChild<QLabel*>("hintLabel");
    if (hintLabel) {
        hintLabel->hide();
    }

    // 显示静态图片
    if (m_imageLabel) {
        m_imageLabel->show();
    }

    // 更新切换按钮
    if (m_switchModeBtn) {
        m_switchModeBtn->setText("切换到3D查看器");
    }

    // 如果预览图还没有加载，现在生成并加载
    if (m_previewImagePath.isEmpty()) {
        // 显示加载提示
        m_imageLabel->setText("正在生成高质量预览图...");
        m_imageLabel->setStyleSheet(
            "background: rgba(15,23,42,0.5);"
            "border: 1px solid rgba(100,116,139,0.3);"
            "border-radius: 12px;"
            "color: #94a3b8;"
            "font-size: 16px;"
        );

        // 使用服务生成静态预览图
        if (!m_instrumentService) {
            qWarning() << "[InstrumentPreviewDialog] 服务未初始化";
            m_imageLabel->setText("❌ 服务未初始化");
            m_imageLabel->setStyleSheet(
                "background: rgba(15,23,42,0.5);"
                "border: 1px solid rgba(100,116,139,0.3);"
                "border-radius: 12px;"
                "color: #ef4444;"
                "font-size: 16px;"
            );
            return;
        }

        // 生成预览图输出路径
        QFileInfo modelInfo(m_modelFilePath);
        QString projectPath = QDir(m_modelFilePath).absolutePath();

        // 向上查找项目根目录
        QDir dir(projectPath);
        while (!dir.isRoot()) {
            if (dir.dirName() == "medicalpro") {
                projectPath = dir.absolutePath();
                break;
            }
            if (!dir.cdUp()) break;
        }

        QString thumbnailsPath = projectPath + "/data/instrumentThumbnails";
        QDir().mkpath(thumbnailsPath);

        QString outputPath;
        if (m_instrumentId > 0) {
            outputPath = thumbnailsPath + QString("/instrument_%1_preview.png").arg(m_instrumentId);
        } else {
            outputPath = thumbnailsPath + QString("/%1_preview.png").arg(modelInfo.baseName());
        }

        qDebug() << "[InstrumentPreviewDialog] 异步生成预览图:" << outputPath;

        // 创建进度对话框
        QProgressDialog* progress = new QProgressDialog("正在生成高质量预览图，请稍候...", "取消", 0, 0, this);
        progress->setWindowModality(Qt::WindowModal);
        progress->setWindowTitle("生成预览图");
        progress->setMinimumDuration(0);
        progress->setValue(0);
        progress->show();

        // 使用 QtConcurrent 异步生成
        QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(this);

        // 连接完成信号
        connect(watcher, &QFutureWatcher<bool>::finished, this, [=]() {
            progress->close();
            progress->deleteLater();

            bool success = watcher->result();

            if (success && QFile::exists(outputPath)) {
                m_previewImagePath = outputPath;
                loadStaticPreview(m_previewImagePath);
            } else {
                m_imageLabel->setText("❌ 预览图生成失败");
                m_imageLabel->setStyleSheet(
                    "background: rgba(15,23,42,0.5);"
                    "border: 1px solid rgba(100,116,139,0.3);"
                    "border-radius: 12px;"
                    "color: #ef4444;"
                    "font-size: 16px;"
                );
            }

            watcher->deleteLater();
        });

        // 连接取消信号
        connect(progress, &QProgressDialog::canceled, watcher, &QFutureWatcher<bool>::cancel);

        // 启动异步任务
        QFuture<bool> future = QtConcurrent::run([=]() {
            return m_instrumentService->generatePreviewAsync(m_modelFilePath, outputPath, 800);
        });

        watcher->setFuture(future);
    } else {
        loadStaticPreview(m_previewImagePath);
    }
}

void InstrumentPreviewDialog::loadStaticPreview(const QString& previewImagePath)
{
    qDebug() << "[InstrumentPreviewDialog] 加载静态预览图:" << previewImagePath;

    if (!QFile::exists(previewImagePath)) {
        qWarning() << "[InstrumentPreviewDialog] 预览图文件不存在:" << previewImagePath;
        m_imageLabel->setText("❌ 预览图文件不存在");
        m_imageLabel->setStyleSheet(
            "background: rgba(15,23,42,0.5);"
            "border: 1px solid rgba(100,116,139,0.3);"
            "border-radius: 12px;"
            "color: #ef4444;"
            "font-size: 16px;"
        );
        return;
    }

    QPixmap pixmap(previewImagePath);
    if (pixmap.isNull()) {
        qWarning() << "[InstrumentPreviewDialog] 预览图加载失败";
        m_imageLabel->setText("❌ 预览图加载失败");
        m_imageLabel->setStyleSheet(
            "background: rgba(15,23,42,0.5);"
            "border: 1px solid rgba(100,116,139,0.3);"
            "border-radius: 12px;"
            "color: #ef4444;"
            "font-size: 16px;"
        );
        return;
    }

    // 清除样式中的文字颜色，只保留背景和边框
    m_imageLabel->setStyleSheet(
        "border: 1px solid rgba(100,116,139,0.3);"
        "border-radius: 12px;"
    );

    // 显示预览图（保持宽高比）
    QPixmap scaledPixmap = pixmap.scaled(840, 840, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(scaledPixmap);

    qDebug() << "[InstrumentPreviewDialog] 预览图加载成功";
}

void InstrumentPreviewDialog::onSwitchViewMode()
{
    qDebug() << "[InstrumentPreviewDialog] 切换查看模式";

    if (m_currentMode == Interactive3DMode) {
        switchToStaticMode();
    } else {
        switchTo3DMode();
    }
}
#endif
