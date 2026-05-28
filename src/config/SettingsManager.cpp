/**
 * @file SettingsManager.cpp
 * @brief Implementation of settings manager
 */

#include "multipack/config/SettingsManager.h"
#include "multipack/config/ConfigDefaults.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QRandomGenerator>
#include <QCoreApplication>

namespace multipack {
namespace config {

namespace {

QJsonObject mergeObjects(const QJsonObject& defaults, const QJsonObject& overrides)
{
    QJsonObject merged = defaults;
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        if (it.value().isObject() && merged.value(it.key()).isObject()) {
            merged[it.key()] = mergeObjects(merged.value(it.key()).toObject(), it.value().toObject());
        } else {
            merged[it.key()] = it.value();
        }
    }
    return merged;
}

bool setNestedValue(QJsonObject& object, const QStringList& parts, int index, const QJsonValue& value)
{
    if (index >= parts.size()) {
        return false;
    }

    if (index == parts.size() - 1) {
        object[parts[index]] = value;
        return true;
    }

    QJsonObject child = object.value(parts[index]).toObject();
    const bool updated = setNestedValue(child, parts, index + 1, value);
    object[parts[index]] = child;
    return updated;
}

bool removeNestedValue(QJsonObject& object, const QStringList& parts, int index)
{
    if (index >= parts.size()) {
        return false;
    }

    if (index == parts.size() - 1) {
        const bool existed = object.contains(parts[index]);
        object.remove(parts[index]);
        return existed;
    }

    if (!object.value(parts[index]).isObject()) {
        return false;
    }

    QJsonObject child = object.value(parts[index]).toObject();
    const bool removed = removeNestedValue(child, parts, index + 1);
    object[parts[index]] = child;
    return removed;
}

}

SettingsManager::SettingsManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "SettingsManager::SettingsManager - constructor";
    resetToDefaults();
}

SettingsManager::~SettingsManager()
{
    qDebug() << "SettingsManager::~SettingsManager - destructor";
}

bool SettingsManager::load(const QString& path)
{
    QString filePath = path.isEmpty() ? defaultPath() : path;
    qDebug() << "SettingsManager::load -" << filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open settings file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "Settings file root is not an object";
        return false;
    }

    const QJsonObject loadedSettings = doc.object();
    resetToDefaults();
    m_settings = mergeObjects(m_settings, loadedSettings);
    m_currentPath = filePath;

    QJsonObject admin = m_settings["admin"].toObject();
    if (!admin.contains("password_hash")) {
        admin["password_hash"] = hashPassword("666666");
    }
    m_settings["admin"] = admin;

    emit settingsLoaded();
    qDebug() << "Settings loaded successfully";
    return true;
}

bool SettingsManager::save(const QString& path)
{
    QString filePath = path.isEmpty() ?
                       (m_currentPath.isEmpty() ? defaultPath() : m_currentPath)
                       : path;
    qDebug() << "SettingsManager::save -" << filePath;

    QJsonDocument doc(m_settings);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open settings file for writing:" << filePath;
        return false;
    }

    QByteArray data = doc.toJson(QJsonDocument::Indented);
    qint64 bytesWritten = file.write(data);
    file.close();

    if (bytesWritten != data.size()) {
        qWarning() << "Failed to write complete settings file:" << filePath
                   << "- wrote" << bytesWritten << "of" << data.size() << "bytes";
        return false;
    }

    m_currentPath = filePath;
    emit settingsSaved();
    qDebug() << "Settings saved successfully";
    return true;
}

