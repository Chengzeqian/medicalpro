#pragma once

#include "Framework/Navigation/ankle_navigation_types.h"

#include <functional>
#include <QMatrix4x4>
#include <QStringList>

class EmbeddedVtkViewHost;
class FourViewDisplayService;
class Navigation3DViewWidget;
class PointRegistrationService;
class QWidget;

class NavigationVtkBridge
{
public:
    using FourViewServiceProvider = std::function<FourViewDisplayService*()>;
    using RegistrationServiceProvider = std::function<PointRegistrationService*()>;

    NavigationVtkBridge(EmbeddedVtkViewHost* planningHost,
                        EmbeddedVtkViewHost* navigationHost,
                        EmbeddedVtkViewHost* registrationHost,
                        FourViewServiceProvider fourViewServiceProvider,
                        RegistrationServiceProvider registrationServiceProvider);

    QWidget* ensureFourViewWidget(QWidget* parent);
    QWidget* showFourViewInPlanning(QWidget* parent);
    QWidget* showFourViewInNavigation(QWidget* parent);
    void setNavigationViewWidget(Navigation3DViewWidget* widget);
    QWidget* showSingleNavigationSpace(QWidget* widget);
    QWidget* showNavigationContent(QWidget* widget);
    QWidget* ensureRegistrationWidget(QWidget* parent);
    bool loadBoneModels(const QStringList& boneModelPaths);
    bool loadInstrumentModel(const QString& toolId, const QString& modelPath);
    void updateInstrumentPose(const QString& toolId, const QMatrix4x4& vtkToolTransform);
    void setInstrumentVisible(const QString& toolId, bool visible);
    void setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& definition);
    void clearTargetRegionDefinition();
    bool hasTargetRegionDefinition() const;
    DigitalTwinTargetRegionDefinition targetRegionDefinition() const;
    void setTargetRegionRiskTone(const QString& tone);
    QString targetRegionRiskTone() const;
    void detachFourView();
    void detachNavigationContent();
    void detachRegistration();
    void detachAll();
    void pauseFourView() const;
    void resumeFourView() const;
    QWidget* fourViewWidget() const;
    QWidget* registrationWidget() const;

private:
    EmbeddedVtkViewHost* m_planningHost = nullptr;
    EmbeddedVtkViewHost* m_navigationHost = nullptr;
    EmbeddedVtkViewHost* m_registrationHost = nullptr;
    FourViewServiceProvider m_fourViewServiceProvider;
    RegistrationServiceProvider m_registrationServiceProvider;
    QWidget* m_fourViewWidget = nullptr;
    QWidget* m_registrationWidget = nullptr;
    Navigation3DViewWidget* m_navigationViewWidget = nullptr;
    DigitalTwinTargetRegionDefinition m_targetRegionDefinition;
    QString m_targetRegionRiskTone = QStringLiteral("ok");
    QStringList m_boneModelPaths;
    QString m_activeToolId;
    QString m_activeToolModelPath;
    bool m_hasTargetRegionDefinition = false;
};
