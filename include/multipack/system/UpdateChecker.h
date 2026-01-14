/**
 * @file UpdateChecker.h
 * @brief Application update checker
 *
 * Checks for available updates.
 */

#ifndef MULTIPACK_SYSTEM_UPDATECHECKER_H
#define MULTIPACK_SYSTEM_UPDATECHECKER_H

#include <QObject>
#include <QString>

namespace multipack {
namespace system {

/**
 * @struct UpdateInfo
 * @brief Information about an available update
 */
struct UpdateInfo {
    QString version;        ///< Version string
    QString releaseNotes;   ///< Release notes
    QString downloadUrl;    ///< Download URL
    bool isRequired = false; ///< Whether update is required
};

/**
 * @class UpdateChecker
 * @brief Checks for application updates
 *
 * Placeholder for update checking functionality.
 */
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
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
     * @brief Check for updates
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

private:
    UpdateInfo m_updateInfo;
    bool m_updateAvailable = false;
};

} // namespace system
} // namespace multipack

#endif // MULTIPACK_SYSTEM_UPDATECHECKER_H