void SettingsManager::resetToDefaults()
{
    qDebug() << "SettingsManager::resetToDefaults";

    m_settings = QJsonObject();

    // Info section
    QJsonObject info;
    info["version"] = Defaults::VERSION;
    info["UR_Model"] = "UR10";
    info["UR_Serial_Number"] = "N/A";
    info["UR_Manufacturing_Date"] = "N/A";
    info["UR_Software_Version"] = "N/A";
    info["Pallettierer_Name"] = "N/A";
    info["Pallettierer_Standort"] = "N/A";
    info["number_of_plans"] = 0;
    info["number_of_use_cycles"] = 0;
    info["last_restart"] = "Never";
    m_settings["info"] = info;

    // Robot section
    QJsonObject robot;
    robot["ip"] = Defaults::ROBOT_IP;
    robot["dashboard_port"] = Defaults::DASHBOARD_PORT;
    m_settings["robot"] = robot;

    // Server section
    QJsonObject server;
    server["port"] = Defaults::XMLRPC_PORT;
    server["usb_path"] = Defaults::defaultUsbPath();
    m_settings["server"] = server;

    // Database section
    QJsonObject database;
    database["path"] = Defaults::defaultDatabasePath();
    m_settings["database"] = database;

    // Audio section
    QJsonObject audio;
    audio["enabled"] = Defaults::AUDIO_ENABLED;
    audio["volume"] = Defaults::AUDIO_VOLUME;
    m_settings["audio"] = audio;

    // Display section
    QJsonObject display;
    display["width"] = Defaults::DISPLAY_WIDTH;
    display["height"] = Defaults::DISPLAY_HEIGHT;
    display["fullscreen"] = Defaults::FULLSCREEN_DEFAULT;

    QJsonObject specs;
    specs["model"] = "";
    specs["width"] = 0;
    specs["height"] = 0;
    specs["refresh_rate"] = 60;
    display["specs"] = specs;
    m_settings["display"] = display;

    // UI section
    QJsonObject ui;
    ui["fullscreen"] = Defaults::FULLSCREEN_DEFAULT;
    ui["virtual_keyboard"] = Defaults::VIRTUAL_KEYBOARD_ENABLED;
    m_settings["ui"] = ui;

    // Admin section
    QJsonObject admin;
    admin["password_hash"] = hashPassword("666666");
    admin["path"] = "..";
    admin["alarm_sound_file"] = Defaults::ALARM_SOUND_FILE;
    admin["scanner_warning_sound_file"] = Defaults::SCANNER_WARNING_SOUND_FILE;
    admin["usb_key"] = "";
    admin["usb_expected_value"] = "";
    m_settings["admin"] = admin;
}

QVariant SettingsManager::value(const QString& key, const QVariant& defaultValue) const
{
    QStringList parts = key.split('.');
    if (parts.isEmpty()) {
        return defaultValue;
    }

    QJsonValue current = m_settings;
    for (const QString& part : parts) {
        if (current.isObject()) {
            current = current.toObject()[part];
        } else {
            return defaultValue;
        }
    }

    return current.toVariant();
}

void SettingsManager::setValue(const QString& key, const QVariant& value)
{
    QStringList parts = key.split('.');
    if (parts.isEmpty()) {
        return;
    }

    setNestedValue(m_settings, parts, 0, QJsonValue::fromVariant(value));

    emit settingChanged(key, value);
}

bool SettingsManager::contains(const QString& key) const
{
    return !value(key).isNull();
}

void SettingsManager::remove(const QString& key)
{
    QStringList parts = key.split('.');
    if (parts.isEmpty()) {
        return;
    }

    if (removeNestedValue(m_settings, parts, 0)) {
        emit settingChanged(key, QVariant());
    }
}

RobotModel SettingsManager::robotModel() const
{
    QString model = value(Keys::INFO_UR_MODEL, "UR10").toString();
    return (model == "UR20") ? RobotModel::UR20 : RobotModel::UR10;
}

void SettingsManager::setRobotModel(RobotModel model)
{
    QString modelStr = (model == RobotModel::UR20) ? "UR20" : "UR10";
    setValue(Keys::INFO_UR_MODEL, modelStr);
}

QString SettingsManager::robotIp() const
{
    return value(Keys::ROBOT_IP, Defaults::ROBOT_IP).toString();
}

void SettingsManager::setRobotIp(const QString& ip)
{
    setValue(Keys::ROBOT_IP, ip);
}

int SettingsManager::xmlRpcPort() const
{
    return value(Keys::SERVER_PORT, Defaults::XMLRPC_PORT).toInt();
}

void SettingsManager::setXmlRpcPort(int port)
{
    setValue(Keys::SERVER_PORT, port);
}

QString SettingsManager::usbPath() const
{
    return value(Keys::SERVER_USB_PATH, Defaults::defaultUsbPath()).toString();
}

