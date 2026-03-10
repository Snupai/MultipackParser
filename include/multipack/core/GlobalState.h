/**
 * @file GlobalState.h
 * @brief Centralized application state management
 */

#ifndef MULTIPACK_CORE_GLOBALSTATE_H
#define MULTIPACK_CORE_GLOBALSTATE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QVariant>
#include <QMutex>

// Forward declaration
namespace multipack {
namespace database {
struct PaletteData;
}
namespace system {
    struct RobFileData;
    struct PackagePosition;
}
}

namespace multipack {
namespace core {

/**
 * @namespace DataIndices
 * @brief Index constants for data arrays (matching Python LI_* constants)
 */
namespace DataIndices {
    // Line indices in raw data
    constexpr int PaletteData = 0;
    constexpr int PackageData = 1;
    constexpr int LayerTypes = 2;
    constexpr int NumberOfLayers = 3;

    // Palette dimension indices
    constexpr int PaletteLength = 0;
    constexpr int PaletteWidth = 1;
    constexpr int PaletteHeight = 2;

    // Package dimension indices
    constexpr int PackageLength = 0;
    constexpr int PackageWidth = 1;
    constexpr int PackageHeight = 2;
    constexpr int PackageGap = 3;

    // Position array indices
    constexpr int PositionXPick = 0;
    constexpr int PositionYPick = 1;
    constexpr int PositionAnglePick = 2;
    constexpr int PositionXDrop = 3;
    constexpr int PositionYDrop = 4;
    constexpr int PositionAngleDrop = 5;
    constexpr int PositionCount = 6;
    constexpr int PositionXVector = 7;
    constexpr int PositionYVector = 8;
}

/**
 * @class GlobalState
 * @brief Centralized application state singleton
 *
 * Holds all palette data, robot status, and application state.
 * Thread-safe access to shared state data.
 */
class GlobalState : public QObject
{
    Q_OBJECT

public:
    static GlobalState& instance();

    // =========================================================================
    // Palette Data
    // =========================================================================

    /** @brief Get palette dimensions [length, width, height] in mm */
    QVector<int> paletteDimensions() const;
    void setPaletteDimensions(const QVector<int>& dims);
    void setPaletteDimensions(int length, int width, int height);

    int paletteLength() const;
    int paletteWidth() const;
    int paletteHeight() const;

    // =========================================================================
    // Package Data
    // =========================================================================

    /** @brief Get package dimensions [length, width, height, gap] in mm */
    QVector<int> packageDimensions() const;
    void setPackageDimensions(const QVector<int>& dims);
    void setPackageDimensions(int length, int width, int height, int gap);

    int packageLength() const;
    int packageWidth() const;
    int packageHeight() const;
    int packageGap() const;

    /** @brief Package weight in kg */
    double packageWeight() const;
    void setPackageWeight(double weight);

    /** @brief Single package lengthwise flag */
    bool einzelpaketLaengs() const;
    void setEinzelpaketLaengs(bool value);

    // =========================================================================
    // Layer Data
    // =========================================================================

    /** @brief Number of different layer types */
    int layerTypeCount() const;
    void setLayerTypeCount(int count);

    /** @brief Total number of layers */
    int numberOfLayers() const;
    void setNumberOfLayers(int count);

    /** @brief Layer type assignments (which type for each layer) */
    QVector<int> layerAssignments() const;
    void setLayerAssignments(const QVector<int>& assignments);

    /** @brief Intermediate layer flags (0 or 1 for each layer) */
    QVector<int> intermediateLayers() const;
    void setIntermediateLayers(const QVector<int>& layers);

    /** @brief Packages per layer type */
    QVector<int> packagesPerLayerType() const;
    void setPackagesPerLayerType(const QVector<int>& counts);

    // Deprecated compatibility names
    QVector<int> layerTypes() const { return layerAssignments(); }
    void setLayerTypes(const QVector<int>& types) { setLayerAssignments(types); }

    // =========================================================================
    // Package Positions
    // =========================================================================

    /** @brief Get all package positions */
    QVector<QVector<int>> packagePositions() const;
    void setPackagePositions(const QVector<QVector<int>>& positions);

    /** @brief Get position for specific package */
    QVector<int> packagePosition(int index) const;

