#include "VTKTestDialog.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QDebug>
#include <QSurfaceFormat>

#ifdef VTK_FOUND
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkSTLReader.h>
#include <vtkProperty.h>
#include <vtkCamera.h>
#include <vtkRenderWindowInteractor.h>
#endif

VTKTestDialog::VTKTestDialog(QWidget* parent)
    : QDialog(parent)
#ifdef VTK_FOUND
    , m_vtkWidget(nullptr)
    , m_renderer(nullptr)
    , m_renderWindow(nullptr)
    , m_actor(nullptr)
#endif
{
    setWindowTitle("VTK 透明测试窗口");
    setModal(true);
    setFixedSize(800, 600);
    setAttribute(Qt::WA_DeleteOnClose);

    qDebug() << "[VTKTestDialog] 创建测试对话框";

    setupUI();
    initializeVTK();
}

VTKTestDialog::~VTKTestDialog()
{
    qDebug() << "[VTKTestDialog] 销毁测试对话框";
}

void VTKTestDialog::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

#ifdef VTK_FOUND
    // 使用 VTK 官方推荐的默认表面格式，避免自定义格式带来的兼容性问题
    QSurfaceFormat format = QVTKOpenGLNativeWidget::defaultFormat();
    QSurfaceFormat::setDefaultFormat(format);

    // 使用最简配置创建 VTK Widget
    m_vtkWidget = new QVTKOpenGLNativeWidget(this);
    m_vtkWidget->setFormat(format);
    m_vtkWidget->setMinimumSize(600, 400);
    layout->addWidget(m_vtkWidget);

    qDebug() << "[VTKTestDialog] Qt SurfaceFormat:"
             << "alpha=" << format.alphaBufferSize()
             << "depth=" << format.depthBufferSize()
             << "stencil=" << format.stencilBufferSize()
             << "samples=" << format.samples()
             << "version=" << format.majorVersion() << "." << format.minorVersion();
#else
    QLabel* label = new QLabel("VTK 未启用");
    layout->addWidget(label);
#endif

    QPushButton* closeBtn = new QPushButton("关闭");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeBtn);
}

void VTKTestDialog::initializeVTK()
{
#ifdef VTK_FOUND
    qDebug() << "[VTKTestDialog] 初始化VTK";

    // 创建渲染窗口（使用默认配置，不做额外修改）
    m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();

    // 创建渲染器 - 使用明显的蓝色背景
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderer->SetBackground(0.0, 0.0, 1.0);  // 纯蓝色背景
    m_renderWindow->AddRenderer(m_renderer);

    // 连接到 Widget
    m_vtkWidget->setRenderWindow(m_renderWindow);

    // 立刻渲染一次，至少应该能看到蓝色背景
    m_renderWindow->Render();

    qDebug() << "[VTKTestDialog] VTK 渲染窗口配置:";
    qDebug() << "  - AlphaBitPlanes:" << m_renderWindow->GetAlphaBitPlanes();
    qDebug() << "  - MultiSamples:" << m_renderWindow->GetMultiSamples();

    qDebug() << "[VTKTestDialog] VTK初始化完成";
#endif
}

void VTKTestDialog::loadSTL(const QString& filePath)
{
#ifdef VTK_FOUND
    qDebug() << "[VTKTestDialog] 加载STL:" << filePath;
    
    vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
    reader->SetFileName(filePath.toStdString().c_str());
    reader->Update();
    
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(reader->GetOutputPort());
    
    m_actor = vtkSmartPointer<vtkActor>::New();
    m_actor->SetMapper(mapper);
    m_actor->GetProperty()->SetColor(1.0, 1.0, 0.0);  // 黄色模型
    
    m_renderer->AddActor(m_actor);
    m_renderer->ResetCamera();
    m_renderWindow->Render();
    
    qDebug() << "[VTKTestDialog] STL加载完成";
#endif
}

