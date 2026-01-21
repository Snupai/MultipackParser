/**
 * @file SafetyMonitor.h
 * @brief Safety monitoring with audio alerts
 *
 * Monitors robot safety state and triggers audio alerts.
 */

#ifndef MULTIPACK_AUDIO_SAFETYMONITOR_H
#define MULTIPACK_AUDIO_SAFETYMONITOR_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <memory>

namespace multipack {

namespace robot {
class RobotController;
enum class SafetyStatus;
}

namespace audio {

class AudioManager;

/**
 * @class SafetyMonitor
 * @brief Monitors robot safety and triggers audio alerts
 *
 * Runs in background and triggers audio alerts when
 * safety status changes.
 */
class SafetyMonitor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new Safety Monitor
     * @param audioManager Audio manager for alerts
     * @param robotController Robot controller to monitor
     * @param parent Parent QObject
     */
    SafetyMonitor(AudioManager* audioManager,
                  robot::RobotController* robotController,
                  QObject* parent = nullptr);

    /**
     * @brief Destroy the Safety Monitor
     */
    ~SafetyMonitor() override;

    /**
     * @brief Start monitoring
     * @param intervalMs Check interval in milliseconds
     */
    void start(int intervalMs = 1000);

    /**
     * @brief Stop monitoring
     */
    void stop();

    /**
     * @brief Check if monitoring is active
     * @return true if active
     */
    bool isActive() const;

    /**
     * @brief Enable or disable audio alerts
     * @param enabled Whether to enable alerts
     */
    void setAudioAlertsEnabled(bool enabled);

    /**
     * @brief Check if audio alerts are enabled
     * @return true if enabled
     */
    bool audioAlertsEnabled() const;

public slots:
    /**
     * @brief Handle safety status change
     * @param status New safety status
     */
    void onSafetyStatusChanged(robot::SafetyStatus status);

    /**
     * @brief Force check now
     */
    void checkNow();

signals:
    /**
     * @brief Emitted when safety alert triggered
     * @param message Alert message
     */
    void safetyAlert(const QString& message);

    /**
     * @brief Emitted when safety is restored
     */
    void safetyRestored();

private slots:
    /**
     * @brief Periodic check handler
     */
    void onTimerTick();

private:
    /**
     * @brief Check current safety status
     */
    void checkSafetyStatus();

    /**
     * @brief Trigger alarm for safety issue
     * @param status Current status
     */
    void triggerAlarm(robot::SafetyStatus status);

    /**
     * @brief Clear alarm
     */
    void clearAlarm();

    AudioManager* m_audioManager;
    robot::RobotController* m_robotController;
    std::unique_ptr<QTimer> m_timer;

    bool m_audioAlertsEnabled = true;
    bool m_alarmActive = false;
    robot::SafetyStatus m_lastStatus;
    
    // 30-second warning interval tracking (like Python version)
    static constexpr int WARNING_INTERVAL_SECONDS = 30;
    QDateTime m_reducedModeStartTime;
    QDateTime m_lastWarningTime;
    bool m_inReducedMode = false;
};

} // namespace audio
} // namespace multipack

#endif // MULTIPACK_AUDIO_SAFETYMONITOR_H
