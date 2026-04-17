#ifndef VTKTESTDIALOG_H
#define VTKTESTDIALOG_H

#include <QDialog>

#ifdef VTK_FOUND
class QVTKOpenGLNativeWidget;
#include <vtkSmartPointer.h>
class vtkRenderer;
class vtkGenericOpenGLRenderWindow;
class vtkActor;
#endif

/**
 * @brief 最简单的VTK测试对话框
 * 
 * 用于验证VTK Widget透明问题是否是系统级问题
 * - 不继承任何自定义基类
 * - 不使用任何全局样式表
 * - 最小化配置
 */
class VTKTestDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VTKTestDialog(QWidget* parent = nullptr);
    ~VTKTestDialog();

    void loadSTL(const QString& filePath);

private:
    void setupUI();
    void initializeVTK();

#ifdef VTK_FOUND
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkActor> m_actor;
#endif
};

#endif // VTKTESTDIALOG_H

