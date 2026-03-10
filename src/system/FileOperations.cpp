/**
 * @file FileOperations.cpp
 * @brief Implementation of file operation utilities
 */

#include "multipack/system/FileOperations.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QStorageInfo>
#include <QRegularExpression>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace multipack {
namespace system {

namespace FileOperations {

/**
 * @brief Parse a single line of tab-separated integers
 * @param line The line to parse
 * @return Vector of integers
 */
static QVector<int> parseIntLine(const QString& line)
{
    QVector<int> values;
    QStringList parts = line.split('\t', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        bool ok;
        int value = part.trimmed().toInt(&ok);
        if (ok) {
            values.append(value);
        }
    }
    return values;
}

/**
 * @brief Try to read file with different encodings
 * @param path File path
 * @param lines Output: parsed lines
 * @return true if successful
 */
static bool readFileWithEncodings(const QString& path, QVector<QVector<int>>& lines)
{
    // Try different encodings in order of likelihood
    QStringList encodings = {"UTF-8", "ISO-8859-1", "Windows-1252"};

    for (const QString& encoding : encodings) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (encoding == "UTF-8") {
            stream.setEncoding(QStringConverter::Utf8);
        } else if (encoding == "ISO-8859-1") {
            stream.setEncoding(QStringConverter::Latin1);
        }
        // Windows-1252 falls back to Latin1 in Qt6
#else
        stream.setCodec(encoding.toUtf8().constData());
#endif

        lines.clear();
        bool parseError = false;

        while (!stream.atEnd()) {
            QString line = stream.readLine();
            if (line.trimmed().isEmpty()) {
                lines.append(QVector<int>());  // Keep empty lines for structure
                continue;
            }

            QVector<int> values = parseIntLine(line);
            if (values.isEmpty() && !line.trimmed().isEmpty()) {
                // Non-empty line but couldn't parse - might be encoding issue
                parseError = true;
                break;
            }
            lines.append(values);
        }

        file.close();

        if (!parseError && !lines.isEmpty()) {
            qDebug() << "Successfully parsed file with encoding:" << encoding;
            return true;
        }
    }

    return false;
}

RobFileData parseRobFile(const QString& path)
{
    qDebug() << "FileOperations::parseRobFile -" << path;

    RobFileData data;
    data.filePath = path;
    data.fileName = baseName(path) + ".rob";
    data.centerOfGravity = {0.0, 0.0, 0.0};

    // Get file info
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        data.errorMessage = "File does not exist";
        qWarning() << data.errorMessage << ":" << path;
        return data;
    }
    data.fileTimestamp = fileInfo.lastModified().toMSecsSinceEpoch();

    // Parse file content
    QVector<QVector<int>> rawData;
    if (!readFileWithEncodings(path, rawData)) {
        data.errorMessage = "Could not decode file with any encoding";
        qWarning() << data.errorMessage << ":" << path;
        return data;
    }

    data.rawData = rawData;

    // Validate minimum structure
    // Need at least 5 lines: palette, package, layer types, num layers, separator
    if (rawData.size() < 5) {
        data.errorMessage = "File too short - missing required data";
        qWarning() << data.errorMessage;
        return data;
    }

    // Data index constants (matching Python LI_* constants)
    constexpr int LI_PALETTE_DATA = 0;
    constexpr int LI_PACKAGE_DATA = 1;
    constexpr int LI_LAYERTYPES = 2;
    constexpr int LI_NUMBER_OF_LAYERS = 3;

    // Position indices within each position line
    constexpr int LI_POSITION_XP = 0;
    constexpr int LI_POSITION_YP = 1;
    constexpr int LI_POSITION_AP = 2;
    constexpr int LI_POSITION_XD = 3;
    constexpr int LI_POSITION_YD = 4;
    constexpr int LI_POSITION_AD = 5;
    constexpr int LI_POSITION_NOP = 6;
    constexpr int LI_POSITION_XVEC = 7;
    constexpr int LI_POSITION_YVEC = 8;

    // Parse palette dimensions from line 0 [length, width, height]
    if (rawData[LI_PALETTE_DATA].size() >= 3) {
        data.paletteLength = rawData[LI_PALETTE_DATA][0];
        data.paletteWidth = rawData[LI_PALETTE_DATA][1];
        data.paletteHeight = rawData[LI_PALETTE_DATA][2];
    } else {
        data.errorMessage = "Invalid palette dimensions line";
        qWarning() << data.errorMessage;
        return data;
    }

