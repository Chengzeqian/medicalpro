#ifndef MAININTERFACEFACTORY_H
#define MAININTERFACEFACTORY_H

#include <memory>

class INavigationFacadePort;
class MainInterfaceWidget;
class PlatformStateStore;
class QWidget;

std::unique_ptr<MainInterfaceWidget> createMainInterface(
    INavigationFacadePort* navigationPort,
    const PlatformStateStore* bootstrapStateStore,
    QWidget* parent = nullptr);

#endif // MAININTERFACEFACTORY_H
