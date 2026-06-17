/**
 * @file AutoUpdater.cpp
 * @brief Implementation of comprehensive automatic updater
 */

#include "multipack/system/AutoUpdater.h"
#include "multipack/config/ConfigDefaults.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#endif

namespace multipack {
namespace system {

namespace {
constexpr const char* kLatestReleaseUrl =
    "https://api.github.com/repos/Snupai/MultipackParser/releases/latest";
constexpr const char* kPackageAssetName = "multipack-parser-arm64.tar.gz";
constexpr const char* kManifestAssetName = "multipack-parser-arm64-manifest.json";
constexpr const char* kSignatureAssetName = "multipack-parser-arm64-manifest.sig";
constexpr int kFetchTimeoutMs = 30000;
constexpr const char* kEmbeddedUpdatePublicKeyPem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MCowBQYDK2VwAyEADfoYxtNm/GAOuTbuRNLuI0o4WNLtNc9jG2VZrf7bHXg=\n"
    "-----END PUBLIC KEY-----\n";

QString sanitizeForFileName(const QString& value)
{
    QString sanitized = value;
    sanitized.replace(QRegularExpression("[^A-Za-z0-9._-]"), "_");
    return sanitized;
}
} // namespace

AutoUpdater::AutoUpdater(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_checkReply(nullptr)
    , m_downloadReply(nullptr)
    , m_checkTimer(new QTimer(this))
    , m_status(UpdateStatus::Idle)
    , m_updateCacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/multipack-updates")
    , m_usbUpdateDir(config::Defaults::defaultUsbPath())
{
    qDebug() << "AutoUpdater - initialized";

    initializeNetwork();

    m_checkTimer->setSingleShot(true);
    m_checkTimer->setInterval(m_checkIntervalHours * 3600 * 1000);
    connect(m_checkTimer, &QTimer::timeout, this, &AutoUpdater::startGitHubCheck);

    QDir cacheDir(m_updateCacheDir);
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }

    m_currentVersion = config::Defaults::VERSION;
}

AutoUpdater::~AutoUpdater()
{
    if (m_checkReply) {
        m_checkReply->abort();
        m_checkReply->deleteLater();
    }

    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
    }

    delete m_checkTimer;
    delete m_networkManager;

    qDebug() << "AutoUpdater - destroyed";
}

bool AutoUpdater::checkForUpdates()
{
    if (m_status != UpdateStatus::Idle) {
        qWarning() << "AutoUpdater: Cannot check for updates, operation in progress";
        return false;
    }

    startGitHubCheck();
    return true;
}

bool AutoUpdater::checkForUsbUpdates(const QString& usbDirectory)
{
    if (m_status != UpdateStatus::Idle) {
        qWarning() << "AutoUpdater: Cannot check USB updates, operation in progress";
        return false;
    }

    setStatus(UpdateStatus::Checking);
    emit checkStarted();

    UpdateInfo usbInfo;
    QString errorMessage;
    if (!loadAndVerifyManifestFromDirectory(usbDirectory, usbInfo, errorMessage)) {
        setStatus(UpdateStatus::Failed);
        emit checkFailed(errorMessage);
        emit checkCompleted(false, errorMessage);
        return false;
    }

    if (compareVersions(usbInfo.version, m_currentVersion) <= 0) {
        clearAvailableUpdate();
        setStatus(UpdateStatus::Idle);
        emit checkCompleted(true, tr("No newer USB update found"));
        return true;
    }

    m_availableUpdate = usbInfo;
    setStatus(UpdateStatus::Idle);
    emit checkCompleted(true, tr("USB update to version %1 available").arg(usbInfo.version));
    return true;
}

bool AutoUpdater::downloadAndInstallUpdate()
{
    if (m_availableUpdate.version.isEmpty()) {
        qWarning() << "AutoUpdater: No update available to download";
        return false;
    }

    if (m_status != UpdateStatus::Idle) {
        qWarning() << "AutoUpdater: Cannot download, operation in progress";
        return false;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr,
        tr("Update Available"),
        tr("Version %1 is available. Would you like to download and install it?")
            .arg(m_availableUpdate.version),
        QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
        QMessageBox::StandardButton::No);

    if (reply != QMessageBox::StandardButton::Yes) {
        qDebug() << "AutoUpdater: User cancelled update";
        return false;
    }

    QString targetName = QString("update-%1.tar.gz").arg(sanitizeForFileName(m_availableUpdate.version));
    QString targetPath = QDir(m_updateCacheDir).filePath(targetName);

    if (!m_availableUpdate.localFilePath.isEmpty()) {
        QString verifyError;
        if (!verifyFileSha256(m_availableUpdate.localFilePath, m_availableUpdate.sha256, verifyError)) {
            setStatus(UpdateStatus::Failed);
            emit updateFailed(tr("USB update verification failed: %1").arg(verifyError));
            return false;
        }

        setStatus(UpdateStatus::Downloading);
        QFileInfo sourceInfo(m_availableUpdate.localFilePath);
        m_progress.fileName = sourceInfo.fileName();
        m_progress.bytesTotal = sourceInfo.size();
        m_progress.bytesReceived = sourceInfo.size();
        m_progress.statusText = tr("Copying update from USB...");

        emit downloadStarted(m_progress.fileName);
        emit downloadProgress(m_progress);

        if (QFile::exists(targetPath)) {
            QFile::remove(targetPath);
        }

        if (!QFile::copy(m_availableUpdate.localFilePath, targetPath)) {
            setStatus(UpdateStatus::Failed);
            emit updateFailed(tr("Failed to copy update from USB"));
            return false;
        }

        emit downloadCompleted(m_progress.fileName);

        setStatus(UpdateStatus::Installing);
        if (!installUpdate(targetPath)) {
            setStatus(UpdateStatus::Failed);
            emit installationCompleted(false, tr("Failed to install update"));
            return false;
        }

        setStatus(UpdateStatus::Finished);
        emit installationCompleted(true, tr("Update installed successfully"));
        cleanupOldUpdates();
        return true;
    }

    UpdateInfo verifiedInfo = m_availableUpdate;
    QString manifestError;
    if (!loadAndVerifyManifestFromUrls(verifiedInfo.manifestUrl,
                                       verifiedInfo.signatureUrl,
                                       verifiedInfo,
                                       manifestError)) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Signed manifest verification failed: %1").arg(manifestError));
        return false;
    }

