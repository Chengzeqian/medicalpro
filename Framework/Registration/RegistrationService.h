#ifndef REGISTRATION_SERVICE_H
#define REGISTRATION_SERVICE_H

#include "FrameworkExport.h"
#include "Registration/RegistrationTypes.h"

#include <QObject>

class FRAMEWORK_EXPORT RegistrationService : public QObject
{
    Q_OBJECT

public:
    explicit RegistrationService(QObject* parent = nullptr) : QObject(parent) {}
    ~RegistrationService() override = default;

    virtual RegistrationResult computeLandmarkTransform(const RegistrationRequest& request) = 0;
    virtual RegistrationProcessResult runImageRegistration(const ImageRegistrationRequest& request) = 0;

signals:
    void registrationFinished(const RegistrationResult& result);
    void registrationFailed(const QString& reason);
};

// Define service interface ID for CTK service registration
#define RegistrationService_iid "org.medicalpro.RegistrationService"
Q_DECLARE_INTERFACE(RegistrationService, RegistrationService_iid)

#endif // REGISTRATION_SERVICE_H
