/**
 * @file VisualizationWidget.h
 * @brief 3D pallet visualization widget using QPainter
 *
 * Renders pallets with stacked packages using isometric projection.
 * Matches the Python Matplotlib 3D visualization style.
 */

#ifndef MULTIPACK_UI_VISUALIZATIONWIDGET_H
#define MULTIPACK_UI_VISUALIZATIONWIDGET_H

#include <QWidget>
#include <QColor>
#include <QPointF>
#include <QVector>
#include <QString>
#include <memory>

// Forward declarations
namespace multipack {
namespace system {
struct RobFileData;
}
}

namespace multipack {
namespace ui {

/**
 * @brief Package position data for visualization
 */
struct VisualPackage {
    double x = 0;           ///< X position (mm)
    double y = 0;           ///< Y position (mm)
    double width = 0;       ///< Width (mm)
    double length = 0;      ///< Length (mm)
    double height = 0;      ///< Height (mm)
    int rotation = 0;       ///< Rotation (0, 90, 180, 270)
    int layer = 0;          ///< Layer number
};

/**
 * @brief Palette data for visualization
 */
struct VisualPalette {
    QString name;
    double width = 0;       ///< Pallet width (mm)
    double length = 0;      ///< Pallet length (mm)
    double height = 0;      ///< Pallet height (mm)
    int packageLength = 0;  ///< Package length (mm)
    int packageWidth = 0;   ///< Package width (mm)
    int packageHeight = 0;  ///< Package height (mm)
    int totalPackages = 0;
    int totalLayers = 0;
    int conveyorDirection = 0; ///< 0 = Längs, 1 = Quer
    QVector<VisualPackage> packages;
};

/**
 * @class VisualizationWidget
 * @brief 3D pallet visualization using isometric projection
 *
 * Renders a 3D view of the pallet with stacked packages using
 * QPainter. The view is fixed (no rotation) for clarity.
 *
 * Color scheme matches Python implementation:
 * - Green: Top/bottom faces
 * - White: Label face (front, rotated)
 * - Red: Back face
 * - Blue: Side faces
 */
class VisualizationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VisualizationWidget(QWidget* parent = nullptr);
    ~VisualizationWidget() override;

    /**
     * @brief Set the palette data to visualize
     * @param palette Palette data
     */
    void setVisualizationData(const VisualPalette& palette);

    /**
     * @brief Load palette from GlobalState
     */
    void loadFromGlobalState();

    /**
     * @brief Load palette from parsed ROB file data
     * @param robData Parsed ROB file data
     */
    void loadFromRobFileData(const system::RobFileData& robData);

    /**
     * @brief Create VisualPalette from RobFileData
     * @param robData Parsed ROB file data
     * @return Visual palette for rendering
     */
    static VisualPalette createFromRobData(const system::RobFileData& robData);

    /**
     * @brief Update the visualization
     */
    void updateVisualization();

    /**
     * @brief Set which layer to highlight (0 = all)
     * @param layer Layer number
     */
    void setLayer(int layer);

    /**
     * @brief Get current highlighted layer
     * @return Layer number
     */
    int currentLayer() const;

    /**
     * @brief Set view elevation angle (degrees)
     * @param angle Elevation angle (10-80)
     */
    void setElevation(double angle);

    /**
     * @brief Set view azimuth angle (degrees)
     * @param angle Azimuth angle (0-360)
     */
    void setAzimuth(double angle);

    /**
     * @brief Set zoom level
     * @param zoom Zoom factor (0.5 - 2.0)
     */
    void setZoom(double zoom);

    /**
     * @brief Reset view to defaults
     */
    void resetView();

signals:
    /**
     * @brief Emitted when rendering completes
     * @param packageCount Number of packages rendered
     */
    void renderComplete(int packageCount);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    /**
     * @brief Project 3D point to 2D screen coordinates
     * @param x 3D X coordinate
     * @param y 3D Y coordinate
     * @param z 3D Z coordinate
     * @return 2D screen point
     */
    QPointF project3D(double x, double y, double z) const;

    /**
     * @brief Draw a 3D box (package)
     * @param painter QPainter to use
     * @param pkg Package data
     */
    void drawBox(QPainter& painter, const VisualPackage& pkg);

    /**
     * @brief Draw the pallet base
     * @param painter QPainter to use
     */
    void drawPalletBase(QPainter& painter);

    /**
     * @brief Draw coordinate axes
     * @param painter QPainter to use
     */
    void drawAxes(QPainter& painter);

    /**
     * @brief Draw legend/info box
     * @param painter QPainter to use
     */
    void drawLegend(QPainter& painter);

    /**
     * @brief Get face color based on rotation and face type
     * @param rotation Package rotation
     * @param face Face index (0=bottom, 1=top, 2=front, 3=back, 4=left, 5=right)
     * @return Face color
     */
    QColor getFaceColor(int rotation, int face) const;

    /**
     * @brief Sort packages by depth for proper rendering
     */
    void sortPackagesByDepth();

    // View parameters
    double m_elevation = 30.0;    ///< View elevation (degrees)
    double m_azimuth = 40.0;      ///< View azimuth (degrees)
    double m_zoom = 1.0;          ///< Zoom factor
    double m_scale = 1.0;         ///< Calculated scale factor
    QPointF m_offset;             ///< Pan offset

    // Data
    VisualPalette m_palette;
    int m_currentLayer = 0;

    // Cached trig values
    double m_sinElev = 0;
    double m_cosElev = 0;
    double m_sinAzim = 0;
    double m_cosAzim = 0;

    // Colors
    static const QColor COLOR_GREEN;
    static const QColor COLOR_WHITE;
    static const QColor COLOR_RED;
    static const QColor COLOR_BLUE;
    static const QColor COLOR_PALLET;
    static const QColor COLOR_EDGE;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_VISUALIZATIONWIDGET_H