    m_availableUpdate = verifiedInfo;

    if (m_availableUpdate.downloadUrl.isEmpty()) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("No package download URL found in manifest"));
        return false;
    }

    setStatus(UpdateStatus::Downloading);

    if (!downloadUpdateFile(m_availableUpdate.downloadUrl, targetPath)) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Failed to start update download"));
        return false;
    }

    return true;
}

void AutoUpdater::cancelUpdate()
{
    if (m_checkReply) {
        m_checkReply->abort();
        m_checkReply->deleteLater();
        m_checkReply = nullptr;
    }

    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }

    if (m_checkTimer->isActive()) {
        m_checkTimer->stop();
    }

    setStatus(UpdateStatus::Idle);
    emit updateCancelled();

    qDebug() << "AutoUpdater: Update cancelled";
}

UpdateStatus AutoUpdater::currentStatus() const
{
    return m_status;
}

UpdateProgress AutoUpdater::currentProgress() const
{
    return m_progress;
}

UpdateInfo AutoUpdater::availableUpdate() const
{
    return m_availableUpdate;
}

QString AutoUpdater::currentVersion() const
{
    return m_currentVersion;
}

void AutoUpdater::setCheckInterval(int hours)
{
    m_checkIntervalHours = hours;

    if (m_checkTimer->isActive()) {
        m_checkTimer->stop();
        m_checkTimer->setInterval(hours * 3600 * 1000);
        m_checkTimer->start();
    }

    qDebug() << "AutoUpdater: Check interval set to" << hours << "hours";
}

void AutoUpdater::setUsbUpdatePath(const QString& usbDirectory)
{
    m_usbUpdateDir = usbDirectory;
}

QString AutoUpdater::usbUpdatePath() const
{
    return m_usbUpdateDir;
}

void AutoUpdater::showUpdateDialog()
{
    if (m_availableUpdate.version.isEmpty()) {
        QMessageBox::information(nullptr,
                                 tr("No Updates Available"),
                                 tr("You are running the latest version (%1)").arg(m_currentVersion));
        return;
    }

    double sizeMB = static_cast<double>(m_availableUpdate.sizeBytes) / (1024.0 * 1024.0);
    QString details = tr("New Version: %1\n%2\nSize: %3 MB\nSignature verified: %4")
                          .arg(m_availableUpdate.version)
                          .arg(m_availableUpdate.releaseNotes)
                          .arg(QString::number(sizeMB, 'f', 1))
                          .arg(m_availableUpdate.signatureVerified ? tr("Yes") : tr("No"));

    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr,
        tr("Software Update Available"),
        details,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::StandardButton::Yes);

    if (reply == QMessageBox::Yes) {
        downloadAndInstallUpdate();
    }
}

