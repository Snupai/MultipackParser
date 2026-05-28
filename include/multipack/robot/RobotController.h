/**
 * @file RobotController.h
 * @brief Robot controller for UR Dashboard Server communication
 */
#ifndef MULTIPACK_ROBOT_ROBOTCONTROLLER_H
#define MULTIPACK_ROBOT_ROBOTCONTROLLER_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include "RobotEnums.h"

namespace multipack {
namespace robot {

/**
 * @class RobotController
 * @brief Manages communication with UR robot Dashboard Server
 *
 * Connects to the robot's Dashboard Server (port 29999) to send
 * commands and monitor robot status.
 */
class RobotController : public QObject {
    Q_OBJECT

public:
    static constexpr int DASHBOARD_PORT = 29999;
    static constexpr int SOCKET_TIMEOUT = 5000;

    explicit RobotController(QObject* parent = nullptr);
    ~RobotController() override;

    /**
     * @brief Connect to robot Dashboard Server
     * @param ip Robot IP address
     * @param port Dashboard port (default 29999)
     * @return true if connection initiated
     */
    bool connect(const QString& ip, int port = DASHBOARD_PORT);

    /**
     * @brief Disconnect from robot
     */
    void disconnect();

    /**
     * @brief Check if connected to robot
     */
    bool isConnected() const;

    /**
     * @brief Get current robot mode
     */
    RobotMode robotMode() const;

    /**
     * @brief Get current safety status
     */
    SafetyStatus safetyStatus() const;

    /**
     * @brief Get current program state
     */
    ProgramState programState() const;

    /**
     * @brief Check if robot is in remote control mode
     */
    bool isInRemoteControl();

    // Dashboard commands

    /**
     * @brief Send play command to start program
     * @return true on success
     */
    bool play();

    /**
     * @brief Send pause command
     * @return true on success
     */
    bool pause();

    /**
     * @brief Send stop command
     * @return true on success
     */
    bool stop();

    /**
     * @brief Power on the robot
     */
    bool powerOn();

    /**
     * @brief Power off the robot
     */
    bool powerOff();

    /**
     * @brief Release brakes
     */
    bool brakeRelease();

    /**
     * @brief Unlock protective stop
     */
    bool unlockProtectiveStop();

    /**
     * @brief Close safety popup
     */
    bool closeSafetyPopup();

    /**
     * @brief Load a program
     * @param programName Name of the program file
     */
    bool loadProgram(const QString& programName);

    /**
     * @brief Get robot serial number
     */
    QString getSerialNumber();

    /**
     * @brief Get robot model
     */
    QString getRobotModel();

    /**
     * @brief Get software version
     */
    QString getSoftwareVersion();

    /**
     * @brief Send a raw command to Dashboard Server
     * @param command Command string
     * @return Response string
     */
    QString sendCommand(const QString& command);

signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void robotModeChanged(RobotMode mode);
    void safetyStatusChanged(SafetyStatus status);
    void programStateChanged(ProgramState state);
    void commandResponse(const QString& response);

private slots:
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReadyRead();
    void pollStatus();

private:
    /**
     * @brief Parse robot mode from string
     */
    RobotMode parseRobotMode(const QString& response);

    /**
     * @brief Parse safety status from string
     */
    SafetyStatus parseSafetyStatus(const QString& response);

    /**
     * @brief Parse program state from string
     */
    ProgramState parseProgramState(const QString& response);

    /**
     * @brief Wait for response with timeout
     */
    QString waitForResponse(int timeoutMs = SOCKET_TIMEOUT);

    QTcpSocket* m_socket;
    QTimer* m_pollTimer;
    QString m_robotIp;
    int m_port;
    bool m_connected;
    RobotMode m_robotMode;
    SafetyStatus m_safetyStatus;
    ProgramState m_programState;
    QString m_pendingResponse;
};

} // namespace robot
} // namespace multipack

#endif // MULTIPACK_ROBOT_ROBOTCONTROLLER_H
