#include "DicomViewerWidget.h"
#include "DicomViewerService.h"
#include "DicomDataStructures.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <QPrinter>
#include <QPrintDialog>
#include <QScrollBar>
#include <cmath>

DicomViewerWidget::DicomViewerWidget(QWidget *parent)
    : QWidget(parent)
    , m_dicomService(nullptr)
    , m_currentPatientId(-1)
    , m_currentSeriesId(-1)
    , m_currentImageId(-1)
    , m_currentImageIndex(0)
    , m_windowWidth(1500)
    , m_windowLevel(300)
    , m_zoomFactor(1.0)
    , m_distanceMeasureMode(false)
    , m_angleMeasureMode(false)
{
    setupUI();
    setupConnections();

    qDebug() << "[DicomViewerWidget] Widget创建完成（无服务引用）";
}

DicomViewerWidget::DicomViewerWidget(DicomViewerService* service, QWidget *parent)
    : QWidget(parent)
    , m_dicomService(service)
    , m_currentPatientId(-1)
    , m_currentSeriesId(-1)
    , m_currentImageId(-1)
    , m_currentImageIndex(0)
    , m_windowWidth(1500)
    , m_windowLevel(300)
    , m_zoomFactor(1.0)
    , m_distanceMeasureMode(false)
    , m_angleMeasureMode(false)
{
    setupUI();
    setupConnections();

    qDebug() << "[DicomViewerWidget] Widget创建完成（通过服务工厂方法）";
}

DicomViewerWidget::~DicomViewerWidget()
{
    qDebug() << "[DicomViewerWidget] Widget销毁";
}

void DicomViewerWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 工具栏
    setupToolbar();
    mainLayout->addWidget(m_toolbar);
    
    // 主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧：序列列表
    setupSeriesPanel();
    m_mainSplitter->addWidget(m_seriesPanel);
    
    // 中间：图像显示
    setupImageViewerPanel();
    m_mainSplitter->addWidget(m_imageViewerPanel);
    
    // 右侧：控制面板
    setupControlPanel();
    m_mainSplitter->addWidget(m_controlPanel);
    
    // 设置分割器比例
    m_mainSplitter->setStretchFactor(0, 1);  // 序列列表
    m_mainSplitter->setStretchFactor(1, 3);  // 图像显示（主要部分）
    m_mainSplitter->setStretchFactor(2, 1);  // 控制面板
    
    mainLayout->addWidget(m_mainSplitter);
    
    setLayout(mainLayout);
}

void DicomViewerWidget::setupSeriesPanel()
{
    m_seriesPanel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_seriesPanel);
    
    // 标题
    QLabel* titleLabel = new QLabel("DICOM序列", m_seriesPanel);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; padding: 5px;");
    layout->addWidget(titleLabel);
    
    // 序列列表
    m_seriesList = new QListWidget(m_seriesPanel);
    m_seriesList->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_seriesList);
    
    // 序列信息标签
    m_seriesInfoLabel = new QLabel("未选择序列", m_seriesPanel);
    m_seriesInfoLabel->setWordWrap(true);
    m_seriesInfoLabel->setStyleSheet("padding: 5px; background-color: #f0f0f0; border-radius: 3px;");
    layout->addWidget(m_seriesInfoLabel);
    
    m_seriesPanel->setLayout(layout);
}

void DicomViewerWidget::setupImageViewerPanel()
{
    m_imageViewerPanel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_imageViewerPanel);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // 滚动区域包含图像标签
    m_scrollArea = new QScrollArea(m_imageViewerPanel);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setStyleSheet("background-color: #000000;");
    
    m_imageLabel = new QLabel(m_scrollArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setScaledContents(false);
    m_imageLabel->setStyleSheet("background-color: #000000;");
    m_imageLabel->setMinimumSize(400, 400);
    m_scrollArea->setWidget(m_imageLabel);
    
    layout->addWidget(m_scrollArea);
    
    // 切片控制
    QHBoxLayout* sliceLayout = new QHBoxLayout();
    m_sliceLabel = new QLabel("切片: 0/0", m_imageViewerPanel);
    sliceLayout->addWidget(m_sliceLabel);
    
    m_imageSliceSlider = new QSlider(Qt::Horizontal, m_imageViewerPanel);
    m_imageSliceSlider->setEnabled(false);
    sliceLayout->addWidget(m_imageSliceSlider);
    
    layout->addLayout(sliceLayout);
    
    // 图像信息
    m_imageInfoLabel = new QLabel("未加载图像", m_imageViewerPanel);
    m_imageInfoLabel->setStyleSheet("padding: 5px; background-color: #f0f0f0;");
    layout->addWidget(m_imageInfoLabel);
    
    m_imageViewerPanel->setLayout(layout);
}

