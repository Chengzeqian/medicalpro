#pragma once

#include "Framework/FrameworkExport.h"
#include "Framework/Platform/Contracts/PlatformFacadePorts.h"

class FRAMEWORK_EXPORT LegacyImagingAdapter : public IImagingFacadePort
{
public:
    void setCurrentPatientId(int patientId);
    QString currentPatientName() const override;
    bool hasReadableStudy() const override;
    QList<DicomStudyInfo> listStudiesByPatient(int patientId) const override;

private:
    int m_currentPatientId = -1;
};