void SettingsManager::setUsbPath(const QString& path)
{
    setValue(Keys::SERVER_USB_PATH, path);
}

QString SettingsManager::databasePath() const
{
    return value(Keys::DATABASE_PATH, Defaults::defaultDatabasePath()).toString();
}

void SettingsManager::setDatabasePath(const QString& path)
{
    setValue(Keys::DATABASE_PATH, path);
}

bool SettingsManager::audioEnabled() const
{
    return value(Keys::AUDIO_ENABLED, Defaults::AUDIO_ENABLED).toBool();
}

void SettingsManager::setAudioEnabled(bool enabled)
{
    setValue(Keys::AUDIO_ENABLED, enabled);
}

float SettingsManager::audioVolume() const
{
    return value(Keys::AUDIO_VOLUME, Defaults::AUDIO_VOLUME).toFloat();
}

void SettingsManager::setAudioVolume(float volume)
{
    setValue(Keys::AUDIO_VOLUME, qBound(0.0f, volume, 1.0f));
}

// Display settings

int SettingsManager::displayWidth() const
{
    return value(Keys::DISPLAY_WIDTH, Defaults::DISPLAY_WIDTH).toInt();
}

void SettingsManager::setDisplayWidth(int width)
{
    setValue(Keys::DISPLAY_WIDTH, width);
}

int SettingsManager::displayHeight() const
{
    return value(Keys::DISPLAY_HEIGHT, Defaults::DISPLAY_HEIGHT).toInt();
}

void SettingsManager::setDisplayHeight(int height)
{
    setValue(Keys::DISPLAY_HEIGHT, height);
}

bool SettingsManager::fullscreen() const
{
    return value(Keys::DISPLAY_FULLSCREEN, Defaults::FULLSCREEN_DEFAULT).toBool();
}

void SettingsManager::setFullscreen(bool fullscreen)
{
    setValue(Keys::DISPLAY_FULLSCREEN, fullscreen);
}

QString SettingsManager::displaySpecsModel() const
{
    return value(Keys::DISPLAY_SPECS_MODEL, "").toString();
}

void SettingsManager::setDisplaySpecsModel(const QString& model)
{
    setValue(Keys::DISPLAY_SPECS_MODEL, model);
}

int SettingsManager::displaySpecsWidth() const
{
    return value(Keys::DISPLAY_SPECS_WIDTH, 0).toInt();
}

void SettingsManager::setDisplaySpecsWidth(int width)
{
    setValue(Keys::DISPLAY_SPECS_WIDTH, width);
}

int SettingsManager::displaySpecsHeight() const
{
    return value(Keys::DISPLAY_SPECS_HEIGHT, 0).toInt();
}

void SettingsManager::setDisplaySpecsHeight(int height)
{
    setValue(Keys::DISPLAY_SPECS_HEIGHT, height);
}

int SettingsManager::displaySpecsRefreshRate() const
{
    return value(Keys::DISPLAY_SPECS_REFRESH_RATE, 60).toInt();
}

void SettingsManager::setDisplaySpecsRefreshRate(int rate)
{
    setValue(Keys::DISPLAY_SPECS_REFRESH_RATE, rate);
}

// Info settings

QString SettingsManager::urSerialNumber() const
{
    return value(Keys::INFO_UR_SERIAL_NUMBER, "N/A").toString();
}

void SettingsManager::setUrSerialNumber(const QString& serial)
{
    setValue(Keys::INFO_UR_SERIAL_NUMBER, serial);
}

QString SettingsManager::urManufacturingDate() const
{
    return value(Keys::INFO_UR_MANUFACTURING_DATE, "N/A").toString();
}

void SettingsManager::setUrManufacturingDate(const QString& date)
{
    setValue(Keys::INFO_UR_MANUFACTURING_DATE, date);
}

QString SettingsManager::urSoftwareVersion() const
{
    return value(Keys::INFO_UR_SOFTWARE_VERSION, "N/A").toString();
}

void SettingsManager::setUrSoftwareVersion(const QString& version)
{
    setValue(Keys::INFO_UR_SOFTWARE_VERSION, version);
}

