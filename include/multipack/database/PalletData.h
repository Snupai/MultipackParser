/**
 * @file PalletData.h
 */
#ifndef MULTIPACK_DATABASE_PALLETDATA_H
#define MULTIPACK_DATABASE_PALLETDATA_H
#include <QString>
#include <QVector>
namespace multipack { namespace database {
struct PalletData {
    QString fileName;
    QVector<double> paletteDims;
    QVector<double> packageDims;
    int numberOfLayers = 0;
    QVector<int> layerTypes;
    QVector<QVector<double>> positions;
};
}} // namespace
#endif
