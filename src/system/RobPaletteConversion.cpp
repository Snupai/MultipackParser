/**
 * @file RobPaletteConversion.cpp
 * @brief Conversion from parsed .rob data to the database palette model
 */
#include "multipack/system/RobPaletteConversion.h"

#include <QFileInfo>

namespace multipack {
namespace system {

database::PaletteData robFileToPaletteData(const RobFileData& fileData)
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

} // namespace system
} // namespace multipack