void AutoUpdater::initializeNetwork()
{
    m_networkManager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    m_networkManager->setStrictTransportSecurityEnabled(true);
}

void AutoUpdater::startGitHubCheck()
{
    setStatus(UpdateStatus::Checking);
    emit checkStarted();

    QNetworkRequest request;
    request.setUrl(QUrl(kLatestReleaseUrl));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "MultipackParser-Updater");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    QNetworkReply* reply = m_networkManager->get(request);
    m_checkReply = reply;
    connect(reply, &QNetworkReply::finished,
            this, [this, reply]() { onCheckNetworkReply(reply); });

    qDebug() << "AutoUpdater: Checking for updates via GitHub API";
}

bool AutoUpdater::parseGitHubResponse(const QByteArray& data)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "AutoUpdater: JSON parse error:" << parseError.errorString();
        emit checkFailed(tr("Invalid response from update server"));
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "AutoUpdater: Expected JSON object in response";
        emit checkFailed(tr("Invalid response format from update server"));
        return false;
    }

    QString message;
    if (!parseReleaseObject(doc.object(), message)) {
        emit checkFailed(message);
        return false;
    }

    emit checkCompleted(true, message);
    return true;
}

bool AutoUpdater::parseReleaseObject(const QJsonObject& release, QString& errorMessage)
{
    const QString tagName = release.value("tag_name").toString();
    const QString latestVersion = normalizeVersion(tagName);
    if (latestVersion.isEmpty()) {
        errorMessage = tr("Release version is missing in update metadata");
        return false;
    }

    if (compareVersions(latestVersion, m_currentVersion) <= 0) {
        clearAvailableUpdate();
        errorMessage = tr("You are running the latest version");
        return true;
    }

    QString packageUrl;
    QString manifestUrl;
    QString signatureUrl;
    qint64 packageSize = 0;

    const QJsonArray assets = release.value("assets").toArray();
    for (const QJsonValue& assetValue : assets) {
        const QJsonObject asset = assetValue.toObject();
        const QString name = asset.value("name").toString();
        const QString browserUrl = asset.value("browser_download_url").toString();

        if (name == QLatin1String(kPackageAssetName)) {
            packageUrl = browserUrl;
            packageSize = asset.value("size").toVariant().toLongLong();
        } else if (name == QLatin1String(kManifestAssetName)) {
            manifestUrl = browserUrl;
        } else if (name == QLatin1String(kSignatureAssetName)) {
            signatureUrl = browserUrl;
        }
    }

    if (packageUrl.isEmpty()) {
        clearAvailableUpdate();
        errorMessage = tr("Latest release does not contain an ARM64 package");
        return true;
    }

    if (manifestUrl.isEmpty() || signatureUrl.isEmpty()) {
        clearAvailableUpdate();
        errorMessage = tr("Latest release is missing signed update metadata");
        return true;
    }

    m_availableUpdate.version = latestVersion;
    m_availableUpdate.releaseNotes = release.value("body").toString();
    m_availableUpdate.downloadUrl = packageUrl;
    m_availableUpdate.sizeBytes = packageSize;
    m_availableUpdate.manifestUrl = manifestUrl;
    m_availableUpdate.signatureUrl = signatureUrl;
    m_availableUpdate.packageFileName = kPackageAssetName;
    m_availableUpdate.localFilePath.clear();
    m_availableUpdate.sha256.clear();
    m_availableUpdate.signatureVerified = false;
    m_availableUpdate.isPrerelease = release.value("prerelease").toBool(false);

    errorMessage = tr("Update to version %1 available").arg(latestVersion);
    return true;
}

