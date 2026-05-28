/**
 * @file LoggingConfig.h
 * @brief Logging configuration and setup with Python-parity rotating file sinks.
 *
 * Provides two channels (App and Server) that mirror the original Python
 * loggers (`multipack_parser` and `server`). Each channel writes to a
 * rotating file (5 MB per file, 5 backups) and optionally to the console.
 *
 * Server-channel messages must be emitted through the
 * `multipack::config::serverLog` Qt logging category to be routed to the
 * server sink. This replaces the previous heuristic routing by substring.
 */

#ifndef MULTIPACK_CONFIG_LOGGINGCONFIG_H
#define MULTIPACK_CONFIG_LOGGINGCONFIG_H

#include <QString>
#include <QFile>
#include <QLoggingCategory>
#include <QMutex>
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
 * @enum LogChannel
 * @brief Logical logger channel (parity with Python named loggers).
 */
enum class LogChannel {
    App,    ///< multipack_parser
    Server  ///< server
};

/**
 * @brief Server logging category. Emit with qCDebug/qCInfo/qCWarning(serverLog).
 *
 * Messages emitted under this category are routed to the dedicated server
 * sink and are not mirrored to the application sink.
 */
Q_DECLARE_LOGGING_CATEGORY(serverLog)

/**
 * @class LoggingConfig
 * @brief Configures application logging with rotating file sinks.
 */
class LoggingConfig
{
public:
    /**
     * @brief Initialize logging.
     * @param logDir Directory for log files. Falls back to home directory if not writable.
     * @param level Minimum app channel log level (server channel always logs DEBUG and above).
     * @param consoleOutput Mirror output to stderr.
     * @return true on success.
     */
    static bool initialize(const QString& logDir = QString(),
                          LogLevel level = LogLevel::Info,
                          bool consoleOutput = true);

    /**
     * @brief Shut down logging and close sinks.
     */
    static void shutdown();

    /**
     * @brief Set the app channel log level.
     */
    static void setLogLevel(LogLevel level);

    /**
     * @brief Current app channel log level.
     */
    static LogLevel logLevel();

    /**
     * @brief Toggle console mirroring.
     */
    static void setConsoleOutput(bool enabled);

    /**
     * @brief Whether console output is enabled.
     */
    static bool consoleOutputEnabled();

    /**
     * @brief Active app channel log file path.
     */
    static QString logFilePath();

    /**
     * @brief Active server channel log file path.
     */
    static QString serverLogFilePath();

    /**
     * @brief Directory currently used for log files (may be the home fallback).
     */
    static QString logDirectory();

    /**
     * @brief Whether logging is currently using the fallback home directory.
     */
    static bool isUsingFallbackDirectory();

    /**
     * @brief Flush both sinks to disk.
     */
    static void flush();

    /**
     * @brief Maximum size of a single log file before rotation (bytes).
     */
    static constexpr qint64 MAX_FILE_BYTES = 5 * 1024 * 1024;

    /**
     * @brief Number of historical files kept per channel (besides the active one).
     */
    static constexpr int BACKUP_COUNT = 5;

    /**
     * @brief Test/diagnostics helper: write a single record to a channel without
     *        going through Qt's message handler. Useful for unit tests.
     */
    static void writeRecord(LogChannel channel, LogLevel level, const QString& message);

private:
    static void messageHandler(QtMsgType type,
                               const QMessageLogContext& context,
                               const QString& msg);

    /**
     * @brief Format a single record in Python-compatible shape.
     *        `yyyy-MM-dd HH:mm:ss,zzz - <loggerName> - <LEVEL> - <message>`
     */
    static QString formatRecord(LogChannel channel,
                                LogLevel level,
                                const QString& message);

    /**
     * @brief Append a formatted line to a channel sink, rotating if needed.
     */
    static void appendToChannel(LogChannel channel, const QString& formattedLine);

    /**
     * @brief Rotate the active sink for `channel` (closes, shifts, reopens).
     */
    static void rotateChannel(LogChannel channel);

    /**
     * @brief Try opening the active file for a channel inside `directory`.
     */
    static bool openChannelFile(LogChannel channel, const QString& directory);

    /**
     * @brief Filename for the active file of a channel (without rotation suffix).
     */
    static QString activeFileName(LogChannel channel);

    /**
     * @brief Logical logger name for the format `%(name)s` field.
     */
    static const char* loggerName(LogChannel channel);

    /**
     * @brief Map a level to its display string.
     */
    static const char* levelString(LogLevel level);

    /**
     * @brief Map a QtMsgType to LogLevel.
     */
    static LogLevel levelFromMsgType(QtMsgType type);

    /**
     * @brief Channel sink record.
     */
    struct ChannelSink {
        std::unique_ptr<QFile> file;
    };

    static ChannelSink s_appSink;
    static ChannelSink s_serverSink;
    static QString s_logDirectory;
    static bool s_usingFallbackDir;
    static LogLevel s_logLevel;
    static bool s_consoleOutput;
    static bool s_initialized;
    static QMutex s_writeMutex;
};

} // namespace config
} // namespace multipack

#endif // MULTIPACK_CONFIG_LOGGINGCONFIG_H
