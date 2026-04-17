#include "FourViewVTKWidget.h"
#include "FourViewDisplayService.h"
#include "Framework/VTKWidgetFactory.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QShowEvent>
#include <QHideEvent>
#include <QDebug>
#include <QLabel>
#include <QFrame>
#include <exception>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRendererCollection.h>
#include <vtkCamera.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkInteractorStyleImage.h>
#include <vtkPointPicker.h>
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>

namespace {
constexpr double kDefaultWindowWidth = 2000.0;
constexpr double kDefaultWindowLevel = 400.0;
constexpr double kDefault3DOpacity = 1.0;
}

// VTK 回调：在3D视图中左键点击拾取世界坐标，并通过Qt信号抛出
static void OnFourView3DLeftButtonDown(vtkObject* caller,
                                       unsigned long eventId,
                                       void* clientData,
                                       void* callData)
{
    Q_UNUSED(eventId);
    Q_UNUSED(callData);

    FourViewVTKWidget* widget = static_cast<FourViewVTKWidget*>(clientData);
    if (!widget) return;

    vtkRenderWindowInteractor* interactor =
        vtkRenderWindowInteractor::SafeDownCast(caller);
    if (!interactor) return;

    int* clickPos = interactor->GetEventPosition();

    vtkSmartPointer<vtkPointPicker> picker = vtkSmartPointer<vtkPointPicker>::New();
    picker->SetTolerance(0.005);

    vtkRenderer* renderer = interactor->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
    if (renderer && picker->Pick(clickPos[0], clickPos[1], 0, renderer)) {
        double* pos = picker->GetPickPosition();
        qDebug() << "[FourViewVTKWidget] 3D视图点击位置:" << pos[0] << pos[1] << pos[2];
        emit widget->imagePointPicked(pos[0], pos[1], pos[2]);
    }
}

FourViewVTKWidget::FourViewVTKWidget(FourViewDisplayService* service, QWidget* parent)
    : QWidget(parent)
    , m_service(service)
    , m_axialViewWidget(nullptr)
    , m_sagittalViewWidget(nullptr)
    , m_coronalViewWidget(nullptr)
    , m_3dViewWidget(nullptr)
    , m_vtkInitialized(false)
    , m_imageLoaded(false)
    , m_windowWidth(kDefaultWindowWidth)
    , m_windowLevel(kDefaultWindowLevel)
    , m_3dOpacity(kDefault3DOpacity)
    , m_toolCrosshairVisible(false)
{
    m_toolPosition[0] = m_toolPosition[1] = m_toolPosition[2] = 0.0;
    setupUI();
    qDebug() << "[FourViewVTKWidget] 纯VTK Widget已创建";
}

FourViewVTKWidget::~FourViewVTKWidget()
{
    qDebug() << "[FourViewVTKWidget] 销毁";
}

QWidget* FourViewVTKWidget::createViewContainer(QVTKOpenGLNativeWidget* vtkWidget,
                                                  const QString& labelText,
                                                  const QString& labelColor)
{
    // 创建容器
    QWidget* container = new QWidget(this);
    container->setStyleSheet("background: transparent;");

    QVBoxLayout* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // 创建带标签的顶部栏
    QFrame* labelBar = new QFrame();
    labelBar->setFixedHeight(24);
    labelBar->setStyleSheet(QString(
        "QFrame { "
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "    stop:0 rgba(15,23,42,0.95), stop:0.5 rgba(30,41,59,0.9), stop:1 rgba(15,23,42,0.95)); "
        "  border: 1px solid rgba(%1, 0.5); "
        "  border-bottom: 2px solid %1; "
        "  border-radius: 4px 4px 0 0; "
        "}"
    ).arg(labelColor));

    QHBoxLayout* labelLayout = new QHBoxLayout(labelBar);
    labelLayout->setContentsMargins(10, 2, 10, 2);

    // 视图标签
    QLabel* viewLabel = new QLabel(labelText);
    viewLabel->setStyleSheet(QString(
        "QLabel { "
        "  color: %1; "
        "  font-size: 11px; "
        "  font-weight: bold; "
        "  background: transparent; "
        "  letter-spacing: 1px; "
        "}"
    ).arg(labelColor));
    labelLayout->addWidget(viewLabel);
    labelLayout->addStretch();

    // VTK widget容器（带边框）
    QFrame* vtkContainer = new QFrame();
    vtkContainer->setStyleSheet(QString(
        "QFrame { "
        "  background: #1a1a2e; "
        "  border: 1px solid rgba(%1, 0.3); "
        "  border-top: none; "
        "  border-radius: 0 0 4px 4px; "
        "}"
    ).arg(labelColor));

    QVBoxLayout* vtkLayout = new QVBoxLayout(vtkContainer);
    vtkLayout->setContentsMargins(1, 1, 1, 1);
    vtkLayout->addWidget(vtkWidget);

    containerLayout->addWidget(labelBar);
    containerLayout->addWidget(vtkContainer, 1);

    return container;
}