bool AutoUpdater::loadAndVerifyManifestFromUrls(const QString& manifestUrl,
                                                const QString& signatureUrl,
                                                UpdateInfo& info,
                                                QString& errorMessage)
{
    if (manifestUrl.isEmpty() || signatureUrl.isEmpty()) {
        errorMessage = tr("Manifest or signature URL is missing");
        return false;
    }

    QByteArray manifestBytes;
    if (!fetchUrlSync(QUrl(manifestUrl), manifestBytes, errorMessage)) {
        return false;
    }

    QByteArray signatureBytes;
    if (!fetchUrlSync(QUrl(signatureUrl), signatureBytes, errorMessage)) {
        return false;
    }

    if (!verifyManifestSignature(manifestBytes, signatureBytes, errorMessage)) {
        return false;
    }

    UpdateInfo manifestInfo;
    if (!parseManifest(manifestBytes, manifestInfo, errorMessage)) {
        return false;
    }

    if (!info.version.isEmpty() && compareVersions(manifestInfo.version, info.version) != 0) {
        errorMessage = tr("Manifest version does not match release tag");
        return false;
    }

    if (manifestInfo.downloadUrl.isEmpty()) {
        manifestInfo.downloadUrl = info.downloadUrl;
    }

    if (manifestInfo.downloadUrl.isEmpty()) {
        errorMessage = tr("Manifest does not provide package download URL");
        return false;
    }

    if (manifestInfo.releaseNotes.isEmpty()) {
        manifestInfo.releaseNotes = info.releaseNotes;
    }

    manifestInfo.manifestUrl = manifestUrl;
    manifestInfo.signatureUrl = signatureUrl;
    manifestInfo.localFilePath.clear();
    manifestInfo.signatureVerified = true;

    info = manifestInfo;
    return true;
}

bool AutoUpdater::loadAndVerifyManifestFromDirectory(const QString& directory,
                                                     UpdateInfo& info,
                                                     QString& errorMessage)
{
    if (directory.isEmpty()) {
        errorMessage = tr("USB update directory is empty");
        return false;
    }

    QDir rootDir(directory);
    if (!rootDir.exists()) {
        errorMessage = tr("USB update directory does not exist: %1").arg(directory);
        return false;
    }

    const QString manifestPath = rootDir.filePath(kManifestAssetName);
    const QString signaturePath = rootDir.filePath(kSignatureAssetName);

    if (!QFile::exists(manifestPath) || !QFile::exists(signaturePath)) {
        const QString manifestInUpdates = rootDir.filePath(QStringLiteral("updates/%1").arg(kManifestAssetName));
        const QString signatureInUpdates = rootDir.filePath(QStringLiteral("updates/%1").arg(kSignatureAssetName));

        if (!QFile::exists(manifestInUpdates) || !QFile::exists(signatureInUpdates)) {
            errorMessage = tr("No signed update metadata found in USB directory");
            return false;
        }

        return loadAndVerifyManifestFromDirectory(rootDir.filePath("updates"), info, errorMessage);
    }

    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        errorMessage = tr("Cannot read manifest file");
        return false;
    }
    const QByteArray manifestBytes = manifestFile.readAll();

    QFile signatureFile(signaturePath);
    if (!signatureFile.open(QIODevice::ReadOnly)) {
        errorMessage = tr("Cannot read signature file");
        return false;
    }
    const QByteArray signatureBytes = signatureFile.readAll();

    if (!verifyManifestSignature(manifestBytes, signatureBytes, errorMessage)) {
        return false;
    }

    UpdateInfo localInfo;
    if (!parseManifest(manifestBytes, localInfo, errorMessage)) {
        return false;
    }

    if (localInfo.packageFileName.isEmpty()) {
        errorMessage = tr("Manifest does not contain a package filename");
        return false;
    }

    QString packagePath = rootDir.filePath(localInfo.packageFileName);
    if (!QFile::exists(packagePath)) {
        errorMessage = tr("Package referenced by manifest was not found: %1")
                           .arg(localInfo.packageFileName);
        return false;
    }

    localInfo.localFilePath = QFileInfo(packagePath).absoluteFilePath();
    localInfo.downloadUrl.clear();
    localInfo.manifestUrl = manifestPath;
    localInfo.signatureUrl = signaturePath;
    localInfo.signatureVerified = true;

    if (localInfo.sizeBytes <= 0) {
        localInfo.sizeBytes = QFileInfo(localInfo.localFilePath).size();
    }

    info = localInfo;
    return true;
}

bool AutoUpdater::parseManifest(const QByteArray& manifestBytes,
                                UpdateInfo& info,
                                QString& errorMessage) const
{
    QJsonParseError parseError;
    QJsonDocument manifestDoc = QJsonDocument::fromJson(manifestBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !manifestDoc.isObject()) {
        errorMessage = tr("Manifest is not valid JSON");
        return false;
    }

    const QJsonObject root = manifestDoc.object();

    const QString version = normalizeVersion(root.value("version").toString());
    if (version.isEmpty()) {
        errorMessage = tr("Manifest is missing version");
        return false;
    }

    const QJsonObject package = root.value("package").toObject();
    if (package.isEmpty()) {
        errorMessage = tr("Manifest is missing package section");
        return false;
    }

    const QString packageFile = package.value("file").toString().trimmed();
    const QString packageSha = package.value("sha256").toString().trimmed().toLower();
    const QString packageUrl = package.value("url").toString().trimmed();

    if (packageFile.isEmpty()) {
        errorMessage = tr("Manifest package filename is empty");
        return false;
    }

    if (!QRegularExpression("^[0-9a-f]{64}$").match(packageSha).hasMatch()) {
        errorMessage = tr("Manifest package SHA-256 is invalid");
        return false;
    }

    info.version = version;
    info.releaseNotes = root.value("release_notes").toString();
    info.packageFileName = packageFile;
    info.downloadUrl = packageUrl;
    info.sha256 = packageSha;
    info.sizeBytes = package.value("size").toVariant().toLongLong();
    info.localFilePath.clear();
    info.signatureVerified = false;

    return true;
}

