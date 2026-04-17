#include "Navigation3DViewWidget.h"

#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkSTLReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkSphereSource.h>
#include <vtkProperty.h>
#include <vtkCamera.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>

#include <QDebug>

Navigation3DViewWidget::Navigation3DViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_vtkWidget(nullptr)
    , m_probeRadius(3.0)      // 3mm球体
    , m_boneOpacity(0.8)      // 80%不透明
    , m_initialized(false)
{
    initializeVTK();
    createProbeActor();
    m_initialized = true;
}

Navigation3DViewWidget::~Navigation3DViewWidget()
{
    // VTK智能指针自动清理资源
    if (m_renderer) {
        m_renderer->RemoveAllViewProps();
    }
}

void Navigation3DViewWidget::initializeVTK()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 创建VTK Widget
    m_vtkWidget = new QVTKOpenGLNativeWidget(this);
    layout->addWidget(m_vtkWidget);

    // 创建渲染窗口
    auto renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_vtkWidget->setRenderWindow(renderWindow);

    // 创建渲染器
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderer->SetBackground(0.1, 0.1, 0.15);  // 深灰蓝色背景
    m_renderer->SetBackground2(0.2, 0.2, 0.25);
    m_renderer->GradientBackgroundOn();
    renderWindow->AddRenderer(m_renderer);

    // 设置交互样式
    setupInteraction();
}

void Navigation3DViewWidget::setupInteraction()
{
    auto interactor = m_vtkWidget->interactor();
    if (interactor) {
        auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        interactor->SetInteractorStyle(style);
    }
}

void Navigation3DViewWidget::createProbeActor()
{
    // 创建球体源
    auto sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetRadius(m_probeRadius);
    sphere->SetThetaResolution(32);
    sphere->SetPhiResolution(32);
    sphere->SetCenter(0, 0, 0);

    // 创建Mapper
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(sphere->GetOutputPort());

    // 创建Actor
    m_probeActor = vtkSmartPointer<vtkActor>::New();
    m_probeActor->SetMapper(mapper);

    // 设置探针外观 - 红色
    m_probeActor->GetProperty()->SetColor(1.0, 0.2, 0.2);
    m_probeActor->GetProperty()->SetAmbient(0.3);
    m_probeActor->GetProperty()->SetDiffuse(0.7);
    m_probeActor->GetProperty()->SetSpecular(0.3);
    m_probeActor->GetProperty()->SetSpecularPower(20);

    // 初始状态隐藏
    m_probeActor->SetVisibility(false);

    // 添加到渲染器
    m_renderer->AddActor(m_probeActor);
}

bool Navigation3DViewWidget::loadBoneModel(const QString& stlPath)
{
    if (stlPath.isEmpty()) {
        qWarning() << "Navigation3DViewWidget: Empty STL path";
        return false;
    }

    // 移除旧的骨骼Actor
    if (m_boneActor) {
        m_renderer->RemoveActor(m_boneActor);
        m_boneActor = nullptr;
    }

    // 读取STL文件
    auto reader = vtkSmartPointer<vtkSTLReader>::New();
    reader->SetFileName(stlPath.toStdString().c_str());
    reader->Update();

    // 检查是否读取成功
    if (reader->GetOutput() == nullptr ||
        reader->GetOutput()->GetNumberOfPoints() == 0) {
        qWarning() << "Navigation3DViewWidget: Failed to load STL:" << stlPath;
        emit boneModelLoaded(false, QVector3D(), QVector3D());
        return false;
    }

    // 创建Mapper
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(reader->GetOutputPort());

    // 创建Actor
    m_boneActor = vtkSmartPointer<vtkActor>::New();
    m_boneActor->SetMapper(mapper);

    // 设置骨骼外观 - 骨骼色
    m_boneActor->GetProperty()->SetColor(0.9, 0.85, 0.7);
    m_boneActor->GetProperty()->SetOpacity(m_boneOpacity);
    m_boneActor->GetProperty()->SetAmbient(0.3);
    m_boneActor->GetProperty()->SetDiffuse(0.6);
    m_boneActor->GetProperty()->SetSpecular(0.1);

    // 添加到渲染器
    m_renderer->AddActor(m_boneActor);

    // 重置相机以显示整个模型
    resetCamera();

    // 计算边界框并发送信号
    double bounds[6];
    reader->GetOutput()->GetBounds(bounds);
    QVector3D center((bounds[0] + bounds[1]) / 2,
                      (bounds[2] + bounds[3]) / 2,
                      (bounds[4] + bounds[5]) / 2);
    QVector3D size(bounds[1] - bounds[0],
                   bounds[3] - bounds[2],
                   bounds[5] - bounds[4]);

    qDebug() << "Navigation3DViewWidget: Loaded bone model, center:" << center
             << "size:" << size;

    emit boneModelLoaded(true, center, size);
    return true;
}

