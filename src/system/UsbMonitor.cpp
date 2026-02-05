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
#include <QtConcurrent/QtConcurrent>

namespace multipack {
namespace system {

UsbMonitor::UsbMonitor(const QString& usbPath,
                       const QString& databasePath,
                       QObject* parent)
    : QObject(parent)
    , m_usbPath(usbPath)
    , m_databasePath(databasePath)
    , m_fileSystemWatcher(new QFileSystemWatcher(this))
    , m_periodicScanTimer(new QTimer(this))
    , m_updateWatcher(new QFutureWatcher<int>(this))
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

    connect(m_updateWatcher, &QFutureWatcher<int>::finished, this, [this]() {
        int updatedCount = m_updateWatcher->result();
        Q_UNUSED(updatedCount);
        m_isProcessing = false;
        if (m_updateQueued) {
            m_updateQueued = false;
            updateDatabaseFromUsbAsync();
        }
    });
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
    
    if (m_databasePath.isEmpty()) {
        qWarning() << "Cannot start USB monitoring: Database path is empty";
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
    if (m_databasePath.isEmpty()) {
        qCritical() << "Cannot update database: Database path is empty";
        emit databaseUpdateFailed("Database path is empty");
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
    
    database::DatabaseManager dbManager;
    if (!dbManager.open(m_databasePath)) {
        QString error = QString("Failed to open database: %1").arg(m_databasePath);
        qCritical() << error;
        emit databaseUpdateFailed(error);
        return 0;
    }

    QStringList updatedFiles;
    
    for (const QString& filename : robFiles) {
        // Skip files that previously failed in this session
        if (isFailedFile(filename)) {
            qDebug() << "Skipping previously failed file:" << filename;
            continue;
        }
        
        if (needsUpdate(filename)) {
            qDebug() << "Processing file:" << filename;
            
            if (processFile(filename, dbManager)) {
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
            updateDatabaseFromUsbAsync();
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

    // Compare file timestamp to last processed timestamp
    QDateTime fileTimestamp = getFileTimestamp(filename);
    QDateTime knownTimestamp = m_fileTimestamps.value(filename);

    if (!knownTimestamp.isValid()) {
        return true;
    }

    return fileTimestamp > knownTimestamp;
}

void UsbMonitor::updateDatabaseFromUsbAsync()
{
    if (m_isProcessing) {
        m_updateQueued = true;
        return;
    }

    m_isProcessing = true;
    auto future = QtConcurrent::run([this]() {
        return updateDatabaseFromUsb();
    });
    m_updateWatcher->setFuture(future);
}

bool UsbMonitor::processFile(const QString& filename, database::DatabaseManager& dbManager)
{
    // Parse the file using RobFileParser
    RobFileParser parser(m_usbPath);
    RobFileData fileData = parser.parseFile(filename);
    
    if (!fileData.isValid) {
        qWarning() << "Failed to parse file" << filename << ":" << fileData.errorMessage;
        return false;
    }
    
    // Save to database
    database::PaletteData paletteData;
    paletteData.metadata.fileName = QFileInfo(fileData.filePath).fileName();
    paletteData.metadata.fileTimestamp = fileData.fileTimestamp.toMSecsSinceEpoch();
    paletteData.metadata.paketQuer = fileData.g_paket_quer;
    paletteData.metadata.centerOfGravity = fileData.g_CenterOfGravity;
    paletteData.metadata.lageArten = fileData.g_LageArten;
    paletteData.metadata.anzLagen = fileData.g_AnzLagen;
    paletteData.metadata.anzahlPakete = fileData.g_AnzahlPakete;

    if (fileData.g_PalettenDim.size() >= 3) {
        paletteData.paletteDimensions.length = fileData.g_PalettenDim[0];
        paletteData.paletteDimensions.width = fileData.g_PalettenDim[1];
        paletteData.paletteDimensions.height = fileData.g_PalettenDim[2];
    }

    if (fileData.g_PaketDim.size() >= 4) {
        paletteData.packageDimensions.length = fileData.g_PaketDim[0];
        paletteData.packageDimensions.width = fileData.g_PaketDim[1];
        paletteData.packageDimensions.height = fileData.g_PaketDim[2];
        paletteData.packageDimensions.gap = fileData.g_PaketDim[3];
    }

    paletteData.rawData = fileData.g_Daten;
    paletteData.layerAssignments = fileData.g_LageZuordnung;
    paletteData.intermediaryLayers = fileData.g_Zwischenlagen;
    paletteData.packagesPerLayerType = fileData.g_PaketeZuordnung;

    for (const auto& pos : fileData.g_PaketPos) {
        if (pos.size() < 9) {
            continue;
        }
        database::PackagePosition position;
        position.xp = pos[0];
        position.yp = pos[1];
        position.ap = pos[2];
        position.xd = pos[3];
        position.yd = pos[4];
        position.ad = pos[5];
        position.nop = pos[6];
        position.xvec = pos[7];
        position.yvec = pos[8];
        paletteData.packagePositions.append(position);
    }

    bool saved = dbManager.savePaletteData(paletteData);
    if (saved) {
        updateFileTracking(filename, fileData.fileTimestamp);
    }
    return saved;
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