bool AutoUpdater::verifyManifestSignature(const QByteArray& manifestBytes,
                                          const QByteArray& signatureBytes,
                                          QString& errorMessage) const
{
    QByteArray signatureB64;
    QJsonParseError sigParseError;
    QJsonDocument sigDoc = QJsonDocument::fromJson(signatureBytes, &sigParseError);

    if (sigParseError.error == QJsonParseError::NoError && sigDoc.isObject()) {
        const QJsonObject sigObj = sigDoc.object();
        const QString algorithm = sigObj.value("algorithm").toString().toLower();
        if (!algorithm.isEmpty() && algorithm != QLatin1String("ed25519")) {
            errorMessage = tr("Unsupported update signature algorithm");
            return false;
        }

        const QString manifestSha = sigObj.value("manifest_sha256").toString().toLower();
        if (!manifestSha.isEmpty()) {
            const QString currentSha = QString(QCryptographicHash::hash(manifestBytes, QCryptographicHash::Sha256).toHex());
            if (manifestSha != currentSha) {
                errorMessage = tr("Manifest hash does not match signature metadata");
                return false;
            }
        }

        signatureB64 = sigObj.value("signature").toString().toUtf8();
    } else {
        signatureB64 = signatureBytes.trimmed();
    }

    const QByteArray signatureRaw = QByteArray::fromBase64(signatureB64.trimmed());
    if (signatureRaw.isEmpty()) {
        errorMessage = tr("Signature payload is invalid base64");
        return false;
    }

    QString keyError;
    const QString publicKeyPem = trustedPublicKeyPem(keyError);
    if (publicKeyPem.isEmpty()) {
        errorMessage = keyError;
        return false;
    }

#ifdef HAVE_OPENSSL
    BIO* bio = BIO_new_mem_buf(publicKeyPem.constData(), static_cast<int>(publicKeyPem.size()));
    if (!bio) {
        errorMessage = tr("Failed to initialize OpenSSL BIO");
        return false;
    }

    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        errorMessage = tr("Failed to parse update public key");
        return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        errorMessage = tr("Failed to initialize OpenSSL verification context");
        return false;
    }

    int ok = EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey);
    if (ok != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        errorMessage = tr("Failed to initialize Ed25519 verification");
        return false;
    }

    ok = EVP_DigestVerify(ctx,
                          reinterpret_cast<const unsigned char*>(signatureRaw.constData()),
                          static_cast<size_t>(signatureRaw.size()),
                          reinterpret_cast<const unsigned char*>(manifestBytes.constData()),
                          static_cast<size_t>(manifestBytes.size()));

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    if (ok != 1) {
        errorMessage = tr("Manifest signature verification failed");
        return false;
    }

    return true;
#else
    Q_UNUSED(manifestBytes);
    Q_UNUSED(signatureRaw);
    errorMessage = tr("OpenSSL support is required for signed updates");
    return false;
#endif
}

bool AutoUpdater::verifyFileSha256(const QString& filePath,
                                   const QString& expectedSha256,
                                   QString& errorMessage) const
{
    if (expectedSha256.isEmpty()) {
        errorMessage = tr("No expected SHA-256 value provided");
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = tr("Cannot open package for hash verification");
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        errorMessage = tr("Failed to calculate package SHA-256");
        return false;
    }

    const QString actualSha = QString(hash.result().toHex()).toLower();
    if (actualSha != expectedSha256.toLower()) {
        errorMessage = tr("Package SHA-256 mismatch");
        return false;
    }

    return true;
}