    // Parse package dimensions from line 1 [length, width, height, gap]
    if (rawData[LI_PACKAGE_DATA].size() >= 4) {
        data.packageLength = rawData[LI_PACKAGE_DATA][0];
        data.packageWidth = rawData[LI_PACKAGE_DATA][1];
        data.packageHeight = rawData[LI_PACKAGE_DATA][2];
        data.packageGap = rawData[LI_PACKAGE_DATA][3];
    } else {
        data.errorMessage = "Invalid package dimensions line";
        qWarning() << data.errorMessage;
        return data;
    }

    // Parse number of layer types from line 2
    if (rawData[LI_LAYERTYPES].size() >= 1) {
        data.layerTypeCount = rawData[LI_LAYERTYPES][0];
    } else {
        data.errorMessage = "Invalid layer types line";
        qWarning() << data.errorMessage;
        return data;
    }

    // Parse number of layers from line 3
    if (rawData[LI_NUMBER_OF_LAYERS].size() >= 1) {
        data.numberOfLayers = rawData[LI_NUMBER_OF_LAYERS][0];
    } else {
        data.errorMessage = "Invalid number of layers line";
        qWarning() << data.errorMessage;
        return data;
    }

    // Parse layer assignments and intermediate layers
    // Start at line 5 (after separator line 4), one line per layer
    int index = LI_NUMBER_OF_LAYERS + 2;  // Skip header lines
    int endIndex = index + data.numberOfLayers;

    if (static_cast<int>(rawData.size()) < endIndex) {
        data.errorMessage = "File too short for layer data";
        qWarning() << data.errorMessage;
        return data;
    }

    while (index < endIndex) {
        if (rawData[index].size() >= 2) {
            data.layerAssignments.append(rawData[index][0]);
            data.intermediateLayers.append(rawData[index][1]);
        } else if (rawData[index].size() >= 1) {
            data.layerAssignments.append(rawData[index][0]);
            data.intermediateLayers.append(0);
        }
        index++;
    }

    // Parse package positions
    // First line after layer data contains total package count
    int firstLayerLine = 4 + (data.numberOfLayers + 1);
    if (static_cast<int>(rawData.size()) <= firstLayerLine) {
        data.errorMessage = "File too short for position data";
        qWarning() << data.errorMessage;
        return data;
    }

    index = firstLayerLine;
    if (rawData[index].size() >= 1) {
        data.totalPackages = rawData[index][0];
    }

    // Get number of packages per layer type
    int indexPackageCount = index;
    for (int i = 0; i < data.layerTypeCount; i++) {
        if (static_cast<int>(rawData.size()) > indexPackageCount &&
            rawData[indexPackageCount].size() >= 1) {
            int packagesInLayerType = rawData[indexPackageCount][0];
            data.packagesPerLayerType.append(packagesInLayerType);
            indexPackageCount = indexPackageCount + packagesInLayerType + 1;
        }
    }

    // Parse individual package positions for each layer type
    for (int layerType = 0; layerType < data.layerTypeCount; layerType++) {
        index++;  // Skip count line

        int packagesInThisType = 0;
        if (layerType < data.packagesPerLayerType.size()) {
            packagesInThisType = data.packagesPerLayerType[layerType];
        }

        for (int pkg = 0; pkg < packagesInThisType; pkg++) {
            if (static_cast<int>(rawData.size()) <= index) {
                break;
            }

            const QVector<int>& posLine = rawData[index];
            if (posLine.size() >= 9) {
                PackagePosition pos;
                pos.xPick = posLine[LI_POSITION_XP];
                pos.yPick = posLine[LI_POSITION_YP];
                pos.anglePick = posLine[LI_POSITION_AP];
                pos.xDrop = posLine[LI_POSITION_XD];
                pos.yDrop = posLine[LI_POSITION_YD];
                pos.angleDrop = posLine[LI_POSITION_AD];
                pos.count = posLine[LI_POSITION_NOP];
                pos.xVector = posLine[LI_POSITION_XVEC];
                pos.yVector = posLine[LI_POSITION_YVEC];
                data.packagePositions.append(pos);
            }
            index++;
        }
    }

    qDebug() << "Parsed .rob file successfully:";
    qDebug() << "  Palette:" << data.paletteLength << "x" << data.paletteWidth << "x" << data.paletteHeight;
    qDebug() << "  Package:" << data.packageLength << "x" << data.packageWidth << "x" << data.packageHeight;
    qDebug() << "  Layers:" << data.numberOfLayers << "types:" << data.layerTypeCount;
    qDebug() << "  Positions:" << data.packagePositions.size();

