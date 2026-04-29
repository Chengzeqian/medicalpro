#pragma once

#include <QMap>
#include <QList>
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
};

struct AnklePlanningData
{
    QString caseId;
    QStringList primaryBones;
    QMap<QString, QVector3D> referenceLandmarks;
    QStringList recommendedPointOrder;
    QVector3D targetTranslation;
    QQuaternion targetOrientation;
    QString planningFileVersion;
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
};

struct AnkleEvaluationReport
{
    QString caseId;
    double translationErrorMm = 0.0;
    double rotationErrorDeg = 0.0;
    bool allowNavigation = false;
};
