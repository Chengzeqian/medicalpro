#include "PointRegistrationVTKWidget.h"
#include "../PointRegistrationService.h"
#include "../PointRegistrationDataStructures.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QFileInfo>
#include <QShowEvent>
#include <QHideEvent>

#ifdef VTK_FOUND
#include "Framework/VTKWidgetFactory.h"
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCamera.h>
#include <vtkActor.h>
#include <vtkSphereSource.h>
#include <vtkConeSource.h>
#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkPointPicker.h>
#include <vtkCellPicker.h>
#include <vtkCallbackCommand.h>
#include <vtkSTLReader.h>
#include <vtkOBJReader.h>
#include <vtkPLYReader.h>
#include <vtkPolyData.h>
#include <vtkTubeFilter.h>
#include <vtkVectorText.h>
#include <vtkFollower.h>
#endif

// ========== VTK点击回调 ==========
#ifdef VTK_FOUND
static void OnLeftButtonDownCallback(vtkObject* caller, unsigned long eventId,
                                     void* clientData, void* callData)
{
    Q_UNUSED(eventId); Q_UNUSED(callData);

    PointRegistrationVTKWidget* widget = static_cast<PointRegistrationVTKWidget*>(clientData);
    if (!widget) return;

    vtkRenderWindowInteractor* interactor = static_cast<vtkRenderWindowInteractor*>(caller);
    if (!interactor) return;

    int* clickPos = interactor->GetEventPosition();

    vtkSmartPointer<vtkPointPicker> picker = vtkSmartPointer<vtkPointPicker>::New();
    picker->SetTolerance(0.005);

    vtkRenderer* renderer = interactor->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
    if (picker->Pick(clickPos[0], clickPos[1], 0, renderer)) {
        double* pos = picker->GetPickPosition();
        emit widget->pointPicked(pos[0], pos[1], pos[2]);
    }
}
#endif

// ========== 构造/析构 ==========

PointRegistrationVTKWidget::PointRegistrationVTKWidget(PointRegistrationService* service,
                                                       QWidget* parent)
    : QWidget(parent)
    , m_service(service)
    , m_renderingPaused(false)
    , m_errorLinesVisible(false)
    , m_highlightedPointIndex(-1)
#ifdef VTK_FOUND
    , m_vtkWidget(nullptr)
    , m_vtkInitialized(false)
#endif
{
    setupUI();
    qDebug() << "[PointRegistrationVTKWidget] VTK Widget created";
}

PointRegistrationVTKWidget::~PointRegistrationVTKWidget()
{
#ifdef VTK_FOUND
    clearPointMarkers();
#endif
    qDebug() << "[PointRegistrationVTKWidget] VTK Widget destroyed";
}

// ========== UI初始化 ==========