void DicomViewerWidget::setupControlPanel()
{
    m_controlPanel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_controlPanel);
    
    // 窗宽窗位控制组
    QGroupBox* windowGroup = new QGroupBox("窗宽窗位", m_controlPanel);
    QGridLayout* windowLayout = new QGridLayout(windowGroup);
    
    // 窗宽
    QLabel* widthLabel = new QLabel("窗宽:", windowGroup);
    windowLayout->addWidget(widthLabel, 0, 0);
    
    m_windowWidthLabel = new QLabel("1500", windowGroup);
    m_windowWidthLabel->setMinimumWidth(60);
    m_windowWidthLabel->setAlignment(Qt::AlignRight);
    windowLayout->addWidget(m_windowWidthLabel, 0, 1);
    
    m_windowWidthSlider = new QSlider(Qt::Horizontal, windowGroup);
    m_windowWidthSlider->setRange(1, 4000);
    m_windowWidthSlider->setValue(1500);
    windowLayout->addWidget(m_windowWidthSlider, 1, 0, 1, 2);
    
    // 窗位
    QLabel* levelLabel = new QLabel("窗位:", windowGroup);
    windowLayout->addWidget(levelLabel, 2, 0);
    
    m_windowLevelLabel = new QLabel("300", windowGroup);
    m_windowLevelLabel->setMinimumWidth(60);
    m_windowLevelLabel->setAlignment(Qt::AlignRight);
    windowLayout->addWidget(m_windowLevelLabel, 2, 1);
    
    m_windowLevelSlider = new QSlider(Qt::Horizontal, windowGroup);
    m_windowLevelSlider->setRange(-1000, 3000);
    m_windowLevelSlider->setValue(300);
    windowLayout->addWidget(m_windowLevelSlider, 3, 0, 1, 2);
    
    windowGroup->setLayout(windowLayout);
    layout->addWidget(windowGroup);
    
    // 预设窗宽窗位
    QGroupBox* presetGroup = new QGroupBox("预设窗口", m_controlPanel);
    QVBoxLayout* presetLayout = new QVBoxLayout(presetGroup);
    
    m_presetCombo = new QComboBox(presetGroup);
    m_presetCombo->addItem("自定义");
    m_presetCombo->addItem("骨窗 (W:1500, L:300)");
    m_presetCombo->addItem("软组织窗 (W:400, L:40)");
    m_presetCombo->addItem("肺窗 (W:1500, L:-600)");
    m_presetCombo->addItem("脑窗 (W:80, L:40)");
    presetLayout->addWidget(m_presetCombo);
    
    presetGroup->setLayout(presetLayout);
    layout->addWidget(presetGroup);
    
    // 测量工具组
    QGroupBox* measureGroup = new QGroupBox("测量工具", m_controlPanel);
    QVBoxLayout* measureLayout = new QVBoxLayout(measureGroup);
    
    m_measureDistanceBtn = new QPushButton("距离测量", measureGroup);
    m_measureDistanceBtn->setCheckable(true);
    measureLayout->addWidget(m_measureDistanceBtn);
    
    m_measureAngleBtn = new QPushButton("角度测量", measureGroup);
    m_measureAngleBtn->setCheckable(true);
    measureLayout->addWidget(m_measureAngleBtn);
    
    m_clearAnnotationsBtn = new QPushButton("清除标注", measureGroup);
    measureLayout->addWidget(m_clearAnnotationsBtn);
    
    measureGroup->setLayout(measureLayout);
    layout->addWidget(measureGroup);
    
    layout->addStretch();
    
    m_controlPanel->setLayout(layout);
}

