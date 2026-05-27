#ifndef NAVIGATION3DVIEWWIDGET_H
#define NAVIGATION3DVIEWWIDGET_H

#include <QWidget>
#include <QVector3D>
#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkRenderer.h>
#include <vtkPolyData.h>

class QVTKOpenGLNativeWidget;
class QVBoxLayout;

/**
 * @brief 导航3D视图Widget
 *
 * 独立的VTK 3D视图，用于实时导航显示：
 * - 显示骨骼STL模型
 * - 显示探针尖端位置（红色球体）
 * - 支持鼠标交互（旋转、缩放、平移）
 */
class Navigation3DViewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit Navigation3DViewWidget(QWidget* parent = nullptr);
    ~Navigation3DViewWidget();

    /**
     * @brief 加载骨骼模型
     * @param stlPath STL文件路径
     * @return 是否加载成功
     */
    bool loadBoneModel(const QString& stlPath);

    /**
     * @brief 从vtkPolyData加载骨骼模型
     * @param polyData 多边形数据
     * @return 是否加载成功
     */
    bool loadBoneModel(vtkSmartPointer<vtkPolyData> polyData);

    /**
     * @brief 更新探针位置（骨骼空间坐标）
     * @param position 探针尖端位置
     */
    void updateProbePosition(const QVector3D& position);

    /**
     * @brief 设置探针可见性
     * @param visible 是否可见
     */
    void setProbeVisible(bool visible);

    /**
     * @brief 设置骨骼模型可见性
     * @param visible 是否可见
     */
    void setBoneVisible(bool visible);

    /**
     * @brief 设置骨骼模型透明度
     * @param opacity 透明度 [0.0, 1.0]
     */
    void setBoneOpacity(double opacity);

    /**
     * @brief 设置探针球体半径
     * @param radius 半径 (mm)
     */
    void setProbeRadius(double radius);

    /**
     * @brief 设置探针颜色
     * @param r, g, b RGB颜色值 [0.0, 1.0]
     */
    void setProbeColor(double r, double g, double b);
    void setTargetRegionMarker(const QVector3D& center, double radiusMm);
    void clearTargetRegionMarker();
    bool hasTargetRegionActor() const;
    void setTargetRegionRiskTone(const QString& tone);
    QString targetRegionRiskTone() const;

    /**
     * @brief 重置相机视角
     */
    void resetCamera();

    /**
     * @brief 获取骨骼模型边界框
     * @param bounds 输出边界 [xmin, xmax, ymin, ymax, zmin, zmax]
     * @return 是否有有效边界
     */
    bool getBoneBounds(double bounds[6]) const;

    /**
     * @brief 强制渲染更新
     */
    void render();

signals:
    /**
     * @brief 骨骼模型加载完成信号
     * @param success 是否成功
     * @param bounds 边界框
     */
    void boneModelLoaded(bool success, const QVector3D& center, const QVector3D& size);

private:
    void initializeVTK();
    void createProbeActor();
    void setupInteraction();

    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkActor> m_boneActor;
    vtkSmartPointer<vtkActor> m_probeActor;
    vtkSmartPointer<vtkActor> m_targetRegionActor;

    double m_probeRadius;     // 探针球体半径 (mm)
    double m_boneOpacity;     // 骨骼透明度
    bool m_initialized;       // VTK是否已初始化
    QString m_targetRegionRiskTone = QStringLiteral("ok");
};

#endif // NAVIGATION3DVIEWWIDGET_H
