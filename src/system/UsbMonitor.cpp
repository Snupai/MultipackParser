/**
 * @file UsbMonitor.cpp
 * @brief Implementation of USB stick file monitoring
 */

#include "multipack/system/UsbMonitor.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMutexLocker>
#include <QApplication>

namespace multipack {
namespace system {

UsbMonitor::UsbMonitor(const QString& usbPath, 
                       database::DatabaseManager* dbManager,
                       QObject* parent)
    : QObject(parent)
    , m_usbPath(usbPath)
    , m_dbManager(dbManager)
    , m_fileSystemWatcher(new QFileSystemWatcher(this))
    , m_periodicScanTimer(new QTimer(this))
    , m_isMonitoring(false)
    , m_isProcessing(false)
{
    // Connect file system watcher signals
    connect(m_fileSystemWatcher, &QFileSystemWatcher::directoryChanged,
            this, &UsbMonitor::onDirectoryChanged);
    connect(m_fileSystemWatcher, &QFileSystemWatcher::fileChanged,
            this, &UsbMonitor::onFileChanged);
    
    // Set up periodic scan timer (backup mechanism every 30 seconds)
    m_periodicScanTimer->setInterval(30000);
    connect(m_periodicScanTimer, &QTimer::timeout,
            this, &UsbMonitor::performPeriodicScan);
}

UsbMonitor::~UsbMonitor()
{
    stopMonitoring();
}

bool UsbMonitor::startMonitoring()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isMonitoring) {
        qWarning() << "USB monitoring is already active";
        return true;
    }
    
    if (!m_dbManager) {
        qWarning() << "Cannot start USB monitoring: Database manager is null";
        return false;
    }
    
    // Check if USB directory exists
    QDir usbDir(m_usbPath);
    if (!usbDir.exists()) {
        qWarning() << "USB directory does not exist:" << m_usbPath;
        return false;
    }
    
    // Initialize file system watcher
    if (!initializeWatcher()) {
        qWarning() << "Failed to initialize file system watcher";
        return false;
    }
    
    // Set up initial tracking
    setupInitialTracking();
    
    // Start periodic scan timer
    m_periodicScanTimer->start();
    
    m_isMonitoring = true;
    qDebug() << "USB monitoring started for path:" << m_usbPath;
    
    return true;
}

void UsbMonitor::stopMonitoring()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_isMonitoring) {
        return;
    }
    
    m_periodicScanTimer->stop();
    m_fileSystemWatcher->removePaths(m_fileSystemWatcher->files());
    m_fileSystemWatcher->removePaths(m_fileSystemWatcher->directories());
    
    m_isMonitoring = false;
    qDebug() << "USB monitoring stopped";
}

bool UsbMonitor::isMonitoring() const
{
    QMutexLocker locker(&m_mutex);
    return m_isMonitoring;
}

void UsbMonitor::setUsbPath(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isMonitoring) {
        qWarning() << "Cannot change USB path while monitoring is active";
        return;
    }
    
    m_usbPath = path;
}

QString UsbMonitor::getUsbPath() const
{
    QMutexLocker locker(&m_mutex);
    return m_usbPath;
}

int UsbMonitor::updateDatabaseFromUsb()
{
    if (!m_dbManager) {
        qCritical() << "Cannot update database: Database manager is null";
        emit databaseUpdateFailed("Database manager is null");
        return 0;
    }
    
    QDir usbDir(m_usbPath);
    if (!usbDir.exists()) {
        QString error = QString("USB directory does not exist: %1").arg(m_usbPath);
        qCritical() << error;
        emit databaseUpdateFailed(error);
        return 0;
    }
    
    emit databaseUpdateStarted();
    
    QStringList robFiles = scanDirectory();
    qDebug() << "Found" << robFiles.size() << ".rob files to process";
    
    QStringList updatedFiles;
    
    for (const QString& filename : robFiles) {
        // Skip files that previously failed in this session
        if (isFailedFile(filename)) {
            qDebug() << "Skipping previously failed file:" << filename;
            continue;
        }
        
        if (needsUpdate(filename)) {
            qDebug() << "Processing file:" << filename;
            
            if (processFile(filename)) {
                updatedFiles.append(filename);
                qDebug() << "Successfully processed:" << filename;
            } else {
                addFailedFile(filename);
                qWarning() << "Failed to process file:" << filename;
            }
        } else {
            qDebug() << "File is up to date:" << filename;
        }
    }
    
    if (!updatedFiles.isEmpty()) {
        emit databaseUpdateCompleted(updatedFiles);
    }
    
    return updatedFiles.size();
}

QStringList UsbMonitor::getAvailableRobFiles()
{
    QMutexLocker locker(&m_mutex);
    return scanDirectory();
}

QSet<QString> UsbMonitor::getFailedFiles() const
{
    QMutexLocker locker(&m_mutex);
    return m_failedFiles;
}

void UsbMonitor::clearFailedFiles()
{
    QMutexLocker locker(&m_mutex);
    m_failedFiles.clear();
    qDebug() << "Failed files cache cleared";
}

void UsbMonitor::onDirectoryChanged(const QString& path)
{
    Q_UNUSED(path);
    qDebug() << "Directory changed:" << path;
    
    // Use a timer to batch multiple rapid changes
    QTimer::singleShot(1000, this, &UsbMonitor::processChanges);
}

