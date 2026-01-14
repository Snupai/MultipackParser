/**
 * @file AudioQueue.h
 * @brief Thread-safe audio playback queue
 *
 * Provides a queue for sequential audio playback.
 */

#ifndef MULTIPACK_AUDIO_AUDIOQUEUE_H
#define MULTIPACK_AUDIO_AUDIOQUEUE_H

#include <QObject>
#include <QString>
#include <QQueue>
#include <QMutex>

namespace multipack {
namespace audio {

/**
 * @struct AudioItem
 * @brief Item in the audio queue
 */
struct AudioItem {
    QString path;           ///< Path or resource to play
    bool isResource = false; ///< True if resource path
    int priority = 0;       ///< Priority (higher = more urgent)
};

/**
 * @class AudioQueue
 * @brief Thread-safe queue for audio items
 *
 * Manages a queue of audio files to be played sequentially.
 */
class AudioQueue : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new Audio Queue
     * @param parent Parent QObject
     */
    explicit AudioQueue(QObject* parent = nullptr);

    /**
     * @brief Destroy the Audio Queue
     */
    ~AudioQueue() override;

    /**
     * @brief Enqueue an audio item
     * @param item Audio item to add
     */
    void enqueue(const AudioItem& item);

    /**
     * @brief Enqueue a file path
     * @param path File path
     * @param priority Priority level
     */
    void enqueueFile(const QString& path, int priority = 0);

    /**
     * @brief Enqueue a resource path
     * @param resource Resource path
     * @param priority Priority level
     */
    void enqueueResource(const QString& resource, int priority = 0);

    /**
     * @brief Dequeue the next item
     * @return Next audio item (empty path if queue empty)
     */
    AudioItem dequeue();

    /**
     * @brief Peek at the next item without removing
     * @return Next audio item (empty path if queue empty)
     */
    AudioItem peek() const;

    /**
     * @brief Check if queue is empty
     * @return true if empty
     */
    bool isEmpty() const;

    /**
     * @brief Get queue size
     * @return Number of items in queue
     */
    int size() const;

    /**
     * @brief Clear all items from queue
     */
    void clear();

signals:
    /**
     * @brief Emitted when an item is added
     */
    void itemEnqueued();

    /**
     * @brief Emitted when queue becomes empty
     */
    void queueEmpty();

private:
    mutable QMutex m_mutex;
    QQueue<AudioItem> m_queue;
};

} // namespace audio
} // namespace multipack

#endif // MULTIPACK_AUDIO_AUDIOQUEUE_H
