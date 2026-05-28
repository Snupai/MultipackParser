/**
 * @file URCommonFunctions.h
 */
#ifndef MULTIPACK_NETWORK_URCOMMONFUNCTIONS_H
#define MULTIPACK_NETWORK_URCOMMONFUNCTIONS_H
#include "XmlRpcServer.h"
namespace multipack { namespace network { namespace URCommonFunctions {
    RpcValue getData(const QVector<RpcValue>& params);
    RpcValue getVersion(const QVector<RpcValue>& params);
}}} // namespace
#endif
