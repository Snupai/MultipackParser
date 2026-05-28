/**
 * @file UR10ServerFunctions.h
 * @brief UR10-specific XML-RPC server functions
 */

#ifndef MULTIPACK_NETWORK_UR10SERVERFUNCTIONS_H
#define MULTIPACK_NETWORK_UR10SERVERFUNCTIONS_H

namespace multipack {
namespace network {

// Forward declaration
class RpcMethodRegistry;

namespace UR10ServerFunctions {

/**
 * @brief Register UR10-specific RPC methods
 * @param registry Method registry to register with
 */
void registerMethods(RpcMethodRegistry* registry);

} // namespace UR10ServerFunctions
} // namespace network
} // namespace multipack

#endif // MULTIPACK_NETWORK_UR10SERVERFUNCTIONS_H