    /** @brief Total number of packages */
    int totalPackages() const;
    void setTotalPackages(int count);

    // =========================================================================
    // Raw Data
    // =========================================================================

    /** @brief Raw data from .rob file (for compatibility) */
    QVector<QVector<int>> rawData() const;
    void setRawData(const QVector<QVector<int>>& data);

    // =========================================================================
    // Metadata
    // =========================================================================

    /** @brief Package orientation (1 = default) */
    bool paketQuer() const;
    void setPaketQuer(bool value);

    /** @brief Center of gravity [x, y, z] */
    QVector<double> centerOfGravity() const;
    void setCenterOfGravity(const QVector<double>& cog);

    // =========================================================================
    // Current File
    // =========================================================================

    QString currentFileName() const;
    void setCurrentFileName(const QString& name);

    QString currentFilePath() const;
    void setCurrentFilePath(const QString& path);

    qint64 currentFileTimestamp() const;
    void setCurrentFileTimestamp(qint64 timestamp);

    // =========================================================================
    // Robot State
    // =========================================================================

    bool robotConnected() const;
    void setRobotConnected(bool connected);

    QString robotIp() const;
    void setRobotIp(const QString& ip);

    // =========================================================================
    // UR20 Specific State
    // =========================================================================

    int ur20ActivePalette() const;
    void setUr20ActivePalette(int palette);

    bool ur20Palette1Empty() const;
    void setUr20Palette1Empty(bool empty);

    bool ur20Palette2Empty() const;
    void setUr20Palette2Empty(bool empty);

    bool ur20Zwischenlage() const;
    void setUr20Zwischenlage(bool value);

    qint64 palette1NonEmptyTimestamp() const;
    void setPalette1NonEmptyTimestamp(qint64 timestamp);

    qint64 palette2NonEmptyTimestamp() const;
    void setPalette2NonEmptyTimestamp(qint64 timestamp);

    // =========================================================================
    // Scanner State (UR10/UR20)
    // =========================================================================

    /** @brief Get scanner status string (e.g. "True,True,True") */
    QString scannerStatus() const;
    void setScannerStatus(const QString& status);

    /** @brief Get previous scanner status (for change detection) */
    QString previousScannerStatus() const;

    /** @brief Timestamp when scanner entered safe state */
    qint64 timestampScannerSafe() const;
    void setTimestampScannerSafe(qint64 timestamp);

    /** @brief Timestamp of last scanner fault */
    qint64 timestampScannerFault() const;
    void setTimestampScannerFault(qint64 timestamp);

    /** @brief Last time scanner warning was played */
    qint64 lastScannerWarningTime() const;
    void setLastScannerWarningTime(qint64 timestamp);

    // =========================================================================
    // UR10 Scanner Values (from UI spin boxes)
    // =========================================================================

    int scanner1and2NioValue() const;
    void setScanner1and2NioValue(int value);

    int scanner1Value() const;
    void setScanner1Value(int value);

    int scanner2Value() const;
    void setScanner2Value(int value);

    int scanner1and2IoValue() const;
    void setScanner1and2IoValue(int value);

    // =========================================================================
    // UI State (for RPC queries)
    // =========================================================================

    bool klemmungAktiv() const;
    void setKlemmungAktiv(bool active);

    QVector<bool> scannerOverride() const;
    void setScannerOverride(const QVector<bool>& override);
    void setScannerOverride(int index, bool value);

    /** @brief Label invert checkbox state (adds 180 to rotation) */
    bool labelInvert() const;
    void setLabelInvert(bool invert);

    // =========================================================================
    // Current Layer/Position Tracking
    // =========================================================================

    int currentLayer() const;
    void setCurrentLayer(int layer);

    int startLayer() const;
    void setStartLayer(int layer);

    // =========================================================================
    // Additional Data Variables (from Python global_vars)
    // =========================================================================

    /** @brief Start layer positions array */
    QVector<int> startlage() const;
    void setStartlage(const QVector<int>& startlage);



    /** @brief Package mass in kg */
    double massePaket() const;
    void setMassePaket(double masse);

    /** @brief Pick offset coordinates [x, y] */
    double pickOffsetX() const;
    void setPickOffsetX(double offset);

    double pickOffsetY() const;
    void setPickOffsetY(double offset);

