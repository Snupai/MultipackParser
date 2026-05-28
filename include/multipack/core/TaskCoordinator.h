#ifndef MULTIPACK_CORE_TASKCOORDINATOR_H
#define MULTIPACK_CORE_TASKCOORDINATOR_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QString>
#include <QThread>
#include <memory>
#include <functional>

namespace multipack {
namespace audio {
    class SafetyMonitor;
}
namespace system {
    class UsbMonitor;
}
namespace robot {
    class RobotStatusMonitor;
}
namespace network {
    class XmlRpcServer;
}

namespace multipack {
namespace core {

/**
 * @brief Task status enumeration
 */
enum class TaskStatus {
    Idle = 0,        ///< Task is not running
    Starting = 1,     ///< Task is starting
    Running = 2,      ///< Task is running
    Stopping = 3,     ///< Task is stopping
    Stopped = 4,      ///< Task has stopped
    Error = 5,         ///< Task failed with error
    Paused = 6          ///< Task is paused
};

/**
 * @brief Task priority levels
 */
enum class TaskPriority {
    Critical = 1,     ///< Critical for system functionality
    High = 2,         ///< High priority tasks
    Medium = 3,       ///< Medium priority tasks
    Low = 4,         ///< Low priority tasks
    Background = 5       ///< Background maintenance tasks
};

/**
 * @brief Background task descriptor
 */
struct TaskDescriptor {
    QString name;                    ///< Task name for logging
    QString description;              ///< Task description
    TaskPriority priority;              ///< Task priority level
    std::function<void()> startFunction;  ///< Function to start the task
    std::function<void()> stopFunction;   ///< Function to stop the task
    std::function<void()> pauseFunction;  ///< Function to pause the task (optional)
    std::function<void()> resumeFunction; ///< Function to resume the task (optional)
    bool autoRestart = false;            ///< Whether task should auto-restart on failure
    int maxRetries = 3;                ///< Maximum number of retries
    int retryDelayMs = 5000;            ///< Delay between retries
};

/**
 * @brief Task execution status
 */
struct TaskStatus {
    TaskStatus status = TaskStatus::Idle;   ///< Current status
    QString lastError;                         ///< Last error message
    QDateTime startTime;                        ///< Task start time
    QDateTime lastActivityTime;                   ///< Last activity time
    int retryCount = 0;                        ///< Number of retries attempted
    qint64 totalRunTimeMs = 0;              ///< Total time task has been running
};

/**
 * @brief Coordinates all background tasks in the application
 * 
 * This class provides centralized management for all background threads and services:
 * - Robot status monitoring
 * - USB file monitoring  
 * - Audio safety monitoring
 * - XML-RPC server management
 * - Database operations
 * - Update management
 * 
 * Features:
 * - Coordinated startup and shutdown sequence
 * - Thread lifecycle management
 * - Resource usage monitoring
 * - Error recovery and restart logic
 * - Performance metrics collection
 * - Configurable task priorities and dependencies
 */
class TaskCoordinator : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit TaskCoordinator(QObject* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~TaskCoordinator() override;

    /**
     * @brief Register a background task
     * @param task Task descriptor
     * @return Task ID for reference
     */
    QString registerTask(const TaskDescriptor& task);
    
    /**
     * @brief Unregister a background task
     * @param taskId Task ID to unregister
     * @return true if task was unregistered
     */
    bool unregisterTask(const QString& taskId);
    
    /**
     * @brief Start all registered tasks
     * @return true if all tasks started successfully
     */
    bool startAllTasks();
    
    /**
     * @brief Stop all running tasks
     * @return true if all tasks stopped successfully
     */
    bool stopAllTasks();
    
    /**
     * @brief Get task status by ID
     * @param taskId Task ID
     * @return Task status information
     */
    TaskStatus getTaskStatus(const QString& taskId) const;
    
    /**
     * @brief Get all task statuses
     * @return Map of task ID to status
     */
    QHash<QString, TaskStatus> getAllTaskStatuses() const;
    
    /**
     * @brief Check if all critical tasks are running
     * @return true if all critical tasks are operational
     */
    bool areCriticalTasksHealthy() const;
    
    /**
     * @brief Get system resource usage summary
     * @return Resource usage information
     */
    QString getResourceUsageSummary() const;
    
    /**
     * @brief Start coordinated shutdown
     * @param timeoutMs Maximum time to wait for graceful shutdown
     * @return true if shutdown completed
     */
    bool coordinatedShutdown(int timeoutMs = 10000);

signals:
    /**
     * @brief Emitted when a task status changes
     * @param taskId Task ID
     * @param status New status
     */
    void taskStatusChanged(const QString& taskId, TaskStatus status);
    
    /**
     * @brief Emitted when a task encounters an error
     * @param taskId Task ID
     * @param error Error message
     */
    void taskError(const QString& taskId, const QString& error);
    
    /**
     * @brief Emitted when resource usage threshold is exceeded
     * @param resourceType Type of resource
     * @param usage Current usage value
     * @param threshold Threshold that was exceeded
     */
    void resourceUsageExceeded(const QString& resourceType, double usage, double threshold);
    
    /**
     * @brief Emitted when system is ready (all critical tasks running)
     */
    void systemReady();

private slots:
    /**
     * @brief Monitor task health and handle failures
     */
    void monitorTaskHealth();
    
    /**
     * @brief Update resource usage statistics
     */
    void updateResourceUsage();
    
    /**
     * @brief Handle task failure
     * @param taskId Task ID that failed
     * @param error Error message
     */
    void handleTaskFailure(const QString& taskId, const QString& error);

private:
    // Task management
    QHash<QString, TaskDescriptor> m_tasks;
    QHash<QString, TaskStatus> m_taskStatuses;
    
    // Background task instances
    std::unique_ptr<audio::SafetyMonitor> m_safetyMonitor;
    std::unique_ptr<system::UsbMonitor> m_usbMonitor;
    std::unique_ptr<robot::RobotStatusMonitor> m_robotMonitor;
    std::unique_ptr<network::XmlRpcServer> m_xmlRpcServer;
    
    // Resource monitoring
    QTimer* m_resourceMonitorTimer;
    double m_cpuUsage = 0.0;
    double m_memoryUsage = 0.0;
    int m_activeThreadCount = 0;
    
    // Health monitoring
    QTimer* m_healthCheckTimer;
    static constexpr int HEALTH_CHECK_INTERVAL_MS = 10000; // 10 seconds
    
    // System state
    bool m_isShuttingDown = false;
    bool m_allCriticalHealthy = false;
    
    // Task management
    QString generateTaskId();
    void logTaskOperation(const QString& taskId, const QString& operation, const QString& details = QString());
    
    /**
     * @brief Check if task is critical to system operation
     * @param priority Task priority
     * @return true if critical
     */
    bool isCriticalTask(TaskPriority priority) const;
};

} // namespace core
} // namespace multipack

#endif // MULTIPACK_CORE_TASKCOORDINATOR_H