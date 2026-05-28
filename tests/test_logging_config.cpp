/**
 * @file test_logging_config.cpp
 * @brief Parity tests for LoggingConfig (format, routing, rotation, fallback).
 */

#include "multipack/config/LoggingConfig.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using multipack::config::LogChannel;
using multipack::config::LogLevel;
using multipack::config::LoggingConfig;
using multipack::config::serverLog;

namespace {

QString readAllText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

class LoggingConfigTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void formatMatchesPythonContract();
    void appLevelFiltering();
    void serverChannelAlwaysDebug();
    void serverCategoryRoutesToServerSink_andNotAppSink();
    void rotationRollsAtSizeBoundary();
    void rotationKeepsAtMostFiveBackups();
    void fallbackUsedWhenPrimaryDirUnwritable();
    void doubleInitializeIsNoOp();

private:
    QTemporaryDir m_tempDir;
};

void LoggingConfigTest::init()
{
    QVERIFY(m_tempDir.isValid());
    // Ensure no global state leaks between tests.
    LoggingConfig::shutdown();
}

void LoggingConfigTest::cleanup()
{
    LoggingConfig::shutdown();
}

void LoggingConfigTest::formatMatchesPythonContract()
{
    const QString dir = m_tempDir.filePath("logs_format");
    QVERIFY(LoggingConfig::initialize(dir, LogLevel::Debug, false));

    LoggingConfig::writeRecord(LogChannel::App, LogLevel::Info, "hello world");
    LoggingConfig::flush();

    const QString contents = readAllText(LoggingConfig::logFilePath());
    QVERIFY(!contents.isEmpty());

    // Expected shape (last line):
    // YYYY-MM-DD HH:MM:SS,mmm - multipack_parser - INFO - hello world
    const QRegularExpression re(
        R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3} - multipack_parser - INFO - hello world$)",
        QRegularExpression::MultilineOption);
    QVERIFY2(re.match(contents).hasMatch(),
             qPrintable(QString("format mismatch, contents:\n%1").arg(contents)));
}

void LoggingConfigTest::appLevelFiltering()
{
    const QString dir = m_tempDir.filePath("logs_filter");
    QVERIFY(LoggingConfig::initialize(dir, LogLevel::Warning, false));

    LoggingConfig::writeRecord(LogChannel::App, LogLevel::Debug, "debug-line");
    LoggingConfig::writeRecord(LogChannel::App, LogLevel::Info, "info-line");
    LoggingConfig::writeRecord(LogChannel::App, LogLevel::Warning, "warn-line");
    LoggingConfig::writeRecord(LogChannel::App, LogLevel::Error, "error-line");
    LoggingConfig::flush();

    const QString contents = readAllText(LoggingConfig::logFilePath());
    QVERIFY(!contents.contains("debug-line"));
    QVERIFY(!contents.contains("info-line"));
    QVERIFY(contents.contains("warn-line"));
    QVERIFY(contents.contains("error-line"));
}

void LoggingConfigTest::serverChannelAlwaysDebug()
{
    const QString dir = m_tempDir.filePath("logs_server_level");
    // App level set to Error - but server channel must still log Debug+.
    QVERIFY(LoggingConfig::initialize(dir, LogLevel::Error, false));

    LoggingConfig::writeRecord(LogChannel::Server, LogLevel::Debug, "srv-debug-line");
    LoggingConfig::writeRecord(LogChannel::Server, LogLevel::Info, "srv-info-line");
    LoggingConfig::flush();

    const QString serverContents = readAllText(LoggingConfig::serverLogFilePath());
    QVERIFY(serverContents.contains("srv-debug-line"));
    QVERIFY(serverContents.contains("srv-info-line"));
    QVERIFY(serverContents.contains(" - server - "));
}

void LoggingConfigTest::serverCategoryRoutesToServerSink_andNotAppSink()
{
    const QString dir = m_tempDir.filePath("logs_routing");
    QVERIFY(LoggingConfig::initialize(dir, LogLevel::Debug, false));

    qCInfo(serverLog) << "server-category-marker";
    qInfo() << "app-default-marker";
    LoggingConfig::flush();

    const QString appContents = readAllText(LoggingConfig::logFilePath());
    const QString serverContents = readAllText(LoggingConfig::serverLogFilePath());

    QVERIFY(serverContents.contains("server-category-marker"));
    QVERIFY(!appContents.contains("server-category-marker"));

    QVERIFY(appContents.contains("app-default-marker"));
    QVERIFY(!serverContents.contains("app-default-marker"));
}

