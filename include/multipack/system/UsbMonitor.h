#ifndef MULTIPACK_SYSTEM_USBMONITOR_H
#define MULTIPACK_SYSTEM_USBMONITOR_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QDateTime>
#include <QStringList>
#include <QSet>
#include <QMutex>
#include <QDir>
#include "multipack/system/RobFileParser.h"
#include "multipack/database/DatabaseManager.h"

namespace multipack {
namespace system {

/**
 * @brief USB stick file monitoring for automatic database updates
 * 
 * This class monitors a USB stick directory for .rob file changes and automatically
 * updates the database when new or modified files are detected. It provides the
 * same functionality as the Python update_database_from_usb() function.
 */
class UsbMonitor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param usbPath Path to USB stick directory to monitor
     * @param dbManager Pointer to database manager for updates
     * @param parent Parent QObject
     */
    explicit UsbMonitor(const QString& usbPath, 
                       database::DatabaseManager* dbManager,
                       QObject* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~UsbMonitor() override;
    
    /**
     * @brief Start monitoring the USB directory
     * @return true if monitoring started successfully
     */
    bool startMonitoring();
    
    /**
     * @brief Stop monitoring the USB directory
     */
    void stopMonitoring();
    
    /**
     * @brief Check if monitoring is currently active
     * @return true if monitoring is active
     */
    bool isMonitoring() const;
    
    /**
     * @brief Set the USB path to monitor
     * @param path New USB path
     */
    void setUsbPath(const QString& path);
    
    /**
     * @brief Get the current USB path being monitored
     * @return Current USB path
     */
    QString getUsbPath() const;
    
    /**
     * @brief Manually trigger a database update from USB files
     * @return Number of files processed
     */
    int updateDatabaseFromUsb();
    
    /**
     * @brief Get list of currently available .rob files
     * @return List of .rob file names
     */
    QStringList getAvailableRobFiles();
    
    /**
     * @brief Get list of failed files from current session
     * @return Set of failed file names
     */
    QSet<QString> getFailedFiles() const;
    
    /**
     * @brief Clear the failed files cache
     */
    void clearFailedFiles();

signals:
    /**
     * @brief Emitted when database update starts
     */
    void databaseUpdateStarted();
    
    /**
     * @brief Emitted when database update completes
     * @param filesUpdated List of files that were updated
     */
    void databaseUpdateCompleted(const QStringList& filesUpdated);
    
    /**
     * @brief Emitted when database update fails
     * @param errorMessage Error message
     */
    void databaseUpdateFailed(const QString& errorMessage);
    
    /**
     * @brief Emitted when new .rob files are detected
     * @param newFiles List of new file names
     */
    void newFilesDetected(const QStringList& newFiles);
    
    /**
     * @brief Emitted when .rob files are modified
     * @param modifiedFiles List of modified file names
     */
    void filesModified(const QStringList& modifiedFiles);

private slots:
    /**
     * @brief Handle directory changes
     * @param path Path that changed
     */
    void onDirectoryChanged(const QString& path);
    
    /**
     * @brief Handle file changes
     * @param path File that changed
     */
    void onFileChanged(const QString& path);
    
    /**
     * @brief Periodic scan for changes (backup mechanism)
     */
    void performPeriodicScan();
    
    /**
     * @brief Process detected changes
     */
    void processChanges();

private:
    QString m_usbPath;
    database::DatabaseManager* m_dbManager;
    QFileSystemWatcher* m_fileSystemWatcher;
    QTimer* m_periodicScanTimer;
    
    // State tracking
    bool m_isMonitoring;
    QSet<QString> m_failedFiles;  // Files that failed to parse in current session
    QSet<QString> m_knownFiles;   // Files we know about
    QHash<QString, QDateTime> m_fileTimestamps; // Last known timestamps
    
    // Thread safety
    mutable QMutex m_mutex;
    
    // Processing state
    bool m_isProcessing;
    QStringList m_pendingChanges;
    
    /**
     * @brief Initialize the file system watcher
     * @return true if successful
     */
    bool initializeWatcher();
    
    /**
     * @brief Scan directory for .rob files
     * @return List of detected file names
     */
    QStringList scanDirectory();
    
    /**
     * @brief Check if a file needs updating based on timestamps
     * @param filename Name of the file to check
     * @return true if file needs updating
     */
    bool needsUpdate(const QString& filename);
    
    /**
     * @brief Process a single .rob file
     * @param filename Name of the file to process
     * @return true if successful
     */
    bool processFile(const QString& filename);
    
    /**
     * @brief Get file timestamp
     * @param filename Name of the file
     * @return File timestamp
     */
    QDateTime getFileTimestamp(const QString& filename);
    
    /**
     * @brief Update internal file tracking
     * @param filename Name of the file
     * @param timestamp File timestamp
     */
    void updateFileTracking(const QString& filename, const QDateTime& timestamp);
    
    /**
     * @brief Add file to failed list
     * @param filename Name of the file that failed
     */
    void addFailedFile(const QString& filename);
    
    /**
     * @brief Check if file is in failed list
     * @param filename Name of the file to check
     * @return true if file is in failed list
     */
    bool isFailedFile(const QString& filename) const;
    
    /**
     * @brief Set up initial file tracking
     */
    void setupInitialTracking();
};

} // namespace system
} // namespace multipack

#endif // MULTIPACK_SYSTEM_USBMONITOR_H