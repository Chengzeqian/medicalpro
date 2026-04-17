#include "Instrument3DPreviewWidget.h"
#include "Framework/VTKWidgetFactory.h"

#include <QVBoxLayout>
#include <QDebug>
#include <QShowEvent>
#include <QHideEvent>
#include <QLabel>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QApplication>
#include <QProgressBar>
#include <QTimer>
#include <QOpenGLContext>
#include <QSurfaceFormat>

// VTK includes
#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkCleanPolyData.h>
#include <vtkTriangleFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCamera.h>
#include <vtkLight.h>
#include <vtkLightCollection.h>
#include <vtkActorCollection.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkDecimatePro.h>

// 静态缓存
QMap<QString, vtkPolyData*> Instrument3DPreviewWidget::s_modelCache;


namespace {
static const bool kEnableVerboseVTKDiagnostics = false;  // 需要详细排查时可改为 true
}

Instrument3DPreviewWidget::Instrument3DPreviewWidget(QWidget* parent)
    : QWidget(parent)
    , m_modelLoaded(false)
    , m_stackedWidget(nullptr)
    , m_placeholderWidget(nullptr)
    , m_loadingWidget(nullptr)
    , m_vtkContainer(nullptr)
    , m_loadButton(nullptr)
    , m_placeholderLabel(nullptr)
    , m_loadingLabel(nullptr)
    , m_progressBar(nullptr)
    , m_vtkWidget(nullptr)
    , m_renderer(nullptr)
    , m_renderWindow(nullptr)
    , m_interactor(nullptr)
    , m_actor(nullptr)
    , m_stlReader(nullptr)
    , m_mapper(nullptr)
    , m_isOrthographic(false)
    , m_isThreePointLighting(true)
    , m_vtkInitialized(false)
    , m_isLoading(false)
    , m_loadWatcher(nullptr)
{
    qDebug() << "[Instrument3DPreviewWidget] ========== Constructor begin ==========";

    // 设置深色背景
    setAutoFillBackground(true);
    QPalette widgetPalette = palette();
    widgetPalette.setColor(QPalette::Window, QColor(25, 25, 38));
    widgetPalette.setColor(QPalette::Base, QColor(25, 25, 38));
    setPalette(widgetPalette);
    setStyleSheet("background-color: #191926;");

    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ========== 1. VTK容器（底层，始终存在）==========
    m_vtkContainer = new QWidget(this);
    m_vtkContainer->setAutoFillBackground(true);

    QPalette palette = m_vtkContainer->palette();
    palette.setColor(QPalette::Window, QColor(25, 25, 38));
    m_vtkContainer->setPalette(palette);

    QVBoxLayout* vtkLayout = new QVBoxLayout(m_vtkContainer);
    vtkLayout->setContentsMargins(0, 0, 0, 0);
    vtkLayout->setSpacing(0);

    // 立即初始化 VTK
    initializeVTK();
    vtkLayout->addWidget(m_vtkWidget);

    mainLayout->addWidget(m_vtkContainer);

    // ========== 2. 占位界面（叠加层）==========
    setupPlaceholderUI();
    // 重新设置父对象和定位
    m_placeholderWidget->setParent(this);
    m_placeholderWidget->setStyleSheet("background-color: rgba(240, 240, 245, 0.98);");
    m_placeholderWidget->raise();
    m_placeholderWidget->show();

    // ========== 3. 加载界面（叠加层）==========
    m_loadingWidget = new QWidget(this);
    m_loadingWidget->setStyleSheet("background-color: rgba(240, 240, 245, 0.98);");
    QVBoxLayout* loadingLayout = new QVBoxLayout(m_loadingWidget);
    loadingLayout->setAlignment(Qt::AlignCenter);

    m_loadingLabel = new QLabel("正在加载3D模型...", m_loadingWidget);
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setStyleSheet("font-size: 16px; color: #333; margin-bottom: 20px; background: transparent;");

    m_progressBar = new QProgressBar(m_loadingWidget);
    m_progressBar->setFixedWidth(400);
    m_progressBar->setFixedHeight(30);
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(true);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "  border: 2px solid #cbd5e1;"
        "  border-radius: 8px;"
        "  text-align: center;"
        "  background-color: #f1f5f9;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #8b5cf6);"
        "  border-radius: 6px;"
        "}"
    );

    loadingLayout->addWidget(m_loadingLabel);
    loadingLayout->addWidget(m_progressBar);

    m_loadingWidget->raise();
    m_loadingWidget->hide();

    qDebug() << "[Instrument3DPreviewWidget] ========== Constructor complete ==========";
}

Instrument3DPreviewWidget::~Instrument3DPreviewWidget()
{
    qDebug() << "[Instrument3DPreviewWidget] Destructor begin";

    // 取消正在进行的异步加载
    if (m_loadWatcher) {
        m_loadWatcher->cancel();
        m_loadWatcher->waitForFinished();
        delete m_loadWatcher;
        m_loadWatcher = nullptr;
    }

    cleanupVTK();

    // 注意：不在这里清理缓存，缓存是全局的，由应用程序管理

    qDebug() << "[Instrument3DPreviewWidget] Destructor complete";
}

