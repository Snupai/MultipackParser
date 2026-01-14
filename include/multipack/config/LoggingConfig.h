/**
 * @file LoggingConfig.h
 * @brief Logging configuration and setup
 *
 * Configures Qt logging to file and console.
 */

#ifndef MULTIPACK_CONFIG_LOGGINGCONFIG_H
#define MULTIPACK_CONFIG_LOGGINGCONFIG_H

#include <QString>
#include <QFile>
#include <memory>

namespace multipack {
namespace config {

/**
 * @enum LogLevel
 * @brief Logging verbosity level
 */
enum class LogLevel {
    Debug,      ///< All messages including debug
    Info,       ///< Info and above
    Warning,    ///< Warning and above
    Error,      ///< Errors only
    Silent      ///< No logging
};

/**
 * @class LoggingConfig
 * @brief Configures application logging
 *
 * Sets up Qt message handlers for file and console output.
 */
class LoggingConfig
{
public:
    /**
     * @brief Initialize logging
     * @param logDir Directory for log files
     * @param level Minimum log level
     * @param consoleOutput Whether to also output to console
     * @return true on success
     */
    static bool initialize(const QString& logDir = QString(),
                          LogLevel level = LogLevel::Info,
                          bool consoleOutput = true);

    /**
     * @brief Shut down logging
     */
    static void shutdown();

    /**
     * @brief Set log level
     * @param level New log level
     */
    static void setLogLevel(LogLevel level);

    /**
     * @brief Get current log level
     * @return Current level
     */
    static LogLevel logLevel();

    /**
     * @brief Set console output
     * @param enabled Whether to output to console
     */
    static void setConsoleOutput(bool enabled);

    /**
     * @brief Check if console output is enabled
     * @return true if enabled
     */
    static bool consoleOutputEnabled();

    /**
     * @brief Get current log file path
     * @return Log file path
     */
    static QString logFilePath();

    /**
     * @brief Get server log file path
     * @return Server log file path
     */
    static QString serverLogFilePath();

    /**
     * @brief Rotate log files
     * @param maxFiles Maximum number of files to keep
     */
    static void rotateLogFiles(int maxFiles = 10);

    /**
     * @brief Flush log buffers
     */
    static void flush();

private:
    /**
     * @brief Qt message handler
     * @param type Message type
     * @param context Message context
     * @param msg Message text
     */
    static void messageHandler(QtMsgType type,
                               const QMessageLogContext& context,
                               const QString& msg);

    /**
     * @brief Generate log file name
     * @param prefix File prefix
     * @return Full path
     */
    static QString generateLogFileName(const QString& prefix);

    static std::unique_ptr<QFile> s_logFile;
    static std::unique_ptr<QFile> s_serverLogFile;
    static LogLevel s_logLevel;
    static bool s_consoleOutput;
    static bool s_initialized;
};

} // namespace config
} // namespace multipack

#endif // MULTIPACK_CONFIG_LOGGINGCONFIG_H
