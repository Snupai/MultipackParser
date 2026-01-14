/**
 * @file StatusManager.cpp
 * @brief Implementation of status bar manager
 */

#include "multipack/message/StatusManager.h"

#include <QDebug>

namespace multipack {
namespace message {

StatusManager& StatusManager::instance()
{
    static StatusManager instance;
    return instance;
}

StatusManager::StatusManager(QObject* parent)
    : QObject(parent)
    , m_timer(std::make_unique<QTimer>(this))
{
    qDebug() << "StatusManager::StatusManager - singleton created";

    m_timer->setSingleShot(true);
    connect(m_timer.get(), &QTimer::timeout, this, &StatusManager::clearStatus);
}

StatusManager::~StatusManager()
{
    qDebug() << "StatusManager::~StatusManager - singleton destroyed";
}

void StatusManager::showStatus(const QString& message, StatusType type, int timeoutMs)
{
    m_timer->stop();

    m_currentMessage = message;
    m_currentType = type;

    emit statusChanged(message, type);

    if (timeoutMs > 0) {
        m_timer->start(timeoutMs);
    }
}

void StatusManager::showTemporaryStatus(const QString& message, StatusType type,
                                        int timeoutMs)
{
    showStatus(message, type, timeoutMs);
}

void StatusManager::showBusy(const QString& message)
{
    showStatus(message, StatusType::Busy, 0);
}

void StatusManager::showSuccess(const QString& message, int timeoutMs)
{
    showStatus(message, StatusType::Success, timeoutMs);
}

void StatusManager::showWarning(const QString& message, int timeoutMs)
{
    showStatus(message, StatusType::Warning, timeoutMs);
}

void StatusManager::showError(const QString& message, int timeoutMs)
{
    showStatus(message, StatusType::Error, timeoutMs);
}

void StatusManager::clearStatus()
{
    m_timer->stop();

    m_currentMessage.clear();
    m_currentType = StatusType::Normal;

    emit statusCleared();
}

QString StatusManager::currentMessage() const
{
    return m_currentMessage;
}

StatusType StatusManager::currentType() const
{
    return m_currentType;
}

} // namespace message
} // namespace multipack