void Instrument3DPreviewWidget::setupPlaceholderUI()
{
    // 🔥 新方法：创建独立的占位Widget（不添加到QStackedWidget）
    m_placeholderWidget = new QWidget(this);
    m_placeholderWidget->setStyleSheet("background-color: rgba(240, 240, 245, 0.98);");

    QVBoxLayout* placeholderLayout = new QVBoxLayout(m_placeholderWidget);
    placeholderLayout->setAlignment(Qt::AlignCenter);

    // 图标标签
    QLabel* iconLabel = new QLabel("🎮", m_placeholderWidget);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 64px; background: transparent;");

    // 提示文本
    m_placeholderLabel = new QLabel("点击下方按钮加载3D模型", m_placeholderWidget);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setStyleSheet(
        "font-size: 18px;"
        "color: #333;"
        "margin-top: 20px;"
        "margin-bottom: 30px;"
        "background: transparent;"
    );

    // 加载按钮
    m_loadButton = new QPushButton("🚀 加载3D模型", m_placeholderWidget);
    m_loadButton->setFixedSize(200, 50);
    m_loadButton->setCursor(Qt::PointingHandCursor);
    m_loadButton->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #8b5cf6);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 12px;"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  padding: 10px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2563eb, stop:1 #7c3aed);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1d4ed8, stop:1 #6d28d9);"
        "}"
    );

    connect(m_loadButton, &QPushButton::clicked, this, &Instrument3DPreviewWidget::onLoadButtonClicked);

    placeholderLayout->addWidget(iconLabel);
    placeholderLayout->addWidget(m_placeholderLabel);
    placeholderLayout->addWidget(m_loadButton, 0, Qt::AlignCenter);
}

void Instrument3DPreviewWidget::initializeVTK()
{
    qDebug() << "[Instrument3DPreviewWidget] ========== VTK initialization begin ==========";

    // 使用 VTKWidgetFactory 创建标准配置的 VTK widget（内部采用 VTK 默认表面格式）
    qDebug() << "[Instrument3DPreviewWidget] Creating QVTKOpenGLNativeWidget via VTKWidgetFactory";
    m_vtkWidget = VTKWidgetFactory::createStandardVTKWidget(this);

    if (!m_vtkWidget) {
        qCritical() << "[Instrument3DPreviewWidget] [fail] VTKWidgetFactory creation failed";
        m_vtkInitialized = false;
        return;
    }

    m_vtkWidget->setMinimumSize(400, 400);

    qDebug() << "[Instrument3DPreviewWidget] [ok] QVTKOpenGLNativeWidget created via factory";

    // 创建渲染窗口和渲染器，使用 VTK 默认配置
    m_renderWindow = vtkGenericOpenGLRenderWindow::New();
    qDebug() << "[Instrument3DPreviewWidget] [ok] vtkGenericOpenGLRenderWindow created";

    m_renderer = vtkRenderer::New();
    m_renderer->SetBackground(0.1, 0.1, 0.15);  // 深蓝灰色背景
    m_renderWindow->AddRenderer(m_renderer);

    qDebug() << "[Instrument3DPreviewWidget] [ok] vtkRenderer created with dark blue-gray background";

    // 获取交互器
    m_interactor = m_renderWindow->GetInteractor();
    if (m_interactor) {
        vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
            vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        m_interactor->SetInteractorStyle(style);
        qDebug() << "[Instrument3DPreviewWidget] [ok] Interactor configured";
    }

    // 让 RenderWindow 在 showEvent 中绑定到 widget（保持现有逻辑）
    qDebug() << "[Instrument3DPreviewWidget] [warn] RenderWindow will be attached in showEvent";
    m_vtkInitialized = true;
    qDebug() << "[Instrument3DPreviewWidget] ========== VTK initialization complete ==========";
}

void Instrument3DPreviewWidget::setModelFilePath(const QString& filePath)
{
    qDebug() << "[Instrument3DPreviewWidget] setModelFilePath:" << filePath;

    m_modelFilePath = filePath;
    m_modelLoaded = false;

    // 不立即加载，只是设置路径并显示占位界面
    if (!m_modelFilePath.isEmpty()) {
        // 检查是否有缓存
        if (getCachedModel(m_modelFilePath)) {
            m_placeholderLabel->setText("模型已缓存，点击按钮即可快速加载");
            m_loadButton->setText("⚡ 快速加载（已缓存）");
        } else {
            m_placeholderLabel->setText("点击下方按钮加载3D模型");
            m_loadButton->setText("🚀 加载3D模型");
        }

        // 🔥 显示占位界面
        if (m_placeholderWidget) {
            m_placeholderWidget->show();
            m_placeholderWidget->raise();
        }
        if (m_loadingWidget) {
            m_loadingWidget->hide();
        }
    } else {
        qWarning() << "[Instrument3DPreviewWidget] Model path is empty";
    }
}

