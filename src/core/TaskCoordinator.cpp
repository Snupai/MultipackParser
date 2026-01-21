/**
 * @file TaskCoordinator.cpp
 * @brief Implementation of background task coordination system
 */

#include "multipack/core/TaskCoordinator.h"
#include "multipack/audio/SafetyMonitor.h"
#include "multipack/system/UsbMonitor.h"
#include "multipack/robot/RobotStatusMonitor.h"
#include "multipack/network/XmlRpcServer.h"
#include <QDebug>
#include <QCoreApplication>
#include <QThread>

namespace multipack {
namespace core {

TaskCoordinator::TaskCoordinator(QObject* parent)
    : QObject(parent)
    , m_resourceMonitorTimer(new QTimer(this))
    , m_healthCheckTimer(new QTimer(this))
{
    qDebug() << "TaskCoordinator initialized";
    
    // Setup resource monitoring timer
    m_resourceMonitorTimer->setInterval(5000); // Every 5 seconds
    connect(m_resourceMonitorTimer, &QTimer::timeout, this, &TaskCoordinator::updateResourceUsage);
    
    // Setup health check timer
    m_healthCheckTimer->setInterval(HEALTH_CHECK_INTERVAL_MS);
    connect(m_healthCheckTimer, &QTimer::timeout, this, &TaskCoordinator::monitorTaskHealth);
    
    // Initialize active thread counter
    m_activeThreadCount = QThread::idealThreadCount();
}

TaskCoordinator::~TaskCoordinator()
{
    qDebug() << "TaskCoordinator destroyed";
    
    // Ensure graceful shutdown
    stopAllTasks();
}

QString TaskCoordinator::registerTask(const TaskDescriptor& task)
{
    QString taskId = generateTaskId();
    
    // Store task descriptor
    m_tasks[taskId] = task;
    m_taskStatuses[taskId] = TaskStatus{TaskStatus::Idle};
    
    qDebug() << QString("Registered task: %1 (%2) - Priority: %3")
                .arg(task.name, task.description, static_cast<int>(task.priority));
    
    return taskId;
}

bool TaskCoordinator::unregisterTask(const QString& taskId)
{
    if (!m_tasks.contains(taskId)) {
        qWarning() << "Cannot unregister task - task not found:" << taskId;
        return false;
    }
    
    // Stop the task if it's running
    if (m_taskStatuses[taskId].status == TaskStatus::Running) {
        TaskDescriptor task = m_tasks[taskId];
        if (task.stopFunction) {
            logTaskOperation(taskId, "Stopping task");
            task.stopFunction();
        }
        m_taskStatuses[taskId].status = TaskStatus::Stopped;
        emit taskStatusChanged(taskId, m_taskStatuses[taskId]);
    }
    
    // Remove from collections
    m_tasks.remove(taskId);
    m_taskStatuses.remove(taskId);
    
    qDebug() << "Unregistered task:" << taskId;
    return true;
}

bool TaskCoordinator::startAllTasks()
{
    qDebug() << "Starting all background tasks";
    
    bool allCriticalTasksHealthy = true;
    
    // Start tasks in priority order
    QList<TaskDescriptor> tasksByPriority;
    
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        tasksByPriority.append(it.value());
    }
    
    // Sort by priority (lower number = higher priority)
    std::sort(tasksByPriority.begin(), tasksByPriority.end(),
              [](const TaskDescriptor& a, const TaskDescriptor& b) {
                  return static_cast<int>(a.priority) < static_cast<int>(b.priority);
              });
    
    // Start tasks in order
    for (const TaskDescriptor& task : tasksByPriority) {
        QString taskId = m_tasks.key(task);
        
        if (taskId.isEmpty()) {
            continue;
        }
        
        try {
            logTaskOperation(taskId, "Starting task");
            task.startFunction();
            m_taskStatuses[taskId].status = TaskStatus::Starting;
            emit taskStatusChanged(taskId, m_taskStatuses[taskId]);
            
            // Wait a bit for task to start properly
            QThread::msleep(100);
            
            m_taskStatuses[taskId].status = TaskStatus::Running;
            m_taskStatuses[taskId].startTime = QDateTime::currentDateTime();
            m_taskStatuses[taskId].lastActivityTime = QDateTime::currentDateTime();
            emit taskStatusChanged(taskId, m_taskStatuses[taskId]);
            
            qDebug() << QString("Started task: %1").arg(task.name);
            
        } catch (const std::exception& e) {
            QString error = QString("Task failed to start: %1").arg(e.what());
            handleTaskFailure(taskId, error);
            
            if (!isCriticalTask(task.priority)) {
                allCriticalTasksHealthy = false;
            }
        }
    }
    
