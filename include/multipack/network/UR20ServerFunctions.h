/**
 * @file UR20ServerFunctions.h
 * @brief UR20-specific XML-RPC server functions
 */

#ifndef MULTIPACK_NETWORK_UR20SERVERFUNCTIONS_H
#define MULTIPACK_NETWORK_UR20SERVERFUNCTIONS_H

namespace multipack {
namespace network {

// Forward declaration
class RpcMethodRegistry;

namespace UR20ServerFunctions {

/**
 * @brief Register UR20-specific RPC methods
 * @param registry Method registry to register with
 */
void registerMethods(RpcMethodRegistry* registry);

} // namespace UR20ServerFunctions
} // namespace network
} // namespace multipack

#endif // MULTIPACK_NETWORK_UR20SERVERFUNCTIONS_H
