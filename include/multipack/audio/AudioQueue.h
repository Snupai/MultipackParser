/**
 * @file AudioQueue.h
 * @brief Thread-safe audio playback queue with playback counting
 *
 * Provides a queue for sequential audio playback with support for:
 * - ID-based item tracking and removal
 * - Playback count control (-1 for infinite, positive for specific count)
 * - Thread-safe operations
 * - Automatic rotation for infinite playback items
 */

#ifndef MULTIPACK_AUDIO_AUDIOQUEUE_H
#define MULTIPACK_AUDIO_AUDIOQUEUE_H

#include <QObject>
#include <QString>
#include <QQueue>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <deque>

namespace multipack {
namespace audio {

/**
 * @struct AudioItem
 * @brief Item in the audio queue with playback control
 */
struct AudioItem {
    QString id;              ///< Unique identifier for this audio item
    QString filePath;        ///< Path to audio file
    int playbackCount = 1;   ///< Number of times to play (-1 = infinite)
    int currentCount = 0;    ///< Current playback count
    int priority = 0;        ///< Priority (higher = more urgent)
    bool isResource = false; ///< True if resource path (qrc:/)
    
    // Legacy compatibility
    QString path() const { return filePath; }
};

/**
 * @class AudioQueue
 * @brief Thread-safe queue for audio items with playback management
 *
 * Manages a queue of audio files to be played sequentially.
 * Features:
 * - Add items with specific playback counts or infinite loop
 * - Stop specific audio by ID
 * - Thread-safe operations with mutex protection
 * - Signal emission for queue state changes
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
     * @brief Add audio to queue with playback count (Python-compatible)
     * @param id Unique identifier for this audio
     * @param filePath Path to audio file
     * @param playbackCount Number of times to play (-1 = infinite, positive = specific count)
     */
    void addToQueue(const QString& id, const QString& filePath, int playbackCount);

    /**
     * @brief Stop playback of a specific audio by ID
     * @param id Audio ID to stop
     * @return true if audio was found and stopped
     */
    bool stopAudio(const QString& id);

    /**
     * @brief Stop all audio playback and clear the queue
     */
    void killAll();

    /**
     * @brief Enqueue an audio item (legacy method)
     * @param item Audio item to add
     */
    void enqueue(const AudioItem& item);

    /**
     * @brief Enqueue a file path (legacy method)
     * @param path File path
     * @param priority Priority level
     */
    void enqueueFile(const QString& path, int priority = 0);

    /**
     * @brief Enqueue a resource path (legacy method)
     * @param resource Resource path
     * @param priority Priority level
     */
    void enqueueResource(const QString& resource, int priority = 0);

    /**
     * @brief Dequeue the next item
     * @return Next audio item (empty id if queue empty)
     */
    AudioItem dequeue();

    /**
     * @brief Peek at the next item without removing
     * @return Next audio item (empty id if queue empty)
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

    /**
     * @brief Check if currently playing
     * @return true if playback is active
     */
    bool isPlaying() const;

    /**
     * @brief Get current item being played
     * @return Current audio item (empty if none)
     */
    AudioItem currentItem() const;

    /**
     * @brief Mark current item as played once (increment counter)
     * Called by AudioManager when playback completes
     */
    void markCurrentPlayed();

    /**
     * @brief Check if current item should be removed (playback count reached)
     * @return true if current item is done
     */
    bool isCurrentItemDone() const;

    /**
     * @brief Move to next item (handle rotation for infinite items)
     */
    void advanceQueue();

signals:
    /**
     * @brief Emitted when an item is added
     */
    void itemEnqueued();

    /**
     * @brief Emitted when queue becomes empty
     */
    void queueEmpty();

    /**
     * @brief Emitted when an audio item starts playing
     * @param id Audio ID
     */
    void playbackStarted(const QString& id);

    /**
     * @brief Emitted when an audio item finishes
     * @param id Audio ID
     */
    void playbackFinished(const QString& id);

    /**
     * @brief Emitted when audio is stopped by ID
     * @param id Audio ID that was stopped
     */
    void audioStopped(const QString& id);

private:
    mutable QMutex m_mutex;
    std::deque<AudioItem> m_queue;  // Using deque for rotation support
    AudioItem m_currentItem;
    bool m_isPlaying = false;
};

} // namespace audio
} // namespace multipack

#endif // MULTIPACK_AUDIO_AUDIOQUEUE_H
