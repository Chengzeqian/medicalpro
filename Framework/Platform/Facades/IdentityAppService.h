#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformFacadePorts.h"

class FRAMEWORK_EXPORT IdentityAppService
{
public:
    explicit IdentityAppService(IIdentityFacadePort* port);
    UserInfo authenticate(const QString& username, const QString& password);
    bool logoutCurrentUser();
    QString currentUserName() const;
    bool hasActiveSession() const;
    QList<UserInfo> listDoctors() const;
    QList<PatientItem> listPatients() const;
    PatientItem patientById(int patientId) const;
    QList<SurgeryItem> listSurgeries() const;

private:
    IIdentityFacadePort* m_port;
};