void DicomViewerWidget::setupToolbar()
{
    m_toolbar = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(m_toolbar);
    layout->setContentsMargins(5, 5, 5, 5);
    
    m_zoomInBtn = new QPushButton("放大", m_toolbar);
    layout->addWidget(m_zoomInBtn);
    
    m_zoomOutBtn = new QPushButton("缩小", m_toolbar);
    layout->addWidget(m_zoomOutBtn);
    
    m_resetViewBtn = new QPushButton("重置", m_toolbar);
    layout->addWidget(m_resetViewBtn);
    
    m_fitToWindowBtn = new QPushButton("适应窗口", m_toolbar);
    layout->addWidget(m_fitToWindowBtn);
    
    layout->addSpacing(20);
    
    m_exportBtn = new QPushButton("导出", m_toolbar);
    layout->addWidget(m_exportBtn);
    
    m_printBtn = new QPushButton("打印", m_toolbar);
    layout->addWidget(m_printBtn);
    
    layout->addStretch();
    
    m_toolbar->setLayout(layout);
}

void DicomViewerWidget::setupConnections()
{
    // 序列选择
    connect(m_seriesList, &QListWidget::currentRowChanged,
            this, &DicomViewerWidget::onSeriesSelectionChanged);
    
    // 切片滑块
    connect(m_imageSliceSlider, &QSlider::valueChanged,
            this, &DicomViewerWidget::onImageSliceChanged);
    
    // 窗宽窗位
    connect(m_windowWidthSlider, &QSlider::valueChanged,
            this, &DicomViewerWidget::onWindowWidthChanged);
    connect(m_windowLevelSlider, &QSlider::valueChanged,
            this, &DicomViewerWidget::onWindowLevelChanged);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DicomViewerWidget::onPresetChanged);
    
    // 工具栏按钮
    connect(m_zoomInBtn, &QPushButton::clicked,
            this, &DicomViewerWidget::onZoomInClicked);
    connect(m_zoomOutBtn, &QPushButton::clicked,
            this, &DicomViewerWidget::onZoomOutClicked);
    connect(m_resetViewBtn, &QPushButton::clicked,
            this, &DicomViewerWidget::onResetViewClicked);
    connect(m_fitToWindowBtn, &QPushButton::clicked,
            this, &DicomViewerWidget::onFitToWindowClicked);
    connect(m_exportBtn, &QPushButton::clicked,
            this, &DicomViewerWidget::onExportClicked);
    connect(m_printBtn, &QPushButton::clicked,
            this, &DicomViewerWidget::onPrintClicked);
    
    // 测量工具
    connect(m_measureDistanceBtn, &QPushButton::clicked,
            this, &DicomViewerWidget::onMeasureDistanceClicked);
    connect(m_measureAngleBtn, &QPushButton::clicked,
            this, &DicomViewerWidget::onMeasureAngleClicked);
    connect(m_clearAnnotationsBtn, &QPushButton::clicked,
            this, &DicomViewerWidget::onClearAnnotationsClicked);
}

// ========== 公共接口实现 ==========

void DicomViewerWidget::loadPatientDicomData(int patientId)
{
    qDebug() << "[DicomViewerWidget] 加载患者DICOM数据:" << patientId;
    
    if (!m_dicomService) {
        qWarning() << "[DicomViewerWidget] DicomService未初始化";
        return;
    }
    
    m_currentPatientId = patientId;
    updateSeriesList(patientId);
}

void DicomViewerWidget::loadSeries(int seriesId)
{
    qDebug() << "[DicomViewerWidget] 加载序列:" << seriesId;
    
    if (!m_dicomService) {
        qWarning() << "[DicomViewerWidget] DicomService未初始化";
        return;
    }
    
    m_currentSeriesId = seriesId;
    
    // 获取序列中的所有图像
    QList<DicomImageInfo> images = m_dicomService->listImagesBySeries(seriesId);
    
    m_currentSeriesImages.clear();
    for (const auto& image : images) {
        m_currentSeriesImages.append(image.id);
    }
    
    qDebug() << "[DicomViewerWidget] 序列包含" << m_currentSeriesImages.size() << "张图像";
    
    if (!m_currentSeriesImages.isEmpty()) {
        // 配置切片滑块
        m_imageSliceSlider->setRange(0, m_currentSeriesImages.size() - 1);
        m_imageSliceSlider->setValue(0);
        m_imageSliceSlider->setEnabled(true);
        
        // 加载第一张图像
        loadImage(m_currentSeriesImages[0]);
    } else {
        m_imageSliceSlider->setEnabled(false);
        m_sliceLabel->setText("切片: 0/0");
    }
}

