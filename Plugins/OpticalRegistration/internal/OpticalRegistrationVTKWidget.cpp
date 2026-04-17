#include "OpticalRegistrationVTKWidget.h"
#include "../OpticalRegistrationService.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QShowEvent>
#include <QHideEvent>

#ifdef VTK_FOUND
#include "Framework/VTKWidgetFactory.h"
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCamera.h>
#include <vtkActor.h>
#include <vtkSphereSource.h>
#include <vtkConeSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkAxesActor.h>
#include <vtkTransform.h>
#endif

// ========== 构造/析构 ==========

OpticalRegistrationVTKWidget::OpticalRegistrationVTKWidget(OpticalRegistrationService* service,
                                                           QWidget* parent)
    : QWidget(parent)
    , m_service(service)
    , m_renderingPaused(false)
#ifdef VTK_FOUND
    , m_vtkWidget(nullptr)
    , m_vtkInitialized(false)
#endif
{
    setupUI();
    qDebug() << "[OpticalRegistrationVTKWidget] VTK Widget created";
}

OpticalRegistrationVTKWidget::~OpticalRegistrationVTKWidget()
{
#ifdef VTK_FOUND
    clearPointMarkers();
#endif
    qDebug() << "[OpticalRegistrationVTKWidget] VTK Widget destroyed";
}

// ========== UI初始化 ==========

void OpticalRegistrationVTKWidget::setupUI()
{
    setObjectName("opticalRegistrationVTKWidget");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef VTK_FOUND
    try {
        m_vtkWidget = VTKWidgetFactory::createVTKWidget(this);
        if (m_vtkWidget) {
            m_vtkWidget->setMinimumSize(300, 300);
            layout->addWidget(m_vtkWidget);
            qDebug() << "[OpticalRegistrationVTKWidget] VTK widget created successfully";
        }
    } catch (const std::exception& e) {
        qCritical() << "[OpticalRegistrationVTKWidget] VTK widget creation failed:" << e.what();
        m_vtkWidget = nullptr;
    } catch (...) {
        qCritical() << "[OpticalRegistrationVTKWidget] VTK widget creation failed with unknown error";
        m_vtkWidget = nullptr;
    }

    if (!m_vtkWidget) {
#endif
        // Fallback placeholder
        QLabel* placeholder = new QLabel("3D Tracking View\n(VTK not available)");
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(
            "background: rgba(30,41,59,0.6); "
            "border: 2px dashed rgba(251,191,36,0.5); "
            "border-radius: 10px; "
            "color: #94a3b8; "
            "font-size: 16px; "
            "min-height: 300px;"
        );
        layout->addWidget(placeholder);
#ifdef VTK_FOUND
    }
#endif
}

#ifdef VTK_FOUND
void OpticalRegistrationVTKWidget::initializeVTK()
{
    if (m_vtkInitialized || !m_vtkWidget) return;

    try {
        qDebug() << "[OpticalRegistrationVTKWidget] Initializing VTK pipeline...";

        // Create render window
        m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();

        // Create renderer
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        m_renderer->SetBackground(0.1, 0.12, 0.16);
        m_renderer->SetBackground2(0.05, 0.06, 0.08);
        m_renderer->GradientBackgroundOn();
        m_renderWindow->AddRenderer(m_renderer);

        // Connect to widget
        m_vtkWidget->setRenderWindow(m_renderWindow);

        // Add coordinate axes
        m_axesActor = vtkSmartPointer<vtkAxesActor>::New();
        m_axesActor->SetTotalLength(50, 50, 50);
        m_renderer->AddActor(m_axesActor);

        // Create tool representation (cone)
        vtkSmartPointer<vtkConeSource> cone = vtkSmartPointer<vtkConeSource>::New();
        cone->SetHeight(30);
        cone->SetRadius(5);
        cone->SetResolution(20);

        vtkSmartPointer<vtkPolyDataMapper> coneMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        coneMapper->SetInputConnection(cone->GetOutputPort());

        m_toolActor = vtkSmartPointer<vtkActor>::New();
        m_toolActor->SetMapper(coneMapper);
        m_toolActor->GetProperty()->SetColor(1.0, 0.8, 0.0);  // Yellow tool
        m_toolActor->SetVisibility(false);  // Hidden until tracking starts
        m_renderer->AddActor(m_toolActor);

        // Setup camera
        vtkCamera* camera = m_renderer->GetActiveCamera();
        camera->SetPosition(200, 200, 200);
        camera->SetFocalPoint(0, 0, 0);
        camera->SetViewUp(0, 0, 1);
        m_renderer->ResetCamera();

        m_renderWindow->Render();
        m_vtkInitialized = true;

        qDebug() << "[OpticalRegistrationVTKWidget] VTK initialization complete";
    } catch (const std::exception& e) {
        qCritical() << "[OpticalRegistrationVTKWidget] VTK init error:" << e.what();
        m_vtkInitialized = false;
    }
}
#endif

