/**
 * @file UsbMonitor.cpp
 * @brief Implementation of USB stick file monitoring
 */

#include "multipack/system/UsbMonitor.h"

#include <QDebug>
#include <QFileInfo>
#include <QMutexLocker>
#include <QtConcurrent/QtConcurrent>

namespace multipack {
namespace system {

namespace {

QStringList scanRobFiles(const QString& usbPath)
{
    QStringList robFiles;
    QDir usbDir(usbPath);

    if (!usbDir.exists()) {
        qWarning() << "USB directory does not exist:" << usbPath;
        return robFiles;
    }

    const QFileInfoList fileList = usbDir.entryInfoList({"*.rob"}, QDir::Files, QDir::Name);
    for (const QFileInfo& fileInfo : fileList) {
        robFiles.append(fileInfo.fileName());
    }

    return robFiles;
}

QDateTime getRobFileTimestamp(const QString& usbPath, const QString& filename)
{
    const QFileInfo fileInfo(QDir(usbPath).filePath(filename));
    return fileInfo.exists() ? fileInfo.lastModified() : QDateTime();
}

database::PaletteData toPaletteData(const RobFileData& fileData)
{
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

    return paletteData;
}

database::SaveResult processRobFile(const QString& usbPath,
                                    const QString& filename,
                                    database::DatabaseManager& dbManager,
                                    QString* errorMessage)
{
    RobFileParser parser(usbPath);
    const RobFileData fileData = parser.parseFile(filename);

    if (!fileData.isValid) {
        if (errorMessage) {
            *errorMessage = fileData.errorMessage;
        }
        qWarning() << "Failed to parse file" << filename << ":" << fileData.errorMessage;
        return database::SaveResult::Error;
    }

    const database::SaveResult saveResult = dbManager.savePaletteData(toPaletteData(fileData));
    if (saveResult == database::SaveResult::Error && errorMessage) {
        *errorMessage = QString("Database save failed for %1").arg(filename);
    }

    return saveResult;
}

UsbMonitor::UpdateResult runUpdatePass(const QString& usbPath,
                                       const QString& databasePath,
                                       const QSet<QString>& failedFilesSnapshot,
                                       const QHash<QString, QDateTime>& knownTimestampsSnapshot)
{
    UsbMonitor::UpdateResult result;

    if (databasePath.isEmpty()) {
        result.errorMessage = "Database path is empty";
        return result;
    }

    QDir usbDir(usbPath);
    if (!usbDir.exists()) {
        result.errorMessage = QString("USB directory does not exist: %1").arg(usbPath);
        return result;
    }

    database::DatabaseManager dbManager;
    if (!dbManager.open(databasePath)) {
        result.errorMessage = QString("Failed to open database: %1").arg(databasePath);
        return result;
    }

    const QStringList robFiles = scanRobFiles(usbPath);
    for (const QString& filename : robFiles) {
        const QDateTime currentTimestamp = getRobFileTimestamp(usbPath, filename);
        result.currentFiles.insert(filename);
        result.currentFileTimestamps.insert(filename, currentTimestamp);

        const QDateTime knownTimestamp = knownTimestampsSnapshot.value(filename);
        const bool previouslyFailed = failedFilesSnapshot.contains(filename);
        const bool unchangedFailedFile = previouslyFailed &&
            knownTimestamp.isValid() &&
            currentTimestamp.isValid() &&
            currentTimestamp <= knownTimestamp;

        if (unchangedFailedFile) {
            result.failedFiles.insert(filename);
            continue;
        }

        const bool requiresUpdate = !knownTimestamp.isValid() ||
            (currentTimestamp.isValid() && currentTimestamp > knownTimestamp) ||
            previouslyFailed;
        if (!requiresUpdate) {
            continue;
        }

        QString errorMessage;
        const database::SaveResult saveResult = processRobFile(usbPath, filename, dbManager, &errorMessage);
        if (saveResult == database::SaveResult::Error) {
            result.failedFiles.insert(filename);
            if (result.errorMessage.isEmpty()) {
                result.errorMessage = errorMessage;
            }
            continue;
        }

        if (saveResult == database::SaveResult::Inserted || saveResult == database::SaveResult::Updated) {
            result.updatedFiles.append(filename);
        }
    }

    result.success = true;
    return result;
}

} // namespace

UsbMonitor::UsbMonitor(const QString& usbPath,
                       const QString& databasePath,
                       QObject* parent)
    : QObject(parent)
    , m_usbPath(usbPath)
    , m_databasePath(databasePath)
    , m_fileSystemWatcher(new QFileSystemWatcher(this))
    , m_periodicScanTimer(new QTimer(this))
    , m_updateWatcher(new QFutureWatcher<UpdateResult>(this))
    , m_isMonitoring(false)
{
    connect(m_fileSystemWatcher, &QFileSystemWatcher::directoryChanged,
            this, &UsbMonitor::onDirectoryChanged);
    connect(m_fileSystemWatcher, &QFileSystemWatcher::fileChanged,
            this, &UsbMonitor::onFileChanged);

    m_periodicScanTimer->setInterval(30000);
    connect(m_periodicScanTimer, &QTimer::timeout,
            this, &UsbMonitor::performPeriodicScan);

    connect(m_updateWatcher, &QFutureWatcher<UpdateResult>::finished, this, [this]() {
        const UpdateResult result = m_updateWatcher->result();
        applyUpdateResult(result);

        bool shouldRestart = false;
        {
            QMutexLocker locker(&m_mutex);
            m_updateInFlight = false;
            if (m_updateQueued && !m_shutdownRequested) {
                m_updateQueued = false;
                shouldRestart = true;
            } else {
                m_updateQueued = false;
            }
        }

        if (shouldRestart) {
            updateDatabaseFromUsbAsync();
        }
    });
}

UsbMonitor::~UsbMonitor()
{
    stopMonitoring();
    waitForPendingUpdate();
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

    const QDir usbDir(m_usbPath);
    if (!usbDir.exists()) {
        qWarning() << "USB directory does not exist:" << m_usbPath;
        return false;
    }

    if (!initializeWatcher()) {
        qWarning() << "Failed to initialize file system watcher";
        return false;
    }

    m_shutdownRequested = false;
    setupInitialTracking();
    m_periodicScanTimer->start();
    m_isMonitoring = true;
    qDebug() << "USB monitoring started for path:" << m_usbPath;
    return true;
}

void UsbMonitor::stopMonitoring()
{
    {
        QMutexLocker locker(&m_mutex);
        m_shutdownRequested = true;

        if (m_isMonitoring) {
            m_periodicScanTimer->stop();
            m_fileSystemWatcher->removePaths(m_fileSystemWatcher->files());
            m_fileSystemWatcher->removePaths(m_fileSystemWatcher->directories());
            m_isMonitoring = false;
            qDebug() << "USB monitoring stopped";
        }
    }

    waitForPendingUpdate();
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
    QString usbPath;
    QString databasePath;
    QSet<QString> failedFilesSnapshot;
    QHash<QString, QDateTime> knownTimestampsSnapshot;

    {
        QMutexLocker locker(&m_mutex);
        usbPath = m_usbPath;
        databasePath = m_databasePath;
        failedFilesSnapshot = m_failedFiles;
        knownTimestampsSnapshot = m_fileTimestamps;
    }

    emit databaseUpdateStarted();
    const UpdateResult result = runUpdatePass(usbPath, databasePath, failedFilesSnapshot, knownTimestampsSnapshot);
    applyUpdateResult(result);
    return result.updatedFiles.size();
}

void UsbMonitor::updateDatabaseFromUsbAsync()
{
    QString usbPath;
    QString databasePath;
    QSet<QString> failedFilesSnapshot;
    QHash<QString, QDateTime> knownTimestampsSnapshot;

    {
        QMutexLocker locker(&m_mutex);
        if (m_shutdownRequested) {
            return;
        }

        if (m_updateInFlight) {
            m_updateQueued = true;
            return;
        }

        m_updateInFlight = true;
        usbPath = m_usbPath;
        databasePath = m_databasePath;
        failedFilesSnapshot = m_failedFiles;
        knownTimestampsSnapshot = m_fileTimestamps;
    }

    emit databaseUpdateStarted();
    auto future = QtConcurrent::run([usbPath, databasePath, failedFilesSnapshot, knownTimestampsSnapshot]() {
        return runUpdatePass(usbPath, databasePath, failedFilesSnapshot, knownTimestampsSnapshot);
    });
    m_updateWatcher->setFuture(future);
}

QStringList UsbMonitor::getAvailableRobFiles()
{
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
    qDebug() << "Directory changed:" << path;
    QTimer::singleShot(1000, this, &UsbMonitor::processChanges);
}

void UsbMonitor::onFileChanged(const QString& path)
{
    qDebug() << "File changed:" << path;
    QTimer::singleShot(1000, this, &UsbMonitor::processChanges);
}

void UsbMonitor::performPeriodicScan()
{
    qDebug() << "Performing periodic scan for USB changes";
    processChanges();
}

void UsbMonitor::processChanges()
{
    QSet<QString> knownFilesSnapshot;
    QHash<QString, QDateTime> knownTimestampsSnapshot;

    {
        QMutexLocker locker(&m_mutex);
        if (m_shutdownRequested) {
            return;
        }

        knownFilesSnapshot = m_knownFiles;
        knownTimestampsSnapshot = m_fileTimestamps;
    }

    const QStringList currentFiles = scanDirectory();
    const QSet<QString> currentFileSet(currentFiles.begin(), currentFiles.end());

    QStringList newFiles;
    QStringList modifiedFiles;
    for (const QString& filename : currentFiles) {
        if (!knownFilesSnapshot.contains(filename)) {
            newFiles.append(filename);
            continue;
        }

        const QDateTime currentTimestamp = getFileTimestamp(filename);
        const QDateTime knownTimestamp = knownTimestampsSnapshot.value(filename);
        if (knownTimestamp.isValid() && currentTimestamp.isValid() && currentTimestamp > knownTimestamp) {
            modifiedFiles.append(filename);
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        m_knownFiles = currentFileSet;

        for (auto it = m_fileTimestamps.begin(); it != m_fileTimestamps.end();) {
            if (!currentFileSet.contains(it.key())) {
                it = m_fileTimestamps.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = m_failedFiles.begin(); it != m_failedFiles.end();) {
            if (!currentFileSet.contains(*it)) {
                it = m_failedFiles.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (!newFiles.isEmpty()) {
        emit newFilesDetected(newFiles);
    }
    if (!modifiedFiles.isEmpty()) {
        emit filesModified(modifiedFiles);
    }

    if (!newFiles.isEmpty() || !modifiedFiles.isEmpty()) {
        updateDatabaseFromUsbAsync();
    }
}

bool UsbMonitor::initializeWatcher()
{
    if (!m_usbPath.isEmpty() && !m_fileSystemWatcher->addPath(m_usbPath)) {
        qWarning() << "Failed to watch directory:" << m_usbPath;
        return false;
    }

    return true;
}

QStringList UsbMonitor::scanDirectory()
{
    QString usbPath;
    {
        QMutexLocker locker(&m_mutex);
        usbPath = m_usbPath;
    }

    return scanRobFiles(usbPath);
}

bool UsbMonitor::needsUpdate(const QString& filename)
{
    const QDateTime fileTimestamp = getFileTimestamp(filename);

    QMutexLocker locker(&m_mutex);
    const QDateTime knownTimestamp = m_fileTimestamps.value(filename);
    if (m_failedFiles.contains(filename)) {
        return !knownTimestamp.isValid() || (fileTimestamp.isValid() && fileTimestamp > knownTimestamp);
    }

    return !knownTimestamp.isValid() || (fileTimestamp.isValid() && fileTimestamp > knownTimestamp);
}

database::SaveResult UsbMonitor::processFile(const QString& filename, database::DatabaseManager& dbManager)
{
    return processRobFile(m_usbPath, filename, dbManager, nullptr);
}

QDateTime UsbMonitor::getFileTimestamp(const QString& filename)
{
    QString usbPath;
    {
        QMutexLocker locker(&m_mutex);
        usbPath = m_usbPath;
    }

    return getRobFileTimestamp(usbPath, filename);
}

void UsbMonitor::updateFileTracking(const QString& filename, const QDateTime& timestamp)
{
    QMutexLocker locker(&m_mutex);
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
    const QStringList initialFiles = scanRobFiles(m_usbPath);
    m_knownFiles = QSet<QString>(initialFiles.begin(), initialFiles.end());
    qDebug() << "Initial tracking set up for" << initialFiles.size() << "files";
}

void UsbMonitor::waitForPendingUpdate()
{
    if (m_updateWatcher->isRunning()) {
        m_updateWatcher->waitForFinished();
    }
}

void UsbMonitor::applyUpdateResult(const UpdateResult& result)
{
    if (result.success) {
        QMutexLocker locker(&m_mutex);
        m_knownFiles = result.currentFiles;
        m_fileTimestamps = result.currentFileTimestamps;
        m_failedFiles = result.failedFiles;
    }

    if (!result.errorMessage.isEmpty()) {
        qWarning() << result.errorMessage;
        emit databaseUpdateFailed(result.errorMessage);
    }

    if (!result.updatedFiles.isEmpty()) {
        emit databaseUpdateCompleted(result.updatedFiles);
    }
}

} // namespace system
} // namespace multipack
