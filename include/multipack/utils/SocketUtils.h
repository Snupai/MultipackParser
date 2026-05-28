/**
 * @file SocketUtils.h
 * @brief Socket and network utilities
 */

#ifndef MULTIPACK_UTILS_SOCKETUTILS_H
#define MULTIPACK_UTILS_SOCKETUTILS_H

#include <QString>
#include <QHostAddress>

class QTcpSocket;

namespace multipack {
namespace utils {

/**
 * @namespace SocketUtils
 * @brief Socket and network utilities
 */
namespace SocketUtils {

/**
 * @brief Check if host is reachable
 * @param host Host address
 * @param port Port number
 * @param timeoutMs Timeout in milliseconds
 * @return true if reachable
 */
bool isHostReachable(const QString& host, int port, int timeoutMs = 1000);

/**
 * @brief Wait for socket connection
 * @param socket Socket to wait on
 * @param timeoutMs Timeout in milliseconds
 * @return true if connected
 */
bool waitForConnected(QTcpSocket* socket, int timeoutMs = 5000);

/**
 * @brief Wait for socket data
 * @param socket Socket to wait on
 * @param timeoutMs Timeout in milliseconds
 * @return true if data available
 */
bool waitForReadyRead(QTcpSocket* socket, int timeoutMs = 5000);

/**
 * @brief Send data and wait for response
 * @param socket Socket to use
 * @param data Data to send
 * @param timeoutMs Timeout in milliseconds
 * @return Response data, empty on error
 */
QByteArray sendAndReceive(QTcpSocket* socket, const QByteArray& data,
                          int timeoutMs = 5000);

/**
 * @brief Read line from socket
 * @param socket Socket to read from
 * @param timeoutMs Timeout in milliseconds
 * @return Line read, empty on error
 */
QString readLine(QTcpSocket* socket, int timeoutMs = 5000);

/**
 * @brief Check if IP address is valid
 * @param ip IP address string
 * @return true if valid
 */
bool isValidIpAddress(const QString& ip);

/**
 * @brief Check if port number is valid
 * @param port Port number
 * @return true if valid (1-65535)
 */
bool isValidPort(int port);

/**
 * @brief Get local IP address
 * @return Local IP address
 */
QString getLocalIpAddress();

/**
 * @brief Parse host:port string
 * @param hostPort Host:port string
 * @param host Output host
 * @param port Output port
 * @return true if parsed successfully
 */
bool parseHostPort(const QString& hostPort, QString& host, int& port);

/**
 * @brief Format socket error message
 * @param socket Socket with error
 * @return Error message
 */
QString formatSocketError(QTcpSocket* socket);

} // namespace SocketUtils

} // namespace utils
} // namespace multipack

#endif // MULTIPACK_UTILS_SOCKETUTILS_H
