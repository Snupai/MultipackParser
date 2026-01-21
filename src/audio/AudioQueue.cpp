/**
 * @file AudioQueue.cpp
 * @brief Implementation of thread-safe audio queue with playback counting
 */

#include "multipack/audio/AudioQueue.h"

#include <QMutexLocker>
#include <QDebug>
#include <algorithm>

namespace multipack {
namespace audio {

AudioQueue::AudioQueue(QObject* parent)
    : QObject(parent)
{
    qDebug() << "AudioQueue - initialized";
}

AudioQueue::~AudioQueue()
{
    qDebug() << "AudioQueue - destroying";
    killAll();
}

void AudioQueue::addToQueue(const QString& id, const QString& filePath, int playbackCount)
{
    QMutexLocker locker(&m_mutex);
    
    AudioItem item;
    item.id = id;
    item.filePath = filePath;
    item.playbackCount = playbackCount;
    item.currentCount = 0;
    item.priority = 0;
    item.isResource = filePath.startsWith("qrc:/") || filePath.startsWith(":/");
    
    m_queue.push_back(item);
    qDebug() << "AudioQueue: Added" << id << "to queue with" << playbackCount << "plays";
    
    locker.unlock();
    emit itemEnqueued();
}

bool AudioQueue::stopAudio(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    
    bool stopped = false;
    
    // Check if current item matches
    if (m_currentItem.id == id) {
        qDebug() << "AudioQueue: Stopping current item" << id;
        m_currentItem = AudioItem();
        stopped = true;
    }
    
    // Remove from queue
    auto it = std::remove_if(m_queue.begin(), m_queue.end(),
        [&id](const AudioItem& item) { return item.id == id; });
    
    if (it != m_queue.end()) {
        m_queue.erase(it, m_queue.end());
        stopped = true;
    }
    
    if (stopped) {
        qDebug() << "AudioQueue: Stopped audio" << id;
        locker.unlock();
        emit audioStopped(id);
    }
    
    return stopped;
}

void AudioQueue::killAll()
{
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "AudioQueue: Killing all audio";
    m_queue.clear();
    m_currentItem = AudioItem();
    m_isPlaying = false;
    
    locker.unlock();
    emit queueEmpty();
}

void AudioQueue::enqueue(const AudioItem& item)
{
    QMutexLocker locker(&m_mutex);

    // Insert based on priority (higher priority = earlier in queue)
    auto it = std::find_if(m_queue.begin(), m_queue.end(),
        [&item](const AudioItem& existing) { return existing.priority < item.priority; });
    
    m_queue.insert(it, item);
    
    qDebug() << "AudioQueue: Enqueued item" << item.id << "priority:" << item.priority;

    locker.unlock();
    emit itemEnqueued();
}

void AudioQueue::enqueueFile(const QString& path, int priority)
{
    AudioItem item;
    item.id = path;  // Use path as ID for legacy compatibility
    item.filePath = path;
    item.isResource = false;
    item.priority = priority;
    item.playbackCount = 1;
    enqueue(item);
}

void AudioQueue::enqueueResource(const QString& resource, int priority)
{
    AudioItem item;
    item.id = resource;
    item.filePath = resource;
    item.isResource = true;
    item.priority = priority;
    item.playbackCount = 1;
    enqueue(item);
}

AudioItem AudioQueue::dequeue()
{
    QMutexLocker locker(&m_mutex);

    if (m_queue.empty()) {
        return AudioItem();
    }

    AudioItem item = m_queue.front();
    m_queue.pop_front();
    m_currentItem = item;
    m_isPlaying = true;

    if (m_queue.empty()) {
        locker.unlock();
        emit queueEmpty();
    }

    return item;
}

AudioItem AudioQueue::peek() const
{
    QMutexLocker locker(&m_mutex);

    if (m_queue.empty()) {
        return AudioItem();
    }

    return m_queue.front();
}

bool AudioQueue::isEmpty() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue.empty();
}

int AudioQueue::size() const
{
    QMutexLocker locker(&m_mutex);
    return static_cast<int>(m_queue.size());
}

void AudioQueue::clear()
{
    QMutexLocker locker(&m_mutex);
    m_queue.clear();
}

bool AudioQueue::isPlaying() const
{
    QMutexLocker locker(&m_mutex);
    return m_isPlaying;
}

AudioItem AudioQueue::currentItem() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentItem;
}

void AudioQueue::markCurrentPlayed()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_currentItem.id.isEmpty()) {
        return;
    }
    
    // Increment play count if not infinite
    if (m_currentItem.playbackCount != -1) {
        m_currentItem.currentCount++;
        qDebug() << "AudioQueue: Marked" << m_currentItem.id << "played:"
                 << m_currentItem.currentCount << "/" << m_currentItem.playbackCount;
    }
    
    locker.unlock();
    emit playbackFinished(m_currentItem.id);
}

bool AudioQueue::isCurrentItemDone() const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_currentItem.id.isEmpty()) {
        return true;
    }
    
    // Infinite playback never done
    if (m_currentItem.playbackCount == -1) {
        return false;
    }
    
    return m_currentItem.currentCount >= m_currentItem.playbackCount;
}

void AudioQueue::advanceQueue()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_currentItem.id.isEmpty()) {
        m_isPlaying = false;
        return;
    }
    
    // Handle infinite playback - rotate to end of queue
    if (m_currentItem.playbackCount == -1) {
        qDebug() << "AudioQueue: Rotating infinite item" << m_currentItem.id << "to end";
        m_queue.push_back(m_currentItem);
    } else if (m_currentItem.currentCount < m_currentItem.playbackCount) {
        // Not done yet, keep at front
        qDebug() << "AudioQueue: Keeping" << m_currentItem.id << "at front (more plays remaining)";
        m_queue.push_front(m_currentItem);
    } else {
        qDebug() << "AudioQueue: Removing" << m_currentItem.id << "- playback count reached";
    }
    
    m_currentItem = AudioItem();
    
    if (m_queue.empty()) {
        m_isPlaying = false;
        locker.unlock();
        emit queueEmpty();
    }
}

} // namespace audio
} // namespace multipack
