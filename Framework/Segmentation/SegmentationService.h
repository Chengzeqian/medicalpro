#ifndef SEGMENTATION_SERVICE_H
#define SEGMENTATION_SERVICE_H

#include "FrameworkExport.h"

#include "Segmentation/SegmentationTypes.h"

#include <QObject>
#include <QList>

class QWidget;

class FRAMEWORK_EXPORT SegmentationService : public QObject
{
    Q_OBJECT

public:
    explicit SegmentationService(QObject* parent = nullptr) : QObject(parent) {}
    ~SegmentationService() override = default;

    virtual QString createEmptySegmentation(const QString& displayName = QString()) = 0;
    virtual bool removeSegmentation(const QString& segmentationId) = 0;
    virtual SegmentationHandle segmentation(const QString& segmentationId) const = 0;

    virtual QList<SegmentationMetadata> listSegmentations() const = 0;
    virtual bool saveSegmentationToFile(const QString& segmentationId, const QString& filePath) = 0;
    virtual QString loadSegmentationFromFile(const QString& filePath) = 0;
    virtual bool setSegmentationSourcePath(const QString& segmentationId, const QString& sourcePath) = 0;

    virtual QWidget* createSegmentEditorWidget(const QString& segmentationId,
        QWidget* parent = nullptr,
        const QString& masterVolumePath = QString()) = 0;

signals:
    void segmentationAdded(const SegmentationMetadata& metadata);
    void segmentationRemoved(const QString& segmentationId);
    void segmentationUpdated(const SegmentationMetadata& metadata);
};

#define SegmentationService_iid "org.medicalpro.SegmentationService"
Q_DECLARE_INTERFACE(SegmentationService, SegmentationService_iid)

#endif // SEGMENTATION_SERVICE_H