    data.isValid = true;
    return data;
}

QStringList listRobFiles(const QString& directory)
{
    QDir dir(directory);
    if (!dir.exists()) {
        qWarning() << "Directory does not exist:" << directory;
        return QStringList();
    }

    QStringList filters;
    filters << "*.rob" << "*.ROB";

    QStringList files;
    for (const QFileInfo& info : dir.entryInfoList(filters, QDir::Files)) {
        files.append(info.absoluteFilePath());
    }

    qDebug() << "Found" << files.size() << ".rob files in" << directory;
    return files;
}

bool isValidRobFile(const QString& path)
{
    QFileInfo info(path);

    if (!info.exists()) {
        return false;
    }

    if (!info.isReadable()) {
        return false;
    }

    QString suffix = info.suffix().toLower();
    if (suffix != "rob") {
        return false;
    }

    // Basic content validation: check file size
    // .rob files should have some content
    if (info.size() < 100) {
        qWarning() << "File too small to be valid .rob file:" << path;
        return false;
    }

    // Check if file is not empty
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open file for validation:" << path;
        return false;
    }

    // Read first line and check for expected .rob file markers
    QTextStream in(&file);
    QString firstLine = in.readLine();
    file.close();

    // .rob files typically start with comments or data
    // Just verify it's not binary data
    if (firstLine.isEmpty() && info.size() > 0) {
        qWarning() << "File appears to be binary or invalid:" << path;
        return false;
    }

    return true;
}

QString baseName(const QString& path)
{
    QFileInfo info(path);
    return info.completeBaseName();
}

bool ensureDirectory(const QString& path)
{
    QDir dir(path);
    if (dir.exists()) {
        return true;
    }
    return dir.mkpath(".");
}

bool copyFile(const QString& source, const QString& destination, bool overwrite)
{
    if (!QFile::exists(source)) {
        qWarning() << "Source file does not exist:" << source;
        return false;
    }

    if (QFile::exists(destination)) {
        if (!overwrite) {
            qWarning() << "Destination file exists:" << destination;
            return false;
        }
        if (!QFile::remove(destination)) {
            qWarning() << "Cannot remove existing file:" << destination;
            return false;
        }
    }

    return QFile::copy(source, destination);
}

bool deleteFile(const QString& path)
{
    if (!QFile::exists(path)) {
        return true;  // Already doesn't exist
    }
    return QFile::remove(path);
}

qint64 fileSize(const QString& path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return -1;
    }
    return info.size();
}

QString modificationTime(const QString& path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return QString();
    }
    return info.lastModified().toString("yyyy-MM-dd hh:mm:ss");
}

bool isOnUsbDrive(const QString& path)
{
    QStorageInfo storage(path);
    if (!storage.isValid()) {
        return false;
    }

    // Check if it's a removable device
    // This is a heuristic - USB drives are typically removable
    QString rootPath = storage.rootPath();

#ifdef Q_OS_WIN
    const std::wstring rootPathW = rootPath.toStdWString();
    if (GetDriveTypeW(rootPathW.c_str()) == DRIVE_REMOVABLE) {
        return true;
    }
    return false;
#else
    // On Linux, check common USB mount points
    // Also verify it's actually a removable device
    if (!storage.isReady() || storage.isRoot()) {
        return false;
    }
    return rootPath.startsWith("/media/") ||
           rootPath.startsWith("/mnt/usb") ||
           rootPath.startsWith("/run/media/");
#endif
}

QStringList getUsbMountPoints()
{
    QStringList mountPoints;

    for (const QStorageInfo& storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) {
            continue;
        }

        QString rootPath = storage.rootPath();

#ifdef Q_OS_WIN
        const std::wstring rootPathW = rootPath.toStdWString();
        if (GetDriveTypeW(rootPathW.c_str()) == DRIVE_REMOVABLE) {
            mountPoints.append(rootPath);
        }
#else
        // On Linux, check common USB mount points
        // Filter out system mounts
        if (rootPath.startsWith("/media/") ||
            rootPath.startsWith("/mnt/usb") ||
            rootPath.startsWith("/run/media/")) {
            mountPoints.append(rootPath);
        }
#endif
    }

    return mountPoints;
}

} // namespace FileOperations

} // namespace system
} // namespace multipack