bool AutoUpdater::fetchUrlSync(const QUrl& url, QByteArray& data, QString& errorMessage) const
{
    if (!url.isValid()) {
        errorMessage = tr("Invalid URL: %1").arg(url.toString());
        return false;
    }

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/octet-stream");
    request.setRawHeader("User-Agent", "MultipackParser-Updater");

    QNetworkReply* reply = m_networkManager->get(request);

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });

    timeoutTimer.start(kFetchTimeoutMs);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        errorMessage = tr("Network fetch failed for %1: %2")
                           .arg(url.toString(), reply->errorString());
        reply->deleteLater();
        return false;
    }

    data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        errorMessage = tr("Downloaded data is empty for %1").arg(url.toString());
        return false;
    }

    return true;
}

QString AutoUpdater::trustedPublicKeyPem(QString& errorMessage) const
{
    Q_UNUSED(errorMessage);

    const QString keyFromEnv = qEnvironmentVariable("MULTIPACK_UPDATE_PUBLIC_KEY_PEM");
    if (!keyFromEnv.trimmed().isEmpty()) {
        return keyFromEnv;
    }

    QStringList keyPaths;

    const QString envPath = qEnvironmentVariable("MULTIPACK_UPDATE_PUBLIC_KEY_FILE");
    if (!envPath.trimmed().isEmpty()) {
        keyPaths.append(envPath);
    }

    keyPaths.append(QCoreApplication::applicationDirPath() + "/update-public-key.pem");
    keyPaths.append(QDir::current().filePath("update-public-key.pem"));

    for (const QString& path : keyPaths) {
        QFile file(path);
        if (!file.exists()) {
            continue;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        const QString pem = QString::fromUtf8(file.readAll());
        if (pem.contains("BEGIN PUBLIC KEY")) {
            return pem;
        }
    }

    return QString::fromLatin1(kEmbeddedUpdatePublicKeyPem);
}

QString AutoUpdater::normalizeVersion(const QString& version) const
{
    QString normalized = version.trimmed();
    if (normalized.startsWith('v')) {
        normalized = normalized.mid(1);
    }

    const int dashIndex = normalized.indexOf('-');
    if (dashIndex > 0) {
        normalized = normalized.left(dashIndex);
    }

    return normalized;
}

void AutoUpdater::clearAvailableUpdate()
{
    m_availableUpdate = UpdateInfo{};
}

int AutoUpdater::compareVersions(const QString& v1, const QString& v2)
{
    const QString ver1 = normalizeVersion(v1);
    const QString ver2 = normalizeVersion(v2);

    const QStringList parts1 = ver1.split('.');
    const QStringList parts2 = ver2.split('.');

    const int maxLen = qMax(parts1.size(), parts2.size());
    for (int i = 0; i < maxLen; ++i) {
        bool ok1 = false;
        bool ok2 = false;
        const int num1 = (i < parts1.size()) ? parts1[i].toInt(&ok1) : 0;
        const int num2 = (i < parts2.size()) ? parts2[i].toInt(&ok2) : 0;

        const int a = ok1 ? num1 : 0;
        const int b = ok2 ? num2 : 0;

        if (a < b) {
            return -1;
        }
        if (a > b) {
            return 1;
        }
    }

    return 0;
}

bool AutoUpdater::downloadUpdateFile(const QString& url, const QString& filePath)
{
    if (url.isEmpty()) {
        qWarning() << "AutoUpdater: Download URL is empty";
        return false;
    }

    emit downloadStarted(QFileInfo(filePath).fileName());

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("User-Agent", QString("MultipackParser-Updater/%1").arg(m_currentVersion).toUtf8());
    request.setRawHeader("Accept", "application/octet-stream");

    m_downloadTargetPath = filePath;
    QNetworkReply* reply = m_networkManager->get(request);
    m_downloadReply = reply;
    connect(reply, &QNetworkReply::downloadProgress,
            this, &AutoUpdater::onDownloadProgress);
    connect(reply, &QNetworkReply::finished,
            this, [this, reply]() { onDownloadNetworkReply(reply); });

    m_progress.fileName = QFileInfo(filePath).fileName();
    m_progress.bytesTotal = 0;
    m_progress.bytesReceived = 0;
    m_progress.statusText = tr("Downloading %1...").arg(m_progress.fileName);

    emit downloadProgress(m_progress);

    qDebug() << "AutoUpdater: Downloading update from" << url;
    return true;
}

void AutoUpdater::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    updateProgress(bytesReceived, bytesTotal);

    const int percentage = (bytesTotal > 0) ? static_cast<int>((bytesReceived * 100) / bytesTotal) : 0;
    m_progress.statusText = tr("Downloaded %1% (%2 of %3 MB)")
                                .arg(percentage)
                                .arg(QString::number(static_cast<double>(bytesReceived) / (1024.0 * 1024.0), 'f', 1))
                                .arg(QString::number(static_cast<double>(bytesTotal) / (1024.0 * 1024.0), 'f', 1));

    emit downloadProgress(m_progress);
}

