/**
 * @file RobotStatusMonitor.cpp
 * @brief Implementation of robot status monitoring
 */

#include "multipack/robot/RobotStatusMonitor.h"
#include "multipack/robot/DashboardClient.h"
#include "multipack/core/GlobalState.h"

#include <QDebug>
#include <QMutexLocker>

namespace multipack {
namespace robot {

RobotStatusMonitor::RobotStatusMonitor(QObject* parent)
    : QObject(parent)
    , m_robotIp("192.168.0.1")
{
}

RobotStatusMonitor::~RobotStatusMonitor()
{
    // Set flag first to prevent any connection attempts
    m_shouldStop.store(true);
    stop();
}

void RobotStatusMonitor::setRobotIp(const QString& ip)
{
    m_robotIp = ip;
}

QString RobotStatusMonitor::robotIp() const
{
    return m_robotIp;
}

void RobotStatusMonitor::start(int intervalMs)
{
    // Reset stop flag
    m_shouldStop.store(false);

    if (!m_client) {
        m_client = std::make_unique<DashboardClient>(this);
    }

    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &RobotStatusMonitor::onPollTimeout);
    }

    if (m_pollTimer->isActive()) {
        return;
    }

    qDebug() << "RobotStatusMonitor: Starting monitoring at" << intervalMs << "ms interval";
    m_pollTimer->start(intervalMs);

    // Do an immediate poll
    updateNow();
}

void RobotStatusMonitor::stop()
{
    // Set flag to stop any pending connection attempts
    m_shouldStop.store(true);

    if (m_pollTimer && m_pollTimer->isActive()) {
        qDebug() << "RobotStatusMonitor: Stopping monitoring";
        m_pollTimer->stop();
    }

    // Force disconnect even if not fully connected
    if (m_client) {
        m_client->disconnect();
    }
}

bool RobotStatusMonitor::isRunning() const
{
    return m_pollTimer && m_pollTimer->isActive();
}

RobotStatus RobotStatusMonitor::currentStatus() const
{
    return m_status;
}

void RobotStatusMonitor::updateNow()
{
    onPollTimeout();
}

QString RobotStatusMonitor::getPolyscopeVersion()
{
    QMutexLocker locker(&m_detailMutex);
    return m_cachedPolyscopeVersion;
}

QString RobotStatusMonitor::getLoadedProgram()
{
    QMutexLocker locker(&m_detailMutex);
    return m_cachedLoadedProgram;
}

QString RobotStatusMonitor::getSerialNumber()
{
    QMutexLocker locker(&m_detailMutex);
    return m_cachedSerialNumber;
}

void RobotStatusMonitor::onPollTimeout()
{
    // Check if we should stop
    if (m_shouldStop.load()) {
        return;
    }

    if (!m_client) {
        return;
    }

    // Ensure connected
    if (!m_client->isConnected()) {
        if (!m_client->connect(m_robotIp)) {
            // Connection failed
            if (m_wasConnected) {
                m_wasConnected = false;
                m_status.isConnected = false;
                m_status.connectionError = "Connection failed";
                emit connectionChanged(false);
            }
            m_status.robotMode = RobotMode::Unknown;
            m_status.safetyStatus = SafetyStatus::Unknown;
            m_status.programState = ProgramState::Unknown;
            return;
        }
    }

    // Poll status
    QString modeResponse = m_client->getRobotMode();
    QString safetyResponse = m_client->getSafetyMode();
    QString progResponse = m_client->getProgramState();

    // Check if any succeeded
    bool anySuccess = !modeResponse.isEmpty() ||
                      !safetyResponse.isEmpty() ||
                      !progResponse.isEmpty();

    // Parse responses
    RobotMode newMode = parseRobotMode(modeResponse);
    SafetyStatus newSafety = parseSafetyStatus(safetyResponse);
    ProgramState newProgram = parseProgramState(progResponse);

    // Emit signals on changes
    if (newMode != m_status.robotMode) {
        m_status.robotMode = newMode;
        emit robotModeChanged(newMode);
    }

    if (newSafety != m_status.safetyStatus) {
        m_status.safetyStatus = newSafety;
        emit safetyStatusChanged(newSafety);
    }

    if (newProgram != m_status.programState) {
        m_status.programState = newProgram;
        emit programStateChanged(newProgram);
    }

    // Update connection status
    m_status.lastUpdate = QDateTime::currentDateTime();
    m_status.isConnected = anySuccess;

    if (m_status.isConnected && (++m_detailCounter % 10 == 0)) {
        QString polyscope = m_client->getPolyscopeVersion();
        QString serial = m_client->getSerialNumber();
        QString program = m_client->getLoadedProgram();

        {
            QMutexLocker locker(&m_detailMutex);
            if (!polyscope.isEmpty()) {
                m_cachedPolyscopeVersion = polyscope;
            }
            if (!serial.isEmpty()) {
                m_cachedSerialNumber = serial;
            }
            if (!program.isEmpty()) {
                m_cachedLoadedProgram = program;
            }
        }

        emit detailsUpdated(m_cachedPolyscopeVersion, m_cachedSerialNumber, m_cachedLoadedProgram);
    }

    if (anySuccess && !m_wasConnected) {
        m_wasConnected = true;
        m_status.connectionError.clear();
        emit connectionChanged(true);
    } else if (!anySuccess && m_wasConnected) {
        m_wasConnected = false;
        m_status.connectionError = "Lost connection";
        emit connectionChanged(false);
    }

    // Update global state
    auto& state = core::GlobalState::instance();
    state.setRobotConnected(m_status.isConnected);

    emit statusUpdated(m_status);
}

