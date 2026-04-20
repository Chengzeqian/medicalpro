#include "Framework/Platform/LegacyAdapters/LegacyUserManagementAdapter.h"

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Framework/CTKManager.h"
#include "Plugins/UserManagement/UserManagementService.h"
#endif

UserInfo LegacyUserManagementAdapter::authenticate(const QString& username, const QString& password)
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* service = CTKManager::instance()->getService<UserManagementService>();
    if (!service) return {};
    return service->loginUser(username, password);
#else
    Q_UNUSED(username);
    Q_UNUSED(password);
    return {};
#endif
}

bool LegacyUserManagementAdapter::logoutCurrentUser()
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* service = CTKManager::instance()->getService<UserManagementService>();
    if (!service) return false;

    const auto user = service->getCurrentUser();
    if (!user.isValid()) return false;
    return service->logoutUser(user.id);
#else
    return false;
#endif
}

QString LegacyUserManagementAdapter::currentUserName() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* service = CTKManager::instance()->getService<UserManagementService>();
    if (!service) return {};

    const auto user = service->getCurrentUser();
    if (!user.isValid()) return {};
    return user.username;
#else
    return {};
#endif
}

bool LegacyUserManagementAdapter::hasActiveSession() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* service = CTKManager::instance()->getService<UserManagementService>();
    if (!service) return false;
    return service->getCurrentUser().isValid();
#else
    return false;
#endif
}

QList<UserInfo> LegacyUserManagementAdapter::listDoctors() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* service = CTKManager::instance()->getService<UserManagementService>();
    if (!service) return {};
    return service->getAllUsers();
#else
    return {};
#endif
}

QList<PatientItem> LegacyUserManagementAdapter::listPatients() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* service = CTKManager::instance()->getService<UserManagementService>();
    if (!service) return {};
    return service->listPatients();
#else
    return {};
#endif
}

PatientItem LegacyUserManagementAdapter::patientById(int patientId) const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* service = CTKManager::instance()->getService<UserManagementService>();
    if (!service) return {};
    return service->getPatient(patientId);
#else
    Q_UNUSED(patientId);
    return {};
#endif
}

QList<SurgeryItem> LegacyUserManagementAdapter::listSurgeries() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* service = CTKManager::instance()->getService<UserManagementService>();
    if (!service) return {};
    return service->listSurgeryItems();
#else
    return {};
#endif
}
