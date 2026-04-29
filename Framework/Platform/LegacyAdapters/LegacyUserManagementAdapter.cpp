#include "Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h"

#include "Framework/Platform/Kernel/platform_plugin_host.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"
#include "Plugins/UserManagement/UserManagementService.h"

namespace
{
UserManagementService* userManagementService()
{
    auto* registry = PlatformPluginHost::sharedInstance().serviceRegistry();
    return registry
        ? qobject_cast<UserManagementService*>(registry->service(QStringLiteral("UserManagementService")))
        : nullptr;
}
}

UserInfo LegacyUserManagementAdapter::authenticate(const QString& username, const QString& password)
{
    auto* service = userManagementService();
    if (!service) return {};
    return service->loginUser(username, password);
}

bool LegacyUserManagementAdapter::logoutCurrentUser()
{
    auto* service = userManagementService();
    if (!service) return false;

    const auto user = service->getCurrentUser();
    if (!user.isValid()) return false;
    return service->logoutUser(user.id);
}

QString LegacyUserManagementAdapter::currentUserName() const
{
    auto* service = userManagementService();
    if (!service) return {};

    const auto user = service->getCurrentUser();
    if (!user.isValid()) return {};
    return user.username;
}

bool LegacyUserManagementAdapter::hasActiveSession() const
{
    auto* service = userManagementService();
    if (!service) return false;
    return service->getCurrentUser().isValid();
}

QList<UserInfo> LegacyUserManagementAdapter::listDoctors() const
{
    auto* service = userManagementService();
    if (!service) return {};
    return service->getAllUsers();
}

QList<PatientItem> LegacyUserManagementAdapter::listPatients() const
{
    auto* service = userManagementService();
    if (!service) return {};
    return service->listPatients();
}

PatientItem LegacyUserManagementAdapter::patientById(int patientId) const
{
    auto* service = userManagementService();
    if (!service) return {};
    return service->getPatient(patientId);
}

QList<SurgeryItem> LegacyUserManagementAdapter::listSurgeries() const
{
    auto* service = userManagementService();
    if (!service) return {};
    return service->listSurgeryItems();
}
