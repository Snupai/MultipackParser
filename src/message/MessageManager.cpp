/**
 * @file MessageManager.cpp
 * @brief Implementation of central message manager
 */

#include "multipack/message/MessageManager.h"

#include <QDebug>
#include <QMutexLocker>

namespace multipack {
namespace message {

MessageManager& MessageManager::instance()
{
    static MessageManager instance;
    return instance;
}

MessageManager::MessageManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "MessageManager::MessageManager - singleton created";
}

MessageManager::~MessageManager()
{
    qDebug() << "MessageManager::~MessageManager - singleton destroyed";
}

void MessageManager::post(const Message& message)
{
    {
        QMutexLocker locker(&m_mutex);

        m_messages.prepend(message);

        // Trim history if needed
        while (m_messages.size() > m_maxHistory) {
            m_messages.removeLast();
        }
    }

    // Emit signals (outside mutex)
    emit messagePosted(message);

    if (message.level() >= MessageLevel::Warning) {
        emit alertPosted(message);
    }

    // Also log to Qt debug output
    switch (message.level()) {
        case MessageLevel::Debug:
            qDebug().noquote() << message.toString();
            break;
        case MessageLevel::Info:
            qInfo().noquote() << message.toString();
            break;
        case MessageLevel::Warning:
            qWarning().noquote() << message.toString();
            break;
        case MessageLevel::Error:
        case MessageLevel::Critical:
            qCritical().noquote() << message.toString();
            break;
    }
}

void MessageManager::post(MessageLevel level, MessageCategory category,
                          const QString& text)
{
    post(Message(level, category, text));
}

void MessageManager::debug(const QString& text, MessageCategory category)
{
    post(MessageLevel::Debug, category, text);
}

void MessageManager::info(const QString& text, MessageCategory category)
{
    post(MessageLevel::Info, category, text);
}

void MessageManager::warning(const QString& text, MessageCategory category)
{
    post(MessageLevel::Warning, category, text);
}

void MessageManager::error(const QString& text, MessageCategory category)
{
    post(MessageLevel::Error, category, text);
}

void MessageManager::critical(const QString& text, MessageCategory category)
{
    post(MessageLevel::Critical, category, text);
}

QList<Message> MessageManager::recentMessages(int count) const
{
    QMutexLocker locker(&m_mutex);

    if (count >= m_messages.size()) {
        return m_messages;
    }

    return m_messages.mid(0, count);
}

QList<Message> MessageManager::messagesByLevel(MessageLevel level, int count) const
{
    QMutexLocker locker(&m_mutex);

    QList<Message> result;
    for (const Message& msg : m_messages) {
        if (msg.level() == level) {
            result.append(msg);
            if (result.size() >= count) {
                break;
            }
        }
    }

    return result;
}

QList<Message> MessageManager::messagesByCategory(MessageCategory category,
                                                   int count) const
{
    QMutexLocker locker(&m_mutex);

    QList<Message> result;
    for (const Message& msg : m_messages) {
        if (msg.category() == category) {
            result.append(msg);
            if (result.size() >= count) {
                break;
            }
        }
    }

    return result;
}

void MessageManager::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_messages.clear();
    }

    emit messagesCleared();
}

void MessageManager::setMaxHistory(int max)
{
    QMutexLocker locker(&m_mutex);

    m_maxHistory = max;

    while (m_messages.size() > m_maxHistory) {
        m_messages.removeLast();
    }
}

int MessageManager::count() const
{
    QMutexLocker locker(&m_mutex);
    return m_messages.size();
}

} // namespace message
} // namespace multipack