    // =========================================================================
    // Filter Variables (for palette list)
    // =========================================================================

    /** @brief Filter dimensions for palette list */
    int filterLength() const;
    void setFilterLength(int length);

    int filterWidth() const;
    void setFilterWidth(int width);

    int filterHeight() const;
    void setFilterHeight(int height);

    // =========================================================================
    // Audio State
    // =========================================================================

    bool audioMuted() const;
    void setAudioMuted(bool muted);

    // =========================================================================
    // Data Loading
    // =========================================================================

    /** @brief Load data from parsed RobFileData */
    void loadFromRobFileData(const system::RobFileData& data);

    /** @brief Load data from database palette structure */
    void applyPaletteData(const database::PaletteData& data);

    /** @brief Clear all state to defaults */
    void clear();

    /** @brief Check if palette data is loaded */
    bool hasLoadedData() const;

signals:
    void paletteDataChanged();
    void packageDataChanged();
    void layerDataChanged();
    void positionsChanged();
    void fileChanged(const QString& fileName);
    void robotStatusChanged(bool connected);
    void audioMutedChanged(bool muted);
    void ur20StateChanged();
    void currentLayerChanged(int layer);
    void scannerStatusChanged(const QString& status, const QString& imagePath);
    void filterDimensionsChanged();
    void additionalDataChanged();

private:
    GlobalState();
    ~GlobalState() override;
    GlobalState(const GlobalState&) = delete;
    GlobalState& operator=(const GlobalState&) = delete;

    mutable QMutex m_mutex;

    // Palette dimensions [length, width, height]
    int m_paletteLength = 0;
    int m_paletteWidth = 0;
    int m_paletteHeight = 0;

    // Package dimensions [length, width, height, gap]
    int m_packageLength = 0;
    int m_packageWidth = 0;
    int m_packageHeight = 0;
    int m_packageGap = 0;
    double m_packageWeight = 0.0;
    bool m_einzelpaketLaengs = false;

    // Layer data
    int m_layerTypeCount = 0;
    int m_numberOfLayers = 0;
    QVector<int> m_layerAssignments;
    QVector<int> m_intermediateLayers;
    QVector<int> m_packagesPerLayerType;

    // Package positions
    QVector<QVector<int>> m_packagePositions;
    int m_totalPackages = 0;

    // Raw data from file
    QVector<QVector<int>> m_rawData;

    // Metadata
    int m_paketQuer = 1;
    QVector<double> m_centerOfGravity;
    
    // Additional data variables
    QVector<int> m_startlage;
    double m_massePaket = 0.0;
    double m_pickOffsetX = 0.0;
    double m_pickOffsetY = 0.0;
    
    // Filter variables for palette list
    int m_filterLength = 0;
    int m_filterWidth = 0;
    int m_filterHeight = 0;

    // Current file
    QString m_currentFileName;
    QString m_currentFilePath;
    qint64 m_currentFileTimestamp = 0;

    // Robot state
    bool m_robotConnected = false;
    QString m_robotIp = "192.168.0.1";

    // UR20 state
    int m_ur20ActivePalette = 0;
    bool m_ur20Palette1Empty = true;
    bool m_ur20Palette2Empty = true;
    bool m_ur20Zwischenlage = false;
    qint64 m_palette1NonEmptyTimestamp = 0;
    qint64 m_palette2NonEmptyTimestamp = 0;

    // Scanner state
    QString m_scannerStatus = "True,True,True";
    QString m_previousScannerStatus = "True,True,True";
    qint64 m_timestampScannerSafe = 0;
    qint64 m_timestampScannerFault = 0;
    qint64 m_lastScannerWarningTime = 0;

    // UR10 scanner values (from UI)
    int m_scanner1and2NioValue = 0;
    int m_scanner1Value = 0;
    int m_scanner2Value = 0;
    int m_scanner1and2IoValue = 0;

    // UI state
    bool m_klemmungAktiv = false;
    QVector<bool> m_scannerOverride = {false, false, false};
    bool m_labelInvert = false;

    // Current layer tracking
    int m_currentLayer = 1;
    int m_startLayer = 1;

    // Audio
    bool m_audioMuted = false;
};

} // namespace core
} // namespace multipack

#endif // MULTIPACK_CORE_GLOBALSTATE_H