void LoggingConfigTest::rotationRollsAtSizeBoundary()
{
    const QString dir = m_tempDir.filePath("logs_rotate");
    QVERIFY(LoggingConfig::initialize(dir, LogLevel::Debug, false));

    const QString activePath = LoggingConfig::logFilePath();
    QVERIFY(!activePath.isEmpty());

    // Build a payload that, when repeated, exceeds 5 MB to force at least one rotation.
    const QString chunk = QString("x").repeated(8 * 1024); // 8 KiB visible chars
    const int iterations = (LoggingConfig::MAX_FILE_BYTES / chunk.size()) + 32;

    for (int i = 0; i < iterations; ++i) {
        LoggingConfig::writeRecord(LogChannel::App, LogLevel::Info, chunk);
    }
    LoggingConfig::flush();

    const QFileInfo activeInfo(activePath);
    QVERIFY2(QFile::exists(activePath + ".1"),
             "Expected rotated backup .1 to exist after >5 MB of writes");
    QVERIFY2(activeInfo.size() <= LoggingConfig::MAX_FILE_BYTES + (16 * 1024),
             qPrintable(QString("Active file too large after rotation: %1").arg(activeInfo.size())));
}

void LoggingConfigTest::rotationKeepsAtMostFiveBackups()
{
    const QString dir = m_tempDir.filePath("logs_rotate_cap");
    QVERIFY(LoggingConfig::initialize(dir, LogLevel::Debug, false));

    const QString activePath = LoggingConfig::logFilePath();
    const QString chunk = QString("y").repeated(8 * 1024);

    // Drive enough writes for many rotations.
    const int rotations = LoggingConfig::BACKUP_COUNT + 3;
    const int perRotation = (LoggingConfig::MAX_FILE_BYTES / chunk.size()) + 4;
    for (int r = 0; r < rotations; ++r) {
        for (int i = 0; i < perRotation; ++i) {
            LoggingConfig::writeRecord(LogChannel::App, LogLevel::Info, chunk);
        }
    }
    LoggingConfig::flush();

    // The retention contract: only .1 .. .BACKUP_COUNT may exist.
    QVERIFY(!QFile::exists(activePath + "." + QString::number(LoggingConfig::BACKUP_COUNT + 1)));
    QVERIFY(QFile::exists(activePath + ".1"));
    QVERIFY(QFile::exists(activePath + "." + QString::number(LoggingConfig::BACKUP_COUNT)));
}

void LoggingConfigTest::fallbackUsedWhenPrimaryDirUnwritable()
{
    // Create a regular file and try to use its path as a log "directory".
    // mkpath() must refuse because a non-directory entry already occupies the name,
    // which deterministically triggers the home-directory fallback.
    const QString blockingFilePath = m_tempDir.filePath("not_a_directory");
    {
        QFile blocker(blockingFilePath);
        QVERIFY(blocker.open(QIODevice::WriteOnly | QIODevice::Truncate));
        blocker.write("blocker");
        blocker.close();
    }

    QVERIFY(LoggingConfig::initialize(blockingFilePath, LogLevel::Info, false));
    QVERIFY2(LoggingConfig::isUsingFallbackDirectory(),
             "Expected fallback to engage when primary path is a file, not a directory");

    const QString dir = LoggingConfig::logDirectory();
    QVERIFY(!dir.isEmpty());
    QCOMPARE(QFileInfo(dir).canonicalFilePath(),
             QFileInfo(QDir::homePath()).canonicalFilePath());

    LoggingConfig::writeRecord(LogChannel::App, LogLevel::Info, "fallback-write");
    LoggingConfig::flush();

    const QString appPath = LoggingConfig::logFilePath();
    const QString serverPath = LoggingConfig::serverLogFilePath();
    QVERIFY(QFile::exists(appPath));

    // Clean up files in $HOME so the test does not litter the user environment.
    // Shutdown first so the files are closed before removal on Windows.
    LoggingConfig::shutdown();
    QFile::remove(appPath);
    QFile::remove(serverPath);
}

void LoggingConfigTest::doubleInitializeIsNoOp()
{
    const QString dir = m_tempDir.filePath("logs_double_init");
    QVERIFY(LoggingConfig::initialize(dir, LogLevel::Info, false));
    const QString firstPath = LoggingConfig::logFilePath();

    // Second call should be a no-op and report success without changing paths.
    QVERIFY(LoggingConfig::initialize(dir, LogLevel::Debug, false));
    QCOMPARE(LoggingConfig::logFilePath(), firstPath);
}

QTEST_MAIN(LoggingConfigTest)
#include "test_logging_config.moc"
