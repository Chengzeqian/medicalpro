#include "Framework/Platform/Facades/ImagingAppService.h"

#include <QtGlobal>

ImagingAppService::ImagingAppService(IImagingFacadePort* port)
    : m_port(port)
{
    Q_ASSERT(m_port);
}

QString ImagingAppService::currentPatientName() const
{
    Q_ASSERT(m_port);
    return m_port->currentPatientName();
}

bool ImagingAppService::hasReadableStudy() const
{
    Q_ASSERT(m_port);
    return m_port->hasReadableStudy();
}

QList<DicomStudyInfo> ImagingAppService::listStudiesByPatient(int patientId) const
{
    Q_ASSERT(m_port);
    return m_port->listStudiesByPatient(patientId);
}
