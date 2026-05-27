#pragma once

#include "Framework/Navigation/innovation_experiment_types.h"

#include <QMap>
#include <QList>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector3D>

struct AnkleModelAsset
{
    QString boneName;
    QString sourcePath;
    QString normalizedPath;
    QString sourceType;
};

struct AnkleInstrumentAsset
{
    QString instrumentAssetId;
    QString displayName;
    QString sourcePath;
    QString normalizedPath;
    QString sourceType;
    QString trackingMarkerId;
    QString geometryFilePath;
    QString geometryAssetId;
};

struct AnkleCaseManifest
{
    QString caseId;
    QString patientId;
    QString patientName;
    QString surgeryId;
    QString dicomDir;
    QString workflowStage;
    QString createdAtIso;
    QString updatedAtIso;
    QList<AnkleModelAsset> modelAssets;
    QList<AnkleInstrumentAsset> instrumentAssets;
};

struct AnkleInstrumentGeometryBinding
{
    QString instrumentAssetId;
    QString geometryAssetId;
    QString geometryFilePath;
};

struct AnkleCaseAssetBindings
{
    QString caseId;
    QStringList boundBoneAssetIds;
    QStringList activeBoneAssetIds;
    QStringList boundInstrumentAssetIds;
    QStringList activeInstrumentAssetIds;
    QList<AnkleInstrumentGeometryBinding> instrumentGeometryBindings;
    QString createdAtIso;
    QString updatedAtIso;
};

struct AnkleConstraintRegionMetadata
{
    QString boneName;
    QString regionRole;
    QString source;
    QString version;
};

struct AnklePlanningData
{
    QString caseId;
    QStringList primaryBones;
    QMap<QString, QVector3D> referenceLandmarks;
    QMap<QString, QList<QVector3D>> anatomicalConstraintRegions;
    QMap<QString, AnkleConstraintRegionMetadata> anatomicalConstraintRegionMetadata;
    QStringList recommendedPointOrder;
    QVector3D targetRegionCenter;
    double targetRegionRadiusMm = 0.0;
    QVector3D targetTranslation;
    QQuaternion targetOrientation;
    QString planningFileVersion;
};

struct DigitalTwinTargetRegionDefinition
{
    bool available = false;
    QVector3D centerPatient;
    QVector3D plannedAxisPatient = QVector3D(0.0f, 0.0f, 1.0f);
    double radiusMm = 0.0;
};

struct TargetRegionNavigationStatus
{
    bool targetRegionAvailable = false;
    double distanceToTargetMm = 0.0;
    double angleErrorDeg = 0.0;
    double targetHitProbability = 0.0;
    double localConfidenceScore = 0.0;
};

struct DigitalTwinRiskReport
{
    QString dominantRiskSource;
    QStringList riskReasons;
    QVariantMap rawMetrics;
};

struct DigitalTwinState
{
    bool valid = false;
    QString statusCode;
    QString statusText;
    double twinConfidenceScore = 0.0;
    double localRiskScore = 0.0;
    bool allowNavigation = false;
    bool reRegisterRecommended = false;
    bool trackingDegradationDetected = false;
    QVariantMap evidence;
};

struct NavigationDisplayState
{
    QString activeToolId;
    QString activeToolModelPath;
    QStringList boneModelPaths;
    bool toolVisible = false;
    bool validPose = false;
    QString statusText;
    QMatrix4x4 vtkToolTransform;
};

struct AnkleRegistrationRecord
{
    QString caseId;
    QString registrationMode;
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    QVariantMap metrics;
};

struct AnkleNavigationRunRecord
{
    QString caseId;
    QString navigationMode;
    double confidenceScore = 0.0;
    QStringList warnings;
    QVariantMap metrics;
};

struct AnkleEvaluationReport
{
    QString caseId;
    double translationErrorMm = 0.0;
    double rotationErrorDeg = 0.0;
    bool allowNavigation = false;
    double confidenceScore = 0.0;
    QStringList gateReasons;
    bool calibrated = false;
    double calibrationAccuracyMm = 0.0;
    QVariantMap metrics;
};

struct AnkleEvaluationSnapshot
{
    QString caseId;
    bool hasRegistration = false;
    bool hasNavigationRun = false;
    bool hasEvaluationReport = false;
    QString registrationMode;
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    QString navigationMode;
    double navigationConfidenceScore = 0.0;
    double translationErrorMm = 0.0;
    double rotationErrorDeg = 0.0;
    bool allowNavigation = false;
    double evaluationConfidenceScore = 0.0;
    QStringList gateReasons;
    bool calibrated = false;
    double calibrationAccuracyMm = 0.0;
    QVariantMap registrationMetrics;
    QVariantMap navigationMetrics;
    QVariantMap evaluationMetrics;
};

struct AnkleInnovationSummaryRow
{
    QString caseId;
    QString innovationId;
    QString strategyId;
    InnovationPerturbationProfile perturbation;
    QVariantMap metrics;
};

struct AnkleInnovationBatchDefinition
{
    QStringList caseIds;
    QList<InnovationPerturbationProfile> perturbations;
    int repeatCount = 1;
};
