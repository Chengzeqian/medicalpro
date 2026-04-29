#ifndef USER_MANAGEMENT_SERVICE_IMPL_H
#define USER_MANAGEMENT_SERVICE_IMPL_H

#include "UserManagementService.h"
#include "UserDataStructures.h"

#include <QObject>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMap>
#include <QMutex>

class IPlatformEventBusPort;

// Concrete implementation of the user management service contract.
class UserManagementServiceImpl : public UserManagementService
{
    Q_OBJECT
    Q_INTERFACES(UserManagementService)

public:
    explicit UserManagementServiceImpl(QObject* parent = nullptr);
    ~UserManagementServiceImpl() override;
    void setEventBus(IPlatformEventBusPort* eventBus);

    // Authentication
    UserInfo loginUser(const QString& username, const QString& password) override;
    bool logoutUser(int userId) override;
    bool isUserLoggedIn(int userId) override;
    UserInfo getCurrentUser() override;

    // User accounts
    bool registerUser(const UserInfo& user) override;
    bool updateUser(const UserInfo& user) override;
    bool deleteUser(int userId) override;
    UserInfo getUser(int userId) override;
    UserInfo getUserById(int userId) override;
    UserInfo getUserByUsername(const QString& username) override;
    QList<UserInfo> getAllUsers() override;
    QList<UserInfo> searchUsers(const UserSearchCriteria& criteria) override;

    // Doctor profiles
    bool upsertDoctorProfile(const DoctorProfile& profile) override;
    DoctorProfile getDoctorProfile(int userId) override;

    // Surgery items
    bool createSurgeryItem(const SurgeryItem& item) override;
    bool updateSurgeryItem(const SurgeryItem& item) override;
    bool deleteSurgeryItem(int id) override;
    SurgeryItem getSurgeryItem(int id) override;
    QList<SurgeryItem> listSurgeryItems(const QString& keyword = QString()) override;
    bool isSurgeryNameExists(const QString& name, int excludeId = -1) override;

    // Patients
    bool createPatient(const PatientItem& p) override;
    bool updatePatient(const PatientItem& p) override;
    bool deletePatient(int id) override;
    PatientItem getPatient(int id) override;
    virtual QList<PatientItem> listPatients(const QString& keyword = QString()) override;
    virtual bool isPatientDuplicate(const QString& name, const QString& phone, int excludeId = -1) override;

    QString getLastError() const;

    // Surgery cases
    bool createSurgeryCase(const SurgeryCase& c) override;
    bool updateSurgeryCase(const SurgeryCase& c) override;
    bool deleteSurgeryCase(int id) override;
    QList<SurgeryCase> listSurgeryCasesByPatient(int patientId) override;
    QList<SurgeryCase> listSurgeryCasesByDoctor(int doctorUserId) override;
    QList<SurgeryCase> listSurgeryCasesBySurgeryItem(int surgeryItemId) override;

    // Doctor-patient relationships
    bool linkDoctorPatient(int doctorUserId, int patientId) override;
    bool unlinkDoctorPatient(int doctorUserId, int patientId) override;
    QList<PatientItem> listPatientsByDoctor(int doctorUserId) override;
    QList<UserInfo> listDoctorsByPatient(int patientId) override;
    bool isDoctorPatientLinked(int doctorUserId, int patientId) override;

    // Instruments
    bool createInstrument(const InstrumentItem& ins) override;
    bool updateInstrument(const InstrumentItem& ins) override;
    bool deleteInstrument(int id) override;
    QList<InstrumentItem> listInstruments(const QString& keyword = QString()) override;
    InstrumentItem getInstrument(int id) override;

    // Surgery-instrument relationships
    bool linkSurgeryInstrument(int surgeryItemId, int instrumentId) override;
    bool unlinkSurgeryInstrument(int surgeryItemId, int instrumentId) override;
    QList<InstrumentItem> listInstrumentsBySurgeryItem(int surgeryItemId) override;
    QList<SurgeryItem> listSurgeryItemsByInstrument(int instrumentId) override;

