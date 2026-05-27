#pragma once

#include "UI/NewPages/Navigation/navigation_workflow_stage.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector3D>

struct NavigationStageGate
{
    AnkleWorkflowStage requestedStage = AnkleWorkflowStage::Preparation;
    bool allowed = false;
    QString reasonCode;
    QString reasonText;
    QString severity = QStringLiteral("warning");
    QDateTime lastComputedAt;
};

struct NavigationWorkspaceCaseContext
{
    QString caseId;
    QString patientId;
    QString patientName;
    QString surgeryId;
    AnkleWorkflowStage currentStage = AnkleWorkflowStage::Preparation;
    QDateTime lastUpdatedAt;
};

struct NavigationInstrumentGeometryState
{
    QString instrumentId;
    QString instrumentDisplayName;
    QString modelFilePath;
    QString geometryId;
    QString geometryFilePath;
    QString trackingMarkerId;
    bool geometryReady = false;
};

struct NavigationWorkspaceAssetState
{
    bool dicomReady = false;
    bool boneModelReady = false;
    QString boneModelPath;
    QStringList boundBoneAssets;
    QStringList activeBoneAssets;
    QStringList boundInstrumentIds;
    QStringList activeInstrumentIds;
    QList<NavigationInstrumentGeometryState> instrumentGeometryBindings;
    bool geometryReady = false;
    QStringList selectedBoneAssets;
    QString selectedBoneAsset;
    QString selectedInstrumentId;
    QString selectedInstrumentDisplayName;
    QString instrumentModelPath;
    QString geometryFilePath;
    QString geometryId;
    QString trackingMarkerId;
    bool instrumentServiceAvailable = false;
    bool toolVisible = false;
};

struct NavigationInstrumentCalibrationState
{
    QString instrumentId;
    QString geometryId;
    bool started = false;
    int collectedPoints = 0;
    int requiredPoints = 0;
    bool completed = false;
    double accuracy = 0.0;
    QDateTime completedAt;
};

struct NavigationWorkspaceCalibrationState
{
    bool trackingReady = false;
    bool started = false;
    int collectedPoints = 0;
    int requiredPoints = 0;
    bool completed = false;
    double tipOffset = 0.0;
    double accuracy = 0.0;
    QString statusText;
    QString geometryId;
    QDateTime completedAt;
};

struct NavigationWorkspacePreparationState
{
    QList<NavigationInstrumentCalibrationState> instrumentCalibrationStates;
    bool allRequiredInstrumentsCalibrated = false;
    QStringList blockingReasons;
};

struct NavigationWorkspacePlanningState
{
    bool hasPlanning = false;
    bool targetRegionReady = false;
    QString targetBone;
    QString targetRegion;
    QVector3D targetRegionCenter;
    double targetRegionRadiusMm = 0.0;
    QVector3D targetRegionAxis = QVector3D(0.0f, 0.0f, 1.0f);
    QStringList referenceBones;
    QStringList constraintRegions;
    QStringList recommendedPointOrder;
    bool completed = false;
    QDateTime savedAt;
};

struct NavigationPerBoneRegistrationState
{
    QString boneAssetId;
    QString boneRegionId;
    int pointCount = 0;
    bool success = false;
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    QString transformMatrix;
    QDateTime completedAt;
};

struct NavigationWorkspaceRegistrationState
{
    int pointCount = 0;
    bool success = false;
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    QString registrationMethodId;
    QString pointSelectionStrategyId;
    QString refineMethod;
    bool constraintRefineUsed = false;
    int constraintRegionCount = 0;
    double targetRegionRadiusMm = 0.0;
    double translationX = 0.0;
    double translationY = 0.0;
    double translationZ = 0.0;
    double rotationX = 0.0;
    double rotationY = 0.0;
    double rotationZ = 0.0;
    QString transformMatrix;
    QList<NavigationPerBoneRegistrationState> perBoneResults;
    bool fusedNavigationSpaceReady = false;
    QString fusedNavigationSpacePath;
    double fusedCoverageScore = 0.0;
    QStringList fusionBlockingReasons;
    QDateTime completedAt;
};

struct NavigationWorkspaceNavigationState
{
    bool trackerConnected = false;
    QString activeToolId;
    bool toolVisible = false;
    bool running = false;
    double confidence = 0.0;
    bool allowNavigation = false;
    QStringList blockReasons;
    QString latestPoseSummary;
    bool hasRunRecord = false;
    bool hasEvaluationReport = false;
    QString summaryText;
    bool exportAvailable = false;
};

struct NavigationWorkspaceEvaluationState
{
    bool hasSummary = false;
    QVariantMap errorMetrics;
    QStringList perBoneQualitySummary;
    QString navigationProcessSummary;
    QString summaryText;
    bool reportReady = false;
    QStringList exportableArtifacts;
    QDateTime lastUpdatedAt;
};

struct NavigationWorkspaceSnapshot
{
    QString caseId;
    NavigationWorkspaceCaseContext caseContext;
    NavigationWorkspaceAssetState assetState;
    NavigationWorkspaceCalibrationState calibrationState;
    NavigationWorkspacePreparationState preparationState;
    NavigationWorkspacePlanningState planningState;
    NavigationWorkspaceRegistrationState registrationState;
    NavigationWorkspaceNavigationState navigationState;
    NavigationWorkspaceEvaluationState evaluationState;
    NavigationStageGate stageGate;
    QDateTime lastRefreshedAt;
};
