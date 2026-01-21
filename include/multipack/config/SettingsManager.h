/**
 * @file SettingsManager.h
 * @brief Application settings management
 *
 * Manages loading, saving, and accessing application settings.
 */

#ifndef MULTIPACK_CONFIG_SETTINGSMANAGER_H
#define MULTIPACK_CONFIG_SETTINGSMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QJsonObject>
#include <memory>

namespace multipack {
namespace config {

/**
 * @enum RobotModel
 * @brief Supported robot models
 */
enum class RobotModel {
    UR10,   ///< Universal Robots UR10
    UR20    ///< Universal Robots UR20
};

/**
 * @class SettingsManager
 * @brief Manages application settings
 *
 * Loads and saves settings to a JSON file,
 * with encrypted password storage.
 */
class SettingsManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new Settings Manager
     * @param parent Parent QObject
     */
    explicit SettingsManager(QObject* parent = nullptr);

    /**
     * @brief Destroy the Settings Manager
     */
    ~SettingsManager() override;

    /**
     * @brief Load settings from file
     * @param path Settings file path (empty = default)
     * @return true on success
     */
    [[nodiscard]] bool load(const QString& path = QString());

    /**
     * @brief Save settings to file
     * @param path Settings file path (empty = default)
     * @return true on success
     */
    [[nodiscard]] bool save(const QString& path = QString());

    /**
     * @brief Reset to default settings
     */
    void resetToDefaults();

    // General accessors

    /**
     * @brief Get a setting value
     * @param key Setting key (dot notation: "section.key")
     * @param defaultValue Default if not found
     * @return Setting value
     */
    [[nodiscard]] QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief Set a setting value
     * @param key Setting key (dot notation)
     * @param value Value to set
     */
    void setValue(const QString& key, const QVariant& value);

    /**
     * @brief Check if setting exists
     * @param key Setting key
     * @return true if exists
     */
    [[nodiscard]] bool contains(const QString& key) const;

    /**
     * @brief Remove a setting
     * @param key Setting key
     */
    void remove(const QString& key);

    // Typed accessors

    RobotModel robotModel() const;
    void setRobotModel(RobotModel model);

    QString robotIp() const;
    void setRobotIp(const QString& ip);

    int xmlRpcPort() const;
    void setXmlRpcPort(int port);

    QString usbPath() const;
    void setUsbPath(const QString& path);

    QString databasePath() const;
    void setDatabasePath(const QString& path);

    bool audioEnabled() const;
    void setAudioEnabled(bool enabled);

    float audioVolume() const;
    void setAudioVolume(float volume);

    // Display settings
    int displayWidth() const;
    void setDisplayWidth(int width);

    int displayHeight() const;
    void setDisplayHeight(int height);

    bool fullscreen() const;
    void setFullscreen(bool fullscreen);

    QString displaySpecsModel() const;
    void setDisplaySpecsModel(const QString& model);

    int displaySpecsWidth() const;
    void setDisplaySpecsWidth(int width);

    int displaySpecsHeight() const;
    void setDisplaySpecsHeight(int height);

    int displaySpecsRefreshRate() const;
    void setDisplaySpecsRefreshRate(int rate);

    // Info settings
    QString urSerialNumber() const;
    void setUrSerialNumber(const QString& serial);

    QString urManufacturingDate() const;
    void setUrManufacturingDate(const QString& date);

    QString urSoftwareVersion() const;
    void setUrSoftwareVersion(const QString& version);

    QString pallettiererName() const;
    void setPallettiererName(const QString& name);

    QString pallettiererStandort() const;
    void setPallettiererStandort(const QString& standort);

    int numberOfPlans() const;
    void setNumberOfPlans(int count);

    int numberOfUseCycles() const;
    void setNumberOfUseCycles(int count);

    QString lastRestart() const;
    void setLastRestart(const QString& timestamp);

    // Audio file settings
    QString alarmSoundFile() const;
    void setAlarmSoundFile(const QString& path);

    QString scannerWarningSoundFile() const;
    void setScannerWarningSoundFile(const QString& path);

    // USB key bypass settings
    QString usbKeyPath() const;
    void setUsbKeyPath(const QString& path);

    QString usbKeyExpectedValue() const;
    void setUsbKeyExpectedValue(const QString& value);

    /**
     * @brief Check if USB key bypass is valid
     * @return true if USB key file exists and contains expected value
     */
    [[nodiscard]] bool isUsbKeyBypassValid() const;

    // Password management (encrypted)

    /**
     * @brief Set admin password
     * @param password Plain text password
     */
    void setAdminPassword(const QString& password);

    /**
     * @brief Verify admin password
     * @param password Password to check
     * @return true if matches
     */
    [[nodiscard]] bool verifyAdminPassword(const QString& password) const;

    /**
     * @brief Check if admin password is set
     * @return true if set
     */
    [[nodiscard]] bool hasAdminPassword() const;

    // Validation methods

    /**
     * @brief Validate IP address format
     * @param ip IP address string
     * @return true if valid IPv4 format
     */
    [[nodiscard]] static bool isValidIpAddress(const QString& ip);

    /**
     * @brief Validate port number
     * @param port Port number
     * @return true if in valid range (1-65535)
     */
    [[nodiscard]] static bool isValidPort(int port);

    /**
     * @brief Validate volume level
     * @param volume Volume value
     * @return true if in valid range (0.0-1.0)
     */
    [[nodiscard]] static bool isValidVolume(float volume);

    /**
     * @brief Validate robot model string
     * @param model Model string
     * @return true if valid (UR10 or UR20)
     */
    [[nodiscard]] static bool isValidRobotModel(const QString& model);

    /**
     * @brief Validate password meets minimum requirements
     * @param password Password to validate
     * @return true if meets minimum length
     */
    [[nodiscard]] static bool isValidPassword(const QString& password);

    /**
     * @brief Validate all current settings
     * @param errors Output list of validation errors
     * @return true if all settings are valid
     */
    [[nodiscard]] bool validateSettings(QStringList* errors = nullptr) const;

signals:
    /**
     * @brief Emitted when settings are loaded
     */
    void settingsLoaded();

    /**
     * @brief Emitted when settings are saved
     */
    void settingsSaved();

    /**
     * @brief Emitted when a setting changes
     * @param key Changed key
     * @param value New value
     */
    void settingChanged(const QString& key, const QVariant& value);

private:
    /**
     * @brief Get default settings path
     * @return Default file path
     */
    QString defaultPath() const;

    /**
     * @brief Encrypt a password
     * @param password Plain text
     * @return Encrypted string
     */
    QString encryptPassword(const QString& password) const;

    /**
     * @brief Hash password for verification
     * @param password Plain text
     * @return SHA-256 hash of password with salt
     */
    QString hashPassword(const QString& password) const;
    
    /**
     * @brief Generate random salt for password hashing
     * @return 16-byte random salt
     */
    QByteArray generateSalt() const;
    
    /**
     * @brief Hash password with provided salt (for verification)
     * @param password Plain text password
     * @param salt Salt bytes to use
     * @return SHA-256 hash of salted password
     */
    QString hashPasswordWithSalt(const QString& password, const QByteArray& salt) const;

    QJsonObject m_settings;
    QString m_currentPath;
};

} // namespace config
} // namespace multipack

#endif // MULTIPACK_CONFIG_SETTINGSMANAGER_H
