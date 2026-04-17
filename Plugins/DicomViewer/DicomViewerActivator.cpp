#include "DicomViewerActivator.h"
#include "DicomViewerService.h"
#include "DicomViewerServiceImpl.h"

#include <ctkPluginContext.h>

#include <QDebug>

#include <stdexcept>

DicomViewerActivator::DicomViewerActivator()
    : m_serviceImpl(nullptr)
{
}

DicomViewerActivator::~DicomViewerActivator() = default;

void DicomViewerActivator::start(ctkPluginContext* context)
{
    qDebug() << "[DicomViewer] Starting DicomViewer plugin...";

    if (!context) {
        qCritical() << "[DicomViewer] Plugin context is null; cannot start.";
        return;
    }

    try {
        m_serviceImpl = new DicomViewerServiceImpl(this);
        m_serviceImpl->setPluginContext(context);

        if (!m_serviceImpl->initialize()) {
            qWarning() << "[DicomViewer] Service initialization failed";
            delete m_serviceImpl;
            m_serviceImpl = nullptr;
            throw std::runtime_error("DicomViewerService initialization failed");
        }

        ctkDictionary properties;
        properties.insert("service.description", "DICOM image viewing service");
        properties.insert("service.vendor", "MedicalPro");
        properties.insert("service.version", "1.0.0");
        properties.insert("plugin.category", "Medical");

        m_serviceRegistration = context->registerService<DicomViewerService>(m_serviceImpl, properties);

        if (m_serviceRegistration) {
            qDebug() << "[DicomViewer] DicomViewerService registered successfully";
        } else {
            qWarning() << "[DicomViewer] DicomViewerService registration failed";
            delete m_serviceImpl;
            m_serviceImpl = nullptr;
            throw std::runtime_error("DicomViewerService registration failed");
        }

        qDebug() << "[DicomViewer] DicomViewer plugin started";

    } catch (const std::exception& e) {
        qCritical() << "[DicomViewer] Plugin start exception:" << e.what();
        if (m_serviceImpl) {
            delete m_serviceImpl;
            m_serviceImpl = nullptr;
        }
    } catch (...) {
        qCritical() << "[DicomViewer] Plugin start unknown exception";
        if (m_serviceImpl) {
            delete m_serviceImpl;
            m_serviceImpl = nullptr;
        }
    }
}

void DicomViewerActivator::stop(ctkPluginContext* context)
{
    Q_UNUSED(context)

    qDebug() << "[DicomViewer] Stopping DicomViewer plugin...";

    try {
        if (m_serviceRegistration) {
            m_serviceRegistration.unregister();
            m_serviceRegistration = ctkServiceRegistration();
            qDebug() << "[DicomViewer] Service unregistered";
        }

        if (m_serviceImpl) {
            m_serviceImpl->shutdown();
            delete m_serviceImpl;
            m_serviceImpl = nullptr;
            qDebug() << "[DicomViewer] Service shutdown";
        }

        qDebug() << "[DicomViewer] DicomViewer plugin stopped";

    } catch (const std::exception& e) {
        qCritical() << "[DicomViewer] Plugin stop exception:" << e.what();
    } catch (...) {
        qCritical() << "[DicomViewer] Plugin stop unknown exception";
    }
}
