/**
 * @file VisualizationWidget.cpp
 * @brief 3D pallet visualization implementation
 */

#include "multipack/ui/VisualizationWidget.h"
#include "multipack/core/GlobalState.h"
#include "multipack/system/FileOperations.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QDebug>
#include <cmath>
#include <algorithm>

namespace multipack {
namespace ui {

// Color constants (matching Python implementation)
const QColor VisualizationWidget::COLOR_GREEN(76, 175, 80);      // Top/bottom
const QColor VisualizationWidget::COLOR_WHITE(255, 255, 255);    // Label face
const QColor VisualizationWidget::COLOR_RED(244, 67, 54);        // Back face
const QColor VisualizationWidget::COLOR_BLUE(33, 150, 243);      // Side faces
const QColor VisualizationWidget::COLOR_PALLET(139, 90, 43);     // Pallet base
const QColor VisualizationWidget::COLOR_EDGE(0, 0, 0);           // Edge color

// Helper: degrees to radians
static constexpr double DEG2RAD = M_PI / 180.0;

VisualizationWidget::VisualizationWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(400, 300);
    setAutoFillBackground(true);

    // Set background color
    QPalette pal = QWidget::palette();
    pal.setColor(QPalette::Window, QColor(245, 245, 245));
    QWidget::setPalette(pal);

    // Initialize view
    resetView();
}

VisualizationWidget::~VisualizationWidget() = default;

void VisualizationWidget::setVisualizationData(const VisualPalette& palette)
{
    m_palette = palette;
    sortPackagesByDepth();
    update();
}

void VisualizationWidget::loadFromGlobalState()
{
    auto& state = core::GlobalState::instance();

    VisualPalette visData;
    visData.name = state.currentFileName();
    visData.length = state.paletteLength();
    visData.width = state.paletteWidth();
    visData.height = state.paletteHeight();
    visData.packageLength = state.packageLength();
    visData.packageWidth = state.packageWidth();
    visData.packageHeight = state.packageHeight();
    visData.totalLayers = state.numberOfLayers();

    // Get package positions from state
    auto positions = state.packagePositions();
    int layer = 0;

    for (const auto& pos : positions) {
        VisualPackage pkg;
        // Position data: XD, YD are drop positions (on pallet)
        pkg.x = pos.value(3, 0);  // LI_POSITION_XD
        pkg.y = pos.value(4, 0);  // LI_POSITION_YD
        pkg.width = visData.packageWidth;
        pkg.length = visData.packageLength;
        pkg.height = visData.packageHeight;
        pkg.rotation = static_cast<int>(pos.value(5, 0));  // LI_POSITION_AD (angle)
        pkg.layer = layer;

        visData.packages.append(pkg);
        visData.totalPackages++;
    }

    setVisualizationData(visData);
}

void VisualizationWidget::loadFromRobFileData(const system::RobFileData& robData)
{
    VisualPalette palette = createFromRobData(robData);
    setVisualizationData(palette);
}

VisualPalette VisualizationWidget::createFromRobData(const system::RobFileData& robData)
{
    VisualPalette palette;
    palette.name = robData.fileName;
    palette.length = robData.paletteLength;
    palette.width = robData.paletteWidth;
    palette.height = robData.paletteHeight;
    palette.packageLength = robData.packageLength;
    palette.packageWidth = robData.packageWidth;
    palette.packageHeight = robData.packageHeight;
    palette.totalLayers = robData.numberOfLayers;
    palette.conveyorDirection = robData.paketQuer;

    // Build package list from layer assignments and positions
    // Layer assignments tells us which layer type each layer uses
    // Package positions are grouped by layer type

    // For each layer from bottom to top
    for (int layerNum = 0; layerNum < robData.numberOfLayers; layerNum++) {
        // Get which layer type this layer uses (0-indexed)
        int layerType = 0;
        if (layerNum < robData.layerAssignments.size()) {
            layerType = robData.layerAssignments[layerNum];
        }

        // Find positions for this layer type
        // Positions are stored sequentially for each layer type
        int typeStartIndex = 0;
        for (int t = 0; t < layerType; t++) {
            if (t < robData.packagesPerLayerType.size()) {
                typeStartIndex += robData.packagesPerLayerType[t];
            }
        }

        int packagesInType = 1;
        if (layerType < robData.packagesPerLayerType.size()) {
            packagesInType = robData.packagesPerLayerType[layerType];
        }

        // Add packages for this layer
        for (int pkgIdx = 0; pkgIdx < packagesInType; pkgIdx++) {
            int posIdx = typeStartIndex + pkgIdx;
            if (posIdx < robData.packagePositions.size()) {
                const auto& pos = robData.packagePositions[posIdx];

                VisualPackage pkg;
                pkg.x = pos.xDrop;
                pkg.y = pos.yDrop;
                pkg.width = robData.packageWidth;
                pkg.length = robData.packageLength;
                pkg.height = robData.packageHeight;
                pkg.rotation = pos.angleDrop;
                pkg.layer = layerNum;

                palette.packages.append(pkg);
                palette.totalPackages++;
            }
        }
    }

    return palette;
}

void VisualizationWidget::updateVisualization()
{
    update();
}

void VisualizationWidget::setLayer(int layer)
{
    m_currentLayer = layer;
    update();
}

int VisualizationWidget::currentLayer() const
{
    return m_currentLayer;
}

void VisualizationWidget::setElevation(double angle)
{
    m_elevation = qBound(10.0, angle, 80.0);
    m_sinElev = std::sin(m_elevation * DEG2RAD);
    m_cosElev = std::cos(m_elevation * DEG2RAD);
    update();
}

void VisualizationWidget::setAzimuth(double angle)
{
    m_azimuth = std::fmod(angle, 360.0);
    if (m_azimuth < 0) m_azimuth += 360.0;
    m_sinAzim = std::sin(m_azimuth * DEG2RAD);
    m_cosAzim = std::cos(m_azimuth * DEG2RAD);
    update();
}

void VisualizationWidget::setZoom(double zoom)
{
    m_zoom = qBound(0.5, zoom, 2.0);
    update();
}

void VisualizationWidget::resetView()
{
    m_elevation = -30.0;
    m_azimuth = 45.0;
    m_zoom = 1.0;

    // Pre-calculate trig values
    m_sinElev = std::sin(m_elevation * DEG2RAD);
    m_cosElev = std::cos(m_elevation * DEG2RAD);
    m_sinAzim = std::sin(m_azimuth * DEG2RAD);
    m_cosAzim = std::cos(m_azimuth * DEG2RAD);

    update();
}

void VisualizationWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Clear background
    painter.fillRect(rect(), QWidget::palette().color(QPalette::Window));

