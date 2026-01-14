/**
 * @file LoggingConfig.cpp
 * @brief Implementation of logging configuration
 */

#include "multipack/config/LoggingConfig.h"
#include "multipack/config/ConfigDefaults.h"

#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <iostream>

namespace multipack {
namespace config {

// Static member initialization
std::unique_ptr<QFile> LoggingConfig::s_logFile;
std::unique_ptr<QFile> LoggingConfig::s_serverLogFile;
LogLevel LoggingConfig::s_logLevel = LogLevel::Info;
bool LoggingConfig::s_consoleOutput = true;
bool LoggingConfig::s_initialized = false;

bool LoggingConfig::initialize(const QString& logDir, LogLevel level, bool consoleOutput)
{
    if (s_initialized) {
        qWarning() << "LoggingConfig already initialized";
        return true;
    }

    s_logLevel = level;
    s_consoleOutput = consoleOutput;

    // Create log directory
    QString dir = logDir.isEmpty() ? Defaults::defaultLogDir() : logDir;
    QDir logDirectory(dir);
    if (!logDirectory.exists()) {
        if (!logDirectory.mkpath(".")) {
            std::cerr << "Failed to create log directory: "
                      << dir.toStdString() << std::endl;
            return false;
        }
    }

    // Create log files
    s_logFile = std::make_unique<QFile>(generateLogFileName("multipack_parser"));
    if (!s_logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
        std::cerr << "Failed to open log file" << std::endl;
        return false;
    }

    s_serverLogFile = std::make_unique<QFile>(generateLogFileName("server"));
    if (!s_serverLogFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
        std::cerr << "Failed to open server log file" << std::endl;
        return false;
    }

    // Install message handler
    qInstallMessageHandler(messageHandler);

    s_initialized = true;
    qInfo() << "Logging initialized - level:" << static_cast<int>(level);

    return true;
}

void LoggingConfig::shutdown()
{
    if (!s_initialized) {
        return;
    }

    qInfo() << "Logging shutdown";

    qInstallMessageHandler(nullptr);

    if (s_logFile && s_logFile->isOpen()) {
        s_logFile->close();
    }
    s_logFile.reset();

    if (s_serverLogFile && s_serverLogFile->isOpen()) {
        s_serverLogFile->close();
    }
    s_serverLogFile.reset();

    s_initialized = false;
}

void LoggingConfig::setLogLevel(LogLevel level)
{
    s_logLevel = level;
}

LogLevel LoggingConfig::logLevel()
{
    return s_logLevel;
}

void LoggingConfig::setConsoleOutput(bool enabled)
{
    s_consoleOutput = enabled;
}

bool LoggingConfig::consoleOutputEnabled()
{
    return s_consoleOutput;
}

QString LoggingConfig::logFilePath()
{
    if (s_logFile) {
        return s_logFile->fileName();
    }
    return QString();
}

QString LoggingConfig::serverLogFilePath()
{
    if (s_serverLogFile) {
        return s_serverLogFile->fileName();
    }
    return QString();
}

void LoggingConfig::rotateLogFiles(int maxFiles)
{
    // TODO: Implement log rotation
    Q_UNUSED(maxFiles);
    qDebug() << "LoggingConfig::rotateLogFiles - TODO";
}

void LoggingConfig::flush()
{
    if (s_logFile && s_logFile->isOpen()) {
        s_logFile->flush();
    }
    if (s_serverLogFile && s_serverLogFile->isOpen()) {
        s_serverLogFile->flush();
    }
}

void LoggingConfig::messageHandler(QtMsgType type,
                                   const QMessageLogContext& context,
                                   const QString& msg)
{
    // Check log level
    LogLevel msgLevel;
    QString levelStr;

    switch (type) {
        case QtDebugMsg:
            msgLevel = LogLevel::Debug;
            levelStr = "DEBUG";
            break;
        case QtInfoMsg:
            msgLevel = LogLevel::Info;
            levelStr = "INFO";
            break;
        case QtWarningMsg:
            msgLevel = LogLevel::Warning;
            levelStr = "WARNING";
            break;
        case QtCriticalMsg:
            msgLevel = LogLevel::Error;
            levelStr = "ERROR";
            break;
        case QtFatalMsg:
            msgLevel = LogLevel::Error;
            levelStr = "FATAL";
            break;
    }

    // Skip if below log level
    if (static_cast<int>(msgLevel) < static_cast<int>(s_logLevel)) {
        return;
    }

    // Format message
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString formattedMsg = QString("[%1] [%2] %3")
        .arg(timestamp)
        .arg(levelStr)
        .arg(msg);

    // Add context in debug mode
    if (s_logLevel == LogLevel::Debug && context.file) {
        formattedMsg += QString(" (%1:%2)")
            .arg(context.file)
            .arg(context.line);
    }

    // Write to file
    if (s_logFile && s_logFile->isOpen()) {
        QTextStream stream(s_logFile.get());
        stream << formattedMsg << "\n";
    }

    // Write to console
    if (s_consoleOutput) {
        std::cerr << formattedMsg.toStdString() << std::endl;
    }

    // Handle fatal messages
    if (type == QtFatalMsg) {
        flush();
        abort();
    }
}

QString LoggingConfig::generateLogFileName(const QString& prefix)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("%1/%2_%3.log")
        .arg(Defaults::defaultLogDir())
        .arg(prefix)
        .arg(timestamp);
}

} // namespace config
} // namespace multipack
