#pragma once

#include "UI/NewPages/Navigation/navigation_workflow_stage.h"

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>

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

struct NavigationWorkspaceAssetState
{
    bool dicomReady = false;
    bool boneModelReady = false;
    QString boneModelPath;
    QStringList selectedBoneAssets;
    QString selectedBoneAsset;
    QString selectedInstrumentId;
    QString geometryFilePath;
    QString geometryId;
    bool instrumentServiceAvailable = false;
    bool toolVisible = false;
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

struct NavigationWorkspacePlanningState
{
    bool hasPlanning = false;
    bool targetRegionReady = false;
    QStringList referenceBones;
    QStringList constraintRegions;
    QStringList recommendedPointOrder;
    QDateTime savedAt;
};

struct NavigationWorkspaceRegistrationState
{
    int pointCount = 0;
    bool success = false;
    double fre = 0.0;
    double targetTre = 0.0;
    double coverageScore = 0.0;
    double translationX = 0.0;
    double translationY = 0.0;
    double translationZ = 0.0;
    double rotationX = 0.0;
    double rotationY = 0.0;
    double rotationZ = 0.0;
    QString transformMatrix;
    QDateTime completedAt;
};

struct NavigationWorkspaceNavigationState
{
    bool trackerConnected = false;
    bool toolVisible = false;
    bool running = false;
    double confidence = 0.0;
    bool allowNavigation = false;
    QStringList blockReasons;
    bool hasRunRecord = false;
    bool hasEvaluationReport = false;
    QString summaryText;
    bool exportAvailable = false;
};

struct NavigationWorkspaceEvaluationState
{
    bool hasSummary = false;
    QString summaryText;
    QStringList exportableArtifacts;
    QDateTime lastUpdatedAt;
};

struct NavigationWorkspaceSnapshot
{
    QString caseId;
    NavigationWorkspaceCaseContext caseContext;
    NavigationWorkspaceAssetState assetState;
    NavigationWorkspaceCalibrationState calibrationState;
    NavigationWorkspacePlanningState planningState;
    NavigationWorkspaceRegistrationState registrationState;
    NavigationWorkspaceNavigationState navigationState;
    NavigationWorkspaceEvaluationState evaluationState;
    NavigationStageGate stageGate;
    QDateTime lastRefreshedAt;
};
