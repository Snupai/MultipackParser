/**
 * @file AppInitializer.h
 * @brief Application initialization sequence manager
 *
 * Manages the startup sequence including:
 * - Loading configuration
 * - Initializing database
 * - Starting XML-RPC server
 * - Connecting to robot
 * - Setting up UI
 */

#ifndef MULTIPACK_CORE_APPINITIALIZER_H
#define MULTIPACK_CORE_APPINITIALIZER_H

#include <QObject>
#include <functional>
#include <memory>

namespace multipack {

// Forward declarations
namespace config {
class SettingsManager;
}
namespace database {
class DatabaseManager;
}
namespace network {
class XmlRpcServer;
}
namespace robot {
class RobotController;
}
namespace audio {
class AudioManager;
}
namespace system {
class UsbMonitor;
class AutoUpdater;
}

namespace core {

/**
 * @brief Callback type for progress updates
 * @param percentage Progress percentage (0-100)
 * @param message Status message to display
 */
using ProgressCallback = std::function<void(int percentage, const QString& message)>;

/**
 * @class AppInitializer
 * @brief Manages application initialization sequence
 *
 * Coordinates the startup of all application subsystems
 * in the correct order, with progress reporting.
 */
class AppInitializer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new App Initializer
     * @param parent Parent QObject
     */
    explicit AppInitializer(QObject* parent = nullptr);

    /**
     * @brief Destroy the App Initializer
     */
    ~AppInitializer() override;

    /**
     * @brief Run the initialization sequence
     * @param progressCallback Optional callback for progress updates
     * @return true if all initialization succeeded
     */
    bool initialize(ProgressCallback progressCallback = nullptr);

    /**
     * @brief Shut down all subsystems gracefully
     */
    void shutdown();

    // Accessors for initialized components
    config::SettingsManager* settingsManager() const;
    database::DatabaseManager* databaseManager() const;
    network::XmlRpcServer* xmlRpcServer() const;
    robot::RobotController* robotController() const;
    audio::AudioManager* audioManager() const;
    system::AutoUpdater* autoUpdater() const;
    system::UsbMonitor* usbMonitor() const;

signals:
    /**
     * @brief Emitted during initialization with progress
     * @param percentage Progress percentage (0-100)
     * @param message Status message
     */
    void initializationProgress(int percentage, const QString& message);

    /**
     * @brief Emitted when initialization completes
     * @param success True if successful
     */
    void initializationComplete(bool success);

    /**
     * @brief Emitted if initialization fails
     * @param error Error message
     */
    void initializationFailed(const QString& error);

private:
    /**
     * @brief Initialize logging system
     * @return true on success
     */
    bool initializeLogging();

    /**
     * @brief Load application settings
     * @return true on success
     */
    bool initializeSettings();

    /**
     * @brief Initialize database connection
     * @return true on success
     */
    bool initializeDatabase();

    /**
     * @brief Start XML-RPC server
     * @return true on success
     */
    bool initializeXmlRpcServer();

    /**
     * @brief Initialize robot controller
     * @return true on success
     */
    bool initializeRobotController();

    /**
     * @brief Initialize audio system
     * @return true on success
     */
    bool initializeAudio();

    /**
     * @brief Initialize auto-updater system
     * @return true on success
     */
    bool initializeAutoUpdater();

    /**
     * @brief Initialize USB monitoring and database refresh
     * @return true if initialization succeeded
     */
    bool initializeUsbMonitor();

    /**
     * @brief Report progress
     * @param percentage Progress percentage
     * @param message Status message
     */
    void reportProgress(int percentage, const QString& message);

    ProgressCallback m_progressCallback;

    std::unique_ptr<config::SettingsManager> m_settingsManager;
    std::unique_ptr<database::DatabaseManager> m_databaseManager;
    std::unique_ptr<network::XmlRpcServer> m_xmlRpcServer;
    std::unique_ptr<robot::RobotController> m_robotController;
    std::unique_ptr<audio::AudioManager> m_audioManager;
    std::unique_ptr<system::AutoUpdater> m_autoUpdater;
    std::unique_ptr<system::UsbMonitor> m_usbMonitor;
    QString m_databasePath;

    bool m_initialized = false;
};

} // namespace core
} // namespace multipack

#endif // MULTIPACK_CORE_APPINITIALIZER_H
