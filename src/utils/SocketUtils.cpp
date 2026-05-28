/**
 * @file SocketUtils.cpp
 * @brief Implementation of socket utilities
 */

#include "multipack/utils/SocketUtils.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QDebug>
#include <QRegularExpression>

namespace multipack {
namespace utils {

namespace SocketUtils {

bool isHostReachable(const QString& host, int port, int timeoutMs)
{
    QTcpSocket socket;
    socket.connectToHost(host, port);

    bool connected = socket.waitForConnected(timeoutMs);

    if (connected) {
        socket.disconnectFromHost();
    }

    return connected;
}

bool waitForConnected(QTcpSocket* socket, int timeoutMs)
{
    if (!socket) {
        return false;
    }

    if (socket->state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    return socket->waitForConnected(timeoutMs);
}

bool waitForReadyRead(QTcpSocket* socket, int timeoutMs)
{
    if (!socket) {
        return false;
    }

    return socket->waitForReadyRead(timeoutMs);
}

QByteArray sendAndReceive(QTcpSocket* socket, const QByteArray& data, int timeoutMs)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Socket not connected";
        return QByteArray();
    }

    // Send data
    qint64 written = socket->write(data);
    if (written != data.size()) {
        qWarning() << "Failed to write all data";
        return QByteArray();
    }

    if (!socket->waitForBytesWritten(timeoutMs)) {
        qWarning() << "Timeout waiting for write";
        return QByteArray();
    }

    // Wait for response
    if (!socket->waitForReadyRead(timeoutMs)) {
        qWarning() << "Timeout waiting for response";
        return QByteArray();
    }

    return socket->readAll();
}

QString readLine(QTcpSocket* socket, int timeoutMs)
{
    if (!socket) {
        return QString();
    }

    // Wait for data if not available
    if (!socket->canReadLine()) {
        if (!socket->waitForReadyRead(timeoutMs)) {
            return QString();
        }
    }

    // Try to read a line
    if (socket->canReadLine()) {
        return QString::fromUtf8(socket->readLine()).trimmed();
    }

    // No complete line available
    return QString();
}

bool isValidIpAddress(const QString& ip)
{
    QHostAddress address;
    return address.setAddress(ip);
}

bool isValidPort(int port)
{
    return port >= 1 && port <= 65535;
}

QString getLocalIpAddress()
{
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();

    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol &&
            address != QHostAddress::LocalHost) {
            return address.toString();
        }
    }

    return "127.0.0.1";
}

bool parseHostPort(const QString& hostPort, QString& host, int& port)
{
    QStringList parts = hostPort.split(':');

    if (parts.size() != 2) {
        return false;
    }

    host = parts[0].trimmed();

    bool ok = false;
    port = parts[1].toInt(&ok);

    if (!ok || !isValidPort(port)) {
        return false;
    }

    return true;
}

QString formatSocketError(QTcpSocket* socket)
{
    if (!socket) {
        return "Invalid socket";
    }

    return QString("Socket error %1: %2")
        .arg(static_cast<int>(socket->error()))
        .arg(socket->errorString());
}

} // namespace SocketUtils

} // namespace utils
} // namespace multipack
