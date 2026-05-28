#ifndef MULTIPACK_TESTS_TESTHELPERS_H
#define MULTIPACK_TESTS_TESTHELPERS_H

#include "multipack/database/DatabaseManager.h"

#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTextStream>

namespace testhelpers {

inline multipack::database::PaletteData makeSamplePaletteData(const QString& fileName = "sample.rob",
                                                              qint64 timestampMs = 1735689600000)
{
    multipack::database::PaletteData paletteData;
    paletteData.metadata.fileName = fileName;
    paletteData.metadata.fileTimestamp = timestampMs;
    paletteData.metadata.paketQuer = 1;
    paletteData.metadata.centerOfGravity = {0.0, 0.0, 0.0};
    paletteData.metadata.lageArten = 1;
    paletteData.metadata.anzLagen = 1;
    paletteData.metadata.anzahlPakete = 1;

    paletteData.paletteDimensions.length = 1200;
    paletteData.paletteDimensions.width = 800;
    paletteData.paletteDimensions.height = 150;

    paletteData.packageDimensions.length = 200;
    paletteData.packageDimensions.width = 100;
    paletteData.packageDimensions.height = 50;
    paletteData.packageDimensions.gap = 0;
    paletteData.packageDimensions.weight = 1.25;
    paletteData.packageDimensions.einzelpaketLaengs = false;

    paletteData.rawData = {
        {1200, 800, 150},
        {200, 100, 50, 0},
        {1},
        {1},
        {0},
        {1, 0},
        {1},
        {0, 0, 0, 100, 100, 0, 1, 0, 0}
    };

    paletteData.layerAssignments = {1};
    paletteData.intermediaryLayers = {0};
    paletteData.packagesPerLayerType = {1};

    multipack::database::PackagePosition position;
    position.xp = 0;
    position.yp = 0;
    position.ap = 0;
    position.xd = 100;
    position.yd = 100;
    position.ad = 0;
    position.nop = 1;
    position.xvec = 0;
    position.yvec = 0;
    paletteData.packagePositions = {position};

    return paletteData;
}

inline QString sampleRobFileContents()
{
    return QStringLiteral(
        "1200\t800\t150\n"
        "200\t100\t50\t0\n"
        "1\n"
        "1\n"
        "0\n"
        "1\t0\n"
        "1\n"
        "0\t0\t0\t100\t100\t0\t1\t0\t0\n");
}

inline bool writeSampleRobFile(const QString& directoryPath, const QString& fileName)
{
    QDir().mkpath(directoryPath);
    QFile file(QDir(directoryPath).filePath(fileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << sampleRobFileContents();
    return stream.status() == QTextStream::Ok;
}

inline quint16 findFreePort()
{
    QTcpServer server;
    const bool listening = server.listen(QHostAddress::LocalHost, 0);
    Q_ASSERT(listening);
    return server.serverPort();
}

} // namespace testhelpers

#endif // MULTIPACK_TESTS_TESTHELPERS_H
