#include <QtTest/QtTest>

#include "Framework/Platform/Facades/IdentityAppService.h"
#include "Framework/Platform/Facades/ImagingAppService.h"
#include "Framework/Platform/Facades/NavigationAppService.h"
#include "Framework/Platform/LegacyAdapters/LegacyNavigationAdapter.h"

class FakeIdentityPort : public IIdentityFacadePort
{
public:
    UserInfo authenticate(const QString& username, const QString& password) override
    {
        lastAuthenticateUsername = username;
        lastAuthenticatePassword = password;
        return authenticateResult;
    }

    bool logoutCurrentUser() override
    {
        logoutRequested = true;
        return logoutResult;
    }

    QString currentUserName() const override { return QStringLiteral("admin"); }
    bool hasActiveSession() const override { return true; }
    QList<UserInfo> listDoctors() const override { return doctors; }
    QList<PatientItem> listPatients() const override { return patients; }
    PatientItem patientById(int patientId) const override
    {
        lastPatientId = patientId;
        for (const auto& patient : patients) {
            if (patient.id == patientId) return patient;
        }
        return {};
    }
    QList<SurgeryItem> listSurgeries() const override { return surgeries; }

    QString lastAuthenticateUsername;
    QString lastAuthenticatePassword;
    UserInfo authenticateResult;
    bool logoutRequested = false;
    bool logoutResult = false;
    mutable int lastPatientId = -1;
    QList<UserInfo> doctors;
    QList<PatientItem> patients;
    QList<SurgeryItem> surgeries;
};

class FakeImagingPort : public IImagingFacadePort
{
public:
    QString currentPatientName() const override { return QStringLiteral("patient-a"); }
    bool hasReadableStudy() const override { return true; }
    QList<DicomStudyInfo> listStudiesByPatient(int patientId) const override
    {
        requestedPatientId = patientId;
        return studiesByPatient.value(patientId);
    }

    mutable int requestedPatientId = -1;
    QHash<int, QList<DicomStudyInfo>> studiesByPatient;
};

class FakeNavigationPort : public INavigationFacadePort
{
public:
    bool ensureReady(const QString& pluginId) override
    {
        lastPluginId = pluginId;
        return true;
    }

    QString lastPluginId;
};

class PlatformFacadesTest : public QObject
{
    Q_OBJECT

private slots:
    void identityFacade_authenticates_with_port();
    void identityFacade_logs_out_current_user_with_port();
    void identityFacade_reads_current_session_from_port();
    void identityFacade_lists_doctors_from_port();
    void identityFacade_lists_patients_from_port();
    void identityFacade_reads_patient_by_id_from_port();
    void identityFacade_lists_surgeries_from_port();
    void imagingFacade_lists_studies_by_patient_from_port();
    void navigationFacade_forwards_ensure_ready_to_port();
    void navigationLegacyAdapter_delegates_to_governed_activation_service();
    void navigationLegacyAdapter_returns_false_when_activation_service_is_missing();
};

void PlatformFacadesTest::identityFacade_authenticates_with_port()
{
    FakeIdentityPort port;
    UserInfo user;
    user.id = 3;
    user.username = QStringLiteral("doctor.li");
    user.realName = QStringLiteral("李医生");
    port.authenticateResult = user;

    IdentityAppService service(&port);
    const auto result = service.authenticate(QStringLiteral("doctor.li"), QStringLiteral("secret"));

    QCOMPARE(port.lastAuthenticateUsername, QStringLiteral("doctor.li"));
    QCOMPARE(port.lastAuthenticatePassword, QStringLiteral("secret"));
    QCOMPARE(result.id, 3);
    QCOMPARE(result.username, QStringLiteral("doctor.li"));
}

void PlatformFacadesTest::identityFacade_logs_out_current_user_with_port()
{
    FakeIdentityPort port;
    port.logoutResult = true;

    IdentityAppService service(&port);

    QVERIFY(service.logoutCurrentUser());
    QVERIFY(port.logoutRequested);
}

void PlatformFacadesTest::identityFacade_reads_current_session_from_port()
{
    FakeIdentityPort port;
    IdentityAppService service(&port);
    QCOMPARE(service.currentUserName(), QStringLiteral("admin"));
    QVERIFY(service.hasActiveSession());
}