void FourViewVTKWidget::setupUI()
{
    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    try {
        // 使用 VTKWidgetFactory 创建 4 个 VTK widgets，确保正确的初始化和异常处理
        qDebug() << "[FourViewVTKWidget] 使用 VTKWidgetFactory 创建 VTK widgets...";

        m_axialViewWidget = VTKWidgetFactory::createVTKWidget(this, false);
        m_sagittalViewWidget = VTKWidgetFactory::createVTKWidget(this, false);
        m_coronalViewWidget = VTKWidgetFactory::createVTKWidget(this, false);
        m_3dViewWidget = VTKWidgetFactory::createVTKWidget(this, false);

        // 检查是否全部创建成功
        if (!m_axialViewWidget || !m_sagittalViewWidget ||
            !m_coronalViewWidget || !m_3dViewWidget) {
            qCritical() << "[FourViewVTKWidget] ✗ VTK widgets 创建失败";
            // 清理已创建的 widgets
            if (m_axialViewWidget) { delete m_axialViewWidget; m_axialViewWidget = nullptr; }
            if (m_sagittalViewWidget) { delete m_sagittalViewWidget; m_sagittalViewWidget = nullptr; }
            if (m_coronalViewWidget) { delete m_coronalViewWidget; m_coronalViewWidget = nullptr; }
            if (m_3dViewWidget) { delete m_3dViewWidget; m_3dViewWidget = nullptr; }
            return;
        }

        qDebug() << "[FourViewVTKWidget] ✓ 所有 VTK widgets 创建成功";

        // 设置背景色
        QString vtkStyle = "background-color: #1a1a2e;";
        m_axialViewWidget->setStyleSheet(vtkStyle);
        m_sagittalViewWidget->setStyleSheet(vtkStyle);
        m_coronalViewWidget->setStyleSheet(vtkStyle);
        m_3dViewWidget->setStyleSheet(vtkStyle);

        // 创建带标签的视图容器并添加到网格布局
        // 颜色方案：轴位-橙色，矢状位-绿色，冠状位-蓝色，3D-紫色
        QWidget* axialContainer = createViewContainer(m_axialViewWidget, "⬛ 轴位 Axial", "#f59e0b");
        QWidget* sagittalContainer = createViewContainer(m_sagittalViewWidget, "⬛ 矢状位 Sagittal", "#10b981");
        QWidget* coronalContainer = createViewContainer(m_coronalViewWidget, "⬛ 冠状位 Coronal", "#3b82f6");
        QWidget* view3DContainer = createViewContainer(m_3dViewWidget, "🎲 3D视图 Volume", "#8b5cf6");

        layout->addWidget(axialContainer, 0, 0);
        layout->addWidget(sagittalContainer, 0, 1);
        layout->addWidget(coronalContainer, 1, 0);
        layout->addWidget(view3DContainer, 1, 1);

    } catch (const std::exception& e) {
        qCritical() << "[FourViewVTKWidget] setupUI 异常:" << e.what();
        // 清理已创建的 widgets
        if (m_axialViewWidget) { delete m_axialViewWidget; m_axialViewWidget = nullptr; }
        if (m_sagittalViewWidget) { delete m_sagittalViewWidget; m_sagittalViewWidget = nullptr; }
        if (m_coronalViewWidget) { delete m_coronalViewWidget; m_coronalViewWidget = nullptr; }
        if (m_3dViewWidget) { delete m_3dViewWidget; m_3dViewWidget = nullptr; }
    } catch (...) {
        qCritical() << "[FourViewVTKWidget] setupUI 未知异常";
        // 清理已创建的 widgets
        if (m_axialViewWidget) { delete m_axialViewWidget; m_axialViewWidget = nullptr; }
        if (m_sagittalViewWidget) { delete m_sagittalViewWidget; m_sagittalViewWidget = nullptr; }
        if (m_coronalViewWidget) { delete m_coronalViewWidget; m_coronalViewWidget = nullptr; }
        if (m_3dViewWidget) { delete m_3dViewWidget; m_3dViewWidget = nullptr; }
    }
}

void FourViewVTKWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    try {
        if (!m_vtkInitialized) {
            initializeVTK();
        }
        // 【防闪烁】不在showEvent中立即恢复渲染
        // 渲染恢复由外部通过延迟调用 resumeRendering() 控制
    } catch (const std::exception& e) {
        qCritical() << "[FourViewVTKWidget] showEvent异常:" << e.what();
    } catch (...) {
        qCritical() << "[FourViewVTKWidget] showEvent未知异常";
    }
}

void FourViewVTKWidget::hideEvent(QHideEvent* event)
{
    try {
        pauseVTKRendering();
    } catch (...) {
        qCritical() << "[FourViewVTKWidget] hideEvent异常";
    }
    QWidget::hideEvent(event);
}

void FourViewVTKWidget::setVisible(bool visible)
{
    try {
        if (!visible && m_vtkInitialized) {
            // 隐藏前先暂停渲染
            pauseVTKRendering();
        }

        QWidget::setVisible(visible);

        if (visible && !m_vtkInitialized) {
            initializeVTK();
        }
        // 【防闪烁】不在setVisible中恢复渲染
        // 渲染恢复由外部通过延迟调用 resumeRendering() 控制
    } catch (const std::exception& e) {
        qCritical() << "[FourViewVTKWidget] setVisible异常:" << e.what();
        QWidget::setVisible(visible);
    } catch (...) {
        qCritical() << "[FourViewVTKWidget] setVisible未知异常";
        QWidget::setVisible(visible);
    }
}

