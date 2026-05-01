#include "UI/NewPages/Navigation/navigation_vtk_bridge.h"

#include "Framework/VTK/embedded_vtk_view_host.h"
#include "Plugins/FourViewDisplay/FourViewDisplayService.h"
#include "Plugins/PointRegistration/PointRegistrationService.h"

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

QWidget* NavigationVtkBridge::showNavigationContent(QWidget* widget)
{
    if (!widget) {
        return nullptr;
    }

    if (m_planningHost) {
        m_planningHost->detach();
    }
    if (m_navigationHost) {
        m_navigationHost->attach(widget);
    }

    return widget;
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
