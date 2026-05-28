/**
 * @file AudioManager.h
 * @brief Audio playback manager using Qt Multimedia
 *
 * Manages audio playback for notifications and alerts.
 */

#ifndef MULTIPACK_AUDIO_AUDIOMANAGER_H
#define MULTIPACK_AUDIO_AUDIOMANAGER_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QHash>
#include <memory>

class QMediaPlayer;
class QAudioOutput;

namespace multipack {
namespace audio {

class AudioQueue;

/**
 * @enum AudioType
 * @brief Types of audio notifications
 */
enum class AudioType {
    Info,       ///< Informational sound (info.wav)
    Warning,    ///< Warning sound (warning.wav)
    ScannerWarning, ///< Scanner warning sound
    Error,      ///< Error sound
    Success,    ///< Success sound
    Alarm,      ///< Continuous alarm
    StepBack,   ///< Step back notification (stepback.wav)
    Output,     ///< Output notification (output.wav)
    Custom      ///< Custom audio file
};

/**
 * @class AudioManager
 * @brief Manages audio playback
 *
 * Thread-safe audio manager that handles playback
 * of notification sounds and alarms.
 */
class AudioManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new Audio Manager
     * @param parent Parent QObject
     */
    explicit AudioManager(QObject* parent = nullptr);

    /**
     * @brief Destroy the Audio Manager
     */
    ~AudioManager() override;

    /**
     * @brief Initialize the audio system
     * @return true on success
     */
    bool initialize();

    /**
     * @brief Check if audio is enabled
     * @return true if enabled
     */
    bool isEnabled() const;

    /**
     * @brief Enable or disable audio
     * @param enabled Whether to enable audio
     */
    void setEnabled(bool enabled);

    /**
     * @brief Get current volume
     * @return Volume (0.0 - 1.0)
     */
    float volume() const;

    /**
     * @brief Set volume
     * @param volume Volume (0.0 - 1.0)
     */
    void setVolume(float volume);

    /**
     * @brief Check if currently playing
     * @return true if playing
     */
    bool isPlaying() const;

    /**
     * @brief Play a notification sound
     * @param type Type of notification
     */
    void playNotification(AudioType type);

    /**
     * @brief Play a custom audio file
     * @param path Path to audio file
     */
    void playFile(const QString& path);

    /**
     * @brief Play audio from Qt resource
     * @param resource Resource path (e.g., ":/audio/beep.wav")
     */
    void playResource(const QString& resource);

    /**
     * @brief Set a custom audio file for a notification type
     * @param type Notification type
     * @param path Path to audio file (empty to clear)
     */
    void setCustomFile(AudioType type, const QString& path);

    /**
     * @brief Start playing alarm (loops until stopped)
     */
    void startAlarm();

    /**
     * @brief Stop alarm
     */
    void stopAlarm();

    /**
     * @brief Stop all audio
     */
    void stop();

    /**
     * @brief Queue an audio file for playback
     * @param path Path to audio file
     */
    void queueFile(const QString& path);

    /**
     * @brief Clear the playback queue
     */
    void clearQueue();

public slots:
    /**
     * @brief Handle playback completion
     */
    void onPlaybackFinished();

signals:
    /**
     * @brief Emitted when playback starts
     */
    void playbackStarted();

    /**
     * @brief Emitted when playback ends
     */
    void playbackFinished();

    /**
     * @brief Emitted on playback error
     * @param error Error message
     */
    void playbackError(const QString& error);

private:
    /**
     * @brief Get resource path for notification type
     * @param type Notification type
     * @return Resource path
     */
    QString getResourcePath(AudioType type) const;
    QString customFilePath(AudioType type) const;

    /**
     * @brief Play next item from queue
     */
    void playNext();

    std::unique_ptr<QMediaPlayer> m_player;
    std::unique_ptr<QAudioOutput> m_audioOutput;
    std::unique_ptr<AudioQueue> m_queue;

    bool m_enabled = true;
    bool m_alarmActive = false;
    bool m_initialized = false;
    QHash<int, QString> m_customFiles;
};

} // namespace audio
} // namespace multipack

#endif // MULTIPACK_AUDIO_AUDIOMANAGER_H
