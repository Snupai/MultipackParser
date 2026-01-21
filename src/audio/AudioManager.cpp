/**
 * @file AudioManager.cpp
 * @brief Implementation of audio playback manager
 */

#include "multipack/audio/AudioManager.h"
#include "multipack/audio/AudioQueue.h"

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QDebug>

namespace multipack {
namespace audio {

AudioManager::AudioManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "AudioManager::AudioManager - constructor";
}

AudioManager::~AudioManager()
{
    qDebug() << "AudioManager::~AudioManager - destructor";
    stop();
}

bool AudioManager::initialize()
{
    qDebug() << "AudioManager::initialize - setting up audio system";

    m_player = std::make_unique<QMediaPlayer>(this);
    m_audioOutput = std::make_unique<QAudioOutput>(this);
    m_queue = std::make_unique<AudioQueue>(this);

    m_player->setAudioOutput(m_audioOutput.get());

    // Connect signals
    connect(m_player.get(), &QMediaPlayer::mediaStatusChanged,
            this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            onPlaybackFinished();
        }
    });

    connect(m_player.get(), &QMediaPlayer::errorOccurred,
            this, [this](QMediaPlayer::Error error, const QString& errorString) {
        Q_UNUSED(error);
        qWarning() << "Audio playback error:" << errorString;
        emit playbackError(errorString);
    });

    connect(m_queue.get(), &AudioQueue::itemEnqueued, this, &AudioManager::playNext);

    m_initialized = true;
    qDebug() << "AudioManager::initialize - complete";
    return true;
}

bool AudioManager::isEnabled() const
{
    return m_enabled;
}

void AudioManager::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        stop();
    }
}

float AudioManager::volume() const
{
    if (m_audioOutput) {
        return m_audioOutput->volume();
    }
    return 1.0f;
}

void AudioManager::setVolume(float volume)
{
    if (m_audioOutput) {
        m_audioOutput->setVolume(qBound(0.0f, volume, 1.0f));
    }
}

bool AudioManager::isPlaying() const
{
    if (m_player) {
        return m_player->playbackState() == QMediaPlayer::PlayingState;
    }
    return false;
}

void AudioManager::playNotification(AudioType type)
{
    if (!m_enabled || !m_initialized) return;

    QString resource = getResourcePath(type);
    playResource(resource);
}

void AudioManager::playFile(const QString& path)
{
    if (!m_enabled || !m_initialized) return;

    qDebug() << "AudioManager::playFile -" << path;

    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->play();

    emit playbackStarted();
}

void AudioManager::playResource(const QString& resource)
{
    if (!m_enabled || !m_initialized) return;

    qDebug() << "AudioManager::playResource -" << resource;

    m_player->setSource(QUrl(resource));
    m_player->play();

    emit playbackStarted();
}

void AudioManager::startAlarm()
{
    if (!m_enabled || !m_initialized) return;

    qDebug() << "AudioManager::startAlarm";

    m_alarmActive = true;
    m_player->setLoops(QMediaPlayer::Infinite);
    playResource(getResourcePath(AudioType::Alarm));
}

void AudioManager::stopAlarm()
{
    qDebug() << "AudioManager::stopAlarm";

    m_alarmActive = false;
    m_player->setLoops(1);
    stop();
}

void AudioManager::stop()
{
    if (m_player) {
        m_player->stop();
    }
}

void AudioManager::queueFile(const QString& path)
{
    if (m_queue) {
        m_queue->enqueueFile(path);
    }
}

void AudioManager::clearQueue()
{
    if (m_queue) {
        m_queue->clear();
    }
}

void AudioManager::onPlaybackFinished()
{
    qDebug() << "AudioManager::onPlaybackFinished";

    if (m_alarmActive) {
        // Alarm will loop automatically
        return;
    }

    emit playbackFinished();

    // Play next queued item
    playNext();
}

QString AudioManager::getResourcePath(AudioType type) const
{
    switch (type) {
        case AudioType::Info:
            return "qrc:/audio/info.wav";
        case AudioType::Warning:
            return "qrc:/audio/warning.wav";
        case AudioType::Error:
            return "qrc:/audio/warning.wav";  // Use warning for errors too
        case AudioType::Success:
            return "qrc:/audio/output.wav";
        case AudioType::Alarm:
            return "qrc:/audio/warning.wav";  // Use warning as alarm
        case AudioType::StepBack:
            return "qrc:/audio/stepback.wav";
        case AudioType::Output:
            return "qrc:/audio/output.wav";
        default:
            return QString();
    }
}

void AudioManager::playNext()
{
    if (!m_queue || m_queue->isEmpty()) {
        return;
    }

    if (isPlaying()) {
        return;  // Wait for current playback to finish
    }

    AudioItem item = m_queue->dequeue();
    if (item.filePath.isEmpty()) {
        return;
    }

    if (item.isResource) {
        playResource(item.filePath);
    } else {
        playFile(item.filePath);
    }
}

} // namespace audio
} // namespace multipack
