/**
 * @file FileOperations.h
 * @brief File system utilities
 *
 * Provides utilities for file operations including
 * .rob file parsing.
 */

#ifndef MULTIPACK_SYSTEM_FILEOPERATIONS_H
#define MULTIPACK_SYSTEM_FILEOPERATIONS_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace multipack {
namespace system {

/**
 * @struct PackagePosition
 * @brief Position data for a single package pick/place operation
 */
struct PackagePosition {
    int xPick = 0;      ///< X pick position
    int yPick = 0;      ///< Y pick position
    int anglePick = 0;  ///< Pick angle
    int xDrop = 0;      ///< X drop position
    int yDrop = 0;      ///< Y drop position
    int angleDrop = 0;  ///< Drop angle
    int count = 0;      ///< Number of packages
    int xVector = 0;    ///< X vector
    int yVector = 0;    ///< Y vector
};

/**
 * @struct RobFileData
 * @brief Parsed data from a .rob file
 *
 * File format is tab-separated values:
 * - Line 0: Pallet dimensions (length, width, height)
 * - Line 1: Package dimensions (length, width, height, gap)
 * - Line 2: Number of layer types
 * - Line 3: Number of layers
 * - Line 4: Empty/separator
 * - Lines 5 to 4+numLayers: Layer assignments and intermediate layer flags
 * - Remaining lines: Package positions for each layer type
 */
struct RobFileData {
    QString fileName;                         ///< Original file name
    QString filePath;                         ///< Full file path
    qint64 fileTimestamp = 0;                 ///< File modification timestamp

    // Pallet dimensions [length, width, height] in mm
    int paletteLength = 0;
    int paletteWidth = 0;
    int paletteHeight = 0;

    // Package dimensions [length, width, height, gap] in mm
    int packageLength = 0;
    int packageWidth = 0;
    int packageHeight = 0;
    int packageGap = 0;
    double packageWeight = 0.0;               ///< Package weight in kg (from database)
    bool einzelpaketLaengs = false;           ///< Single package lengthwise

    // Layer information
    int layerTypeCount = 0;                   ///< Number of different layer types
    int numberOfLayers = 0;                   ///< Total number of layers
    QVector<int> layerAssignments;            ///< Which layer type for each layer
    QVector<int> intermediateLayers;          ///< Intermediate layer flags (0 or 1)
    QVector<int> packagesPerLayerType;        ///< Number of packages per layer type

    // Package positions for all layer types
    QVector<PackagePosition> packagePositions;

    // Raw data from file (for compatibility)
    QVector<QVector<int>> rawData;

    // Metadata
    int paketQuer = 1;                        ///< Package orientation
    QVector<double> centerOfGravity;          ///< Center of gravity [x, y, z]
    int totalPackages = 0;                    ///< Total number of packages

    bool isValid = false;                     ///< Whether parsing was successful
    QString errorMessage;                     ///< Error message if parsing failed
};

/**
 * @namespace FileOperations
 * @brief File operation utilities
 */
namespace FileOperations {

/**
 * @brief Parse a .rob file
 * @param path Path to .rob file
 * @return Parsed file data
 */
RobFileData parseRobFile(const QString& path);

/**
 * @brief List .rob files in directory
 * @param directory Directory to search
 * @return List of .rob file paths
 */
QStringList listRobFiles(const QString& directory);

/**
 * @brief Check if file is a valid .rob file
 * @param path Path to check
 * @return true if valid .rob file
 */
bool isValidRobFile(const QString& path);

/**
 * @brief Get file name without extension
 * @param path Full file path
 * @return File name without extension
 */
QString baseName(const QString& path);

/**
 * @brief Ensure directory exists
 * @param path Directory path
 * @return true if exists or was created
 */
bool ensureDirectory(const QString& path);

/**
 * @brief Copy file safely
 * @param source Source path
 * @param destination Destination path
 * @param overwrite Whether to overwrite existing
 * @return true on success
 */
bool copyFile(const QString& source, const QString& destination,
              bool overwrite = false);

/**
 * @brief Delete file safely
 * @param path File path
 * @return true on success
 */
bool deleteFile(const QString& path);

/**
 * @brief Get file size
 * @param path File path
 * @return File size in bytes, -1 on error
 */
qint64 fileSize(const QString& path);

/**
 * @brief Get file modification time
 * @param path File path
 * @return Modification time string
 */
QString modificationTime(const QString& path);

/**
 * @brief Check if path is on USB drive
 * @param path Path to check
 * @return true if on USB
 */
bool isOnUsbDrive(const QString& path);

/**
 * @brief Get USB mount points
 * @return List of USB mount paths
 */
QStringList getUsbMountPoints();

} // namespace FileOperations

} // namespace system
} // namespace multipack

#endif // MULTIPACK_SYSTEM_FILEOPERATIONS_H