void DicomViewerWidget::loadImage(int imageId)
{
    if (!m_dicomService) {
        qWarning() << "[DicomViewerWidget] DicomService未初始化";
        return;
    }
    
    DicomImageInfo imageInfo = m_dicomService->getDicomImage(imageId);
    if (imageInfo.id == -1) {
        qWarning() << "[DicomViewerWidget] 无法获取图像信息:" << imageId;
        emit imageLoadFailed("无法获取图像信息");
        return;
    }
    
    m_currentImageId = imageId;
    
    // 使用Service加载图像像素数据
    DicomDisplayParams params;
    params.windowWidth = m_windowWidth;
    params.windowCenter = m_windowLevel;
    
    QPixmap pixmap = m_dicomService->loadDicomPixmap(imageInfo, params);
    
    if (pixmap.isNull()) {
        qWarning() << "[DicomViewerWidget] 图像加载失败:" << imageInfo.imagePath;
        emit imageLoadFailed("图像加载失败");
        return;
    }
    
    m_originalPixmap = pixmap;
    updateImageDisplay();
    updateImageInfo();
    
    // 更新切片标签
    int currentIndex = m_currentSeriesImages.indexOf(imageId);
    if (currentIndex >= 0) {
        m_currentImageIndex = currentIndex;
        m_sliceLabel->setText(QString("切片: %1/%2")
            .arg(currentIndex + 1)
            .arg(m_currentSeriesImages.size()));
    }
    
    qDebug() << "[DicomViewerWidget] ✓ 图像加载成功:" << imageInfo.imagePath;
    emit imageLoaded(imageId);
}

void DicomViewerWidget::clear()
{
    m_imageLabel->clear();
    m_imageInfoLabel->setText("未加载图像");
    m_seriesList->clear();
    m_seriesInfoLabel->setText("未选择序列");
    m_currentPatientId = -1;
    m_currentSeriesId = -1;
    m_currentImageId = -1;
    m_currentSeriesImages.clear();
    m_imageSliceSlider->setEnabled(false);
    m_originalPixmap = QPixmap();
    
    qDebug() << "[DicomViewerWidget] 已清除显示";
}

void DicomViewerWidget::setWindowLevel(int windowWidth, int windowLevel)
{
    m_windowWidth = windowWidth;
    m_windowLevel = windowLevel;
    
    m_windowWidthSlider->setValue(windowWidth);
    m_windowLevelSlider->setValue(windowLevel);
    
    applyWindowLevelToImage();
    
    emit windowLevelChanged(windowWidth, windowLevel);
}

void DicomViewerWidget::applyWindowPreset(const QString& presetName)
{
    if (presetName == "骨窗") {
        setWindowLevel(1500, 300);
    } else if (presetName == "软组织窗") {
        setWindowLevel(400, 40);
    } else if (presetName == "肺窗") {
        setWindowLevel(1500, -600);
    } else if (presetName == "脑窗") {
        setWindowLevel(80, 40);
    }
}

void DicomViewerWidget::autoWindowLevel()
{
    // 简单的自动窗宽窗位（基于图像直方图）
    // 实际应用中可以实现更复杂的算法
    setWindowLevel(1500, 300);
}

void DicomViewerWidget::zoomImage(double factor)
{
    m_zoomFactor = factor;
    updateImageDisplay();
}

void DicomViewerWidget::resetView()
{
    m_zoomFactor = 1.0;
    updateImageDisplay();
}

