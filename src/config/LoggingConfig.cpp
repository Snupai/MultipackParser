/**
 * @file LoggingConfig.cpp
 * @brief Implementation of logging configuration with rotating sinks and
 *        Python-equivalent format/routing.
 */

#include "multipack/config/LoggingConfig.h"
#include "multipack/config/ConfigDefaults.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QMutexLocker>
#include <QTextStream>
#include <iostream>

namespace multipack {
namespace config {

Q_LOGGING_CATEGORY(serverLog, "server")

// Static member initialization
LoggingConfig::ChannelSink LoggingConfig::s_appSink;
LoggingConfig::ChannelSink LoggingConfig::s_serverSink;
QString LoggingConfig::s_logDirectory;
bool LoggingConfig::s_usingFallbackDir = false;
LogLevel LoggingConfig::s_logLevel = LogLevel::Info;
bool LoggingConfig::s_consoleOutput = true;
bool LoggingConfig::s_initialized = false;
QMutex LoggingConfig::s_writeMutex;

namespace {

QString joinPath(const QString& dir, const QString& name)
{
    if (dir.isEmpty()) {
        return name;
    }
    if (dir.endsWith('/') || dir.endsWith('\\')) {
        return dir + name;
    }
    return dir + '/' + name;
}

bool ensureDirectoryWritable(const QString& path)
{
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }
    // Probe writability with a sentinel file.
    const QString probe = joinPath(path, ".multipack_log_probe");
    QFile probeFile(probe);
    if (!probeFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    probeFile.close();
    QFile::remove(probe);
    return true;
}

} // namespace

bool LoggingConfig::initialize(const QString& logDir, LogLevel level, bool consoleOutput)
{
    if (s_initialized) {
        qWarning() << "LoggingConfig already initialized";
        return true;
    }

    s_logLevel = level;
    s_consoleOutput = consoleOutput;
    s_usingFallbackDir = false;

    const QString requested = logDir.isEmpty() ? Defaults::defaultLogDir() : logDir;

    QString chosen = requested;
    if (!ensureDirectoryWritable(chosen)) {
        // Fallback to user home directory (Python parity).
        const QString fallback = QDir::homePath();
        std::cerr << "LoggingConfig: preferred log directory unavailable ("
                  << chosen.toStdString() << "), falling back to "
                  << fallback.toStdString() << std::endl;
        if (!ensureDirectoryWritable(fallback)) {
            std::cerr << "LoggingConfig: fallback log directory also unavailable: "
                      << fallback.toStdString() << std::endl;
            return false;
        }
        chosen = fallback;
        s_usingFallbackDir = true;
    }

    s_logDirectory = chosen;

    if (!openChannelFile(LogChannel::App, chosen)) {
        std::cerr << "LoggingConfig: failed to open app log file in "
                  << chosen.toStdString() << std::endl;
        return false;
    }
    if (!openChannelFile(LogChannel::Server, chosen)) {
        std::cerr << "LoggingConfig: failed to open server log file in "
                  << chosen.toStdString() << std::endl;
        // Close app sink to keep state consistent.
        if (s_appSink.file && s_appSink.file->isOpen()) {
            s_appSink.file->close();
        }
        s_appSink.file.reset();
        return false;
    }

    qInstallMessageHandler(messageHandler);

    s_initialized = true;
    qInfo().noquote() << QString("Logging initialized - dir=%1 level=%2 fallback=%3")
                                 .arg(chosen)
                                 .arg(static_cast<int>(level))
                                 .arg(s_usingFallbackDir ? "true" : "false");

    return true;
}

void LoggingConfig::shutdown()
{
    if (!s_initialized) {
        return;
    }

    qInfo() << "Logging shutdown";

    qInstallMessageHandler(nullptr);

    QMutexLocker lock(&s_writeMutex);

    if (s_appSink.file && s_appSink.file->isOpen()) {
        s_appSink.file->flush();
        s_appSink.file->close();
    }
    s_appSink.file.reset();

    if (s_serverSink.file && s_serverSink.file->isOpen()) {
        s_serverSink.file->flush();
        s_serverSink.file->close();
    }
    s_serverSink.file.reset();

    s_initialized = false;
    s_usingFallbackDir = false;
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
    return s_appSink.file ? s_appSink.file->fileName() : QString();
}

QString LoggingConfig::serverLogFilePath()
{
    return s_serverSink.file ? s_serverSink.file->fileName() : QString();
}

QString LoggingConfig::logDirectory()
{
    return s_logDirectory;
}

bool LoggingConfig::isUsingFallbackDirectory()
{
    return s_usingFallbackDir;
}

void LoggingConfig::flush()
{
    QMutexLocker lock(&s_writeMutex);
    if (s_appSink.file && s_appSink.file->isOpen()) {
        s_appSink.file->flush();
    }
    if (s_serverSink.file && s_serverSink.file->isOpen()) {
        s_serverSink.file->flush();
    }
}

void LoggingConfig::writeRecord(LogChannel channel, LogLevel level, const QString& message)
{
    if (!s_initialized) {
        return;
    }

    // Server channel always logs DEBUG and above (Python parity).
    if (channel == LogChannel::App
        && static_cast<int>(level) < static_cast<int>(s_logLevel)) {
        return;
    }

    const QString line = formatRecord(channel, level, message);

    {
        QMutexLocker lock(&s_writeMutex);
        appendToChannel(channel, line);
    }

    if (s_consoleOutput) {
        std::cerr << line.toStdString() << std::endl;
    }
}

void LoggingConfig::messageHandler(QtMsgType type,
                                   const QMessageLogContext& context,
                                   const QString& msg)
{
    const LogLevel msgLevel = levelFromMsgType(type);

    const QByteArray category = context.category ? QByteArray(context.category) : QByteArray("default");
    const bool isServer = (category == QByteArrayLiteral("server"));
    const LogChannel channel = isServer ? LogChannel::Server : LogChannel::App;

    // App channel honors configured level. Server channel always logs DEBUG+.
    if (channel == LogChannel::App
        && static_cast<int>(msgLevel) < static_cast<int>(s_logLevel)) {
        return;
    }

    const QString formatted = formatRecord(channel, msgLevel, msg);

    {
        QMutexLocker lock(&s_writeMutex);
        appendToChannel(channel, formatted);
    }

    if (s_consoleOutput) {
        std::cerr << formatted.toStdString() << std::endl;
    }

    if (type == QtFatalMsg) {
        flush();
        abort();
    }
}

QString LoggingConfig::formatRecord(LogChannel channel,
                                    LogLevel level,
                                    const QString& message)
{
    // Python format: "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
    // Python uses comma as the millisecond separator in asctime by default.
    const QString timestamp = QDateTime::currentDateTime()
                                  .toString("yyyy-MM-dd HH:mm:ss,zzz");
    return QStringLiteral("%1 - %2 - %3 - %4")
        .arg(timestamp,
             QString::fromLatin1(loggerName(channel)),
             QString::fromLatin1(levelString(level)),
             message);
}

void LoggingConfig::appendToChannel(LogChannel channel, const QString& formattedLine)
{
    ChannelSink& sink = (channel == LogChannel::Server) ? s_serverSink : s_appSink;
    if (!sink.file || !sink.file->isOpen()) {
        return;
    }

    const QByteArray payload = (formattedLine + QLatin1Char('\n')).toUtf8();

    // Pre-write rotation check: rotate if next write would exceed the cap.
    const qint64 currentSize = sink.file->size();
    if (currentSize + payload.size() > MAX_FILE_BYTES) {
        rotateChannel(channel);
    }

    if (!sink.file || !sink.file->isOpen()) {
        return;
    }

    sink.file->write(payload);
    sink.file->flush();
}

void LoggingConfig::rotateChannel(LogChannel channel)
{
    ChannelSink& sink = (channel == LogChannel::Server) ? s_serverSink : s_appSink;
    if (!sink.file) {
        return;
    }

    const QString activePath = sink.file->fileName();

    // Close active file before renames (required on Windows).
    if (sink.file->isOpen()) {
        sink.file->flush();
        sink.file->close();
    }

    // Shift .N -> .(N+1) from highest down to 1, dropping the oldest.
    const QString suffixBase = activePath;
    auto suffixPath = [&suffixBase](int n) {
        return suffixBase + QStringLiteral(".") + QString::number(n);
    };

    // Remove the oldest backup if it exists.
    const QString oldest = suffixPath(BACKUP_COUNT);
    if (QFile::exists(oldest)) {
        QFile::remove(oldest);
    }

    for (int n = BACKUP_COUNT - 1; n >= 1; --n) {
        const QString src = suffixPath(n);
        const QString dst = suffixPath(n + 1);
        if (QFile::exists(src)) {
            if (QFile::exists(dst)) {
                QFile::remove(dst);
            }
            QFile::rename(src, dst);
        }
    }

    // Rename active -> .1
    if (QFile::exists(activePath)) {
        const QString firstBackup = suffixPath(1);
        if (QFile::exists(firstBackup)) {
            QFile::remove(firstBackup);
        }
        QFile::rename(activePath, firstBackup);
    }

    // Reopen a fresh active file.
    sink.file = std::make_unique<QFile>(activePath);
    if (!sink.file->open(QIODevice::WriteOnly | QIODevice::Append)) {
        // Failed to reopen - drop sink so we don't keep trying.
        sink.file.reset();
    }
}

bool LoggingConfig::openChannelFile(LogChannel channel, const QString& directory)
{
    ChannelSink& sink = (channel == LogChannel::Server) ? s_serverSink : s_appSink;
    const QString path = joinPath(directory, activeFileName(channel));
    sink.file = std::make_unique<QFile>(path);
    if (!sink.file->open(QIODevice::WriteOnly | QIODevice::Append)) {
        sink.file.reset();
        return false;
    }
    return true;
}

QString LoggingConfig::activeFileName(LogChannel channel)
{
    switch (channel) {
        case LogChannel::App:
            return QStringLiteral("multipack_parser.log");
        case LogChannel::Server:
            return QStringLiteral("server.log");
    }
    return QStringLiteral("multipack_parser.log");
}

const char* LoggingConfig::loggerName(LogChannel channel)
{
    switch (channel) {
        case LogChannel::App:    return "multipack_parser";
        case LogChannel::Server: return "server";
    }
    return "multipack_parser";
}

const char* LoggingConfig::levelString(LogLevel level)
{
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Silent:  return "SILENT";
    }
    return "INFO";
}

LogLevel LoggingConfig::levelFromMsgType(QtMsgType type)
{
    switch (type) {
        case QtDebugMsg:    return LogLevel::Debug;
        case QtInfoMsg:     return LogLevel::Info;
        case QtWarningMsg:  return LogLevel::Warning;
        case QtCriticalMsg: return LogLevel::Error;
        case QtFatalMsg:    return LogLevel::Error;
    }
    return LogLevel::Info;
}

} // namespace config
} // namespace multipack
