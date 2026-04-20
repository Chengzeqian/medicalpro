#pragma once

#include "Framework/FrameworkExport.h"
#include "Plugins/DicomViewer/DicomDataStructures.h"
#include "Plugins/UserManagement/UserDataStructures.h"

#include <QString>

class FRAMEWORK_EXPORT IIdentityFacadePort
{
public:
    virtual ~IIdentityFacadePort() = default;
    virtual UserInfo authenticate(const QString& username, const QString& password) = 0;
    virtual bool logoutCurrentUser() = 0;
    virtual QString currentUserName() const = 0;
    virtual bool hasActiveSession() const = 0;
    virtual QList<UserInfo> listDoctors() const = 0;
    virtual QList<PatientItem> listPatients() const = 0;
    virtual PatientItem patientById(int patientId) const = 0;
    virtual QList<SurgeryItem> listSurgeries() const = 0;
};

class FRAMEWORK_EXPORT IImagingFacadePort
{
public:
    virtual ~IImagingFacadePort() = default;
    virtual QString currentPatientName() const = 0;
    virtual bool hasReadableStudy() const = 0;
    virtual QList<DicomStudyInfo> listStudiesByPatient(int patientId) const = 0;
};

class FRAMEWORK_EXPORT INavigationFacadePort
{
public:
    virtual ~INavigationFacadePort() = default;
    virtual bool ensureReady(const QString& pluginId) = 0;
};