QString SettingsManager::pallettiererName() const
{
    return value(Keys::INFO_PALLETTIERER_NAME, "N/A").toString();
}

void SettingsManager::setPallettiererName(const QString& name)
{
    setValue(Keys::INFO_PALLETTIERER_NAME, name);
}

QString SettingsManager::pallettiererStandort() const
{
    return value(Keys::INFO_PALLETTIERER_STANDORT, "N/A").toString();
}

void SettingsManager::setPallettiererStandort(const QString& standort)
{
    setValue(Keys::INFO_PALLETTIERER_STANDORT, standort);
}

int SettingsManager::numberOfPlans() const
{
    return value(Keys::INFO_NUMBER_OF_PLANS, 0).toInt();
}

void SettingsManager::setNumberOfPlans(int count)
{
    setValue(Keys::INFO_NUMBER_OF_PLANS, count);
}

int SettingsManager::numberOfUseCycles() const
{
    return value(Keys::INFO_NUMBER_OF_USE_CYCLES, 0).toInt();
}

void SettingsManager::setNumberOfUseCycles(int count)
{
    setValue(Keys::INFO_NUMBER_OF_USE_CYCLES, count);
}

QString SettingsManager::lastRestart() const
{
    return value(Keys::INFO_LAST_RESTART, "Never").toString();
}

void SettingsManager::setLastRestart(const QString& timestamp)
{
    setValue(Keys::INFO_LAST_RESTART, timestamp);
}

// Audio file settings

QString SettingsManager::alarmSoundFile() const
{
    return value(Keys::ADMIN_ALARM_SOUND_FILE, Defaults::ALARM_SOUND_FILE).toString();
}

void SettingsManager::setAlarmSoundFile(const QString& path)
{
    setValue(Keys::ADMIN_ALARM_SOUND_FILE, path);
}

QString SettingsManager::scannerWarningSoundFile() const
{
    return value(Keys::ADMIN_SCANNER_WARNING_SOUND_FILE, Defaults::SCANNER_WARNING_SOUND_FILE).toString();
}

void SettingsManager::setScannerWarningSoundFile(const QString& path)
{
    setValue(Keys::ADMIN_SCANNER_WARNING_SOUND_FILE, path);
}

// USB key bypass settings

QString SettingsManager::usbKeyPath() const
{
    return value(Keys::ADMIN_USB_KEY, "").toString();
}

void SettingsManager::setUsbKeyPath(const QString& path)
{
    setValue(Keys::ADMIN_USB_KEY, path);
}

QString SettingsManager::usbKeyExpectedValue() const
{
    return value(Keys::ADMIN_USB_EXPECTED_VALUE, "").toString();
}

void SettingsManager::setUsbKeyExpectedValue(const QString& value)
{
    setValue(Keys::ADMIN_USB_EXPECTED_VALUE, value);
}