void Instrument3DPreviewWidget::loadModel()
{
    if (m_modelFilePath.isEmpty()) {
        qWarning() << "[Instrument3DPreviewWidget] Cannot load model: path is empty";
        return;
    }

    if (m_isLoading) {
        qWarning() << "[Instrument3DPreviewWidget] A model is already loading";
        return;
    }

    if (!m_vtkInitialized) {
        qCritical() << "[Instrument3DPreviewWidget] VTK is not initialized";
        return;
    }

    qDebug() << "[Instrument3DPreviewWidget] Starting model load:" << m_modelFilePath;

    // 检查缓存
    vtkPolyData* cachedData = getCachedModel(m_modelFilePath);
    if (cachedData) {
        qDebug() << "[Instrument3DPreviewWidget] [ok] Using cached model";

        // ⚠️ 关键修复：先显示VTK Widget，确保OpenGL上下文就绪
        showVTKWidget();
        QApplication::processEvents();  // 等待UI更新

        // 然后渲染模型
        renderLoadedModel(cachedData);
        m_modelLoaded = true;
        return;
    }

    // 没有缓存，异步加载
    loadAndRenderModelAsync();
}

void Instrument3DPreviewWidget::onLoadButtonClicked()
{
    qDebug() << "[Instrument3DPreviewWidget] Load button clicked";
    loadModel();
}

void Instrument3DPreviewWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    qDebug() << "[Instrument3DPreviewWidget] showEvent triggered";

    // 🔥🔥🔥 关键修复：在showEvent中设置RenderWindow到Widget
    // 这样可以确保OpenGL上下文已经准备好
    if (m_renderWindow && m_vtkWidget) {
        qDebug() << "[Instrument3DPreviewWidget] [render] Attaching RenderWindow to widget in showEvent";
        m_vtkWidget->setRenderWindow(m_renderWindow);
        qDebug() << "[Instrument3DPreviewWidget] [ok] RenderWindow attached to widget";
    }

    // 确保叠加层大小正确
    updateOverlayGeometry();

    // 恢复VTK渲染
    resumeVTKRendering();
}

void Instrument3DPreviewWidget::hideEvent(QHideEvent* event)
{
    // 【关键】隐藏时暂停VTK渲染，防止页面切换时闪烁
    pauseVTKRendering();
    QWidget::hideEvent(event);
}

void Instrument3DPreviewWidget::pauseVTKRendering()
{
    if (!m_vtkInitialized || !m_renderWindow) return;

    qDebug() << "[Instrument3DPreviewWidget] Pausing VTK rendering";

    // 【关键】彻底停止渲染
    m_renderWindow->SetAbortRender(1);  // 中止渲染
    m_renderWindow->SetSwapBuffers(false);
    if (m_interactor) {
        m_interactor->Disable();
    }
}

void Instrument3DPreviewWidget::resumeVTKRendering()
{
    if (!m_vtkInitialized || !m_renderWindow) return;

    qDebug() << "[Instrument3DPreviewWidget] Resuming VTK rendering";

    // 【关键】恢复渲染
    m_renderWindow->SetAbortRender(0);  // 允许渲染
    m_renderWindow->SetSwapBuffers(true);
    if (m_interactor) {
        m_interactor->Enable();
    }

    // 确保VTK Widget可见时渲染一次
    if (m_vtkWidget && m_vtkWidget->isVisible()) {
        m_renderWindow->Render();
    }
}

void Instrument3DPreviewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateOverlayGeometry();
}

void Instrument3DPreviewWidget::updateOverlayGeometry()
{
    // 更新叠加层的几何位置和大小
    if (m_placeholderWidget) {
        m_placeholderWidget->setGeometry(0, 0, width(), height());
    }
    if (m_loadingWidget) {
        m_loadingWidget->setGeometry(0, 0, width(), height());
    }
}

void Instrument3DPreviewWidget::resetView()
{
    if (m_renderer && m_renderWindow) {
        m_renderer->ResetCamera();
        m_renderWindow->Render();
        qDebug() << "[Instrument3DPreviewWidget] View reset";
    }
}

void Instrument3DPreviewWidget::setParallelProjection(bool enabled)
{
    if (!m_renderer) return;
    vtkCamera* camera = m_renderer->GetActiveCamera();
    if (!camera) return;

    m_isOrthographic = enabled;
    if (enabled) {
        camera->ParallelProjectionOn();
        qDebug() << "[Instrument3DPreviewWidget] Switched to orthographic projection";
    } else {
        camera->ParallelProjectionOff();
        qDebug() << "[Instrument3DPreviewWidget] Switched to perspective projection";
    }

    if (m_renderWindow) {
        m_renderWindow->Render();
    }
}