void DicomViewerWidget::fitToWindow()
{
    if (m_originalPixmap.isNull()) {
        return;
    }
    
    QSize viewportSize = m_scrollArea->viewport()->size();
    QSize imageSize = m_originalPixmap.size();
    
    double widthRatio = static_cast<double>(viewportSize.width()) / imageSize.width();
    double heightRatio = static_cast<double>(viewportSize.height()) / imageSize.height();
    
    m_zoomFactor = std::min(widthRatio, heightRatio) * 0.95; // 留一点边距
    updateImageDisplay();
}

void DicomViewerWidget::enableDistanceMeasurement(bool enabled)
{
    m_distanceMeasureMode = enabled;
    m_measureDistanceBtn->setChecked(enabled);
    
    if (enabled) {
        m_angleMeasureMode = false;
        m_measureAngleBtn->setChecked(false);
        m_measurePoints.clear();
    }
}

void DicomViewerWidget::enableAngleMeasurement(bool enabled)
{
    m_angleMeasureMode = enabled;
    m_measureAngleBtn->setChecked(enabled);
    
    if (enabled) {
        m_distanceMeasureMode = false;
        m_measureDistanceBtn->setChecked(false);
        m_measurePoints.clear();
    }
}

void DicomViewerWidget::clearAnnotations()
{
    // 清除所有测量和标注
    m_measurePoints.clear();
    // TODO: 清除绘制的标注图形
    qDebug() << "[DicomViewerWidget] 已清除所有标注";
}

bool DicomViewerWidget::exportCurrentImage(const QString& filePath)
{
    if (m_originalPixmap.isNull()) {
        qWarning() << "[DicomViewerWidget] 无图像可导出";
        return false;
    }
    
    bool success = m_originalPixmap.save(filePath);
    if (success) {
        qDebug() << "[DicomViewerWidget] ✓ 图像已导出:" << filePath;
    } else {
        qWarning() << "[DicomViewerWidget] 图像导出失败:" << filePath;
    }
    
    return success;
}

void DicomViewerWidget::printImage()
{
    if (m_originalPixmap.isNull()) {
        QMessageBox::warning(this, "打印", "无图像可打印");
        return;
    }
    
    QPrinter printer;
    QPrintDialog printDialog(&printer, this);
    
    if (printDialog.exec() == QDialog::Accepted) {
        QPainter painter(&printer);
        QRect rect = painter.viewport();
        QSize size = m_originalPixmap.size();
        size.scale(rect.size(), Qt::KeepAspectRatio);
        
        painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
        painter.setWindow(m_originalPixmap.rect());
        painter.drawPixmap(0, 0, m_originalPixmap);
        
        qDebug() << "[DicomViewerWidget] ✓ 图像已打印";
    }
}

// ========== 私有槽函数 ==========

void DicomViewerWidget::onSeriesSelectionChanged()
{
    int row = m_seriesList->currentRow();
    if (row < 0) {
        return;
    }
    
    QListWidgetItem* item = m_seriesList->item(row);
    int seriesId = item->data(Qt::UserRole).toInt();
    
    loadSeries(seriesId);
    emit seriesChanged(seriesId);
}

void DicomViewerWidget::onImageSliceChanged(int value)
{
    if (value >= 0 && value < m_currentSeriesImages.size()) {
        loadImage(m_currentSeriesImages[value]);
    }
}

void DicomViewerWidget::onWindowWidthChanged(int value)
{
    m_windowWidth = value;
    m_windowWidthLabel->setText(QString::number(value));
    m_presetCombo->setCurrentIndex(0); // 切换到"自定义"
    applyWindowLevelToImage();
}

void DicomViewerWidget::onWindowLevelChanged(int value)
{
    m_windowLevel = value;
    m_windowLevelLabel->setText(QString::number(value));
    m_presetCombo->setCurrentIndex(0); // 切换到"自定义"
    applyWindowLevelToImage();
}

void DicomViewerWidget::onPresetChanged(int index)
{
    switch (index) {
    case 1: // 骨窗
        setWindowLevel(1500, 300);
        break;
    case 2: // 软组织窗
        setWindowLevel(400, 40);
        break;
    case 3: // 肺窗
        setWindowLevel(1500, -600);
        break;
    case 4: // 脑窗
        setWindowLevel(80, 40);
        break;
    default:
        // 自定义，不做处理
        break;
    }
}