    bool hasDimensions = m_palette.length > 0 && m_palette.width > 0 &&
                         m_palette.packageHeight > 0 && m_palette.totalLayers > 0;
    if (!hasDimensions) {
        painter.setPen(Qt::gray);
        painter.setFont(QFont("Arial", 14));
        painter.drawText(rect(), Qt::AlignCenter, tr("No palette loaded"));
        return;
    }

    // Calculate scale based on widget size and pallet dimensions
    double maxDim = std::max({m_palette.length, m_palette.width,
                              static_cast<double>(m_palette.packageHeight * m_palette.totalLayers)});
    if (maxDim <= 0.0) {
        painter.setPen(Qt::gray);
        painter.setFont(QFont("Arial", 14));
        painter.drawText(rect(), Qt::AlignCenter, tr("Invalid palette data"));
        return;
    }
    double viewSize = std::min(width(), height()) * 0.7;
    m_scale = viewSize / maxDim * m_zoom;

    // Center offset
    m_offset = QPointF(width() / 2.0, height() / 2.0);

    // Draw components back to front
    drawPalletBase(painter);

    if (!m_palette.packages.isEmpty()) {
        struct FaceDraw {
            QPolygonF poly;
            QColor color;
            double depth;
        };

        QVector<FaceDraw> faces;
        faces.reserve(m_palette.packages.size() * 6);

        auto projectWithDepth = [this](double x, double y, double z) {
            double cx = m_palette.length / 2.0;
            double cy = m_palette.width / 2.0;
            double cz = (m_palette.packageHeight * m_palette.totalLayers) / 2.0;

            double dx = x - cx;
            double dy = y - cy;
            double dz = z - cz;

            double rx = dx * m_cosAzim + dy * m_sinAzim;
            double ry = -dx * m_sinAzim + dy * m_cosAzim;
            double rz = dz;

            double screenX = rx * m_scale;
            double screenY = (ry * m_sinElev + rz * m_cosElev) * m_scale;
            // Depth: for negative elevation, invert Z contribution
            double depth = ry * m_cosElev - rz * m_sinElev;

            return qMakePair(m_offset + QPointF(screenX, -screenY), depth);
        };

        for (const auto& pkg : m_palette.packages) {
            if (m_currentLayer > 0 && pkg.layer != m_currentLayer - 1) {
                continue;
            }

            double w = pkg.width;
            double l = pkg.length;
            if (pkg.rotation == 90 || pkg.rotation == 270) {
                std::swap(w, l);
            }

            double z = pkg.layer * pkg.height;
            double hw = w / 2.0;
            double hl = l / 2.0;

            struct Vertex { double x, y, z; };
            Vertex vertices[8] = {
                {pkg.x - hw, pkg.y - hl, z},              // 0
                {pkg.x + hw, pkg.y - hl, z},              // 1
                {pkg.x + hw, pkg.y + hl, z},              // 2
                {pkg.x - hw, pkg.y + hl, z},              // 3
                {pkg.x - hw, pkg.y - hl, z + pkg.height}, // 4
                {pkg.x + hw, pkg.y - hl, z + pkg.height}, // 5
                {pkg.x + hw, pkg.y + hl, z + pkg.height}, // 6
                {pkg.x - hw, pkg.y + hl, z + pkg.height}, // 7
            };

            struct Face {
                int v[4];
                int faceType;
            };

            Face faceDefs[6] = {
                {{0, 1, 2, 3}, 0},  // bottom
                {{4, 5, 6, 7}, 1},  // top
                {{0, 1, 5, 4}, 2},  // front
                {{2, 3, 7, 6}, 3},  // back
                {{0, 3, 7, 4}, 4},  // left
                {{1, 2, 6, 5}, 5},  // right
            };

            for (const auto& face : faceDefs) {
                QPolygonF poly;
                double depthSum = 0.0;
                for (int i = 0; i < 4; ++i) {
                    const auto& vert = vertices[face.v[i]];
                    auto projected = projectWithDepth(vert.x, vert.y, vert.z);
                    poly << projected.first;
                    depthSum += projected.second;
                }

                FaceDraw draw;
                draw.poly = poly;
                draw.color = getFaceColor(pkg.rotation, face.faceType);
                draw.depth = depthSum / 4.0;
                faces.append(draw);
            }
        }

        std::sort(faces.begin(), faces.end(),
                  [](const FaceDraw& a, const FaceDraw& b) { return a.depth < b.depth; });

        painter.setPen(QPen(COLOR_EDGE, 1));
        for (const auto& face : faces) {
            painter.setBrush(face.color);
            painter.drawPolygon(face.poly);
        }
    } else {
        painter.setPen(Qt::gray);
        painter.setFont(QFont("Arial", 12));
        painter.drawText(rect(), Qt::AlignCenter, tr("No packages to render"));
    }

