#include "UI/MainInterfaceFactory.h"

#include "Framework/Navigation/real_case_workspace_seed_coordinator.h"
#include "Framework/Platform/Bootstrap/PlatformStateStoreHandoff.h"
#include "UI/MainInterfaceWidget.h"

#include <QtGlobal>

std::unique_ptr<MainInterfaceWidget> createMainInterface(
    INavigationFacadePort* navigationPort,
    const PlatformStateStore* bootstrapStateStore,
    const RealCaseWorkspaceSeed& realCaseWorkspaceSeed,
    QWidget* parent)
{
    Q_ASSERT(navigationPort);

    auto mainInterface = std::make_unique<MainInterfaceWidget>(navigationPort, realCaseWorkspaceSeed, parent);
    copyPlatformStateStore(mainInterface->platformStateStore(), bootstrapStateStore);
    return mainInterface;
}