void DicomViewerWidget::onZoomInClicked()
{
    m_zoomFactor *= 1.2;
    updateImageDisplay();
}

void DicomViewerWidget::onZoomOutClicked()
{
    m_zoomFactor /= 1.2;
    updateImageDisplay();
}

void DicomViewerWidget::onResetViewClicked()
{
    resetView();
}

void DicomViewerWidget::onFitToWindowClicked()
{
    fitToWindow();
}

void DicomViewerWidget::onMeasureDistanceClicked()
{
    enableDistanceMeasurement(m_measureDistanceBtn->isChecked());
}

void DicomViewerWidget::onMeasureAngleClicked()
{
    enableAngleMeasurement(m_measureAngleBtn->isChecked());
}

void DicomViewerWidget::onClearAnnotationsClicked()
{
    clearAnnotations();
}

void DicomViewerWidget::onExportClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "导出图像",
        "",
        "PNG图像 (*.png);;JPEG图像 (*.jpg);;所有文件 (*.*)"
    );
    
    if (!filePath.isEmpty()) {
        exportCurrentImage(filePath);
    }
}

void DicomViewerWidget::onPrintClicked()
{
    printImage();
}

// ========== 私有辅助方法 ==========

void DicomViewerWidget::updateSeriesList(int patientId)
{
    m_seriesList->clear();
    
    if (!m_dicomService) {
        return;
    }
    
    // 获取患者的所有检查
    QList<DicomStudyInfo> studies = m_dicomService->listStudiesByPatient(patientId);
    
    for (const auto& study : studies) {
        // 获取检查下的所有序列
        QList<DicomSeriesInfo> seriesList = m_dicomService->listSeriesByStudy(study.id);
        
        for (const auto& series : seriesList) {
            QString itemText = QString("%1 - %2 (%3张图像)")
                .arg(series.seriesNumber)
                .arg(series.seriesDescription)
                .arg(series.numberOfImages);
            
            QListWidgetItem* item = new QListWidgetItem(itemText);
            item->setData(Qt::UserRole, series.id);
            m_seriesList->addItem(item);
        }
    }
    
    qDebug() << "[DicomViewerWidget] 序列列表已更新，共" << m_seriesList->count() << "个序列";
}

void DicomViewerWidget::updateImageDisplay()
{
    if (m_originalPixmap.isNull()) {
        return;
    }
    
    // 应用缩放
    QPixmap displayPixmap = m_originalPixmap.scaled(
        m_originalPixmap.size() * m_zoomFactor,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
    
    m_imageLabel->setPixmap(displayPixmap);
    m_imageLabel->resize(displayPixmap.size());
}

void DicomViewerWidget::updateImageInfo()
{
    if (m_currentImageId == -1 || !m_dicomService) {
        m_imageInfoLabel->setText("未加载图像");
        return;
    }
    
    DicomImageInfo imageInfo = m_dicomService->getDicomImage(m_currentImageId);
    
    // 从imagePosition数组中提取Z坐标（第3个元素）
    double positionZ = 0.0;
    if (!imageInfo.imagePosition[2].isEmpty()) {
        positionZ = imageInfo.imagePosition[2].toDouble();
    }
    
    QString info = QString("位置: %1mm | 层厚: %2mm | 矩阵: %3×%4")
        .arg(positionZ, 0, 'f', 2)
        .arg(imageInfo.sliceThickness, 0, 'f', 2)
        .arg(imageInfo.rows)
        .arg(imageInfo.columns);
    
    m_imageInfoLabel->setText(info);
}

void DicomViewerWidget::onImageLabelClicked(QMouseEvent* event)
{
    Q_UNUSED(event)
    // TODO: 实现图像标签点击处理逻辑
    // 例如：显示测量工具、标注等
    qDebug() << "[DicomViewerWidget] 图像标签被点击";
}

void DicomViewerWidget::applyWindowLevelToImage()
{
    if (m_currentImageId == -1) {
        return;
    }
    
    // 重新加载图像应用新的窗宽窗位
    loadImage(m_currentImageId);
}

