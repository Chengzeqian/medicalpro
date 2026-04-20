#include "Framework/Platform/Facades/IdentityAppService.h"

#include <QtGlobal>

IdentityAppService::IdentityAppService(IIdentityFacadePort* port)
    : m_port(port)
{
    Q_ASSERT(m_port);
}

UserInfo IdentityAppService::authenticate(const QString& username, const QString& password)
{
    Q_ASSERT(m_port);
    return m_port->authenticate(username, password);
}

bool IdentityAppService::logoutCurrentUser()
{
    Q_ASSERT(m_port);
    return m_port->logoutCurrentUser();
}

QString IdentityAppService::currentUserName() const
{
    Q_ASSERT(m_port);
    return m_port->currentUserName();
}

bool IdentityAppService::hasActiveSession() const
{
    Q_ASSERT(m_port);
    return m_port->hasActiveSession();
}

QList<UserInfo> IdentityAppService::listDoctors() const
{
    Q_ASSERT(m_port);
    return m_port->listDoctors();
}

QList<PatientItem> IdentityAppService::listPatients() const
{
    Q_ASSERT(m_port);
    return m_port->listPatients();
}

PatientItem IdentityAppService::patientById(int patientId) const
{
    Q_ASSERT(m_port);
    return m_port->patientById(patientId);
}

QList<SurgeryItem> IdentityAppService::listSurgeries() const
{
    Q_ASSERT(m_port);
    return m_port->listSurgeries();
}