void Instrument3DPreviewWidget::setThreePointLighting(bool enabled)
{
    if (!m_renderer) return;

    if (enabled) {
        setupThreePointLighting();
        qDebug() << "[Instrument3DPreviewWidget] Switched to three-point lighting";
    } else {
        setupSingleLighting();
        qDebug() << "[Instrument3DPreviewWidget] Switched to single-point lighting";
    }

    if (m_renderWindow) {
        m_renderWindow->Render();
    }
}

// ========== 异步加载实现 ==========

void Instrument3DPreviewWidget::loadAndRenderModelAsync()
{
    qDebug() << "[Instrument3DPreviewWidget] Starting asynchronous model load:" << m_modelFilePath;

    if (m_isLoading) {
        qWarning() << "[Instrument3DPreviewWidget] A model is already loading, ignoring request";
        return;
    }

    if (!m_renderer) {
        qCritical() << "[Instrument3DPreviewWidget] Renderer is null, cannot load model";
        return;
    }

    // 显示加载界面
    showLoadingUI();
    m_isLoading = true;

    // 清理旧的模型（如果有）
    if (m_actor && m_renderer) {
        m_renderer->RemoveActor(m_actor);
        m_actor->Delete();
        m_actor = nullptr;
    }
    if (m_mapper) {
        m_mapper->Delete();
        m_mapper = nullptr;
    }
    if (m_stlReader) {
        m_stlReader->Delete();
        m_stlReader = nullptr;
    }

    // 在后台线程加载 STL 文件
    QFuture<LoadResult> future = QtConcurrent::run([this]() {
        return loadSTLInBackground(m_modelFilePath);
    });

    // 监听加载完成
    if (!m_loadWatcher) {
        m_loadWatcher = new QFutureWatcher<LoadResult>(this);
        connect(m_loadWatcher, &QFutureWatcher<LoadResult>::finished,
                this, &Instrument3DPreviewWidget::onModelLoadFinished);
    }
    m_loadWatcher->setFuture(future);
}

Instrument3DPreviewWidget::LoadResult Instrument3DPreviewWidget::loadSTLInBackground(const QString& filePath)
{
    LoadResult result;
    result.polyData = nullptr;
    result.originalCells = 0;
    result.finalCells = 0;
    result.simplified = false;

    qDebug() << "[WorkerThread] ========== Starting STL load ==========";
    qDebug() << "[WorkerThread] File path:" << filePath;

    // 读取 STL 文件
    vtkSTLReader* reader = vtkSTLReader::New();
    reader->SetFileName(filePath.toStdString().c_str());
    reader->Update();

    vtkPolyData* rawData = reader->GetOutput();
    if (!rawData || rawData->GetNumberOfPoints() == 0) {
        qCritical() << "[WorkerThread] [fail] Failed to read STL file or file is empty";
        reader->Delete();
        return result;
    }

    result.originalCells = rawData->GetNumberOfCells();
    qDebug() << "[WorkerThread] [ok] STL raw data - points:" << rawData->GetNumberOfPoints()
             << "cells:" << result.originalCells;

    // 数据清理
    vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputData(rawData);
    cleaner->Update();

    // 三角化
    vtkSmartPointer<vtkTriangleFilter> triangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
    triangleFilter->SetInputConnection(cleaner->GetOutputPort());
    triangleFilter->Update();

    // 模型简化（如果面片数量过多）
    vtkPolyData* processedData = triangleFilter->GetOutput();
    int numCells = processedData->GetNumberOfCells();

    vtkSmartPointer<vtkPolyData> simplifiedData;
    if (numCells > 500000) {  // 如果超过50万面片，进行简化
        qDebug() << "[WorkerThread] [warn] Model has too many polygons, simplifying...";

        vtkSmartPointer<vtkDecimatePro> decimate = vtkSmartPointer<vtkDecimatePro>::New();
        decimate->SetInputData(processedData);

        // 根据面片数量动态调整简化比例
        double targetReduction = 1.0 - (300000.0 / numCells);  // 目标保留30万面片
        targetReduction = qMax(0.5, qMin(0.9, targetReduction));  // 限制在50%-90%之间

        decimate->SetTargetReduction(targetReduction);
        decimate->PreserveTopologyOn();
        decimate->Update();

        simplifiedData = decimate->GetOutput();
        result.finalCells = simplifiedData->GetNumberOfCells();
        result.simplified = true;

        qDebug() << "[WorkerThread] [ok] Simplification complete - polygons after reduction:" << result.finalCells
                 << "reduction:" << (targetReduction * 100) << "%";
    } else {
        simplifiedData = processedData;
        result.finalCells = numCells;
        result.simplified = false;
        qDebug() << "[WorkerThread] [ok] Polygon count acceptable, skipping simplification";
    }

    // 计算法线
    qDebug() << "[WorkerThread] Computing normals...";
    vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
    normals->SetInputData(simplifiedData);
    normals->ComputePointNormalsOn();
    normals->ComputeCellNormalsOn();
    normals->SplittingOff();
    normals->ConsistencyOn();
    normals->AutoOrientNormalsOn();
    normals->Update();

    qDebug() << "[WorkerThread] [ok] Normal computation complete";

    // 创建一个新的 vtkPolyData 对象并复制数据
    result.polyData = vtkPolyData::New();
    result.polyData->DeepCopy(normals->GetOutput());

    reader->Delete();

    qDebug() << "[WorkerThread] ========== STL load complete ==========";
    return result;
}

