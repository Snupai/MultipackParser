/**
 * @file SafetyMonitor.cpp
 * @brief Enhanced safety monitoring with timed warnings and comprehensive status management
 */

#include "multipack/audio/SafetyMonitor.h"
#include "multipack/audio/AudioManager.h"
#include "multipack/robot/RobotController.h"
#include "multipack/robot/RobotEnums.h"
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QCoreApplication>

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
    // Initialize timer for periodic checks
    m_timer->setInterval(1000); // Check every second
    connect(m_timer.get(), &QTimer::timeout, this, &SafetyMonitor::onTimerTick);
    
    qDebug() << "SafetyMonitor initialized";
}

SafetyMonitor::~SafetyMonitor()
{
    stop();
    qDebug() << "SafetyMonitor destroyed";
}

void SafetyMonitor::start(int intervalMs)
{
    if (!m_timer->isActive()) {
        m_timer->setInterval(intervalMs);
        m_timer->start();
        qDebug() << "SafetyMonitor started periodic monitoring with interval:" << intervalMs << "ms";
    }
}

void SafetyMonitor::stop()
{
    if (m_timer->isActive()) {
        m_timer->stop();
        qDebug() << "SafetyMonitor stopped periodic monitoring";
    }
}

bool SafetyMonitor::isActive() const
{
    return m_timer->isActive();
}

void SafetyMonitor::setAudioAlertsEnabled(bool enabled)
{
    m_audioAlertsEnabled = enabled;
    qDebug() << "SafetyMonitor audio alerts enabled:" << enabled;
}

bool SafetyMonitor::audioAlertsEnabled() const
{
    return m_audioAlertsEnabled;
}

void SafetyMonitor::onSafetyStatusChanged(robot::SafetyStatus status)
{
    QString statusString;
    switch (status) {
        case robot::SafetyStatus::Normal: statusString = "Normal"; break;
        case robot::SafetyStatus::ReducedMode: statusString = "ReducedMode"; break;
        case robot::SafetyStatus::ProtectiveStop: statusString = "ProtectiveStop"; break;
        case robot::SafetyStatus::Recovery: statusString = "Recovery"; break;
        case robot::SafetyStatus::SafeguardStop: statusString = "SafeguardStop"; break;
        case robot::SafetyStatus::SystemEmergencyStop: statusString = "SystemEmergencyStop"; break;
        case robot::SafetyStatus::RobotEmergencyStop: statusString = "RobotEmergencyStop"; break;
        case robot::SafetyStatus::Violation: statusString = "Violation"; break;
        case robot::SafetyStatus::Fault: statusString = "Fault"; break;
        default: statusString = "Unknown"; break;
    }
    
    qDebug() << "Safety status changed to:" << statusString;
    
    m_lastStatus = status;
    checkSafetyStatus();
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

    robot::SafetyStatus currentStatus = m_robotController->safetyStatus();
    
    if (currentStatus != m_lastStatus) {
        onSafetyStatusChanged(currentStatus);
        m_lastStatus = currentStatus;
    }

    // Handle REDUCED mode with 30-second warning intervals (matching Python behavior)
    if (currentStatus == robot::SafetyStatus::ReducedMode) {
        if (m_alarmActive && m_audioManager) {
            m_audioManager->stopAlarm();
            m_alarmActive = false;
        }
        QDateTime currentTime = QDateTime::currentDateTime();
        
        if (!m_inReducedMode) {
            // First time entering reduced mode
            m_reducedModeStartTime = currentTime;
            m_inReducedMode = true;
            m_lastWarningTime = QDateTime(); // Reset last warning time
            qDebug() << "SafetyMonitor: Entered REDUCED mode, starting 30-second warning timer";
        }
        
        bool shouldPlayWarning = false;
        
        if (!m_lastWarningTime.isValid()) {
            // First time - wait 30 seconds before first warning
            if (m_reducedModeStartTime.secsTo(currentTime) >= WARNING_INTERVAL_SECONDS) {
                shouldPlayWarning = true;
            }
        } else {
            // Check if 30 seconds have passed since last warning
            if (m_lastWarningTime.secsTo(currentTime) >= WARNING_INTERVAL_SECONDS) {
                shouldPlayWarning = true;
            }
        }
        
        if (shouldPlayWarning) {
            qDebug() << "SafetyMonitor: Robot in REDUCED mode, playing warning sound (once every 30 seconds)";
            if (m_audioAlertsEnabled && m_audioManager) {
                m_audioManager->playNotification(AudioType::Warning);
            }
            m_lastWarningTime = currentTime;
            
            if (!m_alarmActive) {
                m_alarmActive = true;
                emit safetyAlert("Reduced Mode - Warning (repeats every 30 seconds)");
            }
        }
    } else {
        // Not in reduced mode - reset tracking
        if (m_inReducedMode) {
            qDebug() << "SafetyMonitor: Exited REDUCED mode";
            m_inReducedMode = false;
            m_reducedModeStartTime = QDateTime();
            m_lastWarningTime = QDateTime();
        }
        
        // Handle other safety statuses
        if (currentStatus != robot::SafetyStatus::Normal) {
            if (!m_alarmActive) {
                triggerAlarm(currentStatus);
            } else {
                QDateTime currentTime = QDateTime::currentDateTime();
                if (!m_lastAlarmTime.isValid() || m_lastAlarmTime.secsTo(currentTime) >= WARNING_INTERVAL_SECONDS) {
                    if (m_audioAlertsEnabled && m_audioManager) {
                        m_audioManager->playNotification(AudioType::Alarm);
                    }
                    m_lastAlarmTime = currentTime;
                }
            }
        } else if (m_alarmActive) {
            clearAlarm();
        }
    }
}

void SafetyMonitor::triggerAlarm(robot::SafetyStatus status)
{
    QString statusString;
    switch (status) {
        case robot::SafetyStatus::ReducedMode: statusString = "Reduced Mode - Warning"; break;
        case robot::SafetyStatus::ProtectiveStop: statusString = "Protective Stop - Safety Alert"; break;
        case robot::SafetyStatus::SafeguardStop: statusString = "Safeguard Stop - Safety Alert"; break;
        case robot::SafetyStatus::SystemEmergencyStop: statusString = "System Emergency Stop - Critical Alert"; break;
        case robot::SafetyStatus::RobotEmergencyStop: statusString = "Robot Emergency Stop - Critical Alert"; break;
        case robot::SafetyStatus::Violation: statusString = "Safety Violation - Critical Alert"; break;
        case robot::SafetyStatus::Fault: statusString = "System Fault - Critical Alert"; break;
        default: statusString = "Safety Alert"; break;
    }
    
    qDebug() << "SafetyMonitor: Alarm triggered -" << statusString;
    m_alarmActive = true;

    m_lastAlarmTime = QDateTime::currentDateTime();
    
    if (m_audioAlertsEnabled && m_audioManager) {
        m_audioManager->playNotification(AudioType::Alarm);
    }
    
    emit safetyAlert(statusString);
}

void SafetyMonitor::clearAlarm()
{
    qDebug() << "SafetyMonitor: Alarm cleared - Safety restored";
    m_alarmActive = false;
    if (m_audioManager) {
        m_audioManager->stopAlarm();
    }
    m_lastAlarmTime = QDateTime();
    emit safetyRestored();
}

} // namespace audio
} // namespace multipack