    drawAxes(painter);
    drawLegend(painter);

    emit renderComplete(m_palette.packages.size());
}

void VisualizationWidget::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    update();
}

QPointF VisualizationWidget::project3D(double x, double y, double z) const
{
    // Isometric projection with configurable view angles
    // Center the pallet in view
    double cx = m_palette.length / 2.0;
    double cy = m_palette.width / 2.0;
    double cz = (m_palette.packageHeight * m_palette.totalLayers) / 2.0;

    // Translate to center
    double dx = x - cx;
    double dy = y - cy;
    double dz = z - cz;

    // Rotate around Y axis (azimuth)
    double rx = dx * m_cosAzim + dy * m_sinAzim;
    double ry = -dx * m_sinAzim + dy * m_cosAzim;
    double rz = dz;

    // Project with elevation
    double screenX = rx * m_scale;
    double screenY = (ry * m_sinElev + rz * m_cosElev) * m_scale;

    return m_offset + QPointF(screenX, -screenY);  // Flip Y for screen coordinates
}

void VisualizationWidget::drawBox(QPainter& painter, const VisualPackage& pkg)
{
    // Calculate actual dimensions based on rotation
    double w = pkg.width;
    double l = pkg.length;

    // Swap dimensions for 90/270 degree rotations
    if (pkg.rotation == 90 || pkg.rotation == 270) {
        std::swap(w, l);
    }

    // Calculate Z position based on layer
    double z = pkg.layer * pkg.height;

    // Calculate box vertices (centered at x, y)
    double hw = w / 2.0;
    double hl = l / 2.0;

    // 8 vertices of the box
    struct Vertex { double x, y, z; };
    Vertex vertices[8] = {
        {pkg.x - hw, pkg.y - hl, z},              // 0: bottom-front-left
        {pkg.x + hw, pkg.y - hl, z},              // 1: bottom-front-right
        {pkg.x + hw, pkg.y + hl, z},              // 2: bottom-back-right
        {pkg.x - hw, pkg.y + hl, z},              // 3: bottom-back-left
        {pkg.x - hw, pkg.y - hl, z + pkg.height}, // 4: top-front-left
        {pkg.x + hw, pkg.y - hl, z + pkg.height}, // 5: top-front-right
        {pkg.x + hw, pkg.y + hl, z + pkg.height}, // 6: top-back-right
        {pkg.x - hw, pkg.y + hl, z + pkg.height}, // 7: top-back-left
    };

    // Project all vertices
    QPointF projected[8];
    for (int i = 0; i < 8; i++) {
        projected[i] = project3D(vertices[i].x, vertices[i].y, vertices[i].z);
    }

    // Define faces with vertex indices
    // Order: bottom, back, left, right, front, top (painter's algorithm)
    struct Face {
        int v[4];
        int faceType; // 0=bottom, 1=top, 2=front, 3=back, 4=left, 5=right
    };

    // Faces ordered for proper occlusion (back faces first)
    Face faces[6] = {
        {{0, 1, 2, 3}, 0},  // bottom
        {{3, 2, 6, 7}, 3},  // back
        {{0, 3, 7, 4}, 4},  // left
        {{1, 5, 6, 2}, 5},  // right
        {{0, 4, 5, 1}, 2},  // front
        {{4, 7, 6, 5}, 1},  // top
    };

    // Sort faces by average depth
    auto faceDepth = [&](const Face& f) {
        double avgZ = 0;
        for (int i = 0; i < 4; i++) {
            avgZ += vertices[f.v[i]].z - (vertices[f.v[i]].x + vertices[f.v[i]].y) * 0.5;
        }
        return avgZ / 4.0;
    };

    std::sort(std::begin(faces), std::end(faces),
              [&](const Face& a, const Face& b) { return faceDepth(a) < faceDepth(b); });

    // Draw faces
    painter.setPen(QPen(COLOR_EDGE, 1));

    for (const auto& face : faces) {
        QPolygonF poly;
        for (int i = 0; i < 4; i++) {
            poly << projected[face.v[i]];
        }

        QColor faceColor = getFaceColor(pkg.rotation, face.faceType);
        painter.setBrush(faceColor);
        painter.drawPolygon(poly);
    }
}

