#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformFacadePorts.h"

class FRAMEWORK_EXPORT LegacyUserManagementAdapter : public IIdentityFacadePort
{
public:
    UserInfo authenticate(const QString& username, const QString& password) override;
    bool logoutCurrentUser() override;
    QString currentUserName() const override;
    bool hasActiveSession() const override;
    QList<UserInfo> listDoctors() const override;
    QList<PatientItem> listPatients() const override;
    PatientItem patientById(int patientId) const override;
    QList<SurgeryItem> listSurgeries() const override;
};
