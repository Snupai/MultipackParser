/**
 * @file PaletData.h
 * @brief Data models for palette information
 *
 * Defines structures and classes for representing
 * palette and package data in memory.
 */

#ifndef MULTIPACK_DATABASE_PALETDATA_H
#define MULTIPACK_DATABASE_PALETDATA_H

#include <QString>
#include <QVector>
#include <QJsonObject>

namespace multipack {
namespace database {

/**
 * @brief Layer type enumeration
 */
enum class LayerType {
    Standard = 0,    ///< Normal layer
    Interlayer = 1,  ///< Interlayer/separator
    Custom = 2       ///< Custom layer type
};

/**
 * @brief Package position within a layer
 */
struct Position {
    double x = 0.0;          ///< X coordinate (mm)
    double y = 0.0;          ///< Y coordinate (mm)
    double z = 0.0;          ///< Z coordinate (mm)
    double rotation = 0.0;   ///< Rotation angle (degrees)

    /**
     * @brief Convert to JSON
     * @return JSON object
     */
    QJsonObject toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return Position
     */
    static Position fromJson(const QJsonObject& json);
};

/**
 * @brief Package placement data
 */
struct PackagePlacement {
    int index = 0;              ///< Package index within layer
    Position pickPosition;      ///< Pick position
    Position dropPosition;      ///< Drop position
    int packageCount = 1;       ///< Number of packages (for multi-pick)
    double xVector = 0.0;       ///< X direction vector
    double yVector = 0.0;       ///< Y direction vector

    /**
     * @brief Convert to JSON
     * @return JSON object
     */
    QJsonObject toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return PackagePlacement
     */
    static PackagePlacement fromJson(const QJsonObject& json);
};

/**
 * @brief Layer data
 */
struct Layer {
    int layerNumber = 0;                      ///< Layer index (0-based)
    LayerType type = LayerType::Standard;     ///< Layer type
    QVector<PackagePlacement> placements;     ///< Package placements

    /**
     * @brief Get number of packages in layer
     * @return Package count
     */
    int packageCount() const;

    /**
     * @brief Convert to JSON
     * @return JSON object
     */
    QJsonObject toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return Layer
     */
    static Layer fromJson(const QJsonObject& json);
};

/**
 * @class PaletData
 * @brief Complete palette configuration data
 *
 * Contains all information about a palette configuration
 * including dimensions, packages, and layer layouts.
 */
class PaletData
{
public:
    /**
     * @brief Construct empty palet data
     */
    PaletData() = default;

    /**
     * @brief Construct with file name
     * @param fileName Source file name
     */
    explicit PaletData(const QString& fileName);

    // File info
    QString fileName() const { return m_fileName; }
    void setFileName(const QString& name) { m_fileName = name; }

    // Palette dimensions
    double paletteLength() const { return m_paletteLength; }
    double paletteWidth() const { return m_paletteWidth; }
    double paletteHeight() const { return m_paletteHeight; }
    double paletteOverhang() const { return m_paletteOverhang; }

    void setPaletteDimensions(double length, double width, double height,
                             double overhang = 0.0);

    // Package dimensions
    double packageLength() const { return m_packageLength; }
    double packageWidth() const { return m_packageWidth; }
    double packageHeight() const { return m_packageHeight; }
    double packageWeight() const { return m_packageWeight; }

    void setPackageDimensions(double length, double width, double height,
                             double weight = 0.0);

    // Layers
    int layerCount() const { return m_layers.size(); }
    const QVector<Layer>& layers() const { return m_layers; }
    void setLayers(const QVector<Layer>& layers) { m_layers = layers; }
    void addLayer(const Layer& layer) { m_layers.append(layer); }
    void clearLayers() { m_layers.clear(); }

    /**
     * @brief Get total package count across all layers
     * @return Total packages
     */
    int totalPackageCount() const;

    /**
     * @brief Validate the data
     * @return true if valid
     */
    bool isValid() const;

    /**
     * @brief Convert to JSON
     * @return JSON object
     */
    QJsonObject toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return PaletData instance
     */
    static PaletData fromJson(const QJsonObject& json);

    /**
     * @brief Clear all data
     */
    void clear();

private:
    QString m_fileName;

    // Palette dimensions (mm)
    double m_paletteLength = 0.0;
    double m_paletteWidth = 0.0;
    double m_paletteHeight = 0.0;
    double m_paletteOverhang = 0.0;

    // Package dimensions
    double m_packageLength = 0.0;
    double m_packageWidth = 0.0;
    double m_packageHeight = 0.0;
    double m_packageWeight = 0.0;  // kg

    // Layers
    QVector<Layer> m_layers;
};

} // namespace database
} // namespace multipack

#endif // MULTIPACK_DATABASE_PALETDATA_H