RobotMode RobotStatusMonitor::parseRobotMode(const QString& response)
{
    // Response format: "Robotmode: RUNNING" or similar
    QString upper = response.toUpper();

    if (upper.contains("RUNNING")) {
        return RobotMode::Running;
    } else if (upper.contains("IDLE")) {
        return RobotMode::Idle;
    } else if (upper.contains("POWER_OFF")) {
        return RobotMode::PowerOff;
    } else if (upper.contains("BACKDRIVE")) {
        return RobotMode::BackDrive;
    } else if (upper.contains("CONFIRM_SAFETY")) {
        return RobotMode::ConfirmSafety;
    } else if (upper.contains("BOOTING")) {
        return RobotMode::Booting;
    } else if (upper.contains("NO_CONTROLLER")) {
        return RobotMode::NoController;
    } else if (upper.contains("DISCONNECTED")) {
        return RobotMode::Disconnected;
    }

    return RobotMode::Unknown;
}

SafetyStatus RobotStatusMonitor::parseSafetyStatus(const QString& response)
{
    // Response format: "Safetymode: NORMAL" or similar
    QString upper = response.toUpper();

    if (upper.contains("NORMAL")) {
        return SafetyStatus::Normal;
    } else if (upper.contains("REDUCED")) {
        return SafetyStatus::ReducedMode;
    } else if (upper.contains("PROTECTIVE_STOP")) {
        return SafetyStatus::ProtectiveStop;
    } else if (upper.contains("RECOVERY")) {
        return SafetyStatus::Recovery;
    } else if (upper.contains("SAFEGUARD_STOP")) {
        return SafetyStatus::SafeguardStop;
    } else if (upper.contains("SYSTEM_EMERGENCY_STOP")) {
        return SafetyStatus::SystemEmergencyStop;
    } else if (upper.contains("ROBOT_EMERGENCY_STOP")) {
        return SafetyStatus::RobotEmergencyStop;
    } else if (upper.contains("VIOLATION")) {
        return SafetyStatus::Violation;
    } else if (upper.contains("FAULT")) {
        return SafetyStatus::Fault;
    }

    return SafetyStatus::Unknown;
}

ProgramState RobotStatusMonitor::parseProgramState(const QString& response)
{
    // Response format: "PLAYING" or "STOPPED program_name.urp"
    QString upper = response.toUpper();

    if (upper.contains("PLAYING")) {
        return ProgramState::Playing;
    } else if (upper.contains("PAUSED")) {
        return ProgramState::Paused;
    } else if (upper.contains("STOPPED")) {
        return ProgramState::Stopped;
    }

    return ProgramState::Unknown;
}

} // namespace robot
} // namespace multipack
