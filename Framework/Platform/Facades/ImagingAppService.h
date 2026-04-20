#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformFacadePorts.h"

class FRAMEWORK_EXPORT ImagingAppService
{
public:
    explicit ImagingAppService(IImagingFacadePort* port);
    QString currentPatientName() const;
    bool hasReadableStudy() const;
    QList<DicomStudyInfo> listStudiesByPatient(int patientId) const;

private:
    IImagingFacadePort* m_port;
};
