/**
 * @file PaletData.cpp
 * @brief Implementation of palette data models
 */

#include "multipack/database/PaletData.h"

#include <QDebug>
#include <QJsonArray>

namespace multipack {
namespace database {

// Position implementation
QJsonObject Position::toJson() const
{
    QJsonObject obj;
    obj["x"] = x;
    obj["y"] = y;
    obj["z"] = z;
    obj["rotation"] = rotation;
    return obj;
}

Position Position::fromJson(const QJsonObject& json)
{
    Position pos;
    pos.x = json["x"].toDouble();
    pos.y = json["y"].toDouble();
    pos.z = json["z"].toDouble();
    pos.rotation = json["rotation"].toDouble();
    return pos;
}

// PackagePlacement implementation
QJsonObject PackagePlacement::toJson() const
{
    QJsonObject obj;
    obj["index"] = index;
    obj["pick_position"] = pickPosition.toJson();
    obj["drop_position"] = dropPosition.toJson();
    obj["package_count"] = packageCount;
    obj["x_vector"] = xVector;
    obj["y_vector"] = yVector;
    return obj;
}

PackagePlacement PackagePlacement::fromJson(const QJsonObject& json)
{
    PackagePlacement placement;
    placement.index = json["index"].toInt();
    placement.pickPosition = Position::fromJson(json["pick_position"].toObject());
    placement.dropPosition = Position::fromJson(json["drop_position"].toObject());
    placement.packageCount = json["package_count"].toInt(1);
    placement.xVector = json["x_vector"].toDouble();
    placement.yVector = json["y_vector"].toDouble();
    return placement;
}

// Layer implementation
int Layer::packageCount() const
{
    int count = 0;
    for (const auto& p : placements) {
        count += p.packageCount;
    }
    return count;
}

QJsonObject Layer::toJson() const
{
    QJsonObject obj;
    obj["layer_number"] = layerNumber;
    obj["type"] = static_cast<int>(type);

    QJsonArray placementsArray;
    for (const auto& p : placements) {
        placementsArray.append(p.toJson());
    }
    obj["placements"] = placementsArray;

    return obj;
}

Layer Layer::fromJson(const QJsonObject& json)
{
    Layer layer;
    layer.layerNumber = json["layer_number"].toInt();
    layer.type = static_cast<LayerType>(json["type"].toInt());

    QJsonArray placementsArray = json["placements"].toArray();
    for (const auto& val : placementsArray) {
        layer.placements.append(PackagePlacement::fromJson(val.toObject()));
    }

    return layer;
}

// PaletData implementation
PaletData::PaletData(const QString& fileName)
    : m_fileName(fileName)
{
}

void PaletData::setPaletteDimensions(double length, double width,
                                     double height, double overhang)
{
    m_paletteLength = length;
    m_paletteWidth = width;
    m_paletteHeight = height;
    m_paletteOverhang = overhang;
}

void PaletData::setPackageDimensions(double length, double width,
                                     double height, double weight)
{
    m_packageLength = length;
    m_packageWidth = width;
    m_packageHeight = height;
    m_packageWeight = weight;
}

int PaletData::totalPackageCount() const
{
    int count = 0;
    for (const auto& layer : m_layers) {
        count += layer.packageCount();
    }
    return count;
}

bool PaletData::isValid() const
{
    // Check for required dimensions
    if (m_paletteLength <= 0 || m_paletteWidth <= 0) {
        return false;
    }
    if (m_packageLength <= 0 || m_packageWidth <= 0 || m_packageHeight <= 0) {
        return false;
    }

    // Must have at least one layer
    if (m_layers.isEmpty()) {
        return false;
    }

    return true;
}

QJsonObject PaletData::toJson() const
{
    QJsonObject obj;

    obj["file_name"] = m_fileName;

    // Palette dimensions
    QJsonObject paletteDims;
    paletteDims["length"] = m_paletteLength;
    paletteDims["width"] = m_paletteWidth;
    paletteDims["height"] = m_paletteHeight;
    paletteDims["overhang"] = m_paletteOverhang;
    obj["palette_dimensions"] = paletteDims;

    // Package dimensions
    QJsonObject packageDims;
    packageDims["length"] = m_packageLength;
    packageDims["width"] = m_packageWidth;
    packageDims["height"] = m_packageHeight;
    packageDims["weight"] = m_packageWeight;
    obj["package_dimensions"] = packageDims;

    // Layers
    QJsonArray layersArray;
    for (const auto& layer : m_layers) {
        layersArray.append(layer.toJson());
    }
    obj["layers"] = layersArray;

    return obj;
}

PaletData PaletData::fromJson(const QJsonObject& json)
{
    PaletData data;

    data.m_fileName = json["file_name"].toString();

    // Palette dimensions
    QJsonObject paletteDims = json["palette_dimensions"].toObject();
    data.m_paletteLength = paletteDims["length"].toDouble();
    data.m_paletteWidth = paletteDims["width"].toDouble();
    data.m_paletteHeight = paletteDims["height"].toDouble();
    data.m_paletteOverhang = paletteDims["overhang"].toDouble();

    // Package dimensions
    QJsonObject packageDims = json["package_dimensions"].toObject();
    data.m_packageLength = packageDims["length"].toDouble();
    data.m_packageWidth = packageDims["width"].toDouble();
    data.m_packageHeight = packageDims["height"].toDouble();
    data.m_packageWeight = packageDims["weight"].toDouble();

    // Layers
    QJsonArray layersArray = json["layers"].toArray();
    for (const auto& val : layersArray) {
        data.m_layers.append(Layer::fromJson(val.toObject()));
    }

    return data;
}

void PaletData::clear()
{
    m_fileName.clear();
    m_paletteLength = 0;
    m_paletteWidth = 0;
    m_paletteHeight = 0;
    m_paletteOverhang = 0;
    m_packageLength = 0;
    m_packageWidth = 0;
    m_packageHeight = 0;
    m_packageWeight = 0;
    m_layers.clear();
}

} // namespace database
} // namespace multipack
