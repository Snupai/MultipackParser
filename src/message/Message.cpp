/**
 * @file Message.cpp
 * @brief Implementation of Message class
 */

#include "multipack/message/Message.h"

namespace multipack {
namespace message {

Message::Message(MessageLevel level, MessageCategory category, const QString& text)
    : m_level(level)
    , m_category(category)
    , m_text(text)
    , m_timestamp(QDateTime::currentDateTime())
{
}

QString Message::toString() const
{
    return QString("[%1] [%2] [%3] %4")
        .arg(m_timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
        .arg(levelToString(m_level))
        .arg(categoryToString(m_category))
        .arg(m_text);
}

QString Message::levelToString(MessageLevel level)
{
    switch (level) {
        case MessageLevel::Debug:    return "DEBUG";
        case MessageLevel::Info:     return "INFO";
        case MessageLevel::Warning:  return "WARNING";
        case MessageLevel::Error:    return "ERROR";
        case MessageLevel::Critical: return "CRITICAL";
        default:                     return "UNKNOWN";
    }
}

QString Message::categoryToString(MessageCategory category)
{
    switch (category) {
        case MessageCategory::System:   return "System";
        case MessageCategory::Robot:    return "Robot";
        case MessageCategory::Database: return "Database";
        case MessageCategory::Network:  return "Network";
        case MessageCategory::UI:       return "UI";
        case MessageCategory::Audio:    return "Audio";
        case MessageCategory::User:     return "User";
        default:                        return "Unknown";
    }
}

} // namespace message
} // namespace multipack
