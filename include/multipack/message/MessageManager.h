/**
 * @file MessageManager.h
 * @brief Central message management system
 *
 * Manages message routing, storage, and notification.
 */

#ifndef MULTIPACK_MESSAGE_MESSAGEMANAGER_H
#define MULTIPACK_MESSAGE_MESSAGEMANAGER_H

#include <QObject>
#include <QList>
#include <QMutex>
#include <memory>

#include "Message.h"

namespace multipack {
namespace message {

/**
 * @class MessageManager
 * @brief Central hub for message management
 *
 * Thread-safe message manager that stores recent messages
 * and emits signals for new messages.
 */
class MessageManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     * @return MessageManager instance
     */
    static MessageManager& instance();

    /**
     * @brief Post a new message
     * @param message Message to post
     */
    void post(const Message& message);

    /**
     * @brief Post a message with parameters
     * @param level Message level
     * @param category Message category
     * @param text Message text
     */
    void post(MessageLevel level, MessageCategory category, const QString& text);

    // Convenience methods
    void debug(const QString& text, MessageCategory category = MessageCategory::System);
    void info(const QString& text, MessageCategory category = MessageCategory::System);
    void warning(const QString& text, MessageCategory category = MessageCategory::System);
    void error(const QString& text, MessageCategory category = MessageCategory::System);
    void critical(const QString& text, MessageCategory category = MessageCategory::System);

    /**
     * @brief Get recent messages
     * @param count Maximum number to return
     * @return List of recent messages
     */
    QList<Message> recentMessages(int count = 100) const;

    /**
     * @brief Get messages by level
     * @param level Level to filter
     * @param count Maximum number to return
     * @return Filtered messages
     */
    QList<Message> messagesByLevel(MessageLevel level, int count = 100) const;

    /**
     * @brief Get messages by category
     * @param category Category to filter
     * @param count Maximum number to return
     * @return Filtered messages
     */
    QList<Message> messagesByCategory(MessageCategory category, int count = 100) const;

    /**
     * @brief Clear all messages
     */
    void clear();

    /**
     * @brief Set maximum message history
     * @param max Maximum messages to keep
     */
    void setMaxHistory(int max);

    /**
     * @brief Get message count
     * @return Number of stored messages
     */
    int count() const;

signals:
    /**
     * @brief Emitted when a new message is posted
     * @param message The new message
     */
    void messagePosted(const Message& message);

    /**
     * @brief Emitted for warning or higher
     * @param message The message
     */
    void alertPosted(const Message& message);

    /**
     * @brief Emitted when messages are cleared
     */
    void messagesCleared();

private:
    explicit MessageManager(QObject* parent = nullptr);
    ~MessageManager() override;

    // Prevent copying
    MessageManager(const MessageManager&) = delete;
    MessageManager& operator=(const MessageManager&) = delete;

    mutable QMutex m_mutex;
    QList<Message> m_messages;
    int m_maxHistory = 1000;
};

// Global convenience functions
inline void postMessage(const Message& message) {
    MessageManager::instance().post(message);
}

inline void logDebug(const QString& text, MessageCategory category = MessageCategory::System) {
    MessageManager::instance().debug(text, category);
}

inline void logInfo(const QString& text, MessageCategory category = MessageCategory::System) {
    MessageManager::instance().info(text, category);
}

inline void logWarning(const QString& text, MessageCategory category = MessageCategory::System) {
    MessageManager::instance().warning(text, category);
}

inline void logError(const QString& text, MessageCategory category = MessageCategory::System) {
    MessageManager::instance().error(text, category);
}

inline void logCritical(const QString& text, MessageCategory category = MessageCategory::System) {
    MessageManager::instance().critical(text, category);
}

} // namespace message
} // namespace multipack

#endif // MULTIPACK_MESSAGE_MESSAGEMANAGER_H