void VisualizationWidget::drawPalletBase(QPainter& painter)
{
    if (m_palette.length <= 0 || m_palette.width <= 0) {
        return;
    }

    // Draw pallet as a thin box at z=0
    const double palletThickness = 20.0;  // 20mm thick pallet base

    // Pallet corners
    QPointF corners[8] = {
        project3D(0, 0, -palletThickness),
        project3D(m_palette.length, 0, -palletThickness),
        project3D(m_palette.length, m_palette.width, -palletThickness),
        project3D(0, m_palette.width, -palletThickness),
        project3D(0, 0, 0),
        project3D(m_palette.length, 0, 0),
        project3D(m_palette.length, m_palette.width, 0),
        project3D(0, m_palette.width, 0),
    };

    // Draw pallet faces
    painter.setPen(QPen(Qt::darkGray, 1));
    painter.setBrush(COLOR_PALLET);

    // Top face
    QPolygonF top;
    top << corners[4] << corners[5] << corners[6] << corners[7];
    painter.drawPolygon(top);

    // Front face
    QPolygonF front;
    front << corners[0] << corners[4] << corners[5] << corners[1];
    painter.setBrush(COLOR_PALLET.darker(120));
    painter.drawPolygon(front);

    // Right face
    QPolygonF right;
    right << corners[1] << corners[5] << corners[6] << corners[2];
    painter.setBrush(COLOR_PALLET.darker(110));
    painter.drawPolygon(right);
}

void VisualizationWidget::drawAxes(QPainter& painter)
{
    const double axisLength = 150.0;  // mm

    QPointF origin = project3D(0, 0, 0);
    QPointF xEnd = project3D(axisLength, 0, 0);
    QPointF yEnd = project3D(0, axisLength, 0);
    QPointF zEnd = project3D(0, 0, axisLength);

    // Draw axes
    painter.setPen(QPen(Qt::red, 2));
    painter.drawLine(origin, xEnd);

    painter.setPen(QPen(Qt::green, 2));
    painter.drawLine(origin, yEnd);

    painter.setPen(QPen(Qt::blue, 2));
    painter.drawLine(origin, zEnd);

    // Labels
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(xEnd + QPointF(5, 0), "X");
    painter.drawText(yEnd + QPointF(5, 0), "Y");
    painter.drawText(zEnd + QPointF(5, 0), "Z");
}

