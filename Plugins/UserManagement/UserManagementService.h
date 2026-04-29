#ifndef USER_MANAGEMENT_SERVICE_H
#define USER_MANAGEMENT_SERVICE_H

#include "UserDataStructures.h"

#include <QObject>

class QWidget;

// User management service interface.
class UserManagementService : public QObject
{
    Q_OBJECT

public:
    explicit UserManagementService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~UserManagementService() = default;

    // Authentication
    virtual UserInfo loginUser(const QString& username, const QString& password) = 0;
    virtual bool logoutUser(int userId) = 0;
    virtual bool isUserLoggedIn(int userId) = 0;
    virtual UserInfo getCurrentUser() = 0;

    // User accounts
    virtual bool registerUser(const UserInfo& user) = 0;
    virtual bool updateUser(const UserInfo& user) = 0;
    virtual bool deleteUser(int userId) = 0;
    virtual UserInfo getUser(int userId) = 0;
    virtual UserInfo getUserById(int userId) = 0;
    virtual UserInfo getUserByUsername(const QString& username) = 0;
    virtual QList<UserInfo> getAllUsers() = 0;
    virtual QList<UserInfo> searchUsers(const UserSearchCriteria& criteria) = 0;

    // Doctor profiles
    virtual bool upsertDoctorProfile(const DoctorProfile& profile) = 0;
    virtual DoctorProfile getDoctorProfile(int userId) = 0;

    // Surgery items
    virtual bool createSurgeryItem(const SurgeryItem& item) = 0;
    virtual bool updateSurgeryItem(const SurgeryItem& item) = 0;
    virtual bool deleteSurgeryItem(int id) = 0;
    virtual SurgeryItem getSurgeryItem(int id) = 0;
    virtual QList<SurgeryItem> listSurgeryItems(const QString& keyword = QString()) = 0;
    virtual bool isSurgeryNameExists(const QString& name, int excludeId = -1) = 0;

    // Patients
    virtual bool createPatient(const PatientItem& p) = 0;
    virtual bool updatePatient(const PatientItem& p) = 0;
    virtual bool deletePatient(int id) = 0;
    virtual PatientItem getPatient(int id) = 0;
    virtual QList<PatientItem> listPatients(const QString& keyword = QString()) = 0;
    virtual bool isPatientDuplicate(const QString& name, const QString& phone, int excludeId = -1) = 0;

    // Surgery cases
    virtual bool createSurgeryCase(const SurgeryCase& c) = 0;
    virtual bool updateSurgeryCase(const SurgeryCase& c) = 0;
    virtual bool deleteSurgeryCase(int id) = 0;
    virtual QList<SurgeryCase> listSurgeryCasesByPatient(int patientId) = 0;
    virtual QList<SurgeryCase> listSurgeryCasesByDoctor(int doctorUserId) = 0;
    virtual QList<SurgeryCase> listSurgeryCasesBySurgeryItem(int surgeryItemId) = 0;

    // Doctor-patient relationships
    virtual bool linkDoctorPatient(int doctorUserId, int patientId) = 0;
    virtual bool unlinkDoctorPatient(int doctorUserId, int patientId) = 0;
    virtual QList<PatientItem> listPatientsByDoctor(int doctorUserId) = 0;
    virtual QList<UserInfo> listDoctorsByPatient(int patientId) = 0;
    virtual bool isDoctorPatientLinked(int doctorUserId, int patientId) = 0;

    // Instruments
    virtual bool createInstrument(const InstrumentItem& ins) = 0;
    virtual bool updateInstrument(const InstrumentItem& ins) = 0;
    virtual bool deleteInstrument(int id) = 0;
    virtual QList<InstrumentItem> listInstruments(const QString& keyword = QString()) = 0;
    virtual InstrumentItem getInstrument(int id) = 0;

    // Surgery-instrument relationships
    virtual bool linkSurgeryInstrument(int surgeryItemId, int instrumentId) = 0;
    virtual bool unlinkSurgeryInstrument(int surgeryItemId, int instrumentId) = 0;
    virtual QList<InstrumentItem> listInstrumentsBySurgeryItem(int surgeryItemId) = 0;
    virtual QList<SurgeryItem> listSurgeryItemsByInstrument(int instrumentId) = 0;

    // Passwords and permissions
    virtual bool changePassword(int userId, const QString& oldPassword, const QString& newPassword) = 0;
    virtual bool resetPassword(int userId, const QString& newPassword) = 0;
    virtual int validatePasswordStrength(const QString& password) = 0;
    virtual bool checkUserPermission(int userId, const QString& permission) = 0;
    virtual UserRole getUserRole(int userId) = 0;
    virtual bool setUserRole(int userId, UserRole role) = 0;

    // Database and status
    virtual bool initializeDatabase() = 0;
    virtual QString getDatabaseStatus() = 0;
    virtual int getUserCount() = 0;
    virtual bool backupDatabase(const QString& backupPath) = 0;

    // UI entry points
    virtual bool showLoginDialog(QWidget* parent = nullptr) = 0;
    virtual bool showRegisterDialog(QWidget* parent = nullptr) = 0;
    virtual bool showUserManagementDialog(QWidget* parent = nullptr) = 0;

signals:
    void userLoggedIn(const UserInfo& user);
    void userLoggedOut(int userId);
    void userRegistered(const UserInfo& user);
    void userUpdated(const UserInfo& user);
    void userDeleted(int userId);
    void passwordChanged(int userId);
    void userRoleChanged(int userId, UserRole newRole);
    void databaseError(const QString& error);
    void authenticationFailed(const QString& username, const QString& reason);
};

// Service interface declaration.
Q_DECLARE_INTERFACE(UserManagementService, "medical.UserManagementService")

#endif // USER_MANAGEMENT_SERVICE_H
