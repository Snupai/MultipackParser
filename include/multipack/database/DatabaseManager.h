/**
 * @file DatabaseManager.h
 * @brief SQLite database manager for palette data storage
 */
#ifndef MULTIPACK_DATABASE_DATABASEMANAGER_H
#define MULTIPACK_DATABASE_DATABASEMANAGER_H

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVector>
#include <QDateTime>
#include <optional>

namespace multipack {
namespace database {

/**
 * @struct PaletteMetadata
 * @brief Metadata about a stored palette configuration
 */
struct PaletteMetadata {
    int id = 0;
    QString fileName;
    qint64 fileTimestamp = 0;
    int paketQuer = 1;
    QVector<double> centerOfGravity = {0, 0, 0};
    int lageArten = 0;       // Number of layer types
    int anzLagen = 0;        // Number of layers
    int anzahlPakete = 0;    // Number of packages
};

/**
 * @struct PaletteDimensions
 * @brief Pallet dimensions
 */
struct PaletteDimensions {
    int length = 0;
    int width = 0;
    int height = 0;
};

/**
 * @struct PackageDimensions
 * @brief Package dimensions with gap
 */
struct PackageDimensions {
    int length = 0;
    int width = 0;
    int height = 0;
    int gap = 0;
    double weight = 0.0;
    bool einzelpaketLaengs = false;
};

/**
 * @struct PackagePosition
 * @brief Position data for a single package
 */
struct PackagePosition {
    int xp = 0;   // X pick position
    int yp = 0;   // Y pick position
    int ap = 0;   // Pick angle
    int xd = 0;   // X drop position
    int yd = 0;   // Y drop position
    int ad = 0;   // Drop angle
    int nop = 0;  // Number of packages
    int xvec = 0; // X vector
    int yvec = 0; // Y vector
};

/**
 * @struct PaletteData
 * @brief Complete palette configuration data
 */
struct PaletteData {
    PaletteMetadata metadata;
    PaletteDimensions paletteDimensions;
    PackageDimensions packageDimensions;
    QVector<QVector<int>> rawData;           // g_Daten
    QVector<int> layerAssignments;           // g_LageZuordnung
    QVector<int> intermediaryLayers;         // g_Zwischenlagen
    QVector<int> packagesPerLayerType;       // g_PaketeZuordnung
    QVector<PackagePosition> packagePositions; // g_PaketPos
};

/**
 * @struct FileInfo
 * @brief Information about a stored file
 */
struct FileInfo {
    int id = 0;
    QString fileName;
    qint64 timestamp = 0;
    QString timestampStr;
};

/**
 * @class DatabaseManager
 * @brief Manages SQLite database for palette storage
 */
class DatabaseManager : public QObject {
    Q_OBJECT

public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager() override;

    /**
     * @brief Open database at specified path
     * @param path Database file path
     * @return true on success
     */
    bool open(const QString& path = "paletten.db");

    /**
     * @brief Close the database
     */
    void close();

    /**
     * @brief Check if database is open
     */
    bool isOpen() const;

    /**
     * @brief Create database tables if they don't exist
     * @return true on success
     */
    bool createTables();

    /**
     * @brief Save palette data to database
     * @param data Palette data to save
     * @return true on success
     */
    bool savePaletteData(const PaletteData& data);

    /**
     * @brief Load palette data from database
     * @param fileName File name to load (empty = most recent)
     * @param metadataId Specific ID to load (takes precedence)
     * @return Optional containing data if found
     */
    std::optional<PaletteData> loadPaletteData(const QString& fileName = QString(), int metadataId = -1);

    /**
     * @brief List all available palette files
     * @return List of file info structures
     */
    QVector<FileInfo> listAvailableFiles();

    /**
     * @brief Find a file by name
     * @param fileName File name to search
     * @return Optional containing info if found
     */
    std::optional<FileInfo> findFile(const QString& fileName);

    /**
     * @brief Find palettes matching package dimensions
     * @param length Package length (0 = any)
     * @param width Package width (0 = any)
     * @param height Package height (0 = any)
     * @return List of matching file names
     */
    QStringList findByPackageDimensions(int length = 0, int width = 0, int height = 0);

    /**
     * @brief Update box dimensions for a file
     * @param fileName File name to update
     * @param height New height (-1 = unchanged)
     * @param weight New weight (-1 = unchanged)
     * @param einzelpaketLaengs Single package lengthwise setting (-1 = unchanged)
     * @return true on success
     */
    bool updateBoxDimensions(const QString& fileName, int height = -1, double weight = -1.0, int einzelpaketLaengs = -1);

    /**
     * @brief Get box weight for a file
     */
    std::optional<double> getBoxWeight(const QString& fileName);

    /**
     * @brief Get box height for a file
     */
    std::optional<int> getBoxHeight(const QString& fileName);

    /**
     * @brief Get einzelpaket laengs setting for a file
     */
    std::optional<bool> getEinzelpaketLaengs(const QString& fileName);

    /**
     * @brief Delete a palette entry by file name
     */
    bool deletePalette(const QString& fileName);

signals:
    void databaseOpened();
    void databaseClosed();
    void dataChanged();

private:
    /**
     * @brief Get metadata ID for a file
     */
    int getMetadataId(const QString& fileName);

    /**
     * @brief Run migrations for existing databases
     */
    void runMigrations();

    QSqlDatabase m_db;
    QString m_connectionName;
    bool m_open = false;
};

} // namespace database
} // namespace multipack

#endif // MULTIPACK_DATABASE_DATABASEMANAGER_H
