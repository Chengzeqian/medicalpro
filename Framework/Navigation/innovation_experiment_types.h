#pragma once

#include <QString>
#include <QVariantMap>

struct InnovationPerturbationProfile
{
    QString noiseProfile;
    QString trackingProfile;
    int pointBudget = 0;
};

struct InnovationExperimentRecord
{
    QString caseId;
    QString innovationId;
    QString strategyId;
    InnovationPerturbationProfile perturbation;
    int runIndex = 0;
    QVariantMap metrics;
};
