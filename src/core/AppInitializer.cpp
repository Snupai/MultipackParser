/**
 * @file AppInitializer.cpp
 * @brief Implementation of the application initialization sequence
 */

#include "multipack/core/AppInitializer.h"
#include "multipack/config/SettingsManager.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/network/XmlRpcServer.h"
#include "multipack/robot/RobotController.h"
#include "multipack/audio/AudioManager.h"
#include "multipack/system/UsbMonitor.h"
#include "multipack/system/AutoUpdater.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>

namespace multipack {
namespace core {

AppInitializer::AppInitializer(QObject* parent)
    : QObject(parent)
{
    qDebug() << "AppInitializer - constructor";
}

AppInitializer::~AppInitializer()
{
    qDebug() << "AppInitializer - destructor";
    if (m_initialized) {
        shutdown();
    }
}

bool AppInitializer::initialize(ProgressCallback progressCallback)
{
    qDebug() << "AppInitializer - starting initialization sequence";

    m_progressCallback = progressCallback;

    // Step 1: Initialize logging (5%)
    reportProgress(5, "Initializing logging...");
    if (!initializeLogging()) {
        emit initializationFailed("Failed to initialize logging");
        return false;
    }

    // Step 2: Load settings (15%)
    reportProgress(15, "Loading settings...");
    if (!initializeSettings()) {
        emit initializationFailed("Failed to load settings");
        return false;
    }

    // Step 3: Initialize database (30%)
    reportProgress(30, "Initializing database...");
    if (!initializeDatabase()) {
        emit initializationFailed("Failed to initialize database");
        return false;
    }

    // Step 4: Refresh database from USB (40%)
    reportProgress(40, "Updating database from USB...");
    if (!initializeUsbMonitor()) {
        qWarning() << "USB monitoring initialization failed - continuing without USB updates";
    }

    // Step 5: Start XML-RPC server (50%)
    reportProgress(50, "Starting XML-RPC server...");
    if (!initializeXmlRpcServer()) {
        // Not fatal - continue without server
        qWarning() << "XML-RPC server failed to start - continuing";
    }

    // Step 6: Initialize robot controller (70%)
    reportProgress(70, "Connecting to robot...");
    if (!initializeRobotController()) {
        // Robot connection failure is not fatal
        qWarning() << "Robot connection failed - continuing without robot";
    }

    // Step 7: Initialize audio (85%)
    reportProgress(85, "Initializing audio...");
    if (!initializeAudio()) {
        // Audio failure is not fatal
        qWarning() << "Audio initialization failed - continuing without audio";
    }

    // Step 8: Initialize auto-updater (90%)
    reportProgress(90, "Initializing update system...");
    if (!initializeAutoUpdater()) {
        // Auto-updater failure is not fatal
        qWarning() << "Auto-updater initialization failed - continuing without auto-update";
    }

    // Step 9: Complete (100%)
    reportProgress(100, "Initialization complete");

    m_initialized = true;
    emit initializationComplete(true);

    qDebug() << "AppInitializer - completed successfully";
    return true;
}

void AppInitializer::shutdown()
{
    qDebug() << "AppInitializer - shutting down subsystems";

    if (!m_initialized) {
        return;
    }

    // Shutdown in reverse order
    if (m_audioManager) {
        qDebug() << "Shutting down audio manager...";
        m_audioManager.reset();
    }

    if (m_robotController) {
        qDebug() << "Shutting down robot controller...";
        m_robotController->disconnect();
        m_robotController.reset();
    }

    if (m_xmlRpcServer) {
        qDebug() << "Shutting down XML-RPC server...";
        m_xmlRpcServer.reset();
    }

    if (m_usbMonitor) {
        qDebug() << "Stopping USB monitor...";
        m_usbMonitor.reset();
    }

    if (m_databaseManager) {
        qDebug() << "Closing database...";
        m_databaseManager->close();
        m_databaseManager.reset();
    }

    if (m_settingsManager) {
        qDebug() << "Saving settings...";
        if (!m_settingsManager->save()) {
            qWarning() << "Failed to save settings during shutdown";
        }
        m_settingsManager.reset();
    }

    m_initialized = false;
    qDebug() << "AppInitializer - complete";
}

config::SettingsManager* AppInitializer::settingsManager() const
{
    return m_settingsManager.get();
}

database::DatabaseManager* AppInitializer::databaseManager() const
{
    return m_databaseManager.get();
}

network::XmlRpcServer* AppInitializer::xmlRpcServer() const
{
    return m_xmlRpcServer.get();
}

robot::RobotController* AppInitializer::robotController() const
{
    return m_robotController.get();
}

audio::AudioManager* AppInitializer::audioManager() const
{
    return m_audioManager.get();
}

system::AutoUpdater* AppInitializer::autoUpdater() const
{
    return m_autoUpdater.get();
}

bool AppInitializer::initializeLogging()
{
    qDebug() << "AppInitializer - initializing logging";

    // Create logs directory
    QString logPath = QDir::currentPath() + "/logs";
    QDir logDir(logPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    qDebug() << "Log directory:" << logPath;
    return true;
}

bool AppInitializer::initializeSettings()
{
    qDebug() << "AppInitializer - initializing settings";

    m_settingsManager = std::make_unique<config::SettingsManager>();

    // Try to load settings
    QString settingsPath = QDir::currentPath() + "/settings.json";
    if (QFile::exists(settingsPath)) {
        if (!m_settingsManager->load(settingsPath)) {
            qWarning() << "Failed to load settings from" << settingsPath;
            // Continue with default settings
        }
    } else {
        qDebug() << "No settings file found, using defaults";
    }

    return true;
}

bool AppInitializer::initializeDatabase()
{
    qDebug() << "AppInitializer - initializing database";

    m_databaseManager = std::make_unique<database::DatabaseManager>();

    // Open database
    m_databasePath = QDir::currentPath() + "/paletten.db";
    if (!m_databaseManager->open(m_databasePath)) {
        qCritical() << "Failed to open database:" << m_databasePath;
        return false;
    }

    // Create tables if needed
    if (!m_databaseManager->createTables()) {
        qCritical() << "Failed to create database tables";
        return false;
    }

    qDebug() << "Database opened:" << m_databasePath;
    return true;
}

bool AppInitializer::initializeXmlRpcServer()
{
    qDebug() << "AppInitializer - initializing XML-RPC server";

    m_xmlRpcServer = std::make_unique<network::XmlRpcServer>();
    // Server will be started when user clicks "Start Server" in UI
    return true;
}

bool AppInitializer::initializeRobotController()
{
    qDebug() << "AppInitializer - initializing robot controller";

    m_robotController = std::make_unique<robot::RobotController>();

    // Try to connect to robot at default IP
    // Connection is optional - robot may not be available during development
    QString robotIp = "192.168.0.1";
    if (m_settingsManager) {
        // Could get IP from settings if configured
    }

    qDebug() << "Robot controller initialized (not connected yet)";
    return true;
}

bool AppInitializer::initializeAudio()
{
    qDebug() << "AppInitializer - initializing audio";

    m_audioManager = std::make_unique<audio::AudioManager>();

    if (!m_audioManager->initialize()) {
        qWarning() << "Failed to initialize audio manager";
        m_audioManager.reset();
        return false;
    }

    // Load default audio files if they exist
    QString audioPath = QDir::currentPath() + "/audio";
    if (QDir(audioPath).exists()) {
        // Audio manager will load files when needed
    }

    return true;
}

bool AppInitializer::initializeAutoUpdater()
{
    qDebug() << "AppInitializer - initializing auto-updater";

    m_autoUpdater = std::make_unique<system::AutoUpdater>();

    qDebug() << "Auto-updater initialized";
    return true;
}

bool AppInitializer::initializeUsbMonitor()
{
    if (!m_settingsManager || !m_databaseManager) {
        qWarning() << "Cannot initialize USB monitor: missing settings or database";
        return false;
    }

    QString usbPath = m_settingsManager->usbPath();
    if (usbPath.isEmpty()) {
        qWarning() << "USB path is empty; skipping USB monitor";
        return false;
    }

    m_usbMonitor = std::make_unique<system::UsbMonitor>(usbPath, m_databasePath);

    m_usbMonitor->updateDatabaseFromUsbAsync();
    qDebug() << "USB database update triggered";

    if (!m_usbMonitor->startMonitoring()) {
        qWarning() << "Failed to start USB monitoring";
        return false;
    }

    return true;
}

void AppInitializer::reportProgress(int percentage, const QString& message)
{
    emit initializationProgress(percentage, message);

    if (m_progressCallback) {
        m_progressCallback(percentage, message);
    }
}

} // namespace core
} // namespace multipack