void VisualizationWidget::drawLegend(QPainter& painter)
{
    // Legend box in bottom-right corner
    const int margin = 10;
    const int boxWidth = 180;
    const int boxHeight = 120;

    QRect legendRect(width() - boxWidth - margin,
                     height() - boxHeight - margin,
                     boxWidth, boxHeight);

    // Draw background
    painter.setPen(Qt::gray);
    painter.setBrush(QColor(255, 255, 255, 230));
    painter.drawRoundedRect(legendRect, 5, 5);

    // Draw text
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 9));

    int y = legendRect.top() + 15;
    int x = legendRect.left() + 10;
    int lineHeight = 16;

    // Title
    painter.setFont(QFont("Arial", 9, QFont::Bold));
    QString title = m_palette.name.isEmpty() ? tr("Palette Info") : m_palette.name;
    painter.drawText(x, y, title);
    y += lineHeight + 2;

    painter.setFont(QFont("Arial", 9));

    // Package dimensions
    painter.drawText(x, y, tr("Package: %1 × %2 × %3 mm")
                     .arg(m_palette.packageLength)
                     .arg(m_palette.packageWidth)
                     .arg(m_palette.packageHeight));
    y += lineHeight;

    // Pallet dimensions
    painter.drawText(x, y, tr("Pallet: %1 × %2 mm")
                     .arg(static_cast<int>(m_palette.length))
                     .arg(static_cast<int>(m_palette.width)));
    y += lineHeight;

    // Counts
    painter.drawText(x, y, tr("Packages: %1").arg(m_palette.totalPackages));
    y += lineHeight;

    painter.drawText(x, y, tr("Layers: %1").arg(m_palette.totalLayers));
    y += lineHeight;

    // Conveyor direction
    QString direction = (m_palette.conveyorDirection == 1) ? tr("Quer") : tr("Längs");
    painter.drawText(x, y, tr("Direction: %1").arg(direction));
}

QColor VisualizationWidget::getFaceColor(int rotation, int face) const
{
    // Face types: 0=bottom, 1=top, 2=front, 3=back, 4=left, 5=right

    // Top and bottom are always green
    if (face == 0 || face == 1) {
        return COLOR_GREEN;
    }

    // Color mapping based on rotation (matching Python implementation)
    // rotation 0: Front=White, Back=Red, Left=Blue, Right=Blue
    // rotation 90: Front=Blue, Back=Blue, Left=Red, Right=White
    // rotation 180: Front=Red, Back=White, Left=Blue, Right=Blue
    // rotation 270: Front=Blue, Back=Blue, Left=White, Right=Red

    switch (rotation) {
        case 0:
            switch (face) {
                case 2: return COLOR_WHITE;  // front
                case 3: return COLOR_RED;    // back
                case 4: return COLOR_BLUE;   // left
                case 5: return COLOR_BLUE;   // right
            }
            break;
        case 90:
            switch (face) {
                case 2: return COLOR_BLUE;   // front
                case 3: return COLOR_BLUE;   // back
                case 4: return COLOR_RED;    // left
                case 5: return COLOR_WHITE;  // right
            }
            break;
        case 180:
            switch (face) {
                case 2: return COLOR_RED;    // front
                case 3: return COLOR_WHITE;  // back
                case 4: return COLOR_BLUE;   // left
                case 5: return COLOR_BLUE;   // right
            }
            break;
        case 270:
            switch (face) {
                case 2: return COLOR_BLUE;   // front
                case 3: return COLOR_BLUE;   // back
                case 4: return COLOR_WHITE;  // left
                case 5: return COLOR_RED;    // right
            }
            break;
    }

    return COLOR_BLUE;  // default
}

void VisualizationWidget::sortPackagesByDepth()
{
    // Sort packages from back to front for proper rendering
    std::sort(m_palette.packages.begin(), m_palette.packages.end(),
              [this](const VisualPackage& a, const VisualPackage& b) {
                  // Calculate depth based on view angle
                  double depthA = a.x * m_sinAzim + a.y * m_cosAzim + a.layer * 1000;
                  double depthB = b.x * m_sinAzim + b.y * m_cosAzim + b.layer * 1000;
                  return depthA < depthB;
              });
}

} // namespace ui
} // namespace multipack
