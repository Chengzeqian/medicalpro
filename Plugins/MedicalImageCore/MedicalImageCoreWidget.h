#ifndef MEDICAL_IMAGE_CORE_WIDGET_H
#define MEDICAL_IMAGE_CORE_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTreeWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

// CTK相关
#include <ctkPluginContext.h>

// 前向声明（CTK架构）
class MedicalImageData;
class MedicalImageCoreService;
class ctkEventAdmin;

/**
 * @brief 医学图像管理主界面
 * 
 * 提供完整的医学图像管理功能：
 * - 图像文件浏览和加载
 * - 图像格式转换
 * - 图像信息查看
 * - 批量操作
 * - 拖拽支持
 */
class MedicalImageCoreWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit MedicalImageCoreWidget(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MedicalImageCoreWidget() override;

    /**
     * @brief 设置CTK插件上下文
     * @param context CTK插件上下文
     */
    void setPluginContext(ctkPluginContext* context);

    /**
     * @brief 设置医学图像服务实例
     * @param service 服务实例
     */
    void setImageService(MedicalImageCoreService* service);

    /**
     * @brief 刷新界面显示
     */
    void refreshDisplay();

public slots:
    /**
     * @brief 打开图像文件
     */
    void onLoadImages();

    /**
     * @brief 打开DICOM序列
     */
    void onLoadDicomSeries();

    /**
     * @brief 删除选中图像
     */
    void onRemoveImages();

    /**
     * @brief 清除所有图像
     */
    void onClearAllImages();

    /**
     * @brief 导出图像
     */
    void onExportImages();

    /**
     * @brief 图像格式转换
     */
    void onConvertFormat();

    /**
     * @brief 显示图像信息
     */
    void onShowImageInfo();

    /**
     * @brief 图像列表选择改变
     */
    void onImageSelectionChanged();

    /**
     * @brief 图像加载完成
     * @param operationId 操作ID
     * @param result 加载结果
     */
    void onImageLoadCompleted(const QString& operationId, const QString& result);

    /**
     * @brief 图像加载失败
     * @param operationId 操作ID
     * @param error 错误信息
     */
    void onImageLoadFailed(const QString& operationId, const QString& error);
    
    /**
     * @brief 图像加载失败（单参数版本）
     * @param error 错误信息
     */
    void onImageLoadFailed(const QString& error);
    
    /**
     * @brief 异步加载进度更新
     * @param taskId 任务ID
     * @param progress 进度百分比
     * @param message 进度消息
     */
    void onAsyncLoadProgress(const QString& taskId, int progress, const QString& message);

signals:
    /**
     * @brief 图像选择改变信号
     * @param imageId 选中的图像ID
     */
    void imageSelected(const QString& imageId);

    /**
     * @brief 图像数据改变信号
     */
    void imageDataChanged();

    /**
     * @brief 请求显示图像信号
     * @param imageId 图像ID
     */
    void requestShowImage(const QString& imageId);

protected:
    /**
     * @brief 拖拽进入事件
     */
    void dragEnterEvent(QDragEnterEvent* event) override;

    /**
     * @brief 拖拽移动事件
     */
    void dragMoveEvent(QDragMoveEvent* event) override;

    /**
     * @brief 拖拽放下事件
     */
    void dropEvent(QDropEvent* event) override;

private slots:
    /**
     * @brief 处理文件拖拽
     * @param urls 文件URL列表
     */
    void handleFileDrop(const QList<QUrl>& urls);

    /**
     * @brief 更新进度显示
     * @param value 进度值 (0-100)
     */
    void updateProgress(int value);

    /**
     * @brief 更新状态信息
     * @param message 状态消息
     */
    void updateStatusMessage(const QString& message);

    /**
     * @brief 初始化EventAdmin服务
     */
    void initializeEventAdmin();

    /**
     * @brief 发送图像事件
     * @param eventType 事件类型 ("loaded", "selected", "removed", "list_updated")
     * @param imageId 图像ID
     * @param additionalData 额外数据
     */
    void sendImageEvent(const QString& eventType, const QString& imageId = QString(), const QVariantMap& additionalData = QVariantMap());

private:
    /**
     * @brief 初始化UI界面
     */
    void initializeUI();

    /**
     * @brief 创建工具栏
     */
    void createToolBar();

    /**
     * @brief 创建图像列表
     */
    void createImageList();

    /**
     * @brief 创建信息面板
     */
    void createInfoPanel();

    /**
     * @brief 创建状态栏
     */
    void createStatusBar();

    /**
     * @brief 设置样式
     */
    void setupStyles();

    /**
     * @brief 连接信号槽
     */
    void connectSignals();

    /**
     * @brief 初始化服务连接
     */
    void initializeServiceConnection();

    /**
     * @brief 更新图像列表显示
     */
    void updateImageList();

    /**
     * @brief 更新图像信息显示
     * @param imageId 图像ID
     */
    void updateImageInfo(const QString& imageId);

    /**
     * @brief 获取选中的图像ID列表
     * @return 图像ID列表
     */
    QStringList getSelectedImageIds() const;

    /**
     * @brief 处理图像加载
     * @param filePaths 文件路径列表
     */
    void loadImageFiles(const QStringList& filePaths);

    /**
     * @brief 验证文件格式
     * @param filePath 文件路径
     * @return 是否为支持的格式
     */
    bool isValidImageFile(const QString& filePath) const;

private:
    // CTK相关
    ctkPluginContext* m_pluginContext;
    MedicalImageCoreService* m_imageService; // CTK服务对象
    ctkEventAdmin* m_eventAdmin; // 事件管理服务

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;
    
    // 工具栏
    QWidget* m_toolBarWidget;
    QHBoxLayout* m_toolBarLayout;
    QPushButton* m_loadImagesBtn;
    QPushButton* m_loadDicomSeriesBtn;
    QPushButton* m_removeImagesBtn;
    QPushButton* m_clearAllBtn;
    QPushButton* m_exportBtn;
    QPushButton* m_convertBtn;
    QPushButton* m_showInfoBtn;
    QComboBox* m_formatFilterCombo;

    // 图像列表
    QWidget* m_imageListWidget;
    QVBoxLayout* m_imageListLayout;
    QLabel* m_imageListTitle;
    QTableWidget* m_imageTable;

    // 信息面板
    QWidget* m_infoPanelWidget;
    QVBoxLayout* m_infoPanelLayout;
    QLabel* m_infoPanelTitle;
    QTextEdit* m_imageInfoText;
    QGroupBox* m_metadataGroup;
    QTableWidget* m_metadataTable;

    // 状态栏
    QWidget* m_statusWidget;
    QHBoxLayout* m_statusLayout;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QLabel* m_imageCountLabel;

    // 状态变量
    bool m_serviceConnected;
    QString m_currentImageId;
    QStringList m_supportedFormats;
};

#endif // MEDICAL_IMAGE_CORE_WIDGET_H
