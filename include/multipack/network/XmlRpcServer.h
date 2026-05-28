/**
 * @file XmlRpcServer.h
 * @brief XML-RPC server for robot communication
 */
#ifndef MULTIPACK_NETWORK_XMLRPCSERVER_H
#define MULTIPACK_NETWORK_XMLRPCSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QVector>
#include <QVariant>
#include <QMap>
#include <QHash>
#include <functional>
#include <memory>

namespace multipack {

namespace database { class DatabaseManager; }
namespace core { class GlobalState; }

namespace network {

/**
 * @struct RpcValue
 * @brief Value type for XML-RPC parameters and return values
 */
struct RpcValue {
    enum Type { Null, Int, Double, Bool, String, Array, Struct };
    Type type = Null;
    int intValue = 0;
    double doubleValue = 0;
    bool boolValue = false;
    QString stringValue;
    QVector<RpcValue> arrayValue;
    QMap<QString, RpcValue> structValue;

    RpcValue() = default;
    RpcValue(const QString& s) : type(String), stringValue(s) {}
    RpcValue(int i) : type(Int), intValue(i) {}
    RpcValue(double d) : type(Double), doubleValue(d) {}
    RpcValue(bool b) : type(Bool), boolValue(b) {}

    QString toString() const { return stringValue; }
    int toInt() const { return intValue; }
    double toDouble() const { return doubleValue; }
    bool toBool() const { return boolValue; }

    static RpcValue fromArray(const QVector<RpcValue>& arr) {
        RpcValue v;
        v.type = Array;
        v.arrayValue = arr;
        return v;
    }

    static RpcValue fromIntArray(const QVector<int>& arr) {
        RpcValue v;
        v.type = Array;
        for (int i : arr) {
            v.arrayValue.append(RpcValue(i));
        }
        return v;
    }

    static RpcValue fromDoubleArray(const QVector<double>& arr) {
        RpcValue v;
        v.type = Array;
        for (double d : arr) {
            v.arrayValue.append(RpcValue(d));
        }
        return v;
    }
};

/**
 * @brief RPC method function type
 */
using RpcMethod = std::function<RpcValue(const QVector<RpcValue>&)>;

/**
 * @class XmlRpcServer
 * @brief HTTP server implementing XML-RPC protocol
 */
class XmlRpcServer : public QObject {
    Q_OBJECT

public:
    static constexpr int DEFAULT_PORT = 8080;

    explicit XmlRpcServer(QObject* parent = nullptr);
    ~XmlRpcServer() override;

    /**
     * @brief Start the server
     * @param port Port to listen on
     * @return true on success
     */
    [[nodiscard]] bool start(int port = DEFAULT_PORT);

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Get port number
     */
    [[nodiscard]] int port() const { return m_port; }

    /**
     * @brief Register an RPC method
     * @param name Method name
     * @param method Method implementation
     */
    void registerMethod(const QString& name, RpcMethod method);

    /**
     * @brief Remove an RPC method
     * @param name Method name
     */
    void unregisterMethod(const QString& name);

    /**
     * @brief Set database manager for RPC methods
     */
    void setDatabaseManager(database::DatabaseManager* db);

    /**
     * @brief Set global state for RPC methods
     */
    void setGlobalState(core::GlobalState* state);

    /**
     * @brief Register standard UR10/UR20 RPC methods
     */
    void registerStandardMethods();

signals:
    void started();
    void stopped();
    void methodCalled(const QString& method, const QString& clientIp);
    void paletteDataLoaded(const QString& fileName);
    void error(const QString& message);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    void processHttpRequest(QTcpSocket* socket, const QByteArray& requestData);

    /**
     * @brief Parse HTTP request
     */
    QString parseHttpRequest(const QByteArray& data, QString& methodName, QVector<RpcValue>& params);

    /**
     * @brief Parse XML-RPC method call
     */
    bool parseMethodCall(const QString& xml, QString& methodName, QVector<RpcValue>& params);

    /**
     * @brief Parse XML-RPC value
     */
    RpcValue parseValue(const QString& xml);

    /**
     * @brief Build XML-RPC response
     */
    QByteArray buildResponse(const RpcValue& result);

    /**
     * @brief Build fault response
     */
    QByteArray buildFaultResponse(int code, const QString& message);

    /**
     * @brief Build HTTP response
     */
    QByteArray buildHttpResponse(const QByteArray& content);

    /**
     * @brief Call RPC method
     */
    RpcValue callMethod(const QString& name, const QVector<RpcValue>& params);

    // Standard RPC method implementations
    RpcValue rpcGetPalettenDaten(const QVector<RpcValue>& params);
    RpcValue rpcGetPaketDaten(const QVector<RpcValue>& params);
    RpcValue rpcGetPaketPos(const QVector<RpcValue>& params);
    RpcValue rpcGetLageArten(const QVector<RpcValue>& params);
    RpcValue rpcGetAnzLagen(const QVector<RpcValue>& params);
    RpcValue rpcGetAnzPakete(const QVector<RpcValue>& params);
    RpcValue rpcGetLageZuordnung(const QVector<RpcValue>& params);
    RpcValue rpcGetZwischenlagen(const QVector<RpcValue>& params);
    RpcValue rpcGetPaketeZuordnung(const QVector<RpcValue>& params);
    RpcValue rpcGetStartlage(const QVector<RpcValue>& params);
    RpcValue rpcGetEinzelpaketLaengs(const QVector<RpcValue>& params);
    RpcValue rpcGetKartonhoehe(const QVector<RpcValue>& params);
    RpcValue rpcGetGewicht(const QVector<RpcValue>& params);
    RpcValue rpcGetKlemmungAktiv(const QVector<RpcValue>& params);
    RpcValue rpcGetVerschiebungX(const QVector<RpcValue>& params);
    RpcValue rpcGetVerschiebungY(const QVector<RpcValue>& params);
    RpcValue rpcSetLage(const QVector<RpcValue>& params);
    RpcValue rpcGetLabelInvert(const QVector<RpcValue>& params);

    std::unique_ptr<QTcpServer> m_server;
    QMap<QString, RpcMethod> m_methods;
    database::DatabaseManager* m_database = nullptr;
    core::GlobalState* m_state = nullptr;
    QHash<QTcpSocket*, QByteArray> m_socketBuffers;
    int m_port = DEFAULT_PORT;
    bool m_running = false;
};

} // namespace network
} // namespace multipack

#endif // MULTIPACK_NETWORK_XMLRPCSERVER_H
