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
    QString logDir = Defaults::defaultLogDir();
    QDir dir(logDir);

    if (!dir.exists()) {
        return;
    }

    // Get all log files sorted by modification time (oldest first)
    QStringList filters;
    filters << "*.log";
    QFileInfoList logFiles = dir.entryInfoList(filters, QDir::Files, QDir::Time | QDir::Reversed);

    qDebug() << "LoggingConfig::rotateLogFiles - found" << logFiles.size() << "log files, keeping" << maxFiles;

    // Group files by prefix (multipack_parser, server)
    QMap<QString, QFileInfoList> filesByPrefix;
    for (const QFileInfo& fi : logFiles) {
        QString name = fi.baseName();
        // Extract prefix (everything before the timestamp)
        int underscoreIdx = name.lastIndexOf('_');
        if (underscoreIdx > 0) {
            // Try to find the second-to-last underscore for timestamp pattern
            int prevUnderscore = name.lastIndexOf('_', underscoreIdx - 1);
            if (prevUnderscore > 0) {
                QString prefix = name.left(prevUnderscore);
                filesByPrefix[prefix].append(fi);
            } else {
                // Simple prefix_timestamp pattern
                filesByPrefix[name.left(underscoreIdx)].append(fi);
            }
        }
    }

    // Delete old files for each prefix
    for (auto it = filesByPrefix.begin(); it != filesByPrefix.end(); ++it) {
        const QString& prefix = it.key();
        QFileInfoList& files = it.value();

        // Keep only maxFiles per prefix
        while (files.size() > maxFiles) {
            QFileInfo oldest = files.takeFirst();

            // Don't delete currently open log files
            if (s_logFile && oldest.absoluteFilePath() == s_logFile->fileName()) {
                continue;
            }
            if (s_serverLogFile && oldest.absoluteFilePath() == s_serverLogFile->fileName()) {
                continue;
            }

            if (QFile::remove(oldest.absoluteFilePath())) {
                qDebug() << "Deleted old log file:" << oldest.fileName();
            } else {
                qWarning() << "Failed to delete old log file:" << oldest.fileName();
            }
        }
    }
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
