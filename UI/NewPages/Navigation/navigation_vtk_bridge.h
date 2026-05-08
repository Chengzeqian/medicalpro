#pragma once

#include <functional>

class EmbeddedVtkViewHost;
class FourViewDisplayService;
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
    QWidget* showSingleNavigationSpace(QWidget* widget);
    QWidget* showNavigationContent(QWidget* widget);
    QWidget* ensureRegistrationWidget(QWidget* parent);
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
};
