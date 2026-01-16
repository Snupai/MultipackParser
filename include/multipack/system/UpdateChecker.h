/**
 * @file UpdateChecker.h
 * @brief Application update checker
 *
 * Checks for available updates via GitHub releases API.
 */

#ifndef MULTIPACK_SYSTEM_UPDATECHECKER_H
#define MULTIPACK_SYSTEM_UPDATECHECKER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace multipack {
namespace system {

/**
 * @struct UpdateInfo
 * @brief Information about an available update
 */
struct UpdateInfo {
    QString version;        ///< Version string
    QString releaseNotes;   ///< Release notes (body)
    QString downloadUrl;    ///< Download URL for asset
    QString releaseName;    ///< Release name/title
    bool isRequired = false; ///< Whether update is required
};

/**
 * @class UpdateChecker
 * @brief Checks for application updates via GitHub
 *
 * Queries the GitHub releases API to check for new versions.
 */
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    /// GitHub API URL for latest release
    static constexpr const char* GITHUB_RELEASES_URL =
        "https://api.github.com/repos/Snupai/MultipackParser/releases/latest";

    /// Expected asset name
    static constexpr const char* ASSET_NAME = "MultipackParser";

    /**
     * @brief Construct a new Update Checker
     * @param parent Parent QObject
     */
    explicit UpdateChecker(QObject* parent = nullptr);

    /**
     * @brief Destroy the Update Checker
     */
    ~UpdateChecker() override;

    /**
     * @brief Check for updates asynchronously
     */
    void checkForUpdates();

    /**
     * @brief Get current version
     * @return Current version string
     */
    QString currentVersion() const;

    /**
     * @brief Check if update is available
     * @return true if update available
     */
    bool updateAvailable() const;

    /**
     * @brief Get update info
     * @return Update information
     */
    UpdateInfo updateInfo() const;

    /**
     * @brief Check if currently checking for updates
     */
    bool isChecking() const { return m_checking; }

    /**
     * @brief Compare two version strings
     * @param v1 First version
     * @param v2 Second version
     * @return negative if v1 < v2, 0 if equal, positive if v1 > v2
     */
    static int compareVersions(const QString& v1, const QString& v2);

signals:
    /**
     * @brief Emitted when check starts
     */
    void checkingStarted();

    /**
     * @brief Emitted when check completes
     * @param available Whether update is available
     */
    void checkCompleted(bool available);

    /**
     * @brief Emitted on check error
     * @param error Error message
     */
    void checkFailed(const QString& error);

private slots:
    void onNetworkReply(QNetworkReply* reply);

private:
    void parseGitHubResponse(const QByteArray& data);

    QNetworkAccessManager* m_networkManager;
    UpdateInfo m_updateInfo;
    bool m_updateAvailable = false;
    bool m_checking = false;
};

} // namespace system
} // namespace multipack

#endif // MULTIPACK_SYSTEM_UPDATECHECKER_H
