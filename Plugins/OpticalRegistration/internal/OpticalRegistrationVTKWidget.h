#ifndef OPTICAL_REGISTRATION_VTK_WIDGET_H
#define OPTICAL_REGISTRATION_VTK_WIDGET_H

/**
 * @file OpticalRegistrationVTKWidget.h
 * @brief 光学配准专用的纯VTK 3D视图Widget
 *
 * 只包含VTK渲染相关功能，不包含任何控制UI（按钮、表格等）。
 * 控制UI由主程序的页面类负责。
 *
 * 功能：
 * - 实时跟踪工具位姿3D可视化
 * - 坐标轴显示
 * - 配准点标记显示
 * - 渲染暂停/恢复（防闪烁）
 */

#include <QWidget>
#include <QVector3D>

#ifdef VTK_FOUND
#include <vtkSmartPointer.h>
class vtkRenderer;
class vtkGenericOpenGLRenderWindow;
class vtkRenderWindowInteractor;
class vtkActor;
class vtkAxesActor;
class QVTKOpenGLNativeWidget;
#endif

class OpticalRegistrationService;

/**
 * @brief 光学配准VTK视图Widget
 *
 * 纯VTK渲染Widget，提供：
 * - 实时工具位姿可视化
 * - 坐标系显示
 * - 配准点标记
 * - 渲染控制
 */
class OpticalRegistrationVTKWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OpticalRegistrationVTKWidget(OpticalRegistrationService* service,
                                          QWidget* parent = nullptr);
    ~OpticalRegistrationVTKWidget() override;

    // ========== 渲染控制 ==========

    /**
     * @brief 暂停渲染
     */
    void pauseRendering();

    /**
     * @brief 恢复渲染
     */
    void resumeRendering();

    /**
     * @brief 检查渲染是否暂停
     */
    bool isRenderingPaused() const { return m_renderingPaused; }

    // ========== 工具位姿更新 ==========

    /**
     * @brief 更新工具位姿显示
     * @param position 位置 [x, y, z]
     * @param rotation 旋转 [rx, ry, rz] 欧拉角（度）
     */
    void updateToolPose(const QVector3D& position, const QVector3D& rotation);

    // ========== 标记点管理 ==========

    /**
     * @brief 刷新配准点标记
     */
    void updatePointMarkers();

    /**
     * @brief 清除所有标记点
     */
    void clearPointMarkers();

    // ========== 视图控制 ==========

    /**
     * @brief 重置相机视角
     */
    void resetCamera();

    /**
     * @brief 渲染一帧
     */
    void render();

signals:
    /**
     * @brief 工具位姿更新信号
     */
    void toolPoseChanged(const QVector3D& position, const QVector3D& rotation);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void setupUI();
    void initializeVTK();
    void addPointMarker(const QVector3D& pos, bool isImagePoint);

private:
    OpticalRegistrationService* m_service;
    bool m_renderingPaused;

#ifdef VTK_FOUND
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkAxesActor> m_axesActor;
    vtkSmartPointer<vtkActor> m_toolActor;  // 工具表示Actor
    QVector<vtkSmartPointer<vtkActor>> m_imagePointMarkers;   // 影像点标记
    QVector<vtkSmartPointer<vtkActor>> m_trackerPointMarkers; // 跟踪点标记
    bool m_vtkInitialized;
#endif
};

#endif // OPTICAL_REGISTRATION_VTK_WIDGET_H
