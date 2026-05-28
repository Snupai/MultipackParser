/**
 * @file StatusManager.h
 * @brief Status bar and status display management
 *
 * Manages status messages shown in the UI.
 */

#ifndef MULTIPACK_MESSAGE_STATUSMANAGER_H
#define MULTIPACK_MESSAGE_STATUSMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

namespace multipack {
namespace message {

/**
 * @enum StatusType
 * @brief Type of status message
 */
enum class StatusType {
    Normal,     ///< Normal status
    Busy,       ///< Busy/working
    Success,    ///< Operation succeeded
    Warning,    ///< Warning state
    Error       ///< Error state
};

/**
 * @class StatusManager
 * @brief Manages status bar messages
 *
 * Singleton that manages status messages for the UI.
 */
class StatusManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     * @return StatusManager instance
     */
    static StatusManager& instance();

    /**
     * @brief Show a status message
     * @param message Message text
     * @param type Status type
     * @param timeoutMs Timeout in ms (0 = persistent)
     */
    void showStatus(const QString& message,
                   StatusType type = StatusType::Normal,
                   int timeoutMs = 0);

    /**
     * @brief Show a temporary status message
     * @param message Message text
     * @param type Status type
     * @param timeoutMs Timeout (default 3000ms)
     */
    void showTemporaryStatus(const QString& message,
                            StatusType type = StatusType::Normal,
                            int timeoutMs = 3000);

    /**
     * @brief Show busy status with message
     * @param message Message text
     */
    void showBusy(const QString& message);

    /**
     * @brief Show success status
     * @param message Message text
     * @param timeoutMs Timeout in ms
     */
    void showSuccess(const QString& message, int timeoutMs = 3000);

    /**
     * @brief Show warning status
     * @param message Message text
     * @param timeoutMs Timeout in ms (0 = persistent)
     */
    void showWarning(const QString& message, int timeoutMs = 0);

    /**
     * @brief Show error status
     * @param message Message text
     * @param timeoutMs Timeout in ms (0 = persistent)
     */
    void showError(const QString& message, int timeoutMs = 0);

    /**
     * @brief Clear current status
     */
    void clearStatus();

    /**
     * @brief Get current status message
     * @return Current message
     */
    QString currentMessage() const;

    /**
     * @brief Get current status type
     * @return Current type
     */
    StatusType currentType() const;

signals:
    /**
     * @brief Emitted when status changes
     * @param message New message
     * @param type Status type
     */
    void statusChanged(const QString& message, StatusType type);

    /**
     * @brief Emitted when status is cleared
     */
    void statusCleared();

private:
    explicit StatusManager(QObject* parent = nullptr);
    ~StatusManager() override;

    // Prevent copying
    StatusManager(const StatusManager&) = delete;
    StatusManager& operator=(const StatusManager&) = delete;

    QString m_currentMessage;
    StatusType m_currentType = StatusType::Normal;
    std::unique_ptr<QTimer> m_timer;
};

// Global convenience functions
inline void showStatus(const QString& message, StatusType type = StatusType::Normal) {
    StatusManager::instance().showStatus(message, type);
}

inline void showBusy(const QString& message) {
    StatusManager::instance().showBusy(message);
}

inline void showSuccess(const QString& message) {
    StatusManager::instance().showSuccess(message);
}

inline void showWarning(const QString& message) {
    StatusManager::instance().showWarning(message);
}

inline void showError(const QString& message) {
    StatusManager::instance().showError(message);
}

inline void clearStatus() {
    StatusManager::instance().clearStatus();
}

} // namespace message
} // namespace multipack

#endif // MULTIPACK_MESSAGE_STATUSMANAGER_H