void Instrument3DPreviewWidget::onModelLoadFinished()
{
    qDebug() << "[Instrument3DPreviewWidget] ========== Model load complete ==========";

    LoadResult result = m_loadWatcher->result();

    if (!result.polyData) {
        qCritical() << "[Instrument3DPreviewWidget] [fail] Model load failed";
        hideLoadingUI();
        m_isLoading = false;

        // 🔥 显示错误提示
        m_placeholderLabel->setText("❌ 模型加载失败，请重试");
        if (m_placeholderWidget) {
            m_placeholderWidget->show();
            m_placeholderWidget->raise();
        }
        return;
    }

    qDebug() << "[Instrument3DPreviewWidget] Load statistics:";
    qDebug() << "  - Original polygon count:" << result.originalCells;
    qDebug() << "  - Final polygon count:" << result.finalCells;
    qDebug() << "  - Simplified:" << (result.simplified ? "yes" : "no");

    // ⚠️ 关键修复：先显示VTK Widget，确保OpenGL上下文就绪
    qDebug() << "[Instrument3DPreviewWidget] Step 1: Show VTK widget";
    showVTKWidget();

    // 然后渲染模型
    qDebug() << "[Instrument3DPreviewWidget] Step 2: Render model";
    renderLoadedModel(result.polyData);

    // 缓存模型（用于下次快速加载）
    qDebug() << "[Instrument3DPreviewWidget] Step 3: Cache model";
    cacheModel(m_modelFilePath, result.polyData);

    hideLoadingUI();
    m_isLoading = false;
    m_modelLoaded = true;

    qDebug() << "[Instrument3DPreviewWidget] ========== Model rendering complete ==========";
}

void Instrument3DPreviewWidget::onLoadProgress(int progress)
{
    if (m_progressBar) {
        m_progressBar->setValue(progress);
    }
}