// ========== 渲染控制 ==========

void OpticalRegistrationVTKWidget::pauseRendering()
{
    m_renderingPaused = true;
#ifdef VTK_FOUND
    if (m_vtkWidget) {
        m_vtkWidget->setUpdatesEnabled(false);
    }
#endif
    qDebug() << "[OpticalRegistrationVTKWidget] Rendering paused";
}

void OpticalRegistrationVTKWidget::resumeRendering()
{
    m_renderingPaused = false;
#ifdef VTK_FOUND
    if (m_vtkWidget) {
        m_vtkWidget->setUpdatesEnabled(true);
        if (m_renderWindow) {
            m_renderWindow->Render();
        }
    }
#endif
    qDebug() << "[OpticalRegistrationVTKWidget] Rendering resumed";
}

// ========== 工具位姿更新 ==========

void OpticalRegistrationVTKWidget::updateToolPose(const QVector3D& position, const QVector3D& rotation)
{
#ifdef VTK_FOUND
    if (!m_vtkInitialized || !m_toolActor) return;

    // Create transform
    vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
    transform->Identity();
    transform->Translate(position.x(), position.y(), position.z());
    transform->RotateZ(rotation.z());
    transform->RotateY(rotation.y());
    transform->RotateX(rotation.x());

    m_toolActor->SetUserTransform(transform);
    m_toolActor->SetVisibility(true);

    render();

    emit toolPoseChanged(position, rotation);
#else
    Q_UNUSED(position);
    Q_UNUSED(rotation);
#endif
}

// ========== 标记点管理 ==========

void OpticalRegistrationVTKWidget::updatePointMarkers()
{
#ifdef VTK_FOUND
    if (!m_vtkInitialized || !m_renderer || !m_service) return;

    clearPointMarkers();

    const auto points = m_service->getAllPoints();
    for (const auto& pt : points) {
        if (pt.hasImagePosition) {
            addPointMarker(pt.imagePosition, true);   // Blue for image points
        }
        if (pt.hasTrackerPosition) {
            addPointMarker(pt.trackerPosition, false); // Yellow for tracker points
        }
    }

    render();
#endif
}

void OpticalRegistrationVTKWidget::clearPointMarkers()
{
#ifdef VTK_FOUND
    if (!m_renderer) return;

    for (auto& actor : m_imagePointMarkers) {
        m_renderer->RemoveActor(actor);
    }
    m_imagePointMarkers.clear();

    for (auto& actor : m_trackerPointMarkers) {
        m_renderer->RemoveActor(actor);
    }
    m_trackerPointMarkers.clear();
#endif
}

#ifdef VTK_FOUND
void OpticalRegistrationVTKWidget::addPointMarker(const QVector3D& pos, bool isImagePoint)
{
    if (!m_renderer) return;

    vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetCenter(pos.x(), pos.y(), pos.z());
    sphere->SetRadius(3.0);
    sphere->SetPhiResolution(16);
    sphere->SetThetaResolution(16);

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(sphere->GetOutputPort());

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    if (isImagePoint) {
        actor->GetProperty()->SetColor(0.4, 0.65, 1.0);  // Blue for image points
        m_imagePointMarkers.append(actor);
    } else {
        actor->GetProperty()->SetColor(1.0, 0.75, 0.15); // Yellow for tracker points
        m_trackerPointMarkers.append(actor);
    }

    actor->GetProperty()->SetAmbient(0.3);
    actor->GetProperty()->SetDiffuse(0.7);
    actor->GetProperty()->SetSpecular(0.3);

    m_renderer->AddActor(actor);
}
#endif

// ========== 视图控制 ==========

void OpticalRegistrationVTKWidget::resetCamera()
{
#ifdef VTK_FOUND
    if (m_renderer) {
        m_renderer->ResetCamera();
        render();
    }
#endif
}

void OpticalRegistrationVTKWidget::render()
{
#ifdef VTK_FOUND
    if (!m_renderingPaused && m_renderWindow) {
        m_renderWindow->Render();
    }
#endif
}

// ========== 事件处理 ==========

void OpticalRegistrationVTKWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
#ifdef VTK_FOUND
    if (!m_vtkInitialized && m_vtkWidget) {
        initializeVTK();
    }
    resumeRendering();
#endif
}

void OpticalRegistrationVTKWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    pauseRendering();
}