void FourViewVTKWidget::initializeVTK()
{
    if (m_vtkInitialized) return;

    // 检查 VTK widgets 是否已正确创建
    if (!m_axialViewWidget || !m_sagittalViewWidget ||
        !m_coronalViewWidget || !m_3dViewWidget) {
        qCritical() << "[FourViewVTKWidget] VTK widgets 未创建，跳过初始化";
        return;
    }

    try {
        qDebug() << "[FourViewVTKWidget] 初始化VTK组件";

        // 初始化轴位视图
        auto axialRenderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        m_axialViewWidget->setRenderWindow(axialRenderWindow);
        m_axialViewer = vtkSmartPointer<vtkImageViewer2>::New();
        m_axialViewer->SetSliceOrientationToXY();
        auto axialRenderer = vtkSmartPointer<vtkRenderer>::New();
        axialRenderer->SetBackground(0.1, 0.1, 0.15);
        axialRenderWindow->AddRenderer(axialRenderer);
        // 设置2D交互样式（只允许平移和缩放，不允许旋转）
        if (axialRenderWindow->GetInteractor()) {
            auto axialStyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
            axialRenderWindow->GetInteractor()->SetInteractorStyle(axialStyle);
        }

        // 初始化矢状位视图
        auto sagittalRenderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        m_sagittalViewWidget->setRenderWindow(sagittalRenderWindow);
        m_sagittalViewer = vtkSmartPointer<vtkImageViewer2>::New();
        m_sagittalViewer->SetSliceOrientationToYZ();
        auto sagittalRenderer = vtkSmartPointer<vtkRenderer>::New();
        sagittalRenderer->SetBackground(0.1, 0.1, 0.15);
        sagittalRenderWindow->AddRenderer(sagittalRenderer);
        // 设置2D交互样式
        if (sagittalRenderWindow->GetInteractor()) {
            auto sagittalStyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
            sagittalRenderWindow->GetInteractor()->SetInteractorStyle(sagittalStyle);
        }

        // 初始化冠状位视图
        auto coronalRenderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        m_coronalViewWidget->setRenderWindow(coronalRenderWindow);
        m_coronalViewer = vtkSmartPointer<vtkImageViewer2>::New();
        m_coronalViewer->SetSliceOrientationToXZ();
        auto coronalRenderer = vtkSmartPointer<vtkRenderer>::New();
        coronalRenderer->SetBackground(0.1, 0.1, 0.15);
        coronalRenderWindow->AddRenderer(coronalRenderer);
        // 设置2D交互样式
        if (coronalRenderWindow->GetInteractor()) {
            auto coronalStyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
            coronalRenderWindow->GetInteractor()->SetInteractorStyle(coronalStyle);
        }

        // 初始化3D视图
        m_3dRenderer = vtkSmartPointer<vtkRenderer>::New();
        m_3dRenderer->SetBackground(0.1, 0.1, 0.15);
        auto render3DWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        m_3dViewWidget->setRenderWindow(render3DWindow);
        render3DWindow->AddRenderer(m_3dRenderer);
        // 设置3D交互样式（允许旋转）
        if (render3DWindow->GetInteractor()) {
            auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
            render3DWindow->GetInteractor()->SetInteractorStyle(style);

            // 为3D视图添加点击拾取回调，用于光学配准等模块获取影像坐标
            vtkSmartPointer<vtkCallbackCommand> clickCallback =
                vtkSmartPointer<vtkCallbackCommand>::New();
            clickCallback->SetCallback(OnFourView3DLeftButtonDown);
            clickCallback->SetClientData(this);
            render3DWindow->GetInteractor()->AddObserver(
                vtkCommand::LeftButtonPressEvent, clickCallback);
        }

        m_vtkInitialized = true;
        qDebug() << "[FourViewVTKWidget] VTK初始化完成";
    } catch (const std::exception& e) {
        qCritical() << "[FourViewVTKWidget] initializeVTK异常:" << e.what();
        m_vtkInitialized = false;
    } catch (...) {
        qCritical() << "[FourViewVTKWidget] initializeVTK未知异常";
        m_vtkInitialized = false;
    }
}

void FourViewVTKWidget::pauseVTKRendering()
{
    if (!m_vtkInitialized) return;

    qDebug() << "[FourViewVTKWidget] 暂停VTK渲染";

    auto stopRenderWindow = [](QVTKOpenGLNativeWidget* widget) {
        if (!widget || !widget->renderWindow()) return;
        try {
            vtkRenderWindow* rw = widget->renderWindow();
            if (rw) {
                rw->SetAbortRender(1);
                rw->SetSwapBuffers(false);
                if (rw->GetInteractor()) {
                    rw->GetInteractor()->Disable();
                }
            }
        } catch (...) {}
    };

    stopRenderWindow(m_axialViewWidget);
    stopRenderWindow(m_sagittalViewWidget);
    stopRenderWindow(m_coronalViewWidget);
    stopRenderWindow(m_3dViewWidget);
}