void Instrument3DPreviewWidget::renderLoadedModel(vtkPolyData* polyData)
{
    if (!polyData || !m_renderer) {
        qCritical() << "[Instrument3DPreviewWidget] Cannot render: data or renderer is null";
        return;
    }

    qDebug() << "[Instrument3DPreviewWidget] ========== Begin model rendering ==========";
    qDebug() << "[Instrument3DPreviewWidget] Model data - points:" << polyData->GetNumberOfPoints()
             << "cells:" << polyData->GetNumberOfCells();

    // ⚠️ 清理旧的模型（如果有）
    if (m_actor) {
        qDebug() << "[Instrument3DPreviewWidget] Removing previous actor";
        m_renderer->RemoveActor(m_actor);
        m_actor->Delete();
        m_actor = nullptr;
    }
    if (m_mapper) {
        qDebug() << "[Instrument3DPreviewWidget] Removing previous mapper";
        m_mapper->Delete();
        m_mapper = nullptr;
    }

    // 确保 VTK Widget 可见
    if (m_vtkWidget && !m_vtkWidget->isVisible()) {
        qWarning() << "[Instrument3DPreviewWidget] [warn] VTK widget is hidden, forcing visibility";
        m_vtkWidget->show();
    } else {
        qDebug() << "[Instrument3DPreviewWidget] [ok] VTK widget is visible";
    }

    // 创建 mapper
    qDebug() << "[Instrument3DPreviewWidget] Creating mapper";
    m_mapper = vtkPolyDataMapper::New();
    m_mapper->SetInputData(polyData);

    // 创建 actor
    qDebug() << "[Instrument3DPreviewWidget] Creating actor";
    m_actor = vtkActor::New();
    m_actor->SetMapper(m_mapper);
    applyMetallicMaterial(m_actor);

    // 添加到渲染器
    qDebug() << "[Instrument3DPreviewWidget] Adding actor to renderer";
    m_renderer->AddActor(m_actor);

    // 设置照明
    qDebug() << "[Instrument3DPreviewWidget] Configuring three-point lighting";
    setupThreePointLighting();

    // 重置相机
    qDebug() << "[Instrument3DPreviewWidget] Resetting camera";
    resetCameraInternal();

    if (kEnableVerboseVTKDiagnostics) {
        // 🔍 详细诊断：在渲染前检查所有状态
        qDebug() << "[Diagnostics] ========== Final pre-render diagnostics ==========";
        qDebug() << "[Diagnostics] Widget hierarchy:";
        qDebug() << "[Diagnostics]   - m_vtkWidget visible:" << (m_vtkWidget ? m_vtkWidget->isVisible() : false);
        qDebug() << "[Diagnostics]   - m_vtkWidget size:" << (m_vtkWidget ? m_vtkWidget->size() : QSize(0,0));
        qDebug() << "[Diagnostics]   - m_vtkContainer visible:" << (m_vtkContainer ? m_vtkContainer->isVisible() : false);
        qDebug() << "[Diagnostics]   - m_vtkContainer background color:" << (m_vtkContainer ? m_vtkContainer->palette().color(QPalette::Window) : QColor());
        qDebug() << "[Diagnostics]   - m_vtkContainer autoFillBackground:" << (m_vtkContainer ? m_vtkContainer->autoFillBackground() : false);

        qDebug() << "[Diagnostics] VTK state:";
        if (m_renderer) {
            double bg[3];
            m_renderer->GetBackground(bg);
            qDebug() << "[Diagnostics]   - Renderer background:" << bg[0] << bg[1] << bg[2];
            qDebug() << "[Diagnostics]   - Actor count:" << m_renderer->GetActors()->GetNumberOfItems();
            qDebug() << "[Diagnostics]   - Light count:" << m_renderer->GetLights()->GetNumberOfItems();

            // 检查Actor属性
            if (m_actor) {
                vtkProperty* prop = m_actor->GetProperty();
                double color[3];
                prop->GetColor(color);
                qDebug() << "[Diagnostics]   - Actor color:" << color[0] << color[1] << color[2];
                qDebug() << "[Diagnostics]   - Actor opacity:" << prop->GetOpacity();
                qDebug() << "[Diagnostics]   - Actor visibility:" << m_actor->GetVisibility();
            }
        }

        if (m_renderWindow) {
            qDebug() << "[Diagnostics]   - RenderWindow size:" << m_renderWindow->GetSize()[0] << "x" << m_renderWindow->GetSize()[1];
            qDebug() << "[Diagnostics]   - RenderWindow MultiSamples:" << m_renderWindow->GetMultiSamples();
        }

        qDebug() << "[Diagnostics] ========== Diagnostics complete, starting render ==========";
    }

    // 🔥 强制激活OpenGL上下文后再渲染
    if (m_vtkWidget && m_renderWindow) {
        QOpenGLContext* context = m_vtkWidget->context();
        if (context && context->isValid()) {
            qDebug() << "[Instrument3DPreviewWidget] [render] Activating OpenGL context for rendering";
            m_vtkWidget->makeCurrent();

            qDebug() << "[Instrument3DPreviewWidget] Executing render...";
            m_renderWindow->Render();
            qDebug() << "[Instrument3DPreviewWidget] [ok] Render complete";

            m_vtkWidget->doneCurrent();
        } else {
            qCritical() << "[Instrument3DPreviewWidget] [fail] OpenGL context invalid, cannot render";
        }
    } else {
        qCritical() << "[Instrument3DPreviewWidget] [fail] VTK widget or RenderWindow is null";
    }

    if (m_vtkWidget) {
        m_vtkWidget->update();
        qDebug() << "[Instrument3DPreviewWidget] [ok] Widget updated";
    }

    // 🔍 渲染后诊断：检查Widget的Z-order（仅在需要时启用）
    if (kEnableVerboseVTKDiagnostics) {
        qDebug() << "[Diagnostics] ========== Post-render widget hierarchy diagnostics ==========";
        if (m_vtkWidget) {
            qDebug() << "[Diagnostics]   - m_vtkWidget parent:" << (m_vtkWidget->parentWidget() ? m_vtkWidget->parentWidget()->metaObject()->className() : "nullptr");
            qDebug() << "[Diagnostics]   - m_vtkWidget geometry:" << m_vtkWidget->geometry();
            qDebug() << "[Diagnostics]   - m_vtkWidget hidden:" << m_vtkWidget->isHidden();
        }
        if (m_vtkContainer) {
            qDebug() << "[Diagnostics]   - m_vtkContainer geometry:" << m_vtkContainer->geometry();
            qDebug() << "[Diagnostics]   - m_vtkContainer hidden:" << m_vtkContainer->isHidden();
        }
        if (m_placeholderWidget) {
            qDebug() << "[Diagnostics]   - m_placeholderWidget visible:" << m_placeholderWidget->isVisible();
            qDebug() << "[Diagnostics]   - m_placeholderWidget geometry:" << m_placeholderWidget->geometry();
        }
        if (m_loadingWidget) {
            qDebug() << "[Diagnostics]   - m_loadingWidget visible:" << m_loadingWidget->isVisible();
            qDebug() << "[Diagnostics]   - m_loadingWidget geometry:" << m_loadingWidget->geometry();
        }
        qDebug() << "[Diagnostics] ========== Hierarchy diagnostics complete ==========";
    }

    qDebug() << "[Instrument3DPreviewWidget] ========== Model rendering complete ==========";
}

