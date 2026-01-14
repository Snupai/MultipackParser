/**
 * @file DashboardClient.cpp
 * @brief Implementation of UR Dashboard Server client
 */

#include "multipack/robot/DashboardClient.h"

#include <QDebug>
#include <QEventLoop>
#include <QTimer>

namespace multipack {
namespace robot {

DashboardClient::DashboardClient(QObject* parent)
    : QObject(parent)
    , m_socket(std::make_unique<QTcpSocket>(this))
{
    qDebug() << "DashboardClient::DashboardClient - constructor";

    QObject::connect(m_socket.get(), &QTcpSocket::connected,
                     this, &DashboardClient::onConnected);
    QObject::connect(m_socket.get(), &QTcpSocket::disconnected,
                     this, &DashboardClient::onDisconnected);
    QObject::connect(m_socket.get(), &QTcpSocket::errorOccurred,
                     this, &DashboardClient::onError);
    QObject::connect(m_socket.get(), &QTcpSocket::readyRead,
                     this, &DashboardClient::onReadyRead);
}

DashboardClient::~DashboardClient()
{
    qDebug() << "DashboardClient::~DashboardClient - destructor";
    disconnect();
}

bool DashboardClient::connect(const QString& host, int port)
{
    if (isConnected()) {
        if (m_host == host && m_port == port) {
            return true;  // Already connected to same host
        }
        disconnect();
    }

    qDebug() << "DashboardClient::connect -" << host << ":" << port;

    m_host = host;
    m_port = port;
    m_readBuffer.clear();

    m_socket->connectToHost(host, port);

    // Wait for connection with timeout
    if (!m_socket->waitForConnected(DEFAULT_TIMEOUT)) {
        qWarning() << "DashboardClient: Connection timeout";
        emit error("Connection timeout");
        return false;
    }

    // Dashboard server sends a welcome message, read it
    waitForResponse(2000);

    return true;
}

void DashboardClient::disconnect()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "DashboardClient::disconnect";
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }
}

bool DashboardClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

QString DashboardClient::sendCommand(const QString& command, int timeout)
{
    if (!isConnected()) {
        qWarning() << "DashboardClient: Not connected";
        return QString();
    }

    qDebug() << "DashboardClient::sendCommand -" << command;

    // Clear any pending data
    m_readBuffer.clear();
    m_socket->readAll();

    // Send command with newline
    QString cmd = command.trimmed() + "\n";
    m_socket->write(cmd.toUtf8());
    m_socket->flush();

    // Wait for response
    QString response = waitForResponse(timeout);

    qDebug() << "DashboardClient response:" << response;
    emit responseReceived(command, response);

    return response;
}

QString DashboardClient::waitForResponse(int timeout)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(m_socket.get(), &QTcpSocket::readyRead, &loop, &QEventLoop::quit);
    QObject::connect(m_socket.get(), &QTcpSocket::disconnected, &loop, &QEventLoop::quit);

    timer.start(timeout);

    while (timer.isActive() && isConnected()) {
        loop.exec();

        // Read available data
        if (m_socket->bytesAvailable() > 0) {
            m_readBuffer += QString::fromUtf8(m_socket->readAll());
        }

        // Check for complete line (response ends with newline)
        if (m_readBuffer.contains('\n')) {
            break;
        }
    }

    // Extract first complete line
    int newlinePos = m_readBuffer.indexOf('\n');
    if (newlinePos >= 0) {
        QString response = m_readBuffer.left(newlinePos).trimmed();
        m_readBuffer = m_readBuffer.mid(newlinePos + 1);
        return response;
    }

    return m_readBuffer.trimmed();
}

bool DashboardClient::isSuccessResponse(const QString& response) const
{
    // Dashboard responses typically start with the result
    // Successful commands often echo the action or return specific values
    // Failed commands return "Failed to..." or error messages
    return !response.isEmpty() &&
           !response.startsWith("Failed") &&
           !response.startsWith("Error") &&
           !response.contains("not in remote control");
}

// === Status Queries ===

QString DashboardClient::getRobotMode()
{
    return sendCommand("robotmode");
}

QString DashboardClient::getSafetyMode()
{
    return sendCommand("safetymode");
}

QString DashboardClient::getProgramState()
{
    return sendCommand("programState");
}

QString DashboardClient::getPolyscopeVersion()
{
    return sendCommand("PolyscopeVersion");
}

QString DashboardClient::getSerialNumber()
{
    return sendCommand("get serial number");
}

QString DashboardClient::getLoadedProgram()
{
    return sendCommand("get loaded program");
}

bool DashboardClient::isRobotRunning()
{
    QString mode = getRobotMode();
    return mode.contains("RUNNING", Qt::CaseInsensitive);
}

bool DashboardClient::isProgramRunning()
{
    QString state = getProgramState();
    return state.contains("PLAYING", Qt::CaseInsensitive);
}

// === Control Commands ===

bool DashboardClient::loadProgram(const QString& programPath)
{
    QString response = sendCommand(QString("load %1").arg(programPath));
    return isSuccessResponse(response) && response.contains("Loading program");
}

bool DashboardClient::play()
{
    QString response = sendCommand("play");
    return isSuccessResponse(response) && response.contains("Starting program");
}

bool DashboardClient::pause()
{
    QString response = sendCommand("pause");
    return isSuccessResponse(response) && response.contains("Pausing program");
}

bool DashboardClient::stop()
{
    QString response = sendCommand("stop");
    return isSuccessResponse(response) && response.contains("Stopped");
}

bool DashboardClient::powerOn()
{
    QString response = sendCommand("power on");
    return isSuccessResponse(response);
}

bool DashboardClient::powerOff()
{
    QString response = sendCommand("power off");
    return isSuccessResponse(response);
}

bool DashboardClient::brakeRelease()
{
    QString response = sendCommand("brake release");
    return isSuccessResponse(response);
}

bool DashboardClient::closeSafetyPopup()
{
    QString response = sendCommand("close safety popup");
    return isSuccessResponse(response);
}

bool DashboardClient::unlockProtectiveStop()
{
    QString response = sendCommand("unlock protective stop");
    return isSuccessResponse(response);
}

bool DashboardClient::restartSafety()
{
    QString response = sendCommand("restart safety");
    return isSuccessResponse(response);
}

// === Private Slots ===

void DashboardClient::onConnected()
{
    qDebug() << "DashboardClient: Connected to" << m_host << ":" << m_port;
    emit connected();
}

void DashboardClient::onDisconnected()
{
    qDebug() << "DashboardClient: Disconnected";
    emit disconnected();
}

void DashboardClient::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    QString errorMsg = m_socket->errorString();
    qWarning() << "DashboardClient error:" << errorMsg;
    emit error(errorMsg);
}

void DashboardClient::onReadyRead()
{
    // Data will be read in waitForResponse
}

} // namespace robot
} // namespace multipack