void AutoUpdater::onDownloadNetworkReply(QNetworkReply* reply)
{
    if (!reply) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Internal error: missing download reply"));
        return;
    }

    if (reply != m_downloadReply) {
        reply->deleteLater();
        return;
    }

    m_downloadReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        const QString error = tr("Download failed: %1").arg(reply->errorString());
        reply->deleteLater();
        setStatus(UpdateStatus::Failed);
        emit updateFailed(error);
        emit installationCompleted(false, error);
        return;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Downloaded file is empty"));
        emit installationCompleted(false, tr("Downloaded file is empty"));
        return;
    }

    QFileInfo targetInfo(m_downloadTargetPath);
    QDir().mkpath(targetInfo.absolutePath());

    QFile file(m_downloadTargetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Cannot save downloaded file"));
        emit installationCompleted(false, tr("Cannot save downloaded file"));
        return;
    }

    if (file.write(data) != data.size()) {
        file.close();
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Incomplete write while saving downloaded file"));
        emit installationCompleted(false, tr("Incomplete write while saving downloaded file"));
        return;
    }

    file.close();

    QString hashError;
    if (!verifyFileSha256(m_downloadTargetPath, m_availableUpdate.sha256, hashError)) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Downloaded package verification failed: %1").arg(hashError));
        emit installationCompleted(false, tr("Downloaded package verification failed"));
        return;
    }

    emit downloadCompleted(QFileInfo(m_downloadTargetPath).fileName());

    setStatus(UpdateStatus::Installing);
    if (!installUpdate(m_downloadTargetPath)) {
        setStatus(UpdateStatus::Failed);
        emit installationCompleted(false, tr("Failed to install update"));
        return;
    }

    setStatus(UpdateStatus::Finished);
    emit installationCompleted(true, tr("Update installed successfully"));
    cleanupOldUpdates();
}

bool AutoUpdater::installUpdate(const QString& filePath)
{
    emit installationStarted();

    const QString appDir = QCoreApplication::applicationDirPath();
    QString installDir = appDir;
    QString currentBinary = QDir(appDir).filePath("multipack-parser");

    // ARM64 deployment runs from <bundle>/bin/multipack-parser via run.sh.
    // Install release archives at the bundle root so lib/, plugins/, and
    // run.sh are updated together with bin/multipack-parser.
    const QFileInfo appDirInfo(appDir);
    if (appDirInfo.fileName() == "bin") {
        QDir bundleRoot(appDir);
        if (bundleRoot.cdUp() && QFileInfo(bundleRoot.filePath("run.sh")).isFile()
            && QFileInfo(bundleRoot.filePath("lib")).isDir()
            && QFileInfo(bundleRoot.filePath("plugins")).isDir()) {
            installDir = bundleRoot.absolutePath();
            currentBinary = QDir(installDir).filePath("bin/multipack-parser");
        }
    }

    const QString backupPath = appDir + "/multipack-parser.backup";

    if (QFile::exists(currentBinary)) {
        if (QFile::exists(backupPath)) {
            QFile::remove(backupPath);
        }

        if (!QFile::copy(currentBinary, backupPath)) {
            qWarning() << "AutoUpdater: Failed to create backup";
            emit updateFailed(tr("Failed to create binary backup"));
            return false;
        }
    }

    QProcess listProcess;
    listProcess.setProgram("tar");
    listProcess.setArguments({"-tzf", filePath});
    listProcess.start();
    listProcess.waitForFinished();

    QStringList extractArguments = {"-xzf", filePath, "-C", installDir};
    if (listProcess.exitStatus() == QProcess::NormalExit && listProcess.exitCode() == 0) {
        const QString archiveListing = QString::fromUtf8(listProcess.readAllStandardOutput());
        const QStringList entries = archiveListing.split('\n', Qt::SkipEmptyParts);
        bool hasEntries = false;
        bool hasBundleRoot = false;
        bool allEntriesUnderBundleRoot = true;
        for (const QString& entry : entries) {
            const QString cleanEntry = entry.trimmed();
            if (cleanEntry.isEmpty()) {
                continue;
            }

            hasEntries = true;
            if (cleanEntry == "multipack-parser-arm64/" || cleanEntry == "multipack-parser-arm64") {
                hasBundleRoot = true;
                continue;
            }
            if (cleanEntry.startsWith("multipack-parser-arm64/")) {
                hasBundleRoot = true;
                continue;
            }

            allEntriesUnderBundleRoot = false;
            break;
        }

        if (hasEntries && hasBundleRoot && allEntriesUnderBundleRoot) {
            extractArguments << "--strip-components=1";
        }
    } else {
        qWarning() << "AutoUpdater: Failed to inspect update archive before extraction"
                   << listProcess.readAllStandardError();
    }

    QProcess extractProcess;
    extractProcess.setWorkingDirectory(installDir);
    extractProcess.setProgram("tar");
    extractProcess.setArguments(extractArguments);

    qDebug() << "AutoUpdater: Extracting update" << filePath;

    extractProcess.start();
    extractProcess.waitForFinished();

    if (extractProcess.exitStatus() != QProcess::NormalExit || extractProcess.exitCode() != 0) {
        emit updateFailed(tr("Failed to extract update archive"));
        return false;
    }

    if (!QFile::setPermissions(currentBinary,
                               QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                   QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                   QFileDevice::ReadOther | QFileDevice::ExeOther)) {
        qWarning() << "AutoUpdater: Failed to set executable permissions on" << currentBinary;
    }

    qDebug() << "AutoUpdater: Installation completed";
    return true;
}