void PlatformFacadesTest::identityFacade_lists_doctors_from_port()
{
    FakeIdentityPort port;
    UserInfo doctor;
    doctor.id = 7;
    doctor.username = QStringLiteral("doctor.li");
    doctor.department = QStringLiteral("骨科");
    doctor.jobTitle = QStringLiteral("主任医师");
    doctor.realName = QStringLiteral("李主任");
    port.doctors.append(doctor);

    IdentityAppService service(&port);
    const auto doctors = service.listDoctors();

    QCOMPARE(doctors.size(), 1);
    QCOMPARE(doctors.front().id, 7);
    QCOMPARE(doctors.front().username, QStringLiteral("doctor.li"));
}

void PlatformFacadesTest::identityFacade_lists_patients_from_port()
{
    FakeIdentityPort port;
    PatientItem patient;
    patient.id = 12;
    patient.name = QStringLiteral("患者甲");
    patient.gender = QStringLiteral("女");
    patient.age = 56;
    patient.description = QStringLiteral("踝关节炎");
    port.patients.append(patient);

    IdentityAppService service(&port);
    const auto patients = service.listPatients();

    QCOMPARE(patients.size(), 1);
    QCOMPARE(patients.front().id, 12);
    QCOMPARE(patients.front().name, QStringLiteral("患者甲"));
}

void PlatformFacadesTest::identityFacade_reads_patient_by_id_from_port()
{
    FakeIdentityPort port;
    PatientItem patient;
    patient.id = 24;
    patient.name = QStringLiteral("患者乙");
    patient.description = QStringLiteral("距骨骨折");
    port.patients.append(patient);

    IdentityAppService service(&port);
    const auto result = service.patientById(24);

    QCOMPARE(port.lastPatientId, 24);
    QCOMPARE(result.id, 24);
    QCOMPARE(result.name, QStringLiteral("患者乙"));
}

void PlatformFacadesTest::identityFacade_lists_surgeries_from_port()
{
    FakeIdentityPort port;
    SurgeryItem surgery;
    surgery.id = 5;
    surgery.name = QStringLiteral("踝关节置换");
    surgery.isActive = true;
    port.surgeries.append(surgery);

    IdentityAppService service(&port);
    const auto surgeries = service.listSurgeries();

    QCOMPARE(surgeries.size(), 1);
    QCOMPARE(surgeries.front().id, 5);
    QCOMPARE(surgeries.front().name, QStringLiteral("踝关节置换"));
}

void PlatformFacadesTest::imagingFacade_lists_studies_by_patient_from_port()
{
    FakeImagingPort port;
    DicomStudyInfo study;
    study.id = 3;
    study.patientId = 24;
    study.studyUID = QStringLiteral("study-24-1");
    study.studyDescription = QStringLiteral("术前 CT");
    port.studiesByPatient.insert(24, { study });

    ImagingAppService service(&port);
    const auto studies = service.listStudiesByPatient(24);

    QCOMPARE(port.requestedPatientId, 24);
    QCOMPARE(studies.size(), 1);
    QCOMPARE(studies.front().id, 3);
    QCOMPARE(studies.front().studyDescription, QStringLiteral("术前 CT"));
}

void PlatformFacadesTest::navigationFacade_forwards_ensure_ready_to_port()
{
    FakeNavigationPort port;
    NavigationAppService service(&port);
    QVERIFY(service.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QCOMPARE(port.lastPluginId, QStringLiteral("org.medicalpro.registration_core"));
}

void PlatformFacadesTest::navigationLegacyAdapter_delegates_to_governed_activation_service()
{
    QString ensuredPluginId;
    LegacyNavigationAdapter adapter(
        [&ensuredPluginId](const QString& pluginId) {
            ensuredPluginId = pluginId;
            return true;
        });
    NavigationAppService service(&adapter);

    QVERIFY(service.ensureReady(QStringLiteral("org.medicalpro.registration_core")));
    QCOMPARE(ensuredPluginId, QStringLiteral("org.medicalpro.registration_core"));
}

void PlatformFacadesTest::navigationLegacyAdapter_returns_false_when_activation_service_is_missing()
{
    LegacyNavigationAdapter adapter;
    NavigationAppService service(&adapter);

    QVERIFY(!service.ensureReady(QStringLiteral("org.medicalpro.optical_tracking")));
}

QTEST_APPLESS_MAIN(PlatformFacadesTest)
#include "PlatformFacadesTest.moc"
