/**
 * @file UR10ServerFunctions.cpp
 * @brief UR10-specific XML-RPC server functions
 *
 * Implements scanner status queries for UR10 robot.
 */

#include "multipack/network/UR10ServerFunctions.h"
#include "multipack/network/RpcMethodRegistry.h"
#include "multipack/core/GlobalState.h"

#include <QDebug>

namespace multipack {
namespace network {
namespace UR10ServerFunctions {

void registerMethods(RpcMethodRegistry* registry)
{
    if (!registry) {
        qWarning() << "UR10ServerFunctions: Registry is null";
        return;
    }

    qDebug() << "UR10ServerFunctions::registerMethods - registering UR10 methods";

    auto& state = core::GlobalState::instance();

    // UR10_scanner1and2niobild - Get scanner status for both scanners (NIO)
    registry->registerMethod("UR10_scanner1and2niobild",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            Q_UNUSED(params);
            qDebug() << "RPC: UR10_scanner1and2niobild called";
            return RpcValue(state.scanner1and2NioValue());
        },
        "Get scanner 1&2 NIO status value",
        "int"
    );

    // UR10_scanner1bild - Get scanner 1 status
    registry->registerMethod("UR10_scanner1bild",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            Q_UNUSED(params);
            qDebug() << "RPC: UR10_scanner1bild called";
            return RpcValue(state.scanner1Value());
        },
        "Get scanner 1 status value",
        "int"
    );

    // UR10_scanner2bild - Get scanner 2 status
    registry->registerMethod("UR10_scanner2bild",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            Q_UNUSED(params);
            qDebug() << "RPC: UR10_scanner2bild called";
            return RpcValue(state.scanner2Value());
        },
        "Get scanner 2 status value",
        "int"
    );

    // UR10_scanner1and2iobild - Get scanner status for both scanners (IO)
    registry->registerMethod("UR10_scanner1and2iobild",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            Q_UNUSED(params);
            qDebug() << "RPC: UR10_scanner1and2iobild called";
            return RpcValue(state.scanner1and2IoValue());
        },
        "Get scanner 1&2 IO status value",
        "int"
    );

    qDebug() << "UR10ServerFunctions: Registered 4 methods";
}

} // namespace UR10ServerFunctions
} // namespace network
} // namespace multipack
