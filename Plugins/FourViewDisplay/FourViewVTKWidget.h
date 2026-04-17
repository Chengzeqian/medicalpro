#ifndef FOUR_VIEW_VTK_WIDGET_H
#define FOUR_VIEW_VTK_WIDGET_H

#include <QWidget>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>
#include <vtkImageViewer2.h>
#include <vtkRenderer.h>
#include <vtkImageData.h>

class FourViewDisplayService;

/**
 * @brief 纯 VTK 渲染 Widget（无控制 UI）
 * 
 * 这个类只包含 4 个 VTK 渲染窗口，没有任何控制 UI（滑块、按钮等）。
 * 控制 UI 应该在调用方（如 SurgicalPlanningTab）中实现。
 * 
 * 这种设计可以避免 VTK 渲染时触发整个插件 Widget 的重绘，从而消除闪烁。
 */
class FourViewVTKWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FourViewVTKWidget(FourViewDisplayService* service, QWidget* parent = nullptr);
    ~FourViewVTKWidget() override;

    // ========== 图像加载 ==========
    void loadImageData(vtkImageData* imageData);
    bool isImageLoaded() const { return m_imageLoaded; }

    // ========== 视图控制（由外部调用） ==========
    void setAxialSlice(int slice);
    void setSagittalSlice(int slice);
    void setCoronalSlice(int slice);
    void setWindowWidth(double width);
    void setWindowLevel(double level);
    void set3DOpacity(double opacity);
    void resetViews();

    // ========== 获取切片范围 ==========
    int getAxialSliceMin() const;
    int getAxialSliceMax() const;
    int getSagittalSliceMin() const;
    int getSagittalSliceMax() const;
    int getCoronalSliceMin() const;
    int getCoronalSliceMax() const;
    
    // ========== 获取当前值 ==========
    int getAxialSlice() const;
    int getSagittalSlice() const;
    int getCoronalSlice() const;

    // ========== VTK 渲染控制 ==========
    void pauseVTKRendering();
    void resumeVTKRendering();

    // ========== 导航工具叠加 ==========
    void updateToolCrosshair(double x, double y, double z);
    void setToolCrosshairVisible(bool visible);
    void render3DView();
    void ensureVTKInitialized();

    // ========== 获取3D渲染器（供服务添加3D内容） ==========
    vtkRenderer* get3DRenderer() const { return m_3dRenderer; }

signals:
    // 导航四视图3D窗口中的选点（世界坐标 / 影像坐标系）
    void imagePointPicked(double x, double y, double z);

    void imageLoaded(const QString& filePath);
    void imageLoadFailed(const QString& error);
    void sliceChanged(int orientation, int slice);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

    // 【关键】重写 setVisible，确保 VTK 渲染状态与可见性同步
    void setVisible(bool visible) override;

private:
    void setupUI();
    void initializeVTK();
    void updateWindowLevel();

    // 创建带标签的视图容器
    QWidget* createViewContainer(QVTKOpenGLNativeWidget* vtkWidget,
                                  const QString& labelText,
                                  const QString& labelColor);

private:
    FourViewDisplayService* m_service;
    
    // VTK Widgets - 4个视图
    QVTKOpenGLNativeWidget* m_axialViewWidget;
    QVTKOpenGLNativeWidget* m_sagittalViewWidget;
    QVTKOpenGLNativeWidget* m_coronalViewWidget;
    QVTKOpenGLNativeWidget* m_3dViewWidget;

    // VTK 对象
    vtkSmartPointer<vtkImageViewer2> m_axialViewer;
    vtkSmartPointer<vtkImageViewer2> m_sagittalViewer;
    vtkSmartPointer<vtkImageViewer2> m_coronalViewer;
    vtkSmartPointer<vtkRenderer> m_3dRenderer;

    // 状态
    bool m_vtkInitialized;
    bool m_imageLoaded;
    double m_windowWidth;
    double m_windowLevel;
    double m_3dOpacity;

    // 工具十字线
    bool m_toolCrosshairVisible;
    double m_toolPosition[3];
};

#endif // FOUR_VIEW_VTK_WIDGET_H