void Instrument3DPreviewWidget::showLoadingUI()
{
    // 🔥 新方法：隐藏占位界面，显示加载界面
    if (m_placeholderWidget) {
        m_placeholderWidget->hide();
    }
    if (m_loadingWidget) {
        m_progressBar->setValue(0);
        m_loadingLabel->setText("正在加载3D模型，请稍候...");
        m_loadingWidget->show();
        m_loadingWidget->raise();
        qDebug() << "[Instrument3DPreviewWidget] [ok] Showing loading overlay";
    }
}

void Instrument3DPreviewWidget::hideLoadingUI()
{
    // 🔥 新方法：隐藏加载界面
    if (m_loadingWidget) {
        m_loadingWidget->hide();
    }
    qDebug() << "[Instrument3DPreviewWidget] [ok] Hiding loading overlay";
}

void Instrument3DPreviewWidget::showVTKWidget()
{
    // 🔥 新方法：隐藏所有叠加层，显示VTK Widget
    qDebug() << "[Instrument3DPreviewWidget] Showing VTK widget";

    if (m_placeholderWidget) {
        m_placeholderWidget->hide();
    }
    if (m_loadingWidget) {
        m_loadingWidget->hide();
    }

    // 显示VTK容器
    if (m_vtkContainer) {
        m_vtkContainer->setAutoFillBackground(true);
        QPalette palette = m_vtkContainer->palette();
        palette.setColor(QPalette::Window, QColor(25, 25, 38));
        m_vtkContainer->setPalette(palette);
        m_vtkContainer->show();
    }

    // 确保VTK Widget可见
    if (m_vtkWidget) {
        m_vtkWidget->setAutoFillBackground(true);
        m_vtkWidget->show();

        // 🔍 诊断：检查OpenGL上下文（仅在需要时启用详细日志）
        QOpenGLContext* context = m_vtkWidget->context();
        if (kEnableVerboseVTKDiagnostics) {
            qDebug() << "[Diagnostics] ========== OpenGL context diagnostics ==========";
            if (context) {
                qDebug() << "[Diagnostics] OpenGL context present: YES";
                qDebug() << "[Diagnostics] OpenGL context valid:" << context->isValid();

                QSurfaceFormat format = context->format();
                qDebug() << "[Diagnostics] OpenGL version:" << format.majorVersion() << "." << format.minorVersion();
                qDebug() << "[Diagnostics] Alpha buffer size:" << format.alphaBufferSize();
                qDebug() << "[Diagnostics] Depth buffer size:" << format.depthBufferSize();
                qDebug() << "[Diagnostics] Stencil buffer size:" << format.stencilBufferSize();
                qDebug() << "[Diagnostics] Sample count:" << format.samples();
                qDebug() << "[Diagnostics] Swap behavior:" << format.swapBehavior();
            } else {
                qCritical() << "[Diagnostics] OpenGL context present: NO - this is the issue";
            }
            qDebug() << "[Diagnostics] ========== OpenGL context diagnostics complete ==========";
        }

        // 🔧 关键修复：强制激活OpenGL上下文
        if (context && context->isValid()) {
            qDebug() << "[Instrument3DPreviewWidget] [force] Forcing OpenGL context activation";
            m_vtkWidget->makeCurrent();

            // 强制渲染背景（确保OpenGL上下文激活）
            if (m_renderWindow) {
                qDebug() << "[Instrument3DPreviewWidget] Rendering VTK background with active OpenGL context";
                m_renderWindow->Render();
            }

            m_vtkWidget->doneCurrent();
        } else {
            qWarning() << "[Instrument3DPreviewWidget] [warn] OpenGL context invalid, cannot render";
        }

        qDebug() << "[Instrument3DPreviewWidget] [ok] VTK widget shown";
        qDebug() << "[Instrument3DPreviewWidget] VTK widget visibility:" << m_vtkWidget->isVisible();
        qDebug() << "[Instrument3DPreviewWidget] VTK widget size:" << m_vtkWidget->width() << "x" << m_vtkWidget->height();
    }
}

// ========== 缓存管理 ==========

void Instrument3DPreviewWidget::cacheModel(const QString& filePath, vtkPolyData* polyData)
{
    if (!polyData || filePath.isEmpty()) {
        return;
    }

    // 检查是否已经缓存
    if (s_modelCache.contains(filePath)) {
        qDebug() << "[Cache] Model already cached:" << filePath;
        return;
    }

    // 如果缓存已满，删除最旧的
    if (s_modelCache.size() >= MAX_CACHE_SIZE) {
        QString oldestKey = s_modelCache.keys().first();
        vtkPolyData* oldData = s_modelCache.take(oldestKey);
        if (oldData) {
            oldData->Delete();
        }
        qDebug() << "[Cache] Cache full, removing oldest model:" << oldestKey;
    }

    // 深拷贝并缓存
    vtkPolyData* cachedData = vtkPolyData::New();
    cachedData->DeepCopy(polyData);
    s_modelCache.insert(filePath, cachedData);

    qDebug() << "[Cache] [ok] Model cached:" << filePath;
    qDebug() << "[Cache] Current cache size:" << s_modelCache.size() << "/" << MAX_CACHE_SIZE;
}

