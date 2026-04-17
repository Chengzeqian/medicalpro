#ifndef INSTRUMENT3DPREVIEWWIDGET_H
#define INSTRUMENT3DPREVIEWWIDGET_H

#include <QWidget>
#include <QFutureWatcher>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QStackedWidget>
#include <QMap>

// VTK Qt 小部件
#include <QVTKOpenGLNativeWidget.h>

class vtkRenderer;
class vtkGenericOpenGLRenderWindow;
class vtkRenderWindowInteractor;
class vtkActor;
class vtkSTLReader;
class vtkPolyDataMapper;
class vtkPolyData;
class QLabel;

/**
 * @brief 器械3D预览组件（优化版）
 *
 * 特性：
 * 1. 预先初始化VTK Widget（确保OpenGL上下文就绪）
 * 2. 占位界面 + 手动加载按钮（按需加载）
 * 3. 异步加载 + 进度显示
 * 4. 模型缓存（第二次加载秒开）
 * 5. 自动模型简化（大模型优化）
 */
class Instrument3DPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit Instrument3DPreviewWidget(QWidget* parent = nullptr);
    ~Instrument3DPreviewWidget() override;

    /** 设置要加载的 STL 模型路径（不立即加载，显示占位界面） */
    void setModelFilePath(const QString& filePath);

    /** 手动触发加载模型 */
    void loadModel();

    /** 重置视角 */
    void resetView();

    /** 切换正交/透视投影 */
    void setParallelProjection(bool enabled);

    /** 切换三点/单点照明 */
    void setThreePointLighting(bool enabled);

    /** 检查模型是否已加载 */
    bool isModelLoaded() const { return m_modelLoaded; }

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // VTK渲染控制 - 防止页面切换时闪烁
    void pauseVTKRendering();
    void resumeVTKRendering();

private slots:
    void onLoadButtonClicked();
    void onModelLoadFinished();
    void onLoadProgress(int progress);

private:
    void initializeVTK();
    void setupPlaceholderUI();
    void loadAndRenderModelAsync();

    struct LoadResult {
        vtkPolyData* polyData;
        int originalCells;
        int finalCells;
        bool simplified;
    };

    static LoadResult loadSTLInBackground(const QString& filePath);
    void renderLoadedModel(vtkPolyData* polyData);
    void applyMetallicMaterial(vtkActor* actor);
    void setupThreePointLighting();
    void setupSingleLighting();
    void resetCameraInternal();
    void cleanupVTK();
    void showLoadingUI();
    void hideLoadingUI();
    void showVTKWidget();
    void updateOverlayGeometry();

    // 缓存相关
    void cacheModel(const QString& filePath, vtkPolyData* polyData);
    vtkPolyData* getCachedModel(const QString& filePath);
    void clearCache();

    QString m_modelFilePath;
    bool m_modelLoaded;

    // UI组件
    QStackedWidget* m_stackedWidget;
    QWidget* m_placeholderWidget;
    QWidget* m_loadingWidget;
    QWidget* m_vtkContainer;

    QPushButton* m_loadButton;
    QLabel* m_placeholderLabel;
    QLabel* m_loadingLabel;
    QProgressBar* m_progressBar;

    // VTK组件
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkRenderer* m_renderer;
    vtkGenericOpenGLRenderWindow* m_renderWindow;
    vtkRenderWindowInteractor* m_interactor;
    vtkActor* m_actor;
    vtkSTLReader* m_stlReader;
    vtkPolyDataMapper* m_mapper;

    bool m_isOrthographic;
    bool m_isThreePointLighting;
    bool m_vtkInitialized;
    bool m_isLoading;

    // 异步加载相关
    QFutureWatcher<LoadResult>* m_loadWatcher;

    // 缓存相关
    static QMap<QString, vtkPolyData*> s_modelCache;
    static const int MAX_CACHE_SIZE = 5;  // 最多缓存5个模型
};

#endif // INSTRUMENT3DPREVIEWWIDGET_H
