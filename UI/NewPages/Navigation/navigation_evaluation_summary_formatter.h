#pragma once

#include "Framework/Navigation/ankle_navigation_types.h"

#include <QString>

struct NavigationEvaluationSummary
{
    bool hasData = false;
    QString headerText;
    QString registrationText;
    QString constraintText;
    QString trackingText;
    QString gateText;
};

NavigationEvaluationSummary buildNavigationEvaluationSummary(const AnkleEvaluationSnapshot& snapshot);
