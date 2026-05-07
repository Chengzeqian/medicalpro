#pragma once

#include "Framework/Navigation/ankle_navigation_types.h"
#include "UI/NewPages/Navigation/navigation_workspace_types.h"

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
NavigationEvaluationSummary buildNavigationEvaluationSummary(const NavigationWorkspaceSnapshot& snapshot);