void FourViewVTKWidget::resumeVTKRendering()
{
    if (!m_vtkInitialized) return;

    qDebug() << "[FourViewVTKWidget] 恢复VTK渲染";

    auto resumeRenderWindow = [](QVTKOpenGLNativeWidget* widget) {
        if (!widget || !widget->renderWindow()) return;
        try {
            vtkRenderWindow* rw = widget->renderWindow();
            if (rw) {
                rw->SetAbortRender(0);
                rw->SetSwapBuffers(true);
                if (rw->GetInteractor()) {
                    rw->GetInteractor()->Enable();
                }
                rw->Render();
            }
        } catch (...) {}
    };

    resumeRenderWindow(m_axialViewWidget);
    resumeRenderWindow(m_sagittalViewWidget);
    resumeRenderWindow(m_coronalViewWidget);
    resumeRenderWindow(m_3dViewWidget);
}

void FourViewVTKWidget::ensureVTKInitialized()
{
    if (!m_vtkInitialized) {
        initializeVTK();
    }
}

void FourViewVTKWidget::loadImageData(vtkImageData* imageData)
{
    if (!imageData) {
        qWarning() << "[FourViewVTKWidget] 图像数据为空，无法加载";
        return;
    }

    // 兼容：如果此时VTK尚未初始化（例如在Widget首次显示之前就加载了影像），
    // 在这里主动初始化一次，避免‘影像已加载但视图为空’的问题
    if (!m_vtkInitialized) {
        qDebug() << "[FourViewVTKWidget] VTK尚未初始化，在loadImageData中尝试初始化";
        initializeVTK();
        if (!m_vtkInitialized) {
            qWarning() << "[FourViewVTKWidget] initializeVTK失败，无法加载图像数据";
            return;
        }
    }

    qDebug() << "[FourViewVTKWidget] 加载图像数据";

    // 连接viewer到渲染窗口
    m_axialViewer->SetRenderWindow(m_axialViewWidget->renderWindow());
    m_axialViewer->SetInputData(imageData);
    m_axialViewer->SetSlice(m_axialViewer->GetSliceMax() / 2);

    m_sagittalViewer->SetRenderWindow(m_sagittalViewWidget->renderWindow());
    m_sagittalViewer->SetInputData(imageData);
    m_sagittalViewer->SetSlice(m_sagittalViewer->GetSliceMax() / 2);

    m_coronalViewer->SetRenderWindow(m_coronalViewWidget->renderWindow());
    m_coronalViewer->SetInputData(imageData);
    m_coronalViewer->SetSlice(m_coronalViewer->GetSliceMax() / 2);

    updateWindowLevel();

    m_imageLoaded = true;
    qDebug() << "[FourViewVTKWidget] 图像数据加载完成";
}

void FourViewVTKWidget::updateWindowLevel()
{
    if (!m_imageLoaded) return;

    if (m_axialViewer) {
        m_axialViewer->SetColorWindow(m_windowWidth);
        m_axialViewer->SetColorLevel(m_windowLevel);
        m_axialViewer->Render();
    }
    if (m_sagittalViewer) {
        m_sagittalViewer->SetColorWindow(m_windowWidth);
        m_sagittalViewer->SetColorLevel(m_windowLevel);
        m_sagittalViewer->Render();
    }
    if (m_coronalViewer) {
        m_coronalViewer->SetColorWindow(m_windowWidth);
        m_coronalViewer->SetColorLevel(m_windowLevel);
        m_coronalViewer->Render();
    }
}

void FourViewVTKWidget::setAxialSlice(int slice)
{
    if (m_axialViewer && m_imageLoaded) {
        m_axialViewer->SetSlice(slice);
        m_axialViewer->Render();
        emit sliceChanged(0, slice);
    }
}

void FourViewVTKWidget::setSagittalSlice(int slice)
{
    if (m_sagittalViewer && m_imageLoaded) {
        m_sagittalViewer->SetSlice(slice);
        m_sagittalViewer->Render();
        emit sliceChanged(1, slice);
    }
}

