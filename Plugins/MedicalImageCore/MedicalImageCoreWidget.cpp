#include "MedicalImageCoreWidget.h"
#include "MedicalImageCoreService.h"
#include "MedicalImageCoreServiceImpl.h"  // 用于qobject_cast
#include "MedicalImageData.h"  // 现在来自Framework/Core

#include <ctkPluginContext.h>
#include <ctkServiceReference.h>
#include <service/event/ctkEventAdmin.h>
#include <service/event/ctkEvent.h>
#include <ctkDictionary.h>

#include <QApplication>
#include <QMessageBox>
#include <QHeaderView>
#include <QSplitter>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDebug>
#include <QFrame>
#include <QTimer>
#include <QThread>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

//-----------------------------------------------------------------------------
MedicalImageCoreWidget::MedicalImageCoreWidget(QWidget* parent)
    : QWidget(parent)
    , m_pluginContext(nullptr)
    , m_imageService(nullptr)
    , m_eventAdmin(nullptr)
    , m_mainLayout(nullptr)
    , m_mainSplitter(nullptr)
    , m_toolBarWidget(nullptr)
    , m_imageListWidget(nullptr)
    , m_infoPanelWidget(nullptr)
    , m_statusWidget(nullptr)
    , m_imageTable(nullptr)
    , m_imageInfoText(nullptr)
    , m_metadataTable(nullptr)
    , m_progressBar(nullptr)
    , m_serviceConnected(false)
{
    qDebug() << "[MedicalImageManagerWidget] 创建医学图像管理界面";
    
    // 设置窗口属性
    setWindowTitle("医学图像管理");
    setMinimumSize(1000, 700);
    setAcceptDrops(true);
    
    // 初始化UI
    initializeUI();
    
    // 设置样式
    setupStyles();
    
    // 连接信号槽
    connectSignals();
    
    qDebug() << "[MedicalImageManagerWidget] 医学图像管理界面创建完成";
}

