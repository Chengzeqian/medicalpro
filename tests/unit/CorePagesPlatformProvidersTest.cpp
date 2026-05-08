#include <QtTest/QtTest>

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>

#include "Framework/Navigation/ankle_case_workspace_repository.h"
#include "Framework/Platform/Facades/IdentityAppService.h"
#include "Framework/Platform/Facades/ImagingAppService.h"
#include "Framework/Platform/Facades/NavigationAppService.h"
#include "UI/NewPages/DashboardPage.h"
#include "UI/NewPages/ManagementPage.h"
#include "UI/NewPages/ModuleSelectionPage.h"
#include "UI/NewPages/SystemSettingsPage.h"

class FakeIdentityProviderPort : public IIdentityFacadePort
{
public:
    UserInfo authenticate(const QString& username, const QString& password) override
    {
        lastAuthenticatedUsername = username;
        lastAuthenticatedPassword = password;
        return authenticatedUser;
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
        for (const auto& patient : patients) {
            if (patient.id == patientId) return patient;
        }
        return {};
    }
    QList<SurgeryItem> listSurgeries() const override { return surgeries; }

    QString lastAuthenticatedUsername;
    QString lastAuthenticatedPassword;
    UserInfo authenticatedUser;
    bool logoutRequested = false;
    bool logoutResult = true;
    QList<UserInfo> doctors;
    QList<PatientItem> patients;
    QList<SurgeryItem> surgeries;
};

class FakeImagingProviderPort : public IImagingFacadePort
{
public:
    QString currentPatientName() const override { return QStringLiteral("患者甲"); }
    bool hasReadableStudy() const override { return true; }
    QList<DicomStudyInfo> listStudiesByPatient(int patientId) const override
    {
        requestedPatientId = patientId;
        return studies.value(patientId);
    }

    mutable int requestedPatientId = -1;
    QHash<int, QList<DicomStudyInfo>> studies;
};

class FakeNavigationProviderPort : public INavigationFacadePort
{
public:
    bool ensureReady(const QString& pluginId) override
    {
        lastPluginId = pluginId;
        return ready;
    }

    bool ready = true;
    QString lastPluginId;
};

class CorePagesPlatformProvidersTest : public QObject
{
    Q_OBJECT

private slots:
    void moduleSelection_uses_runtime_status_provider();
    void systemSettings_uses_runtime_status_provider();
    void managementPage_uses_identity_service_for_tables();
    void dashboardPage_uses_facade_services_for_patient_and_imaging_views();
};

void CorePagesPlatformProvidersTest::moduleSelection_uses_runtime_status_provider()
{
    int providerCallCount = 0;
    ModuleSelectionPageNew page(nullptr, [&providerCallCount]() {
        ++providerCallCount;
        ModuleSelectionPageNew::ModuleRuntimeStatus status;
        status.frameworkReady = true;
        status.workflowReady = true;
        status.readyServices = 3;
        status.totalServices = 3;
        return status;
    });

    page.onActivated();
    QVERIFY(providerCallCount > 0);
    QVERIFY(page.findChild<QLabel*>(QStringLiteral("systemValueLabel")) != nullptr);
}

void CorePagesPlatformProvidersTest::systemSettings_uses_runtime_status_provider()
{
    int providerCallCount = 0;
    SystemSettingsPageNew page(nullptr, [&providerCallCount]() {
        ++providerCallCount;
        SystemSettingsPageNew::RuntimeStatusSnapshot status;
        status.frameworkReady = true;
        status.pluginCount = 3;
        status.readyServices = 3;
        status.totalServices = 3;
        status.dataDirectoryReadable = true;
        status.dicomDirectoryReadable = true;
        return status;
    });

    page.onActivated();
    QVERIFY(providerCallCount > 0);
    QVERIFY(page.findChild<QLabel*>(QStringLiteral("systemRecommendationLabel")) != nullptr);
}

void CorePagesPlatformProvidersTest::managementPage_uses_identity_service_for_tables()
{
    FakeIdentityProviderPort port;

    UserInfo doctor;
    doctor.id = 1;
    doctor.username = QStringLiteral("doctor.zhang");
    doctor.department = QStringLiteral("骨科");
    doctor.jobTitle = QStringLiteral("主任医师");
    doctor.phone = QStringLiteral("13800000000");
    doctor.email = QStringLiteral("doctor@hospital.test");
    doctor.realName = QStringLiteral("张主任");
    port.doctors.append(doctor);

    PatientItem patient;
    patient.id = 9;
    patient.name = QStringLiteral("患者甲");
    patient.gender = QStringLiteral("男");
    patient.age = 45;
    patient.phone = QStringLiteral("13900000000");
    patient.description = QStringLiteral("踝关节炎");
    port.patients.append(patient);

    SurgeryItem surgery;
    surgery.id = 11;
    surgery.name = QStringLiteral("踝关节置换");
    surgery.createdAt = QDateTime(QDate(2026, 4, 17), QTime(10, 0, 0));
    surgery.isActive = true;
    port.surgeries.append(surgery);

    IdentityAppService identityAppService(&port);
    ManagementPageNew page(nullptr, &identityAppService);

    page.onActivated();

    auto* doctorTable = page.findChild<QTableWidget*>(QStringLiteral("doctorTable"));
    auto* patientTable = page.findChild<QTableWidget*>(QStringLiteral("patientTable"));
    auto* surgeryTable = page.findChild<QTableWidget*>(QStringLiteral("surgeryTable"));
    QVERIFY(doctorTable != nullptr);
    QVERIFY(patientTable != nullptr);
    QVERIFY(surgeryTable != nullptr);
    QCOMPARE(doctorTable->rowCount(), 1);
    QCOMPARE(patientTable->rowCount(), 1);
    QCOMPARE(surgeryTable->rowCount(), 1);
    QCOMPARE(doctorTable->item(0, 1)->text(), QStringLiteral("doctor.zhang"));
    QCOMPARE(patientTable->item(0, 1)->text(), QStringLiteral("患者甲"));
    QCOMPARE(surgeryTable->item(0, 3)->text(), QStringLiteral("踝关节置换"));
}