void FourViewVTKWidget::setCoronalSlice(int slice)
{
    if (m_coronalViewer && m_imageLoaded) {
        m_coronalViewer->SetSlice(slice);
        m_coronalViewer->Render();
        emit sliceChanged(2, slice);
    }
}

void FourViewVTKWidget::setWindowWidth(double width)
{
    m_windowWidth = width;
    updateWindowLevel();
}

void FourViewVTKWidget::setWindowLevel(double level)
{
    m_windowLevel = level;
    updateWindowLevel();
}

void FourViewVTKWidget::set3DOpacity(double opacity)
{
    m_3dOpacity = opacity;
    // TODO: 更新3D视图透明度
}

void FourViewVTKWidget::resetViews()
{
    if (!m_imageLoaded) return;

    if (m_axialViewer) {
        m_axialViewer->SetSlice(m_axialViewer->GetSliceMax() / 2);
        m_axialViewer->GetRenderer()->ResetCamera();
        m_axialViewer->Render();
    }
    if (m_sagittalViewer) {
        m_sagittalViewer->SetSlice(m_sagittalViewer->GetSliceMax() / 2);
        m_sagittalViewer->GetRenderer()->ResetCamera();
        m_sagittalViewer->Render();
    }
    if (m_coronalViewer) {
        m_coronalViewer->SetSlice(m_coronalViewer->GetSliceMax() / 2);
        m_coronalViewer->GetRenderer()->ResetCamera();
        m_coronalViewer->Render();
    }
    if (m_3dRenderer) {
        m_3dRenderer->ResetCamera();
        m_3dViewWidget->renderWindow()->Render();
    }
}

int FourViewVTKWidget::getAxialSliceMin() const { return m_axialViewer ? m_axialViewer->GetSliceMin() : 0; }
int FourViewVTKWidget::getAxialSliceMax() const { return m_axialViewer ? m_axialViewer->GetSliceMax() : 0; }
int FourViewVTKWidget::getSagittalSliceMin() const { return m_sagittalViewer ? m_sagittalViewer->GetSliceMin() : 0; }
int FourViewVTKWidget::getSagittalSliceMax() const { return m_sagittalViewer ? m_sagittalViewer->GetSliceMax() : 0; }
int FourViewVTKWidget::getCoronalSliceMin() const { return m_coronalViewer ? m_coronalViewer->GetSliceMin() : 0; }
int FourViewVTKWidget::getCoronalSliceMax() const { return m_coronalViewer ? m_coronalViewer->GetSliceMax() : 0; }
int FourViewVTKWidget::getAxialSlice() const { return m_axialViewer ? m_axialViewer->GetSlice() : 0; }
int FourViewVTKWidget::getSagittalSlice() const { return m_sagittalViewer ? m_sagittalViewer->GetSlice() : 0; }
int FourViewVTKWidget::getCoronalSlice() const { return m_coronalViewer ? m_coronalViewer->GetSlice() : 0; }

// ========== 导航工具叠加实现 ==========

void FourViewVTKWidget::updateToolCrosshair(double x, double y, double z)
{
    m_toolPosition[0] = x;
    m_toolPosition[1] = y;
    m_toolPosition[2] = z;

    // TODO: 在2D视图中绘制十字线标记
    // 这需要在各个ImageViewer2的渲染器中添加2D线条Actor
    // 暂时只记录位置，后续可以添加十字线渲染

    if (m_toolCrosshairVisible) {
        qDebug() << "[FourViewVTKWidget] 工具位置更新:" << x << y << z;
    }
}

void FourViewVTKWidget::setToolCrosshairVisible(bool visible)
{
    m_toolCrosshairVisible = visible;
}

void FourViewVTKWidget::render3DView()
{
    if (m_3dViewWidget && m_3dViewWidget->renderWindow()) {
        m_3dViewWidget->renderWindow()->Render();
    }
}
