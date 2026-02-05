/**
 * @file RobotStatusMonitor.h
 * @brief Continuous robot status monitoring
 *
 * Polls the robot Dashboard Server at regular intervals
 * to monitor mode, safety status, and program state.
 */

#ifndef MULTIPACK_ROBOT_ROBOTSTATUSMONITOR_H
#define MULTIPACK_ROBOT_ROBOTSTATUSMONITOR_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <memory>
#include <QMutex>
#include <atomic>

#include "RobotEnums.h"

namespace multipack {
namespace robot {

class DashboardClient;

/**
 * @struct RobotStatus
 * @brief Current robot status data
 */
struct RobotStatus {
    RobotMode robotMode = RobotMode::Unknown;
    SafetyStatus safetyStatus = SafetyStatus::Unknown;
    ProgramState programState = ProgramState::Unknown;
    QDateTime lastUpdate;
    bool isConnected = false;
    QString connectionError;
    QString loadedProgram;
    QString polyscopeVersion;
    QString serialNumber;
};

/**
 * @class RobotStatusMonitor
 * @brief Monitors robot status via Dashboard Server polling
 *
 * Continuously polls the robot at configurable intervals
 * and emits signals when status changes.
 */
class RobotStatusMonitor : public QObject
{
    Q_OBJECT

public:
    static constexpr int DEFAULT_INTERVAL_MS = 2000;  // 2 seconds like Python

    /**
     * @brief Construct status monitor
     * @param parent Parent QObject
     */
    explicit RobotStatusMonitor(QObject* parent = nullptr);

    /**
     * @brief Destroy status monitor
     */
    ~RobotStatusMonitor() override;

    /**
     * @brief Set robot IP address
     * @param ip IP address
     */
    void setRobotIp(const QString& ip);

    /**
     * @brief Get robot IP address
     * @return IP address
     */
    QString robotIp() const;

    /**
     * @brief Start monitoring
     * @param intervalMs Polling interval in milliseconds
     */
    void start(int intervalMs = DEFAULT_INTERVAL_MS);

    /**
     * @brief Stop monitoring
     */
    void stop();

    /**
     * @brief Check if monitoring is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Get current status
     * @return Current robot status
     */
    RobotStatus currentStatus() const;

    /**
     * @brief Force immediate status update
     */
    void updateNow();

    /**
     * @brief Get Polyscope version
     * @return Version string, empty on error
     */
    QString getPolyscopeVersion();

    /**
     * @brief Get loaded program name
     * @return Program name, empty on error
     */
    QString getLoadedProgram();

    /**
     * @brief Get robot serial number
     * @return Serial number, empty on error
     */
    QString getSerialNumber();

signals:
    /**
     * @brief Emitted when robot mode changes
     * @param mode New robot mode
     */
    void robotModeChanged(RobotMode mode);

    /**
     * @brief Emitted when safety status changes
     * @param status New safety status
     */
    void safetyStatusChanged(SafetyStatus status);

    /**
     * @brief Emitted when program state changes
     * @param state New program state
     */
    void programStateChanged(ProgramState state);

    /**
     * @brief Emitted when connection status changes
     * @param connected true if connected
     */
    void connectionChanged(bool connected);

    /**
     * @brief Emitted on status update
     * @param status Current status
     */
    void statusUpdated(const RobotStatus& status);

    /**
     * @brief Emitted when detail fields update
     * @param polyscopeVersion Polyscope version string
     * @param serialNumber Robot serial number
     * @param loadedProgram Loaded program name
     */
    void detailsUpdated(const QString& polyscopeVersion,
                        const QString& serialNumber,
                        const QString& loadedProgram);

    /**
     * @brief Emitted on connection error
     * @param error Error message
     */
    void error(const QString& error);

private slots:
    void onPollTimeout();

private:
    /**
     * @brief Parse robot mode from dashboard response
     * @param response Raw response string
     * @return Parsed robot mode
     */
    RobotMode parseRobotMode(const QString& response);

    /**
     * @brief Parse safety status from dashboard response
     * @param response Raw response string
     * @return Parsed safety status
     */
    SafetyStatus parseSafetyStatus(const QString& response);

    /**
     * @brief Parse program state from dashboard response
     * @param response Raw response string
     * @return Parsed program state
     */
    ProgramState parseProgramState(const QString& response);

    std::unique_ptr<DashboardClient> m_client;
    QTimer* m_pollTimer = nullptr;
    QString m_robotIp;
    RobotStatus m_status;
    bool m_wasConnected = false;
    int m_detailCounter = 0;
    QString m_cachedPolyscopeVersion;
    QString m_cachedSerialNumber;
    QString m_cachedLoadedProgram;
    mutable QMutex m_detailMutex;
    std::atomic<bool> m_shouldStop{false};
};

} // namespace robot
} // namespace multipack

#endif // MULTIPACK_ROBOT_ROBOTSTATUSMONITOR_H
