/**
 * @file Message.h
 * @brief Message data types for the messaging system
 */

#ifndef MULTIPACK_MESSAGE_MESSAGE_H
#define MULTIPACK_MESSAGE_MESSAGE_H

#include <QString>
#include <QDateTime>
#include <QVariant>

namespace multipack {
namespace message {

/**
 * @enum MessageLevel
 * @brief Severity level of a message
 */
enum class MessageLevel {
    Debug,      ///< Debug information
    Info,       ///< Informational message
    Warning,    ///< Warning (non-critical issue)
    Error,      ///< Error (operation failed)
    Critical    ///< Critical error (system issue)
};

/**
 * @enum MessageCategory
 * @brief Category/source of a message
 */
enum class MessageCategory {
    System,     ///< System/application messages
    Robot,      ///< Robot communication
    Database,   ///< Database operations
    Network,    ///< Network/XML-RPC
    UI,         ///< User interface
    Audio,      ///< Audio system
    User        ///< User actions
};

/**
 * @class Message
 * @brief Represents a message in the system
 */
class Message
{
public:
    /**
     * @brief Default constructor
     */
    Message() = default;

    /**
     * @brief Construct a message
     * @param level Message level
     * @param category Message category
     * @param text Message text
     */
    Message(MessageLevel level, MessageCategory category, const QString& text);

    /**
     * @brief Get message level
     * @return Level
     */
    MessageLevel level() const { return m_level; }

    /**
     * @brief Set message level
     * @param level New level
     */
    void setLevel(MessageLevel level) { m_level = level; }

    /**
     * @brief Get message category
     * @return Category
     */
    MessageCategory category() const { return m_category; }

    /**
     * @brief Set message category
     * @param category New category
     */
    void setCategory(MessageCategory category) { m_category = category; }

    /**
     * @brief Get message text
     * @return Message text
     */
    QString text() const { return m_text; }

    /**
     * @brief Set message text
     * @param text New text
     */
    void setText(const QString& text) { m_text = text; }

    /**
     * @brief Get message timestamp
     * @return Timestamp
     */
    QDateTime timestamp() const { return m_timestamp; }

    /**
     * @brief Get additional data
     * @return Data variant
     */
    QVariant data() const { return m_data; }

    /**
     * @brief Set additional data
     * @param data Data variant
     */
    void setData(const QVariant& data) { m_data = data; }

    /**
     * @brief Check if message is valid
     * @return true if valid
     */
    bool isValid() const { return !m_text.isEmpty(); }

    /**
     * @brief Get formatted string representation
     * @return Formatted message
     */
    QString toString() const;

    /**
     * @brief Get level as string
     * @param level Level to convert
     * @return String representation
     */
    static QString levelToString(MessageLevel level);

    /**
     * @brief Get category as string
     * @param category Category to convert
     * @return String representation
     */
    static QString categoryToString(MessageCategory category);

private:
    MessageLevel m_level = MessageLevel::Info;
    MessageCategory m_category = MessageCategory::System;
    QString m_text;
    QDateTime m_timestamp = QDateTime::currentDateTime();
    QVariant m_data;
};

} // namespace message
} // namespace multipack

#endif // MULTIPACK_MESSAGE_MESSAGE_H
