/**
 * @file RpcMethodRegistry.cpp
 * @brief Implementation of RPC method registry
 */

#include "multipack/network/RpcMethodRegistry.h"
#include "multipack/config/LoggingConfig.h"

#include <QDebug>
#include <QLoggingCategory>

namespace multipack {
namespace network {

using ::multipack::config::serverLog;

RpcMethodRegistry::RpcMethodRegistry(QObject* parent)
    : QObject(parent)
{
    qCDebug(serverLog) << "RpcMethodRegistry::RpcMethodRegistry - constructor";
}

RpcMethodRegistry::~RpcMethodRegistry()
{
    qCDebug(serverLog) << "RpcMethodRegistry::~RpcMethodRegistry - destructor";
}

void RpcMethodRegistry::registerMethod(const QString& name, RpcMethod method,
                                        const QString& description,
                                        const QString& signature)
{
    MethodInfo info;
    info.name = name;
    info.method = method;
    info.description = description;
    info.signature = signature;

    m_methods[name] = info;

    qCDebug(serverLog) << "Registered RPC method:" << name;
    emit methodRegistered(name);
}

bool RpcMethodRegistry::unregisterMethod(const QString& name)
{
    if (m_methods.remove(name) > 0) {
        qCDebug(serverLog) << "Unregistered RPC method:" << name;
        emit methodUnregistered(name);
        return true;
    }
    return false;
}

bool RpcMethodRegistry::hasMethod(const QString& name) const
{
    return m_methods.contains(name);
}

MethodInfo RpcMethodRegistry::getMethod(const QString& name) const
{
    return m_methods.value(name);
}

RpcValue RpcMethodRegistry::callMethod(const QString& name,
                                        const QVector<RpcValue>& params)
{
    auto it = m_methods.find(name);
    if (it == m_methods.end()) {
        RpcValue error;
        error.type = RpcValue::String;
        error.stringValue = QString("Unknown method: %1").arg(name);
        return error;
    }

    try {
        return it.value().method(params);
    } catch (const std::exception& e) {
        qCCritical(serverLog) << "Exception in RPC method" << name << ":" << e.what();

        RpcValue error;
        error.type = RpcValue::String;
        error.stringValue = QString("Method error: %1").arg(e.what());
        return error;
    }
}

QStringList RpcMethodRegistry::methodNames() const
{
    return m_methods.keys();
}

int RpcMethodRegistry::methodCount() const
{
    return m_methods.size();
}

void RpcMethodRegistry::clear()
{
    m_methods.clear();
}

RpcValue RpcMethodRegistry::listMethods(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);

    RpcValue result;
    result.type = RpcValue::Array;

    for (const QString& name : m_methods.keys()) {
        result.arrayValue.append(RpcValue(name));
    }

    return result;
}

RpcValue RpcMethodRegistry::methodSignature(const QVector<RpcValue>& params)
{
    if (params.isEmpty()) {
        return RpcValue(QString("undef"));
    }

    QString methodName = params[0].toString();
    auto it = m_methods.find(methodName);

    if (it != m_methods.end() && !it.value().signature.isEmpty()) {
        return RpcValue(it.value().signature);
    }

    return RpcValue(QString("undef"));
}

RpcValue RpcMethodRegistry::methodHelp(const QVector<RpcValue>& params)
{
    if (params.isEmpty()) {
        return RpcValue(QString(""));
    }

    QString methodName = params[0].toString();
    auto it = m_methods.find(methodName);

    if (it != m_methods.end()) {
        return RpcValue(it.value().description);
    }

    return RpcValue(QString("Unknown method"));
}

} // namespace network
} // namespace multipack
