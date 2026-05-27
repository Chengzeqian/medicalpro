#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Navigation/ankle_navigation_types.h"
#include "Framework/Navigation/navigation_transform_graph.h"
#include "Framework/Navigation/navigation_confidence_evaluator.h"
#include "Plugins/PointRegistration/PointRegistrationDataStructures.h"

FRAMEWORK_EXPORT TargetRegionNavigationStatus buildTargetRegionNavigationStatus(
    const DigitalTwinTargetRegionDefinition& targetRegion,
    const NavigationTransformResult& transformResult);

FRAMEWORK_EXPORT DigitalTwinRiskReport buildDigitalTwinRiskReport(
    const PointRegistrationResult& registrationResult,
    const QVariantMap& trackingQuality,
    const NavigationConfidenceResult& confidenceResult,
    const TargetRegionNavigationStatus& targetStatus);

FRAMEWORK_EXPORT DigitalTwinState buildDigitalTwinState(
    const PointRegistrationResult& registrationResult,
    const QVariantMap& trackingQuality,
    const NavigationConfidenceResult& confidenceResult,
    const TargetRegionNavigationStatus& targetStatus,
    const DigitalTwinRiskReport& riskReport);
