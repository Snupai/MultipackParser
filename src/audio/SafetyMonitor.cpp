/**
 * @file SafetyMonitor.cpp
 * @brief Implementation of safety monitoring with audio alerts
 */

#include "multipack/audio/SafetyMonitor.h"
#include "multipack/audio/AudioManager.h"
#include "multipack/robot/RobotController.h"
#include "multipack/robot/RobotEnums.h"

#include <QDebug>

namespace multipack {
namespace audio {

SafetyMonitor::SafetyMonitor(AudioManager* audioManager,
                             robot::RobotController* robotController,
                             QObject* parent)
    : QObject(parent)
    , m_audioManager(audioManager)
    , m_robotController(robotController)
    , m_timer(std::make_unique<QTimer>(this))
    , m_lastStatus(robot::SafetyStatus::Normal)
{
    qDebug() << "SafetyMonitor::SafetyMonitor - constructor";

    connect(m_timer.get(), &QTimer::timeout, this, &SafetyMonitor::onTimerTick);

    if (m_robotController) {
        connect(m_robotController, &robot::RobotController::safetyStatusChanged,
                this, &SafetyMonitor::onSafetyStatusChanged);
    }
}

SafetyMonitor::~SafetyMonitor()
{
    qDebug() << "SafetyMonitor::~SafetyMonitor - destructor";
    stop();
}

void SafetyMonitor::start(int intervalMs)
{
    qDebug() << "SafetyMonitor::start - interval:" << intervalMs << "ms";
    m_timer->start(intervalMs);
}

void SafetyMonitor::stop()
{
    qDebug() << "SafetyMonitor::stop";
    m_timer->stop();
    clearAlarm();
}

bool SafetyMonitor::isActive() const
{
    return m_timer->isActive();
}

void SafetyMonitor::setAudioAlertsEnabled(bool enabled)
{
    m_audioAlertsEnabled = enabled;
    if (!enabled) {
        clearAlarm();
    }
}

bool SafetyMonitor::audioAlertsEnabled() const
{
    return m_audioAlertsEnabled;
}

void SafetyMonitor::onSafetyStatusChanged(robot::SafetyStatus status)
{
    qDebug() << "SafetyMonitor::onSafetyStatusChanged - status:"
             << static_cast<int>(status);

    if (status == m_lastStatus) {
        return;
    }

    m_lastStatus = status;

    switch (status) {
        case robot::SafetyStatus::Normal:
        case robot::SafetyStatus::ReducedMode:
            clearAlarm();
            emit safetyRestored();
            break;

        case robot::SafetyStatus::ProtectiveStop:
        case robot::SafetyStatus::Recovery:
        case robot::SafetyStatus::SafeguardStop:
        case robot::SafetyStatus::SystemEmergencyStop:
        case robot::SafetyStatus::RobotEmergencyStop:
        case robot::SafetyStatus::Violation:
        case robot::SafetyStatus::Fault:
            triggerAlarm(status);
            break;

        default:
            qWarning() << "Unknown safety status:" << static_cast<int>(status);
            break;
    }
}

void SafetyMonitor::checkNow()
{
    checkSafetyStatus();
}

void SafetyMonitor::onTimerTick()
{
    checkSafetyStatus();
}

void SafetyMonitor::checkSafetyStatus()
{
    if (!m_robotController) {
        return;
    }

    // Query current safety status from robot controller
    robot::SafetyStatus status = m_robotController->safetyStatus();
    if (status != m_lastStatus) {
        onSafetyStatusChanged(status);
    }
}

void SafetyMonitor::triggerAlarm(robot::SafetyStatus status)
{
    if (!m_audioAlertsEnabled || m_alarmActive) {
        return;
    }

    QString message;
    switch (status) {
        case robot::SafetyStatus::ProtectiveStop:
            message = tr("Protective stop activated");
            break;
        case robot::SafetyStatus::SystemEmergencyStop:
        case robot::SafetyStatus::RobotEmergencyStop:
            message = tr("Emergency stop activated");
            break;
        case robot::SafetyStatus::SafeguardStop:
            message = tr("Safeguard stop activated");
            break;
        case robot::SafetyStatus::Violation:
            message = tr("Safety violation detected");
            break;
        case robot::SafetyStatus::Fault:
            message = tr("Safety system fault");
            break;
        default:
            message = tr("Safety alert");
            break;
    }

    qWarning() << "SafetyMonitor: Triggering alarm -" << message;

    m_alarmActive = true;

    if (m_audioManager) {
        m_audioManager->startAlarm();
    }

    emit safetyAlert(message);
}

void SafetyMonitor::clearAlarm()
{
    if (!m_alarmActive) {
        return;
    }

    qDebug() << "SafetyMonitor::clearAlarm";

    m_alarmActive = false;

    if (m_audioManager) {
        m_audioManager->stopAlarm();
    }
}

} // namespace audio
} // namespace multipack
