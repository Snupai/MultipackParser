/**
 * @file UsbKeyCheck.h
 * @brief USB key authentication check
 *
 * Checks for presence of USB key file for password bypass.
 */

#ifndef MULTIPACK_SYSTEM_USBKEYCHECK_H
#define MULTIPACK_SYSTEM_USBKEYCHECK_H

#include <QObject>
#include <QString>

namespace multipack {
namespace system {

/**
 * @class UsbKeyCheck
 * @brief Checks for USB key file
 *
 * Monitors for a special key file on USB drives
 * that can bypass password authentication.
 */
class UsbKeyCheck : public QObject
{
    Q_OBJECT

public:
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
     * @brief Check if USB key is present
     * @return true if valid key found
     */
    bool isKeyPresent() const;

    /**
     * @brief Check specific path for key file
     * @param path Path to check
     * @return true if valid key found
     */
    bool checkPath(const QString& path) const;

    /**
     * @brief Get the key file name
     * @return Key file name
     */
    static QString keyFileName();

    /**
     * @brief Set custom key file name
     * @param name File name to check for
     */
    void setKeyFileName(const QString& name);

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
     * @brief Verify key file contents
     * @param path Path to key file
     * @return true if contents are valid
     */
    bool verifyKeyFile(const QString& path) const;

signals:
    /**
     * @brief Emitted when key status changes
     * @param present Whether key is now present
     */
    void keyStatusChanged(bool present);

private:
    QString m_keyFileName;
    QStringList m_searchPaths;
};

} // namespace system
} // namespace multipack

#endif // MULTIPACK_SYSTEM_USBKEYCHECK_H
