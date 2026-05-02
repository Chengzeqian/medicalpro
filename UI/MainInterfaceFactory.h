#ifndef MAININTERFACEFACTORY_H
#define MAININTERFACEFACTORY_H

#include <memory>

struct RealCaseWorkspaceSeed;

class INavigationFacadePort;
class MainInterfaceWidget;
class PlatformStateStore;
class QWidget;

std::unique_ptr<MainInterfaceWidget> createMainInterface(
    INavigationFacadePort* navigationPort,
    const PlatformStateStore* bootstrapStateStore,
    const RealCaseWorkspaceSeed& realCaseWorkspaceSeed,
    QWidget* parent = nullptr);

#endif // MAININTERFACEFACTORY_H
