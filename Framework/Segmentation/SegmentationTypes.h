#ifndef SEGMENTATION_TYPES_H
#define SEGMENTATION_TYPES_H

#include <QDateTime>
#include <QString>
#include <QUuid>

#include <vtkSmartPointer.h>

class vtkSegmentation;

/**
 * @brief 基础分割元数据，供所有分割流程共享
 */
struct SegmentationMetadata
{
    QString id;
    QString name;
    QString sourcePath;
    QDateTime createdAtUtc;
    QDateTime updatedAtUtc;

    bool isValid() const { return !id.isEmpty(); }
};

/**
 * @brief 分割句柄：包装vtkSegmentation指针和元信息，便于在服务与业务层之间传递
 */
struct SegmentationHandle
{
    SegmentationMetadata metadata;
    vtkSmartPointer<vtkSegmentation> segmentation;

    bool isValid() const { return metadata.isValid() && segmentation != nullptr; }
};

/**
 * @brief 生成带唯一ID的默认元数据
 */
inline SegmentationMetadata createSegmentationMetadata(const QString& name = QString(),
                                                       const QString& sourcePath = QString())
{
    SegmentationMetadata meta;
    meta.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    meta.name = name;
    meta.sourcePath = sourcePath;
    meta.createdAtUtc = QDateTime::currentDateTimeUtc();
    meta.updatedAtUtc = meta.createdAtUtc;
    return meta;
}

#endif // SEGMENTATION_TYPES_H