    // Start health monitoring
    m_healthCheckTimer->start();
    m_resourceMonitorTimer->start();
    
    // Emit system ready signal if all critical tasks are running
    if (allCriticalTasksHealthy) {
        emit systemReady();
    }
    
    return true;
}

bool TaskCoordinator::stopAllTasks()
{
    qDebug() << "Stopping all background tasks";
    
    m_isShuttingDown = true;
    
    // Stop all running tasks
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        QString taskId = it.key();
        if (m_taskStatuses[taskId].status == TaskStatus::Running) {
            TaskDescriptor task = it.value();
            if (task.stopFunction) {
                logTaskOperation(taskId, "Stopping task");
                task.stopFunction();
            }
            m_taskStatuses[taskId].status = TaskStatus::Stopped;
            m_taskStatuses[taskId].lastActivityTime = QDateTime::currentDateTime();
            emit taskStatusChanged(taskId, m_taskStatuses[taskId]);
        }
    }
    
    // Stop monitoring timers
    m_healthCheckTimer->stop();
    m_resourceMonitorTimer->stop();
    
    m_isShuttingDown = false;
    allCriticalTasksHealthy = false;
    
    return true;
}

TaskStatus TaskCoordinator::getTaskStatus(const QString& taskId) const
{
    return m_taskStatuses.value(taskId, TaskStatus{TaskStatus::Idle});
}

QHash<QString, TaskStatus> TaskCoordinator::getAllTaskStatuses() const
{
    return m_taskStatuses;
}

bool TaskCoordinator::areCriticalTasksHealthy() const
{
    return m_allCriticalHealthy;
}

QString TaskCoordinator::getResourceUsageSummary() const
{
    return QString("Threads: %1, CPU: %2%, Memory: %3%")
            .arg(m_activeThreadCount)
            .arg(m_cpuUsage * 100, 0, 'f', 1)
            .arg(m_memoryUsage * 100, 0, 'f', 1);
}

bool TaskCoordinator::coordinatedShutdown(int timeoutMs)
{
    qDebug() << QString("Starting coordinated shutdown (timeout: %1ms)").arg(timeoutMs);
    
    m_isShuttingDown = true;
    
    // Stop all tasks first
    stopAllTasks();
    
    // Wait for tasks to stop (with timeout)
    bool allStopped = false;
    QElapsedTimer shutdownTimer;
    shutdownTimer.start();
    
    while (shutdownTimer.elapsed() < timeoutMs) {
        allStopped = true;
        for (auto it = m_taskStatuses.begin(); it != m_taskStatuses.end(); ++it) {
            if (it.value().status != TaskStatus::Stopped && 
                it.value().status != TaskStatus::Idle) {
                allStopped = false;
                break;
            }
        }
        
        if (allStopped) {
            break;
        }
        
        QThread::msleep(100); // Check every 100ms
    }
    
    bool success = allStopped || (shutdownTimer.elapsed() >= timeoutMs);
    
    if (success) {
        qDebug() << "Coordinated shutdown completed successfully";
    } else {
        qWarning() << "Coordinated shutdown timed out or failed";
    }
    
    return success;
}

void TaskCoordinator::setSafetyMonitor(std::unique_ptr<audio::SafetyMonitor> safetyMonitor)
{
    m_safetyMonitor = std::move(safetyMonitor);
}

void TaskCoordinator::setUsbMonitor(std::unique_ptr<system::UsbMonitor> usbMonitor)
{
    m_usbMonitor = std::move(usbMonitor);
}

void TaskCoordinator::setRobotStatusMonitor(std::unique_ptr<robot::RobotStatusMonitor> robotMonitor)
{
    m_robotMonitor = std::move(robotMonitor);
}

void TaskCoordinator::setXmlRpcServer(std::unique_ptr<network::XmlRpcServer> xmlRpcServer)
{
    m_xmlRpcServer = std::move(xmlRpcServer);
}

