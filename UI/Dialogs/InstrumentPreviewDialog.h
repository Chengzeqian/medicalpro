#ifndef INSTRUMENTPREVIEWDIALOG_H
#define INSTRUMENTPREVIEWDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QString>
#include <QPushButton>

// 前向声明
class InstrumentManagementService;
class Instrument3DPreviewWidget;
class QKeyEvent;
class QPaintEvent;

// 说明：具体的VTK集成细节封装在 Instrument3DPreviewWidget 内部，
// 这里仅在需要时按 VTK_FOUND 进行条件编译 3D 预览相关代码。

/**
 * @brief 器械预览对话框 - 支持静态图片和交互式3D查看器
 *
 * 功能：
 * - 模式1：静态高质量预览图（800x800 PNG）
 * - 模式2：交互式3D模型（支持旋转、缩放、平移）
 * - 一键切换两种模式
 * - 重置视角、切换投影模式
 * - 金属材质和三点照明
 */
class InstrumentPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    enum ViewMode {
        StaticImageMode,    // 静态图片模式
        Interactive3DMode   // 交互式3D模式
    };

    explicit InstrumentPreviewDialog(InstrumentManagementService* service, QWidget *parent = nullptr);
    ~InstrumentPreviewDialog() override;

    /**
     * @brief 设置预览内容
     * @param instrumentName 器械名称
     * @param modelFilePath 3D模型文件路径（STL格式）
     * @param instrumentId 器械ID（用于生成预览图）
     */
    void setPreviewContent(const QString& instrumentName, const QString& modelFilePath, int instrumentId);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onResetView();
    void onToggleProjection();
    void onToggleLighting();
    void onSwitchViewMode();  // 切换查看模式

private:
    void setupUI();
    void loadStaticPreview(const QString& previewImagePath);
    void switchToStaticMode();
    void switchTo3DMode();

    // UI组件
    QLabel* m_titleLabel;
    QLabel* m_imageLabel;        // 静态图片显示
    QLabel* m_loadingLabel;
    QPushButton* m_resetViewBtn;
    QPushButton* m_projectionBtn;
    QPushButton* m_lightingBtn;
    QPushButton* m_switchModeBtn;  // 模式切换按钮
    QWidget* m_controlBar;         // 3D控制栏

    // 服务
    InstrumentManagementService* m_instrumentService;

    // 数据
    ViewMode m_currentMode;
    QString m_modelFilePath;
    QString m_previewImagePath;
    int m_instrumentId;

#ifdef VTK_FOUND
    // 3D预览组件（封装了所有VTK相关逻辑）
    Instrument3DPreviewWidget* m_3dPreviewWidget;

    // 状态标志：用于更新按钮文本并同步到子组件
    bool m_isOrthographic;       // 投影模式标志
    bool m_isThreePointLighting; // 照明模式标志
#endif
};

#endif // INSTRUMENTPREVIEWDIALOG_H

