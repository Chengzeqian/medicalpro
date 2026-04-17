#ifndef POINT_REGISTRATION_VTK_WIDGET_H
#define POINT_REGISTRATION_VTK_WIDGET_H

/**
 * @file PointRegistrationVTKWidget.h
 * @brief 点配准专用的纯VTK 3D视图Widget
 *
 * 只包含VTK渲染相关功能，不包含任何控制UI（按钮、表格等）。
 * 控制UI由主程序的页面类负责。
 *
 * 功能：
 * - 3D骨骼模型显示
 * - 配准点标记显示（源点蓝色，目标点绿色）
 * - 3D点击选点
 * - 渲染暂停/恢复（防闪烁）
 */

#include <QWidget>
#include <QVector3D>
#include <QColor>

#ifdef VTK_FOUND
#include <vtkSmartPointer.h>
class vtkRenderer;
class vtkGenericOpenGLRenderWindow;
class vtkRenderWindowInteractor;
class vtkActor;
class vtkFollower;
class vtkPolyData;
class QVTKOpenGLNativeWidget;
#endif

class PointRegistrationService;
struct PointRegistrationResult;

/**
 * @brief 点配准VTK视图Widget
 *
 * 纯VTK渲染Widget，提供：
 * - 3D模型加载和显示
 * - 配准点标记可视化
 * - 3D点击选点功能
 * - 渲染控制
 */
class PointRegistrationVTKWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PointRegistrationVTKWidget(PointRegistrationService* service,
                                        QWidget* parent = nullptr);
    ~PointRegistrationVTKWidget() override;

    // ========== 渲染控制 ==========

    /**
     * @brief 暂停渲染
     * @note 页面隐藏时调用，防止闪烁
     */
    void pauseRendering();

    /**
     * @brief 恢复渲染
     * @note 页面显示时调用
     */
    void resumeRendering();

    /**
     * @brief 检查渲染是否暂停
     */
    bool isRenderingPaused() const { return m_renderingPaused; }

    // ========== 模型加载 ==========

    /**
     * @brief 加载3D模型文件
     * @param filePath 模型文件路径（支持STL、OBJ、PLY）
     * @return 成功返回true
     */
    bool loadModel(const QString& filePath);

#ifdef VTK_FOUND
    /**
     * @brief 直接从 vtkPolyData 加载模型
     * @param polyData VTK数据
     * @param modelName 模型名称（可选）
     * @return 成功返回true
     */
    bool loadModel(vtkSmartPointer<vtkPolyData> polyData, const QString& modelName = QString());
#endif

    /**
     * @brief 清除当前模型
     */
    void clearModel();

    /**
     * @brief 获取模型信息
     * @return 模型信息字符串（文件名+点数）
     */
    QString getModelInfo() const { return m_modelInfo; }

    // ========== 配准可视化 ==========

    /**
     * @brief 显示配准结果（变换后的点与目标点对比）
     * @param result 配准结果
     */
    void showRegistrationResult(const PointRegistrationResult& result);

    /**
     * @brief 显示误差连线（源点到目标点）
     * @param show 是否显示
     */
    void showErrorLines(bool show);

    /**
     * @brief 清除配准结果可视化
     */
    void clearRegistrationVisualization();

    /**
     * @brief 更新探针位置显示
     * @param position 探针位置
     * @param visible 是否显示
     */
    void updateProbePosition(const QVector3D& position, bool visible = true);

    /**
     * @brief 高亮显示选中的点
     * @param pointIndex 点索引 (-1 表示取消所有高亮)
     */
    void highlightPoint(int pointIndex);

    // ========== 标记点管理 ==========

    /**
     * @brief 刷新所有配准点标记
     * @note 从Service获取点数据并更新3D显示
     */
    Q_INVOKABLE void updatePointMarkers();

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

public slots:
    /**
     * @brief 处理3D点击选点事件
     * @param x, y, z 世界坐标
     */
    void onPointPicked(double x, double y, double z);

signals:
    /**
     * @brief 3D点击选点信号
     * @param x, y, z 点击位置的世界坐标
     */
    void pointPicked(double x, double y, double z);

    /**
     * @brief 模型加载完成信号
     * @param success 是否成功
     * @param info 模型信息或错误信息
     */
    void modelLoaded(bool success, const QString& info);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void setupUI();
    void initializeVTK();
    void addPointMarker(const QVector3D& pos, const QColor& color, bool isSource);
    void addPointLabel(const QVector3D& pos, int index);
    void clearPointLabels();
    void addErrorLine(const QVector3D& from, const QVector3D& to, double error);
    void addTransformedPoint(const QVector3D& pos);

private:
    PointRegistrationService* m_service;
    bool m_renderingPaused;
    QString m_modelInfo;
    bool m_errorLinesVisible;
    int m_highlightedPointIndex;

#ifdef VTK_FOUND
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderWindowInteractor> m_interactor;
    vtkSmartPointer<vtkActor> m_modelActor;
    vtkSmartPointer<vtkActor> m_probeActor;           ///< 探针位置标记
    QVector<vtkSmartPointer<vtkActor>> m_sourceMarkers;
    QVector<vtkSmartPointer<vtkActor>> m_targetMarkers;
    QVector<vtkSmartPointer<vtkActor>> m_transformedMarkers;  ///< 变换后的点
    QVector<vtkSmartPointer<vtkActor>> m_errorLines;          ///< 误差连线
    QVector<vtkSmartPointer<vtkFollower>> m_pointLabels;      ///< 点序号标签
    bool m_vtkInitialized;
#endif
};

#endif // POINT_REGISTRATION_VTK_WIDGET_H
