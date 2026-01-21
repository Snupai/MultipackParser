/**
 * @file AutoUpdater.cpp
 * @brief Implementation of comprehensive automatic updater
 */

#include "multipack/system/AutoUpdater.h"
#include "multipack/config/ConfigDefaults.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QProcess>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>

namespace multipack {
namespace system {

AutoUpdater::AutoUpdater(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_checkReply(nullptr)
    , m_downloadReply(nullptr)
    , m_checkTimer(new QTimer(this))
    , m_status(UpdateStatus::Idle)
    , m_updateCacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/multipack-updates")
{
    qDebug() << "AutoUpdater - initialized";
    
    // Initialize network manager
    initializeNetwork();
    
    // Setup check timer
    m_checkTimer->setSingleShot(true);
    m_checkTimer->setInterval(m_checkIntervalHours * 3600 * 1000); // Convert hours to milliseconds
    connect(m_checkTimer, &QTimer::timeout, this, &AutoUpdater::startGitHubCheck);
    
    // Ensure update cache directory exists
    QDir cacheDir(m_updateCacheDir);
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }
    
    // Load current version
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

bool AutoUpdater::downloadAndInstallUpdate()
{
    if (!m_availableUpdate.version.isEmpty()) {
        qWarning() << "AutoUpdater: No update available to download";
        return false;
    }
    
    if (m_status != UpdateStatus::Idle) {
        qWarning() << "AutoUpdater: Cannot download, operation in progress";
        return false;
    }
    
    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr,
        tr("Update Available"),
        tr("Version %1 is available. Would you like to download and install it?").arg(m_availableUpdate.version),
        QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
        QMessageBox::StandardButton::NoButton
    );
    
    if (reply != QMessageBox::StandardButton::Yes) {
        qDebug() << "AutoUpdater: User cancelled update";
        return false;
    }
    
    setStatus(UpdateStatus::Downloading);
    
    // Download update file
    QString updateFileName = QString("update-%1.%2").arg(m_availableUpdate.version, "tar.gz");
    QString updateFilePath = QDir(m_updateCacheDir).filePath(updateFileName);
    
    if (!downloadUpdateFile(m_availableUpdate.downloadUrl, updateFilePath)) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Failed to download update file"));
        return false;
    }
    
    setStatus(UpdateStatus::Installing);
    
    // Install update
    if (!installUpdate(updateFilePath)) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Failed to install update"));
        return false;
    }
    
    setStatus(UpdateStatus::Finished);
    emit installationCompleted(true, tr("Update installed successfully"));
    
    // Clean up
    cleanupOldUpdates();
    
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
        m_checkTimer->setInterval(hours * 3600 * 1000); // Convert to milliseconds
        m_checkTimer->start();
    }
    
    qDebug() << "AutoUpdater: Check interval set to" << hours << "hours";
}

void AutoUpdater::showUpdateDialog()
{
    if (m_availableUpdate.version.isEmpty()) {
        QMessageBox::information(nullptr, 
            tr("No Updates Available"), 
            tr("You are running the latest version (%1)").arg(m_currentVersion));
        return;
    }
    
    double sizeMB = static_cast<double>(m_availableUpdate.sizeBytes) / (1024 * 1024);
    QString details = tr("New Version: %1\n%2\nSize: %3 MB\nRequired: %4")
        .arg(m_availableUpdate.version)
        .arg(m_availableUpdate.releaseNotes)
        .arg(QString::number(sizeMB, 'f', 1))
        .arg(m_availableUpdate.isRequired ? tr("Yes") : tr("No"));
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr,
        tr("Software Update Available"),
        details,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::StandardButton::Yes
    );
    
    if (reply == QMessageBox::Yes) {
        downloadAndInstallUpdate();
    }
}

void AutoUpdater::initializeNetwork()
{
    // Configure network manager for reliability
    m_networkManager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    m_networkManager->setStrictTransportSecurityEnabled(true);
}

