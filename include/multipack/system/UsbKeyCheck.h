/**
 * @file UsbKeyCheck.h
 * @brief USB key authentication check
 *
 * Checks for presence of USB key file for password bypass.
 * Uses a .keyindex file that points to an encrypted key file
 * verified using Fernet-compatible decryption.
 */

#ifndef MULTIPACK_SYSTEM_USBKEYCHECK_H
#define MULTIPACK_SYSTEM_USBKEYCHECK_H

#include <QObject>
#include <QString>
#include <QStringList>

namespace multipack {

namespace config { class SettingsManager; }

namespace system {

/**
 * @class UsbKeyCheck
 * @brief Checks for USB key file
 *
 * Monitors for a special key file on USB drives
 * that can bypass password authentication.
 *
 * The key verification uses:
 * 1. A .keyindex file containing the relative path to the key file
 * 2. A key file containing Fernet-encrypted data
 * 3. Decryption and comparison against expected value
 */
class UsbKeyCheck : public QObject
{
    Q_OBJECT

public:
    /// Index file name that points to key location
    static constexpr const char* INDEX_FILE_NAME = ".keyindex";

    /**
     * @brief Construct a new Usb Key Check
     * @param parent Parent QObject
     */
    explicit UsbKeyCheck(QObject* parent = nullptr);

    /**
     * @brief Destroy the Usb Key Check
     */
    ~UsbKeyCheck() override;

    /**
     * @brief Set settings manager for key/expected value retrieval
     * @param settings Settings manager
     */
    void setSettingsManager(config::SettingsManager* settings);

    /**
     * @brief Check if USB key is present on any connected drive
     * @return true if valid key found
     */
    bool isKeyPresent() const;

    /**
     * @brief Check specific path for key file
     * @param path Path to check (should be USB root or subdirectory)
     * @return true if valid key found
     */
    bool checkPath(const QString& path) const;

    /**
     * @brief Check hidden key using .keyindex approach
     * @param usbPath USB root path
     * @return true if valid key found and verified
     */
    bool checkHiddenKey(const QString& usbPath) const;

    /**
     * @brief Get the index file name
     * @return Index file name (.keyindex)
     */
    static QString indexFileName();

    /**
     * @brief Add path to search
     * @param path Path to add
     */
    void addSearchPath(const QString& path);

    /**
     * @brief Clear search paths
     */
    void clearSearchPaths();

    /**
     * @brief Get search paths
     * @return List of search paths
     */
    QStringList searchPaths() const;

    /**
     * @brief Find all directories containing .keyindex files
     * @param baseDir Base directory to search
     * @return List of directories with .keyindex files
     */
    QStringList findKeyIndexDirs(const QString& baseDir) const;

signals:
    /**
     * @brief Emitted when key status changes
     * @param present Whether key is now present
     */
    void keyStatusChanged(bool present);

private:
    /**
     * @brief Verify and decrypt key file using Fernet
     * @param keyFilePath Path to key file
     * @return true if decryption succeeds and matches expected
     */
    bool verifyKeyFile(const QString& keyFilePath) const;

    /**
     * @brief Get list of mounted USB drives
     * @return List of USB mount points
     */
    QStringList getUsbDrives() const;

    config::SettingsManager* m_settings = nullptr;
    QStringList m_searchPaths;
};

} // namespace system
} // namespace multipack

#endif // MULTIPACK_SYSTEM_USBKEYCHECK_H