vtkPolyData* Instrument3DPreviewWidget::getCachedModel(const QString& filePath)
{
    if (s_modelCache.contains(filePath)) {
        qDebug() << "[Cache] [ok] Found cached model:" << filePath;
        return s_modelCache.value(filePath);
    }
    return nullptr;
}

void Instrument3DPreviewWidget::clearCache()
{
    qDebug() << "[Cache] Clearing all cached models, count=" << s_modelCache.size() << " models";

    for (vtkPolyData* data : s_modelCache.values()) {
        if (data) {
            data->Delete();
        }
    }
    s_modelCache.clear();

    qDebug() << "[Cache] [ok] Cache cleared";
}

void Instrument3DPreviewWidget::cleanupVTK()
{
    qDebug() << "[Instrument3DPreviewWidget] Cleaning VTK resources";

    if (m_actor) {
        if (m_renderer) {
            m_renderer->RemoveActor(m_actor);
        }
        m_actor->Delete();
        m_actor = nullptr;
    }

    if (m_mapper) {
        m_mapper->Delete();
        m_mapper = nullptr;
    }

    if (m_stlReader) {
        m_stlReader->Delete();
        m_stlReader = nullptr;
    }

    if (m_renderer) {
        if (m_renderWindow) {
            m_renderWindow->RemoveRenderer(m_renderer);
        }
        m_renderer->Delete();
        m_renderer = nullptr;
    }

    if (m_renderWindow) {
        m_renderWindow->Delete();
        m_renderWindow = nullptr;
    }

    qDebug() << "[Instrument3DPreviewWidget] VTK resource cleanup complete";
}


void Instrument3DPreviewWidget::applyMetallicMaterial(vtkActor* actor)
{
    if (!actor) return;
    vtkProperty* property = actor->GetProperty();

    // 稍微偏亮的金属灰色，更接近手术器械质感
    property->SetColor(0.84, 0.84, 0.88);
    property->SetMetallic(0.95);
    property->SetRoughness(0.15);
    property->SetSpecular(0.9);
    property->SetSpecularPower(80);
    property->SetAmbient(0.25);
    property->SetDiffuse(0.6);
    property->SetOpacity(1.0);
}

void Instrument3DPreviewWidget::setupThreePointLighting()
{
    if (!m_renderer) return;

    m_renderer->RemoveAllLights();

    vtkSmartPointer<vtkLight> mainLight = vtkSmartPointer<vtkLight>::New();
    mainLight->SetPosition(1.0, 1.0, 1.0);
    mainLight->SetFocalPoint(0.0, 0.0, 0.0);
    mainLight->SetColor(1.0, 1.0, 1.0);
    mainLight->SetIntensity(1.2);
    m_renderer->AddLight(mainLight);

    vtkSmartPointer<vtkLight> fillLight = vtkSmartPointer<vtkLight>::New();
    fillLight->SetPosition(-1.0, 0.5, 0.5);
    fillLight->SetFocalPoint(0.0, 0.0, 0.0);
    fillLight->SetColor(0.9, 0.95, 1.0);
    fillLight->SetIntensity(0.5);
    m_renderer->AddLight(fillLight);

    vtkSmartPointer<vtkLight> backLight = vtkSmartPointer<vtkLight>::New();
    backLight->SetPosition(0.0, -1.0, -0.5);
    backLight->SetFocalPoint(0.0, 0.0, 0.0);
    backLight->SetColor(0.8, 0.85, 0.9);
    backLight->SetIntensity(0.3);
    m_renderer->AddLight(backLight);

    m_isThreePointLighting = true;
}

void Instrument3DPreviewWidget::setupSingleLighting()
{
    if (!m_renderer) return;

    m_renderer->RemoveAllLights();

    vtkSmartPointer<vtkLight> mainLight = vtkSmartPointer<vtkLight>::New();
    mainLight->SetPosition(1.0, 1.0, 1.0);
    mainLight->SetFocalPoint(0.0, 0.0, 0.0);
    mainLight->SetColor(1.0, 1.0, 1.0);
    mainLight->SetIntensity(1.0);
    m_renderer->AddLight(mainLight);

    m_isThreePointLighting = false;
}

void Instrument3DPreviewWidget::resetCameraInternal()
{
    if (!m_renderer) return;
    vtkCamera* camera = m_renderer->GetActiveCamera();
    if (!camera) return;

    m_renderer->ResetCamera();
    camera->Zoom(1.2);
}

