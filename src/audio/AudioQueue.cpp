/**
 * @file AudioQueue.cpp
 * @brief Implementation of thread-safe audio queue
 */

#include "multipack/audio/AudioQueue.h"

#include <QMutexLocker>
#include <QDebug>

namespace multipack {
namespace audio {

AudioQueue::AudioQueue(QObject* parent)
    : QObject(parent)
{
    qDebug() << "AudioQueue::AudioQueue - constructor";
}

AudioQueue::~AudioQueue()
{
    qDebug() << "AudioQueue::~AudioQueue - destructor";
    clear();
}

void AudioQueue::enqueue(const AudioItem& item)
{
    QMutexLocker locker(&m_mutex);

    // Insert based on priority (higher priority = earlier in queue)
    int insertPos = 0;
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i].priority < item.priority) {
            break;
        }
        insertPos = i + 1;
    }

    // QQueue doesn't have insert, so we need to use a workaround
    if (insertPos == m_queue.size()) {
        m_queue.enqueue(item);
    } else {
        // Insert at position
        QQueue<AudioItem> temp;
        for (int i = 0; i < insertPos; ++i) {
            temp.enqueue(m_queue.dequeue());
        }
        temp.enqueue(item);
        while (!m_queue.isEmpty()) {
            temp.enqueue(m_queue.dequeue());
        }
        m_queue = temp;
    }

    locker.unlock();
    emit itemEnqueued();
}

void AudioQueue::enqueueFile(const QString& path, int priority)
{
    AudioItem item;
    item.path = path;
    item.isResource = false;
    item.priority = priority;
    enqueue(item);
}

void AudioQueue::enqueueResource(const QString& resource, int priority)
{
    AudioItem item;
    item.path = resource;
    item.isResource = true;
    item.priority = priority;
    enqueue(item);
}

AudioItem AudioQueue::dequeue()
{
    QMutexLocker locker(&m_mutex);

    if (m_queue.isEmpty()) {
        return AudioItem();
    }

    AudioItem item = m_queue.dequeue();

    if (m_queue.isEmpty()) {
        locker.unlock();
        emit queueEmpty();
    }

    return item;
}

AudioItem AudioQueue::peek() const
{
    QMutexLocker locker(&m_mutex);

    if (m_queue.isEmpty()) {
        return AudioItem();
    }

    return m_queue.head();
}

bool AudioQueue::isEmpty() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue.isEmpty();
}

int AudioQueue::size() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue.size();
}

void AudioQueue::clear()
{
    QMutexLocker locker(&m_mutex);
    m_queue.clear();
}

} // namespace audio
} // namespace multipack
