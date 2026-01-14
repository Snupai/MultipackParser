/**
 * @file RpcMethodRegistry.h
 * @brief Registry for XML-RPC methods
 *
 * Manages registration and lookup of RPC methods.
 */

#ifndef MULTIPACK_NETWORK_RPCMETHODREGISTRY_H
#define MULTIPACK_NETWORK_RPCMETHODREGISTRY_H

#include <QObject>
#include <QString>
#include <QMap>
#include <functional>

#include "XmlRpcServer.h"

namespace multipack {
namespace network {

/**
 * @brief Method metadata
 */
struct MethodInfo {
    QString name;          ///< Method name
    QString description;   ///< Human-readable description
    QString signature;     ///< Parameter signature
    RpcMethod method;      ///< Method implementation
};

/**
 * @class RpcMethodRegistry
 * @brief Registry for XML-RPC methods
 *
 * Maintains a registry of available RPC methods
 * with metadata for introspection.
 */
class RpcMethodRegistry : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new Rpc Method Registry
     * @param parent Parent QObject
     */
    explicit RpcMethodRegistry(QObject* parent = nullptr);

    /**
     * @brief Destroy the Rpc Method Registry
     */
    ~RpcMethodRegistry() override;

    /**
     * @brief Register a method
     * @param name Method name
     * @param method Method implementation
     * @param description Optional description
     * @param signature Optional signature
     */
    void registerMethod(const QString& name, RpcMethod method,
                       const QString& description = QString(),
                       const QString& signature = QString());

    /**
     * @brief Unregister a method
     * @param name Method name
     * @return true if method was registered
     */
    bool unregisterMethod(const QString& name);

    /**
     * @brief Check if a method is registered
     * @param name Method name
     * @return true if registered
     */
    bool hasMethod(const QString& name) const;

    /**
     * @brief Get a method by name
     * @param name Method name
     * @return Method info, or invalid if not found
     */
    MethodInfo getMethod(const QString& name) const;

    /**
     * @brief Call a method
     * @param name Method name
     * @param params Parameters
     * @return Result, or fault value on error
     */
    RpcValue callMethod(const QString& name, const QVector<RpcValue>& params);

    /**
     * @brief Get all registered method names
     * @return List of method names
     */
    QStringList methodNames() const;

    /**
     * @brief Get method count
     * @return Number of registered methods
     */
    int methodCount() const;

    /**
     * @brief Clear all methods
     */
    void clear();

    // Introspection methods (standard XML-RPC introspection)

    /**
     * @brief List all methods (system.listMethods)
     * @return Array of method names
     */
    RpcValue listMethods(const QVector<RpcValue>& params);

    /**
     * @brief Get method signature (system.methodSignature)
     * @param params [method_name]
     * @return Method signature
     */
    RpcValue methodSignature(const QVector<RpcValue>& params);

    /**
     * @brief Get method help (system.methodHelp)
     * @param params [method_name]
     * @return Method description
     */
    RpcValue methodHelp(const QVector<RpcValue>& params);

signals:
    /**
     * @brief Emitted when a method is registered
     * @param name Method name
     */
    void methodRegistered(const QString& name);

    /**
     * @brief Emitted when a method is unregistered
     * @param name Method name
     */
    void methodUnregistered(const QString& name);

private:
    QMap<QString, MethodInfo> m_methods;
};

} // namespace network
} // namespace multipack

#endif // MULTIPACK_NETWORK_RPCMETHODREGISTRY_H