void CorePagesPlatformProvidersTest::dashboardPage_uses_facade_services_for_patient_and_imaging_views()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    FakeIdentityProviderPort identityPort;
    PatientItem patient;
    patient.id = 21;
    patient.name = QStringLiteral("患者乙");
    patient.gender = QStringLiteral("女");
    patient.age = 52;
    patient.phone = QStringLiteral("13700000000");
    patient.description = QStringLiteral("距骨骨折");
    identityPort.patients.append(patient);

    FakeImagingProviderPort imagingPort;
    DicomStudyInfo study;
    study.id = 4;
    study.patientId = 21;
    study.studyUID = QStringLiteral("study-21-1");
    study.studyDescription = QStringLiteral("术前 CT");
    study.studyDate = QDateTime(QDate(2026, 4, 16), QTime(9, 30, 0));
    imagingPort.studies.insert(21, { study });

    FakeNavigationProviderPort navigationPort;
    IdentityAppService identityAppService(&identityPort);
    ImagingAppService imagingAppService(&imagingPort);
    NavigationAppService navigationAppService(&navigationPort);
    DashboardPageNew page(nullptr, &identityAppService, &imagingAppService, &navigationAppService);
    QSignalSpy enterNavigationSpy(&page, &DashboardPageNew::enterNavigationRequested);

    AnkleCaseWorkspaceRepository repository(tempRoot.path());
    AnkleCaseManifest manifest;
    manifest.caseId = QStringLiteral("ankle-case-021");
    QVERIFY(repository.createCaseWorkspace(manifest));

    AnkleCaseAssetBindings bindings;
    bindings.caseId = manifest.caseId;
    bindings.boundBoneAssetIds = QStringList { QStringLiteral("bone:tibia"), QStringLiteral("bone:talus") };
    bindings.activeBoneAssetIds = bindings.boundBoneAssetIds;
    bindings.boundInstrumentAssetIds = QStringList { QStringLiteral("instrument:probe-main") };
    bindings.activeInstrumentAssetIds = bindings.boundInstrumentAssetIds;
    bindings.instrumentGeometryBindings = {
        AnkleInstrumentGeometryBinding {
            QStringLiteral("instrument:probe-main"),
            QStringLiteral("geometry:probe-main"),
            QStringLiteral("geometry/probe-main.ini")
        }
    };
    QVERIFY(repository.saveCaseAssetBindings(bindings));
    page.setCurrentCaseId(manifest.caseId);
    page.setCaseWorkspaceDataRoot(tempRoot.path());

    page.onActivated();
    QCoreApplication::processEvents();

    auto* patientListWidget = page.findChild<QListWidget*>(QStringLiteral("patientListWidget"));
    auto* patientNameLabel = page.findChild<QLabel*>(QStringLiteral("patientNameLabel"));
    auto* overviewDicomValueLabel = page.findChild<QLabel*>(QStringLiteral("overviewDicomValueLabel"));
    auto* enterNavigationButton = page.findChild<QPushButton*>(QStringLiteral("enterNavigationButton"));
    QVERIFY(patientListWidget != nullptr);
    QVERIFY(patientNameLabel != nullptr);
    QVERIFY(overviewDicomValueLabel != nullptr);
    QVERIFY(enterNavigationButton != nullptr);
    QCOMPARE(patientListWidget->count(), 1);
    QCOMPARE(patientNameLabel->text(), QStringLiteral("患者乙"));
    QCOMPARE(imagingPort.requestedPatientId, 21);
    QCOMPARE(overviewDicomValueLabel->text(), QStringLiteral("1 组检查"));

    enterNavigationButton->click();

    QCOMPARE(navigationPort.lastPluginId, QStringLiteral("org.medicalpro.registration_core"));
    QCOMPARE(enterNavigationSpy.count(), 1);
}

QTEST_MAIN(CorePagesPlatformProvidersTest)
#include "CorePagesPlatformProvidersTest.moc"