void AutoUpdater::setStatus(UpdateStatus status)
{
    m_status = status;
}

void AutoUpdater::updateProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    m_progress.bytesReceived = bytesReceived;
    m_progress.bytesTotal = bytesTotal;

    if (bytesTotal > 0) {
        const int percentage = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progress.statusText = tr("Downloaded %1% (%2 of %3 MB)")
                                    .arg(percentage)
                                    .arg(QString::number(static_cast<double>(bytesReceived) / (1024.0 * 1024.0), 'f', 1))
                                    .arg(QString::number(static_cast<double>(bytesTotal) / (1024.0 * 1024.0), 'f', 1));
    }
}

QString AutoUpdater::getUpdateCacheDir() const
{
    return m_updateCacheDir;
}

void AutoUpdater::cleanupOldUpdates()
{
    QDir cacheDir(m_updateCacheDir);
    if (!cacheDir.exists()) {
        return;
    }

    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-7);

    const QStringList nameFilters = {
        "*.tar.gz",
        "*.zip",
        "*-manifest.json",
        "*-manifest.sig"
    };

    const QFileInfoList oldFiles = cacheDir.entryInfoList(nameFilters, QDir::Files, QDir::Time);
    for (const QFileInfo& fileInfo : oldFiles) {
        if (fileInfo.lastModified() < cutoff) {
            qDebug() << "AutoUpdater: Removing old update file:" << fileInfo.fileName();
            QFile::remove(fileInfo.absoluteFilePath());
        }
    }
}

void AutoUpdater::onCheckNetworkReply(QNetworkReply* reply)
{
    if (!reply) {
        setStatus(UpdateStatus::Failed);
        emit checkFailed(tr("Internal error: missing network reply"));
        emit checkCompleted(false, tr("Internal error: missing network reply"));
        return;
    }

    if (reply != m_checkReply) {
        reply->deleteLater();
        return;
    }

    m_checkReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        const QString networkError = tr("Update check failed: %1").arg(reply->errorString());
        reply->deleteLater();

        if (!m_usbUpdateDir.trimmed().isEmpty()) {
            UpdateInfo usbInfo;
            QString usbError;
            if (loadAndVerifyManifestFromDirectory(m_usbUpdateDir, usbInfo, usbError) &&
                compareVersions(usbInfo.version, m_currentVersion) > 0) {
                m_availableUpdate = usbInfo;
                setStatus(UpdateStatus::Idle);
                emit checkCompleted(true,
                                    tr("Online check failed; USB update to version %1 available")
                                        .arg(usbInfo.version));
                return;
            }
        }

        setStatus(UpdateStatus::Failed);
        emit checkFailed(networkError);
        emit checkCompleted(false, networkError);
        return;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (parseGitHubResponse(data)) {
        setStatus(UpdateStatus::Idle);
    } else {
        setStatus(UpdateStatus::Failed);
    }
}

void AutoUpdater::onSslErrors(const QList<QSslError>& errors)
{
    if (errors.isEmpty()) {
        return;
    }

    QString errorString;
    for (const QSslError& error : errors) {
        if (!errorString.isEmpty()) {
            errorString += ", ";
        }
        errorString += error.errorString();
    }

    qWarning() << "AutoUpdater: SSL errors:" << errorString;
}

} // namespace system
} // namespace multipack
