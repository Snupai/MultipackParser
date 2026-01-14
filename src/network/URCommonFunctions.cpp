/**
 * @file URCommonFunctions.cpp
 */
#include "multipack/network/URCommonFunctions.h"
#include <QDebug>
namespace multipack { namespace network { namespace URCommonFunctions {
RpcValue getData(const QVector<RpcValue>& params) {
    Q_UNUSED(params);
    qDebug() << "URCommonFunctions::getData - TODO";
    return RpcValue();
}
RpcValue getVersion(const QVector<RpcValue>& params) {
    Q_UNUSED(params);
    return RpcValue(QString("1.7.9"));
}
}}} // namespace
