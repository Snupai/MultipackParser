#ifndef MULTIPACK_SYSTEM_AUTOUPDATER_H
#define MULTIPACK_SYSTEM_AUTOUPDATER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QProgressDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QSslError>

namespace multipack {
namespace system {

/**
 * @brief Update download and installation status
 */
enum class UpdateStatus {
    Idle = 0,
    Checking = 1,
    Downloading = 2,
    Installing = 3,
    Finished = 4,
    Failed = 5
};

/**
 * @brief Update download progress information
 */
struct UpdateProgress {
    qint64 bytesTotal = 0;
    qint64 bytesReceived = 0;
    QString fileName;
    QString statusText;
};

/**
 * @brief Update information from repository
 */
struct UpdateInfo {
    QString version;
    QString releaseNotes;
    QString downloadUrl;
    qint64 sizeBytes = 0;
    QString sha256;
    QString manifestUrl;
    QString signatureUrl;
    QString packageFileName;
    QString localFilePath;
    bool signatureVerified = false;
    bool isPrerelease = false;
    bool isRequired = false;
};

/**
 * @brief Comprehensive automatic updater with download and installation capabilities
 * 
 * This class provides automatic update functionality including:
 * - GitHub releases API integration
 * - Download with progress tracking
 * - Automatic installation
 * - UI updates via signals
 * - Error handling and retry logic
 * - Local update cache management
 */
class AutoUpdater : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit AutoUpdater(QObject* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~AutoUpdater() override;

    /**
     * @brief Check for available updates
     * @return true if update check was started successfully
     */
    bool checkForUpdates();

    /**
     * @brief Check for signed updates from a local USB/offline directory
     * @param usbDirectory Directory containing update artifacts
     * @return true if check completed
     */
    bool checkForUsbUpdates(const QString& usbDirectory);

    /**
     * @brief Start downloading and installing updates
     * @return true if download/install process was started successfully
     */
    bool downloadAndInstallUpdate();

    /**
     * @brief Cancel current update operation
     */
    void cancelUpdate();

    /**
     * @brief Get current update status
     * @return Current update status
     */
    UpdateStatus currentStatus() const;

    /**
     * @brief Get current update progress
     * @return Current progress information
     */
    UpdateProgress currentProgress() const;

    /**
     * @brief Get available update info
     * @return Update information if update available
     */
    UpdateInfo availableUpdate() const;

    /**
     * @brief Get current version
     * @return Current application version
     */
    QString currentVersion() const;

    /**
     * @brief Set update check interval
     * @param hours Check interval in hours
     */
    void setCheckInterval(int hours);

    /**
     * @brief Set fallback USB directory used if online checks fail
     */
    void setUsbUpdatePath(const QString& usbDirectory);

    /**
     * @brief Get configured fallback USB directory
     */
    QString usbUpdatePath() const;

public slots:
    /**
     * @brief Show update dialog with available updates
     */
    void showUpdateDialog();

signals:
    /**
     * @brief Emitted when update check starts
     */
    void checkStarted();
    
    /**
     * @brief Emitted when update check completes
     * @param success Whether check was successful
     * @param message Status message
     */
    void checkCompleted(bool success, const QString& message = QString());
    
    /**
     * @brief Emitted when download starts
     * @param fileName Name of file being downloaded
     */
    void downloadStarted(const QString& fileName);
    
    /**
     * @brief Emitted when download progress updates
     * @param progress Progress information
     */
    void downloadProgress(const UpdateProgress& progress);
    
    /**
     * @brief Emitted when download completes
     * @param fileName Name of downloaded file
     */
    void downloadCompleted(const QString& fileName);
    
    /**
     * @brief Emitted when installation starts
     */
    void installationStarted();
    
    /**
     * @brief Emitted when installation completes
     * @param success Whether installation was successful
     * @param message Status message
     */
    void installationCompleted(bool success, const QString& message = QString());
    
    /**
     * @brief Emitted when update operation fails
     * @param error Error message
     */
    void updateFailed(const QString& error);
    
    /**
     * @brief Emitted when update check fails
     * @param error Error message
     */
    void checkFailed(const QString& error);
    
    /**
     * @brief Emitted when update operation is cancelled
     */
    void updateCancelled();

private slots:
    /**
     * @brief Handle network reply for update check
     */
    void onCheckNetworkReply(QNetworkReply* reply);
    
    /**
     * @brief Handle network reply for download
     */
    void onDownloadNetworkReply(QNetworkReply* reply);
    
    /**
     * @brief Handle download progress
     */
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    
    /**
     * @brief Handle SSL errors
     */
    void onSslErrors(const QList<QSslError>& errors);

private:
    // Core components
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_checkReply = nullptr;
    QNetworkReply* m_downloadReply = nullptr;
    QTimer* m_checkTimer = nullptr;
    
    // State
    UpdateStatus m_status = UpdateStatus::Idle;
    UpdateProgress m_progress;
    UpdateInfo m_availableUpdate;
    QString m_currentVersion;
    QString m_updateCacheDir;
    QString m_usbUpdateDir;
    QString m_downloadTargetPath;
    int m_checkIntervalHours = 24; // Check every 24 hours
    
    /**
     * @brief Initialize network manager
     */
    void initializeNetwork();
    
    /**
     * @brief Start update check via GitHub API
     */
    void startGitHubCheck();
    
    /**
     * @brief Parse GitHub releases API response
     * @param data Response data
     * @return true if successful
     */
    bool parseGitHubResponse(const QByteArray& data);

    /**
     * @brief Parse a single release object from GitHub API
     */
    bool parseReleaseObject(const QJsonObject& release, QString& errorMessage);

    /**
     * @brief Load and verify signed manifest from URLs
     */
    bool loadAndVerifyManifestFromUrls(const QString& manifestUrl,
                                       const QString& signatureUrl,
                                       UpdateInfo& info,
                                       QString& errorMessage);

    /**
     * @brief Load and verify signed manifest from local directory
     */
    bool loadAndVerifyManifestFromDirectory(const QString& directory,
                                            UpdateInfo& info,
                                            QString& errorMessage);

    /**
     * @brief Parse manifest JSON into update info
     */
    bool parseManifest(const QByteArray& manifestBytes,
                       UpdateInfo& info,
                       QString& errorMessage) const;

    /**
     * @brief Verify manifest detached signature
     */
    bool verifyManifestSignature(const QByteArray& manifestBytes,
                                 const QByteArray& signatureBytes,
                                 QString& errorMessage) const;

    /**
     * @brief Verify file SHA-256
     */
    bool verifyFileSha256(const QString& filePath,
                          const QString& expectedSha256,
                          QString& errorMessage) const;

    /**
     * @brief Fetch URL synchronously
     */
    bool fetchUrlSync(const QUrl& url, QByteArray& data, QString& errorMessage) const;

    /**
     * @brief Resolve trusted public key PEM
     */
    QString trustedPublicKeyPem(QString& errorMessage) const;

    /**
     * @brief Normalize semantic version string
     */
    QString normalizeVersion(const QString& version) const;

    /**
     * @brief Clear cached update info
     */
    void clearAvailableUpdate();
    
    /**
     * @brief Download update file
     * @param url Download URL
     * @param filePath Local file path
     * @return true if download started successfully
     */
    bool downloadUpdateFile(const QString& url, const QString& filePath);
    
    /**
     * @brief Install update
     * @param filePath Path to update file
     * @return true if installation started successfully
     */
    bool installUpdate(const QString& filePath);
    
    /**
     * @brief Update internal state
     * @param status New status
     */
    void setStatus(UpdateStatus status);
    
    /**
     * @brief Update progress information
     * @param bytesReceived Bytes received
     * @param bytesTotal Total bytes
     */
    void updateProgress(qint64 bytesReceived, qint64 bytesTotal);
    
    /**
     * @brief Get update cache directory
     * @return Cache directory path
     */
    QString getUpdateCacheDir() const;
    
    /**
     * @brief Clean up old update files
     */
    void cleanupOldUpdates();
    
    /**
     * @brief Compare two version strings
     * @param v1 First version string
     * @param v2 Second version string
     * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
     */
    int compareVersions(const QString& v1, const QString& v2);
};

} // namespace system
} // namespace multipack

#endif // MULTIPACK_SYSTEM_AUTOUPDATER_H