//-----------------------------------------------------------------------------
MedicalImageCoreWidget::~MedicalImageCoreWidget()
{
    qDebug() << "[MedicalImageCoreWidget] 开始销毁医学图像管理界面";
    
    // 断开所有信号连接，防止在析构过程中触发信号
    if (m_imageService) {
        disconnect(m_imageService, nullptr, this, nullptr);
        disconnect(this, nullptr, m_imageService, nullptr);
    }
    
    // 清理可能存在的定时器
    QList<QTimer*> timers = findChildren<QTimer*>();
    for (QTimer* timer : timers) {
        if (timer && timer->isActive()) {
            timer->stop();
        }
    }
    
    // 确保停止任何可能的子线程
    QList<QThread*> threads = findChildren<QThread*>();
    for (QThread* thread : threads) {
        if (thread && thread->isRunning()) {
            thread->quit();
            if (!thread->wait(3000)) { // 等待3秒
                thread->terminate();
                thread->wait(1000);
            }
        }
    }
    
    qDebug() << "[MedicalImageCoreWidget] 医学图像管理界面销毁完成";
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::setPluginContext(ctkPluginContext* context)
{
    m_pluginContext = context;
    qDebug() << "[MedicalImageCoreWidget] 设置CTK插件上下文";

    // 初始化EventAdmin服务
    initializeEventAdmin();

    // 注意：不再通过CTK查找服务，改为通过setImageService直接设置
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::initializeUI()
{
    // 创建主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);
    
    // 创建工具栏
    createToolBar();
    
    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainLayout->addWidget(m_mainSplitter);
    
    // 创建图像列表
    createImageList();
    
    // 创建信息面板
    createInfoPanel();
    
    // 创建状态栏
    createStatusBar();
    
    // 设置分割器比例
    m_mainSplitter->setSizes({600, 400});
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::createToolBar()
{
    m_toolBarWidget = new QWidget(this);
    m_toolBarLayout = new QHBoxLayout(m_toolBarWidget);
    m_toolBarLayout->setContentsMargins(5, 5, 5, 5);
    
    // 文件操作按钮
    m_loadImagesBtn = new QPushButton("加载图像", this);
    m_loadImagesBtn->setIcon(QIcon(":/icons/load_image.png"));
    m_loadImagesBtn->setToolTip("加载医学图像文件");
    
    m_loadDicomSeriesBtn = new QPushButton("加载DICOM序列", this);
    m_loadDicomSeriesBtn->setIcon(QIcon(":/icons/load_dicom.png"));
    m_loadDicomSeriesBtn->setToolTip("加载DICOM序列目录");
    
    m_removeImagesBtn = new QPushButton("删除图像", this);
    m_removeImagesBtn->setIcon(QIcon(":/icons/remove.png"));
    m_removeImagesBtn->setToolTip("删除选中的图像");
    m_removeImagesBtn->setEnabled(false);
    
    m_clearAllBtn = new QPushButton("清除所有", this);
    m_clearAllBtn->setIcon(QIcon(":/icons/clear_all.png"));
    m_clearAllBtn->setToolTip("清除所有已加载的图像");
    
    // 处理操作按钮
    m_exportBtn = new QPushButton("导出", this);
    m_exportBtn->setIcon(QIcon(":/icons/export.png"));
    m_exportBtn->setToolTip("导出选中的图像");
    m_exportBtn->setEnabled(false);
    
    m_convertBtn = new QPushButton("格式转换", this);
    m_convertBtn->setIcon(QIcon(":/icons/convert.png"));
    m_convertBtn->setToolTip("转换图像格式");
    m_convertBtn->setEnabled(false);
    
    m_showInfoBtn = new QPushButton("详细信息", this);
    m_showInfoBtn->setIcon(QIcon(":/icons/info.png"));
    m_showInfoBtn->setToolTip("显示图像详细信息");
    m_showInfoBtn->setEnabled(false);
    
    // 格式过滤器
    m_formatFilterCombo = new QComboBox(this);
    m_formatFilterCombo->addItem("所有格式", "*");
    m_formatFilterCombo->addItem("DICOM", "*.dcm;*.dicom");
    m_formatFilterCombo->addItem("NRRD", "*.nrrd;*.nhdr");
    m_formatFilterCombo->addItem("NIfTI", "*.nii;*.nii.gz");
    m_formatFilterCombo->addItem("图像格式", "*.png;*.jpg;*.jpeg;*.tiff;*.bmp");
    
    // 添加到布局
    m_toolBarLayout->addWidget(m_loadImagesBtn);
    m_toolBarLayout->addWidget(m_loadDicomSeriesBtn);
    
    // 创建分隔符
    QFrame* separator1 = new QFrame(this);
    separator1->setFrameShape(QFrame::VLine);
    separator1->setFrameShadow(QFrame::Sunken);
    m_toolBarLayout->addWidget(separator1);
    
    m_toolBarLayout->addWidget(m_removeImagesBtn);
    m_toolBarLayout->addWidget(m_clearAllBtn);
    
    // 创建分隔符
    QFrame* separator2 = new QFrame(this);
    separator2->setFrameShape(QFrame::VLine);
    separator2->setFrameShadow(QFrame::Sunken);
    m_toolBarLayout->addWidget(separator2);
    
    m_toolBarLayout->addWidget(m_exportBtn);
    m_toolBarLayout->addWidget(m_convertBtn);
    m_toolBarLayout->addWidget(m_showInfoBtn);
    
    // 创建分隔符
    QFrame* separator3 = new QFrame(this);
    separator3->setFrameShape(QFrame::VLine);
    separator3->setFrameShadow(QFrame::Sunken);
    m_toolBarLayout->addWidget(separator3);
    
    m_toolBarLayout->addWidget(new QLabel("格式过滤:", this));
    m_toolBarLayout->addWidget(m_formatFilterCombo);
    m_toolBarLayout->addStretch();
    
    m_mainLayout->addWidget(m_toolBarWidget);
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::createImageList()
{
    m_imageListWidget = new QWidget(this);
    m_imageListLayout = new QVBoxLayout(m_imageListWidget);
    m_imageListLayout->setContentsMargins(5, 5, 5, 5);
    
    // 标题
    m_imageListTitle = new QLabel("已加载图像列表", this);
    m_imageListTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #2c3e50;");
    m_imageListLayout->addWidget(m_imageListTitle);
    
    // 图像表格
    m_imageTable = new QTableWidget(this);
    m_imageTable->setColumnCount(6);
    QStringList headers = {"文件名", "格式", "尺寸", "大小", "加载时间", "状态"};
    m_imageTable->setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    m_imageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_imageTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_imageTable->setAlternatingRowColors(true);
    m_imageTable->horizontalHeader()->setStretchLastSection(true);
    m_imageTable->verticalHeader()->setVisible(false);
    
    // 设置列宽
    m_imageTable->setColumnWidth(0, 200);  // 文件名
    m_imageTable->setColumnWidth(1, 80);   // 格式
    m_imageTable->setColumnWidth(2, 120);  // 尺寸
    m_imageTable->setColumnWidth(3, 80);   // 大小
    m_imageTable->setColumnWidth(4, 140);  // 加载时间
    
    m_imageListLayout->addWidget(m_imageTable);
    m_mainSplitter->addWidget(m_imageListWidget);
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::createInfoPanel()
{
    m_infoPanelWidget = new QWidget(this);
    m_infoPanelLayout = new QVBoxLayout(m_infoPanelWidget);
    m_infoPanelLayout->setContentsMargins(5, 5, 5, 5);
    
    // 标题
    m_infoPanelTitle = new QLabel("图像信息", this);
    m_infoPanelTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #2c3e50;");
    m_infoPanelLayout->addWidget(m_infoPanelTitle);
    
    // 基本信息文本
    m_imageInfoText = new QTextEdit(this);
    m_imageInfoText->setMaximumHeight(200);
    m_imageInfoText->setReadOnly(true);
    m_imageInfoText->setPlainText("请选择图像查看详细信息");
    m_infoPanelLayout->addWidget(m_imageInfoText);
    
    // 元数据组
    m_metadataGroup = new QGroupBox("元数据", this);
    QVBoxLayout* metadataLayout = new QVBoxLayout(m_metadataGroup);
    
    m_metadataTable = new QTableWidget(this);
    m_metadataTable->setColumnCount(2);
    m_metadataTable->setHorizontalHeaderLabels({"属性", "值"});
    m_metadataTable->horizontalHeader()->setStretchLastSection(true);
    m_metadataTable->setColumnWidth(0, 150);
    metadataLayout->addWidget(m_metadataTable);
    
    m_infoPanelLayout->addWidget(m_metadataGroup);
    m_mainSplitter->addWidget(m_infoPanelWidget);
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::createStatusBar()
{
    m_statusWidget = new QWidget(this);
    m_statusLayout = new QHBoxLayout(m_statusWidget);
    m_statusLayout->setContentsMargins(5, 2, 5, 2);
    
    // 状态标签
    m_statusLabel = new QLabel("就绪", this);
    m_statusLayout->addWidget(m_statusLabel);
    
    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumWidth(200);
    m_statusLayout->addWidget(m_progressBar);
    
    m_statusLayout->addStretch();
    
    // 图像计数标签
    m_imageCountLabel = new QLabel("图像数量: 0", this);
    m_statusLayout->addWidget(m_imageCountLabel);
    
    m_mainLayout->addWidget(m_statusWidget);
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::setupStyles()
{
    // 设置整体样式
    setStyleSheet(R"(
        QWidget {
            background-color: #f8f9fa;
            font-family: "Microsoft YaHei", Arial, sans-serif;
        }
        
        QPushButton {
            background-color: #007bff;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 4px;
            font-weight: bold;
        }
        
        QPushButton:hover {
            background-color: #0056b3;
        }
        
        QPushButton:pressed {
            background-color: #004085;
        }
        
        QPushButton:disabled {
            background-color: #6c757d;
        }
        
        QTableWidget {
            background-color: white;
            border: 1px solid #dee2e6;
            border-radius: 4px;
            gridline-color: #dee2e6;
        }
        
        QTableWidget::item {
            padding: 8px;
            border-bottom: 1px solid #dee2e6;
        }
        
        QTableWidget::item:selected {
            background-color: #007bff;
            color: white;
        }
        
        QTextEdit {
            background-color: white;
            border: 1px solid #dee2e6;
            border-radius: 4px;
            padding: 8px;
        }
        
        QGroupBox {
            font-weight: bold;
            border: 1px solid #dee2e6;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 8px;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 8px 0 8px;
        }
    )");
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::connectSignals()
{
    // 工具栏按钮连接
    connect(m_loadImagesBtn, &QPushButton::clicked, this, &MedicalImageCoreWidget::onLoadImages);
    connect(m_loadDicomSeriesBtn, &QPushButton::clicked, this, &MedicalImageCoreWidget::onLoadDicomSeries);
    connect(m_removeImagesBtn, &QPushButton::clicked, this, &MedicalImageCoreWidget::onRemoveImages);
    connect(m_clearAllBtn, &QPushButton::clicked, this, &MedicalImageCoreWidget::onClearAllImages);
    connect(m_exportBtn, &QPushButton::clicked, this, &MedicalImageCoreWidget::onExportImages);
    connect(m_convertBtn, &QPushButton::clicked, this, &MedicalImageCoreWidget::onConvertFormat);
    connect(m_showInfoBtn, &QPushButton::clicked, this, &MedicalImageCoreWidget::onShowImageInfo);
    
    // 表格选择改变
    connect(m_imageTable, &QTableWidget::itemSelectionChanged, 
            this, &MedicalImageCoreWidget::onImageSelectionChanged);
    
    // 双击显示图像
    connect(m_imageTable, &QTableWidget::itemDoubleClicked, 
            [this](QTableWidgetItem* item) {
                if (item && item->row() >= 0) {
                    QString imageId = m_imageTable->item(item->row(), 0)->data(Qt::UserRole).toString();
                    if (!imageId.isEmpty()) {
                        emit requestShowImage(imageId);
                    }
                }
            });
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::setImageService(MedicalImageCoreService* service)
{
    m_imageService = service;
    
    if (m_imageService) {
        m_serviceConnected = true;
        qDebug() << "[MedicalImageCoreWidget] 医学图像服务已设置";
        
        // 连接服务信号
        connect(m_imageService, SIGNAL(imageLoaded(QString, QString)),
                this, SLOT(onImageLoadCompleted(QString, QString)));
        connect(m_imageService, SIGNAL(serviceError(QString)),
                this, SLOT(onImageLoadFailed(QString)));
        
        // 连接异步加载进度信号
        if (auto serviceImpl = qobject_cast<MedicalImageCoreServiceImpl*>(m_imageService)) {
            connect(serviceImpl, SIGNAL(asyncLoadProgress(QString, int, QString)),
                    this, SLOT(onAsyncLoadProgress(QString, int, QString)));
        }
        
        // 获取支持的格式
        m_supportedFormats = m_imageService->getSupportedFormats();
        
        // 刷新显示
        refreshDisplay();
        
        updateStatusMessage("医学图像服务已连接");
    } else {
        qWarning() << "[MedicalImageCoreWidget] 医学图像服务为空";
        m_serviceConnected = false;
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::initializeServiceConnection()
{
    // 现在通过setImageService直接设置服务，不再需要CTK查找
    if (!m_imageService) {
        qWarning() << "[MedicalImageCoreWidget] 医学图像服务未设置，请先调用setImageService";
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::refreshDisplay()
{
    qDebug() << "[MedicalImageCoreWidget::refreshDisplay] 开始刷新显示";
    if (!m_imageService) {
        qDebug() << "[MedicalImageCoreWidget::refreshDisplay] 警告：imageService为空";
        return;
    }
    
    qDebug() << "[MedicalImageCoreWidget::refreshDisplay] 调用updateImageList";
    updateImageList();
    qDebug() << "[MedicalImageCoreWidget::refreshDisplay] 刷新显示完成";
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::updateImageList()
{
    if (!m_imageService) {
        return;
    }
    
    // 获取已加载的图像列表
    QStringList imageIds = m_imageService->getLoadedImages();
    
    // 清空表格
    m_imageTable->setRowCount(0);
    
    // 填充图像信息
    qDebug() << "[MedicalImageCoreWidget::updateImageList] 开始填充图像信息，图像数量:" << imageIds.size();
    for (const QString& imageId : imageIds) {
        qDebug() << "[MedicalImageCoreWidget::updateImageList] 处理图像ID:" << imageId;
        QVariantMap imageDetails = m_imageService->getImageDetails(imageId);
        qDebug() << "[MedicalImageCoreWidget::updateImageList] getImageDetails返回:" << imageDetails.size() << "项数据";
        qDebug() << "[MedicalImageCoreWidget::updateImageList] imageDetails内容:" << imageDetails;
        if (!imageDetails.isEmpty()) {
            qDebug() << "[MedicalImageCoreWidget::updateImageList] imageDetails不为空，开始添加表格行";
            int row = m_imageTable->rowCount();
            m_imageTable->insertRow(row);
            
            // 文件名
            QString filePath = imageDetails.value("filePath", "").toString();
            QTableWidgetItem* nameItem = new QTableWidgetItem(QFileInfo(filePath).fileName());
            nameItem->setData(Qt::UserRole, imageId);
            m_imageTable->setItem(row, 0, nameItem);
            
            // 格式
            QString format = m_imageService->getImageFormat(imageId);
            m_imageTable->setItem(row, 1, new QTableWidgetItem(format));
            
            // 尺寸
            QList<int> dimensions = m_imageService->getImageDimensions(imageId);
            QString dimStr = QString("%1x%2").arg(dimensions.value(0, 0)).arg(dimensions.value(1, 0));
            if (dimensions.size() > 2 && dimensions[2] > 1) {
                dimStr += QString("x%1").arg(dimensions[2]);
            }
            m_imageTable->setItem(row, 2, new QTableWidgetItem(dimStr));
            
            // 大小 (简化显示，避免UI阻塞)
            QString sizeStr = "加载中...";
            // 使用异步方式获取大小，避免UI卡死
            QTimer::singleShot(100, [this, imageId, row]() {
                if (row < m_imageTable->rowCount()) {
                    qint64 dataSize = m_imageService->getImageDataSize(imageId);
                    QString sizeStr = QString::number(dataSize / 1024.0 / 1024.0, 'f', 2) + " MB";
                    if (m_imageTable->item(row, 3)) {
                        m_imageTable->item(row, 3)->setText(sizeStr);
                    }
                }
            });
            m_imageTable->setItem(row, 3, new QTableWidgetItem(sizeStr));
            
            // 加载时间（使用简化的CTK方式）
            m_imageTable->setItem(row, 4, new QTableWidgetItem("已加载"));
            
            // 状态（使用简化的CTK方式）
            m_imageTable->setItem(row, 5, new QTableWidgetItem("有效"));
            
            qDebug() << "[MedicalImageCoreWidget::updateImageList] 成功添加表格行" << row << "图像ID:" << imageId;
        } else {
            qDebug() << "[MedicalImageCoreWidget::updateImageList] WARNING: imageDetails为空，跳过图像ID:" << imageId;
        }
    }
    
    qDebug() << "[MedicalImageCoreWidget::updateImageList] 表格更新完成，最终行数:" << m_imageTable->rowCount();
    
    // 更新计数
    m_imageCountLabel->setText(QString("图像数量: %1").arg(imageIds.size()));
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onLoadImages()
{
    QString filter = "医学图像文件 (*.dcm *.dicom *.nrrd *.nhdr *.nii *.nii.gz *.png *.jpg *.jpeg *.tiff *.bmp);;";
    filter += "DICOM文件 (*.dcm *.dicom);;";
    filter += "NRRD文件 (*.nrrd *.nhdr);;";
    filter += "NIfTI文件 (*.nii *.nii.gz);;";
    filter += "图像文件 (*.png *.jpg *.jpeg *.tiff *.bmp);;";
    filter += "所有文件 (*.*)";
    
    QStringList files = QFileDialog::getOpenFileNames(
        this, "选择医学图像文件", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        filter
    );
    
    if (!files.isEmpty()) {
        loadImageFiles(files);
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onLoadDicomSeries()
{
    QString directory = QFileDialog::getExistingDirectory(
        this, "选择DICOM序列目录",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
    );
    
    if (!directory.isEmpty() && m_imageService) {
        updateStatusMessage("正在加载DICOM序列...");
        m_progressBar->setVisible(true);
        
        // 加载DICOM序列
        QString imageId = m_imageService->loadDicomSeries(directory);
        if (!imageId.isEmpty()) {
            updateStatusMessage("DICOM序列加载成功");
            refreshDisplay();
        } else {
            updateStatusMessage("DICOM序列加载失败");
        }
        
        m_progressBar->setVisible(false);
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::loadImageFiles(const QStringList& filePaths)
{
    if (!m_imageService || filePaths.isEmpty()) {
        return;
    }
    
    updateStatusMessage(QString("正在加载 %1 个图像文件...").arg(filePaths.size()));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, filePaths.size());
    
    int loadedCount = 0;
    for (int i = 0; i < filePaths.size(); ++i) {
        const QString& filePath = filePaths[i];
        
        if (isValidImageFile(filePath)) {
            QString imageId = m_imageService->loadImage(filePath);
            if (!imageId.isEmpty()) {
                loadedCount++;
            }
        }
        
        m_progressBar->setValue(i + 1);
        QApplication::processEvents();
    }
    
    m_progressBar->setVisible(false);
    updateStatusMessage(QString("成功加载 %1 个图像文件").arg(loadedCount));
    refreshDisplay();
}

//-----------------------------------------------------------------------------
bool MedicalImageCoreWidget::isValidImageFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    QStringList validSuffixes = {"dcm", "dicom", "nrrd", "nhdr", "nii", "gz", "png", "jpg", "jpeg", "tiff", "bmp"};
    return validSuffixes.contains(suffix);
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onImageSelectionChanged()
{
    QStringList selectedIds = getSelectedImageIds();
    
    // 更新按钮状态
    bool hasSelection = !selectedIds.isEmpty();
    m_removeImagesBtn->setEnabled(hasSelection);
    m_exportBtn->setEnabled(hasSelection);
    m_convertBtn->setEnabled(hasSelection);
    m_showInfoBtn->setEnabled(hasSelection);
    
    // 更新信息显示
    if (selectedIds.size() == 1) {
        updateImageInfo(selectedIds.first());
        emit imageSelected(selectedIds.first());

        // 发送图像选择事件
        sendImageEvent("selected", selectedIds.first());
    } else if (selectedIds.size() > 1) {
        m_imageInfoText->setPlainText(QString("已选择 %1 个图像").arg(selectedIds.size()));
        m_metadataTable->setRowCount(0);
    } else {
        m_imageInfoText->setPlainText("请选择图像查看详细信息");
        m_metadataTable->setRowCount(0);
    }
}

//-----------------------------------------------------------------------------
QStringList MedicalImageCoreWidget::getSelectedImageIds() const
{
    QStringList ids;
    QList<QTableWidgetItem*> selectedItems = m_imageTable->selectedItems();
    
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }
    
    for (int row : selectedRows) {
        QTableWidgetItem* nameItem = m_imageTable->item(row, 0);
        if (nameItem) {
            QString imageId = nameItem->data(Qt::UserRole).toString();
            if (!imageId.isEmpty()) {
                ids.append(imageId);
            }
        }
    }
    
    return ids;
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::updateImageInfo(const QString& imageId)
{
    if (!m_imageService || imageId.isEmpty()) {
        return;
    }
    
    // 获取详细信息（直接调用）
    QVariantMap details = m_imageService->getImageDetails(imageId);
    if (details.isEmpty()) {
        m_imageInfoText->setPlainText("图像信息不可用");
        return;
    }
    
    // 更新基本信息
    QString filePath = details.value("filePath", "").toString();
    QString info = QString("文件路径: %1\n").arg(filePath);
    
    QString imageFormat = m_imageService->getImageFormat(imageId);
    info += QString("图像格式: %1\n").arg(imageFormat);
    
    QList<int> dimensions = m_imageService->getImageDimensions(imageId);
    info += QString("图像尺寸: %1 x %2").arg(dimensions.value(0, 0)).arg(dimensions.value(1, 0));
    if (dimensions.size() > 2 && dimensions[2] > 1) {
        info += QString(" x %1").arg(dimensions[2]);
    }
    info += "\n";
    
    info += QString("加载时间: %1\n").arg(QDateTime::currentDateTime().toString());
    info += QString("状态: 有效\n");
    
    m_imageInfoText->setPlainText(info);
    
    // 更新元数据表格（使用简化的CTK方式）
    m_metadataTable->setRowCount(3);
    m_metadataTable->insertRow(0);
    m_metadataTable->setItem(0, 0, new QTableWidgetItem("图像格式"));
    m_metadataTable->setItem(0, 1, new QTableWidgetItem("NRRD/NIfTI"));
    m_metadataTable->insertRow(1);
    m_metadataTable->setItem(1, 0, new QTableWidgetItem("加载时间"));
    m_metadataTable->setItem(1, 1, new QTableWidgetItem(QDateTime::currentDateTime().toString()));
    m_metadataTable->insertRow(2);
    m_metadataTable->setItem(2, 0, new QTableWidgetItem("状态"));
    m_metadataTable->setItem(2, 1, new QTableWidgetItem("已加载"));
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::updateProgress(int value)
{
    if (m_progressBar) {
        m_progressBar->setValue(value);
        QApplication::processEvents();
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::updateStatusMessage(const QString& message)
{
    m_statusLabel->setText(message);
    qDebug() << "[MedicalImageManagerWidget]" << message;
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onRemoveImages()
{
    QStringList selectedIds = getSelectedImageIds();
    if (selectedIds.isEmpty()) {
        return;
    }
    
    int result = QMessageBox::question(this, "确认删除", 
                                      QString("确定要删除选中的 %1 个图像吗？").arg(selectedIds.size()),
                                      QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes && m_imageService) {
        for (const QString& imageId : selectedIds) {
            m_imageService->releaseImage(imageId);
        }
        
        refreshDisplay();
        updateStatusMessage(QString("已删除 %1 个图像").arg(selectedIds.size()));
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onClearAllImages()
{
    if (!m_imageService) {
        return;
    }
    
    int result = QMessageBox::question(this, "确认清除", 
                                      "确定要清除所有已加载的图像吗？",
                                      QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        m_imageService->clearAllImages();
        refreshDisplay();
        updateStatusMessage("已清除所有图像");
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onExportImages()
{
    // TODO: 实现图像导出功能
    QMessageBox::information(this, "功能提示", "图像导出功能即将推出");
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onConvertFormat()
{
    // TODO: 实现格式转换功能
    QMessageBox::information(this, "功能提示", "格式转换功能即将推出");
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onShowImageInfo()
{
    QStringList selectedIds = getSelectedImageIds();
    if (selectedIds.isEmpty()) {
        return;
    }
    
    // TODO: 显示详细信息对话框
    QString imageId = selectedIds.first();
    updateImageInfo(imageId);
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onImageLoadCompleted(const QString& operationId, const QString& result)
{
    qDebug() << "[MedicalImageCoreWidget::onImageLoadCompleted] 图像加载完成回调";
    qDebug() << "[MedicalImageCoreWidget::onImageLoadCompleted] operationId:" << operationId;
    qDebug() << "[MedicalImageCoreWidget::onImageLoadCompleted] result:" << result;
    Q_UNUSED(operationId);
    updateStatusMessage(QString("图像加载完成: %1").arg(result));
    qDebug() << "[MedicalImageCoreWidget::onImageLoadCompleted] 调用refreshDisplay";
    refreshDisplay();

    // 发送图像加载完成事件
    sendImageEvent("loaded", result);
    // 发送图像列表更新事件
    sendImageEvent("list_updated");

    qDebug() << "[MedicalImageCoreWidget::onImageLoadCompleted] 回调处理完成";
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onImageLoadFailed(const QString& operationId, const QString& error)
{
    Q_UNUSED(operationId);
    updateStatusMessage(QString("图像加载失败: %1").arg(error));
    QMessageBox::warning(this, "加载失败", error);
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onImageLoadFailed(const QString& error)
{
    updateStatusMessage(QString("图像加载失败: %1").arg(error));
    QMessageBox::warning(this, "加载失败", error);
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::onAsyncLoadProgress(const QString& taskId, int progress, const QString& message)
{
    Q_UNUSED(taskId);
    
    // 更新进度条
    if (m_progressBar) {
        m_progressBar->setVisible(true);
        m_progressBar->setValue(progress);
    }
    
    // 更新状态消息
    updateStatusMessage(QString("正在加载... %1% - %2").arg(progress).arg(message));
    
    qDebug() << "[MedicalImageCoreWidget] 异步加载进度:" << taskId << progress << "%" << message;
    
    // 如果完成，隐藏进度条
    if (progress >= 100) {
        QTimer::singleShot(1000, [this]() {
            if (m_progressBar) {
                m_progressBar->setVisible(false);
            }
        });
    }
}

//-----------------------------------------------------------------------------
// 拖拽支持
void MedicalImageCoreWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        handleFileDrop(urls);
        event->acceptProposedAction();
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::handleFileDrop(const QList<QUrl>& urls)
{
    QStringList filePaths;
    
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            QString filePath = url.toLocalFile();
            QFileInfo fileInfo(filePath);
            
            if (fileInfo.isFile() && isValidImageFile(filePath)) {
                filePaths.append(filePath);
            } else if (fileInfo.isDir()) {
                // 对于目录，检查是否为DICOM序列
                QDir dir(filePath);
                QStringList filters = {"*.dcm", "*.dicom"};
                QStringList dicomFiles = dir.entryList(filters, QDir::Files);
                
                if (!dicomFiles.isEmpty()) {
                    // 这是一个DICOM目录，加载为序列
                    if (m_imageService) {
                        QString imageId = m_imageService->loadDicomSeries(filePath);
                    }
                }
            }
        }
    }
    
    if (!filePaths.isEmpty()) {
        loadImageFiles(filePaths);
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::initializeEventAdmin()
{
    if (!m_pluginContext) {
        qWarning() << "[MedicalImageCoreWidget] CTK插件上下文未设置，无法初始化EventAdmin";
        return;
    }

    // 获取EventAdmin服务
    ctkServiceReference eventAdminRef = m_pluginContext->getServiceReference<ctkEventAdmin>();
    if (eventAdminRef) {
        m_eventAdmin = m_pluginContext->getService<ctkEventAdmin>(eventAdminRef);
        if (m_eventAdmin) {
            qDebug() << "[MedicalImageCoreWidget] EventAdmin服务连接成功";
        } else {
            qWarning() << "[MedicalImageCoreWidget] 无法获取EventAdmin服务实例";
        }
    } else {
        qWarning() << "[MedicalImageCoreWidget] 未找到EventAdmin服务";
    }
}

//-----------------------------------------------------------------------------
void MedicalImageCoreWidget::sendImageEvent(const QString& eventType, const QString& imageId, const QVariantMap& additionalData)
{
    if (!m_eventAdmin) {
        return; // EventAdmin未初始化，静默返回
    }

    try {
        ctkDictionary props;

        // 基本事件信息
        props["eventType"] = eventType;
        props["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        props["source"] = "MedicalImageManager";

        // 图像相关信息
        if (!imageId.isEmpty()) {
            props["imageId"] = imageId;

            // 如果有图像服务，获取额外的图像信息
            if (m_imageService) {
                QVariantMap imageDetails = m_imageService->getImageDetails(imageId);
                props["imageFormat"] = imageDetails.value("format", "Unknown").toString();
                props["filePath"] = imageDetails.value("filePath", "").toString();
            }
        }

        // 添加额外数据
        for (auto it = additionalData.begin(); it != additionalData.end(); ++it) {
            props[it.key()] = it.value();
        }

        // 构造事件主题
        QString topic = QString("medical/image/%1").arg(eventType);

        // 发送事件
        ctkEvent event(topic, props);
        m_eventAdmin->sendEvent(event);

        qDebug() << "[MedicalImageCoreWidget] 发送事件:" << topic << "imageId:" << imageId;

    } catch (const std::exception& e) {
        qWarning() << "[MedicalImageCoreWidget] 发送事件异常:" << e.what();
    } catch (...) {
        qWarning() << "[MedicalImageCoreWidget] 发送事件发生未知异常";
    }
}
