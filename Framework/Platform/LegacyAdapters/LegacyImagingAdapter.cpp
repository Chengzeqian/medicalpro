#include "Framework/Platform/LegacyAdapters/LegacyImagingAdapter.h"

#ifdef CTK_PLUGIN_FRAMEWORK
#include "Framework/CTKManager.h"
#include "Plugins/DicomViewer/DicomViewerService.h"
#endif

void LegacyImagingAdapter::setCurrentPatientId(int patientId)
{
    m_currentPatientId = patientId;
}

QString LegacyImagingAdapter::currentPatientName() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (m_currentPatientId < 0) return {};

    auto* service = CTKManager::instance()->getService<DicomViewerService>();
    if (!service) return {};

    const auto patient = service->getDicomPatient(m_currentPatientId);
    if (!patient.isValid()) return {};
    return patient.patientName;
#else
    return {};
#endif
}

bool LegacyImagingAdapter::hasReadableStudy() const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    if (m_currentPatientId < 0) return false;

    auto* service = CTKManager::instance()->getService<DicomViewerService>();
    if (!service) return false;
    return !service->listStudiesByPatient(m_currentPatientId).isEmpty();
#else
    return false;
#endif
}

QList<DicomStudyInfo> LegacyImagingAdapter::listStudiesByPatient(int patientId) const
{
#ifdef CTK_PLUGIN_FRAMEWORK
    auto* service = CTKManager::instance()->getService<DicomViewerService>();
    if (!service) return {};
    return service->listStudiesByPatient(patientId);
#else
    Q_UNUSED(patientId);
    return {};
#endif
}