void AutoUpdater::startGitHubCheck()
{
    setStatus(UpdateStatus::Checking);
    emit checkStarted();
    
    QNetworkRequest request;
    request.setUrl(QUrl("https://api.github.com/repos/szaidel-gmbh/MultipackParser/releases"));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    
    m_checkReply = m_networkManager->get(request);
    connect(m_checkReply, &QNetworkReply::finished,
            this, [this]() { onCheckNetworkReply(m_checkReply); });
    
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
    
    if (!doc.isArray()) {
        qWarning() << "AutoUpdater: Expected JSON array in response";
        emit checkFailed(tr("Invalid response format from update server"));
        return false;
    }
    
    QJsonArray releases = doc.array();
    
    // Find the latest release (excluding prereleases unless current is also prerelease)
    bool currentIsPrerelease = m_currentVersion.contains("alpha") || m_currentVersion.contains("beta") || m_currentVersion.contains("rc");
    
    for (int i = 0; i < releases.size(); ++i) {
        QJsonObject release = releases[i].toObject();
        
        QString tagName = release["tag_name"].toString();
        QString version = tagName.startsWith('v') ? tagName.mid(1) : tagName;
        
        bool isPrerelease = release["prerelease"].toBool(false);
        
        // Skip prereleases unless current version is also prerelease
        if (isPrerelease && !currentIsPrerelease) {
            continue;
        }
        
        // Find the first release with a different version
        if (compareVersions(version, m_currentVersion) > 0) {
            m_availableUpdate.version = version;
            m_availableUpdate.releaseNotes = release["body"].toString();
            m_availableUpdate.downloadUrl = release["assets"].toArray()[0].toObject()["browser_download_url"].toString();
            m_availableUpdate.sizeBytes = release["assets"].toArray()[0].toObject()["size"].toVariant().toLongLong();
            m_availableUpdate.sha256 = release["assets"].toArray()[0].toObject()["name"].toString();
            m_availableUpdate.isRequired = !release.contains("name") || !release["name"].isString();
            
            // Check if this release has assets (our actual files)
            QJsonArray assets = release["assets"].toArray();
            bool hasUpdateFiles = false;
            for (int j = 0; j < assets.size(); ++j) {
                QJsonObject asset = assets[j].toObject();
                QString fileName = asset["name"].toString();
                if (fileName.contains(".tar.gz") || fileName.contains(".zip")) {
                    hasUpdateFiles = true;
                    break;
                }
            }
            
            if (!hasUpdateFiles) {
                qWarning() << "AutoUpdater: Release found but no update files";
                continue;
            }
            
            qDebug() << "AutoUpdater: Update available - version" << version;
            emit checkCompleted(true, tr("Update to version %1 available").arg(version));
            return true;
        }
    }
    
    // No newer version found
    emit checkCompleted(true, tr("You are running the latest version"));
    return false;
}

int AutoUpdater::compareVersions(const QString& v1, const QString& v2)
{
    // Simple version comparison (major.minor.patch)
    QStringList parts1 = v1.split('.');
    QStringList parts2 = v2.split('.');
    
    int maxLen = qMax(parts1.size(), parts2.size());
    
    for (int i = 0; i < maxLen; ++i) {
        int num1 = (i < parts1.size()) ? parts1[i].toInt() : 0;
        int num2 = (i < parts2.size()) ? parts2[i].toInt() : 0;
        
        if (num1 < num2) return -1;
        if (num1 > num2) return 1;
    }
    
    return 0;
}

bool AutoUpdater::downloadUpdateFile(const QString& url, const QString& filePath)
{
    emit downloadStarted(QFileInfo(filePath).fileName());
    
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("User-Agent", QString("MultipackParser-Updater/%1").arg(m_currentVersion).toUtf8());
    request.setRawHeader("Accept", "application/octet-stream");
    
    m_downloadReply = m_networkManager->get(request);
    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &AutoUpdater::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished,
            this, [this]() { onDownloadNetworkReply(m_downloadReply); });
    
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
    
    int percentage = (bytesTotal > 0) ? (bytesReceived * 100) / bytesTotal : 0;
    m_progress.statusText = tr("Downloaded %1% (%2 of %3 MB)")
        .arg(percentage)
        .arg(QString::number(static_cast<double>(bytesReceived) / (1024 * 1024), 'f', 1))
        .arg(QString::number(static_cast<double>(bytesTotal) / (1024 * 1024), 'f', 1));
    
    emit downloadProgress(m_progress);
}

