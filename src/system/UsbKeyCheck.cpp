/**
 * @file UsbKeyCheck.cpp
 * @brief Implementation of USB key authentication check
 *
 * Verifies USB security keys using Fernet-compatible decryption.
 */

#include "multipack/system/UsbKeyCheck.h"
#include "multipack/config/SettingsManager.h"
#include "multipack/config/ConfigDefaults.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStorageInfo>
#include <QMessageAuthenticationCode>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace multipack {
namespace system {

namespace {
constexpr const char* kDefaultFernetKey = "9G-1nNuw_tn7_lLkhpCwd_AG9McjQv_LarKcV2kUxrk=";
constexpr const char* kDefaultExpectedPayload =
    "fc2f8726bb317b17a3cb322672818d2d$580c515fc8852dfd6e36faaaf46581c412683135b87dc8750c89efad4a38b54f";
constexpr const char* kLegacyConfigPath = ".config/Multipack/MultipackParser.conf";

QByteArray decodeBase64Url(const QByteArray& input)
{
    QByteArray normalized = input.trimmed();
    normalized.replace('-', '+');
    normalized.replace('_', '/');

    const int paddingNeeded = (4 - (normalized.size() % 4)) % 4;
    normalized.append(QByteArray(paddingNeeded, '='));
    return QByteArray::fromBase64(normalized);
}

bool constantTimeEqual(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size()) {
        return false;
    }

    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

bool splitFernetKey(const QString& key, QByteArray& signingKey, QByteArray& encryptionKey)
{
    const QByteArray decodedKey = decodeBase64Url(key.toUtf8());
    if (decodedKey.size() != 32) {
        return false;
    }

    signingKey = decodedKey.left(16);
    encryptionKey = decodedKey.mid(16, 16);
    return true;
}

bool decryptAes128Cbc(const QByteArray& cipherText,
                      const QByteArray& encryptionKey,
                      const QByteArray& iv,
                      QByteArray& plainText)
{
#ifdef HAVE_OPENSSL
    if (cipherText.isEmpty() || (cipherText.size() % 16) != 0 ||
        encryptionKey.size() != 16 || iv.size() != 16) {
        return false;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }

    int ok = EVP_DecryptInit_ex(
        ctx,
        EVP_aes_128_cbc(),
        nullptr,
        reinterpret_cast<const unsigned char*>(encryptionKey.constData()),
        reinterpret_cast<const unsigned char*>(iv.constData()));
    if (ok != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    QByteArray buffer(cipherText.size() + EVP_CIPHER_block_size(EVP_aes_128_cbc()), 0);
    int outLen1 = 0;
    ok = EVP_DecryptUpdate(
        ctx,
        reinterpret_cast<unsigned char*>(buffer.data()),
        &outLen1,
        reinterpret_cast<const unsigned char*>(cipherText.constData()),
        cipherText.size());
    if (ok != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    int outLen2 = 0;
    ok = EVP_DecryptFinal_ex(
        ctx,
        reinterpret_cast<unsigned char*>(buffer.data()) + outLen1,
        &outLen2);

    EVP_CIPHER_CTX_free(ctx);

    if (ok != 1) {
        return false;
    }

    buffer.truncate(outLen1 + outLen2);
    plainText = buffer;
    return true;
#else
    Q_UNUSED(cipherText);
    Q_UNUSED(encryptionKey);
    Q_UNUSED(iv);
    Q_UNUSED(plainText);
    return false;
#endif
}

bool verifyFernetToken(const QByteArray& token,
                       const QString& fernetKey,
                       const QByteArray& expectedPayload)
{
    // Minimum Fernet token size: version(1) + ts(8) + iv(16) + 1 block ciphertext(16) + hmac(32)
    const QByteArray decodedToken = decodeBase64Url(token);
    if (decodedToken.size() < 73) {
        return false;
    }

    if (static_cast<unsigned char>(decodedToken[0]) != 0x80) {
        return false;
    }

    const int hmacOffset = decodedToken.size() - 32;
    const QByteArray signedData = decodedToken.left(hmacOffset);
    const QByteArray hmacFromToken = decodedToken.mid(hmacOffset, 32);

    QByteArray signingKey;
    QByteArray encryptionKey;
    if (!splitFernetKey(fernetKey, signingKey, encryptionKey)) {
        return false;
    }

    const QByteArray computedHmac =
        QMessageAuthenticationCode::hash(signedData, signingKey, QCryptographicHash::Sha256);
    if (!constantTimeEqual(computedHmac, hmacFromToken)) {
        return false;
    }

#ifdef HAVE_OPENSSL
    const QByteArray iv = decodedToken.mid(9, 16);
    const QByteArray cipherText = decodedToken.mid(25, hmacOffset - 25);
    QByteArray plainText;
    if (!decryptAes128Cbc(cipherText, encryptionKey, iv, plainText)) {
        return false;
    }

    return constantTimeEqual(plainText, expectedPayload);
#else
    Q_UNUSED(encryptionKey);
    Q_UNUSED(expectedPayload);
    return true;
#endif
}

bool loadUsbConfigFromJsonFile(const QString& path, QString& usbKey, QByteArray& expectedValue)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonObject admin = root.value("admin").toObject();
    const QString key = admin.value("usb_key").toString().trimmed();
    const QString expected = admin.value("usb_expected_value").toString();
    if (key.isEmpty() || expected.isEmpty()) {
        return false;
    }

    usbKey = key;
    expectedValue = expected.toUtf8();
    return true;
}

bool loadUsbConfigFromLegacyIni(const QString& path, QString& usbKey, QByteArray& expectedValue)
{
    if (!QFileInfo::exists(path)) {
        return false;
    }

    QSettings settings(path, QSettings::IniFormat);
    const QString key = settings.value("admin/usb_key").toString().trimmed();
    const QString expected = settings.value("admin/usb_expected_value").toString();
    if (key.isEmpty() || expected.isEmpty()) {
        return false;
    }

    usbKey = key;
    expectedValue = expected.toUtf8();
    return true;
}

void resolveUsbConfig(const multipack::config::SettingsManager* settingsManager,
                      QString& usbKey,
                      QByteArray& expectedValue)
{
    usbKey = QString::fromLatin1(kDefaultFernetKey);
    expectedValue = QByteArray(kDefaultExpectedPayload);

    if (settingsManager) {
        const QString configuredKey = settingsManager->value(config::Keys::ADMIN_USB_KEY).toString().trimmed();
        const QString configuredExpected = settingsManager->value(config::Keys::ADMIN_USB_EXPECTED_VALUE).toString();
        if (!configuredKey.isEmpty() && !configuredExpected.isEmpty()) {
            usbKey = configuredKey;
            expectedValue = configuredExpected.toUtf8();
            return;
        }
    }

    const QString jsonPath = QDir::current().filePath("settings.json");
    if (loadUsbConfigFromJsonFile(jsonPath, usbKey, expectedValue)) {
        return;
    }

    const QString legacyPath = QDir::home().filePath(QLatin1String(kLegacyConfigPath));
    if (loadUsbConfigFromLegacyIni(legacyPath, usbKey, expectedValue)) {
        return;
    }
}
} // namespace

UsbKeyCheck::UsbKeyCheck(QObject* parent)
    : QObject(parent)
{
    qDebug() << "UsbKeyCheck - initialized";

    // Add default search paths based on platform
#ifdef Q_OS_WIN
    // Windows: Will dynamically detect USB drives
#else
    // Linux: Common USB mount points
    m_searchPaths << "/media" << "/mnt";

    // Add user-specific media directory
    QString user = qEnvironmentVariable("USER");
    if (!user.isEmpty()) {
        m_searchPaths << QString("/media/%1").arg(user);
        m_searchPaths << QString("/run/media/%1").arg(user);
    }
#endif
}

UsbKeyCheck::~UsbKeyCheck()
{
    qDebug() << "UsbKeyCheck - destroyed";
}

void UsbKeyCheck::setSettingsManager(config::SettingsManager* settings)
{
    m_settings = settings;
}

bool UsbKeyCheck::isKeyPresent() const
{
    qDebug() << "UsbKeyCheck - checking for key on all USB drives";

    // Get list of USB drives
    QStringList usbDrives = getUsbDrives();

    for (const QString& drive : usbDrives) {
        // Check the drive root
        if (checkHiddenKey(drive)) {
            qDebug() << "UsbKeyCheck - valid key found on" << drive;
            return true;
        }

        // Search subdirectories for .keyindex files
        QStringList keyDirs = findKeyIndexDirs(drive);
        for (const QString& dir : keyDirs) {
            if (checkHiddenKey(dir)) {
                qDebug() << "UsbKeyCheck - valid key found in" << dir;
                return true;
            }
        }
    }

    // Also check custom search paths
    for (const QString& path : m_searchPaths) {
        QStringList keyDirs = findKeyIndexDirs(path);
        for (const QString& dir : keyDirs) {
            if (checkHiddenKey(dir)) {
                qDebug() << "UsbKeyCheck - valid key found in" << dir;
                return true;
            }
        }
    }

    qDebug() << "UsbKeyCheck - no valid key found";
    return false;
}

bool UsbKeyCheck::checkPath(const QString& path) const
{
    return checkHiddenKey(path);
}

bool UsbKeyCheck::checkHiddenKey(const QString& usbPath) const
{
    QDir dir(usbPath);
    if (!dir.exists()) {
        return false;
    }

    // Check for the index file
    QString indexPath = dir.filePath(INDEX_FILE_NAME);
    QFile indexFile(indexPath);

    if (!indexFile.exists()) {
        return false;
    }

    if (!indexFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "UsbKeyCheck - cannot open index file:" << indexPath;
        return false;
    }

    // Read the relative path to the key file
    QString relativeKeyPath = QString::fromUtf8(indexFile.readLine()).trimmed();
    indexFile.close();

    if (relativeKeyPath.isEmpty()) {
        qWarning() << "UsbKeyCheck - empty index file";
        return false;
    }

    // Handle Windows-style paths on Linux/macOS
#ifndef Q_OS_WIN
    relativeKeyPath.replace('\\', '/');
#endif

    // Construct absolute path to key file
    QString keyFilePath = dir.filePath(relativeKeyPath);

    // Verify the key file
    return verifyKeyFile(keyFilePath);
}

QString UsbKeyCheck::indexFileName()
{
    return QString(INDEX_FILE_NAME);
}

void UsbKeyCheck::addSearchPath(const QString& path)
{
    if (!m_searchPaths.contains(path)) {
        m_searchPaths.append(path);
    }
}

void UsbKeyCheck::clearSearchPaths()
{
    m_searchPaths.clear();
}

QStringList UsbKeyCheck::searchPaths() const
{
    return m_searchPaths;
}

QStringList UsbKeyCheck::findKeyIndexDirs(const QString& baseDir) const
{
    QStringList keyDirs;

    QDir dir(baseDir);
    if (!dir.exists()) {
        return keyDirs;
    }

    // Recursively search for .keyindex files
    QDirIterator it(baseDir, QStringList() << INDEX_FILE_NAME,
                    QDir::Files | QDir::Hidden,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        keyDirs.append(it.fileInfo().absolutePath());
    }

    return keyDirs;
}

bool UsbKeyCheck::verifyKeyFile(const QString& keyFilePath) const
{
    QFile keyFile(keyFilePath);

    if (!keyFile.exists()) {
        qDebug() << "UsbKeyCheck - key file not found:" << keyFilePath;
        return false;
    }

    if (!keyFile.open(QIODevice::ReadOnly)) {
        qWarning() << "UsbKeyCheck - cannot open key file:" << keyFilePath;
        return false;
    }

    QByteArray keyData = keyFile.readAll();
    keyFile.close();

    if (keyData.isEmpty()) {
        qWarning() << "UsbKeyCheck - empty key file";
        return false;
    }

    QString usbKey;
    QByteArray expectedValue;
    resolveUsbConfig(m_settings, usbKey, expectedValue);

    const bool verified = verifyFernetToken(keyData, usbKey, expectedValue);
    if (!verified) {
        qDebug() << "UsbKeyCheck - Fernet verification failed for key file:" << keyFilePath;
    }
    return verified;
}

QStringList UsbKeyCheck::getUsbDrives() const
{
    QStringList drives;

#ifdef Q_OS_WIN
    // Windows: Check for removable drives
    DWORD driveMask = GetLogicalDrives();
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        if (driveMask & 1) {
            QString drivePath = QString("%1:\\").arg(letter);
            UINT driveType = GetDriveTypeA(drivePath.toLocal8Bit().constData());
            // DRIVE_REMOVABLE = 2
            if (driveType == DRIVE_REMOVABLE) {
                drives.append(drivePath);
            }
        }
        driveMask >>= 1;
    }
#else
    // Linux/macOS: Use QStorageInfo to find mounted volumes
    for (const QStorageInfo& storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) {
            continue;
        }

        QString rootPath = storage.rootPath();

        // Check if it's a removable drive (mounted under /media, /mnt, or /run/media)
        if (rootPath.startsWith("/media/") ||
            rootPath.startsWith("/mnt/") ||
            rootPath.startsWith("/run/media/")) {
            drives.append(rootPath);
        }
    }
#endif

    qDebug() << "UsbKeyCheck - found USB drives:" << drives;
    return drives;
}

} // namespace system
} // namespace multipack
