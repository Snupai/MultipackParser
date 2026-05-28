/**
 * @file RobotController.cpp
 * @brief Full implementation of UR Dashboard Server communication
 */
#include "multipack/robot/RobotController.h"

#include <QDebug>
#include <QEventLoop>

namespace multipack {
namespace robot {

RobotController::RobotController(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_pollTimer(new QTimer(this))
    , m_port(DASHBOARD_PORT)
    , m_connected(false)
    , m_robotMode(RobotMode::Unknown)
    , m_safetyStatus(SafetyStatus::Unknown)
    , m_programState(ProgramState::Unknown)
{
    qDebug() << "RobotController - initialized";

    // Connect socket signals
    QObject::connect(m_socket, &QTcpSocket::connected,
                     this, &RobotController::onConnected);
    QObject::connect(m_socket, &QTcpSocket::disconnected,
                     this, &RobotController::onDisconnected);
    QObject::connect(m_socket, &QTcpSocket::errorOccurred,
                     this, &RobotController::onSocketError);
    QObject::connect(m_socket, &QTcpSocket::readyRead,
                     this, &RobotController::onReadyRead);

    // Connect poll timer
    QObject::connect(m_pollTimer, &QTimer::timeout,
                     this, &RobotController::pollStatus);
}

RobotController::~RobotController()
{
    disconnect();
}

bool RobotController::connect(const QString& ip, int port)
{
    if (m_connected) {
        disconnect();
    }

    qDebug() << "RobotController::connect -" << ip << ":" << port;

    m_robotIp = ip;
    m_port = port;

    m_socket->connectToHost(ip, port);

    // Wait for connection with timeout
    if (!m_socket->waitForConnected(SOCKET_TIMEOUT)) {
        qWarning() << "Failed to connect to robot:" << m_socket->errorString();
        emit connectionError(m_socket->errorString());
        return false;
    }

    return m_connected;
}

void RobotController::disconnect()
{
    if (m_connected) {
        m_pollTimer->stop();
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
        m_connected = false;
        emit disconnected();
    }
}

bool RobotController::isConnected() const
{
    return m_connected && m_socket->state() == QAbstractSocket::ConnectedState;
}

RobotMode RobotController::robotMode() const
{
    return m_robotMode;
}

SafetyStatus RobotController::safetyStatus() const
{
    return m_safetyStatus;
}

ProgramState RobotController::programState() const
{
    return m_programState;
}

bool RobotController::isInRemoteControl()
{
    if (!isConnected()) {
        return false;
    }

    QString response = sendCommand("is in remote control");
    return response.trimmed().toLower() == "true";
}

bool RobotController::play()
{
    if (!isConnected()) {
        qWarning() << "Cannot send play - not connected";
        return false;
    }

    if (!isInRemoteControl()) {
        qWarning() << "Cannot send play - robot not in remote control mode";
        return false;
    }

    QString response = sendCommand("play");
    return response.contains("Starting", Qt::CaseInsensitive) ||
           response.contains("success", Qt::CaseInsensitive);
}

bool RobotController::pause()
{
    if (!isConnected()) {
        return false;
    }

    if (!isInRemoteControl()) {
        qWarning() << "Cannot send pause - robot not in remote control mode";
        return false;
    }

    QString response = sendCommand("pause");
    return response.contains("Pausing", Qt::CaseInsensitive) ||
           response.contains("success", Qt::CaseInsensitive);
}

bool RobotController::stop()
{
    if (!isConnected()) {
        return false;
    }

    if (!isInRemoteControl()) {
        qWarning() << "Cannot send stop - robot not in remote control mode";
        return false;
    }

    QString response = sendCommand("stop");
    return response.contains("Stopped", Qt::CaseInsensitive) ||
           response.contains("success", Qt::CaseInsensitive);
}

bool RobotController::powerOn()
{
    if (!isConnected()) {
        return false;
    }

    QString response = sendCommand("power on");
    return response.contains("Powering on", Qt::CaseInsensitive);
}

bool RobotController::powerOff()
{
    if (!isConnected()) {
        return false;
    }

    QString response = sendCommand("power off");
    return response.contains("Powering off", Qt::CaseInsensitive);
}

bool RobotController::brakeRelease()
{
    if (!isConnected()) {
        return false;
    }

    QString response = sendCommand("brake release");
    return response.contains("Brake releasing", Qt::CaseInsensitive);
}

bool RobotController::unlockProtectiveStop()
{
    if (!isConnected()) {
        return false;
    }

    QString response = sendCommand("unlock protective stop");
    return response.contains("Protective stop releasing", Qt::CaseInsensitive);
}

bool RobotController::closeSafetyPopup()
{
    if (!isConnected()) {
        return false;
    }

    QString response = sendCommand("close safety popup");
    return response.contains("closing", Qt::CaseInsensitive);
}

bool RobotController::loadProgram(const QString& programName)
{
    if (!isConnected()) {
        return false;
    }

    QString response = sendCommand("load " + programName);
    return response.contains("Loading", Qt::CaseInsensitive);
}

QString RobotController::getSerialNumber()
{
    if (!isConnected()) {
        return QString();
    }

    QString response = sendCommand("get serial number");
    return response.trimmed();
}

QString RobotController::getRobotModel()
{
    if (!isConnected()) {
        return QString();
    }

    QString response = sendCommand("get robot model");
    return response.trimmed();
}

QString RobotController::getSoftwareVersion()
{
    if (!isConnected()) {
        return QString();
    }

    QString response = sendCommand("PolyscopeVersion");
    return response.trimmed();
}

QString RobotController::sendCommand(const QString& command)
{
    if (!isConnected()) {
        qWarning() << "Cannot send command - not connected";
        return QString();
    }

    qDebug() << "Sending command:" << command;

    // Clear pending response
    m_pendingResponse.clear();

    // Send command with newline
    QString cmdLine = command + "\n";
    m_socket->write(cmdLine.toUtf8());
    m_socket->flush();

    // Wait for response
    QString response = waitForResponse();

    qDebug() << "Response:" << response;
    emit commandResponse(response);

    return response;
}

void RobotController::onConnected()
{
    qDebug() << "RobotController - connected to" << m_robotIp;
    m_connected = true;

    // Read welcome message
    waitForResponse(1000);

    // Start polling status
    m_pollTimer->start(2000);  // Poll every 2 seconds

    emit connected();
}

void RobotController::onDisconnected()
{
    qDebug() << "RobotController - disconnected";
    m_connected = false;
    m_pollTimer->stop();
    emit disconnected();
}

void RobotController::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    qWarning() << "Socket error:" << m_socket->errorString();
    emit connectionError(m_socket->errorString());
}

void RobotController::onReadyRead()
{
    m_pendingResponse += QString::fromUtf8(m_socket->readAll());
}

void RobotController::pollStatus()
{
    if (!isConnected()) {
        return;
    }

    // Poll robot mode
    QString modeResponse = sendCommand("robotmode");
    RobotMode newMode = parseRobotMode(modeResponse);
    if (newMode != m_robotMode) {
        m_robotMode = newMode;
        emit robotModeChanged(m_robotMode);
    }

    // Poll safety status
    QString safetyResponse = sendCommand("safetystatus");
    SafetyStatus newSafety = parseSafetyStatus(safetyResponse);
    if (newSafety != m_safetyStatus) {
        m_safetyStatus = newSafety;
        emit safetyStatusChanged(m_safetyStatus);
    }

    // Poll program state
    QString programResponse = sendCommand("programState");
    ProgramState newState = parseProgramState(programResponse);
    if (newState != m_programState) {
        m_programState = newState;
        emit programStateChanged(m_programState);
    }
}

RobotMode RobotController::parseRobotMode(const QString& response)
{
    QString lower = response.toLower().trimmed();

    if (lower.contains("running")) return RobotMode::Running;
    if (lower.contains("idle")) return RobotMode::Idle;
    if (lower.contains("power_off") || lower.contains("poweroff")) return RobotMode::PowerOff;
    if (lower.contains("no_controller")) return RobotMode::NoController;
    if (lower.contains("disconnected")) return RobotMode::Disconnected;
    if (lower.contains("confirm_safety")) return RobotMode::ConfirmSafety;
    if (lower.contains("booting")) return RobotMode::Booting;
    if (lower.contains("power_on") || lower.contains("poweron")) return RobotMode::PowerOn;
    if (lower.contains("backdrive")) return RobotMode::BackDrive;

    return RobotMode::Unknown;
}

SafetyStatus RobotController::parseSafetyStatus(const QString& response)
{
    QString lower = response.toLower().trimmed();

    if (lower.contains("normal")) return SafetyStatus::Normal;
    if (lower.contains("reduced")) return SafetyStatus::ReducedMode;
    if (lower.contains("protective_stop") || lower.contains("protectivestop")) return SafetyStatus::ProtectiveStop;
    if (lower.contains("recovery")) return SafetyStatus::Recovery;
    if (lower.contains("safeguard_stop") || lower.contains("safeguardstop")) return SafetyStatus::SafeguardStop;
    if (lower.contains("system_emergency") || lower.contains("systemem")) return SafetyStatus::SystemEmergencyStop;
    if (lower.contains("robot_emergency") || lower.contains("robotem")) return SafetyStatus::RobotEmergencyStop;
    if (lower.contains("violation")) return SafetyStatus::Violation;
    if (lower.contains("fault")) return SafetyStatus::Fault;

    return SafetyStatus::Unknown;
}

ProgramState RobotController::parseProgramState(const QString& response)
{
    QString lower = response.toLower().trimmed();

    if (lower.contains("stopped")) return ProgramState::Stopped;
    if (lower.contains("playing")) return ProgramState::Playing;
    if (lower.contains("paused")) return ProgramState::Paused;

    return ProgramState::Unknown;
}

QString RobotController::waitForResponse(int timeoutMs)
{
    m_pendingResponse.clear();

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(m_socket, &QTcpSocket::readyRead, [this, &loop]() {
        m_pendingResponse += QString::fromUtf8(m_socket->readAll());
        // Check if we have a complete response (ends with newline)
        if (m_pendingResponse.contains('\n')) {
            loop.quit();
        }
    });

    timer.start(timeoutMs);
    loop.exec();

    return m_pendingResponse.trimmed();
}

} // namespace robot
} // namespace multipack