void TaskCoordinator::monitorTaskHealth()
{
    if (m_isShuttingDown) {
        return;
    }
    
    // Update resource usage
    updateResourceUsage();
    
    // Check each task for health issues
    for (auto it = m_taskStatuses.begin(); it != m_taskStatuses.end(); ++it) {
        QString taskId = it.key();
        TaskStatus status = it.value();
        
        // Check for errors
        if (status.status == TaskStatus::Error) {
            emit taskError(taskId, status.lastError);
        }
        
        // Check for stuck tasks (running too long without activity)
        if (status.status == TaskStatus::Running) {
            qint64 runningTimeMs = status.startTime.msecsTo(QDateTime::currentDateTime());
            
            // Consider stuck if running for more than 5 minutes without activity
            if (runningTimeMs > 300000 && status.lastActivityTime.secsTo(QDateTime::currentDateTime()) > 60000) {
                qWarning() << QString("Task %1 appears to be stuck (running for %2ms)")
                            .arg(task.name)
                            .arg(runningTimeMs);
                
                // Attempt to restart stuck task
                TaskDescriptor task = m_tasks.value(taskId);
                if (task.startFunction && status.retryCount < task.maxRetries) {
                    status.retryCount++;
                    status.lastError = QString("Task restarted due to inactivity (attempt %1/%2)")
                                         .arg(status.retryCount)
                                         .arg(task.maxRetries);
                    
                    logTaskOperation(taskId, "Restarting stuck task");
                    task.startFunction();
                    
                    // Wait before retry
                    QTimer::singleShot(task.retryDelayMs, this, [this, taskId]() {
                        TaskDescriptor retryTask = m_tasks.value(taskId);
                        if (retryTask.startFunction) {
                            retryTask.startFunction();
                        }
                    });
                }
            }
        }
    }
}

void TaskCoordinator::updateResourceUsage()
{
    // Update resource usage statistics
    m_activeThreadCount = QThread::idealThreadCount();
    
    // CPU and memory usage (simplified estimates)
    // In a real implementation, you'd use system APIs to get actual values
    m_cpuUsage = qrand() % 100 / 100.0; // Simulated CPU usage
    m_memoryUsage = qrand() % 100 / 100.0; // Simulated memory usage
    
    // Check resource thresholds
    static constexpr double CPU_THRESHOLD = 80.0; // 80% CPU
    static constexpr double MEMORY_THRESHOLD = 85.0; // 85% memory
    
    if (m_cpuUsage > CPU_THRESHOLD) {
        emit resourceUsageExceeded("CPU", m_cpuUsage, CPU_THRESHOLD);
    }
    
    if (m_memoryUsage > MEMORY_THRESHOLD) {
        emit resourceUsageExceeded("Memory", m_memoryUsage, MEMORY_THRESHOLD);
    }
}

void TaskCoordinator::handleTaskFailure(const QString& taskId, const QString& error)
{
    TaskStatus& status = m_taskStatuses[taskId];
    status.status = TaskStatus::Error;
    status.lastError = error;
    status.lastActivityTime = QDateTime::currentDateTime();
    
    emit taskError(taskId, error);
    
    // Auto-restart if configured
    TaskDescriptor task = m_tasks.value(taskId);
    if (task.autoRestart && task.startFunction && status.retryCount < task.maxRetries) {
        status.retryCount++;
        status.lastError = QString("Auto-restarting task (attempt %1/%2): %3")
                             .arg(status.retryCount)
                             .arg(task.maxRetries)
                             .arg(error);
        
        logTaskOperation(taskId, "Auto-restarting task");
        
        QTimer::singleShot(task.retryDelayMs, this, [this, taskId]() {
            TaskDescriptor restartTask = m_tasks.value(taskId);
            if (restartTask.startFunction) {
                restartTask.startFunction();
            }
        });
    }
}

QString TaskCoordinator::generateTaskId()
{
    static int taskCounter = 0;
    return QString("TASK_%1").arg(++taskCounter, 8, 10, QChar('0'));
}

void TaskCoordinator::logTaskOperation(const QString& taskId, const QString& operation, const QString& details)
{
    QString logMessage = QString("[%1] %2: %3")
                          .arg(taskId, QDateTime::currentDateTime().toString("hh:mm:ss.zzz"), operation);
    
    if (!details.isEmpty()) {
        logMessage += QString(" - %1").arg(details);
    }
    
    qDebug() << logMessage;
}

bool TaskCoordinator::isCriticalTask(TaskPriority priority) const
{
    return priority <= static_cast<int>(TaskPriority::High);
}

} // namespace core
} // namespace multipack