void UsbMonitor::onFileChanged(const QString& path)
{
    Q_UNUSED(path);
    qDebug() << "File changed:" << path;
    
    // Use a timer to batch multiple rapid changes
    QTimer::singleShot(1000, this, &UsbMonitor::processChanges);
}

void UsbMonitor::performPeriodicScan()
{
    if (m_isProcessing) {
        return; // Skip if already processing
    }
    
    qDebug() << "Performing periodic scan for USB changes";
    processChanges();
}

void UsbMonitor::processChanges()
{
    if (m_isProcessing) {
        return;
    }
    
    m_isProcessing = true;
    
    try {
        QStringList currentFiles = scanDirectory();
        
        // Detect new files
        QStringList newFiles;
        for (const QString& filename : currentFiles) {
            if (!m_knownFiles.contains(filename)) {
                newFiles.append(filename);
                m_knownFiles.insert(filename);
                emit newFilesDetected({filename});
            }
        }
        
        // Detect modified files
        QStringList modifiedFiles;
        for (const QString& filename : currentFiles) {
            QDateTime currentTimestamp = getFileTimestamp(filename);
            QDateTime knownTimestamp = m_fileTimestamps.value(filename);
            
            if (knownTimestamp.isValid() && currentTimestamp > knownTimestamp) {
                modifiedFiles.append(filename);
                updateFileTracking(filename, currentTimestamp);
                emit filesModified({filename});
            } else if (!knownTimestamp.isValid()) {
                // New file, track its timestamp
                updateFileTracking(filename, currentTimestamp);
            }
        }
        
        // Remove files that no longer exist
        QSet<QString> removedFiles = m_knownFiles - QSet<QString>(currentFiles.begin(), currentFiles.end());
        for (const QString& filename : removedFiles) {
            m_knownFiles.remove(filename);
            m_fileTimestamps.remove(filename);
            m_failedFiles.remove(filename);
        }
        
        // Update database if there are changes
        if (!newFiles.isEmpty() || !modifiedFiles.isEmpty()) {
            updateDatabaseFromUsb();
        }
        
    } catch (const std::exception& e) {
        qCritical() << "Exception during change processing:" << e.what();
    }
    
    m_isProcessing = false;
}

bool UsbMonitor::initializeWatcher()
{
    if (!m_usbPath.isEmpty()) {
        if (!m_fileSystemWatcher->addPath(m_usbPath)) {
            qWarning() << "Failed to watch directory:" << m_usbPath;
            return false;
        }
    }
    
    return true;
}

QStringList UsbMonitor::scanDirectory()
{
    QStringList robFiles;
    QDir usbDir(m_usbPath);
    
    if (!usbDir.exists()) {
        qWarning() << "USB directory does not exist:" << m_usbPath;
        return robFiles;
    }
    
    QStringList filters;
    filters << "*.rob";
    usbDir.setNameFilters(filters);
    
    QFileInfoList fileList = usbDir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& fileInfo : fileList) {
        robFiles.append(fileInfo.fileName());
    }
    
    return robFiles;
}

bool UsbMonitor::needsUpdate(const QString& filename)
{
    // Check if file is in failed list
    if (isFailedFile(filename)) {
        return false;
    }
    
    // Get database timestamp
    QDateTime dbTimestamp;
    if (m_dbManager) {
        dbTimestamp = m_dbManager->getFileTimestamp(filename);
    }
    
    // Get file timestamp
    QDateTime fileTimestamp = getFileTimestamp(filename);
    
    // Update if file is newer than database entry or no database entry exists
    return !dbTimestamp.isValid() || fileTimestamp > dbTimestamp;
}

bool UsbMonitor::processFile(const QString& filename)
{
    if (!m_dbManager) {
        return false;
    }
    
    // Parse the file using RobFileParser
    RobFileParser parser(m_usbPath);
    RobFileData fileData = parser.parseFile(filename);
    
    if (!fileData.isValid) {
        qWarning() << "Failed to parse file" << filename << ":" << fileData.errorMessage;
        return false;
    }
    
    // Save to database
    return m_dbManager->saveRobFile(fileData);
}

QDateTime UsbMonitor::getFileTimestamp(const QString& filename)
{
    QString fullPath = QDir(m_usbPath).filePath(filename);
    QFileInfo fileInfo(fullPath);
    return fileInfo.exists() ? fileInfo.lastModified() : QDateTime();
}

void UsbMonitor::updateFileTracking(const QString& filename, const QDateTime& timestamp)
{
    m_fileTimestamps[filename] = timestamp;
}

void UsbMonitor::addFailedFile(const QString& filename)
{
    QMutexLocker locker(&m_mutex);
    m_failedFiles.insert(filename);
}

bool UsbMonitor::isFailedFile(const QString& filename) const
{
    QMutexLocker locker(&m_mutex);
    return m_failedFiles.contains(filename);
}

void UsbMonitor::setupInitialTracking()
{
    QStringList initialFiles = scanDirectory();
    
    for (const QString& filename : initialFiles) {
        m_knownFiles.insert(filename);
        updateFileTracking(filename, getFileTimestamp(filename));
    }
    
    qDebug() << "Initial tracking set up for" << initialFiles.size() << "files";
}

} // namespace system
} // namespace multipack

#include "UsbMonitor.moc"