    // Passwords and permissions
    bool changePassword(int userId, const QString& oldPassword, const QString& newPassword) override;
    bool resetPassword(int userId, const QString& newPassword) override;
    int validatePasswordStrength(const QString& password) override;
    bool checkUserPermission(int userId, const QString& permission) override;
    UserRole getUserRole(int userId) override;
    bool setUserRole(int userId, UserRole role) override;

    // Database and status
    bool initializeDatabase() override;
    QString getDatabaseStatus() override;
    int getUserCount() override;
    bool backupDatabase(const QString& backupPath) override;

    bool isOfflineModeEnabled() const { return m_offlineMode; }

    // UI entry points
    bool showLoginDialog(QWidget* parent = nullptr) override;
    bool showRegisterDialog(QWidget* parent = nullptr) override;
    bool showUserManagementDialog(QWidget* parent = nullptr) override;

public slots:
    void checkSessionTimeout();
    void cleanupExpiredSessions();

private slots:
    void handleDatabaseError(const QSqlQuery& query);

private:
    bool ensureDatabaseInitialized();
    bool handleDatabaseInitializationFailure(const QString& errorMessage);
    void logDatabaseStatus(const QString& status);
    bool ensureDatabaseAvailable(const char* operationContext);

    // Database helpers
    bool initializeDatabaseConnection();
    bool createDatabaseTables();
    bool createDoctorTables();
    bool createSurgeryTables();
    bool createPatientsTables();
    bool createSurgeryCaseTables();
    bool createDoctorPatientTables();
    bool createInstrumentTables();
    bool createSurgeryInstrumentTables();
    void initializeTestData();
    bool executeQuery(QSqlQuery& query);
    bool checkDatabaseConnection();

    // Authentication helpers
    UserInfo authenticateUser(const QString& username, const QString& password);
    QString createUserSession(const UserInfo& user);
    bool destroyUserSession(int userId);
    void updateSessionActivity(int userId);

    // Password helpers
    QString hashPassword(const QString& password, const QString& salt);
    QString generateSalt();
    bool verifyPassword(const QString& password, const QString& hash, const QString& salt);

    // Permission helpers
    QStringList getRolePermissions(UserRole role);
    bool checkOperationPermission(UserRole currentUserRole,
                                  const QString& operation,
                                  UserRole targetUserRole = UserRole::Guest);

    // Logging helpers
    void logOperation(int userId,
                      const QString& operation,
                      const QString& description,
                      const QString& target = QString(),
                      bool success = true,
                      const QString& errorMessage = QString());
    void logSecurityEvent(const QString& level, const QString& message, int userId = -1);

    // Validation helpers
    QPair<bool, QString> validateUserInfo(const UserInfo& user, bool isUpdate = false);
    bool isUsernameExists(const QString& username, int excludeUserId = -1);
    bool isEmailExists(const QString& email, int excludeUserId = -1);

    // Platform events
    void publishPlatformEvent(const QString& topic, const QVariantMap& properties);
    void initializeRolePermissions();
    void createDefaultAdminUser();
    QString formatUserRole(UserRole role);

private:
    // Database state
    QSqlDatabase m_database;
    QString m_databasePath;
    bool m_databaseInitialized;
    bool m_offlineMode;
    mutable QMutex m_databaseMutex;
    bool m_databasePromptActive;

    // Session state
    QMap<int, UserSession> m_activeSessions;
    UserInfo m_currentUser;
    QTimer* m_sessionTimer;
    QMutex m_sessionMutex;

    // Password policy
    PasswordPolicy m_passwordPolicy;

    // Login attempt tracking
    QMap<QString, int> m_loginAttempts;
    QMap<QString, QDateTime> m_lockoutEndTime;

    // Role permissions
    QMap<UserRole, QStringList> m_rolePermissions;

    // Platform event bus
    IPlatformEventBusPort* m_eventBus;

    // Service state
    bool m_serviceInitialized;
    QString m_lastError;

    // Statistics
    int m_totalLoginAttempts;
    int m_successfulLogins;
    int m_failedLogins;
};

#endif // USER_MANAGEMENT_SERVICE_IMPL_H
