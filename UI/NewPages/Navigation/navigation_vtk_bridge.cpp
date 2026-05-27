#include "UI/NewPages/Navigation/navigation_vtk_bridge.h"

#include "Framework/VTK/embedded_vtk_view_host.h"
#include "Plugins/FourViewDisplay/FourViewDisplayService.h"
#include "Plugins/PointRegistration/PointRegistrationService.h"
#include "UI/Widgets/Navigation3DViewWidget.h"

#include <QWidget>

NavigationVtkBridge::NavigationVtkBridge(EmbeddedVtkViewHost* planningHost,
                                         EmbeddedVtkViewHost* navigationHost,
                                         EmbeddedVtkViewHost* registrationHost,
                                         FourViewServiceProvider fourViewServiceProvider,
                                         RegistrationServiceProvider registrationServiceProvider)
    : m_planningHost(planningHost)
    , m_navigationHost(navigationHost)
    , m_registrationHost(registrationHost)
    , m_fourViewServiceProvider(std::move(fourViewServiceProvider))
    , m_registrationServiceProvider(std::move(registrationServiceProvider))
{
}

QWidget* NavigationVtkBridge::ensureFourViewWidget(QWidget* parent)
{
    if (m_fourViewWidget) {
        return m_fourViewWidget;
    }

    auto* fourViewService = m_fourViewServiceProvider ? m_fourViewServiceProvider() : nullptr;
    if (!fourViewService) {
        return nullptr;
    }

    m_fourViewWidget = fourViewService->createFourViewVTKWidget(parent);
    return m_fourViewWidget;
}

QWidget* NavigationVtkBridge::showFourViewInPlanning(QWidget* parent)
{
    auto* widget = ensureFourViewWidget(parent);
    if (!widget) {
        return nullptr;
    }

    if (m_navigationHost) {
        m_navigationHost->detach();
    }
    if (m_planningHost) {
        m_planningHost->attach(widget);
    }

    return widget;
}

QWidget* NavigationVtkBridge::showFourViewInNavigation(QWidget* parent)
{
    auto* widget = ensureFourViewWidget(parent);
    if (!widget) {
        return nullptr;
    }

    return showNavigationContent(widget);
}

void NavigationVtkBridge::setNavigationViewWidget(Navigation3DViewWidget* widget)
{
    m_navigationViewWidget = widget;
    if (!m_navigationViewWidget) {
        return;
    }

    if (m_hasTargetRegionDefinition && m_targetRegionDefinition.available) {
        m_navigationViewWidget->setTargetRegionMarker(
            m_targetRegionDefinition.centerPatient,
            m_targetRegionDefinition.radiusMm);
    }
    m_navigationViewWidget->setTargetRegionRiskTone(m_targetRegionRiskTone);
}

QWidget* NavigationVtkBridge::showSingleNavigationSpace(QWidget* widget)
{
    return showNavigationContent(widget);
}

QWidget* NavigationVtkBridge::showNavigationContent(QWidget* widget)
{
    if (!widget) {
        return nullptr;
    }

    m_navigationViewWidget = qobject_cast<Navigation3DViewWidget*>(widget);

    if (m_planningHost) {
        m_planningHost->detach();
    }
    if (m_navigationHost) {
        m_navigationHost->attach(widget);
    }

    if (m_navigationViewWidget) {
        if (m_hasTargetRegionDefinition && m_targetRegionDefinition.available) {
            m_navigationViewWidget->setTargetRegionMarker(
                m_targetRegionDefinition.centerPatient,
                m_targetRegionDefinition.radiusMm);
        }
        m_navigationViewWidget->setTargetRegionRiskTone(m_targetRegionRiskTone);
    }

    return widget;
}

bool NavigationVtkBridge::loadBoneModels(const QStringList& boneModelPaths)
{
    m_boneModelPaths = boneModelPaths;
    if (!m_navigationViewWidget || boneModelPaths.isEmpty()) {
        return false;
    }

    return m_navigationViewWidget->loadBoneModel(boneModelPaths.first());
}