void PointRegistrationVTKWidget::setupUI()
{
    setObjectName("pointRegistrationVTKWidget");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef VTK_FOUND
    try {
        m_vtkWidget = VTKWidgetFactory::createVTKWidget(this);
        if (m_vtkWidget) {
            m_vtkWidget->setMinimumSize(300, 300);
            layout->addWidget(m_vtkWidget);
            qDebug() << "[PointRegistrationVTKWidget] VTK widget created successfully";
        }
    } catch (const std::exception& e) {
        qCritical() << "[PointRegistrationVTKWidget] VTK widget creation failed:" << e.what();
        m_vtkWidget = nullptr;
    } catch (...) {
        qCritical() << "[PointRegistrationVTKWidget] VTK widget creation failed with unknown error";
        m_vtkWidget = nullptr;
    }

    if (!m_vtkWidget) {
#endif
        // Fallback placeholder
        QLabel* placeholder = new QLabel("3D View\n(VTK not available)");
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(
            "background: rgba(30,41,59,0.6); "
            "border: 2px dashed rgba(96,165,250,0.5); "
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
void PointRegistrationVTKWidget::initializeVTK()
{
    if (m_vtkInitialized || !m_vtkWidget) return;

    try {
        qDebug() << "[PointRegistrationVTKWidget] Initializing VTK pipeline...";

        // Create render window
        m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();

        // Create renderer
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        m_renderer->SetBackground(0.1, 0.15, 0.2);
        m_renderer->SetBackground2(0.05, 0.08, 0.12);
        m_renderer->GradientBackgroundOn();
        m_renderWindow->AddRenderer(m_renderer);

        // Connect to widget
        m_vtkWidget->setRenderWindow(m_renderWindow);

        // Setup interactor
        m_interactor = m_renderWindow->GetInteractor();
        if (m_interactor) {
            vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
                vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
            m_interactor->SetInteractorStyle(style);

            // Add click callback
            vtkSmartPointer<vtkCallbackCommand> clickCallback =
                vtkSmartPointer<vtkCallbackCommand>::New();
            clickCallback->SetCallback(OnLeftButtonDownCallback);
            clickCallback->SetClientData(this);
            m_interactor->AddObserver(vtkCommand::LeftButtonPressEvent, clickCallback);
        }

        // Setup camera
        vtkCamera* camera = m_renderer->GetActiveCamera();
        camera->SetPosition(0, 0, 500);
        camera->SetFocalPoint(0, 0, 0);
        camera->SetViewUp(0, 1, 0);
        m_renderer->ResetCamera();

        m_renderWindow->Render();
        m_vtkInitialized = true;

        qDebug() << "[PointRegistrationVTKWidget] VTK initialization complete";
    } catch (const std::exception& e) {
        qCritical() << "[PointRegistrationVTKWidget] VTK init error:" << e.what();
        m_vtkInitialized = false;
    }
}
#endif

// ========== 渲染控制 ==========

void PointRegistrationVTKWidget::pauseRendering()
{
    m_renderingPaused = true;
#ifdef VTK_FOUND
    if (m_vtkWidget) {
        m_vtkWidget->setUpdatesEnabled(false);
    }
#endif
    qDebug() << "[PointRegistrationVTKWidget] Rendering paused";
}

void PointRegistrationVTKWidget::resumeRendering()
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
    qDebug() << "[PointRegistrationVTKWidget] Rendering resumed";
}

// ========== 模型加载 ==========

bool PointRegistrationVTKWidget::loadModel(const QString& filePath)
{
#ifdef VTK_FOUND
    if (!m_vtkInitialized) {
        initializeVTK();
    }

    if (!m_renderer) {
        emit modelLoaded(false, "VTK not initialized");
        return false;
    }

    try {
        vtkSmartPointer<vtkPolyData> polyData;
        QString ext = QFileInfo(filePath).suffix().toLower();

        if (ext == "stl") {
            vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
            reader->SetFileName(filePath.toStdString().c_str());
            reader->Update();
            polyData = reader->GetOutput();
        } else if (ext == "obj") {
            vtkSmartPointer<vtkOBJReader> reader = vtkSmartPointer<vtkOBJReader>::New();
            reader->SetFileName(filePath.toStdString().c_str());
            reader->Update();
            polyData = reader->GetOutput();
        } else if (ext == "ply") {
            vtkSmartPointer<vtkPLYReader> reader = vtkSmartPointer<vtkPLYReader>::New();
            reader->SetFileName(filePath.toStdString().c_str());
            reader->Update();
            polyData = reader->GetOutput();
        } else {
            emit modelLoaded(false, "Unsupported file format");
            return false;
        }

        if (polyData && polyData->GetNumberOfPoints() > 0) {
            // Remove old model
            if (m_modelActor) {
                m_renderer->RemoveActor(m_modelActor);
            }

            // Create new model actor
            vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            mapper->SetInputData(polyData);

            m_modelActor = vtkSmartPointer<vtkActor>::New();
            m_modelActor->SetMapper(mapper);
            // Match the planning view's tool model appearance (FourViewDisplay)
            m_modelActor->GetProperty()->SetColor(0.8, 0.9, 1.0);
            m_modelActor->GetProperty()->SetOpacity(1.0);
            m_modelActor->GetProperty()->SetInterpolationToPhong();

            m_renderer->AddActor(m_modelActor);
            m_renderer->ResetCamera();
            m_renderWindow->Render();

            m_modelInfo = QString("%1 (%2 points)")
                .arg(QFileInfo(filePath).fileName())
                .arg(polyData->GetNumberOfPoints());

            emit modelLoaded(true, m_modelInfo);
            return true;
        } else {
            emit modelLoaded(false, "Model is empty");
            return false;
        }
    } catch (const std::exception& e) {
        emit modelLoaded(false, QString("Load error: %1").arg(e.what()));
        return false;
    }
#else
    Q_UNUSED(filePath);
    emit modelLoaded(false, "VTK not enabled");
    return false;
#endif
}

void PointRegistrationVTKWidget::clearModel()
{
#ifdef VTK_FOUND
    if (m_modelActor && m_renderer) {
        m_renderer->RemoveActor(m_modelActor);
        m_modelActor = nullptr;
        m_modelInfo.clear();
        render();
    }
#endif
}

#ifdef VTK_FOUND
bool PointRegistrationVTKWidget::loadModel(vtkSmartPointer<vtkPolyData> polyData, const QString& modelName)
{
    if (!m_vtkInitialized) {
        initializeVTK();
    }

    if (!m_renderer || !polyData) {
        emit modelLoaded(false, "VTK not initialized or invalid data");
        return false;
    }

    try {
        if (polyData->GetNumberOfPoints() > 0) {
            // Remove old model
            if (m_modelActor) {
                m_renderer->RemoveActor(m_modelActor);
            }

            // Create new model actor
            vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            mapper->SetInputData(polyData);

            m_modelActor = vtkSmartPointer<vtkActor>::New();
            m_modelActor->SetMapper(mapper);
            // Match the planning view's tool model appearance (FourViewDisplay)
            m_modelActor->GetProperty()->SetColor(0.8, 0.9, 1.0);
            m_modelActor->GetProperty()->SetOpacity(1.0);
            m_modelActor->GetProperty()->SetInterpolationToPhong();

            m_renderer->AddActor(m_modelActor);
            m_renderer->ResetCamera();
            m_renderWindow->Render();

            m_modelInfo = QString("%1 (%2 points, %3 cells)")
                .arg(modelName.isEmpty() ? "PolyData" : modelName)
                .arg(polyData->GetNumberOfPoints())
                .arg(polyData->GetNumberOfCells());

            emit modelLoaded(true, m_modelInfo);
            return true;
        } else {
            emit modelLoaded(false, "Model is empty");
            return false;
        }
    } catch (const std::exception& e) {
        emit modelLoaded(false, QString("Load error: %1").arg(e.what()));
        return false;
    }
}
#endif

// ========== 标记点管理 ==========

void PointRegistrationVTKWidget::updatePointMarkers()
{
#ifdef VTK_FOUND
    // 确保VTK已初始化
    if (!m_vtkInitialized) {
        initializeVTK();
    }

    if (!m_vtkInitialized || !m_renderer || !m_service) {
        qWarning() << "[PointRegistrationVTKWidget] updatePointMarkers: VTK not ready or service null";
        return;
    }

    qDebug() << "[PointRegistrationVTKWidget] updatePointMarkers called";

    clearPointMarkers();
    clearPointLabels();

    const auto points = m_service->getAllPoints();
    qDebug() << "[PointRegistrationVTKWidget] Total points:" << points.size();

    int sourceIndex = 0;
    for (const auto& pt : points) {
        if (pt.hasSource) {
            qDebug() << "[PointRegistrationVTKWidget] Adding source marker at:"
                     << pt.sourcePosition.x() << pt.sourcePosition.y() << pt.sourcePosition.z();
            addPointMarker(pt.sourcePosition, QColor(96, 165, 250), true);  // Blue for source
            addPointLabel(pt.sourcePosition, sourceIndex);  // Add sequence label
            sourceIndex++;
        }
        if (pt.hasTarget) {
            addPointMarker(pt.targetPosition, QColor(16, 185, 129), false); // Green for target
        }
    }

    qDebug() << "[PointRegistrationVTKWidget] Added" << sourceIndex << "source markers";
    render();
#endif
}

void PointRegistrationVTKWidget::clearPointMarkers()
{
#ifdef VTK_FOUND
    if (!m_renderer) return;

    for (auto& actor : m_sourceMarkers) {
        m_renderer->RemoveActor(actor);
    }
    m_sourceMarkers.clear();

    for (auto& actor : m_targetMarkers) {
        m_renderer->RemoveActor(actor);
    }
    m_targetMarkers.clear();
#endif
}

void PointRegistrationVTKWidget::clearPointLabels()
{
#ifdef VTK_FOUND
    if (!m_renderer) return;

    for (auto& follower : m_pointLabels) {
        m_renderer->RemoveActor(follower);
    }
    m_pointLabels.clear();
#endif
}

#ifdef VTK_FOUND
void PointRegistrationVTKWidget::addPointLabel(const QVector3D& pos, int index)
{
    if (!m_renderer) {
        qWarning() << "[PointRegistrationVTKWidget] addPointLabel: renderer is null!";
        return;
    }

    qDebug() << "[PointRegistrationVTKWidget] addPointLabel P" << (index + 1) << "at:" << pos;

    // Create 3D text using vtkVectorText
    vtkSmartPointer<vtkVectorText> textSource = vtkSmartPointer<vtkVectorText>::New();
    textSource->SetText(QString("P%1").arg(index + 1).toStdString().c_str());
    textSource->Update();

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(textSource->GetOutputPort());

    // Use vtkFollower to always face the camera
    vtkSmartPointer<vtkFollower> follower = vtkSmartPointer<vtkFollower>::New();
    follower->SetMapper(mapper);
    // Offset the label slightly to avoid overlapping with the sphere
    follower->SetPosition(pos.x() + 8.0, pos.y() + 8.0, pos.z() + 8.0);
    follower->SetScale(4.0, 4.0, 4.0);  // 增大标签尺寸
    follower->GetProperty()->SetColor(1.0, 1.0, 0.0);  // Yellow color for better visibility
    follower->GetProperty()->SetAmbient(1.0);
    follower->SetCamera(m_renderer->GetActiveCamera());

    m_renderer->AddActor(follower);
    m_pointLabels.append(follower);

    qDebug() << "[PointRegistrationVTKWidget] Label added for P" << (index + 1);
}
#endif

#ifdef VTK_FOUND
void PointRegistrationVTKWidget::addPointMarker(const QVector3D& pos, const QColor& color, bool isSource)
{
    if (!m_renderer) {
        qWarning() << "[PointRegistrationVTKWidget] addPointMarker: renderer is null!";
        return;
    }

    qDebug() << "[PointRegistrationVTKWidget] addPointMarker at:" << pos
             << "color:" << color.name() << "isSource:" << isSource;

    vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetCenter(pos.x(), pos.y(), pos.z());
    sphere->SetRadius(5.0);  // 增大半径以便更容易看到
    sphere->SetPhiResolution(20);
    sphere->SetThetaResolution(20);
    sphere->Update();

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(sphere->GetOutputPort());

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
    actor->GetProperty()->SetAmbient(0.5);
    actor->GetProperty()->SetDiffuse(0.8);
    actor->GetProperty()->SetSpecular(0.5);
    actor->GetProperty()->SetSpecularPower(20);

    m_renderer->AddActor(actor);
    qDebug() << "[PointRegistrationVTKWidget] Actor added to renderer, total actors:"
             << m_renderer->GetActors()->GetNumberOfItems();

    if (isSource) {
        m_sourceMarkers.append(actor);
    } else {
        m_targetMarkers.append(actor);
    }
}
#endif

// ========== 视图控制 ==========

void PointRegistrationVTKWidget::resetCamera()
{
#ifdef VTK_FOUND
    if (m_renderer) {
        m_renderer->ResetCamera();
        render();
    }
#endif
}

void PointRegistrationVTKWidget::render()
{
#ifdef VTK_FOUND
    if (m_renderingPaused) {
        qDebug() << "[PointRegistrationVTKWidget] render: skipped (paused)";
        return;
    }
    if (!m_renderWindow) {
        qWarning() << "[PointRegistrationVTKWidget] render: renderWindow is null!";
        return;
    }
    qDebug() << "[PointRegistrationVTKWidget] render() called";
    m_renderWindow->Render();
#endif
}

// ========== 配准可视化 ==========

void PointRegistrationVTKWidget::showRegistrationResult(const PointRegistrationResult& result)
{
#ifdef VTK_FOUND
    if (!m_renderer || !m_service) return;

    // 清除旧的可视化
    clearRegistrationVisualization();

    if (!result.success) return;

    const auto points = m_service->getAllPoints();
    const QMatrix4x4& transform = result.transformMatrix;

    int errorIdx = 0;
    for (const auto& pt : points) {
        if (pt.isComplete()) {
            // 计算变换后的源点
            QVector3D transformed = transform.map(pt.sourcePosition);

            // 添加变换后的点标记 (红色)
            addTransformedPoint(transformed);

            // 添加误差连线
            if (errorIdx < result.pointErrors.size()) {
                addErrorLine(transformed, pt.targetPosition, result.pointErrors[errorIdx]);
            }
            errorIdx++;
        }
    }

    m_errorLinesVisible = true;
    render();

    qDebug() << "[PointRegistrationVTKWidget] Registration result visualized, RMS:" << result.rmsError;
#else
    Q_UNUSED(result);
#endif
}

void PointRegistrationVTKWidget::showErrorLines(bool show)
{
#ifdef VTK_FOUND
    m_errorLinesVisible = show;

    for (auto& actor : m_errorLines) {
        if (actor) {
            actor->SetVisibility(show ? 1 : 0);
        }
    }

    for (auto& actor : m_transformedMarkers) {
        if (actor) {
            actor->SetVisibility(show ? 1 : 0);
        }
    }

    render();
#else
    Q_UNUSED(show);
#endif
}

void PointRegistrationVTKWidget::clearRegistrationVisualization()
{
#ifdef VTK_FOUND
    if (!m_renderer) return;

    // 清除变换后的点
    for (auto& actor : m_transformedMarkers) {
        m_renderer->RemoveActor(actor);
    }
    m_transformedMarkers.clear();

    // 清除误差连线
    for (auto& actor : m_errorLines) {
        m_renderer->RemoveActor(actor);
    }
    m_errorLines.clear();

    m_errorLinesVisible = false;
#endif
}

void PointRegistrationVTKWidget::updateProbePosition(const QVector3D& position, bool visible)
{
#ifdef VTK_FOUND
    if (!m_renderer) return;

    if (!m_probeActor) {
        // 创建探针标记 (黄色锥体)
        vtkSmartPointer<vtkConeSource> cone = vtkSmartPointer<vtkConeSource>::New();
        cone->SetHeight(10.0);
        cone->SetRadius(3.0);
        cone->SetResolution(16);
        cone->SetDirection(0, 0, -1);

        vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(cone->GetOutputPort());

        m_probeActor = vtkSmartPointer<vtkActor>::New();
        m_probeActor->SetMapper(mapper);
        m_probeActor->GetProperty()->SetColor(1.0, 0.8, 0.0);  // 黄色
        m_probeActor->GetProperty()->SetAmbient(0.3);
        m_probeActor->GetProperty()->SetDiffuse(0.7);

        m_renderer->AddActor(m_probeActor);
    }

    m_probeActor->SetPosition(position.x(), position.y(), position.z() + 5);
    m_probeActor->SetVisibility(visible ? 1 : 0);

    render();
#else
    Q_UNUSED(position);
    Q_UNUSED(visible);
#endif
}

void PointRegistrationVTKWidget::highlightPoint(int pointIndex)
{
#ifdef VTK_FOUND
    m_highlightedPointIndex = pointIndex;

    // 重置所有标记的大小
    for (int i = 0; i < m_sourceMarkers.size(); ++i) {
        if (m_sourceMarkers[i]) {
            // 高亮选中的点
            if (i == pointIndex) {
                m_sourceMarkers[i]->GetProperty()->SetColor(1.0, 1.0, 0.0);  // 黄色高亮
            } else {
                m_sourceMarkers[i]->GetProperty()->SetColor(0.376, 0.647, 0.98);  // 恢复蓝色
            }
        }
    }

    for (int i = 0; i < m_targetMarkers.size(); ++i) {
        if (m_targetMarkers[i]) {
            if (i == pointIndex) {
                m_targetMarkers[i]->GetProperty()->SetColor(1.0, 1.0, 0.0);  // 黄色高亮
            } else {
                m_targetMarkers[i]->GetProperty()->SetColor(0.063, 0.725, 0.506);  // 恢复绿色
            }
        }
    }

    render();
#else
    Q_UNUSED(pointIndex);
#endif
}

#ifdef VTK_FOUND
void PointRegistrationVTKWidget::addErrorLine(const QVector3D& from, const QVector3D& to, double error)
{
    if (!m_renderer) return;

    vtkSmartPointer<vtkLineSource> line = vtkSmartPointer<vtkLineSource>::New();
    line->SetPoint1(from.x(), from.y(), from.z());
    line->SetPoint2(to.x(), to.y(), to.z());

    // 使用管道让线条更明显
    vtkSmartPointer<vtkTubeFilter> tube = vtkSmartPointer<vtkTubeFilter>::New();
    tube->SetInputConnection(line->GetOutputPort());
    tube->SetRadius(0.5);
    tube->SetNumberOfSides(8);

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(tube->GetOutputPort());

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    // 根据误差大小设置颜色 (绿色->黄色->红色)
    double r, g, b;
    if (error < 1.0) {
        // 绿色 (误差 < 1mm)
        r = 0.0; g = 0.8; b = 0.2;
    } else if (error < 2.0) {
        // 黄色 (误差 1-2mm)
        r = 1.0; g = 0.8; b = 0.0;
    } else {
        // 红色 (误差 > 2mm)
        r = 1.0; g = 0.2; b = 0.2;
    }
    actor->GetProperty()->SetColor(r, g, b);
    actor->GetProperty()->SetOpacity(0.8);

    m_renderer->AddActor(actor);
    m_errorLines.append(actor);
}

void PointRegistrationVTKWidget::addTransformedPoint(const QVector3D& pos)
{
    if (!m_renderer) return;

    vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetCenter(pos.x(), pos.y(), pos.z());
    sphere->SetRadius(2.5);
    sphere->SetPhiResolution(12);
    sphere->SetThetaResolution(12);

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(sphere->GetOutputPort());

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(1.0, 0.4, 0.4);  // 红色表示变换后的点
    actor->GetProperty()->SetAmbient(0.3);
    actor->GetProperty()->SetDiffuse(0.7);
    actor->GetProperty()->SetOpacity(0.9);

    m_renderer->AddActor(actor);
    m_transformedMarkers.append(actor);
}
#endif

// ========== 事件处理 ==========

void PointRegistrationVTKWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
#ifdef VTK_FOUND
    if (!m_vtkInitialized && m_vtkWidget) {
        initializeVTK();
    }
    resumeRendering();
#endif
}

void PointRegistrationVTKWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    pauseRendering();
}

void PointRegistrationVTKWidget::onPointPicked(double x, double y, double z)
{
    // This is called from the VTK callback, just emit the signal
    emit pointPicked(x, y, z);
}