void AutoUpdater::onDownloadNetworkReply(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        setStatus(UpdateStatus::Failed);
        QString error = tr("Download failed: %1").arg(reply->errorString());
        emit updateFailed(error);
        return;
    }
    
    // Verify download
    QString filePath = QFileInfo(m_progress.fileName).absoluteFilePath();
    QByteArray data = reply->readAll();
    
    if (data.isEmpty()) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Downloaded file is empty"));
        return;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Cannot save downloaded file"));
        return;
    }
    
    file.write(data);
    file.close();
    
    emit downloadCompleted(m_progress.fileName);
    
    qDebug() << "AutoUpdater: Download completed" << filePath;
    
    // Check integrity if SHA-256 is available
    if (!m_availableUpdate.sha256.isEmpty()) {
        QByteArray fileHash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
        if (fileHash.toHex() != m_availableUpdate.sha256) {
            setStatus(UpdateStatus::Failed);
            emit updateFailed(tr("Downloaded file integrity check failed"));
            return;
        }
    }
    
    setStatus(UpdateStatus::Installing);
}

bool AutoUpdater::installUpdate(const QString& filePath)
{
    emit installationStarted();
    
    // For our C++ application, we'd extract and replace the binary
    QString appDir = QCoreApplication::applicationDirPath();
    QString backupPath = appDir + "/multipack-parser.backup";
    
    // Create backup of current binary
    if (QFile::exists(appDir + "/multipack-parser")) {
        if (QFile::exists(backupPath)) {
            QFile::remove(backupPath);
        }
        
        if (!QFile::copy(appDir + "/multipack-parser", backupPath)) {
            qWarning() << "AutoUpdater: Failed to create backup";
            setStatus(UpdateStatus::Failed);
            emit updateFailed(tr("Failed to create backup"));
            return false;
        }
    }
    
    // Extract update (assuming tar.gz format)
    QProcess extractProcess;
    extractProcess.setWorkingDirectory(appDir);
    extractProcess.setProgram("tar");
    extractProcess.setArguments({"-xzf", filePath, "-C", appDir});
    
    qDebug() << "AutoUpdater: Extracting update...";
    
    extractProcess.start();
    extractProcess.waitForFinished();
    
    if (extractProcess.exitCode() != 0) {
        setStatus(UpdateStatus::Failed);
        emit updateFailed(tr("Failed to extract update files"));
        return false;
    }
    
    // Set executable permissions
    QFile::setPermissions(appDir + "/multipack-parser", 
                         QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
    
    // Restart application with new binary
    qDebug() << "AutoUpdater: Installation completed, restarting application";
    
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
    
    // Update status text based on phase
    if (bytesTotal > 0) {
        int percentage = (bytesReceived * 100) / bytesTotal;
        m_progress.statusText = tr("Downloaded %1% (%2 of %3 MB)")
            .arg(percentage)
            .arg(QString::number(static_cast<double>(bytesReceived) / (1024 * 1024), 'f', 1))
            .arg(QString::number(static_cast<double>(bytesTotal) / (1024 * 1024), 'f', 1));
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
    
    // Remove old update files older than 7 days
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-7);
    
    QStringList nameFilters;
    nameFilters << "*.tar.gz" << "*.zip";
    
    QFileInfoList oldFiles = cacheDir.entryInfoList(nameFilters, QDir::Files, QDir::Time);
    
    for (const QFileInfo& fileInfo : oldFiles) {
        if (fileInfo.lastModified() < cutoff) {
            qDebug() << "AutoUpdater: Removing old update file:" << fileInfo.fileName();
            QFile::remove(fileInfo.absoluteFilePath());
        }
    }
}

void AutoUpdater::onCheckNetworkReply(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        setStatus(UpdateStatus::Failed);
        QString error = tr("Update check failed: %1").arg(reply->errorString());
        emit checkFailed(error);
        reply->deleteLater();
        m_checkReply = nullptr;
        return;
    }
    
    QByteArray data = reply->readAll();
    reply->deleteLater();
    m_checkReply = nullptr;
    
    if (parseGitHubResponse(data)) {
        setStatus(UpdateStatus::Idle);
    } else {
        setStatus(UpdateStatus::Failed);
    }
}

void AutoUpdater::onSslErrors(const QList<QSslError>& errors)
{
    if (!errors.isEmpty()) {
        QString errorString;
        for (const QSslError& error : errors) {
            if (!errorString.isEmpty()) {
                errorString += ", ";
            }
            errorString += error.errorString();
        }
        
        qWarning() << "AutoUpdater: SSL errors:" << errorString;
    }
}

} // namespace system
} // namespace multipack