bool NavigationVtkBridge::loadInstrumentModel(const QString& toolId, const QString& modelPath)
{
    m_activeToolId = toolId;
    m_activeToolModelPath = modelPath;
    return !toolId.isEmpty();
}

void NavigationVtkBridge::updateInstrumentPose(const QString& toolId, const QMatrix4x4& vtkToolTransform)
{
    if (!m_navigationViewWidget || toolId.isEmpty()) {
        return;
    }

    m_activeToolId = toolId;
    m_navigationViewWidget->updateProbePosition(vtkToolTransform.column(3).toVector3D());
}

void NavigationVtkBridge::setInstrumentVisible(const QString& toolId, bool visible)
{
    if (!m_navigationViewWidget) {
        return;
    }

    if (!toolId.isEmpty()) {
        m_activeToolId = toolId;
    }
    m_navigationViewWidget->setProbeVisible(visible);
}

QWidget* NavigationVtkBridge::ensureRegistrationWidget(QWidget* parent)
{
    if (!m_registrationWidget) {
        auto* registrationService = m_registrationServiceProvider ? m_registrationServiceProvider() : nullptr;
        if (!registrationService) {
            return nullptr;
        }

        m_registrationWidget = registrationService->createVTKWidget(parent);
    }

    if (m_registrationWidget && m_registrationHost) {
        m_registrationHost->attach(m_registrationWidget);
    }

    return m_registrationWidget;
}

void NavigationVtkBridge::detachFourView()
{
    if (m_planningHost) {
        m_planningHost->detach();
    }
    if (m_navigationHost && m_navigationHost->widget() == m_fourViewWidget) {
        m_navigationHost->detach();
    }
}

void NavigationVtkBridge::detachNavigationContent()
{
    if (m_navigationHost) {
        m_navigationHost->detach();
    }
}

void NavigationVtkBridge::detachRegistration()
{
    if (m_registrationHost) {
        m_registrationHost->detach();
    }
}

void NavigationVtkBridge::detachAll()
{
    detachFourView();
    detachNavigationContent();
    detachRegistration();
}

void NavigationVtkBridge::pauseFourView() const
{
    auto* fourViewService = m_fourViewServiceProvider ? m_fourViewServiceProvider() : nullptr;
    if (fourViewService) {
        fourViewService->pauseRendering();
    }
}

void NavigationVtkBridge::resumeFourView() const
{
    auto* fourViewService = m_fourViewServiceProvider ? m_fourViewServiceProvider() : nullptr;
    if (fourViewService) {
        fourViewService->resumeRendering();
    }
}

QWidget* NavigationVtkBridge::fourViewWidget() const
{
    return m_fourViewWidget;
}

QWidget* NavigationVtkBridge::registrationWidget() const
{
    return m_registrationWidget;
}

void NavigationVtkBridge::setTargetRegionDefinition(const DigitalTwinTargetRegionDefinition& definition)
{
    m_targetRegionDefinition = definition;
    m_hasTargetRegionDefinition = definition.available;
    if (!m_navigationViewWidget) {
        return;
    }

    if (definition.available) {
        m_navigationViewWidget->setTargetRegionMarker(definition.centerPatient, definition.radiusMm);
        return;
    }

    m_navigationViewWidget->clearTargetRegionMarker();
}

void NavigationVtkBridge::clearTargetRegionDefinition()
{
    m_targetRegionDefinition = DigitalTwinTargetRegionDefinition();
    m_hasTargetRegionDefinition = false;
    if (m_navigationViewWidget) {
        m_navigationViewWidget->clearTargetRegionMarker();
    }
}

bool NavigationVtkBridge::hasTargetRegionDefinition() const
{
    return m_hasTargetRegionDefinition;
}

DigitalTwinTargetRegionDefinition NavigationVtkBridge::targetRegionDefinition() const
{
    return m_targetRegionDefinition;
}

void NavigationVtkBridge::setTargetRegionRiskTone(const QString& tone)
{
    m_targetRegionRiskTone = tone;
    if (m_navigationViewWidget) {
        m_navigationViewWidget->setTargetRegionRiskTone(tone);
    }
}

QString NavigationVtkBridge::targetRegionRiskTone() const
{
    return m_targetRegionRiskTone;
}