bool SettingsManager::isUsbKeyBypassValid() const
{
    QString keyPath = usbKeyPath();
    QString expectedValue = usbKeyExpectedValue();

    // No USB key configured
    if (keyPath.isEmpty() || expectedValue.isEmpty()) {
        return false;
    }

    QFile keyFile(keyPath);
    if (!keyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QString content = QString::fromUtf8(keyFile.readAll()).trimmed();
    keyFile.close();

    return content == expectedValue;
}

void SettingsManager::setAdminPassword(const QString& password)
{
    QString hash = hashPassword(password);
    setValue(Keys::ADMIN_PASSWORD_HASH, hash);
}

bool SettingsManager::verifyAdminPassword(const QString& password) const
{
    QString storedHash = value(Keys::ADMIN_PASSWORD_HASH).toString();
    if (storedHash.isEmpty()) {
        return true;  // No password set
    }
    
    // Parse stored format: salt_hex$hash_hex
    QStringList parts = storedHash.split('$');
    if (parts.size() != 2) {
        qWarning() << "Invalid stored password hash format";
        return false;
    }
    
    // Extract salt and hash
    QByteArray salt = QByteArray::fromHex(parts[0].toUtf8());
    QString expectedHash = parts[1];
    
    // Hash provided password with the same salt
    QString actualHash = hashPasswordWithSalt(password, salt);
    
    return actualHash == expectedHash;
}

bool SettingsManager::hasAdminPassword() const
{
    return !value(Keys::ADMIN_PASSWORD_HASH).toString().isEmpty();
}

QString SettingsManager::defaultPath() const
{
    return Defaults::defaultSettingsPath();
}

QString SettingsManager::encryptPassword(const QString& password) const
{
    // Generate salt and hash password like Python version
    return hashPassword(password);
}

QString SettingsManager::hashPassword(const QString& password) const
{
    // Generate random salt (16 bytes) like Python's os.urandom(16)
    QByteArray salt = generateSalt();
    
    // Hash with salt: SHA-256(salt + password)
    QByteArray saltedPassword = salt + password.toUtf8();
    QByteArray hash = QCryptographicHash::hash(saltedPassword, QCryptographicHash::Sha256);
    
    // Return format: salt_hex + '$' + hash_hex (matches Python)
    return salt.toHex() + '$' + QString(hash.toHex());
}

QByteArray SettingsManager::generateSalt() const
{
    // Generate 16 random bytes for salt (equivalent to Python's os.urandom(16))
    // Using QRandomGenerator::securelySeeded() for cryptographically secure randomness
    QByteArray salt(16, 0);
    QRandomGenerator* rng = QRandomGenerator::global();
    for (int i = 0; i < salt.size(); ++i) {
        salt[i] = static_cast<char>(rng->bounded(256));
    }
    return salt;
}

QString SettingsManager::hashPasswordWithSalt(const QString& password, const QByteArray& salt) const
{
    // Hash password with existing salt (for verification)
    QByteArray saltedPassword = salt + password.toUtf8();
    QByteArray hash = QCryptographicHash::hash(saltedPassword, QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

// Validation methods

bool SettingsManager::isValidIpAddress(const QString& ip)
{
    // Validate IPv4 address format
    QStringList parts = ip.split('.');
    if (parts.size() != 4) {
        return false;
    }

    for (const QString& part : parts) {
        bool ok;
        int value = part.toInt(&ok);
        if (!ok || value < 0 || value > 255) {
            return false;
        }
    }
    return true;
}

bool SettingsManager::isValidPort(int port)
{
    return port >= 1 && port <= 65535;
}

bool SettingsManager::isValidVolume(float volume)
{
    return volume >= 0.0f && volume <= 1.0f;
}

bool SettingsManager::isValidRobotModel(const QString& model)
{
    return model == "UR10" || model == "UR20";
}

bool SettingsManager::isValidPassword(const QString& password)
{
    return password.length() >= Defaults::PASSWORD_MIN_LENGTH;
}

bool SettingsManager::validateSettings(QStringList* errors) const
{
    bool valid = true;

    // Validate robot IP
    QString ip = robotIp();
    if (!isValidIpAddress(ip)) {
        valid = false;
        if (errors) {
            errors->append(QString("Invalid robot IP address: %1").arg(ip));
        }
    }

    // Validate XML-RPC port
    int xmlRpcPortVal = xmlRpcPort();
    if (!isValidPort(xmlRpcPortVal)) {
        valid = false;
        if (errors) {
            errors->append(QString("Invalid XML-RPC port: %1 (must be 1-65535)").arg(xmlRpcPortVal));
        }
    }

    // Validate audio volume
    float volume = audioVolume();
    if (!isValidVolume(volume)) {
        valid = false;
        if (errors) {
            errors->append(QString("Invalid audio volume: %1 (must be 0.0-1.0)").arg(volume));
        }
    }

    // Validate robot model
    QString model = value(Keys::INFO_UR_MODEL, "UR10").toString();
    if (!isValidRobotModel(model)) {
        valid = false;
        if (errors) {
            errors->append(QString("Invalid robot model: %1 (must be UR10 or UR20)").arg(model));
        }
    }

    // Validate display dimensions
    int width = displayWidth();
    int height = displayHeight();
    if (width <= 0 || height <= 0) {
        valid = false;
        if (errors) {
            errors->append(QString("Invalid display dimensions: %1x%2").arg(width).arg(height));
        }
    }

    return valid;
}

} // namespace config
} // namespace multipack
