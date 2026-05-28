/**
 * @file DashboardClient.h
 * @brief Client for UR robot Dashboard Server
 *
 * Communicates with the Universal Robots Dashboard Server
 * on port 29999 to query robot status and control execution.
 */
#ifndef MULTIPACK_ROBOT_DASHBOARDCLIENT_H
#define MULTIPACK_ROBOT_DASHBOARDCLIENT_H

#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <memory>

namespace multipack {
namespace robot {

/**
 * @class DashboardClient
 * @brief TCP client for UR Dashboard Server
 *
 * The Dashboard Server provides:
 * - Robot mode and status queries
 * - Program control (load, play, pause, stop)
 * - Power control
 * - Safety status information
 */
class DashboardClient : public QObject
{
    Q_OBJECT

public:
    static constexpr int DEFAULT_PORT = 29999;
    static constexpr int DEFAULT_TIMEOUT = 5000;  // 5 seconds

    explicit DashboardClient(QObject* parent = nullptr);
    ~DashboardClient() override;

    /**
     * @brief Connect to Dashboard Server
     * @param host Robot IP address
     * @param port Port number (default 29999)
     * @return true if connected
     */
    bool connect(const QString& host, int port = DEFAULT_PORT);

    /**
     * @brief Disconnect from server
     */
    void disconnect();

    /**
     * @brief Check if connected
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Send command and wait for response
     * @param command Dashboard command
     * @param timeout Response timeout in ms
     * @return Response string
     */
    QString sendCommand(const QString& command, int timeout = DEFAULT_TIMEOUT);

    // === Status Queries ===

    /**
     * @brief Get robot mode
     * @return Mode string (e.g., "Robotmode: RUNNING")
     */
    QString getRobotMode();

    /**
     * @brief Get safety status
     * @return Safety status string
     */
    QString getSafetyMode();

    /**
     * @brief Get program state
     * @return Program state string
     */
    QString getProgramState();

    /**
     * @brief Get Polyscope version
     * @return Version string
     */
    QString getPolyscopeVersion();

    /**
     * @brief Get robot serial number
     * @return Serial number
     */
    QString getSerialNumber();

    /**
     * @brief Get currently loaded program
     * @return Program name or "No program loaded"
     */
    QString getLoadedProgram();

    /**
     * @brief Check if robot is running
     * @return true if running
     */
    bool isRobotRunning();

    /**
     * @brief Check if program is running
     * @return true if program is running
     */
    bool isProgramRunning();

    // === Control Commands ===

    /**
     * @brief Load a program
     * @param programPath Path to .urp file
     * @return true on success
     */
    bool loadProgram(const QString& programPath);

    /**
     * @brief Play loaded program
     * @return true on success
     */
    bool play();

    /**
     * @brief Pause program execution
     * @return true on success
     */
    bool pause();

    /**
     * @brief Stop program execution
     * @return true on success
     */
    bool stop();

    /**
     * @brief Power on robot
     * @return true on success
     */
    bool powerOn();

    /**
     * @brief Power off robot
     * @return true on success
     */
    bool powerOff();

    /**
     * @brief Release brake
     * @return true on success
     */
    bool brakeRelease();

    /**
     * @brief Close safety popup
     * @return true on success
     */
    bool closeSafetyPopup();

    /**
     * @brief Unlock protective stop
     * @return true on success
     */
    bool unlockProtectiveStop();

    /**
     * @brief Restart safety
     * @return true on success
     */
    bool restartSafety();

signals:
    /**
     * @brief Emitted when connected
     */
    void connected();

    /**
     * @brief Emitted when disconnected
     */
    void disconnected();

    /**
     * @brief Emitted on connection error
     * @param error Error message
     */
    void error(const QString& error);

    /**
     * @brief Emitted when response received
     * @param command Original command
     * @param response Response text
     */
    void responseReceived(const QString& command, const QString& response);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError socketError);
    void onReadyRead();

private:
    /**
     * @brief Wait for response line
     * @param timeout Timeout in ms
     * @return Response string
     */
    QString waitForResponse(int timeout);

    /**
     * @brief Check if response indicates success
     * @param response Response string
     * @return true if successful
     */
    bool isSuccessResponse(const QString& response) const;

    std::unique_ptr<QTcpSocket> m_socket;
    QString m_host;
    int m_port = DEFAULT_PORT;
    QString m_readBuffer;
};

} // namespace robot
} // namespace multipack

#endif // MULTIPACK_ROBOT_DASHBOARDCLIENT_H
