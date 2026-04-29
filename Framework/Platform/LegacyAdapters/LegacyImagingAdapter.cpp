#include "Framework/Platform/LegacyAdapters/LegacyImagingAdapter.h"

#include "Framework/Platform/Kernel/platform_plugin_host.h"
#include "Framework/Platform/Kernel/platform_service_registry.h"
#include "Plugins/DicomViewer/DicomViewerService.h"

namespace
{
DicomViewerService* dicomViewerService()
{
    auto* registry = PlatformPluginHost::sharedInstance().serviceRegistry();
    return registry
        ? qobject_cast<DicomViewerService*>(registry->service(QStringLiteral("DicomViewerService")))
        : nullptr;
}
}

void LegacyImagingAdapter::setCurrentPatientId(int patientId)
{
    m_currentPatientId = patientId;
}

QString LegacyImagingAdapter::currentPatientName() const
{
    if (m_currentPatientId < 0) return {};

    auto* service = dicomViewerService();
    if (!service) return {};

    const auto patient = service->getDicomPatient(m_currentPatientId);
    if (!patient.isValid()) return {};
    return patient.patientName;
}

bool LegacyImagingAdapter::hasReadableStudy() const
{
    if (m_currentPatientId < 0) return false;

    auto* service = dicomViewerService();
    if (!service) return false;
    return !service->listStudiesByPatient(m_currentPatientId).isEmpty();
}

QList<DicomStudyInfo> LegacyImagingAdapter::listStudiesByPatient(int patientId) const
{
    auto* service = dicomViewerService();
    if (!service) return {};
    return service->listStudiesByPatient(patientId);
}