bool Navigation3DViewWidget::loadBoneModel(vtkSmartPointer<vtkPolyData> polyData)
{
    if (!polyData || polyData->GetNumberOfPoints() == 0) {
        qWarning() << "Navigation3DViewWidget: Invalid polydata";
        emit boneModelLoaded(false, QVector3D(), QVector3D());
        return false;
    }

    // 移除旧的骨骼Actor
    if (m_boneActor) {
        m_renderer->RemoveActor(m_boneActor);
        m_boneActor = nullptr;
    }

    // 创建Mapper
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);

    // 创建Actor
    m_boneActor = vtkSmartPointer<vtkActor>::New();
    m_boneActor->SetMapper(mapper);

    // 设置骨骼外观
    m_boneActor->GetProperty()->SetColor(0.9, 0.85, 0.7);
    m_boneActor->GetProperty()->SetOpacity(m_boneOpacity);
    m_boneActor->GetProperty()->SetAmbient(0.3);
    m_boneActor->GetProperty()->SetDiffuse(0.6);
    m_boneActor->GetProperty()->SetSpecular(0.1);

    m_renderer->AddActor(m_boneActor);
    resetCamera();

    // 计算边界框
    double bounds[6];
    polyData->GetBounds(bounds);
    QVector3D center((bounds[0] + bounds[1]) / 2,
                      (bounds[2] + bounds[3]) / 2,
                      (bounds[4] + bounds[5]) / 2);
    QVector3D size(bounds[1] - bounds[0],
                   bounds[3] - bounds[2],
                   bounds[5] - bounds[4]);

    emit boneModelLoaded(true, center, size);
    return true;
}

void Navigation3DViewWidget::updateProbePosition(const QVector3D& position)
{
    if (!m_probeActor) {
        return;
    }

    // 更新探针位置
    m_probeActor->SetPosition(position.x(), position.y(), position.z());
    m_probeActor->SetVisibility(true);

    // 渲染更新
    if (m_vtkWidget && m_vtkWidget->renderWindow()) {
        m_vtkWidget->renderWindow()->Render();
    }
}

void Navigation3DViewWidget::setProbeVisible(bool visible)
{
    if (m_probeActor) {
        m_probeActor->SetVisibility(visible);
        render();
    }
}

void Navigation3DViewWidget::setBoneVisible(bool visible)
{
    if (m_boneActor) {
        m_boneActor->SetVisibility(visible);
        render();
    }
}

void Navigation3DViewWidget::setBoneOpacity(double opacity)
{
    m_boneOpacity = qBound(0.0, opacity, 1.0);
    if (m_boneActor) {
        m_boneActor->GetProperty()->SetOpacity(m_boneOpacity);
        render();
    }
}

void Navigation3DViewWidget::setProbeRadius(double radius)
{
    if (radius <= 0 || radius == m_probeRadius) {
        return;
    }

    m_probeRadius = radius;

    // 需要重新创建探针Actor
    if (m_probeActor) {
        // 保存当前位置和可见性
        double pos[3];
        m_probeActor->GetPosition(pos);
        bool visible = m_probeActor->GetVisibility();

        // 移除旧Actor
        m_renderer->RemoveActor(m_probeActor);

        // 创建新的
        createProbeActor();

        // 恢复位置和可见性
        m_probeActor->SetPosition(pos);
        m_probeActor->SetVisibility(visible);

        render();
    }
}

void Navigation3DViewWidget::setProbeColor(double r, double g, double b)
{
    if (m_probeActor) {
        m_probeActor->GetProperty()->SetColor(r, g, b);
        render();
    }
}

void Navigation3DViewWidget::resetCamera()
{
    if (m_renderer) {
        m_renderer->ResetCamera();
        // 稍微拉远相机以获得更好的视角
        if (m_renderer->GetActiveCamera()) {
            m_renderer->GetActiveCamera()->Zoom(0.9);
        }
        render();
    }
}

bool Navigation3DViewWidget::getBoneBounds(double bounds[6]) const
{
    if (!m_boneActor) {
        return false;
    }

    m_boneActor->GetBounds(bounds);
    return true;
}

void Navigation3DViewWidget::render()
{
    if (m_vtkWidget && m_vtkWidget->renderWindow()) {
        m_vtkWidget->renderWindow()->Render();
    }
}
